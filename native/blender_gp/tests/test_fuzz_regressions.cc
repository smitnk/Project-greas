/* Regression tests for the native bugs the bug-hunt fuzzer (fuzz_edit_commands.cc) found, run with
 * ASan + UBSan + LeakSanitizer by tools/run_native_fuzz.sh. Each test drives the same entry point
 * the app uses (project_grease_gp_apply_edit_command and the bridge) and checks the document. */
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include "project_grease_gp_bridge.h"
#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit2.h"
#include "project_grease_blender_edit4.h"
#include "project_grease_blender_select.h"
#include "BKE_deform.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "MEM_guardedalloc.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}
extern "C" void BLI_mutex_init(pthread_mutex_t *mutex) { pthread_mutex_init(mutex, nullptr); }

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static bGPDframe *frame(ProjectGreaseGPHandle *h)
{
  bGPdata *gpd = const_cast<bGPdata *>(project_grease_gp_document_data(h));
  return static_cast<bGPDlayer *>(gpd->layers.first)->actframe;
}
static bGPDstroke *first_stroke(ProjectGreaseGPHandle *h) { return static_cast<bGPDstroke *>(frame(h)->strokes.first); }
static int stroke_count(ProjectGreaseGPHandle *h) { int n = 0; for (bGPDstroke *s = first_stroke(h); s; s = s->next) n++; return n; }

/* A stroke of n points along x (optionally zig-zag), drawn through the bridge as the app does. */
static void draw(ProjectGreaseGPHandle *h, int n, float y, bool zigzag = false, float x0 = 100.0f, float step = 4.0f)
{
  project_grease_gp_begin_stroke(h, 0, 10.0f);
  for (int i = 0; i < n; i++) {
    ProjectGreaseGPPoint p{x0 + step * i, y + (zigzag ? ((i % 2) ? 30.0f : -30.0f) : 0.0f), 0, 1, 1, 0.01f * i};
    project_grease_gp_add_point(h, p);
  }
  project_grease_gp_end_stroke(h);
}

static bool apply(ProjectGreaseGPHandle *h, int cmd, std::initializer_list<float> args)
{
  return project_grease_gp_apply_edit_command(h, cmd, args.size() ? args.begin() : nullptr, int(args.size())) != 0;
}
static void select_all(ProjectGreaseGPHandle *h) { apply(h, 1, {1.0f}); }

/* Gives every point of the stroke a weight in group 0 equal to its index (to check alignment). */
static void weight_by_index(bGPDstroke *s)
{
  s->dvert = static_cast<MDeformVert *>(MEM_callocN(sizeof(MDeformVert) * s->totpoints, "test dvert"));
  for (int i = 0; i < s->totpoints; i++) {
    MDeformWeight *dw = BKE_defvert_ensure_index(&s->dvert[i], 0);
    dw->weight = float(i);
  }
}

/* 1. Non-finite / out-of-range arguments are refused for every command (UB: inf -> int). */
static void test_non_finite_args_refused()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 10, 200);
  select_all(h);
  const float x0 = first_stroke(h)->points[0].x;
  for (int cmd = 0; cmd <= 130; cmd++) {
    const float bad[3][4] = {{INFINITY, 1, 1, 1}, {NAN, 1, 1, 1}, {1e10f, 1, 1, 1}};
    for (const auto &a : bad) CHECK(project_grease_gp_apply_edit_command(h, cmd, a, 4) == 0);
  }
  CHECK(stroke_count(h) == 1 && first_stroke(h)->points[0].x == x0);
  project_grease_gp_destroy(h);
}

/* 2. Dissolve keeps the weights with their points and frees a stroke left without points. */
static void test_dissolve_weights_and_empty_stroke()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 6, 200);
  draw(h, 4, 400);
  bGPDstroke *a = first_stroke(h), *b = a->next;
  weight_by_index(a);
  weight_by_index(b);
  for (int i = 0; i < a->totpoints; i++) a->points[i].flag = (i % 2) ? GP_SPOINT_SELECT : 0; /* odd points */
  for (int i = 0; i < b->totpoints; i++) b->points[i].flag = GP_SPOINT_SELECT;            /* all */
  CHECK(apply(h, 4, {}));
  CHECK(stroke_count(h) == 1);                                 /* the emptied stroke is gone */
  a = first_stroke(h);
  CHECK(a->totpoints == 3);
  for (int i = 0; i < a->totpoints; i++) CHECK(a->dvert[i].totweight == 1 && a->dvert[i].dw[0].weight == float(2 * i));
  project_grease_gp_destroy(h);
}

/* 3. Resample: a spacing below float resolution at the stroke's position, or a result past the
 *    per-stroke ceiling, leaves the stroke as it is (was: endless loop / billions of points). */
static void test_sample_bounded()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 5, 200);
  select_all(h);
  CHECK(apply(h, PG_EDIT_CMD_TRANSLATE, {9.0e8f, 0.0f}));     /* far out: a float step is 64 units */
  const auto t0 = std::chrono::steady_clock::now();
  apply(h, PG_EDIT2_CMD_SAMPLE, {0.33f, 0.0f});
  CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(5));
  CHECK(first_stroke(h)->totpoints == 5);
  project_grease_gp_reset_document(h);                         /* a fresh stroke near the origin */
  draw(h, 5, 200);
  select_all(h);
  apply(h, PG_EDIT2_CMD_SAMPLE, {1.0e-4f, 0.0f});              /* 16 / 1e-4 = 160000 points */
  CHECK(first_stroke(h)->totpoints <= PG_MAX_STROKE_POINTS);
  CHECK(apply(h, PG_EDIT2_CMD_SAMPLE, {2.0f, 0.0f}));          /* an ordinary resample still works */
  CHECK(first_stroke(h)->totpoints > 5);
  project_grease_gp_destroy(h);
}

/* 4. Length modifier: factor x density past the ceiling leaves the stroke alone (was 842000 points). */
static void test_length_bounded()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 8, 200);
  select_all(h);
  /* mode, start, end, overshoot, curvature, density, segment influence, max angle, invert */
  apply(h, PG_EDIT_CMD_MOD_LENGTH, {1, 943.0f, 6.0f, 0.3f, 1, 892.0f, 0.0f, 1.0f, 0});
  CHECK(first_stroke(h)->totpoints <= PG_MAX_STROKE_POINTS);
  const float x_end = first_stroke(h)->points[first_stroke(h)->totpoints - 1].x;
  CHECK(apply(h, PG_EDIT_CMD_MOD_LENGTH, {0, 0.5f, 0.5f, 0.1f, 0, 30.0f, 0.0f, 1.0f, 0})); /* relative: +50% */
  /* an ordinary extension still works (without curvature Blender moves the end points out) */
  CHECK(first_stroke(h)->points[first_stroke(h)->totpoints - 1].x > x_end + 1.0f);
  project_grease_gp_destroy(h);
}

/* 5. Join, then box select: the joined points' runtime back-pointers are clear (were 0xbebebebe). */
static void test_join_runtime_clear()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 5, 200);
  draw(h, 5, 260);
  select_all(h);
  CHECK(apply(h, PG_EDIT_CMD_JOIN, {0}));
  CHECK(stroke_count(h) == 1);
  bGPDstroke *s = first_stroke(h);
  for (int i = 0; i < s->totpoints; i++) CHECK(s->points[i].runtime.pt_orig == nullptr);
  CHECK(apply(h, PG_SELECT_CMD_BOX, {0, 0, 0, 0, 2000, 2000}) || true);
  project_grease_gp_destroy(h);
}

/* 6. Delete points / dash give arrays sized to their points (were whole source blocks, copied
 *    into every undo snapshot: gigabytes). */
static void test_arrays_sized_to_points()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 4000, 200, false, 10.0f, 0.25f);
  bGPDstroke *s = first_stroke(h);
  s->flag |= GP_STROKE_SELECT;
  for (int i = 0; i < s->totpoints; i++) s->points[i].flag = (i < 3990) ? GP_SPOINT_SELECT : 0;
  CHECK(apply(h, PG_EDIT_CMD_DELETE_POINTS, {}));
  s = first_stroke(h);
  CHECK(s->totpoints == 10);
  CHECK(MEM_allocN_len(s->points) <= sizeof(bGPDspoint) * 10 + 64);
  project_grease_gp_reset_document(h);
  draw(h, 4000, 300, false, 10.0f, 0.25f);
  select_all(h);
  CHECK(apply(h, PG_EDIT4_CMD_DASH, {4, 2, 0}));
  for (bGPDstroke *p = first_stroke(h); p; p = p->next) CHECK(MEM_allocN_len(p->points) <= sizeof(bGPDspoint) * p->totpoints + 64);
  project_grease_gp_destroy(h);
}

/* 7. Merge and join keep vertex-group weights with their points (merge misaligned them; join
 *    dropped them and leaked every weight list). */
static void test_merge_and_join_weights()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 6, 200, false, 100.0f, 0.1f);               /* points 0.1 apart */
  weight_by_index(first_stroke(h));
  select_all(h);
  CHECK(apply(h, 5, {0.25f}));                        /* merge: interior points collapse */
  bGPDstroke *s = first_stroke(h);
  CHECK(s->totpoints < 6);
  CHECK(MEM_allocN_len(s->dvert) == sizeof(MDeformVert) * s->totpoints);
  CHECK(s->dvert[s->totpoints - 1].dw[0].weight == 5.0f);  /* the last point keeps its own weight */
  project_grease_gp_reset_document(h);
  draw(h, 3, 200);
  draw(h, 2, 300);
  weight_by_index(first_stroke(h));
  weight_by_index(first_stroke(h)->next);
  select_all(h);
  CHECK(apply(h, 7, {}));                             /* backend join */
  s = first_stroke(h);
  CHECK(s->totpoints == 5 && s->dvert != nullptr);
  if (s->dvert) {
    const float expect[5] = {0, 1, 2, 0, 1};
    for (int i = 0; i < 5; i++) CHECK(s->dvert[i].totweight == 1 && s->dvert[i].dw[0].weight == expect[i]);
  }
  project_grease_gp_destroy(h);                       /* LeakSanitizer: nothing left behind */
}

/* 8. A document with weights and an open stroke buffer frees everything (new project / load). */
static void test_document_free_releases_weights_and_buffer()
{
  ProjectGreaseGPHandle *h = project_grease_gp_create();
  draw(h, 5, 200);
  weight_by_index(first_stroke(h));
  project_grease_gp_begin_stroke(h, 0, 4.0f);         /* the stroke buffer is allocated */
  ProjectGreaseGPPoint p{1, 1, 0, 1, 1, 0};
  project_grease_gp_add_point(h, p);
  project_grease_gp_reset_document(h);                /* was: sbuffer and every dw leaked */
  project_grease_gp_destroy(h);
}

int main()
{
  test_non_finite_args_refused();
  test_dissolve_weights_and_empty_stroke();
  test_sample_bounded();
  test_length_bounded();
  test_join_runtime_clear();
  test_arrays_sized_to_points();
  test_merge_and_join_weights();
  test_document_free_releases_weights_and_buffer();
  if (failures) { printf("%d FAILURES\n", failures); return 1; }
  printf("fuzz regression tests passed\n");
  return 0;
}
