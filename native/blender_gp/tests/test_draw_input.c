/* Conformance tests of the native Draw input engine (project_grease_draw_input.c):
 * 1. golden replay: every scenario of tools/draw_input_golden (the former Kotlin engine, whose
 *    unit tests were derived from Blender 3.6.23 gpencil_paint.c) must release the same points in
 *    the same calls;
 * 2. the assertions of the former LegacyGpBrushStrokeEngineTest.kt, ported one to one. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project_grease_draw_input.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static float bits(const char *s)
{
  unsigned int u = (unsigned int)strtol(s, NULL, 10);
  float f;
  memcpy(&f, &u, sizeof f);
  return f;
}
static int near(float a, float b) { return fabsf(a - b) <= 1e-4f * fmaxf(1.0f, fabsf(b)); }

static void golden(const char *path)
{
  FILE *f = fopen(path, "r");
  CHECK(f != NULL, "cannot open %s", path);
  if (!f) return;
  char line[1024], name[128] = "";
  PGDrawInput d;
  memset(&d, 0, sizeof d);
  PGDrawPoint expected[512];
  int n_expected = 0, event = -1, scenarios = 0, points = 0, exact = 0;
  float ev[4];
  /* actual stream of (call, point) */
  PGDrawPoint actual[512];
  int actual_call[512], n_actual = 0, expected_call[512];
  while (fgets(line, sizeof line, f)) {
    char *tok[24];
    int nt = 0;
    for (char *t = strtok(line, " \n"); t && nt < 24; t = strtok(NULL, " \n")) tok[nt++] = t;
    if (nt == 0 || tok[0][0] == '#') continue;
    if (!strcmp(tok[0], "scenario")) {
      snprintf(name, sizeof name, "%s", tok[1]);
      n_expected = n_actual = 0;
      event = -1;
    }
    else if (!strcmp(tok[0], "settings")) {
      PGDrawSettings s;
      s.draw_strength = bits(tok[1]); s.use_pressure = atoi(tok[2]); s.use_strength_pressure = atoi(tok[3]);
      s.pressure_curve = bits(tok[4]); s.strength_curve = bits(tok[5]); s.active_smooth = bits(tok[6]);
      s.input_samples = atoi(tok[7]); s.lazy_enabled = atoi(tok[8]); s.smooth_stroke_radius = bits(tok[9]);
      s.smooth_stroke_factor = bits(tok[10]); s.disable_stabilizer = atoi(tok[11]);
      s.manhattan_threshold = atoi(tok[12]); s.euclidean_threshold = bits(tok[13]); s.jitter = bits(tok[14]);
      s.draw_angle_factor = bits(tok[15]); s.draw_angle = bits(tok[16]); s.synthesize_fast_points = atoi(tok[17]);
      pg_draw_input_begin(&d, &s);
    }
    else if (!strcmp(tok[0], "event")) {
      event++;
      for (int k = 0; k < 4; k++) ev[k] = bits(tok[1 + k]);
      const PGDrawPoint *out;
      const int n = pg_draw_input_add(&d, ev[0], ev[1], ev[2], ev[3], &out);
      for (int k = 0; k < n && n_actual < 512; k++) { actual[n_actual] = out[k]; actual_call[n_actual++] = event; }
    }
    else if (!strcmp(tok[0], "out") && n_expected < 512) {
      expected_call[n_expected] = atoi(tok[1]);
      PGDrawPoint p = {bits(tok[2]), bits(tok[3]), bits(tok[4]), bits(tok[5]), bits(tok[6])};
      expected[n_expected++] = p;
    }
    else if (!strcmp(tok[0], "end")) {
      const PGDrawPoint *out;
      const int n = pg_draw_input_end(&d, &out);
      for (int k = 0; k < n && n_actual < 512; k++) { actual[n_actual] = out[k]; actual_call[n_actual++] = -1; }
      scenarios++;
      CHECK(n_actual == n_expected, "%s: %d points released, golden %d", name, n_actual, n_expected);
      for (int k = 0; k < n_actual && k < n_expected; k++) {
        const PGDrawPoint *a = &actual[k], *e = &expected[k];
        const int ok = actual_call[k] == expected_call[k] && near(a->x, e->x) && near(a->y, e->y) &&
                       near(a->pressure, e->pressure) && near(a->strength, e->strength) && near(a->time, e->time);
        CHECK(ok, "%s point %d (call %d/%d): (%g %g p%g s%g t%g) vs golden (%g %g p%g s%g t%g)", name, k,
              actual_call[k], expected_call[k], a->x, a->y, a->pressure, a->strength, a->time, e->x, e->y,
              e->pressure, e->strength, e->time);
        points++;
        exact += !memcmp(a, e, sizeof *a);
      }
    }
  }
  fclose(f);
  pg_draw_input_free(&d);
  CHECK(scenarios >= 20, "only %d golden scenarios", scenarios);
  printf("draw input golden: %d scenarios, %d points (%d bit-identical)\n", scenarios, points, exact);
}

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
  CHECK(pg_draw_input_add(&d, 10, 20, 1, 5, &out) == 1 && out[0].x == 10 && out[0].y == 20, "first sample");
  /* smallMotionUsesBlenderManhattanAndEuclideanFilter */
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 0, &out);
  CHECK(pg_draw_input_add(&d, 1, 0, 1, 0.01f, &out) == 0, "dx 1 filtered");
  CHECK(pg_draw_input_add(&d, 2, 0, 1, 0.02f, &out) == 1, "dx 2 passes");
  /* lazyMouseInterpolatesAcceptedSampleTowardPreviousPoint */
  s = defaults(); s.lazy_enabled = 1; s.smooth_stroke_radius = 5; s.smooth_stroke_factor = 0.5f;
  pg_draw_input_begin(&d, &s);
  pg_draw_input_add(&d, 0, 0, 1, 0, &out);
  CHECK(pg_draw_input_add(&d, 10, 0, 1, 0.01f, &out) == 1 && fabsf(out[0].x - 5) < 1e-5f, "lazy interp");
  CHECK(pg_draw_input_add(&d, 8, 0, 1, 0.02f, &out) == 0, "inside lazy radius");
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
  CHECK(out[0].time == 0.0f, "t0");
  pg_draw_input_add(&d, 10, 0, 1, 100.5f, &out);
  CHECK(fabsf(out[0].time - 0.5f) < 1e-5f, "t1");
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
  CHECK(pg_draw_input_add(&d, 3, 3, 1, 0, &out) == 1, "restart");
  pg_draw_input_end(&d, &out);
  CHECK(pg_draw_input_add(&d, 30, 3, 1, 0.1f, &out) == 0, "after end");
  pg_draw_input_free(&d);
}

int main(int argc, char **argv)
{
  golden(argc > 1 ? argv[1] : "draw_input_golden.txt");
  kotlin_tests();
  if (failures) { fprintf(stderr, "%d draw input check(s) failed\n", failures); return 1; }
  printf("draw input tests passed\n");
  return 0;
}
