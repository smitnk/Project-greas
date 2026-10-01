/* Test-only stand-in for the Blender header of the same name: just the
 * declarations project_grease_blender_primitive.c needs, with Blender 3.6.23
 * math semantics, so the primitive code can be unit-tested without the
 * pinned Blender tree. Production builds use the real headers. */
#pragma once
#include <math.h>
static inline void copy_v2_v2(float r[2], const float a[2]) { r[0] = a[0]; r[1] = a[1]; }
static inline void copy_v4_v4(float r[4], const float a[4]) { for (int i = 0; i < 4; i++) r[i] = a[i]; }
static inline void sub_v2_v2v2(float r[2], const float a[2], const float b[2]) { r[0] = a[0] - b[0]; r[1] = a[1] - b[1]; }
static inline void add_v2_v2v2(float r[2], const float a[2], const float b[2]) { r[0] = a[0] + b[0]; r[1] = a[1] + b[1]; }
static inline void mid_v2_v2v2(float r[2], const float a[2], const float b[2]) { r[0] = 0.5f * (a[0] + b[0]); r[1] = 0.5f * (a[1] + b[1]); }
static inline void interp_v2_v2v2(float r[2], const float a[2], const float b[2], const float t) { const float s = 1.0f - t; r[0] = s * a[0] + t * b[0]; r[1] = s * a[1] + t * b[1]; }
static inline void rotate_v2_v2fl(float r[2], const float p[2], const float angle) { const float co = cosf(angle), si = sinf(angle); r[0] = co * p[0] - si * p[1]; r[1] = si * p[0] + co * p[1]; }
static inline void interp_v2_v2v2v2v2_cubic(float p[2], const float v1[2], const float v2[2], const float v3[2], const float v4[2], const float u)
{ float q0[2], q1[2], q2[2], r0[2], r1[2]; interp_v2_v2v2(q0, v1, v2, u); interp_v2_v2v2(q1, v2, v3, u); interp_v2_v2v2(q2, v3, v4, u); interp_v2_v2v2(r0, q0, q1, u); interp_v2_v2v2(r1, q1, q2, u); interp_v2_v2v2(p, r0, r1, u); }
