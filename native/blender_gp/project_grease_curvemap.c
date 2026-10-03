/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_curvemap.h. */
#include <float.h>
#include <math.h>
#include <string.h>

#include "project_grease_curvemap.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#define USE_ELASTIC_BLEND
#define CM_RESOL 32
#define CM_TABLEDIV (1.0f / 256.0f)

/* Handle types of the port (DNA_curve_types.h HD_AUTO / HD_VECT). */
enum { PGC_HD_AUTO = 1, PGC_HD_VECT = 2 };
typedef struct PGBez { float vec[3][3]; int h1, h2; } PGBez;

/* calchandle_curvemap (colortools.c), HD_AUTO_ANIM branch dropped (brush curves use auto/vector). */
static void pgc_calchandle(PGBez *bezt, const PGBez *prev, const PGBez *next)
{
  float *p2 = bezt->vec[1];
  float pt[3];
  const float *p1, *p3;
  if (bezt->h1 == 0 && bezt->h2 == 0) return;
  if (prev == NULL) {
    p3 = next->vec[1];
    pt[0] = 2.0f * p2[0] - p3[0];
    pt[1] = 2.0f * p2[1] - p3[1];
    p1 = pt;
  }
  else {
    p1 = prev->vec[1];
  }
  if (next == NULL) {
    p1 = prev->vec[1];
    pt[0] = 2.0f * p2[0] - p1[0];
    pt[1] = 2.0f * p2[1] - p1[1];
    p3 = pt;
  }
  else {
    p3 = next->vec[1];
  }
  float dvec_a[2] = {p2[0] - p1[0], p2[1] - p1[1]};
  float dvec_b[2] = {p3[0] - p2[0], p3[1] - p2[1]};
  float len_a = hypotf(dvec_a[0], dvec_a[1]);
  float len_b = hypotf(dvec_b[0], dvec_b[1]);
  if (len_a == 0.0f) len_a = 1.0f;
  if (len_b == 0.0f) len_b = 1.0f;
  if (bezt->h1 == PGC_HD_AUTO || bezt->h2 == PGC_HD_AUTO) {
    float tvec[2];
    tvec[0] = dvec_b[0] / len_b + dvec_a[0] / len_a;
    tvec[1] = dvec_b[1] / len_b + dvec_a[1] / len_a;
    float len = hypotf(tvec[0], tvec[1]) * 2.5614f;
    if (len != 0.0f) {
      if (bezt->h1 == PGC_HD_AUTO) {
        len_a /= len;
        bezt->vec[0][0] = p2[0] - tvec[0] * len_a;
        bezt->vec[0][1] = p2[1] - tvec[1] * len_a;
      }
      if (bezt->h2 == PGC_HD_AUTO) {
        len_b /= len;
        bezt->vec[2][0] = p2[0] + tvec[0] * len_b;
        bezt->vec[2][1] = p2[1] + tvec[1] * len_b;
      }
    }
  }
  if (bezt->h1 == PGC_HD_VECT) {
    bezt->vec[0][0] = p2[0] - dvec_a[0] / 3.0f;
    bezt->vec[0][1] = p2[1] - dvec_a[1] / 3.0f;
  }
  if (bezt->h2 == PGC_HD_VECT) {
    bezt->vec[2][0] = p2[0] + dvec_b[0] / 3.0f;
    bezt->vec[2][1] = p2[1] + dvec_b[1] / 3.0f;
  }
}

/* BKE_curve_correct_bezpart (curve.cc). */
static void pgc_correct_bezpart(const float v1[2], float v2[2], float v3[2], const float v4[2])
{
  float h1[2] = {v1[0] - v2[0], v1[1] - v2[1]}, h2[2] = {v4[0] - v3[0], v4[1] - v3[1]};
  const float len = v4[0] - v1[0], len1 = fabsf(h1[0]), len2 = fabsf(h2[0]);
  if ((len1 + len2) == 0.0f) return;
  if ((len1 + len2) > len) {
    const float fac = len / (len1 + len2);
    v2[0] = (v1[0] - fac * h1[0]);
    v2[1] = (v1[1] - fac * h1[1]);
    v3[0] = (v4[0] - fac * h2[0]);
    v3[1] = (v4[1] - fac * h2[1]);
  }
}

/* BKE_curve_forward_diff_bezier (curve.cc). */
static void pgc_forward_diff(float q0, float q1, float q2, float q3, float *p, int it, int stride)
{
  float f = (float)it;
  const float rt0 = q0, rt1 = 3.0f * (q1 - q0) / f;
  f *= f;
  const float rt2 = 3.0f * (q0 - 2.0f * q1 + q2) / f;
  f *= it;
  const float rt3 = (q3 - q0 + 3.0f * (q1 - q2)) / f;
  q0 = rt0;
  q1 = rt1 + rt2 + rt3;
  q2 = 2 * rt2 + 6 * rt3;
  q3 = 6 * rt3;
  for (int a = 0; a <= it; a++) {
    *p = q0;
    p += stride;
    q0 += q1;
    q1 += q2;
    q2 += q3;
  }
}

void pg_curve_init_linear(PGCurve *c)
{
  const float xy[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  pg_curve_set(c, xy, 2);
}

int pg_curve_set(PGCurve *c, const float *xy, int count)
{
  if (c == NULL || xy == NULL || count < 2 || count > PG_CURVE_MAX_POINTS) return 0;
  memset(c, 0, sizeof(*c));
  for (int i = 0; i < count; i++) {
    float x = xy[2 * i], y = xy[2 * i + 1];
    if (!isfinite(x) || !isfinite(y)) return 0;
    x = x < 0 ? 0 : (x > 1 ? 1 : x);
    y = y < 0 ? 0 : (y > 1 ? 1 : y);
    /* insertion by x, as sort_curvepoints after editing */
    int j = c->totpoint;
    while (j > 0 && c->x[j - 1] > x) { c->x[j] = c->x[j - 1]; c->y[j] = c->y[j - 1]; j--; }
    c->x[j] = x; c->y[j] = y; c->totpoint++;
  }
  pg_curve_make_table(c);
  return 1;
}

/* curvemap_calc_extend without CUMA_EXTEND_EXTRAPOLATE: horizontal. */
static float pgc_extend(float x, const float first[2], const float last[2])
{
  return x <= first[0] ? first[1] : last[1];
}

void pg_curve_make_table(PGCurve *c)
{
  const int n = c->totpoint;
  PGBez bezt[PG_CURVE_MAX_POINTS];
  memset(bezt, 0, sizeof(bezt));
  c->mintable = 0.0f; /* clipr xmin */
  c->maxtable = 1.0f; /* clipr xmax */
  for (int a = 0; a < n; a++) {
    c->mintable = fminf(c->mintable, c->x[a]);
    c->maxtable = fmaxf(c->maxtable, c->x[a]);
    bezt[a].vec[1][0] = c->x[a];
    bezt[a].vec[1][1] = c->y[a];
    bezt[a].h1 = bezt[a].h2 = c->vector[a] ? PGC_HD_VECT : PGC_HD_AUTO;
  }
  const PGBez *prev = NULL;
  for (int a = 0; a < n; a++) {
    pgc_calchandle(&bezt[a], prev, a != n - 1 ? &bezt[a + 1] : NULL);
    prev = &bezt[a];
  }
  /* first and last handle point to the closest handle (colortools.c) */
  if (n > 2) {
    if (bezt[0].h2 == PGC_HD_AUTO) {
      const float hlen = hypotf(bezt[0].vec[2][0] - bezt[0].vec[1][0], bezt[0].vec[2][1] - bezt[0].vec[1][1]);
      float vec[2] = {bezt[1].vec[0][0], bezt[1].vec[0][1]};
      if (vec[0] < bezt[0].vec[1][0]) vec[0] = bezt[0].vec[1][0];
      vec[0] -= bezt[0].vec[1][0];
      vec[1] -= bezt[0].vec[1][1];
      const float nlen = hypotf(vec[0], vec[1]);
      if (nlen > FLT_EPSILON) {
        vec[0] *= hlen / nlen;
        vec[1] *= hlen / nlen;
        bezt[0].vec[2][0] = vec[0] + bezt[0].vec[1][0];
        bezt[0].vec[2][1] = vec[1] + bezt[0].vec[1][1];
        bezt[0].vec[0][0] = bezt[0].vec[1][0] - vec[0];
        bezt[0].vec[0][1] = bezt[0].vec[1][1] - vec[1];
      }
    }
    const int a = n - 1;
    if (bezt[a].h2 == PGC_HD_AUTO) {
      const float hlen = hypotf(bezt[a].vec[0][0] - bezt[a].vec[1][0], bezt[a].vec[0][1] - bezt[a].vec[1][1]);
      float vec[2] = {bezt[a - 1].vec[2][0], bezt[a - 1].vec[2][1]};
      if (vec[0] > bezt[a].vec[1][0]) vec[0] = bezt[a].vec[1][0];
      vec[0] -= bezt[a].vec[1][0];
      vec[1] -= bezt[a].vec[1][1];
      const float nlen = hypotf(vec[0], vec[1]);
      if (nlen > FLT_EPSILON) {
        vec[0] *= hlen / nlen;
        vec[1] *= hlen / nlen;
        bezt[a].vec[0][0] = vec[0] + bezt[a].vec[1][0];
        bezt[a].vec[0][1] = vec[1] + bezt[a].vec[1][1];
        bezt[a].vec[2][0] = bezt[a].vec[1][0] - vec[0];
        bezt[a].vec[2][1] = bezt[a].vec[1][1] - vec[1];
      }
    }
  }
  /* the bezier curve */
  const int totpoint = (n - 1) * CM_RESOL;
  float allpoints[(PG_CURVE_MAX_POINTS - 1) * CM_RESOL * 2];
  float *point = allpoints;
  for (int a = 0; a < n - 1; a++, point += 2 * CM_RESOL) {
    pgc_correct_bezpart(bezt[a].vec[1], bezt[a].vec[2], bezt[a + 1].vec[0], bezt[a + 1].vec[1]);
    pgc_forward_diff(bezt[a].vec[1][0], bezt[a].vec[2][0], bezt[a + 1].vec[0][0], bezt[a + 1].vec[1][0],
                     point, CM_RESOL - 1, 2);
    pgc_forward_diff(bezt[a].vec[1][1], bezt[a].vec[2][1], bezt[a + 1].vec[0][1], bezt[a + 1].vec[1][1],
                     point + 1, CM_RESOL - 1, 2);
  }
  const float range = CM_TABLEDIV * (c->maxtable - c->mintable);
  c->range = 1.0f / range;
  /* table with CM_TABLE equal x distances */
  const float *firstpoint = allpoints;
  const float *lastpoint = allpoints + 2 * (totpoint - 1);
  point = allpoints;
  for (int a = 0; a <= PG_CURVE_TABLE; a++) {
    const float cur_x = c->mintable + range * (float)a;
    c->table_x[a] = cur_x;
    while (cur_x >= point[0] && point != lastpoint) point += 2;
    if (point == firstpoint || (point == lastpoint && cur_x >= point[0])) {
      if (fabsf(cur_x - point[0]) <= 1e-6f) c->table_y[a] = point[1];
      else c->table_y[a] = pgc_extend(cur_x, firstpoint, lastpoint);
    }
    else {
      float fac1 = point[0] - point[-2];
      const float fac2 = point[0] - cur_x;
      fac1 = fac1 > FLT_EPSILON ? fac2 / fac1 : 0.0f;
      c->table_y[a] = fac1 * point[-1] + (1.0f - fac1) * point[1];
    }
  }
  c->built = 1;
}

float pg_curve_evaluate(const PGCurve *c, float value)
{
  if (c == NULL || !c->built || !isfinite(value)) return value;
  float fi = (value - c->mintable) * c->range;
  const int i = (int)fi;
  float val;
  if (fi < 0.0f || fi > PG_CURVE_TABLE) {
    const float first[2] = {c->table_x[0], c->table_y[0]};
    const float last[2] = {c->table_x[PG_CURVE_TABLE], c->table_y[PG_CURVE_TABLE]};
    val = pgc_extend(value, first, last);
  }
  else if (i >= PG_CURVE_TABLE) {
    val = c->table_y[PG_CURVE_TABLE];
  }
  else {
    fi -= (float)i;
    val = (1.0f - fi) * c->table_y[i] + fi * c->table_y[i + 1];
  }
  return val < 0.0f ? 0.0f : (val > 1.0f ? 1.0f : val); /* CUMA_DO_CLIP, clipr 0..1 */
}

int pg_curve_is_linear(const PGCurve *c)
{
  return c == NULL || !c->built ||
         (c->totpoint == 2 && c->x[0] == 0.0f && c->y[0] == 0.0f && c->x[1] == 1.0f && c->y[1] == 1.0f);
}

/* ---- Elastic easing (blenlib/intern/easing.c) ---- */
/* BEGIN VERBATIM source/blender/blenlib/intern/easing.c */
#ifdef USE_ELASTIC_BLEND
/**
 * When the amplitude is less than the change, we need to blend
 * \a f when we're close to the crossing point (int time), else we get an ugly sharp falloff.
 */
static float elastic_blend(
    float time, float change, float duration, float amplitude, float s, float f)
{
  if (change) {
    /* Looks like a magic number,
     * but this is a part of the sine curve we need to blend from */
    const float t = fabsf(s);
    if (amplitude) {
      f *= amplitude / fabsf(change);
    }
    else {
      f = 0.0f;
    }

    if (fabsf(time * duration) < t) {
      float l = fabsf(time * duration) / t;
      f = (f * l) + (1.0f - l);
    }
  }

  return f;
}
#endif

float BLI_easing_elastic_ease_in(
    float time, float begin, float change, float duration, float amplitude, float period)
{
  float s;
  float f = 1.0f;

  if (time == 0.0f) {
    return begin;
  }

  if ((time /= duration) == 1.0f) {
    return begin + change;
  }
  time -= 1.0f;
  if (!period) {
    period = duration * 0.3f;
  }
  if (!amplitude || amplitude < fabsf(change)) {
    s = period / 4;
#ifdef USE_ELASTIC_BLEND
    f = elastic_blend(time, change, duration, amplitude, s, f);
#endif
    amplitude = change;
  }
  else {
    s = period / (2 * (float)M_PI) * asinf(change / amplitude);
  }

  return (-f * (amplitude * powf(2, 10 * time) *
                sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
         begin;
}

float BLI_easing_elastic_ease_out(
    float time, float begin, float change, float duration, float amplitude, float period)
{
  float s;
  float f = 1.0f;

  if (time == 0.0f) {
    return begin;
  }
  if ((time /= duration) == 1.0f) {
    return begin + change;
  }
  time = -time;
  if (!period) {
    period = duration * 0.3f;
  }
  if (!amplitude || amplitude < fabsf(change)) {
    s = period / 4;
#ifdef USE_ELASTIC_BLEND
    f = elastic_blend(time, change, duration, amplitude, s, f);
#endif
    amplitude = change;
  }
  else {
    s = period / (2 * (float)M_PI) * asinf(change / amplitude);
  }

  return (f * (amplitude * powf(2, 10 * time) *
               sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
         change + begin;
}

float BLI_easing_elastic_ease_in_out(
    float time, float begin, float change, float duration, float amplitude, float period)
{
  float s;
  float f = 1.0f;

  if (time == 0.0f) {
    return begin;
  }
  if ((time /= duration / 2) == 2.0f) {
    return begin + change;
  }
  time -= 1.0f;
  if (!period) {
    period = duration * (0.3f * 1.5f);
  }
  if (!amplitude || amplitude < fabsf(change)) {
    s = period / 4;
#ifdef USE_ELASTIC_BLEND
    f = elastic_blend(time, change, duration, amplitude, s, f);
#endif
    amplitude = change;
  }
  else {
    s = period / (2 * (float)M_PI) * asinf(change / amplitude);
  }

  if (time < 0.0f) {
    f *= -0.5f;
    return (f * (amplitude * powf(2, 10 * time) *
                 sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
           begin;
  }

  time = -time;
  f *= 0.5f;
  return (f * (amplitude * powf(2, 10 * time) *
               sinf((time * duration - s) * (2 * (float)M_PI) / period))) +
         change + begin;
}
/* END VERBATIM */

float pg_easing_elastic(int mode, float t, float amplitude, float period)
{
  if (!isfinite(t)) return 0.0f;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  switch (mode) {
    case 1: return BLI_easing_elastic_ease_out(t, 0.0f, 1.0f, 1.0f, amplitude, period);
    case 2: return BLI_easing_elastic_ease_in_out(t, 0.0f, 1.0f, 1.0f, amplitude, period);
    default: return BLI_easing_elastic_ease_in(t, 0.0f, 1.0f, 1.0f, amplitude, period);
  }
}
