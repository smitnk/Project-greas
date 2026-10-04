/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Second batch of Legacy GP operators on the selection. pg_gp_modifier_point_weight() mirrors
 * get_modifier_point_weight() of MOD_gpencil_legacy_util.c; the operators follow the behaviour of
 * the named 3.6.23 operators using BKE functions where Blender has them.
 */

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit2.h"
#include "project_grease_blender_edit3.h"

static float pe2_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static MaterialGPencilStyle pe2_default_style;
static MaterialGPencilStyle *pe2_style(const bGPdata *gpd, int mat_nr)
{
  if (gpd->mat != NULL && mat_nr >= 0 && mat_nr < (int)gpd->totcol && gpd->mat[mat_nr] != NULL &&
      gpd->mat[mat_nr]->gp_style != NULL)
  {
    return gpd->mat[mat_nr]->gp_style;
  }
  return &pe2_default_style;
}

static bool pe2_stroke_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  const MaterialGPencilStyle *st = pe2_style(gpd, gps->mat_nr);
  if (st->flag & GP_MATERIAL_HIDE) return false;
  if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (st->flag & GP_MATERIAL_LOCKED)) return false;
  return true;
}

/* Same traversal as GP_EDITABLE_STROKES_BEGIN (active frame, or selected frames in multi-edit). */
#define PE2_STROKES_BEGIN(gpd_, only_, gpl, gpf, gps) \
  { \
    const bool multi_ = ((gpd_)->flag & GP_DATA_STROKE_MULTIEDIT) != 0; \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd_)->layers) { \
      if (((only_) != NULL && gpl != (only_)) || !BKE_gpencil_layer_is_editable(gpl)) continue; \
      for (bGPDframe *gpf = multi_ ? (bGPDframe *)gpl->frames.first : gpl->actframe; gpf; gpf = gpf->next) { \
        if (!((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && multi_))) continue; \
        bGPDstroke *next_; \
        for (bGPDstroke *gps = (bGPDstroke *)gpf->strokes.first; gps; gps = next_) { \
          next_ = gps->next; \
          if (!pe2_stroke_editable((gpd_), gpl, gps)) continue;
#define PE2_STROKES_END \
        } \
        if (!multi_) break; \
      } \
    } \
  } \
  (void)0

/* get_modifier_point_weight() */
float pg_gp_modifier_point_weight(const MDeformVert *dvert, int inverse, int def_nr)
{
  float weight = 1.0f;
  if ((dvert != NULL) && (def_nr != -1)) {
    const MDeformWeight *dw = NULL;
    for (int i = 0; i < dvert->totweight; i++) { /* BKE_defvert_find_index() */
      if ((int)dvert->dw[i].def_nr == def_nr) { dw = &dvert->dw[i]; break; }
    }
    weight = dw ? dw->weight : -1.0f;
    if ((weight >= 0.0f) && (inverse == 1)) return -1.0f;
    if ((weight < 0.0f) && (inverse == 0)) return -1.0f;
    /* if inverse, weight is always 1 */
    if ((weight < 0.0f) && (inverse == 1)) return 1.0f;
  }
  /* handle special empty groups */
  if ((dvert == NULL) && (def_nr != -1)) {
    if (inverse == 1) return 1.0f;
    return -1.0f;
  }
  return weight;
}

int pg_gp_mod_thickness_vgroup(bGPdata *gpd, const bGPDlayer *only_layer, int def_nr, int invert,
                               int normalize, int thickness, float thickness_fac)
{
  if (gpd == NULL || def_nr < -1 || !isfinite(thickness_fac)) return 0;
  int changed = 0;
  PE2_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) continue;
    const float stroke_thickness_inv = 1.0f / (float)(gps->thickness > 1 ? gps->thickness : 1);
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      const MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
      const float weight = pg_gp_modifier_point_weight(dvert, invert != 0, def_nr);
      if (weight < 0.0f) continue;
      const float target = normalize ? (float)thickness * stroke_thickness_inv : pt->pressure * thickness_fac;
      const float before = pt->pressure;
      pt->pressure = (weight * target) + ((1.0f - weight) * pt->pressure); /* interpf() */
      if (pt->pressure < 0.0f) pt->pressure = 0.0f;
      changed |= (pt->pressure != before);
    }
  }
  PE2_STROKES_END;
  return changed;
}

int pg_gp_select_vertex_color(bGPdata *gpd, const bGPDlayer *only_layer, const float rgb[3],
                              float threshold, int extend)
{
  if (gpd == NULL || rgb == NULL || !isfinite(threshold) || threshold < 0.0f) return 0;
  float hsv_ref[3];
  pg_rgb_to_hsv(rgb, hsv_ref);
  int changed = 0;
  PE2_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (gps->points == NULL) continue;
    bool any = false;
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      const int was = pt->flag & GP_SPOINT_SELECT;
      bool hit = false;
      if (pt->vert_color[3] > 0.0f) {
        float hsv[3];
        pg_rgb_to_hsv(pt->vert_color, hsv);
        float d = fabsf(hsv[0] - hsv_ref[0]);
        if (d > 0.5f) d = 1.0f - d; /* hue is circular */
        hit = d <= threshold;
      }
      if (hit) pt->flag |= GP_SPOINT_SELECT;
      else if (!extend) pt->flag &= ~GP_SPOINT_SELECT;
      if ((pt->flag & GP_SPOINT_SELECT) != was) changed = 1;
      any |= (pt->flag & GP_SPOINT_SELECT) != 0;
    }
    if (any) gps->flag |= GP_STROKE_SELECT;
    else gps->flag &= ~GP_STROKE_SELECT;
  }
  PE2_STROKES_END;
  return changed;
}

int pg_gp_stroke_normalize(bGPdata *gpd, const bGPDlayer *only_layer, int mode, float value)
{
  if (gpd == NULL || (mode != 0 && mode != 1) || !isfinite(value)) return 0;
  int changed = 0;
  PE2_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) continue;
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      if (!(pt->flag & GP_SPOINT_SELECT)) continue;
      float *field = mode == 0 ? &pt->pressure : &pt->strength;
      const float v = mode == 0 ? (value < 0.0f ? 0.0f : value) : pe2_clampf(value, 0.0f, 1.0f);
      if (*field != v) { *field = v; changed = 1; }
    }
  }
  PE2_STROKES_END;
  return changed;
}

int pg_gp_stroke_simplify_fixed(bGPdata *gpd, const bGPDlayer *only_layer, int steps)
{
  if (gpd == NULL || steps < 1 || steps > 100) return 0;
  int changed = 0;
  PE2_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->totpoints < 3) continue;
    const int before = gps->totpoints;
    for (int s = 0; s < steps; s++) BKE_gpencil_stroke_simplify_fixed(gpd, gps);
    if (gps->totpoints != before) changed = 1;
  }
  PE2_STROKES_END;
  return changed;
}

int pg_gp_stroke_sample(bGPdata *gpd, const bGPDlayer *only_layer, float length, float sharp_threshold)
{
  if (gpd == NULL || !isfinite(length) || length <= 0.0f || !isfinite(sharp_threshold)) return 0;
  int changed = 0;
  PE2_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->totpoints < 2) continue;
    /* BKE_gpencil_stroke_sample allocates stroke length / length points: past the ceiling the
     * stroke is left as it is. */
    const double estimate = (double)BKE_gpencil_stroke_length(gps, false) / (double)length + gps->totpoints;
    if (!(estimate <= PG_MAX_STROKE_POINTS)) continue;
    /* stroke_march_next_point_no_interp() steps by `length` from point to point: where the spacing
     * is below the float resolution at the stroke's coordinates a step does not move the point and
     * the march never ends (fuzzer: strokes moved to ~1e9, where a float step is 64 units). */
    float extent = 0.0f;
    for (int i = 0; i < gps->totpoints; i++) {
      extent = fmaxf(extent, fmaxf(fabsf(gps->points[i].x), fmaxf(fabsf(gps->points[i].y), fabsf(gps->points[i].z))));
    }
    if (!(length > extent * 8.0f * FLT_EPSILON)) continue;
    if (BKE_gpencil_stroke_sample(gpd, gps, length, true, sharp_threshold)) changed = 1;
  }
  PE2_STROKES_END;
  return changed;
}

/* Add a copy of point `src` at index `at` (0 = before the first point, totpoints = after the last). */
static bool pe2_insert_point(bGPDstroke *gps, int src, int at)
{
  const int n = gps->totpoints;
  bGPDspoint *pts = MEM_callocN(sizeof(bGPDspoint) * (size_t)(n + 1), "pg_extrude_points");
  if (pts == NULL) return false;
  memcpy(pts, gps->points, sizeof(bGPDspoint) * (size_t)at);
  pts[at] = gps->points[src];
  memcpy(pts + at + 1, gps->points + at, sizeof(bGPDspoint) * (size_t)(n - at));
  if (gps->dvert != NULL) {
    MDeformVert *dv = MEM_callocN(sizeof(MDeformVert) * (size_t)(n + 1), "pg_extrude_dvert");
    if (dv == NULL) { MEM_freeN(pts); return false; }
    memcpy(dv, gps->dvert, sizeof(MDeformVert) * (size_t)at);
    memcpy(dv + at + 1, gps->dvert + at, sizeof(MDeformVert) * (size_t)(n - at)); /* new point: no weights */
    MEM_freeN(gps->dvert);
    gps->dvert = dv;
  }
  MEM_freeN(gps->points);
  gps->points = pts;
  gps->totpoints = n + 1;
  return true;
}

int pg_gp_extrude(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) return 0;
  int changed = 0;
  PE2_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL || gps->totpoints < 1) continue;
    if (gps->flag & GP_STROKE_CYCLIC) continue; /* closed strokes have no ends */
    const bool last_sel = (gps->points[gps->totpoints - 1].flag & GP_SPOINT_SELECT) != 0;
    const bool first_sel = gps->totpoints > 1 && (gps->points[0].flag & GP_SPOINT_SELECT) != 0;
    bool stroke_changed = false;
    if (last_sel && pe2_insert_point(gps, gps->totpoints - 1, gps->totpoints)) {
      gps->points[gps->totpoints - 2].flag &= ~GP_SPOINT_SELECT; /* old end deselected */
      stroke_changed = true;
    }
    if (first_sel && pe2_insert_point(gps, 0, 0)) {
      gps->points[1].flag &= ~GP_SPOINT_SELECT;
      stroke_changed = true;
    }
    if (stroke_changed) {
      BKE_gpencil_stroke_geometry_update(gpd, gps);
      changed = 1;
    }
  }
  PE2_STROKES_END;
  return changed;
}

int pg_gp_edit2_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args,
                         int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) {
    if (!isfinite(args[i])) return 0;
  }
  const bGPDlayer *scope = active_layer;
  int changed = 0;
  switch (command) {
    case PG_EDIT2_CMD_MOD_THICKNESS_VGROUP:
      if (arg_count < 5) return 0;
      changed = pg_gp_mod_thickness_vgroup(gpd, scope, (int)lroundf(args[0]), args[1] != 0.0f,
                                           args[2] != 0.0f, (int)lroundf(args[3]), args[4]);
      break;
    case PG_EDIT2_CMD_SELECT_VCOLOR: {
      if (arg_count < 5) return 0;
      const float rgb[3] = {args[0], args[1], args[2]};
      changed = pg_gp_select_vertex_color(gpd, scope, rgb, args[3], args[4] != 0.0f);
      break;
    }
    case PG_EDIT2_CMD_NORMALIZE:
      if (arg_count < 2) return 0;
      changed = pg_gp_stroke_normalize(gpd, scope, (int)lroundf(args[0]), args[1]);
      break;
    case PG_EDIT2_CMD_SIMPLIFY_FIXED:
      if (arg_count < 1) return 0;
      changed = pg_gp_stroke_simplify_fixed(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT2_CMD_SAMPLE:
      if (arg_count < 2) return 0;
      changed = pg_gp_stroke_sample(gpd, scope, args[0], args[1]);
      break;
    case PG_EDIT2_CMD_EXTRUDE:
      changed = pg_gp_extrude(gpd, scope);
      break;
    default:
      return pg_gp_edit3_dispatch(gpd, active_layer, command, args, arg_count);
  }
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed ? 1 : 0;
}
