/* Host tests for project_grease_blender_eraser.c (Blender 3.6.23 gpencil_stroke_eraser_dostroke). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "BLI_math_geom.h"
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_blender_eraser.h"

/* stand-in for blenlib/intern/math_geom.c */
float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2])
{
  const float dx = l2[0] - l1[0], dy = l2[1] - l1[1];
  const float len2 = dx * dx + dy * dy;
  float t = len2 > 0.0f ? ((p[0] - l1[0]) * dx + (p[1] - l1[1]) * dy) / len2 : 0.0f;
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  const float cx = l1[0] + t * dx - p[0], cy = l1[1] + t * dy - p[1];
  return cx * cx + cy * cy;
}

static int failures = 0;
#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      failures++; \
    } \
  } while (0)

/* n points along x with step dx at height y */
static bGPDstroke *make_stroke(int n, float x0, float dx, float y)
{
  bGPDstroke *gps = calloc(1, sizeof(bGPDstroke));
  gps->totpoints = n;
  gps->points = n > 0 ? calloc((size_t)n, sizeof(bGPDspoint)) : NULL;
  for (int i = 0; i < n; i++) {
    gps->points[i].x = x0 + dx * (float)i;
    gps->points[i].y = y;
    gps->points[i].pressure = 1.0f;
    gps->points[i].strength = 1.0f;
  }
  return gps;
}

static PGEraserParams params(float x, float y, int radius, int mode)
{
  PGEraserParams p = {{x, y}, radius, mode, 1.0f, 1, 1.0f, 1.0f, 1.0f, 0};
  return p;
}

static unsigned tag_mask(const bGPDstroke *gps)
{
  unsigned m = 0;
  for (int i = 0; i < gps->totpoints; i++) {
    if (gps->points[i].flag & GP_SPOINT_TAG) {
      m |= 1u << i;
    }
  }
  return m;
}

static void test_tiny_strokes(void)
{
  bGPDstroke *none = make_stroke(0, 0, 0, 0);
  PGEraserParams p = params(0, 0, 10, PG_ERASER_HARD);
  CHECK(pg_eraser_dostroke(none, &p) == PG_ERASER_FREE, "empty stroke is freed");

  bGPDstroke *one = make_stroke(1, 50, 0, 0);
  p = params(50, 0, 10, PG_ERASER_HARD);
  CHECK(pg_eraser_dostroke(one, &p) == PG_ERASER_FREE, "single point under the eraser is freed");
  p = params(60, 0, 10, PG_ERASER_HARD);
  CHECK(pg_eraser_dostroke(one, &p) == PG_ERASER_FREE, "single point exactly at the radius is freed (<=)");
  p = params(61, 0, 10, PG_ERASER_HARD);
  CHECK(pg_eraser_dostroke(one, &p) == 0, "single point outside the radius is kept");
}

static void test_stroke_eraser_mode(void)
{
  bGPDstroke *gps = make_stroke(10, 0, 10, 0); /* x = 0..90 */
  PGEraserParams p = params(80, 0, 5, PG_ERASER_STROKE);
  CHECK(pg_eraser_dostroke(gps, &p) == PG_ERASER_FREE, "stroke mode frees the whole stroke on a hit");
  /* Blender only tests points 0..n-2: the last point can never trigger it. */
  p = params(90, 0, 5, PG_ERASER_STROKE);
  CHECK(pg_eraser_dostroke(gps, &p) == 0, "stroke mode ignores the final point (Blender's i+1 < totpoints)");
  p = params(500, 500, 5, PG_ERASER_STROKE);
  CHECK(pg_eraser_dostroke(gps, &p) == 0, "far eraser does nothing");
}

static void test_hard_eraser(void)
{
  bGPDstroke *gps = make_stroke(10, 0, 10, 0);
  PGEraserParams p = params(50, 0, 15, PG_ERASER_HARD);
  const int r = pg_eraser_dostroke(gps, &p);
  CHECK((r & PG_ERASER_CULL) && (r & PG_ERASER_MODIFIED) && !(r & PG_ERASER_FREE), "hard erase culls");
  CHECK(tag_mask(gps) == ((1u << 4) | (1u << 5) | (1u << 6)), "tags exactly the points with influence > 0");
  CHECK(gps->points[5].pressure == 0.0f && gps->points[3].pressure == 1.0f, "tagged points get pressure 0");

  bGPDstroke *far = make_stroke(10, 0, 10, 0);
  p = params(50, 300, 15, PG_ERASER_HARD);
  CHECK(pg_eraser_dostroke(far, &p) == 0 && tag_mask(far) == 0, "far eraser changes nothing");

  /* a point exactly at the radius has zero influence and is not erased */
  bGPDstroke *edge = make_stroke(10, 0, 10, 0);
  p = params(50, 0, 10, PG_ERASER_HARD);
  pg_eraser_dostroke(edge, &p);
  CHECK(tag_mask(edge) == (1u << 5), "influence 0 at the rim: only the centre point is erased");
}

static void test_stale_tags_are_cleared(void)
{
  bGPDstroke *gps = make_stroke(10, 0, 10, 0);
  gps->points[0].flag |= GP_SPOINT_TAG; /* leftover from an earlier operation */
  PGEraserParams p = params(50, 0, 15, PG_ERASER_HARD);
  pg_eraser_dostroke(gps, &p);
  CHECK(!(gps->points[0].flag & GP_SPOINT_TAG), "stale tags are cleared before evaluation");
}

static void test_soft_eraser(void)
{
  bGPDstroke *gps = make_stroke(10, 0, 10, 0);
  PGEraserParams p = params(50, 0, 20, PG_ERASER_SOFT);
  const int r = pg_eraser_dostroke(gps, &p);
  CHECK((r & PG_ERASER_MODIFIED) && !(r & PG_ERASER_CULL), "soft erase thins without culling");
  CHECK(gps->points[5].strength < gps->points[4].strength &&
            gps->points[4].strength < gps->points[3].strength,
        "strength falls off toward the centre");
  CHECK(gps->points[5].pressure < gps->points[4].pressure, "thickness falls off toward the centre");
  CHECK(gps->points[0].strength == 1.0f && gps->points[9].strength == 1.0f,
        "points outside the radius are untouched");
  CHECK(tag_mask(gps) == 0, "nothing is tagged while strength/pressure stay visible");

  /* factors scale the effect */
  bGPDstroke *half = make_stroke(10, 0, 10, 0);
  p = params(50, 0, 20, PG_ERASER_SOFT);
  p.soft_strength = 0.0f;
  p.soft_thickness = 0.0f;
  pg_eraser_dostroke(half, &p);
  CHECK(half->points[5].strength == 1.0f && half->points[5].pressure == 1.0f,
        "0% strength/thickness factors leave the points alone");
}

static void test_soft_cull_uses_blender_thresholds(void)
{
  /* Strength 0.005 is above Blender's 0.001 cull threshold but below the 0.01 the old code used.
   * With the strength/thickness factors at 0 the eraser touches the points without changing them. */
  bGPDstroke *gps = make_stroke(10, 0, 10, 0);
  for (int i = 0; i < 10; i++) {
    gps->points[i].strength = 0.005f;
  }
  PGEraserParams p = params(50, 0, 20, PG_ERASER_SOFT);
  p.soft_strength = 0.0f;
  p.soft_thickness = 0.0f;
  const int r = pg_eraser_dostroke(gps, &p);
  CHECK((r & PG_ERASER_MODIFIED) && !(r & PG_ERASER_CULL), "0.005 strength is not culled (threshold is 0.001)");
  CHECK(tag_mask(gps) == 0, "no point is tagged at 0.005 strength");

  /* just under Blender's threshold: culled */
  bGPDstroke *low = make_stroke(10, 0, 10, 0);
  for (int i = 0; i < 10; i++) {
    low->points[i].strength = 0.0009f;
  }
  const int r2 = pg_eraser_dostroke(low, &p);
  CHECK(r2 & PG_ERASER_CULL, "0.0009 strength is culled");
}

static void test_segment_spans_previous_point(void)
{
  /* Eraser at the very start. Blender tests the span pt0 -> pt2 of each segment, so segment i=1
   * (points 0,1,2) also reaches the eraser and touches point 0 again with half influence:
   * 1 - (0.1 [as pt1 of segment 0] + 0.05 [as pt0 of segment 1]) = 0.85. */
  bGPDstroke *gps = make_stroke(10, 0, 10, 0);
  PGEraserParams p = params(0, 0, 5, PG_ERASER_SOFT);
  pg_eraser_dostroke(gps, &p);
  CHECK(fabsf(gps->points[0].strength - 0.85f) < 1e-4f, "segment pt0->pt2 gives point 0 a second, half-strength hit");
  CHECK(gps->points[1].strength == 1.0f, "point 1 is outside the radius and untouched");
}

static void test_zero_pressure_means_zero_influence(void)
{
  /* fac *= p->pressure: no floor (the 0.01 clamp in Blender only sizes the eraser rect). */
  bGPDstroke *gps = make_stroke(10, 0, 10, 0);
  PGEraserParams p = params(50, 0, 20, PG_ERASER_SOFT);
  p.pressure = 0.0f;
  pg_eraser_dostroke(gps, &p);
  CHECK(gps->points[5].strength == 1.0f && gps->points[5].pressure == 1.0f,
        "zero pen pressure erases nothing softly");
  p.use_pressure = 0;
  pg_eraser_dostroke(gps, &p);
  CHECK(gps->points[5].strength < 1.0f, "pressure is ignored when the brush does not use it");
}

static void test_soft_refine_untags_isolated_points(void)
{
  /* Only point 5 can fall under the thresholds; soft_refine untags an isolated tag. */
  bGPDstroke *gps = make_stroke(10, 0, 10, 0);
  gps->points[5].strength = 0.0015f;
  PGEraserParams p = params(50, 0, 20, PG_ERASER_SOFT);
  const int r = pg_eraser_dostroke(gps, &p);
  CHECK(r & PG_ERASER_CULL, "a point under the strength threshold requests a cull");
  CHECK(tag_mask(gps) == 0, "soft_refine untags isolated points so stroke ends are not rounded");
}

int main(void)
{
  test_tiny_strokes();
  test_stroke_eraser_mode();
  test_hard_eraser();
  test_stale_tags_are_cleared();
  test_soft_eraser();
  test_soft_cull_uses_blender_thresholds();
  test_segment_spans_previous_point();
  test_zero_pressure_means_zero_influence();
  test_soft_refine_untags_isolated_points();
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
