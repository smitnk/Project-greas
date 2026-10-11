/* SPDX-License-Identifier: GPL-2.0-or-later
 * key_curve_position_weights() from Blender 3.6.23 key.cc. colorband.c (L5) needs it for B-spline
 * / cardinal colour-band interpolation (Tint gradients); key.cc itself (shape keys, 3D) needs the
 * generated RNA_prototypes.h of a full Blender build, so only this function is taken. */
#include <stdbool.h>
#include <stddef.h>

#include "BKE_key.h"
#include "DNA_key_types.h"

/* BEGIN VERBATIM source/blender/blenkernel/intern/key.cc */
void key_curve_position_weights(float t, float data[4], int type)
{
  float t2, t3, fc;

  if (type == KEY_LINEAR) {
    data[0] = 0.0f;
    data[1] = -t + 1.0f;
    data[2] = t;
    data[3] = 0.0f;
  }
  else if (type == KEY_CARDINAL) {
    t2 = t * t;
    t3 = t2 * t;
    fc = 0.71f;

    data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
    data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
    data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
    data[3] = fc * t3 - fc * t2;
  }
  else if (type == KEY_BSPLINE) {
    t2 = t * t;
    t3 = t2 * t;

    data[0] = -0.16666666f * t3 + 0.5f * t2 - 0.5f * t + 0.16666666f;
    data[1] = 0.5f * t3 - t2 + 0.66666666f;
    data[2] = -0.5f * t3 + 0.5f * t2 + 0.5f * t + 0.16666666f;
    data[3] = 0.16666666f * t3;
  }
  else if (type == KEY_CATMULL_ROM) {
    t2 = t * t;
    t3 = t2 * t;
    fc = 0.5f;

    data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
    data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
    data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
    data[3] = fc * t3 - fc * t2;
  }
}
/* END VERBATIM */
