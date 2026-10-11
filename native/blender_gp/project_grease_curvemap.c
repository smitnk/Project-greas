/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_curvemap.h. */
#include <float.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "BKE_colortools.h"
#include "BLI_rect.h"
#include "MEM_guardedalloc.h"

#include "project_grease_curvemap.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#define USE_ELASTIC_BLEND

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

/* L5: the table is built by Blender's colortools.c (BKE_curvemapping_changed / curvemap_make_table)
 * on a one-channel CurveMapping made exactly as GP brushes and modifiers make theirs
 * (BKE_curvemapping_add(1, 0, 0, 1, 1): CUMA_DO_CLIP | CUMA_EXTEND_EXTRAPOLATE, clipr 0..1). The
 * result is copied into PGCurve so it stays a plain value type. */

void pg_curve_make_table(PGCurve *c)
{
  CurveMapping cm;
  memset(&cm, 0, sizeof(cm));
  BKE_curvemapping_set_defaults(&cm, 1, 0.0f, 0.0f, 1.0f, 1.0f); /* as BKE_curvemapping_add */
  CurveMap *dst = &cm.cm[0];
  MEM_SAFE_FREE(dst->curve);
  dst->totpoint = c->totpoint;
  dst->curve = MEM_callocN(sizeof(CurveMapPoint) * (size_t)c->totpoint, __func__);
  for (int a = 0; a < c->totpoint; a++) {
    dst->curve[a].x = c->x[a];
    dst->curve[a].y = c->y[a];
    if (c->vector[a]) {
      dst->curve[a].flag |= CUMA_HANDLE_VECTOR;
    }
  }
  BKE_curvemapping_changed(&cm, false);
  BKE_curvemapping_init(&cm);
  memcpy(c->table, dst->table, sizeof(c->table));
  c->mintable = dst->mintable;
  c->maxtable = dst->maxtable;
  c->range = dst->range;
  memcpy(c->ext_in, dst->ext_in, sizeof(c->ext_in));
  memcpy(c->ext_out, dst->ext_out, sizeof(c->ext_out));
  /* changed() may re-sort or drop points; keep PGCurve's point list what Blender kept. */
  c->totpoint = dst->totpoint;
  for (int a = 0; a < dst->totpoint; a++) {
    c->x[a] = dst->curve[a].x;
    c->y[a] = dst->curve[a].y;
    c->vector[a] = (dst->curve[a].flag & CUMA_HANDLE_VECTOR) != 0;
  }
  BKE_curvemapping_free_data(&cm);
  c->built = 1;
}

float pg_curve_evaluate(const PGCurve *c, float value)
{
  if (c == NULL || !c->built || !isfinite(value)) return value;
  /* Only what BKE_curvemapping_evaluateF reads: the flag and clipr BKE_curvemapping_set_defaults
   * gives a GP curve, plus the stored table. No allocation per evaluation. */
  CurveMapping cm;
  memset(&cm, 0, sizeof(cm));
  cm.flag = CUMA_DO_CLIP | CUMA_EXTEND_EXTRAPOLATE;
  BLI_rctf_init(&cm.clipr, 0.0f, 1.0f, 0.0f, 1.0f);
  CurveMap cuma;
  memset(&cuma, 0, sizeof(cuma));
  cuma.table = (CurveMapPoint *)c->table; /* read only */
  cuma.mintable = c->mintable;
  cuma.maxtable = c->maxtable;
  cuma.range = c->range;
  memcpy(cuma.ext_in, c->ext_in, sizeof(cuma.ext_in));
  memcpy(cuma.ext_out, c->ext_out, sizeof(cuma.ext_out));
  cm.cm[0] = cuma;
  return BKE_curvemapping_evaluateF(&cm, 0, value);
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
