/* Unit checks of the Draw input (project_grease_draw_input.c over Blender's gpencil_paint.c code):
 * 1. the assertions of the former LegacyGpBrushStrokeEngineTest.kt;
 * 2. Blender pixel math (zoom), BLI_rng jitter sequence and sbuffer point times.
 * The comparison with a real Blender run is test_draw_blender_parity.c. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_brush_enums.h"
#include "project_grease_draw_input.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* ---- the former Kotlin unit tests -------------------------------------------------------- */
typedef struct Run { PGDrawPoint sent[512]; int n; PGDrawInput d; } Run;
static void run(Run *r, const PGDrawSettings *s, const float (*ev)[4], int count)
{
  memset(r, 0, sizeof *r);
  pg_draw_input_begin(&r->d, s);
  const PGDrawPoint *out;
  for (int i = 0; i < count; i++) {
    const int n = pg_draw_input_add(&r->d, ev[i][0], ev[i][1], ev[i][2], ev[i][3], &out);
    for (int k = 0; k < n; k++) r->sent[r->n++] = out[k];
  }
  const int n = pg_draw_input_end(&r->d, &out);
  for (int k = 0; k < n; k++) r->sent[r->n++] = out[k];
}
static int line(float (*ev)[4], int count, float step, float y, float p)
{
  for (int i = 0; i < count; i++) { ev[i][0] = step * i; ev[i][1] = y; ev[i][2] = p; ev[i][3] = 0.016f * i; }
  return count;
}
static PGDrawSettings defaults(void) { PGDrawSettings s; pg_draw_settings_default(&s); return s; }

static void kotlin_tests(void)
{
  PGDrawInput d;
  memset(&d, 0, sizeof d);
  const PGDrawPoint *out;
  PGDrawSettings s = defaults();
  static Run r;
  float ev[64][4];

  /* firstPointerSampleIsAlwaysAccepted */
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 10, 20, 1, 5, &out);
  CHECK(d.used == 1 && d.buffer[0].x == 10 && d.buffer[0].y == 20, "first sample");
  /* smallMotionUsesBlenderManhattanAndEuclideanFilter */
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 0, &out);
  pg_draw_input_add(&d, 1, 0, 1, 0.01f, &out);
  CHECK(d.used == 1, "dx 1 filtered");
  pg_draw_input_add(&d, 2, 0, 1, 0.02f, &out);
  CHECK(d.used == 2, "dx 2 passes");
  /* lazyMouseInterpolatesAcceptedSampleTowardPreviousPoint */
  s = defaults(); s.lazy_enabled = 1; s.smooth_stroke_radius = 5; s.smooth_stroke_factor = 0.5f;
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 0, &out);
  pg_draw_input_add(&d, 10, 0, 1, 0.01f, &out);
  CHECK(d.used == 2 && fabsf(d.buffer[1].x - 5) < 1e-5f, "lazy interp");
  pg_draw_input_add(&d, 8, 0, 1, 0.02f, &out);
  CHECK(d.used == 2, "inside lazy radius");
  /* arcsNeedThreeBufferedPointsAndReplaceTheLastOne */
  s = defaults(); s.input_samples = 4;
  pg_draw_input_begin(&d, &s);
  int sizes[4];
  for (int i = 0; i < 4; i++) { pg_draw_input_add(&d, 40.0f * i, 0, 1, 0.01f * i, &out); sizes[i] = d.used; }
  CHECK(sizes[0] == 1 && sizes[1] == 2 && sizes[2] == 3 && sizes[3] == 5, "arc buffer sizes %d %d %d %d", sizes[0], sizes[1], sizes[2], sizes[3]);
  /* slowMotionNeverSynthesizesPoints */
  for (int i = 0; i < 10; i++) { ev[i][0] = 10.0f * i; ev[i][1] = 5.0f * i; ev[i][2] = 1; ev[i][3] = 0.016f * i; }
  run(&r, &s, (const float (*)[4])ev, 10);
  CHECK(r.n == 10, "slow motion: %d", r.n);
  pg_draw_input_free(&r.d);
  /* fastStraightStrokeNeverDoublesBackOrBends */
  run(&r, &s, (const float (*)[4])ev, line(ev, 12, 40, 100, 1));
  for (int i = 1; i < r.n; i++) CHECK(r.sent[i].x >= r.sent[i - 1].x - 1e-3f, "x went back at %d", i);
  for (int i = 0; i < r.n; i++) CHECK(fabsf(r.sent[i].y - 100) < 1e-3f, "bend");
  pg_draw_input_free(&r.d);
  /* streamSentToNativeEqualsTheFinalBuffer */
  s.active_smooth = 0.5f;
  run(&r, &s, (const float (*)[4])ev, line(ev, 20, 33, 100, 1));
  CHECK(r.n == r.d.used && !memcmp(r.sent, r.d.buffer, sizeof(PGDrawPoint) * (size_t)r.n), "stream == buffer");
  pg_draw_input_free(&r.d);
  /* timeIsNormalizedToStrokeStart */
  s = defaults();
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 100, &out);
  CHECK(d.buffer[0].time == 0.0f, "t0");
  pg_draw_input_add(&d, 10, 0, 1, 100.5f, &out);
  CHECK(d.used == 2 && fabsf(d.buffer[1].time - 0.5f) < 1e-5f, "t1");
  /* pressureOneStaysOneWithoutPressureModifiers */
  run(&r, &s, (const float (*)[4])ev, line(ev, 6, 5, 100, 1));
  for (int i = 0; i < r.n; i++) CHECK(r.sent[i].pressure == 1.0f, "pressure 1");
  pg_draw_input_free(&r.d);
  /* changingPressureChangesOnlyPressureWhenStrengthPressureDisabled */
  s = defaults(); s.draw_strength = 0.8f;
  run(&r, &s, (const float (*)[4])ev, line(ev, 6, 5, 100, 0.3f));
  for (int i = 0; i < r.n; i++) CHECK(fabsf(r.sent[i].pressure - 0.3f) < 1e-5f && fabsf(r.sent[i].strength - 0.8f) < 1e-6f, "pressure only");
  pg_draw_input_free(&r.d);
  /* strengthPressureScalesStrengthButKeepsTheMinimum */
  s = defaults(); s.draw_strength = 0.5f; s.use_strength_pressure = 1;
  run(&r, &s, (const float (*)[4])ev, line(ev, 4, 5, 100, 0.5f));
  for (int i = 0; i < r.n; i++) CHECK(fabsf(r.sent[i].strength - 0.25f) < 1e-5f, "strength pressure");
  pg_draw_input_free(&r.d);
  /* interpfWeightsTheTargetByTheFactor */
  CHECK(fabsf(pg_draw_interpf(10, 0, 0.3f) - 3) < 1e-6f && fabsf(pg_draw_interpf(0, 10, 0.3f) - 7) < 1e-6f, "interpf");
  /* drawAngleBlendsWithThePreviousPointUsingInterpf */
  s = defaults(); s.draw_angle_factor = 0.5f; s.draw_angle = 0;
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 0, &out);
  pg_draw_input_add(&d, 20, 0, 0.5f, 0.01f, &out);
  pg_draw_input_end(&d, &out);
  CHECK(fabsf(d.buffer[0].pressure - 0.8f) < 1e-4f && fabsf(d.buffer[1].pressure - 0.71f) < 1e-4f, "draw angle");
  /* activeSmoothingKeepsAStraightLineStraight */
  s = defaults(); s.active_smooth = 0.65f;
  for (int i = 0; i < 20; i++) { ev[i][0] = 8.0f * i; ev[i][1] = 50; ev[i][2] = 0.4f + 0.02f * i; ev[i][3] = 0.016f * i; }
  run(&r, &s, (const float (*)[4])ev, 20);
  CHECK(r.n == 20, "smooth line count");
  for (int i = 0; i < r.n; i++) CHECK(fabsf(r.sent[i].y - 50) < 1e-4f && r.sent[i].pressure >= 0.3f && r.sent[i].pressure <= 0.9f, "smooth line");
  pg_draw_input_free(&r.d);
  /* activeSmoothingPullsAPressureSpikeTowardItsNeighbours */
  s = defaults(); s.active_smooth = 0.5f;
  pg_draw_input_begin(&d, &s);
  const float spike[6] = {1, 1, 1, 0.2f, 1, 1};
  for (int i = 0; i < 6; i++) pg_draw_input_add(&d, 10.0f * i, 0, spike[i], 0.01f * i, &out);
  CHECK(fabsf(d.buffer[3].pressure - 0.697f) < 2e-3f, "spike %g", d.buffer[3].pressure);
  /* activeSmoothingOffLeavesPointsUntouched */
  s = defaults();
  run(&r, &s, (const float (*)[4])ev, line(ev, 6, 7, 100, 0.5f));
  CHECK(r.n == 6, "no smooth count");
  for (int i = 0; i < r.n; i++) CHECK(fabsf(r.sent[i].x - 7.0f * i) < 1e-5f, "no smooth x");
  pg_draw_input_free(&r.d);
  /* trailingNearZeroPressureIsTruncatedLikeBlender */
  s = defaults(); s.input_samples = 1;
  const float tp[6] = {1, 1, 1, 1, 1, 0};
  for (int i = 0; i < 6; i++) { ev[i][0] = 5.0f * i; ev[i][1] = 0; ev[i][2] = tp[i]; ev[i][3] = 0.01f * i; }
  run(&r, &s, (const float (*)[4])ev, 6);
  CHECK(r.n == 4 && r.d.used == 4, "truncation %d", r.n);
  pg_draw_input_free(&r.d);
  /* endAndCancelResetState */
  s = defaults();
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 0, &out);
  pg_draw_input_cancel(&d);
  CHECK(pg_draw_input_add(&d, 10, 0, 1, 0.1f, &out) == 0 && d.used == 0, "cancel");
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 3, 3, 1, 0, &out);
  CHECK(d.used == 1, "restart");
  pg_draw_input_end(&d, &out);
  CHECK(pg_draw_input_add(&d, 30, 3, 1, 0.1f, &out) == 0, "after end");
  pg_draw_input_free(&d);
}

/* ---- Blender pixel math, BLI_rng and sbuffer times ------------------------------------------ */
static int accepted(PGDrawSettings s, float zoom, float x, float y)
{
  PGDrawInput d;
  memset(&d, 0, sizeof d);
  const PGDrawPoint *out;
  s.px_per_unit = zoom;
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 0, &out);
  pg_draw_input_add(&d, x, y, 1, 0.1f, &out);
  const int n = d.used - 1;
  pg_draw_input_free(&d);
  return n;
}

static void blender_tests(void)
{
  PGDrawSettings s = defaults();
  /* gpencil_stroke_filtermval(): dx = (int)fabsf(mval - mvalo) in region pixels. Euclidean 1 px:
   * 1.5 canvas units are 1 px at zoom 1 (1*1 > 1 fails) and 3 px at zoom 2 (9 > 1 passes). */
  CHECK(accepted(s, 1, 1.5f, 0) == 0, "euclidean zoom 1 filters");
  CHECK(accepted(s, 2, 1.5f, 0) == 1, "euclidean zoom 2 passes");
  /* Manhattan 1 px on both axes (Euclidean 10 px out of the way): (1.5, 1.5) -> 1 px / 3 px. */
  s.euclidean_threshold = 10;
  CHECK(accepted(s, 1, 1.5f, 1.5f) == 0, "manhattan zoom 1 filters");
  CHECK(accepted(s, 2, 1.5f, 1.5f) == 1, "manhattan zoom 2 passes");
  /* Lazy mouse: brush->smooth_stroke_radius is pixels; 4 units = 4 px (16 <= 25) / 8 px (64 > 25). */
  s = defaults(); s.lazy_enabled = 1; s.smooth_stroke_radius = 5; s.smooth_stroke_factor = 0.5f;
  CHECK(accepted(s, 1, 4, 0) == 0, "lazy radius zoom 1 holds");
  CHECK(accepted(s, 2, 4, 0) == 1, "lazy radius zoom 2 passes");
  /* gpencil_add_fake_points(): input_samples 4 -> min_dist 4 * 7 = 28 px; a 15 unit jump is
   * 15 px at zoom 1 (no arc) and 30 px at zoom 2 (arc of 2 slices). */
  for (int zoom = 1; zoom <= 2; zoom++) {
    PGDrawInput d;
    memset(&d, 0, sizeof d);
    const PGDrawPoint *out;
    s = defaults(); s.input_samples = 4; s.px_per_unit = (float)zoom;
    pg_draw_input_begin(&d, &s);
    const float xs[4] = {0, 10, 20, 35};
    for (int i = 0; i < 4; i++) pg_draw_input_add(&d, xs[i], 0, 1, 0.01f * i, &out);
    CHECK(d.used == (zoom == 1 ? 4 : 5), "fake points zoom %d: %d buffered", zoom, d.used);
    pg_draw_input_free(&d);
  }

  /* BLI_rng_new(seed) + BLI_rng_get_float(): blender::RandomNumberGenerator is the drand48 LCG
   * (x = seed << 16 | 0x330E), so srand48() / lrand48() is an independent reference. */
  {
    unsigned long long x;
    pg_draw_rng_seed(&x, 1234u);
    srand48(1234);
    int same = 1;
    for (int i = 0; i < 1000; i++) same &= pg_draw_rng_get_float(&x) == (float)(int)lrand48() / 0x80000000;
    CHECK(same, "BLI_rng sequence");
  }
  /* gpencil_stroke_addpoint(): one BLI_rng_get_float() per point (also for the first two, which
   * gpencil_brush_jitter() does not move); fac = (rand * 2 - 1) * (jitter + 2)^2; the diagonal
   * offset is along the movement from the previous (jittered) buffer point, in region pixels (y up). */
  for (int zoom = 1; zoom <= 2; zoom++) {
    static Run r;
    float ev[8][4];
    for (int i = 0; i < 8; i++) { ev[i][0] = 100.0f * i; ev[i][1] = 100.0f * i; ev[i][2] = 1; ev[i][3] = 0.01f * i; }
    s = defaults(); s.jitter = 0.5f; s.use_seed = 1; s.seed = 77u; s.px_per_unit = (float)zoom;
    run(&r, &s, (const float (*)[4])ev, 8);
    CHECK(r.n == 8, "jitter count %d", r.n);
    srand48(77);
    for (int i = 0; i < r.n; i++) {
      const float rand = (float)(int)lrand48() / 0x80000000 * 2.0f - 1.0f;
      const float fac = rand * 6.25f;
      /* region pixels (x, -y) * zoom; the previous point is the jittered one in the buffer */
      float ex = 100.0f * i * zoom, ey = -100.0f * i * zoom;
      if (i >= 2) {
        float mx = ex - r.sent[i - 1].x * zoom, my = ey + r.sent[i - 1].y * zoom;
        const float l = sqrtf(mx * mx + my * my);
        mx /= l; my /= l;
        const float angle = acosf(my); /* angle_v2v2(mvec, (0, 1)) */
        ex += mx * cosf(angle) * fac * 10.0f;
        ey += my * sinf(angle) * fac * 10.0f;
      }
      CHECK(fabsf(r.sent[i].x - ex / zoom) < 2e-3f && fabsf(r.sent[i].y + ey / zoom) < 2e-3f,
            "jitter zoom %d point %d: (%g %g) expected (%g %g)", zoom, i, r.sent[i].x, r.sent[i].y, ex / zoom, -ey / zoom);
    }
    pg_draw_input_free(&r.d);
  }

  /* gpencil_apply_randomness() (GP_BRUSH_GROUP_RANDOM): per point one BLI_rng_get_float() for the
   * pressure (value = 1 + rand * 2 * draw_random_press, clamp 0.1..1) then one for the strength
   * (value = 1 + rand * draw_random_strength, clamp GPENCIL_STRENGTH_MIN..1). */
  {
    static Run r;
    float ev[6][4];
    for (int i = 0; i < 6; i++) { ev[i][0] = 10.0f * i; ev[i][1] = 0; ev[i][2] = 0.8f; ev[i][3] = 0.01f * i; }
    s = defaults(); s.random_press = 0.5f; s.random_strength = 0.4f; s.draw_strength = 0.9f;
    s.use_seed = 1; s.seed = 4242u;
    run(&r, &s, (const float (*)[4])ev, 6);
    CHECK(r.n == 6, "random count %d", r.n);
    srand48(4242);
    int varied = 0;
    for (int i = 0; i < r.n; i++) {
      const float r1 = (float)(int)lrand48() / 0x80000000 * 2.0f - 1.0f;
      const float r2 = (float)(int)lrand48() / 0x80000000 * 2.0f - 1.0f;
      float pr = 0.8f, st = 0.9f;
      pr *= 1.0 + r1 * 2.0 * 0.5f;
      pr = pr < 0.1f ? 0.1f : (pr > 1.0f ? 1.0f : pr);
      st *= 1.0 + r2 * 0.4f;
      st = st < 0.003f ? 0.003f : (st > 1.0f ? 1.0f : st);
      CHECK(r.sent[i].pressure == pr && r.sent[i].strength == st, "random point %d: p %g s %g expected %g %g", i,
            r.sent[i].pressure, r.sent[i].strength, pr, st);
      varied |= pr != 0.8f;
    }
    CHECK(varied, "random changed nothing");
    pg_draw_input_free(&r.d);
    /* GP_BRUSH_USE_PRESS_AT_STROKE / GP_BRUSH_USE_STRENGTH_AT_STROKE: one factor per stroke, from
     * ED_gpencil_init_random_settings() (BLI_hash of the first event position and the timer) */
    s.random_flag2 = GP_BRUSH_USE_PRESS_AT_STROKE | GP_BRUSH_USE_STRENGTH_AT_STROKE;
    run(&r, &s, (const float (*)[4])ev, 6);
    int same = r.n == 6;
    for (int i = 1; i < r.n; i++) same &= r.sent[i].pressure == r.sent[0].pressure && r.sent[i].strength == r.sent[0].strength;
    CHECK(same && (r.sent[0].pressure != 0.8f || r.sent[0].strength != 0.9f), "per-stroke random: p %g s %g", r.sent[0].pressure, r.sent[0].strength);
    pg_draw_input_free(&r.d);
  }

  /* Point times: pt->time = curtime - inittime for every added point; gpencil_add_arc_points()
   * does not write time: the first arc point reuses the replaced slot (keeps 0.2), the next one is
   * a zeroed sbuffer slot (0). Events 0, 40, 80, 120 px with input_samples 4: 40 > 28 -> 2 slices. */
  {
    static Run r;
    float ev[4][4];
    for (int i = 0; i < 4; i++) { ev[i][0] = 40.0f * i; ev[i][1] = 0; ev[i][2] = 1; ev[i][3] = 10.0f + 0.1f * i; }
    s = defaults(); s.input_samples = 4;
    run(&r, &s, (const float (*)[4])ev, 4);
    const float expect[5] = {0.0f, (10.1f - 10.0f), (10.2f - 10.0f), 0.0f, (10.3f - 10.0f)};
    CHECK(r.n == 5, "arc times count %d", r.n);
    for (int i = 0; i < r.n && i < 5; i++)
      CHECK(r.sent[i].time == expect[i], "arc time %d: %g expected %g", i, r.sent[i].time, expect[i]);
    CHECK(r.sent[2].x > 40.0f && r.sent[2].x < r.sent[3].x && r.sent[3].x < 120.0f, "arc positions interpolated");
    pg_draw_input_free(&r.d);
  }
}

int main(int argc, char **argv)
{
  (void)argc; (void)argv;
  kotlin_tests();
  blender_tests();
  if (failures) { fprintf(stderr, "%d draw input check(s) failed\n", failures); return 1; }
  printf("draw input tests passed\n");
  return 0;
}
