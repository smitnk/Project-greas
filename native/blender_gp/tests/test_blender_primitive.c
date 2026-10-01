#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "project_grease_blender_primitive.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-3f)

static float out[2 * 2048];

static void test_defaults_match_blender_operators(void)
{
  CHECK(project_grease_blender_primitive_default_edges(PG_PRIMITIVE_BOX) == 4, "box: subdivision 3 + 1");
  CHECK(project_grease_blender_primitive_default_edges(PG_PRIMITIVE_LINE) == 8, "line: subdivision 6 + 2");
  CHECK(project_grease_blender_primitive_default_edges(PG_PRIMITIVE_POLYLINE) == 8, "polyline: 6 + 2");
  CHECK(project_grease_blender_primitive_default_edges(PG_PRIMITIVE_CIRCLE) == 96, "circle: 94 + 2");
  CHECK(project_grease_blender_primitive_default_edges(PG_PRIMITIVE_ARC) == 64, "arc: curve op 62 + 2");
  CHECK(project_grease_blender_primitive_default_edges(PG_PRIMITIVE_CURVE) == 64, "curve: 62 + 2");
  CHECK(project_grease_blender_primitive_default_edges(99) == 0, "invalid type");
  CHECK(project_grease_blender_primitive_is_cyclic(PG_PRIMITIVE_BOX), "box cyclic");
  CHECK(project_grease_blender_primitive_is_cyclic(PG_PRIMITIVE_CIRCLE), "circle cyclic");
  CHECK(!project_grease_blender_primitive_is_cyclic(PG_PRIMITIVE_LINE), "line open");
}

static void test_line_is_evenly_spaced_between_ends(void)
{
  const float a[] = {10, 20, 80, 20};
  int n = project_grease_blender_primitive_generate(PG_PRIMITIVE_LINE, a, 2, 0, 0, out, 2048);
  CHECK(n == 8, "line default point count");
  CHECK(NEAR(out[0], 10) && NEAR(out[1], 20), "line starts at start");
  CHECK(NEAR(out[14], 80) && NEAR(out[15], 20), "line ends at end");
  CHECK(NEAR(out[2], 20), "line step is 70/7");
}

static void test_box_walks_corners_per_side(void)
{
  const float a[] = {0, 0, 100, 50};
  int n = project_grease_blender_primitive_generate(PG_PRIMITIVE_BOX, a, 2, 0, 0, out, 2048);
  CHECK(n == 16, "box default: 4 edges per side");
  CHECK(NEAR(out[0], 0) && NEAR(out[1], 0), "box p0 = start");
  CHECK(NEAR(out[8], 100) && NEAR(out[9], 0), "box p4 = (end.x, start.y)");
  CHECK(NEAR(out[16], 100) && NEAR(out[17], 50), "box p8 = end");
  n = project_grease_blender_primitive_generate(PG_PRIMITIVE_BOX, a, 2, 1, 0, out, 2048);
  CHECK(n == 4, "box with 1 edge = 4 corners");
}

static void test_circle_runs_counter_clockwise_on_screen(void)
{
  const float a[] = {0, 0, 100, 100};
  int n = project_grease_blender_primitive_generate(PG_PRIMITIVE_CIRCLE, a, 2, 0, 0, out, 2048);
  CHECK(n == 96, "circle default points");
  CHECK(NEAR(out[0], 100) && NEAR(out[1], 50), "circle starts at angle 0");
  /* Blender region space is y-up: the second point must be visually above. */
  CHECK(out[3] < 50.0f, "circle direction follows Blender's y-up region space");
}

static void test_arc_bulges_like_blender(void)
{
  const float a[] = {0, 0, 100, 0};
  int n = project_grease_blender_primitive_generate(PG_PRIMITIVE_ARC, a, 2, 0, 0, out, 2048);
  CHECK(n == 64, "arc default points");
  CHECK(NEAR(out[0], 0) && NEAR(out[1], 0), "arc starts at start");
  CHECK(NEAR(out[126], 100) && NEAR(out[127], 0), "arc ends at end");
  CHECK(out[2 * 32 + 1] < -1.0f, "left-to-right arc bulges upward on screen");
  project_grease_blender_primitive_generate(PG_PRIMITIVE_ARC, a, 2, 0, 1, out, 2048);
  CHECK(out[2 * 32 + 1] > 1.0f, "flipped arc bulges downward");
}

static void test_curve_with_default_controls_is_straight(void)
{
  const float a[] = {0, 0, 90, 0};
  int n = project_grease_blender_primitive_generate(PG_PRIMITIVE_CURVE, a, 2, 0, 0, out, 2048);
  CHECK(n == 64, "curve default points");
  CHECK(NEAR(out[127], 0) && NEAR(out[126], 90), "curve ends at end");
  CHECK(NEAR(out[61], 0), "default control points keep curve straight");
  const float c[] = {0, 0, 90, 0, 0, 60, 90, 60};
  project_grease_blender_primitive_generate(PG_PRIMITIVE_CURVE, c, 4, 0, 0, out, 2048);
  CHECK(out[2 * 32 + 1] > 10.0f, "explicit control points bend the curve");
}

static void test_polyline_joins_segments_without_duplicates(void)
{
  const float v[] = {0, 0, 70, 0, 70, 70};
  int expected = project_grease_blender_primitive_point_count(PG_PRIMITIVE_POLYLINE, 3, 0);
  CHECK(expected == 15, "polyline: 8 + 7 points");
  int n = project_grease_blender_primitive_generate(PG_PRIMITIVE_POLYLINE, v, 3, 0, 0, out, 2048);
  CHECK(n == 15, "polyline generated count");
  CHECK(NEAR(out[14], 70) && NEAR(out[15], 0), "p7 is the middle vertex");
  CHECK(NEAR(out[16], 70) && NEAR(out[17], 10), "p8 continues the second segment");
  CHECK(NEAR(out[28], 70) && NEAR(out[29], 70), "last point is the final vertex");
}

static void test_rejects_bad_input(void)
{
  const float a[] = {0, 0, 10, 10};
  CHECK(project_grease_blender_primitive_generate(42, a, 2, 0, 0, out, 2048) == 0, "bad type");
  CHECK(project_grease_blender_primitive_generate(PG_PRIMITIVE_LINE, a, 2, 0, 0, out, 3) == 0, "small capacity");
  CHECK(project_grease_blender_primitive_generate(PG_PRIMITIVE_POLYLINE, a, 1, 0, 0, out, 2048) == 0, "1-vertex polyline");
  const float bad[] = {0, NAN, 10, 10};
  CHECK(project_grease_blender_primitive_generate(PG_PRIMITIVE_LINE, bad, 2, 0, 0, out, 2048) == 0, "NaN anchor");
}

int main(void)
{
  test_defaults_match_blender_operators();
  test_line_is_evenly_spaced_between_ends();
  test_box_walks_corners_per_side();
  test_circle_runs_counter_clockwise_on_screen();
  test_arc_bulges_like_blender();
  test_curve_with_default_controls_is_straight();
  test_polyline_joins_segments_without_duplicates();
  test_rejects_bad_input();
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
