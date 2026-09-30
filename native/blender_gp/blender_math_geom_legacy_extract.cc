/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Focused Blender 3.6.23 lasso helper required by Legacy GP lasso selection.
 * Kept here instead of linking the full blenlib lasso/math-geometry closure. */
static bool project_grease_isect_point_poly_v2_int(
    const int pt[2], const int verts[][2], const unsigned int nr)
{
  unsigned int i, j;
  bool isect = false;
  for (i = 0, j = nr - 1; i < nr; j = i++) {
    if (((verts[i][1] > pt[1]) != (verts[j][1] > pt[1])) &&
        (pt[0] <
         (verts[j][0] - verts[i][0]) * (pt[1] - verts[i][1]) /
                 (verts[j][1] - verts[i][1]) +
             verts[i][0])) {
      isect = !isect;
    }
  }
  return isect;
}

extern "C" bool BLI_lasso_is_point_inside(
    const int mcoords[][2],
    unsigned int mcoords_len,
    int sx,
    int sy,
    int error_value)
{
  if (sx == error_value || mcoords_len == 0) {
    return false;
  }

  const int pt[2] = {sx, sy};
  return project_grease_isect_point_poly_v2_int(pt, mcoords, mcoords_len);
}

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


/* Exact Blender 3.6.23 helpers required by the selected Legacy GP trim closure.
 * Kept here instead of linking the full blenlib math-geometry object. */
static float project_grease_closest_to_ray_v3(
    float r_close[3],
    const float p[3],
    const float ray_orig[3],
    const float ray_dir[3])
{
  float h[3], lambda;
  if (is_zero_v3(ray_dir)) {
    copy_v3_v3(r_close, ray_orig);
    return 0.0f;
  }
  sub_v3_v3v3(h, p, ray_orig);
  lambda = dot_v3v3(ray_dir, h) / dot_v3v3(ray_dir, ray_dir);
  madd_v3_v3v3fl(r_close, ray_orig, ray_dir, lambda);
  return lambda;
}

float closest_to_line_v3(
    float r_close[3],
    const float p[3],
    const float l1[3],
    const float l2[3])
{
  float u[3];
  sub_v3_v3v3(u, l2, l1);
  return project_grease_closest_to_ray_v3(r_close, p, l1, u);
}

static int project_grease_isect_line_line_epsilon_v3(
    const float v1[3],
    const float v2[3],
    const float v3[3],
    const float v4[3],
    float r_i1[3],
    float r_i2[3],
    const float epsilon)
{
  float a[3], b[3], c[3], ab[3], cb[3];
  float d, div;

  sub_v3_v3v3(c, v3, v1);
  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v4, v3);

  cross_v3_v3v3(ab, a, b);
  d = dot_v3v3(c, ab);
  div = dot_v3v3(ab, ab);

  if (div == 0.0f) {
    return 0;
  }

  if (fabsf(d) <= epsilon) {
    cross_v3_v3v3(cb, c, b);
    mul_v3_fl(a, dot_v3v3(cb, ab) / div);
    add_v3_v3v3(r_i1, v1, a);
    copy_v3_v3(r_i2, r_i1);
    return 1;
  }

  float n[3], t[3];
  float v3t[3], v4t[3];
  sub_v3_v3v3(t, v1, v3);
  cross_v3_v3v3(n, a, b);
  project_v3_v3v3(t, t, n);

  add_v3_v3v3(v3t, v3, t);
  add_v3_v3v3(v4t, v4, t);
  sub_v3_v3v3(c, v3t, v1);
  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v4t, v3t);

  cross_v3_v3v3(ab, a, b);
  cross_v3_v3v3(cb, c, b);
  mul_v3_fl(a, dot_v3v3(cb, ab) / dot_v3v3(ab, ab));
  add_v3_v3v3(r_i1, v1, a);
  sub_v3_v3v3(r_i2, r_i1, t);
  return 2;
}

float closest_to_line_segment_v3(
    float r_close[3],
    const float p[3],
    const float l1[3],
    const float l2[3])
{
  float lambda, cp[3];
  lambda = closest_to_line_v3(cp, p, l1, l2);
  if (lambda <= 0.0f) {
    copy_v3_v3(r_close, l1);
    return 0.0f;
  }
  if (lambda >= 1.0f) {
    copy_v3_v3(r_close, l2);
    return 1.0f;
  }
  copy_v3_v3(r_close, cp);
  return lambda;
}

int isect_line_line_v3(
    const float v1[3],
    const float v2[3],
    const float v3[3],
    const float v4[3],
    float r_i1[3],
    float r_i2[3])
{
  return project_grease_isect_line_line_epsilon_v3(
      v1, v2, v3, v4, r_i1, r_i2, 0.000001f);
}
