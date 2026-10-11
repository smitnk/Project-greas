/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * CurveMapping evaluation for Project Grease (brush pressure/strength curves, modifier custom
 * curves). L5: the table is built and evaluated by Blender 3.6.23's colortools.c
 * (BKE_curvemapping_changed / BKE_curvemapping_evaluateF) on a one-channel CurveMapping set up
 * like BKE_curvemapping_add(1, 0, 0, 1, 1): CUMA_DO_CLIP over clipr (0,0)-(1,1) and
 * CUMA_EXTEND_EXTRAPOLATE. PGCurve keeps a copy of the resulting table so it stays a value type.
 *
 * Also the elastic easing (BLI_easing_elastic_*, verbatim) used by interpolation.
 */
#pragma once
#include "DNA_color_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define PG_CURVE_MAX_POINTS 8
#define PG_CURVE_TABLE 256 /* CM_TABLE */

typedef struct PGCurve {
  int totpoint;                    /* 2..PG_CURVE_MAX_POINTS */
  float x[PG_CURVE_MAX_POINTS];
  float y[PG_CURVE_MAX_POINTS];
  unsigned char vector[PG_CURVE_MAX_POINTS]; /* CUMA_HANDLE_VECTOR: 1, else auto */
  /* built by pg_curve_make_table */
  float mintable, maxtable, range;
  CurveMapPoint table[PG_CURVE_TABLE + 1]; /* CurveMap.table, built by Blender's colortools.c */
  float ext_in[2], ext_out[2];             /* CurveMap.ext_in / ext_out (extrapolation) */
  int built;
} PGCurve;

/* Linear (0,0)-(1,1), Blender's default brush curve (CURVE_PRESET_LINE). */
void pg_curve_init_linear(PGCurve *c);
/* Sets the points (sorted by x is enforced here) and builds the table. Returns 0 on bad input. */
int pg_curve_set(PGCurve *c, const float *xy, int count);
void pg_curve_make_table(PGCurve *c);
/* BKE_curvemapping_evaluateF with clipping to [0, 1]. */
float pg_curve_evaluate(const PGCurve *c, float value);
/* True when the curve is the identity line (lets callers skip the table). */
int pg_curve_is_linear(const PGCurve *c);

float BLI_easing_elastic_ease_in(float time, float begin, float change, float duration, float amplitude, float period);
float BLI_easing_elastic_ease_out(float time, float begin, float change, float duration, float amplitude, float period);
float BLI_easing_elastic_ease_in_out(float time, float begin, float change, float duration, float amplitude, float period);
/* gpencil_interpolate.c: BEZT_IPO_ELASTIC with the operator's amplitude / period, mode 0 in, 1 out, 2 in-out. */
float pg_easing_elastic(int mode, float t, float amplitude, float period);

#ifdef __cplusplus
}
#endif
