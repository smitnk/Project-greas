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
