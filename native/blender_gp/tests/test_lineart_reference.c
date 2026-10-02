/* Compares Line Art on Scene-lite with Blender 3.6.23's own Line Art output for the scenes of
 * tools/lineart_reference/scenes.txt (reference files written by blender_reference.py).
 *
 * Blender's Line Art writes chained strokes (world-space points); this port stops before
 * chaining, so the comparison is geometric in Line Art's frame-buffer space: every Blender stroke
 * point must lie on a visible segment of ours (Blender -> ours), and every visible segment of ours
 * must lie on a Blender stroke (ours -> Blender), both within TOLERANCE, for at least
 * MIN_COVERAGE of the samples. "Visible" is occlusion level 0..level_end, as the modifier selects
 * with level_start 0. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project_grease_lineart_lite.h"
#include "project_grease_scene_lite.h"

#ifndef PG_LINEART_SCENES
#  define PG_LINEART_SCENES "tools/lineart_reference"
#endif

#define TOLERANCE 0.002    /* frame-buffer units (-1..1): about 2 px across 1920 */
#define SAMPLE_STEP 0.004  /* sampling step along our segments */
#define MIN_COVERAGE 0.99

typedef struct Seg { double x0, y0, x1, y1; int object, type, occlusion; } Seg;
typedef struct SegList { Seg *v; int n, cap; } SegList;

static void push(SegList *l, double x0, double y0, double x1, double y1)
{
  if (l->n == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 256;
    l->v = realloc(l->v, sizeof(Seg) * (size_t)l->cap);
  }
  l->v[l->n++] = (Seg){x0, y0, x1, y1, -1, 0, 0};
}

static double dist_point_seg(double px, double py, const Seg *s)
{
  const double dx = s->x1 - s->x0, dy = s->y1 - s->y0;
  const double len2 = dx * dx + dy * dy;
  double t = len2 > 0 ? ((px - s->x0) * dx + (py - s->y0) * dy) / len2 : 0;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  return hypot(px - (s->x0 + t * dx), py - (s->y0 + t * dy));
}

static double nearest(const SegList *l, double x, double y)
{
  double best = 1e30;
  for (int i = 0; i < l->n; i++) {
    const double d = dist_point_seg(x, y, &l->v[i]);
    if (d < best) best = d;
  }
  return best;
}

static char *read_file(const char *path, int *r_len)
{
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = malloc((size_t)n + 1);
  if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
  fclose(f);
  if (buf) { buf[n] = '\0'; *r_len = (int)n; }
  return buf;
}

/* Blender strokes -> frame-buffer polylines (consecutive points). Returns 0 when unreadable. */
static int load_reference(const char *path, const double vp[4][4], double sx, double sy, SegList *out, int *r_strokes)
{
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  char line[256];
  int remaining = 0, have_prev = 0;
  double prev[2] = {0, 0};
  *r_strokes = 0;
  while (fgets(line, sizeof(line), f)) {
    int count;
    if (line[0] == '#') continue;
    if (sscanf(line, "stroke %d", &count) == 1) {
      remaining = count;
      have_prev = 0;
      (*r_strokes)++;
      continue;
    }
    float co[3];
    if (remaining > 0 && sscanf(line, "%f %f %f", &co[0], &co[1], &co[2]) == 3) {
      double fb[2];
      remaining--;
      if (!pg_lite_project(vp, sx, sy, co, fb)) { have_prev = 0; continue; }
      if (have_prev) push(out, prev[0], prev[1], fb[0], fb[1]);
      else push(out, fb[0], fb[1], fb[0], fb[1]); /* keeps single-point strokes as samples */
      prev[0] = fb[0];
      prev[1] = fb[1];
      have_prev = 1;
    }
  }
  fclose(f);
  return 1;
}

/* Blender strokes in world space: points (x, y, z) and per-stroke [first, count). */
typedef struct WorldStrokes { float *pts; int npts, cap; int *first, *count; int n, scap; } WorldStrokes;

static int load_world_strokes(const char *path, WorldStrokes *w)
{
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    int c;
    float co[3];
    if (line[0] == '#') continue;
    if (sscanf(line, "stroke %d", &c) == 1) {
      if (w->n == w->scap) {
        w->scap = w->scap ? w->scap * 2 : 64;
        w->first = realloc(w->first, sizeof(int) * (size_t)w->scap);
        w->count = realloc(w->count, sizeof(int) * (size_t)w->scap);
      }
      w->first[w->n] = w->npts;
      w->count[w->n] = 0;
      w->n++;
    }
    else if (w->n > 0 && sscanf(line, "%f %f %f", &co[0], &co[1], &co[2]) == 3) {
      if (w->npts == w->cap) {
        w->cap = w->cap ? w->cap * 2 : 256;
        w->pts = realloc(w->pts, sizeof(float) * 3 * (size_t)w->cap);
      }
      memcpy(&w->pts[w->npts * 3], co, sizeof(co));
      w->npts++;
      w->count[w->n - 1]++;
    }
  }
  fclose(f);
  return 1;
}

/* Largest point distance between two equally long point runs, forward or reversed (best of both). */
static double stroke_distance(const float *a, const float *b, int n)
{
  double fwd = 0, rev = 0;
  for (int i = 0; i < n; i++) {
    double df = 0, dr = 0;
    for (int k = 0; k < 3; k++) {
      df += (a[i * 3 + k] - b[i * 3 + k]) * (a[i * 3 + k] - b[i * 3 + k]);
      dr += (a[i * 3 + k] - b[(n - 1 - i) * 3 + k]) * (a[i * 3 + k] - b[(n - 1 - i) * 3 + k]);
    }
    if (df > fwd) fwd = df;
    if (dr > rev) rev = dr;
  }
  return sqrt(fwd < rev ? fwd : rev);
}

#define STROKE_TOLERANCE 1e-3 /* world units */

/* Strokes: every Blender stroke must equal one of ours point for point (forward or reversed). */
static int compare_strokes(const char *ref_dir, const char *name, const PGSceneLite *scene, const PGLineartSettings *base)
{
  PGLineartSettings st = *base;
  st.stroke_depth_offset = 0.0f; /* as in blender_reference.py */
  PGLineartStrokes ours;
  if (pg_lineart_compute_strokes(scene, &st, &ours) < 0) {
    printf("FAIL: strokes %s: compute failed\n", name);
    return 1;
  }
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s.txt", ref_dir, name);
  WorldStrokes w = {0};
  load_world_strokes(path, &w);
  char *used = calloc((size_t)(ours.stroke_count > 0 ? ours.stroke_count : 1), 1);
  int matched = 0;
  for (int i = 0; i < w.n; i++) {
    int found = -1;
    for (int j = 0; j < ours.stroke_count && found < 0; j++) {
      if (used[j] || ours.strokes[j].point_count != w.count[i]) continue;
      if (stroke_distance(&w.pts[w.first[i] * 3], &ours.world[ours.strokes[j].first * 3], w.count[i]) <= STROKE_TOLERANCE) {
        found = j;
      }
    }
    if (found >= 0) {
      used[found] = 1;
      matched++;
    }
    else {
      const float *p = &w.pts[w.first[i] * 3];
      printf("        Blender stroke %d (%d points, from %.4f %.4f %.4f) has no equal stroke of ours\n", i, w.count[i], p[0], p[1], p[2]);
    }
  }
  for (int j = 0; j < ours.stroke_count; j++) {
    if (!used[j]) {
      const float *p = &ours.world[ours.strokes[j].first * 3];
      printf("        our stroke %d (%d points, type 0x%x, level %d, from %.4f %.4f %.4f) is not in Blender's output\n", j,
             ours.strokes[j].point_count, ours.strokes[j].edge_type, ours.strokes[j].level, p[0], p[1], p[2]);
    }
  }
  const int ok = matched == w.n && matched == ours.stroke_count;
  printf("%s strokes   %-16s Blender %3d / ours %3d strokes, %d identical\n", ok ? "  ok  " : "FAIL: ", name, w.n, ours.stroke_count, matched);
  free(used);
  free(w.pts);
  free(w.first);
  free(w.count);
  pg_lineart_free_strokes(&ours);
  return ok ? 0 : 1;
}

static int compare_scene(const char *ref_dir, char **f)
{
  /* name obj camera lens ortho_scale yaw pitch distance shift_x shift_y width height level_end */
  char path[1024];
  snprintf(path, sizeof(path), "%s/scenes/%s", PG_LINEART_SCENES, f[1]);
  int len = 0;
  char *obj = read_file(path, &len);
  if (!obj) { printf("FAIL: reference scene %s: cannot read %s\n", f[0], path); return 1; }
  PGSceneLite *scene = pg_lite_scene_create();
  pg_lite_load_obj(scene, obj, len);
  free(obj);
  PGCameraLite *cam = &scene->camera;
  cam->type = strcmp(f[2], "ortho") == 0 ? 1 : 0;
  cam->lens = (float)atof(f[3]);
  cam->ortho_scale = (float)atof(f[4]);
  cam->shift_x = (float)atof(f[8]);
  cam->shift_y = (float)atof(f[9]);
  const float target[3] = {0, 0, 0};
  pg_lite_camera_orbit(cam, target, (float)atof(f[5]), (float)atof(f[6]), (float)atof(f[7]));
  scene->width = atoi(f[10]);
  scene->height = atoi(f[11]);

  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.overscan = 0.0f;
  st.level_end = atoi(f[12]);

  PGLineartSegment *seg = NULL;
  const int n = pg_lineart_compute(scene, &st, &seg);
  SegList ours = {0}, theirs = {0}, all = {0};
  for (int i = 0; i < n; i++) {
    push(&all, seg[i].x0, seg[i].y0, seg[i].x1, seg[i].y1);
    all.v[all.n - 1].object = seg[i].object_index;
    all.v[all.n - 1].type = seg[i].edge_type;
    all.v[all.n - 1].occlusion = seg[i].occlusion;
    if (seg[i].occlusion < 0 || seg[i].occlusion > st.level_end) continue;
    push(&ours, seg[i].x0, seg[i].y0, seg[i].x1, seg[i].y1);
    ours.v[ours.n - 1].object = seg[i].object_index;
    ours.v[ours.n - 1].type = seg[i].edge_type;
    ours.v[ours.n - 1].occlusion = seg[i].occlusion;
  }
  pg_lineart_free_segments(seg);

  double vp[4][4], sx, sy;
  pg_lite_view_projection(cam, scene->width, scene->height, 0.0f, vp);
  pg_lite_camera_shift(cam, scene->width, scene->height, &sx, &sy);
  snprintf(path, sizeof(path), "%s/%s.txt", ref_dir, f[0]);
  int strokes = 0;
  if (!load_reference(path, vp, sx, sy, &theirs, &strokes)) {
    printf("FAIL: reference scene %s: no Blender output at %s\n", f[0], path);
    pg_lite_scene_free(scene);
    free(ours.v);
    free(all.v);
    return 1;
  }

  /* Blender -> ours: samples along Blender's strokes, as dense as the other direction. */
  int a_total = 0, a_hit = 0;
  for (int i = 0; i < theirs.n; i++) {
    const Seg *s = &theirs.v[i];
    const int steps = (int)(hypot(s->x1 - s->x0, s->y1 - s->y0) / SAMPLE_STEP) + 1;
    for (int k = 1; k <= steps; k++) {
      const double t = (double)k / steps;
      const double px[1] = {s->x0 + (s->x1 - s->x0) * t}, py[1] = {s->y0 + (s->y1 - s->y0) * t};
      const int kk = 0;
      if (fabs(px[kk]) > 1.0 || fabs(py[kk]) > 1.0) continue; /* outside the frame */
      a_total++;
      if (nearest(&ours, px[kk], py[kk]) <= TOLERANCE) {
        a_hit++;
      }
      else if (a_total - a_hit <= 40) {
        /* Diagnostics: where Blender draws something we do not report as visible. */
        int best = -1;
        double bd = 1e30;
        for (int j = 0; j < all.n; j++) {
          const double d = dist_point_seg(px[kk], py[kk], &all.v[j]);
          if (d < bd) { bd = d; best = j; }
        }
        printf("        Blender sample not ours: (%.4f, %.4f) nearest of all ours %.4f away: object %d type 0x%x occlusion %d\n",
               px[kk], py[kk], bd, best >= 0 ? all.v[best].object : -9, best >= 0 ? all.v[best].type : 0,
               best >= 0 ? all.v[best].occlusion : -1);
      }
    }
  }
  /* ours -> Blender: samples along our visible segments. */
  int b_total = 0, b_hit = 0;
  for (int i = 0; i < ours.n; i++) {
    const Seg *s = &ours.v[i];
    const double l = hypot(s->x1 - s->x0, s->y1 - s->y0);
    const int steps = (int)(l / SAMPLE_STEP) + 1;
    int seg_total = 0, seg_hit = 0;
    for (int k = 0; k <= steps; k++) {
      const double t = (double)k / steps;
      const double x = s->x0 + (s->x1 - s->x0) * t, y = s->y0 + (s->y1 - s->y0) * t;
      if (fabs(x) > 1.0 || fabs(y) > 1.0) continue;
      seg_total++;
      seg_hit += nearest(&theirs, x, y) <= TOLERANCE;
    }
    b_total += seg_total;
    b_hit += seg_hit;
    if (seg_hit < seg_total) {
      /* Diagnostics: which of our visible segments Blender does not draw. */
      printf("        not in Blender: object %d type 0x%x occlusion %d (%.4f, %.4f)-(%.4f, %.4f) %d/%d samples\n",
             s->object, s->type, s->occlusion, s->x0, s->y0, s->x1, s->y1, seg_total - seg_hit, seg_total);
    }
  }

  const double ca = a_total ? (double)a_hit / a_total : 1.0;
  const double cb = b_total ? (double)b_hit / b_total : 1.0;
  const int ok = ca >= MIN_COVERAGE && cb >= MIN_COVERAGE && (a_total > 0) == (b_total > 0);
  printf("%s reference %-16s Blender %3d strokes / ours %4d visible segments: Blender->ours %.4f, ours->Blender %.4f\n",
         ok ? "  ok  " : "FAIL: ", f[0], strokes, ours.n, ca, cb);
  const int strokes_failed = compare_strokes(ref_dir, f[0], scene, &st);
  free(ours.v);
  free(theirs.v);
  free(all.v);
  pg_lite_scene_free(scene);
  return (ok ? 0 : 1) + strokes_failed;
}

int pg_lineart_reference_compare(const char *ref_dir)
{
  char path[1024];
  snprintf(path, sizeof(path), "%s/scenes.txt", PG_LINEART_SCENES);
  FILE *m = fopen(path, "r");
  if (!m) { printf("FAIL: cannot read %s\n", path); return 1; }
  char line[512];
  int failures = 0, scenes = 0;
  while (fgets(line, sizeof(line), m)) {
    if (line[0] == '#' || line[0] == '\n') continue;
    char *f[13];
    int k = 0;
    for (char *tok = strtok(line, " \t\r\n"); tok && k < 13; tok = strtok(NULL, " \t\r\n")) f[k++] = tok;
    if (k != 13) continue;
    scenes++;
    failures += compare_scene(ref_dir, f);
  }
  fclose(m);
  if (scenes == 0) { printf("FAIL: no reference scenes\n"); failures++; }
  return failures;
}
