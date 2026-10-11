#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_blender_mod2.h"
static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s (line %d)\n", m, __LINE__); fails++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-3f)
int main(void)
{
  int tp[3] = {10, 10, 10}, v[3];
  CHECK(pg_build_visible(tp, 3, 0, 0, 15, 0, 30, v) == 15 && v[0] == 10 && v[1] == 5 && v[2] == 0, "sequential grow half");
  CHECK(pg_build_visible(tp, 3, 1, 0, 15, 0, 30, v) == 15 && v[0] == 5 && v[2] == 5, "concurrent grow half");
  CHECK(pg_build_visible(tp, 3, 0, 1, 0, 0, 30, v) == 30 && pg_build_visible(tp, 3, 0, 1, 30, 0, 30, v) == 0, "shrink starts full, ends empty");
  CHECK(pg_build_visible(tp, 3, 0, 0, -5, 0, 30, v) == 0 && pg_build_visible(tp, 3, 0, 0, 99, 0, 30, v) == 30, "before start / after end");

  CHECK(pg_time_offset_frame(0, 10, 3, 1, 0, 0, 0, 0) == 13, "normal offset");
  CHECK(pg_time_offset_frame(0, 10, 0, 2, 0, 0, 0, 0) == 20, "scale");
  CHECK(pg_time_offset_frame(2, 10, 7, 1, 0, 0, 0, 0) == 7, "fixed");
  CHECK(pg_time_offset_frame(0, 12, 0, 1, 1, 1, 10, 1) == 2, "loop in range 1..10");
  CHECK(pg_time_offset_frame(0, 12, 0, 1, 1, 1, 10, 0) == 10, "no loop clamps");
  CHECK(pg_time_offset_frame(1, 1, 0, 1, 1, 1, 10, 1) == 10 && pg_time_offset_frame(1, 3, 0, 1, 1, 1, 10, 1) == 8, "reverse");
  CHECK(pg_time_offset_frame(3, 5, 0, 1, 1, 1, 4, 1) == 3 && pg_time_offset_frame(3, 7, 0, 1, 1, 1, 4, 1) == 1, "ping-pong 1 2 3 4 3 2 1");

  int pairs[16];
  CHECK(pg_envelope_segments(5, 0, 2, 0, pairs, 8) == 3 && pairs[0] == 0 && pairs[1] == 2 && pairs[5] == 4, "envelope segments i -> i+2");
  CHECK(pg_envelope_segments(5, 1, 2, 0, pairs, 8) == 5 && pairs[9] == 1, "cyclic wraps");
  CHECK(pg_envelope_segments(6, 0, 1, 1, pairs, 8) == 3 && pairs[2] == 2, "skip 1");

  CHECK(NEAR(pg_weight_angle(0, 0, 10, 0, 0, 0), 1.0f) && NEAR(pg_weight_angle(0, 0, 0, 10, 0, 0), 0.0f), "angle: aligned 1, perpendicular 0");

  float r[2];
  pg_guide_snap(0, 0, 0, 0, 0, 10, 0, 0, 30, r); CHECK(NEAR(r[0], 0) && NEAR(r[1], 10), "circular keeps the radius");
  pg_guide_snap(0, 0, 0, 0, 0, 10, 0, 30, 40, r); CHECK(NEAR(r[0], 6) && NEAR(r[1], 8), "circular scales any direction");
  pg_guide_snap(1, 0, 0, 0, 0, 10, 0, 7, 5, r); CHECK(NEAR(r[0], 7) && NEAR(r[1], 0), "radial projects on the ray");
  pg_guide_snap(2, 0, 0, 0, 0, 0, 5, 9, 8, r); CHECK(NEAR(r[0], 9) && NEAR(r[1], 5), "parallel horizontal through start");
  pg_guide_snap(3, 0, 0, 0, 10, 0, 0, 13, 27, r); CHECK(NEAR(r[0], 10) && NEAR(r[1], 30), "grid");
  pg_guide_snap(4, 0, 0, 0, 0, 0, 0, 0.5f, 9, r); CHECK(NEAR(r[0], 0) && NEAR(r[1], 9), "isometric picks the vertical");

  const int kt[4] = {0, 2, 0, 1}; unsigned char keep[4];
  CHECK(pg_onion_keytype_filter(kt, 4, 0, keep) == 2 && keep[0] && !keep[1], "keyframe type filter");
  CHECK(pg_onion_keytype_filter(kt, 4, -1, keep) == 4, "no filter keeps all");
  printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
  return fails != 0;
}
