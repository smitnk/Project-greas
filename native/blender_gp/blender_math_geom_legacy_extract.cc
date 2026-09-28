/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exact helper extracted from Blender 3.6.23 source/blender/blenlib/intern/math_geom.c.
 * Only cross_poly_v2 is required by the selected Legacy Grease Pencil closure. */
#include "BLI_math_geom.h"

float cross_poly_v2(const float verts[][2], uint nr)
{
  uint a;
  float cross;
  const float *co_curr, *co_prev;

  co_prev = verts[nr - 1];
  co_curr = verts[0];
  cross = 0.0f;
  for (a = 0; a < nr; a++) {
    cross += (co_curr[0] - co_prev[0]) * (co_curr[1] + co_prev[1]);
    co_prev = co_curr;
    co_curr += 2;
  }

  return cross;
}

/* Exact Blender 3.6.23 Legacy BLI_math_geom helpers required by the focused Android GP eraser closure. */
float closest_to_line_v2(float r_close[2], const float p[2], const float l1[2], const float l2[2])
{
  float h[2], u[2], lambda, denom;
  sub_v2_v2v2(u, l2, l1);
  sub_v2_v2v2(h, p, l1);
  denom = dot_v2v2(u, u);
  if (denom == 0.0f) { r_close[0] = l1[0]; r_close[1] = l1[1]; return 0.0f; }
  lambda = dot_v2v2(u, h) / denom;
  r_close[0] = l1[0] + u[0] * lambda;
  r_close[1] = l1[1] + u[1] * lambda;
  return lambda;
}
float closest_to_line_segment_v2(float r_close[2], const float p[2], const float l1[2], const float l2[2])
{
  float lambda, cp[2];
  lambda = closest_to_line_v2(cp, p, l1, l2);
  if (lambda <= 0.0f) { copy_v2_v2(r_close, l1); return 0.0f; }
  if (lambda >= 1.0f) { copy_v2_v2(r_close, l2); return 1.0f; }
  copy_v2_v2(r_close, cp);
  return lambda;
}
float dist_squared_to_line_segment_v2(const float p[2], const float l1[2], const float l2[2])
{
  float closest[2];
  closest_to_line_segment_v2(closest, p, l1, l2);
  return len_squared_v2v2(closest, p);
}
