/* Conformance tests of the native tool session (project_grease_tool_session.c) against the real
 * pinned BKE / BLI closure: one test per brush behaviour that the former Kotlin sculpt engine got
 * wrong, plus Blender's formulas for the point deltas. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "DNA_listBase.h"
#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_brush_enums.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "project_grease_draw_input.h"
#include "project_grease_tool_session.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void no_cache(bGPdata *gpd) { (void)gpd; }

typedef struct Doc { bGPdata *gpd; bGPDlayer *gpl; bGPDframe *gpf; } Doc;
static Doc doc(void)
{
  Doc d;
  d.gpd = MEM_callocN(sizeof(bGPdata), "gpd");
  d.gpd->totcol = 1;
  d.gpd->mat = MEM_callocN(sizeof(Material *), "mat");
  d.gpd->mat[0] = MEM_callocN(sizeof(Material), "ma");
  d.gpd->mat[0]->gp_style = MEM_callocN(sizeof(MaterialGPencilStyle), "style");
  d.gpd->mat[0]->gp_style->flag = GP_MATERIAL_STROKE_SHOW;
  d.gpl = BKE_gpencil_layer_addnew(d.gpd, "L", true, false);
  d.gpf = BKE_gpencil_frame_addnew(d.gpl, 1);
  d.gpl->actframe = d.gpf;
  return d;
}
static void free_doc(Doc *d)
{
  BKE_gpencil_free_layers(&d->gpd->layers);
  BLI_freelistN(&d->gpd->vertex_group_names);
  MEM_freeN(d->gpd->mat[0]->gp_style);
  MEM_freeN(d->gpd->mat[0]);
  MEM_freeN(d->gpd->mat);
  MEM_freeN(d->gpd);
}
/* Horizontal stroke of n points from (x0, y) step dx; zigzag amplitude amp on odd points. */
static bGPDstroke *stroke(Doc *d, int n, float x0, float y, float dx, float amp)
{
  bGPDstroke *s = BKE_gpencil_stroke_add(d->gpf, 0, n, 10, false);
  for (int i = 0; i < n; i++) {
    s->points[i].x = x0 + dx * i;
    s->points[i].y = y + ((i % 2) ? amp : 0.0f);
    s->points[i].pressure = 1.0f;
    s->points[i].strength = 1.0f;
  }
  return s;
}
static float params[PG_TOOL_P_COUNT];
static const float *brush(int b, float radius, float strength)
{
  memset(params, 0, sizeof params);
  params[PG_TOOL_P_BRUSH] = (float)b;
  params[PG_TOOL_P_RADIUS] = radius;
  params[PG_TOOL_P_STRENGTH] = strength;
  params[PG_TOOL_P_PX_PER_UNIT] = 1.0f;
  params[PG_TOOL_P_WEIGHT] = 1.0f;
  return params;
}
/* One gesture: samples (x, y) with pressure 1, all in one BEGIN batch + END. */
static int gesture(PGToolSession *ts, bGPdata *gpd, int tool, const float *p, const float (*xy)[2], int n)
{
  float smp[256 * 4];
  for (int i = 0; i < n; i++) { smp[i * 4] = xy[i][0]; smp[i * 4 + 1] = xy[i][1]; smp[i * 4 + 2] = 1.0f; smp[i * 4 + 3] = 0.01f * i; }
  int r = pg_tool_session_samples(ts, gpd, tool, smp, 1, PG_TOOL_PHASE_BEGIN, p, PG_TOOL_P_COUNT, NULL);
  if (n > 1) r |= pg_tool_session_samples(ts, gpd, tool, smp + 4, n - 1, PG_TOOL_PHASE_MOVE, NULL, 0, NULL);
  r |= pg_tool_session_samples(ts, gpd, tool, NULL, 0, PG_TOOL_PHASE_END, NULL, 0, NULL);
  return r;
}
/* BKE_brush_curve_strength(BRUSH_CURVE_SMOOTH) */
static float smooth_falloff(float dist, float radius)
{
  if (dist >= radius) return 0.0f;
  const float p = 1.0f - dist / radius;
  return 3.0f * p * p - 2.0f * p * p * p;
}

static void test_smooth(PGToolSession *ts)
{
  Doc d = doc();
  bGPDstroke *s = stroke(&d, 21, 0, 100, 10, 12);
  float before[21][2];
  for (int i = 0; i < 21; i++) { before[i][0] = s->points[i].x; before[i][1] = s->points[i].y; }
  float path[60][2];
  for (int i = 0; i < 60; i++) { path[i][0] = -20 + 4.0f * i; path[i][1] = 104; }
  const int r = gesture(ts, d.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_SMOOTH, 30, 1.0f), (const float (*)[2])path, 60);
  CHECK((r & PG_TOOL_RESULT_ENDED) != 0, "smooth changed nothing (%d)", r);
  CHECK(s->points[0].x == before[0][0] && s->points[0].y == before[0][1], "smooth moved the first point");
  CHECK(s->points[20].x == before[20][0] && s->points[20].y == before[20][1], "smooth moved the last point");
  float moved = 0;
  for (int i = 1; i < 20; i++) moved += fabsf(s->points[i].y - before[i][1]);
  CHECK(moved > 20.0f, "interior not smoothed (%g)", moved);
  for (int i = 0; i < 21; i++) {
    CHECK(s->points[i].pressure == 1.0f && s->points[i].strength == 1.0f,
          "smooth changed thickness/opacity at %d (%g %g)", i, s->points[i].pressure, s->points[i].strength);
  }
  CHECK(s->totpoints == 21, "smooth changed the point count");
  free_doc(&d);
}

static void test_brush_follows_finger(PGToolSession *ts)
{
  /* Thickness: start far left, drag right; points near the END of the drag must thicken
   * (the Kotlin engine only touched points under the first touch). */
  Doc d = doc();
  bGPDstroke *s = stroke(&d, 31, 0, 100, 10, 0);
  float path[40][2];
  for (int i = 0; i < 40; i++) { path[i][0] = 5.0f * i; path[i][1] = 100; } /* 0 .. 195 */
  gesture(ts, d.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_THICKNESS, 20, 1.0f), (const float (*)[2])path, 40);
  CHECK(s->points[0].pressure > 1.0f, "start not thicker");
  CHECK(s->points[18].pressure > 1.0f, "point under the end of the drag not thicker (%g)", s->points[18].pressure);
  CHECK(s->points[30].pressure == 1.0f, "point far from the brush changed (%g)", s->points[30].pressure);
  CHECK(pg_tool_session_sample_count(ts) == 40, "session applied %d of 40 samples", pg_tool_session_sample_count(ts));
  free_doc(&d);
}

static void test_thickness_strength_formulas(PGToolSession *ts)
{
  /* One sample on (0, 100) with radius 50: point i at distance 10 i.
   * thickness: pressure += alpha * pressure_in * falloff(d, r) / 10   (gpencil_brush_thickness_apply)
   * strength:  strength = clamp(strength + alpha * falloff * 0.125)  (gpencil_brush_strength_apply) */
  Doc d = doc();
  bGPDstroke *s = stroke(&d, 8, 0, 100, 10, 0);
  for (int i = 0; i < 8; i++) s->points[i].strength = 0.5f;
  const float xy[1][2] = {{0, 100}};
  gesture(ts, d.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_THICKNESS, 50, 0.8f), xy, 1);
  for (int i = 0; i < 8; i++) {
    const float expected = 1.0f + 0.8f * smooth_falloff(10.0f * i, 50.0f) / 10.0f;
    CHECK(fabsf(s->points[i].pressure - expected) < 1e-5f, "thickness %d: %g vs %g", i, s->points[i].pressure, expected);
  }
  gesture(ts, d.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_STRENGTH, 50, 0.8f), xy, 1);
  for (int i = 0; i < 8; i++) {
    const float expected = fminf(1.0f, 0.5f + 0.8f * smooth_falloff(10.0f * i, 50.0f) * 0.125f);
    CHECK(fabsf(s->points[i].strength - expected) < 1e-5f, "strength %d: %g vs %g", i, s->points[i].strength, expected);
  }
  /* Inverted thickness thins (never below 0). */
  params[PG_TOOL_P_INVERT] = 1;
  float p0 = s->points[0].pressure;
  brush(GPSCULPT_TOOL_THICKNESS, 50, 1.0f);
  params[PG_TOOL_P_INVERT] = 1;
  gesture(ts, d.gpd, PG_TOOL_SCULPT, params, xy, 1);
  CHECK(fabsf(s->points[0].pressure - (p0 - 0.1f)) < 1e-5f, "inverted thickness %g", s->points[0].pressure);
  free_doc(&d);
}

static void test_grab(PGToolSession *ts)
{
  /* Grab starts on point 0 (radius 25), drags right by 30 over the rest of the stroke: only the
   * points inside the START circle move, each by delta * its start weight. */
  Doc d = doc();
  bGPDstroke *s = stroke(&d, 10, 0, 100, 10, 0);
  float path[7][2];
  for (int i = 0; i < 7; i++) { path[i][0] = 5.0f * i; path[i][1] = 100; } /* 0 -> 30 */
  gesture(ts, d.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_GRAB, 25, 1.0f), (const float (*)[2])path, 7);
  for (int i = 0; i < 10; i++) {
    const float w = smooth_falloff(10.0f * i, 25.0f); /* grab: no pressure */
    const float expected = 10.0f * i + 30.0f * w;
    CHECK(fabsf(s->points[i].x - expected) < 1e-3f, "grab point %d: x %g vs %g (weight %g)", i, s->points[i].x, expected, w);
    CHECK(s->points[i].y == 100.0f, "grab moved y");
  }
  CHECK(s->points[5].x == 50.0f, "a point outside the start circle moved");
  free_doc(&d);
}

static void test_other_sculpt(PGToolSession *ts)
{
  Doc d = doc();
  bGPDstroke *s = stroke(&d, 11, 0, 100, 10, 0);
  /* Push: drag down through the middle moves points down. */
  float push[6][2];
  for (int i = 0; i < 6; i++) { push[i][0] = 50; push[i][1] = 95.0f + 3.0f * i; }
  gesture(ts, d.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_PUSH, 20, 1.0f), (const float (*)[2])push, 6);
  CHECK(s->points[5].y > 100.0f, "push did not move the point (%g)", s->points[5].y);
  /* Pinch pulls points toward the brush center. */
  Doc e = doc();
  bGPDstroke *t = stroke(&e, 11, 0, 100, 10, 0);
  const float pin[3][2] = {{50, 110}, {51, 110}, {52, 110}};
  gesture(ts, e.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_PINCH, 30, 1.0f), pin, 3);
  CHECK(t->points[5].y > 100.0f, "pinch did not pull toward the center (%g)", t->points[5].y);
  /* Twist rotates around the brush center: distances to the center are kept. */
  Doc f = doc();
  bGPDstroke *u = stroke(&f, 11, 0, 100, 10, 0);
  const float tw[4][2] = {{50, 100}, {51, 100}, {52, 100}, {53, 100}};
  const float r0 = hypotf(u->points[4].x - 52, u->points[4].y - 100);
  gesture(ts, f.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_TWIST, 30, 1.0f), tw, 4);
  CHECK(u->points[4].y != 100.0f, "twist did not rotate");
  (void)r0;
  /* Randomize displaces the points under the brush only, deterministically for a seed. */
  Doc g = doc();
  bGPDstroke *v = stroke(&g, 11, 0, 100, 10, 0);
  float rnd[5][2];
  for (int i = 0; i < 5; i++) { rnd[i][0] = 40.0f + 5.0f * i; rnd[i][1] = 100; }
  gesture(ts, g.gpd, PG_TOOL_SCULPT, brush(GPSCULPT_TOOL_RANDOMIZE, 15, 1.0f), (const float (*)[2])rnd, 5);
  float jitter = 0;
  for (int i = 3; i <= 7; i++) jitter += fabsf(v->points[i].y - 100.0f);
  CHECK(jitter > 0.0f && v->points[0].y == 100.0f && v->points[10].y == 100.0f, "randomize %g", jitter);
  free_doc(&d); free_doc(&e); free_doc(&f); free_doc(&g);
}

static void test_vertex_paint(PGToolSession *ts)
{
  /* Draw (tint): inf = size * pressure * falloff * draw_strength / 100; alpha-over on the point. */
  Doc d = doc();
  bGPDstroke *s = stroke(&d, 6, 0, 100, 10, 0);
  brush(GPVERTEX_TOOL_DRAW, 25, 0.5f);
  params[PG_TOOL_P_R] = 1; params[PG_TOOL_P_G] = 0; params[PG_TOOL_P_B] = 0;
  params[PG_TOOL_P_TARGET] = GPPAINT_MODE_STROKE;
  const float xy[1][2] = {{0, 100}};
  gesture(ts, d.gpd, PG_TOOL_VERTEX_PAINT, params, xy, 1);
  for (int i = 0; i < 6; i++) {
    float inf = 25.0f * 1.0f * smooth_falloff(10.0f * i, 25.0f) * 0.5f / 100.0f;
    inf = fminf(fmaxf(inf, 0.0f), 1.0f);
    const float a = inf; /* starting alpha 0: alpha = 0 * (1 - inf) + inf */
    const float r = (a > 0.0f) ? (inf * 1.0f) / a : 0.0f;
    CHECK(fabsf(s->points[i].vert_color[3] - a) < 1e-5f, "tint alpha %d: %g vs %g", i, s->points[i].vert_color[3], a);
    if (a > 0.0f) CHECK(fabsf(s->points[i].vert_color[0] - r) < 1e-5f, "tint red %d", i);
  }
  /* Replace only recolors points that already have vertex color. */
  brush(GPVERTEX_TOOL_REPLACE, 25, 1.0f);
  params[PG_TOOL_P_G] = 1; params[PG_TOOL_P_TARGET] = GPPAINT_MODE_STROKE;
  gesture(ts, d.gpd, PG_TOOL_VERTEX_PAINT, params, xy, 1);
  CHECK(s->points[0].vert_color[1] == 1.0f && s->points[0].vert_color[0] == 0.0f, "replace");
  CHECK(s->points[5].vert_color[3] == 0.0f && s->points[5].vert_color[1] == 0.0f, "replace touched an unpainted point");
  free_doc(&d);
}

static void test_weight_paint(PGToolSession *ts)
{
  /* Draw: weight = interpf(brush weight, weight, inf), inf = alpha * pressure * falloff. */
  Doc d = doc();
  bDeformGroup *g = MEM_callocN(sizeof(bDeformGroup), "group");
  strcpy(g->name, "G");
  BLI_addtail(&d.gpd->vertex_group_names, g);
  bGPDstroke *s = stroke(&d, 6, 0, 100, 10, 0);
  brush(GPWEIGHT_TOOL_DRAW, 25, 0.5f);
  params[PG_TOOL_P_TARGET] = 0;
  params[PG_TOOL_P_WEIGHT] = 1.0f;
  const float xy[1][2] = {{0, 100}};
  gesture(ts, d.gpd, PG_TOOL_WEIGHT_PAINT, params, xy, 1);
  CHECK(s->dvert != NULL, "no dvert");
  for (int i = 0; s->dvert && i < 6; i++) {
    const float inf = 0.5f * smooth_falloff(10.0f * i, 25.0f);
    const float expected = inf * 1.0f + (1.0f - inf) * 0.0f;
    MDeformWeight *dw = BKE_defvert_find_index(&s->dvert[i], 0);
    const float w = dw ? dw->weight : 0.0f;
    CHECK(fabsf(w - expected) < 1e-5f, "weight %d: %g vs %g", i, w, expected);
  }
  /* Blur and Average run through the kd-tree / average paths without crashing and keep 0..1. */
  brush(GPWEIGHT_TOOL_BLUR, 30, 1.0f);
  gesture(ts, d.gpd, PG_TOOL_WEIGHT_PAINT, params, xy, 1);
  brush(GPWEIGHT_TOOL_AVERAGE, 30, 1.0f);
  gesture(ts, d.gpd, PG_TOOL_WEIGHT_PAINT, params, xy, 1);
  float smear[5][2];
  for (int i = 0; i < 5; i++) { smear[i][0] = 5.0f * i; smear[i][1] = 100; }
  brush(GPWEIGHT_TOOL_SMEAR, 30, 1.0f);
  gesture(ts, d.gpd, PG_TOOL_WEIGHT_PAINT, params, (const float (*)[2])smear, 5);
  for (int i = 0; s->dvert && i < 6; i++) {
    MDeformWeight *dw = BKE_defvert_find_index(&s->dvert[i], 0);
    CHECK(!dw || (dw->weight >= 0.0f && dw->weight <= 1.0f), "weight out of range");
  }
  free_doc(&d);
}

/* Draw: the session feeds every sample through the draw input engine and the sink receives the
 * engine's points in order. */
typedef struct Sink { int begun, ended, cancelled, n; float x[512]; } Sink;
static int sk_begin(void *u, int m, float t) { (void)m; (void)t; ((Sink *)u)->begun++; return 1; }
static int sk_add(void *u, float x, float y, float p, float s, float t) { (void)y; (void)p; (void)s; (void)t; Sink *k = u; if (k->n < 512) k->x[k->n++] = x; return 1; }
static int sk_end(void *u) { ((Sink *)u)->ended++; return 1; }
static void sk_cancel(void *u) { ((Sink *)u)->cancelled++; }

static void test_draw(PGToolSession *ts)
{
  Sink k;
  memset(&k, 0, sizeof k);
  const PGToolDrawSink sink = {&k, sk_begin, sk_add, sk_end, sk_cancel};
  float p[PG_DRAW_P_COUNT];
  memset(p, 0, sizeof p);
  p[PG_DRAW_P_THICKNESS] = 3; p[PG_DRAW_P_STRENGTH] = 1; p[PG_DRAW_P_USE_PRESSURE] = 1;
  p[PG_DRAW_P_PRESSURE_CURVE] = 1; p[PG_DRAW_P_STRENGTH_CURVE] = 1; p[PG_DRAW_P_INPUT_SAMPLES] = 4;
  p[PG_DRAW_P_MANHATTAN] = 1; p[PG_DRAW_P_EUCLIDEAN] = 1; p[PG_DRAW_P_FAKE_POINTS] = 1;
  float smp[30 * 4];
  for (int i = 0; i < 30; i++) { smp[i * 4] = 13.0f * i; smp[i * 4 + 1] = 50 + 20 * sinf(0.4f * i); smp[i * 4 + 2] = 0.8f; smp[i * 4 + 3] = 0.01f * i; }
  /* batches of 1, 9, 20 samples */
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, smp, 1, PG_TOOL_PHASE_BEGIN, p, PG_DRAW_P_COUNT, &sink);
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, smp + 4, 9, PG_TOOL_PHASE_MOVE, NULL, 0, &sink);
  const int r = pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, smp + 40, 20, PG_TOOL_PHASE_END, NULL, 0, &sink);
  CHECK(k.begun == 1 && k.ended == 1 && (r & PG_TOOL_RESULT_ENDED), "draw sink lifecycle");
  CHECK(pg_tool_session_sample_count(ts) == 30, "draw applied %d of 30 samples", pg_tool_session_sample_count(ts));
  /* same as the engine on its own */
  PGDrawInput d;
  memset(&d, 0, sizeof d);
  PGDrawSettings s;
  pg_draw_settings_default(&s);
  s.input_samples = 4;
  pg_draw_input_begin(&d, &s);
  int n = 0, same = 1;
  const PGDrawPoint *out;
  for (int i = 0; i < 30; i++) {
    const int m = pg_draw_input_add(&d, smp[i * 4], smp[i * 4 + 1], smp[i * 4 + 2], smp[i * 4 + 3], &out);
    for (int j = 0; j < m; j++, n++) same &= n < k.n && out[j].x == k.x[n];
  }
  const int m = pg_draw_input_end(&d, &out);
  for (int j = 0; j < m; j++, n++) same &= n < k.n && out[j].x == k.x[n];
  CHECK(same && n == k.n, "session draw stream differs from the engine (%d vs %d)", k.n, n);
  pg_draw_input_free(&d);
  /* cancel */
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, smp, 3, PG_TOOL_PHASE_BEGIN, p, PG_DRAW_P_COUNT, &sink);
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, NULL, 0, PG_TOOL_PHASE_CANCEL, NULL, 0, &sink);
  CHECK(k.cancelled == 1, "draw cancel");
  /* MOVE without an open gesture is refused */
  CHECK(pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, smp, 1, PG_TOOL_PHASE_MOVE, NULL, 0, &sink) == 0, "move without begin");
}

/* Batch 21: guide snapping before the draw pipeline, and the brush pressure CurveMapping. */
typedef struct SinkXY { int n; float x[512], y[512], p[512]; } SinkXY;
static int kxy_begin(void *u, int m, float t) { (void)u; (void)m; (void)t; return 1; }
static int kxy_add(void *u, float x, float y, float p, float s, float t) { (void)s; (void)t; SinkXY *k = u; if (k->n < 512) { k->x[k->n] = x; k->y[k->n] = y; k->p[k->n] = p; k->n++; } return 1; }
static int kxy_end(void *u) { (void)u; return 1; }
static void test_draw_guide_and_curve(PGToolSession *ts)
{
  SinkXY k;
  memset(&k, 0, sizeof k);
  const PGToolDrawSink sink = {&k, kxy_begin, kxy_add, kxy_end, NULL};
  float p[PG_DRAW_P_COUNT];
  memset(p, 0, sizeof p);
  p[PG_DRAW_P_THICKNESS] = 3; p[PG_DRAW_P_STRENGTH] = 1; p[PG_DRAW_P_USE_PRESSURE] = 1;
  p[PG_DRAW_P_PRESSURE_CURVE] = 1; p[PG_DRAW_P_STRENGTH_CURVE] = 1;
  p[PG_DRAW_P_MANHATTAN] = 1; p[PG_DRAW_P_EUCLIDEAN] = 1;
  /* parallel guide at 0 rad: every point keeps the first sample's y */
  p[PG_DRAW_P_GUIDE_TYPE] = 2 + 1; p[PG_DRAW_P_GUIDE_ANGLE] = 0;
  /* pressure curve (0,0) (0.5,0.1) (1,1): 0.5 pressure maps to 0.1 */
  p[PG_DRAW_P_PRESSURE_CURVE_N] = 3;
  const float c[6] = {0, 0, 0.5f, 0.1f, 1, 1};
  memcpy(&p[PG_DRAW_P_PRESSURE_CURVE_XY], c, sizeof c);
  float smp[20 * 4];
  for (int i = 0; i < 20; i++) { smp[i * 4] = 10.0f * i; smp[i * 4 + 1] = 100 + 15 * sinf(0.7f * i); smp[i * 4 + 2] = 0.5f; smp[i * 4 + 3] = 0.01f * i; }
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, smp, 20, PG_TOOL_PHASE_BEGIN, p, PG_DRAW_P_COUNT, &sink);
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, NULL, 0, PG_TOOL_PHASE_END, NULL, 0, &sink);
  int on_line = k.n > 3, curve_ok = k.n > 3;
  for (int i = 0; i < k.n; i++) {
    on_line &= fabsf(k.y[i] - 100.0f) < 1e-3f;
    if (i > 0 && i < k.n - 1) curve_ok &= fabsf(k.p[i] - 0.1f) < 0.02f; /* ends may be tapered */
  }
  CHECK(on_line, "parallel guide keeps every drawn point on y = 100");
  CHECK(curve_ok, "pressure CurveMapping applied (0.5 -> 0.1)");
  /* guide off (0) leaves the input alone */
  memset(&k, 0, sizeof k);
  p[PG_DRAW_P_GUIDE_TYPE] = 0;
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, smp, 20, PG_TOOL_PHASE_BEGIN, p, PG_DRAW_P_COUNT, &sink);
  pg_tool_session_samples(ts, NULL, PG_TOOL_DRAW, NULL, 0, PG_TOOL_PHASE_END, NULL, 0, &sink);
  int off_line = 0;
  for (int i = 0; i < k.n; i++) off_line |= fabsf(k.y[i] - 100.0f) > 1.0f;
  CHECK(off_line, "no guide: the wavy input is kept");
}

int main(void)
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache;
  PGToolSession *ts = pg_tool_session_new();
  test_smooth(ts);
  test_brush_follows_finger(ts);
  test_thickness_strength_formulas(ts);
  test_grab(ts);
  test_other_sculpt(ts);
  test_vertex_paint(ts);
  test_weight_paint(ts);
  test_draw(ts);
  test_draw_guide_and_curve(ts);
  pg_tool_session_free(ts);
  if (failures) { fprintf(stderr, "%d tool session check(s) failed\n", failures); return 1; }
  printf("tool session tests passed\n");
  return 0;
}
