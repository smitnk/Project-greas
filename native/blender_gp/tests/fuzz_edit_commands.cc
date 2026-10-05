/* Host fuzzer for the edit-command routing (project_grease_gp_apply_edit_command, ids 0..130) and
 * the history / layer / frame entry points around it, on the real bridge + backend + pinned Blender
 * 3.6.23 legacy GP code. Built with ASan + UBSan by tools/run_native_fuzz.sh: a memory error aborts
 * with a report. After every operation the document is checked for corruption (stroke / point
 * invariants, finite coordinates, material indices in range).
 *
 * usage: fuzz_edit_commands [iterations] [seed]   (PG_FUZZ_LOG=1 prints each operation) */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "project_grease_gp_bridge.h"
#include <csignal>
#include <pthread.h>
#include <unistd.h>

#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "MEM_guardedalloc.h"

/* The draw module is not part of the host build (nothing renders); same stubs as the backend test.
 * BLI_mempool's mutex comes from BLI_threads, which the closure does not link. */
extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}
extern "C" void BLI_mutex_init(pthread_mutex_t *mutex) { pthread_mutex_init(mutex, nullptr); }

static std::mt19937 rng;
/* Watchdog: one operation taking longer than this is a hang (an unbounded loop or allocation). */
static unsigned kOpSeconds = 300; /* PG_FUZZ_OP_SECONDS: catches endless loops; sanitizers make operations 3-10x slower, and the slowest operation is reported separately */
static char g_current_op[512];
static ProjectGreaseGPHandle *g_handle = nullptr;
static int largest_stroke(const bGPdata *gpd);
extern "C" void __sanitizer_print_stack_trace(void) __attribute__((weak));
static void on_alarm(int)
{
  if (__sanitizer_print_stack_trace) __sanitizer_print_stack_trace();
  static const char msg[] = "FAIL operation hung (watchdog): ";
  (void)!write(2, msg, sizeof(msg) - 1);
  if (g_handle) fprintf(stderr, "(largest stroke %d points) ", largest_stroke(project_grease_gp_document_data(g_handle)));
  (void)!write(2, g_current_op, strlen(g_current_op));
  (void)!write(2, "\n", 1);
  _exit(3);
}
static int irand(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }

/* Plausible values (small indices / modes, factors, canvas coordinates): most commands validate
 * their arguments, so half the calls use these to get past the checks into the operator bodies. */
static float sane_value()
{
  switch (irand(0, 4)) {
    case 0: return float(irand(0, 3));
    case 1: return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    case 2: return float(irand(1, 10));
    case 3: return std::uniform_real_distribution<float>(50.0f, 1000.0f)(rng);
    default: return std::uniform_real_distribution<float>(-2.0f, 2.0f)(rng);
  }
}

static float arg_value()
{
  switch (irand(0, 13)) {
    case 0: return 0.0f;
    case 1: return 1.0f;
    case 2: return -1.0f;
    case 3: return 0.5f;
    case 4: return float(irand(0, 6));
    case 5: return float(irand(-3, 40));
    case 6: return 1e9f;
    case 7: return -1e9f;
    case 8: return NAN;
    case 9: return INFINITY;
    case 10: return -INFINITY;
    case 11: return std::uniform_real_distribution<float>(-2000.0f, 2000.0f)(rng);
    case 12: return float(irand(100, 100000));
    default: return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
  }
}

static void draw_stroke(ProjectGreaseGPHandle *h)
{
  const int n = irand(1, 40);
  if (!project_grease_gp_begin_stroke(h, irand(0, 2), float(irand(1, 60)))) return;
  float x = float(irand(50, 1000)), y = float(irand(50, 900));
  for (int i = 0; i < n; i++) {
    x += float(irand(-30, 30)); y += float(irand(-30, 30));
    ProjectGreaseGPPoint p{x, y, 0.0f, std::uniform_real_distribution<float>(0.05f, 1.0f)(rng), 1.0f, float(i) * 0.01f};
    project_grease_gp_add_point(h, p);
  }
  project_grease_gp_end_stroke(h);
}

/* Corruption checks on the whole document; returns a description of the first problem. */
static std::string check_document(const bGPdata *gpd)
{
  if (!gpd) return "no document";
  char buf[256];
  for (const bGPDlayer *gpl = (const bGPDlayer *)gpd->layers.first; gpl; gpl = gpl->next) {
    int last_frame = -1000000;
    for (const bGPDframe *gpf = (const bGPDframe *)gpl->frames.first; gpf; gpf = gpf->next) {
      if (gpf->framenum <= last_frame) {
        snprintf(buf, sizeof(buf), "layer %s: frames out of order (%d after %d)", gpl->info, gpf->framenum, last_frame);
        return buf;
      }
      last_frame = gpf->framenum;
      for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) {
        if (gps->totpoints < 0 || (gps->totpoints > 0 && gps->points == nullptr)) {
          snprintf(buf, sizeof(buf), "layer %s frame %d: stroke totpoints %d points %p", gpl->info, gpf->framenum,
                   gps->totpoints, (void *)gps->points);
          return buf;
        }
        if (gps->mat_nr < 0 || (gpd->totcol > 0 && gps->mat_nr >= gpd->totcol)) {
          snprintf(buf, sizeof(buf), "layer %s frame %d: stroke mat_nr %d of %d slots", gpl->info, gpf->framenum,
                   gps->mat_nr, gpd->totcol);
          return buf;
        }
        if (gps->dvert && gps->totpoints <= 0) return "dvert on an empty stroke";
        /* Arrays sized to the points (with slack): an oversized block is copied whole into every
         * undo snapshot by MEM_dupallocN. */
        if (gps->points && MEM_allocN_len(gps->points) > 2 * sizeof(bGPDspoint) * (size_t)gps->totpoints + 4096) {
          snprintf(buf, sizeof(buf), "layer %s frame %d: %d points in a %zu byte points block", gpl->info, gpf->framenum,
                   gps->totpoints, MEM_allocN_len(gps->points));
          return buf;
        }
        if (gps->dvert && MEM_allocN_len(gps->dvert) > 2 * sizeof(MDeformVert) * (size_t)gps->totpoints + 4096) {
          snprintf(buf, sizeof(buf), "layer %s frame %d: %d points in a %zu byte dvert block", gpl->info, gpf->framenum,
                   gps->totpoints, MEM_allocN_len(gps->dvert));
          return buf;
        }
        /* The document holds originals only (no evaluated copies): runtime back-pointers must be
         * null. Garbage there is an uninitialized allocation that later code dereferences. */
        if (gps->runtime.gps_orig != nullptr) {
          snprintf(buf, sizeof(buf), "layer %s frame %d: stroke runtime.gps_orig %p (uninitialized)", gpl->info, gpf->framenum,
                   (void *)gps->runtime.gps_orig);
          return buf;
        }
        for (int i = 0; i < gps->totpoints; i++) {
          const bGPDspoint &p = gps->points[i];
          if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(p.pressure) ||
              !std::isfinite(p.strength)) {
            snprintf(buf, sizeof(buf), "layer %s frame %d: point %d not finite (%g, %g, %g p %g s %g)", gpl->info,
                     gpf->framenum, i, p.x, p.y, p.z, p.pressure, p.strength);
            return buf;
          }
          if (p.runtime.pt_orig != nullptr) {
            snprintf(buf, sizeof(buf), "layer %s frame %d: point %d runtime.pt_orig %p (uninitialized)", gpl->info,
                     gpf->framenum, i, (void *)p.runtime.pt_orig);
            return buf;
          }
          for (int c = 0; c < 4; c++) {
            if (!std::isfinite(p.vert_color[c])) {
              snprintf(buf, sizeof(buf), "layer %s frame %d: point %d vertex colour not finite", gpl->info, gpf->framenum, i);
              return buf;
            }
          }
        }
      }
    }
  }
  return "";
}

static int largest_stroke(const bGPdata *gpd)
{
  int n = 0;
  if (!gpd) return 0;
  for (const bGPDlayer *gpl = (const bGPDlayer *)gpd->layers.first; gpl; gpl = gpl->next)
    for (const bGPDframe *gpf = (const bGPDframe *)gpl->frames.first; gpf; gpf = gpf->next)
      for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) n = std::max(n, gps->totpoints);
  return n;
}

/* All points of the document (every layer and frame), for the size bound below. */
static long rss_mb()
{
  long pages = 0, rss = 0;
  if (FILE *f = fopen("/proc/self/statm", "r")) { if (fscanf(f, "%ld %ld", &pages, &rss) != 2) rss = 0; fclose(f); }
  return rss * 4096 / 1048576;
}

static long document_points(const bGPdata *gpd)
{
  long n = 0;
  if (!gpd) return 0;
  for (const bGPDlayer *gpl = (const bGPDlayer *)gpd->layers.first; gpl; gpl = gpl->next)
    for (const bGPDframe *gpf = (const bGPDframe *)gpl->frames.first; gpf; gpf = gpf->next)
      for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) n += gps->totpoints;
  return n;
}

/* Memory watchdog: when RSS passes PG_FUZZ_RSS_MB the main thread prints where it is. */
static pthread_t g_main_thread;
static void on_rss(int)
{
  static const char msg[] = "FAIL memory runaway (RSS watchdog) during: ";
  (void)!write(2, msg, sizeof(msg) - 1);
  (void)!write(2, g_current_op, strlen(g_current_op));
  (void)!write(2, "\n", 1);
  if (__sanitizer_print_stack_trace) __sanitizer_print_stack_trace();
  _exit(4);
}
static void *rss_monitor(void *)
{
  const long limit = getenv("PG_FUZZ_RSS_MB") ? atol(getenv("PG_FUZZ_RSS_MB")) : 1500;
  for (;;) {
    usleep(50000);
    if (rss_mb() > limit) { pthread_kill(g_main_thread, SIGUSR1); return nullptr; }
  }
}

int main(int argc, char **argv)
{
  g_main_thread = pthread_self();
  signal(SIGUSR1, on_rss);
  pthread_t monitor;
  pthread_create(&monitor, nullptr, rss_monitor, nullptr);
  const long iterations = argc > 1 ? atol(argv[1]) : 20000;
  const unsigned seed = argc > 2 ? unsigned(atol(argv[2])) : 20240501u;
  const bool log = getenv("PG_FUZZ_LOG") != nullptr;
  rng.seed(seed);
  signal(SIGALRM, on_alarm);
  if (getenv("PG_FUZZ_OP_SECONDS")) kOpSeconds = unsigned(atoi(getenv("PG_FUZZ_OP_SECONDS")));
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  g_handle = h;
  if (!h) { fprintf(stderr, "FAIL could not create a document\n"); return 1; }
  project_grease_gp_create_material(h);
  project_grease_gp_create_material(h);
  for (int i = 0; i < 6; i++) draw_stroke(h);
  if (getenv("PG_FUZZ_COVERAGE")) {
    const float all = 1.0f; /* SEL_SELECT */
    project_grease_gp_apply_edit_command(h, 1, &all, 1);
    printf("setup: %d strokes, %d points, %d selected\n", project_grease_gp_stroke_count(h),
           project_grease_gp_point_count(h), project_grease_gp_selected_point_count(h));
    const float d[2] = {5.0f, 5.0f};
    printf("setup: translate -> %d\n", project_grease_gp_apply_edit_command(h, 32, d, 2));
  }

  long op_count[8] = {0};
  long ok_count[131] = {0}, tried[131] = {0};
  double slowest_ms = 0.0;
  std::string slowest;
  std::string last_ops[16];
  for (long it = 0; it < iterations; it++) {
    char desc[512];
    const long rss_before = rss_mb();
    const auto op_start = std::chrono::steady_clock::now();
    const int kind = irand(0, 99);
    if (kind < 85) {
      const int cmd = irand(0, 130);
      const int n = irand(0, 12);
      const bool sane = irand(0, 1) == 0;
      float args[12];
      int off = snprintf(desc, sizeof(desc), "edit %d(", cmd);
      for (int i = 0; i < n; i++) {
        args[i] = sane ? sane_value() : arg_value();
        off += snprintf(desc + off, sizeof(desc) - off, "%s%g", i ? "," : "", args[i]);
      }
      snprintf(desc + off, sizeof(desc) - off, ")");
      if (log) fprintf(stderr, "%ld %s\n", it, desc);
      last_ops[it % 16] = desc;
      /* keep something to edit: random deletes empty the frame quickly */
      while (project_grease_gp_stroke_count(h) < 4) {
        const int before = project_grease_gp_stroke_count(h);
        draw_stroke(h);
        if (project_grease_gp_stroke_count(h) == before) break; /* locked / hidden layer */
      }
      if (irand(0, 2) != 0) { const float select = 1.0f; project_grease_gp_apply_edit_command(h, 1, &select, 1); }
      tried[cmd]++;
      snprintf(g_current_op, sizeof(g_current_op), "seed %u iteration %ld %s", seed, it, desc);
      alarm(kOpSeconds);
      if (project_grease_gp_apply_edit_command(h, cmd, n ? args : nullptr, n)) ok_count[cmd]++;
      if (irand(0, 3) == 0) project_grease_gp_history_record(h);
      op_count[0]++;
    }
    else {
      const int op = irand(0, 11);
      if (log) fprintf(stderr, "%ld (structural op %d starting)\n", it, op);
      snprintf(g_current_op, sizeof(g_current_op), "seed %u iteration %ld structural op %d", seed, it, op);
      switch (op) {
        case 0: draw_stroke(h); snprintf(desc, sizeof(desc), "draw"); break;
        case 1: project_grease_gp_history_undo(h); snprintf(desc, sizeof(desc), "undo"); break;
        case 2: project_grease_gp_history_redo(h); snprintf(desc, sizeof(desc), "redo"); break;
        case 3: { const int f = irand(-2, 30); project_grease_gp_create_frame(h, f); snprintf(desc, sizeof(desc), "create_frame %d", f); break; }
        case 4: { const int f = irand(-2, 30); project_grease_gp_select_frame_or_hold(h, f); snprintf(desc, sizeof(desc), "select_frame %d", f); break; }
        case 5: { const int f = irand(-2, 30); project_grease_gp_delete_frame(h, f); snprintf(desc, sizeof(desc), "delete_frame %d", f); break; }
        case 6: { project_grease_gp_create_layer(h, "Fuzz"); snprintf(desc, sizeof(desc), "create_layer"); break; }
        case 7: { const int l = irand(-1, 4); project_grease_gp_select_layer(h, l); snprintf(desc, sizeof(desc), "select_layer %d", l); break; }
        case 8: { const int l = irand(-1, 4); project_grease_gp_delete_layer(h, l); snprintf(desc, sizeof(desc), "delete_layer %d", l); break; }
        case 9: { const int a = irand(-1, 8), b = irand(-1, 8);
                  project_grease_gp_interpolate_frame(h, a, b, irand(-1, 10), std::uniform_real_distribution<float>(-0.5f, 1.5f)(rng));
                  snprintf(desc, sizeof(desc), "interpolate %d %d", a, b); break; }
        case 10: { const int s = irand(-1, 4); project_grease_gp_duplicate_frame(h, s, irand(-1, 12)); snprintf(desc, sizeof(desc), "duplicate_frame %d", s); break; }
        default: { project_grease_gp_create_material(h); snprintf(desc, sizeof(desc), "create_material"); break; }
      }
      /* mostly come back to an editable keyframe, so the edit commands have strokes to act on */
      if (irand(0, 3) != 0) { project_grease_gp_select_layer(h, 0); project_grease_gp_select_frame_or_hold(h, 1); }
      if (log) fprintf(stderr, "%ld %s\n", it, desc);
      last_ops[it % 16] = desc;
      snprintf(g_current_op, sizeof(g_current_op), "seed %u iteration %ld %s", seed, it, desc);
      alarm(kOpSeconds);
      if (log) fprintf(stderr, "%ld history_record\n", it);
      snprintf(g_current_op, sizeof(g_current_op), "seed %u iteration %ld history_record after %s", seed, it, desc);
      if (getenv("PG_FUZZ_STOP_AT") && it == atol(getenv("PG_FUZZ_STOP_AT"))) raise(SIGTRAP);
      project_grease_gp_history_record(h);
      op_count[1]++;
    }
    alarm(0);
    if (const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - op_start).count(); ms > slowest_ms) {
      slowest_ms = ms;
      slowest = last_ops[it % 16] + " (largest stroke " + std::to_string(largest_stroke(project_grease_gp_document_data(h))) + " points)";
    }
    if (getenv("PG_FUZZ_COVERAGE") && it % 1000 == 999) {
      long pages = 0, rss = 0;
      if (FILE *f = fopen("/proc/self/statm", "r")) { if (fscanf(f, "%ld %ld", &pages, &rss) != 2) rss = 0; fclose(f); }
      const bGPdata *d = project_grease_gp_document_data(h);
      int layers = 0, frames = 0, strokes = 0;
      for (const bGPDlayer *l = (const bGPDlayer *)d->layers.first; l; l = l->next, layers++)
        for (const bGPDframe *f = (const bGPDframe *)l->frames.first; f; f = f->next, frames++)
          for (const bGPDstroke *s2 = (const bGPDstroke *)f->strokes.first; s2; s2 = s2->next) strokes++;
      printf("stats it=%ld rss_mb=%ld layers=%d frames=%d strokes=%d points=%ld materials=%d\n", it, rss * 4096 / 1048576,
             layers, frames, strokes, document_points(d), d->totcol);
      fflush(stdout);
    }
    const std::string problem = check_document(project_grease_gp_document_data(h));
    if (!problem.empty()) {
      fprintf(stderr, "FAIL iteration %ld (seed %u): corrupted document: %s\nlast operations:\n", it, seed, problem.c_str());
      for (int k = 15; k >= 0; k--) {
        const long j = it - k;
        if (j >= 0) fprintf(stderr, "  %ld %s\n", j, last_ops[j % 16].c_str());
      }
      return 1;
    }
    /* keep the document small enough for the sanitizers, and start over now and then (a random
     * command can lock / hide the only layer, after which every edit is refused) */
    if (document_points(project_grease_gp_document_data(h)) > 40000 || it % 1500 == 1499) {
      project_grease_gp_reset_document(h);
      for (int i = 0; i < 4; i++) draw_stroke(h);
    }
  }
  project_grease_gp_destroy(h);
  if (getenv("PG_FUZZ_COVERAGE")) {
    printf("edit ids that never applied:");
    for (int c = 0; c <= 130; c++) if (tried[c] && !ok_count[c]) printf(" %d", c);
    printf("\n");
  }
  printf("slowest operation %.0f ms: %s\n", slowest_ms, slowest.c_str());
  printf("PASS fuzz_edit_commands: %ld edit commands, %ld other operations, seed %u\n", op_count[0], op_count[1], seed);
  return 0;
}
