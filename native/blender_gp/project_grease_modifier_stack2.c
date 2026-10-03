/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Batch 21 entries of the live modifier stack (see project_grease_modifier_stack.h):
 *   Build, Time Offset, Hook (2D), Lattice (2D), Envelope, Vertex Weight Proximity / Angle and the
 *   live generators Dot Dash, Outline, Mirror, Array, MultipleStrokes; the common influence filter
 *   block and the custom curve.
 * Build / Time / Hook / Lattice use the tested 2D functions of project_grease_blender_mod2.c.
 * Envelope carries the pinned MOD_gpencil_legacy_envelope.c functions as VERBATIM regions; the glue
 * (generate_geometry / deformStroke without Object, Depsgraph and the layer/material filters) is
 * ADAPTED. The generators reuse the baked operators of edit4 / edit6 / edit on the evaluated copy
 * through a one-layer view of the document whose active frame is the evaluated frame.
 */

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit4.h"
#include "project_grease_blender_edit6.h"
#include "project_grease_blender_mod2.h"
#include "project_grease_curvemap.h"
#include "project_grease_modifier_stack.h"

static float m2_clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float m2_int(float v, float lo, float hi) { return m2_clamp(floorf(v + 0.5f), lo, hi); }
static float m2_flag(float v) { return v != 0.0f ? 1.0f : 0.0f; }

/* ---------------------------------------------------------------------------------------- */
/* Defaults (DNA_gpencil_modifier_defaults.h where Blender has the setting) and sanitizing  */

void pg_mod2_defaults(int type, float p[PG_MOD_MAX_PARAMS])
{
  switch (type) {
    case PG_MOD_BUILD: /* mode sequential, grow, start_delay 0, length 100 */
      p[PG_P_BUILD_LENGTH] = 100.0f;
      break;
    case PG_MOD_TIME: /* normal, offset 1, frame_scale 1, sfra 1, efra 250, GP_TIME_KEEP_LOOP */
      p[PG_P_TIME_OFFSET] = 1.0f;
      p[PG_P_TIME_SCALE] = 1.0f;
      p[PG_P_TIME_SFRA] = 1.0f;
      p[PG_P_TIME_EFRA] = 250.0f;
      p[PG_P_TIME_LOOP] = 1.0f;
      break;
    case PG_MOD_HOOK: /* smooth falloff, force 0.5 (Blender); 2D: canvas centre, radius 200 px */
      p[PG_P_HOOK_CX] = 640.0f;
      p[PG_P_HOOK_CY] = 360.0f;
      p[PG_P_HOOK_SCALE] = 1.0f;
      p[PG_P_HOOK_RADIUS] = 200.0f;
      p[PG_P_HOOK_FALLOFF] = 1.0f;
      p[PG_P_HOOK_STRENGTH] = 0.5f;
      break;
    case PG_MOD_LATTICE: /* strength 1; 2D: 3x3 grid over the 1280x720 canvas */
      p[PG_P_LATTICE_X1] = 1280.0f;
      p[PG_P_LATTICE_Y1] = 720.0f;
      p[PG_P_LATTICE_NU] = 3.0f;
      p[PG_P_LATTICE_NV] = 3.0f;
      p[PG_P_LATTICE_STRENGTH] = 1.0f;
      break;
    case PG_MOD_ENVELOPE: /* spread 10, mode segments, mat_nr -1, thickness 1, strength 1, skip 0 */
      p[PG_P_ENVELOPE_MODE] = GP_ENVELOPE_SEGMENTS;
      p[PG_P_ENVELOPE_SPREAD] = 10.0f;
      p[PG_P_ENVELOPE_THICKNESS] = 1.0f;
      p[PG_P_ENVELOPE_STRENGTH] = 1.0f;
      p[PG_P_ENVELOPE_MAT] = -1.0f;
      break;
    case PG_MOD_WEIGHT_PROX: /* dist_start 0, dist_end 20 (canvas px here) */
      p[PG_P_WPROX_X] = 640.0f;
      p[PG_P_WPROX_Y] = 360.0f;
      p[PG_P_WPROX_DIST_END] = 20.0f;
      break;
    case PG_MOD_WEIGHT_ANGLE:
      break;
    case PG_MOD_DASH: /* one segment: dash 2, gap 1 (DashGpencilModifierSegment defaults) */
      p[PG_P_DASH_DASH] = 2.0f;
      p[PG_P_DASH_GAP] = 1.0f;
      break;
    case PG_MOD_OUTLINE: /* thickness 1, subdiv 3 */
      p[PG_P_OUTLINE_THICKNESS] = 1.0f;
      p[PG_P_OUTLINE_SUBDIV] = 3.0f;
      break;
    case PG_MOD_MIRROR: /* GP_MIRROR_AXIS_X about the canvas centre */
      p[PG_P_MIRROR_X] = 1.0f;
      p[PG_P_MIRROR_PX] = 640.0f;
      p[PG_P_MIRROR_PY] = 360.0f;
      break;
    case PG_MOD_ARRAY: /* count 2, relative shift 1 (here 100 px in x) */
      p[PG_P_ARRAY_COUNT_N] = 2.0f;
      p[PG_P_ARRAY_OX] = 100.0f;
      break;
    case PG_MOD_MULTIPLY: /* duplications 3, distance 0.1 (here 10 px) */
      p[PG_P_MULTIPLY_DUPLICATIONS] = 3.0f;
      p[PG_P_MULTIPLY_DISTANCE] = 10.0f;
      break;
    default:
      break;
  }
}

void pg_mod2_sanitize(int type, float p[PG_MOD_MAX_PARAMS])
{
  switch (type) {
    case PG_MOD_BUILD:
      p[PG_P_BUILD_MODE] = m2_int(p[PG_P_BUILD_MODE], 0, 1);
      p[PG_P_BUILD_TRANSITION] = m2_int(p[PG_P_BUILD_TRANSITION], 0, 1);
      p[PG_P_BUILD_START] = m2_clamp(p[PG_P_BUILD_START], 0.0f, 10000.0f);
      p[PG_P_BUILD_LENGTH] = m2_clamp(p[PG_P_BUILD_LENGTH], 1.0f, 10000.0f);
      break;
    case PG_MOD_TIME:
      p[PG_P_TIME_MODE] = m2_int(p[PG_P_TIME_MODE], 0, 3);
      p[PG_P_TIME_OFFSET] = m2_int(p[PG_P_TIME_OFFSET], -100000, 100000);
      p[PG_P_TIME_SCALE] = m2_clamp(p[PG_P_TIME_SCALE], 0.001f, 100.0f);
      p[PG_P_TIME_USE_RANGE] = m2_flag(p[PG_P_TIME_USE_RANGE]);
      p[PG_P_TIME_SFRA] = m2_int(p[PG_P_TIME_SFRA], 0, 100000);
      p[PG_P_TIME_EFRA] = m2_int(p[PG_P_TIME_EFRA], p[PG_P_TIME_SFRA], 100000);
      p[PG_P_TIME_LOOP] = m2_flag(p[PG_P_TIME_LOOP]);
      break;
    case PG_MOD_HOOK:
      for (int i = PG_P_HOOK_CX; i <= PG_P_HOOK_DY; i++) p[i] = m2_clamp(p[i], -100000.0f, 100000.0f);
      p[PG_P_HOOK_ANGLE] = m2_clamp(p[PG_P_HOOK_ANGLE], -(float)M_PI * 4, (float)M_PI * 4);
      p[PG_P_HOOK_SCALE] = m2_clamp(p[PG_P_HOOK_SCALE], 0.0f, 100.0f);
      p[PG_P_HOOK_RADIUS] = m2_clamp(p[PG_P_HOOK_RADIUS], 0.0f, 100000.0f);
      p[PG_P_HOOK_FALLOFF] = m2_int(p[PG_P_HOOK_FALLOFF], 0, 2);
      p[PG_P_HOOK_STRENGTH] = m2_clamp(p[PG_P_HOOK_STRENGTH], 0.0f, 1.0f);
      break;
    case PG_MOD_LATTICE:
      for (int i = PG_P_LATTICE_X0; i <= PG_P_LATTICE_Y1; i++) p[i] = m2_clamp(p[i], -100000.0f, 100000.0f);
      if (!(p[PG_P_LATTICE_X1] > p[PG_P_LATTICE_X0])) p[PG_P_LATTICE_X1] = p[PG_P_LATTICE_X0] + 1.0f;
      if (!(p[PG_P_LATTICE_Y1] > p[PG_P_LATTICE_Y0])) p[PG_P_LATTICE_Y1] = p[PG_P_LATTICE_Y0] + 1.0f;
      p[PG_P_LATTICE_NU] = m2_int(p[PG_P_LATTICE_NU], 2, PG_LATTICE_MAX);
      p[PG_P_LATTICE_NV] = m2_int(p[PG_P_LATTICE_NV], 2, PG_LATTICE_MAX);
      p[PG_P_LATTICE_STRENGTH] = m2_clamp(p[PG_P_LATTICE_STRENGTH], 0.0f, 1.0f);
      for (int i = PG_P_LATTICE_OFFSETS; i < PG_P_LATTICE_COUNT; i++) p[i] = m2_clamp(p[i], -100000.0f, 100000.0f);
      break;
    case PG_MOD_ENVELOPE:
      p[PG_P_ENVELOPE_MODE] = m2_int(p[PG_P_ENVELOPE_MODE], GP_ENVELOPE_DEFORM, GP_ENVELOPE_FILLS);
      p[PG_P_ENVELOPE_SPREAD] = m2_int(p[PG_P_ENVELOPE_SPREAD], 1, 1000);
      p[PG_P_ENVELOPE_SKIP] = m2_int(p[PG_P_ENVELOPE_SKIP], 0, 1000);
      p[PG_P_ENVELOPE_THICKNESS] = m2_clamp(p[PG_P_ENVELOPE_THICKNESS], 0.0f, 100.0f);
      p[PG_P_ENVELOPE_STRENGTH] = m2_clamp(p[PG_P_ENVELOPE_STRENGTH], 0.0f, 1.0f);
      p[PG_P_ENVELOPE_MAT] = m2_int(p[PG_P_ENVELOPE_MAT], -1, 32767);
      break;
    case PG_MOD_WEIGHT_PROX:
      p[PG_P_WPROX_TARGET] = m2_int(p[PG_P_WPROX_TARGET], 0, 255);
      p[PG_P_WPROX_X] = m2_clamp(p[PG_P_WPROX_X], -100000.0f, 100000.0f);
      p[PG_P_WPROX_Y] = m2_clamp(p[PG_P_WPROX_Y], -100000.0f, 100000.0f);
      p[PG_P_WPROX_DIST_START] = m2_clamp(p[PG_P_WPROX_DIST_START], 0.0f, 100000.0f);
      p[PG_P_WPROX_DIST_END] = m2_clamp(p[PG_P_WPROX_DIST_END], 0.0f, 100000.0f);
      p[PG_P_WPROX_MIN_WEIGHT] = m2_clamp(p[PG_P_WPROX_MIN_WEIGHT], 0.0f, 1.0f);
      p[PG_P_WPROX_INVERT] = m2_flag(p[PG_P_WPROX_INVERT]);
      p[PG_P_WPROX_MULTIPLY] = m2_flag(p[PG_P_WPROX_MULTIPLY]);
      break;
    case PG_MOD_WEIGHT_ANGLE:
      p[PG_P_WANGLE_TARGET] = m2_int(p[PG_P_WANGLE_TARGET], 0, 255);
      p[PG_P_WANGLE_ANGLE] = m2_clamp(p[PG_P_WANGLE_ANGLE], -(float)M_PI * 2, (float)M_PI * 2);
      p[PG_P_WANGLE_MIN_WEIGHT] = m2_clamp(p[PG_P_WANGLE_MIN_WEIGHT], 0.0f, 1.0f);
      p[PG_P_WANGLE_INVERT] = m2_flag(p[PG_P_WANGLE_INVERT]);
      p[PG_P_WANGLE_MULTIPLY] = m2_flag(p[PG_P_WANGLE_MULTIPLY]);
      break;
    case PG_MOD_DASH:
      p[PG_P_DASH_DASH] = m2_int(p[PG_P_DASH_DASH], 1, 1000);
      p[PG_P_DASH_GAP] = m2_int(p[PG_P_DASH_GAP], 1, 1000);
      p[PG_P_DASH_OFFSET] = m2_int(p[PG_P_DASH_OFFSET], -1000, 1000);
      break;
    case PG_MOD_OUTLINE:
      p[PG_P_OUTLINE_THICKNESS] = m2_int(p[PG_P_OUTLINE_THICKNESS], 1, 1000);
      p[PG_P_OUTLINE_SUBDIV] = m2_int(p[PG_P_OUTLINE_SUBDIV], 1, 64);
      break;
    case PG_MOD_MIRROR:
      p[PG_P_MIRROR_X] = m2_flag(p[PG_P_MIRROR_X]);
      p[PG_P_MIRROR_Y] = m2_flag(p[PG_P_MIRROR_Y]);
      p[PG_P_MIRROR_PX] = m2_clamp(p[PG_P_MIRROR_PX], -100000.0f, 100000.0f);
      p[PG_P_MIRROR_PY] = m2_clamp(p[PG_P_MIRROR_PY], -100000.0f, 100000.0f);
      break;
    case PG_MOD_ARRAY:
      p[PG_P_ARRAY_COUNT_N] = m2_int(p[PG_P_ARRAY_COUNT_N], 2, 1000);
      p[PG_P_ARRAY_OX] = m2_clamp(p[PG_P_ARRAY_OX], -100000.0f, 100000.0f);
      p[PG_P_ARRAY_OY] = m2_clamp(p[PG_P_ARRAY_OY], -100000.0f, 100000.0f);
      break;
    case PG_MOD_MULTIPLY:
      p[PG_P_MULTIPLY_DUPLICATIONS] = m2_int(p[PG_P_MULTIPLY_DUPLICATIONS], 1, 100);
      p[PG_P_MULTIPLY_DISTANCE] = m2_clamp(p[PG_P_MULTIPLY_DISTANCE], -10000.0f, 10000.0f);
      break;
    default:
      break;
  }
  /* curve block */
  float *c = &p[PG_P_CURVE_BASE];
  for (int i = 0; i < 16; i++) if (!isfinite(c[i])) c[i] = 0.0f;
  c[PG_P_CURVE_USE] = m2_flag(c[PG_P_CURVE_USE]);
  c[PG_P_CURVE_COUNT_PTS] = m2_int(c[PG_P_CURVE_COUNT_PTS], 0, PG_P_CURVE_MAX_PTS);
  for (int i = 0; i < 2 * PG_P_CURVE_MAX_PTS; i++) c[PG_P_CURVE_XY + i] = m2_clamp(c[PG_P_CURVE_XY + i], 0.0f, 1.0f);
  if (c[PG_P_CURVE_USE] != 0.0f && c[PG_P_CURVE_COUNT_PTS] < 2) { /* a curve needs two points: linear */
    c[PG_P_CURVE_COUNT_PTS] = 2;
    c[PG_P_CURVE_XY + 0] = 0; c[PG_P_CURVE_XY + 1] = 0; c[PG_P_CURVE_XY + 2] = 1; c[PG_P_CURVE_XY + 3] = 1;
  }
  /* filter block */
  float *f = &p[PG_P_FILTER_BASE];
  for (int i = 0; i < 8; i++) if (!isfinite(f[i])) f[i] = 0.0f;
  f[PG_P_FILTER_MATERIAL] = m2_int(f[PG_P_FILTER_MATERIAL], 0, 32767);
  f[PG_P_FILTER_PASS] = m2_int(f[PG_P_FILTER_PASS], 0, 100);
  f[PG_P_FILTER_LAYER_PASS] = m2_int(f[PG_P_FILTER_LAYER_PASS], 0, 100);
  f[PG_P_FILTER_VGROUP] = m2_int(f[PG_P_FILTER_VGROUP], 0, 256);
  f[PG_P_FILTER_INVERT_MATERIAL] = m2_flag(f[PG_P_FILTER_INVERT_MATERIAL]);
  f[PG_P_FILTER_INVERT_PASS] = m2_flag(f[PG_P_FILTER_INVERT_PASS]);
  f[PG_P_FILTER_INVERT_LAYER_PASS] = m2_flag(f[PG_P_FILTER_INVERT_LAYER_PASS]);
  f[PG_P_FILTER_INVERT_VGROUP] = m2_flag(f[PG_P_FILTER_INVERT_VGROUP]);
}

/* ---------------------------------------------------------------------------------------- */
/* Influence: is_stroke_affected_by_modifier() material / pass / layer-pass part, the vertex  */
/* group weight (get_modifier_point_weight) and the custom curve (curve_intensity).           */

int pg_mod_stroke_affected(const PGModContext *ctx, const PGModEntry *e, const bGPDstroke *gps)
{
  const float *f = &e->params[PG_P_FILTER_BASE];
  const int mat = (int)f[PG_P_FILTER_MATERIAL] - 1;
  if (mat >= 0) {
    const bool hit = gps->mat_nr == mat;
    if (hit == (f[PG_P_FILTER_INVERT_MATERIAL] != 0.0f)) return 0;
  }
  const int pass = (int)f[PG_P_FILTER_PASS];
  if (pass > 0) {
    int idx = 0;
    if (ctx->gpd->mat != NULL && gps->mat_nr >= 0 && gps->mat_nr < ctx->gpd->totcol &&
        ctx->gpd->mat[gps->mat_nr] != NULL && ctx->gpd->mat[gps->mat_nr]->gp_style != NULL)
      idx = ctx->gpd->mat[gps->mat_nr]->gp_style->index;
    if ((idx == pass) == (f[PG_P_FILTER_INVERT_PASS] != 0.0f)) return 0;
  }
  const int lpass = (int)f[PG_P_FILTER_LAYER_PASS];
  if (lpass > 0 && ctx->gpl != NULL) {
    if ((ctx->gpl->pass_index == lpass) == (f[PG_P_FILTER_INVERT_LAYER_PASS] != 0.0f)) return 0;
  }
  return 1;
}

int pg_mod_has_point_influence(const PGModEntry *e)
{
  return e->params[PG_P_FILTER_BASE + PG_P_FILTER_VGROUP] > 0.0f ||
         e->params[PG_P_CURVE_BASE + PG_P_CURVE_USE] != 0.0f;
}

static float m2_group_weight(const bGPDstroke *gps, int i, int def_nr, bool invert)
{
  /* get_modifier_point_weight() with a missing weight read as 0 (Blender skips such points; the
   * blend below then leaves them unchanged, which is the same result) */
  float w = 0.0f;
  if (gps->dvert != NULL) {
    const MDeformVert *dv = &gps->dvert[i];
    for (int k = 0; k < dv->totweight; k++) {
      if ((int)dv->dw[k].def_nr == def_nr) { w = dv->dw[k].weight; break; }
    }
  }
  return invert ? 1.0f - w : w;
}

float pg_mod_point_influence(const PGModEntry *e, const bGPDstroke *gps, int index)
{
  float w = 1.0f;
  const int vg = (int)e->params[PG_P_FILTER_BASE + PG_P_FILTER_VGROUP] - 1;
  if (vg >= 0) {
    w *= m2_group_weight(gps, index, vg, e->params[PG_P_FILTER_BASE + PG_P_FILTER_INVERT_VGROUP] != 0.0f);
  }
  const float *c = &e->params[PG_P_CURVE_BASE];
  if (c[PG_P_CURVE_USE] != 0.0f && gps->totpoints > 0) {
    PGCurve curve;
    if (pg_curve_set(&curve, &c[PG_P_CURVE_XY], (int)c[PG_P_CURVE_COUNT_PTS])) {
      const float t = gps->totpoints > 1 ? (float)index / (float)(gps->totpoints - 1) : 0.0f;
      w *= pg_curve_evaluate(&curve, t);
    }
  }
  return w;
}

/* ---------------------------------------------------------------------------------------- */
/* Time Offset                                                                               */

int pg_mod_time_frame(const PGModEntry *entries, int count, int cfra)
{
  int f = cfra;
  for (int i = 0; i < count && entries != NULL; i++) {
    const PGModEntry *e = &entries[i];
    if (!e->enabled || e->type != PG_MOD_TIME) continue;
    const float *p = e->params;
    f = pg_time_offset_frame((int)p[PG_P_TIME_MODE], f, (int)p[PG_P_TIME_OFFSET], p[PG_P_TIME_SCALE],
                             p[PG_P_TIME_USE_RANGE] != 0.0f, (int)p[PG_P_TIME_SFRA], (int)p[PG_P_TIME_EFRA],
                             p[PG_P_TIME_LOOP] != 0.0f);
  }
  return f;
}

/* ---------------------------------------------------------------------------------------- */
/* Envelope (MOD_gpencil_legacy_envelope.c)                                                  */

/* get_modifier_point_weight() of MOD_gpencil_legacy_util.c (the envelope calls it with def_nr -1:
 * no vertex group, weight 1). */
static float get_modifier_point_weight(MDeformVert *dvert, bool inverse, int def_nr)
{
  float weight = 1.0f;
  if ((dvert != NULL) && (def_nr != -1)) {
    MDeformWeight *dw = BKE_defvert_find_index(dvert, def_nr);
    weight = dw ? dw->weight : -1.0f;
    if ((weight >= 0.0f) && (inverse)) {
      return 1.0f - weight;
    }
    if ((weight < 0.0f) && (!inverse)) {
      return -1.0f;
    }
    if ((weight < 0.0f) && (inverse)) {
      return 1.0f;
    }
  }
  return weight;
}

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_envelope.c */
static float calc_min_radius_v3v3(float p1[3], float p2[3], float dir[3])
{
  /* Use plane-conic-intersections to choose the maximal radius.
   * The conic is defined in 4D as f({x,y,z,t}) = x*x + y*y + z*z - t*t = 0
   * Then a plane is defined parametrically as
   * {p}(u, v) = {p1,0}*u + {p2,0}*(1-u) + {dir,1}*v with 0 <= u <= 1 and v >= 0
   * Now compute the intersection point with the smallest t.
   * To do so, compute the parameters u, v such that f(p(u, v)) = 0 and v is minimal.
   * This can be done analytically and the solution is:
   * u = -dot(p2,dir) / dot(p1-p2, dir) +/- sqrt((dot(p2,dir) / dot(p1-p2, dir))^2 -
   * (2*dot(p1-p2,p2)*dot(p2,dir)-dot(p2,p2)*dot(p1-p2,dir))/(dot(p1-p2,dir)*dot(p1-p2,p1-p2)));
   * v = ({p1}u + {p2}*(1-u))^2 / (2*(dot(p1,dir)*u + dot(p2,dir)*(1-u)));
   */
  float diff[3];
  float p1_dir = dot_v3v3(p1, dir);
  float p2_dir = dot_v3v3(p2, dir);
  float p2_sqr = len_squared_v3(p2);
  float diff_dir = p1_dir - p2_dir;
  float u = 0.5f;
  if (diff_dir != 0.0f) {
    float p = p2_dir / diff_dir;
    sub_v3_v3v3(diff, p1, p2);
    float diff_sqr = len_squared_v3(diff);
    float diff_p2 = dot_v3v3(diff, p2);
    float q = (2 * diff_p2 * p2_dir - p2_sqr * diff_dir) / (diff_dir * diff_sqr);
    if (p * p - q >= 0) {
      u = -p - sqrtf(p * p - q) * copysign(1.0f, p);
      CLAMP(u, 0.0f, 1.0f);
    }
    else {
      u = 0.5f - copysign(0.5f, p);
    }
  }
  else {
    float p1_sqr = len_squared_v3(p1);
    u = p1_sqr < p2_sqr ? 1.0f : 0.0f;
  }
  float p[3];
  interp_v3_v3v3(p, p2, p1, u);
  /* v is the determined minimal radius. In case p1 and p2 are the same, there is a
   * simple proof for the following formula using the geometric mean theorem and Thales theorem. */
  float v = len_squared_v3(p) / (2 * interpf(p1_dir, p2_dir, u));
  if (v < 0 || !isfinite(v)) {
    /* No limit to the radius from this segment. */
    return 1e16f;
  }
  return v;
}

static float calc_radius_limit(
    bGPDstroke *gps, bGPDspoint *points, float dir[3], int spread, const int i)
{
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  bGPDspoint *pt = &points[i];

  /* NOTE this part is the second performance critical part. Improvements are welcome. */
  float radius_limit = 1e16f;
  float p1[3], p2[3];
  if (is_cyclic) {
    if (gps->totpoints / 2 < spread) {
      spread = gps->totpoints / 2;
    }
    const int start = i + gps->totpoints;
    for (int j = -spread; j <= spread; j++) {
      j += (j == 0);
      const int i1 = (start + j) % gps->totpoints;
      const int i2 = (start + j + (j > 0) - (j < 0)) % gps->totpoints;
      sub_v3_v3v3(p1, &points[i1].x, &pt->x);
      sub_v3_v3v3(p2, &points[i2].x, &pt->x);
      float r = calc_min_radius_v3v3(p1, p2, dir);
      radius_limit = min_ff(radius_limit, r);
    }
  }
  else {
    const int start = max_ii(-spread, 1 - i);
    const int end = min_ii(spread, gps->totpoints - 2 - i);
    for (int j = start; j <= end; j++) {
      if (j == 0) {
        continue;
      }
      const int i1 = i + j;
      const int i2 = i + j + (j > 0) - (j < 0);
      sub_v3_v3v3(p1, &points[i1].x, &pt->x);
      sub_v3_v3v3(p2, &points[i2].x, &pt->x);
      float r = calc_min_radius_v3v3(p1, p2, dir);
      radius_limit = min_ff(radius_limit, r);
    }
  }
  return radius_limit;
}

static void apply_stroke_envelope(bGPDstroke *gps,
                                  int spread,
                                  const int def_nr,
                                  const bool invert_vg,
                                  const float thickness,
                                  const float pixfactor)
{
  const bool is_cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  if (is_cyclic) {
    const int half = gps->totpoints / 2;
    spread = abs(((spread + half) % gps->totpoints) - half);
  }
  else {
    spread = min_ii(spread, gps->totpoints - 1);
  }

  const int spread_left = (spread + 2) / 2;
  const int spread_right = (spread + 1) / 2;

  /* Copy the point data. Only need positions, but extracting them
   * is probably just as expensive as a full copy. */
  bGPDspoint *old_points = (bGPDspoint *)MEM_dupallocN(gps->points);

  /* Deform the stroke to match the envelope shape. */
  for (int i = 0; i < gps->totpoints; i++) {
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    /* Verify in vertex group. */
    float weight = get_modifier_point_weight(dvert, invert_vg, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    int index1 = i - spread_left;
    int index2 = i + spread_right;
    CLAMP(index1, 0, gps->totpoints - 1);
    CLAMP(index2, 0, gps->totpoints - 1);

    bGPDspoint *point = &gps->points[i];
    point->pressure *= interpf(thickness, 1.0f, weight);

    float closest[3];
    float closest2[3];
    copy_v3_v3(closest2, &point->x);
    float dist = 0.0f;
    float dist2 = 0.0f;
    /* Create plane from point and neighbors and intersect that with the line. */
    float v1[3], v2[3], plane_no[3];
    sub_v3_v3v3(
        v1,
        &old_points[is_cyclic ? (i - 1 + gps->totpoints) % gps->totpoints : max_ii(0, i - 1)].x,
        &old_points[i].x);
    sub_v3_v3v3(
        v2,
        &old_points[is_cyclic ? (i + 1) % gps->totpoints : min_ii(gps->totpoints - 1, i + 1)].x,
        &old_points[i].x);
    normalize_v3(v1);
    normalize_v3(v2);
    sub_v3_v3v3(plane_no, v1, v2);
    if (normalize_v3(plane_no) == 0.0f) {
      continue;
    }
    /* Now find the intersections with the plane. */
    /* NOTE this part is the first performance critical part. Improvements are welcome. */
    float tmp_closest[3];
    for (int j = -spread_right; j <= spread_left; j++) {
      const int i1 = is_cyclic ? (i + j - spread_left + gps->totpoints) % gps->totpoints :
                                 max_ii(0, i + j - spread_left);
      const int i2 = is_cyclic ? (i + j + spread_right) % gps->totpoints :
                                 min_ii(gps->totpoints - 1, i + j + spread_right);
#if 0
      bool side = dot_v3v3(&old_points[i1].x, plane_no) < dot_v3v3(plane_no, &old_points[i2].x);
      if (side) {
        continue;
      }
#endif
      float lambda = line_plane_factor_v3(
          &point->x, plane_no, &old_points[i1].x, &old_points[i2].x);
      if (lambda <= 0.0f || lambda >= 1.0f) {
        continue;
      }
      interp_v3_v3v3(tmp_closest, &old_points[i1].x, &old_points[i2].x, lambda);

      float dir[3];
      sub_v3_v3v3(dir, tmp_closest, &point->x);
      float d = len_v3(dir);
      /* Use a formula to find the diameter of the circle that would touch the line. */
      float cos_angle = fabsf(dot_v3v3(plane_no, &old_points[i1].x) -
                              dot_v3v3(plane_no, &old_points[i2].x)) /
                        len_v3v3(&old_points[i1].x, &old_points[i2].x);
      d *= 2 * cos_angle / (1 + cos_angle);
      float to_closest[3];
      sub_v3_v3v3(to_closest, closest, &point->x);
      if (dist == 0.0f) {
        dist = d;
        copy_v3_v3(closest, tmp_closest);
      }
      else if (dot_v3v3(to_closest, dir) >= 0) {
        if (d > dist) {
          dist = d;
          copy_v3_v3(closest, tmp_closest);
        }
      }
      else {
        if (d > dist2) {
          dist2 = d;
          copy_v3_v3(closest2, tmp_closest);
        }
      }
    }
    if (dist == 0.0f) {
      copy_v3_v3(closest, &point->x);
    }
    if (dist2 == 0.0f) {
      copy_v3_v3(closest2, &point->x);
    }
    dist = dist + dist2;

    if (dist < FLT_EPSILON) {
      continue;
    }

    float use_dist = dist;

    /* Apply radius limiting to not cross existing lines. */
    float dir[3], new_center[3];
    interp_v3_v3v3(new_center, closest2, closest, 0.5f);
    sub_v3_v3v3(dir, new_center, &point->x);
    if (normalize_v3(dir) != 0.0f && (is_cyclic || (i > 0 && i < gps->totpoints - 1))) {
      const float max_radius = calc_radius_limit(gps, old_points, dir, spread, i);
      use_dist = min_ff(use_dist, 2 * max_radius);
    }

    float fac = use_dist * weight;
    point->pressure += fac * pixfactor;
    interp_v3_v3v3(&point->x, &point->x, new_center, fac / len_v3v3(closest, closest2));
  }

  MEM_freeN(old_points);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_envelope.c */
static void add_stroke(Object *ob,
                       bGPDstroke *gps,
                       const int point_index,
                       const int connection_index,
                       const int size2,
                       const int size1,
                       const int mat_nr,
                       const float thickness,
                       const float strength,
                       ListBase *results)
{
  const int size = size1 + size2;
  bGPdata *gpd = ob->data;
  bGPDstroke *gps_dst = BKE_gpencil_stroke_new(mat_nr, size, gps->thickness);
  gps_dst->runtime.gps_orig = gps->runtime.gps_orig;

  memcpy(&gps_dst->points[0], &gps->points[connection_index], size1 * sizeof(bGPDspoint));
  memcpy(&gps_dst->points[size1], &gps->points[point_index], size2 * sizeof(bGPDspoint));

  for (int i = 0; i < size; i++) {
    gps_dst->points[i].pressure *= thickness;
    gps_dst->points[i].strength *= strength;
  }

  if (gps->dvert != NULL) {
    gps_dst->dvert = MEM_malloc_arrayN(size, sizeof(MDeformVert), __func__);
    BKE_defvert_array_copy(&gps_dst->dvert[0], &gps->dvert[connection_index], size1);
    BKE_defvert_array_copy(&gps_dst->dvert[size1], &gps->dvert[point_index], size2);
  }

  BLI_addtail(results, gps_dst);

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps_dst);
}

static void add_stroke_cyclic(Object *ob,
                              bGPDstroke *gps,
                              const int point_index,
                              const int connection_index,
                              const int size,
                              const int mat_nr,
                              const float thickness,
                              const float strength,
                              ListBase *results)
{
  bGPdata *gpd = ob->data;
  bGPDstroke *gps_dst = BKE_gpencil_stroke_new(mat_nr, size * 2, gps->thickness);
  gps_dst->runtime.gps_orig = gps->runtime.gps_orig;

  if (gps->dvert != NULL) {
    gps_dst->dvert = MEM_malloc_arrayN(size * 2, sizeof(MDeformVert), __func__);
  }

  for (int i = 0; i < size; i++) {
    int a = (connection_index + i) % gps->totpoints;
    int b = (point_index + i) % gps->totpoints;

    gps_dst->points[i] = gps->points[a];
    bGPDspoint *pt_dst = &gps_dst->points[i];
    bGPDspoint *pt_orig = &gps->points[a];
    pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
    pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

    gps_dst->points[size + i] = gps->points[b];
    pt_dst = &gps_dst->points[size + i];
    pt_orig = &gps->points[b];
    pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
    pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

    if (gps->dvert != NULL) {
      BKE_defvert_array_copy(&gps_dst->dvert[i], &gps->dvert[a], 1);
      BKE_defvert_array_copy(&gps_dst->dvert[size + i], &gps->dvert[b], 1);
    }
  }
  for (int i = 0; i < size * 2; i++) {
    gps_dst->points[i].pressure *= thickness;
    gps_dst->points[i].strength *= strength;
    memset(&gps_dst->points[i].runtime, 0, sizeof(bGPDspoint_Runtime));
  }

  BLI_addtail(results, gps_dst);

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps_dst);
}

static void add_stroke_simple(Object *ob,
                              bGPDstroke *gps,
                              const int point_index,
                              const int connection_index,
                              const int mat_nr,
                              const float thickness,
                              const float strength,
                              ListBase *results)
{
  bGPdata *gpd = ob->data;
  bGPDstroke *gps_dst = BKE_gpencil_stroke_new(mat_nr, 2, gps->thickness);
  gps_dst->runtime.gps_orig = gps->runtime.gps_orig;

  gps_dst->points[0] = gps->points[connection_index];
  gps_dst->points[0].pressure *= thickness;
  gps_dst->points[0].strength *= strength;
  bGPDspoint *pt_dst = &gps_dst->points[0];
  bGPDspoint *pt_orig = &gps->points[connection_index];
  pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
  pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

  gps_dst->points[1] = gps->points[point_index];
  gps_dst->points[1].pressure *= thickness;
  gps_dst->points[1].strength *= strength;
  pt_dst = &gps_dst->points[1];
  pt_orig = &gps->points[point_index];
  pt_dst->runtime.pt_orig = pt_orig->runtime.pt_orig;
  pt_dst->runtime.idx_orig = pt_orig->runtime.idx_orig;

  if (gps->dvert != NULL) {
    gps_dst->dvert = MEM_malloc_arrayN(2, sizeof(MDeformVert), __func__);
    BKE_defvert_array_copy(&gps_dst->dvert[0], &gps->dvert[connection_index], 1);
    BKE_defvert_array_copy(&gps_dst->dvert[1], &gps->dvert[point_index], 1);
  }

  BLI_addtail(results, gps_dst);

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps_dst);
}
/* END VERBATIM */

/* ADAPTED deformStroke(): no Object / layer / material filter arguments (the entry's filter block
 * is checked by the caller); pixfactor uses the document's pixfactor and the layer's line change as
 * Blender does. */
static int m2_envelope_deform(const PGModContext *ctx, const float *p, bGPDstroke *gps)
{
  const int spread = (int)p[PG_P_ENVELOPE_SPREAD];
  if (spread <= 0 || gps->totpoints < 2) return 0;
  const float line_change = ctx->gpl != NULL ? (float)ctx->gpl->line_change : 0.0f;
  const float thick = (float)gps->thickness + line_change;
  /* ADAPTED: Blender passes 1000 / ((thickness + line_change) * pixfactor) to turn metres into
   * pressure; canvas units are pixels, so a point moved d px gains d / thickness of pressure. */
  apply_stroke_envelope(gps, spread, -1, false, p[PG_P_ENVELOPE_THICKNESS], 1.0f / (thick > 1.0f ? thick : 1.0f));
  return 1;
}

/* ADAPTED generate_geometry(): Object replaced by a stand-in holding the document (add_stroke reads
 * ob->data), ob->totcol by gpd->totcol, the layer/material filters by the entry's filter block. */
static int m2_envelope_generate(const PGModContext *ctx, const PGModEntry *e)
{
  const float *p = e->params;
  const int mode = (int)p[PG_P_ENVELOPE_MODE];
  const int spread = (int)p[PG_P_ENVELOPE_SPREAD];
  const int skip_p = (int)p[PG_P_ENVELOPE_SKIP];
  const float thickness = p[PG_P_ENVELOPE_THICKNESS];
  const float strength = p[PG_P_ENVELOPE_STRENGTH];
  if (spread <= 0) return 0;
  static Object ob_stub;
  Object *ob = &ob_stub;
  ob->data = ctx->gpd;
  bGPDframe *gpf = ctx->gpf;
  ListBase duplicates = {0};
  int any = 0;
  LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
    if (!pg_mod_stroke_affected(ctx, e, gps) || gps->totpoints < 2) continue;
    const int totcol = ctx->gpd->totcol > 0 ? ctx->gpd->totcol : 1;
    const int mat_nr = p[PG_P_ENVELOPE_MAT] < 0 ? gps->mat_nr : min_ii((int)p[PG_P_ENVELOPE_MAT], totcol - 1);
    if (mode == GP_ENVELOPE_FILLS) {
      const int skip = min_ii(skip_p, min_ii(spread / 2, gps->totpoints - 2));
      if (gps->flag & GP_STROKE_CYCLIC) {
        for (int i = 0; i < gps->totpoints; i++) {
          const int connection_index = (i + spread - skip) % gps->totpoints;
          add_stroke_cyclic(ob, gps, i, connection_index, 2 + skip, mat_nr, thickness, strength, &duplicates);
          i += skip_p;
        }
      }
      else {
        for (int i = -spread + skip; i < gps->totpoints - 1; i++) {
          const int point_index = max_ii(0, i);
          const int connection_index = min_ii(i + spread + 1, gps->totpoints - 1);
          const int size1 = min_ii(2 + skip, min_ii(point_index + 1, gps->totpoints - point_index));
          const int size2 = min_ii(2 + skip, min_ii(connection_index + 1, gps->totpoints - connection_index));
          add_stroke(ob, gps, point_index, connection_index + 1 - size2, size1, size2, mat_nr,
                     thickness, strength, &duplicates);
          i += skip_p;
        }
      }
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
    }
    else {
      if (gps->flag & GP_STROKE_CYCLIC) {
        for (int i = 0; i < gps->totpoints; i++) {
          const int connection_index = (i + 1 + spread) % gps->totpoints;
          add_stroke_simple(ob, gps, i, connection_index, mat_nr, thickness, strength, &duplicates);
          i += skip_p;
        }
      }
      else {
        for (int i = -spread; i < gps->totpoints - 1; i++) {
          const int connection_index = min_ii(i + 1 + spread, gps->totpoints - 1);
          add_stroke_simple(ob, gps, max_ii(0, i), connection_index, mat_nr, thickness, strength, &duplicates);
          i += skip_p;
        }
      }
    }
    any = 1;
  }
  if (!BLI_listbase_is_empty(&duplicates)) {
    /* new lines under the original line, as Blender */
    BLI_movelisttolist_reverse(&gpf->strokes, &duplicates);
  }
  return any;
}

/* ---------------------------------------------------------------------------------------- */
/* Build (pg_build_visible on the evaluated frame)                                           */

static int m2_build(const PGModContext *ctx, const PGModEntry *e)
{
  const float *p = e->params;
  int n = 0;
  LISTBASE_FOREACH (bGPDstroke *, gps, &ctx->gpf->strokes) {
    if (pg_mod_stroke_affected(ctx, e, gps)) n++;
  }
  if (n == 0) return 0;
  int *tot = MEM_malloc_arrayN((size_t)n, sizeof(int), "pg_build_tot");
  int *vis = MEM_malloc_arrayN((size_t)n, sizeof(int), "pg_build_vis");
  bGPDstroke **list = MEM_malloc_arrayN((size_t)n, sizeof(bGPDstroke *), "pg_build_list");
  int k = 0;
  LISTBASE_FOREACH (bGPDstroke *, gps, &ctx->gpf->strokes) {
    if (pg_mod_stroke_affected(ctx, e, gps)) { list[k] = gps; tot[k] = gps->totpoints; k++; }
  }
  /* frames are counted from the keyframe, as Blender's build counts from gpf->framenum */
  const float rel = (float)(ctx->cfra - ctx->gpf->framenum);
  pg_build_visible(tot, n, (int)p[PG_P_BUILD_MODE], (int)p[PG_P_BUILD_TRANSITION], rel,
                   p[PG_P_BUILD_START], p[PG_P_BUILD_LENGTH], vis);
  int any = 0;
  for (int i = 0; i < n; i++) {
    bGPDstroke *gps = list[i];
    if (vis[i] >= tot[i]) continue;
    any = 1;
    if (vis[i] <= 0) {
      BLI_remlink(&ctx->gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
    }
    else {
      BKE_gpencil_stroke_trim_points(gps, 0, vis[i] - 1, false);
    }
  }
  MEM_freeN(tot);
  MEM_freeN(vis);
  MEM_freeN(list);
  return any;
}

/* ---------------------------------------------------------------------------------------- */
/* Generators: the baked operators run on the evaluated frame through a one-layer view.      */

typedef struct M2Sel { bGPDstroke *gps; short flag; } M2Sel;

static int m2_generator(const PGModContext *ctx, const PGModEntry *e)
{
  bGPdata view = *ctx->gpd;
  view.flag &= ~GP_DATA_STROKE_MULTIEDIT;
  BLI_listbase_clear(&view.layers);
  bGPDlayer layer;
  memset(&layer, 0, sizeof(layer));
  if (ctx->gpl != NULL) layer.flag = ctx->gpl->flag & GP_LAYER_UNLOCK_COLOR;
  layer.actframe = ctx->gpf;
  layer.frames.first = layer.frames.last = ctx->gpf;
  BLI_addtail(&view.layers, &layer);
  /* the operators act on selected strokes: select the affected ones, restore afterwards */
  int n = BLI_listbase_count(&ctx->gpf->strokes);
  M2Sel *saved = n > 0 ? MEM_malloc_arrayN((size_t)n, sizeof(M2Sel), "pg_gen_sel") : NULL;
  int k = 0;
  LISTBASE_FOREACH (bGPDstroke *, gps, &ctx->gpf->strokes) {
    saved[k].gps = gps;
    saved[k].flag = gps->flag;
    k++;
    if (pg_mod_stroke_affected(ctx, e, gps)) gps->flag |= GP_STROKE_SELECT;
    else gps->flag &= ~GP_STROKE_SELECT;
  }
  const float *p = e->params;
  int changed = 0;
  switch (e->type) {
    case PG_MOD_DASH:
      changed = pg_gp_dash(&view, &layer, (int)p[PG_P_DASH_DASH], (int)p[PG_P_DASH_GAP], (int)p[PG_P_DASH_OFFSET]);
      break;
    case PG_MOD_OUTLINE:
      changed = pg_gp_outline(&view, &layer, (int)p[PG_P_OUTLINE_THICKNESS], (int)p[PG_P_OUTLINE_SUBDIV]);
      break;
    case PG_MOD_MIRROR:
      changed = pg_gp_mirror_copy(&view, &layer, p[PG_P_MIRROR_X] != 0.0f, p[PG_P_MIRROR_Y] != 0.0f,
                                  p[PG_P_MIRROR_PX], p[PG_P_MIRROR_PY]);
      break;
    case PG_MOD_ARRAY:
      changed = pg_gp_array(&view, &layer, (int)p[PG_P_ARRAY_COUNT_N], p[PG_P_ARRAY_OX], p[PG_P_ARRAY_OY]);
      break;
    case PG_MOD_MULTIPLY:
      changed = pg_gp_multiply(&view, &layer, (int)p[PG_P_MULTIPLY_DUPLICATIONS], p[PG_P_MULTIPLY_DISTANCE]);
      break;
    default:
      break;
  }
  /* survivors get their selection back, generated strokes are unselected */
  LISTBASE_FOREACH (bGPDstroke *, gps, &ctx->gpf->strokes) {
    short flag = -1;
    for (int i = 0; i < n; i++) if (saved[i].gps == gps) { flag = saved[i].flag; break; }
    if (flag >= 0) gps->flag = (gps->flag & ~GP_STROKE_SELECT) | (flag & GP_STROKE_SELECT);
    else gps->flag &= ~GP_STROKE_SELECT;
  }
  if (saved != NULL) MEM_freeN(saved);
  return changed;
}

/* ---------------------------------------------------------------------------------------- */
/* Vertex weight modifiers: write the target group of the evaluated copies.                 */

static MDeformWeight *m2_weight_ensure(MDeformVert *dv, int def_nr)
{
  for (int i = 0; i < dv->totweight; i++) {
    if ((int)dv->dw[i].def_nr == def_nr) return &dv->dw[i];
  }
  MDeformWeight *dw = MEM_callocN(sizeof(MDeformWeight) * (size_t)(dv->totweight + 1), "pg_m2_defvert");
  if (dw == NULL) return NULL;
  if (dv->dw != NULL) {
    memcpy(dw, dv->dw, sizeof(MDeformWeight) * (size_t)dv->totweight);
    MEM_freeN(dv->dw);
  }
  dv->dw = dw;
  dv->dw[dv->totweight].def_nr = (unsigned int)def_nr;
  dv->dw[dv->totweight].weight = 0.0f;
  return &dv->dw[dv->totweight++];
}

static void m2_assign_weight(MDeformVert *dv, int target, float w, const float *p, int min_i, int mul_i)
{
  MDeformWeight *dw = m2_weight_ensure(dv, target);
  if (dw == NULL) return;
  dw->weight = (p[mul_i] != 0.0f) ? dw->weight * w : w;
  CLAMP(dw->weight, p[min_i], 1.0f);
}

static int m2_weight_modifier(const PGModContext *ctx, const PGModEntry *e, bGPDstroke *gps)
{
  const float *p = e->params;
  const int target = e->type == PG_MOD_WEIGHT_PROX ? (int)p[PG_P_WPROX_TARGET] : (int)p[PG_P_WANGLE_TARGET];
  if (target < 0 || target >= BLI_listbase_count(&ctx->gpd->vertex_group_names)) return 0; /* target_def_nr == -1 */
  if (gps->dvert == NULL) {
    gps->dvert = MEM_callocN(sizeof(MDeformVert) * (size_t)gps->totpoints, "pg_m2_dvert"); /* BKE_gpencil_dvert_ensure */
    if (gps->dvert == NULL) return 0;
  }
  if (e->type == PG_MOD_WEIGHT_PROX) {
    /* calc_point_weight_by_distance with min / max of start and end (pg_weight_proximity) */
    const float ds = p[PG_P_WPROX_DIST_START], de = p[PG_P_WPROX_DIST_END];
    for (int i = 0; i < gps->totpoints; i++) {
      float w = pg_weight_proximity(gps->points[i].x, gps->points[i].y, p[PG_P_WPROX_X], p[PG_P_WPROX_Y],
                                    MIN2(ds, de), MAX2(ds, de), 0);
      if (p[PG_P_WPROX_INVERT] != 0.0f) w = 1.0f - w; /* GP_WEIGHT_INVERT_OUTPUT */
      m2_assign_weight(&gps->dvert[i], target, w, p, PG_P_WPROX_MIN_WEIGHT, PG_P_WPROX_MULTIPLY);
    }
    return 1;
  }
  /* Weight Angle: 1 - sin(angle between the segment and the reference direction) about the view
   * axis (Blender: axis Z for a front view); canvas y points down, so the angle is mirrored. */
  float weight_pt = 1.0f;
  const float ref[2] = {cosf(p[PG_P_WANGLE_ANGLE]), sinf(p[PG_P_WANGLE_ANGLE])};
  for (int i = 0; i < gps->totpoints; i++) {
    if (gps->totpoints == 1) { weight_pt = 1.0f; break; }
    const bGPDspoint *pt1 = (i > 0) ? &gps->points[i] : &gps->points[i + 1];
    const bGPDspoint *pt2 = (i > 0) ? &gps->points[i - 1] : &gps->points[i];
    float vx = pt1->x - pt2->x, vy = -(pt1->y - pt2->y);
    const float len = hypotf(vx, vy);
    float angle = 0.0f;
    if (len > 1e-8f) {
      const float c = m2_clamp((vx * ref[0] + vy * ref[1]) / len, -1.0f, 1.0f);
      angle = acosf(c);
    }
    weight_pt = 1.0f - sinf(angle);
    if (p[PG_P_WANGLE_INVERT] != 0.0f) weight_pt = 1.0f - weight_pt;
    m2_assign_weight(&gps->dvert[i], target, weight_pt, p, PG_P_WANGLE_MIN_WEIGHT, PG_P_WANGLE_MULTIPLY);
  }
  return 1;
}

/* ---------------------------------------------------------------------------------------- */
/* Dispatch                                                                                  */

int pg_mod2_is_frame_level(int type)
{
  switch (type) {
    case PG_MOD_BUILD: case PG_MOD_TIME: case PG_MOD_ENVELOPE: case PG_MOD_DASH: case PG_MOD_OUTLINE:
    case PG_MOD_MIRROR: case PG_MOD_ARRAY: case PG_MOD_MULTIPLY:
      return 1;
    default:
      return 0;
  }
}

int pg_mod2_run_frame(const PGModContext *ctx, const PGModEntry *e)
{
  switch (e->type) {
    case PG_MOD_BUILD: return m2_build(ctx, e);
    case PG_MOD_TIME: return 0; /* the backend picks the frame (pg_mod_time_frame) */
    case PG_MOD_ENVELOPE:
      if ((int)e->params[PG_P_ENVELOPE_MODE] == GP_ENVELOPE_DEFORM) {
        int any = 0;
        LISTBASE_FOREACH (bGPDstroke *, gps, &ctx->gpf->strokes) {
          if (pg_mod_stroke_affected(ctx, e, gps)) any |= pg_mod2_deform_stroke(ctx, e, gps);
        }
        return any;
      }
      return m2_envelope_generate(ctx, e);
    case PG_MOD_DASH: case PG_MOD_OUTLINE: case PG_MOD_MIRROR: case PG_MOD_ARRAY: case PG_MOD_MULTIPLY:
      return m2_generator(ctx, e);
    default:
      return 0;
  }
}

int pg_mod2_deform_stroke(const PGModContext *ctx, const PGModEntry *e, bGPDstroke *gps)
{
  const float *p = e->params;
  switch (e->type) {
    case PG_MOD_HOOK:
      return pg_hook_deform(gps, p[PG_P_HOOK_CX], p[PG_P_HOOK_CY], p[PG_P_HOOK_DX], p[PG_P_HOOK_DY],
                            p[PG_P_HOOK_ANGLE], p[PG_P_HOOK_SCALE], p[PG_P_HOOK_RADIUS],
                            (int)p[PG_P_HOOK_FALLOFF], p[PG_P_HOOK_STRENGTH]) > 0;
    case PG_MOD_LATTICE:
      return pg_lattice_deform(gps, p[PG_P_LATTICE_X0], p[PG_P_LATTICE_Y0], p[PG_P_LATTICE_X1],
                               p[PG_P_LATTICE_Y1], (int)p[PG_P_LATTICE_NU], (int)p[PG_P_LATTICE_NV],
                               &p[PG_P_LATTICE_OFFSETS], p[PG_P_LATTICE_STRENGTH]) > 0;
    case PG_MOD_ENVELOPE:
      return (int)p[PG_P_ENVELOPE_MODE] == GP_ENVELOPE_DEFORM ? m2_envelope_deform(ctx, p, gps) : 0;
    case PG_MOD_WEIGHT_PROX:
    case PG_MOD_WEIGHT_ANGLE:
      return m2_weight_modifier(ctx, e, gps);
    default:
      return 0;
  }
}
