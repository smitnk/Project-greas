/* CurveMapping port and elastic easing (project_grease_curvemap.c) against values from an independent
 * Python re-implementation of colortools.c / easing.c (tests: reference script in the batch-21 notes). */
#include <math.h>
#include <stdio.h>
#include "project_grease_curvemap.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

int main(void)
{
  PGCurve c;
  pg_curve_init_linear(&c);
  for (int i = 0; i <= 10; i++) {
    const float x = i / 10.0f;
    CHECK(fabsf(pg_curve_evaluate(&c, x) - x) < 1e-5f, "linear %g", x);
  }
  CHECK(pg_curve_is_linear(&c), "linear flag");
  const float a[6] = {0, 0, 0.5f, 0.1f, 1, 1};
  CHECK(pg_curve_set(&c, a, 3), "set");
  const float ax[4] = {0.25f, 0.5f, 0.75f, 0.9f}, ay[4] = {0.000321f, 0.1f, 0.350929f, 0.655729f};
  for (int i = 0; i < 4; i++) CHECK(fabsf(pg_curve_evaluate(&c, ax[i]) - ay[i]) < 2e-4f, "curve A at %g: %g", ax[i], pg_curve_evaluate(&c, ax[i]));
  const float b[8] = {0, 0, 0.7f, 0.4f, 0.3f, 0.6f, 1, 1}; /* unsorted on purpose */
  CHECK(pg_curve_set(&c, b, 4), "set B");
  const float bx[4] = {0.1f, 0.3f, 0.5f, 0.85f}, by[4] = {0.437603f, 0.599939f, 0.5f, 0.494221f};
  for (int i = 0; i < 4; i++) CHECK(fabsf(pg_curve_evaluate(&c, bx[i]) - by[i]) < 2e-4f, "curve B at %g: %g", bx[i], pg_curve_evaluate(&c, bx[i]));
  CHECK(fabsf(pg_curve_evaluate(&c, -1.0f)) < 1e-5f && fabsf(pg_curve_evaluate(&c, 2.0f) - 1.0f) < 1e-5f, "clipped extension");
  /* elastic easing, amplitude / period 0.15 (gpencil_interpolate defaults) */
  const float et[4] = {0.05f, 0.1f, 0.3f, 0.6f}, ev[4] = {1.053033f, 1.0375f, 0.98125f, 0.997656f};
  for (int i = 0; i < 4; i++) CHECK(fabsf(pg_easing_elastic(1, et[i], 0.15f, 0.15f) - ev[i]) < 1e-4f, "elastic out %g: %g", et[i], pg_easing_elastic(1, et[i], 0.15f, 0.15f));
  CHECK(pg_easing_elastic(0, 0.0f, 0.15f, 0.15f) == 0.0f && pg_easing_elastic(0, 1.0f, 0.15f, 0.15f) == 1.0f, "elastic ends");
  printf(failures ? "%d FAILURES\n" : "curvemap tests passed\n", failures);
  return failures ? 1 : 0;
}
