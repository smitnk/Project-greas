/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Focused extraction from Blender 3.6.23 source/blender/blenkernel/intern/curve.cc.
 * Only the two curve helpers required by the Legacy Grease Pencil conversion path
 * are compiled; the full Blender curve subsystem is intentionally not linked. */
#include "BKE_curve.h"
#include "BLI_utildefines.h"

void BKE_curve_forward_diff_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride)
{
  float rt0, rt1, rt2, rt3, f;
  int a;

  f = float(it);
  rt0 = q0;
  rt1 = 3.0f * (q1 - q0) / f;
  f *= f;
  rt2 = 3.0f * (q0 - 2.0f * q1 + q2) / f;
  f *= it;
  rt3 = (q3 - q0 + 3.0f * (q1 - q2)) / f;

  q0 = rt0;
  q1 = rt1 + rt2 + rt3;
  q2 = 2 * rt2 + 6 * rt3;
  q3 = 6 * rt3;

  for (a = 0; a <= it; a++) {
    *p = q0;
    p = (float *)POINTER_OFFSET(p, stride);
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}

uint BKE_curve_calc_coords_axis_len(const uint bezt_array_len,
                                    const uint resolu,
                                    const bool is_cyclic,
                                    const bool use_cyclic_duplicate_endpoint)
{
  const uint segments = bezt_array_len - (is_cyclic ? 0 : 1);
  const uint points_len = (segments * resolu) + (is_cyclic ? (use_cyclic_duplicate_endpoint) : 1);
  return points_len;
}
