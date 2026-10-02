/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Selection-aware editing on the Blender 3.6.23 Legacy GP data model.
 *
 * Click selection follows gpencil_select_exec() (editors/gpencil_legacy/gpencil_select.c); the
 * parts that do not touch bContext/RNA/Object are byte-identical copies (BEGIN/END VERBATIM,
 * checked in CI against the pinned tree). It uses the same nearest-point rule: Manhattan distance
 * to the rounded click compared with `radius_squared`, a strictly nearer point wins.
 *
 * The transforms and deletes are NOT copies of Blender code. They apply Blender's rules:
 *  - only points flagged GP_SPOINT_SELECT in strokes flagged GP_STROKE_SELECT are touched
 *    (createTransGPencil() converts exactly those),
 *  - the default pivot is the median of the selected points,
 *  - strokes with a locked/hidden material or on hidden/locked layers are never edited,
 *  - BKE_gpencil_stroke_geometry_update() runs on every edited stroke so triangulation and
 *    bounding boxes follow,
 *  - delete points / delete strokes use BKE_gpencil_stroke_delete_tagged_points() with
 *    GP_SPOINT_SELECT as the tag and BKE_gpencil_free_stroke(), as GPENCIL_OT_delete does.
 *
 * Differences from the desktop operators: identity "projection" (canvas pixels, rounded), no
 * evaluated-vs-original copies, no curve-edit and segment select modes, and multi-frame editing
 * is keyed off the data flag only.
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
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

#define PGE_V2D_IS_CLIPPED 12000
#define GP_SELECTMODE_STROKE 1

/* Identity projection, rounded to int (like gpencil_point_to_xy()). */
static void pge_point_to_xy(const bGPDspoint *pt, int r_co[2])
{
  if (!isfinite(pt->x) || !isfinite(pt->y) || fabsf(pt->x) > 1.0e6f || fabsf(pt->y) > 1.0e6f) {
    r_co[0] = PGE_V2D_IS_CLIPPED;
    r_co[1] = PGE_V2D_IS_CLIPPED;
    return;
  }
  r_co[0] = (int)lroundf(pt->x);
  r_co[1] = (int)lroundf(pt->y);
}

static MaterialGPencilStyle pge_default_style;
static MaterialGPencilStyle *pge_material_style(const bGPdata *gpd, int act)
{
  const int index = act - 1;
  if (gpd->mat != NULL && index >= 0 && index < (int)gpd->totcol && gpd->mat[index] != NULL &&
      gpd->mat[index]->gp_style != NULL)
  {
    return gpd->mat[index]->gp_style;
  }
  return &pge_default_style;
}

/* ED_gpencil_stroke_material_editable() (gpencil_utils.c), reading gpd->mat[]. */
static bool pge_stroke_material_editable(const bGPdata *gpd,
                                         const bGPDlayer *gpl,
                                         const bGPDstroke *gps)
{
  MaterialGPencilStyle *gp_style = pge_material_style(gpd, gps->mat_nr + 1);
  if (gp_style != NULL) {
    if (gp_style->flag & GP_MATERIAL_HIDE) {
      return false;
    }
    if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (gp_style->flag & GP_MATERIAL_LOCKED)) {
      return false;
    }
  }
  return true;
}

static bool pge_layer_in_scope(const bGPDlayer *gpl, const bGPDlayer *only_layer)
{
  return only_layer == NULL || gpl == only_layer;
}

/* Modeled on GP_EDITABLE_STROKES_BEGIN/END (gpencil_intern.h). `gpsn_` is the next stroke,
 * saved before the body so it may free or replace `gps`. */
#define PGE_EDITABLE_STROKES_BEGIN(gpd_, only_layer_, gpl, gpf, gps) \
  { \
    const bool is_multiedit_ = ((gpd_)->flag & GP_DATA_STROKE_MULTIEDIT) != 0; \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd_)->layers) { \
      if (!pge_layer_in_scope(gpl, (only_layer_)) || !BKE_gpencil_layer_is_editable(gpl)) { \
        continue; \
      } \
      bGPDframe *init_gpf_ = (is_multiedit_) ? (bGPDframe *)gpl->frames.first : gpl->actframe; \
      for (bGPDframe *gpf = init_gpf_; gpf; gpf = gpf->next) { \
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && is_multiedit_)) { \
          bGPDstroke *gpsn_; \
          for (bGPDstroke *gps = (bGPDstroke *)gpf->strokes.first; gps; gps = gpsn_) { \
            gpsn_ = gps->next; \
            if (pge_stroke_material_editable((gpd_), gpl, gps) == false) { \
              continue; \
            } \
    /* ... Do Stuff With Strokes ... */

#define PGE_EDITABLE_STROKES_END \
  } \
  } \
  if (!is_multiedit_) { \
    break; \
  } \
  } \
  } \
  } \
  (void)0

/* ---------------------------------------------------------------------------------------- */
/* Selection helpers                                                                         */

/* deselect_all_selected() (gpencil_select.c): "extend == false" clears the old selection. */
static void pge_deselect_all_selected(bGPdata *gpd, const bGPDlayer *only_layer)
{
  /* Set selection index to 0. */
  gpd->select_last_index = 0;

  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
    /* deselect stroke and its points if selected */
    if (gps->flag & GP_STROKE_SELECT) {
      bGPDspoint *pt;
      int i;

      /* deselect points */
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        pt->flag &= ~GP_SPOINT_SELECT;
      }

      /* deselect stroke itself too */
      gps->flag &= ~GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_reset(gps);
    }
/* END VERBATIM */
  }
  PGE_EDITABLE_STROKES_END;
}

static unsigned char *pge_selection_snapshot(bGPdata *gpd, size_t *r_len)
{
  size_t len = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        len += 1 + (size_t)(gps->totpoints > 0 ? gps->totpoints : 0);
      }
    }
  }
  unsigned char *buf = malloc(len > 0 ? len : 1);
  if (buf == NULL) {
    *r_len = 0;
    return NULL;
  }
  size_t at = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        buf[at++] = (gps->flag & GP_STROKE_SELECT) ? 1 : 0;
        for (int i = 0; i < gps->totpoints; i++) {
          buf[at++] = (gps->points[i].flag & GP_SPOINT_SELECT) ? 1 : 0;
        }
      }
    }
  }
  *r_len = len;
  return buf;
}

static int pge_selection_changed(bGPdata *gpd, const unsigned char *before, size_t before_len)
{
  size_t after_len = 0;
  unsigned char *after = pge_selection_snapshot(gpd, &after_len);
  int changed = (after == NULL || before == NULL || after_len != before_len ||
                 memcmp(before, after, before_len) != 0);
  free(after);
  return changed;
}

/* ---------------------------------------------------------------------------------------- */
/* Click selection: gpencil_select_exec() without curve edit and segment mode                */

int pg_gp_edit_pick(bGPdata *gpd,
                    const bGPDlayer *only_layer,
                    float x,
                    float y,
                    int radius_squared,
                    int flags,
                    int selectmode)
{
  if (gpd == NULL || !isfinite(x) || !isfinite(y) || radius_squared < 0 ||
      (selectmode != 0 && selectmode != GP_SELECTMODE_STROKE))
  {
    return 0;
  }
  size_t before_len = 0;
  unsigned char *before = pge_selection_snapshot(gpd, &before_len);

  const bool extend = (flags & PG_PICK_EXTEND) != 0;
  bool deselect = (flags & PG_PICK_DESELECT) != 0;
  const bool toggle = (flags & PG_PICK_TOGGLE) != 0;
  bool whole = (flags & PG_PICK_ENTIRE) != 0;
  const bool deselect_all = (flags & PG_PICK_DESELECT_ALL) != 0;

  int mval[2];
  mval[0] = (int)lroundf(x);
  mval[1] = (int)lroundf(y);

  bGPDstroke *hit_stroke = NULL;
  bGPDspoint *hit_point = NULL;
  int hit_distance = radius_squared;

  /* if select mode is stroke, use whole stroke */
  whole |= (bool)(selectmode == GP_SELECTMODE_STROKE);

  /* First Pass: Find stroke point which gets hit */
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (gps->points == NULL) {
      continue;
    }
    /* firstly, check for hit-point */
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      int xy[2];
      pge_point_to_xy(pt, xy);

      /* do boundbox check first */
      if (xy[0] != PGE_V2D_IS_CLIPPED && xy[1] != PGE_V2D_IS_CLIPPED) {
        const int pt_distance = abs(mval[0] - xy[0]) + abs(mval[1] - xy[1]); /* len_manhattan_v2v2_int */

        /* check if point is inside */
        if (pt_distance <= radius_squared) {
          /* only use this point if it is a better match than the current hit - #44685 */
          if (pt_distance < hit_distance) {
            hit_stroke = gps;
            hit_point = pt;
            hit_distance = pt_distance;
          }
        }
      }
    }
  }
  PGE_EDITABLE_STROKES_END;

  /* Abort if nothing hit... */
  if (!hit_point && !hit_stroke) {
    if (deselect_all) {
      /* since left mouse select change, deselect all if click outside any hit */
      pge_deselect_all_selected(gpd, only_layer);
    }
    const int changed = pge_selection_changed(gpd, before, before_len);
    free(before);
    return changed;
  }

  /* Pass-through (Project Grease addition, like SelectPick_Params::select_passthrough):
   * picking something that is already selected keeps the whole selection, so a following
   * transform acts on all of it. */
  if (flags & PG_PICK_PASSTHROUGH) {
    const bool already = whole ? ((hit_stroke->flag & GP_STROKE_SELECT) != 0) :
                                 ((hit_point->flag & GP_SPOINT_SELECT) != 0);
    if (already) {
      free(before);
      return 0;
    }
  }

  /* adjust selection behavior - for toggle option */
  if (toggle) {
    deselect = (hit_point->flag & GP_SPOINT_SELECT) != 0;
  }

  /* If not extending selection, deselect everything else */
  if (extend == false) {
    pge_deselect_all_selected(gpd, only_layer);
  }

  /* Perform selection operations... */
  if (whole) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      bGPDspoint *pt;
      int i;

      /* entire stroke's points */
      for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
        if (deselect == false) {
          pt->flag |= GP_SPOINT_SELECT;
        }
        else {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
      }

      /* stroke too... */
      if (deselect == false) {
        hit_stroke->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, hit_stroke);
      }
      else {
        hit_stroke->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_reset(hit_stroke);
      }
/* END VERBATIM */
  }
  else {
    /* just the point (and the stroke) */
    if (deselect == false) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
        /* we're adding selection, so selection must be true */
        hit_point->flag |= GP_SPOINT_SELECT;
        hit_stroke->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, hit_stroke);
/* END VERBATIM */
    }
    else {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
        /* deselect point */
        hit_point->flag &= ~GP_SPOINT_SELECT;

        /* ensure that stroke is selected correctly */
        BKE_gpencil_stroke_sync_selection(gpd, hit_stroke);
/* END VERBATIM */
    }
  }

  const int changed = pge_selection_changed(gpd, before, before_len);
  free(before);
  return changed;
}

/* ---------------------------------------------------------------------------------------- */
/* Pivot and transforms                                                                      */

int pg_gp_edit_selection_pivot(const bGPdata *gpd_in,
                               const bGPDlayer *only_layer,
                               float *r_x,
                               float *r_y)
{
  bGPdata *gpd = (bGPdata *)gpd_in; /* read-only use */
  if (gpd == NULL || r_x == NULL || r_y == NULL) {
    return 0;
  }
  double sum_x = 0.0;
  double sum_y = 0.0;
  long count = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) {
      continue;
    }
    for (int i = 0; i < gps->totpoints; i++) {
      if (gps->points[i].flag & GP_SPOINT_SELECT) {
        sum_x += gps->points[i].x;
        sum_y += gps->points[i].y;
        count++;
      }
    }
  }
  PGE_EDITABLE_STROKES_END;
  if (count == 0) {
    return 0;
  }
  *r_x = (float)(sum_x / (double)count);
  *r_y = (float)(sum_y / (double)count);
  return 1;
}

typedef void (*PGEPointFn)(bGPDspoint *pt, const float *params);

/* Applies `fn` to the selected points of the selected strokes and refreshes their geometry. */
static int pge_apply_to_selected(bGPdata *gpd,
                                 const bGPDlayer *only_layer,
                                 PGEPointFn fn,
                                 const float *params)
{
  int moved = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) {
      continue;
    }
    bool touched = false;
    for (int i = 0; i < gps->totpoints; i++) {
      if (gps->points[i].flag & GP_SPOINT_SELECT) {
        fn(&gps->points[i], params);
        touched = true;
        moved++;
      }
    }
    if (touched) {
      /* Calc geometry data (triangulation of filled strokes, bounding box). */
      BKE_gpencil_stroke_geometry_update(gpd, gps);
    }
  }
  PGE_EDITABLE_STROKES_END;
  return moved;
}

static void pge_fn_translate(bGPDspoint *pt, const float *p)
{
  pt->x += p[0];
  pt->y += p[1];
}

static void pge_fn_rotate(bGPDspoint *pt, const float *p) /* p: cos, sin, cx, cy */
{
  const float x = pt->x - p[2];
  const float y = pt->y - p[3];
  pt->x = p[2] + x * p[0] - y * p[1];
  pt->y = p[3] + x * p[1] + y * p[0];
}

static void pge_fn_scale(bGPDspoint *pt, const float *p) /* p: sx, sy, cx, cy */
{
  pt->x = p[2] + (pt->x - p[2]) * p[0];
  pt->y = p[3] + (pt->y - p[3]) * p[1];
}

static void pge_fn_mirror(bGPDspoint *pt, const float *p) /* p: mx, my, cx, cy */
{
  if (p[0] != 0.0f) {
    pt->x = 2.0f * p[2] - pt->x;
  }
  if (p[1] != 0.0f) {
    pt->y = 2.0f * p[3] - pt->y;
  }
}

static int pge_resolve_pivot(bGPdata *gpd, const bGPDlayer *only_layer, const float *pivot, float out[2])
{
  if (pivot != NULL) {
    if (!isfinite(pivot[0]) || !isfinite(pivot[1])) {
      return 0;
    }
    out[0] = pivot[0];
    out[1] = pivot[1];
    return 1;
  }
  return pg_gp_edit_selection_pivot(gpd, only_layer, &out[0], &out[1]);
}

int pg_gp_edit_translate(bGPdata *gpd, const bGPDlayer *only_layer, float dx, float dy)
{
  if (gpd == NULL || !isfinite(dx) || !isfinite(dy) || (dx == 0.0f && dy == 0.0f)) {
    return 0;
  }
  const float params[2] = {dx, dy};
  return pge_apply_to_selected(gpd, only_layer, pge_fn_translate, params) > 0;
}

int pg_gp_edit_rotate(bGPdata *gpd, const bGPDlayer *only_layer, float radians, const float *pivot_xy)
{
  float pivot[2];
  if (gpd == NULL || !isfinite(radians) || radians == 0.0f ||
      !pge_resolve_pivot(gpd, only_layer, pivot_xy, pivot))
  {
    return 0;
  }
  const float params[4] = {cosf(radians), sinf(radians), pivot[0], pivot[1]};
  return pge_apply_to_selected(gpd, only_layer, pge_fn_rotate, params) > 0;
}

int pg_gp_edit_scale(bGPdata *gpd,
                     const bGPDlayer *only_layer,
                     float scale_x,
                     float scale_y,
                     const float *pivot_xy)
{
  float pivot[2];
  if (gpd == NULL || !isfinite(scale_x) || !isfinite(scale_y) || scale_x == 0.0f ||
      scale_y == 0.0f || (scale_x == 1.0f && scale_y == 1.0f) ||
      !pge_resolve_pivot(gpd, only_layer, pivot_xy, pivot))
  {
    return 0;
  }
  const float params[4] = {scale_x, scale_y, pivot[0], pivot[1]};
  return pge_apply_to_selected(gpd, only_layer, pge_fn_scale, params) > 0;
}

int pg_gp_edit_mirror(bGPdata *gpd,
                      const bGPDlayer *only_layer,
                      int mirror_x,
                      int mirror_y,
                      const float *pivot_xy)
{
  float pivot[2];
  if (gpd == NULL || (!mirror_x && !mirror_y) || !pge_resolve_pivot(gpd, only_layer, pivot_xy, pivot)) {
    return 0;
  }
  const float params[4] = {mirror_x ? 1.0f : 0.0f, mirror_y ? 1.0f : 0.0f, pivot[0], pivot[1]};
  return pge_apply_to_selected(gpd, only_layer, pge_fn_mirror, params) > 0;
}

/* ---------------------------------------------------------------------------------------- */
/* Delete                                                                                    */

int pg_gp_edit_delete_strokes(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      /* free stroke memory arrays, then stroke itself */
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
      changed = 1;
    }
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

int pg_gp_edit_delete_points(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) {
      continue;
    }
    bool any = false;
    for (int i = 0; i < gps->totpoints; i++) {
      if (gps->points[i].flag & GP_SPOINT_SELECT) {
        any = true;
        break;
      }
    }
    if (!any) {
      continue;
    }
    /* delete unwanted points by splitting stroke into several smaller ones */
    BKE_gpencil_stroke_delete_tagged_points(gpd, gpf, gps, gpsn_, GP_SPOINT_SELECT, false, false, 0);
    changed = 1;
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}


/* ---------------------------------------------------------------------------------------- */
/* Legacy GP modifiers, applied to the selected strokes like "Apply modifier".               */
/* The per-stroke code mirrors deformStroke() of the pinned modifier files with no vertex   */
/* group (def_nr = -1, so get_modifier_point_weight() returns 1) and no custom curve.        */

static float pge_clampf(float v, float lo, float hi)
{
  return v < lo ? lo : (v > hi ? hi : v); /* CLAMP() */
}

static float pge_interpf(float target, float origin, float t)
{
  return (t * target) + ((1.0f - t) * origin); /* interpf() */
}

int pg_gp_modstroke_thickness(bGPDstroke *gps, int normalize, int thickness, float thickness_fac)
{
  if (gps == NULL || gps->points == NULL || !isfinite(thickness_fac)) {
    return 0;
  }
  int changed = 0;
  const float stroke_thickness_inv = 1.0f / (float)(gps->thickness > 1 ? gps->thickness : 1) /* max_ii */;
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    const float weight = 1.0f; /* no vertex group */
    const float curvef = 1.0f; /* no custom curve */
    float target;
    if (normalize) {
      target = (float)thickness * stroke_thickness_inv;
      target *= curvef;
    }
    else {
      target = pt->pressure * thickness_fac;
    }
    const float before = pt->pressure;
    pt->pressure = pge_interpf(target, pt->pressure, weight);
    if (pt->pressure < 0.0f) {
      pt->pressure = 0.0f;
    }
    changed |= (pt->pressure != before);
  }
  return changed;
}

int pg_gp_mod_thickness(bGPdata *gpd, const bGPDlayer *only_layer,
                        int normalize, int thickness, float thickness_fac)
{
  if (gpd == NULL || !isfinite(thickness_fac)) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) {
      continue;
    }
    changed |= pg_gp_modstroke_thickness(gps, normalize, thickness, thickness_fac);
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

int pg_gp_modstroke_opacity(bGPDstroke *gps, int modify_color, float factor, int normalize,
                            float hardness)
{
  if (gps == NULL || !isfinite(factor) || !isfinite(hardness) ||
      modify_color < PG_MODIFY_COLOR_BOTH || modify_color > PG_MODIFY_COLOR_HARDNESS)
  {
    return 0;
  }
  int changed = 0;
  /* Hardness (at stroke level). */
  if (modify_color == PG_MODIFY_COLOR_HARDNESS) {
    gps->hardeness *= hardness;
    gps->hardeness = pge_clampf(gps->hardeness, 0.0f, 1.0f);
    return 1;
  }
  if (modify_color != PG_MODIFY_COLOR_FILL && gps->points != NULL) {
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      const float factor_curve = factor; /* no custom curve */
      /* def_nr < 0 */
      if (normalize) {
        pt->strength = factor_curve;
      }
      else {
        pt->strength += factor_curve - 1.0f;
      }
      pt->strength = pge_clampf(pt->strength, 0.0f, 1.0f);
    }
    changed = 1;
  }
  /* Fill using opacity factor. */
  if (modify_color != PG_MODIFY_COLOR_STROKE) {
    gps->fill_opacity_fac = factor;
    gps->fill_opacity_fac = pge_clampf(gps->fill_opacity_fac, 0.0f, 1.0f);
    changed = 1;
  }
  return changed;
}

int pg_gp_mod_opacity(bGPdata *gpd, const bGPDlayer *only_layer,
                      int modify_color, float factor, int normalize, float hardness)
{
  if (gpd == NULL || !isfinite(factor) || !isfinite(hardness) ||
      modify_color < PG_MODIFY_COLOR_BOTH || modify_color > PG_MODIFY_COLOR_HARDNESS)
  {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT)) {
      continue;
    }
    changed |= pg_gp_modstroke_opacity(gps, modify_color, factor, normalize, hardness);
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* gpencil_modify_stroke() (MOD_gpencil_legacy_length.c) */
static bool pge_length_modify_stroke(bGPDstroke *gps, const float length, const float overshoot_fac,
                                     const short len_mode, const bool use_curvature,
                                     const int extra_point_count, const float segment_influence,
                                     const float max_angle, const bool invert_curvature)
{
  bool changed = false;
  if (length == 0.0f) {
    return changed;
  }
  if (length > 0.0f) {
    changed = BKE_gpencil_stroke_stretch(gps, length, overshoot_fac, len_mode, use_curvature,
                                         extra_point_count, segment_influence, max_angle,
                                         invert_curvature);
  }
  else {
    changed = BKE_gpencil_stroke_shrink(gps, fabsf(length), len_mode);
  }
  return changed;
}

int pg_gp_modstroke_length(bGPdata *gpd, bGPDstroke *gps, const PGLengthParams *p)
{
  if (gpd == NULL || gps == NULL || gps->points == NULL || p == NULL || !isfinite(p->start_fac) ||
      !isfinite(p->end_fac) || !isfinite(p->overshoot_fac) || !isfinite(p->point_density) ||
      (p->mode != PG_LENGTH_RELATIVE && p->mode != PG_LENGTH_ABSOLUTE))
  {
    return 0;
  }
  if ((gps->flag & GP_STROKE_CYCLIC) != 0) {
    /* Don't affect cyclic strokes as they have no start/end. */
    return 0;
  }
  /* applyLength() */
  bool changed = false;
  const float len = (p->mode == PG_LENGTH_ABSOLUTE) ? 1.0f : BKE_gpencil_stroke_length(gps, true);
  const int totpoints = gps->totpoints;
  if (len < FLT_EPSILON) {
    return 0;
  }
  float first_fac = p->start_fac;
  int first_mode = 1;
  float second_fac = p->end_fac;
  int second_mode = 2;
  if (first_fac < 0) {
    const float tf = first_fac; first_fac = second_fac; second_fac = tf; /* SWAP */
    const int tm = first_mode; first_mode = second_mode; second_mode = tm;
  }
  const int first_extra_point_count = (int)ceilf(first_fac * p->point_density);
  const int second_extra_point_count = (int)ceilf(second_fac * p->point_density);

  changed |= pge_length_modify_stroke(gps, len * first_fac, p->overshoot_fac, (short)first_mode,
                                      p->use_curvature != 0, first_extra_point_count,
                                      p->segment_influence, p->max_angle, p->invert_curvature != 0);
  const float second_overshoot_fac = p->overshoot_fac * (totpoints - 2) /
                                     ((float)gps->totpoints - 2) *
                                     (1.0f - 0.1f / (totpoints - 1.0f));
  changed |= pge_length_modify_stroke(gps, len * second_fac, second_overshoot_fac,
                                      (short)second_mode, p->use_curvature != 0,
                                      second_extra_point_count, p->segment_influence,
                                      p->max_angle, p->invert_curvature != 0);
  if (changed) {
    BKE_gpencil_stroke_geometry_update(gpd, gps);
    return 1;
  }
  return 0;
}

int pg_gp_mod_length(bGPdata *gpd, const bGPDlayer *only_layer, const PGLengthParams *p)
{
  if (gpd == NULL || p == NULL || !isfinite(p->start_fac) || !isfinite(p->end_fac) ||
      !isfinite(p->overshoot_fac) || !isfinite(p->point_density) ||
      (p->mode != PG_LENGTH_RELATIVE && p->mode != PG_LENGTH_ABSOLUTE))
  {
    return 0;
  }
  int any = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) {
      continue;
    }
    any |= pg_gp_modstroke_length(gpd, gps, p);
  }
  PGE_EDITABLE_STROKES_END;
  return any;
}

static void pge_interp_v3(float r[3], const float a[3], const float b[3], float t)
{
  const float s = 1.0f - t; /* interp_v3_v3v3() */
  r[0] = s * a[0] + t * b[0];
  r[1] = s * a[1] + t * b[1];
  r[2] = s * a[2] + t * b[2];
}

int pg_gp_modstroke_tint(bGPdata *gpd, bGPDstroke *gps, int vertex_mode, float factor,
                         const float rgb[3])
{
  if (gpd == NULL || gps == NULL || gps->points == NULL || rgb == NULL || !isfinite(factor) ||
      !isfinite(rgb[0]) || !isfinite(rgb[1]) || !isfinite(rgb[2]) ||
      vertex_mode < PG_PAINT_MODE_STROKE || vertex_mode > PG_PAINT_MODE_BOTH)
  {
    return 0;
  }
  MaterialGPencilStyle *gp_style = pge_material_style(gpd, gps->mat_nr + 1);

  /* If factor > 1.0, affect the strength of the stroke. */
  if (factor > 1.0f) {
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      pt->strength += factor - 1.0f;
      pt->strength = pge_clampf(pt->strength, 0.0f, 1.0f);
    }
  }

  /* loop points and apply color. */
  bool fill_done = false;
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];

    if (!fill_done) {
      /* Apply to fill. */
      if (vertex_mode != PG_PAINT_MODE_STROKE) {
        const float fill_factor = factor;
        /* If not using Vertex Color, use the material color. */
        if ((gp_style != NULL) && (gps->vert_color_fill[3] == 0.0f) &&
            (gp_style->fill_rgba[3] > 0.0f))
        {
          memcpy(gps->vert_color_fill, gp_style->fill_rgba, sizeof(float[4]));
          gps->vert_color_fill[3] = 1.0f;
        }
        pge_interp_v3(gps->vert_color_fill, gps->vert_color_fill, rgb,
                      pge_clampf(fill_factor, 0.0f, 1.0f));
        /* If no stroke, cancel loop. */
        if (vertex_mode != PG_PAINT_MODE_BOTH) {
          break;
        }
      }
      fill_done = true;
    }

    if (vertex_mode != PG_PAINT_MODE_FILL) {
      const float weight = 1.0f; /* no vertex group, no curve */
      /* If not using Vertex Color, use the material color. */
      if ((gp_style != NULL) && (pt->vert_color[3] == 0.0f) && (gp_style->stroke_rgba[3] > 0.0f)) {
        memcpy(pt->vert_color, gp_style->stroke_rgba, sizeof(float[4]));
        pt->vert_color[3] = 1.0f;
      }
      pge_interp_v3(pt->vert_color, pt->vert_color, rgb, pge_clampf(factor * weight, 0.0f, 1.0f));
    }
  }
  return 1;
}

int pg_gp_mod_tint(bGPdata *gpd, const bGPDlayer *only_layer,
                   int vertex_mode, float factor, const float rgb[3])
{
  if (gpd == NULL || rgb == NULL || !isfinite(factor) || !isfinite(rgb[0]) ||
      !isfinite(rgb[1]) || !isfinite(rgb[2]) || vertex_mode < PG_PAINT_MODE_STROKE ||
      vertex_mode > PG_PAINT_MODE_BOTH)
  {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) {
      continue;
    }
    changed |= pg_gp_modstroke_tint(gpd, gps, vertex_mode, factor, rgb);
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* rgb_to_hsv() (blenlib/intern/math_color.c) */
void pg_rgb_to_hsv(const float rgb[3], float r_hsv[3])
{
  float r = rgb[0], g = rgb[1], b = rgb[2];
  float k = 0.0f;
  float chroma;
  float min_gb;
  if (g < b) {
    const float t = g; g = b; b = t;
    k = -1.0f;
  }
  min_gb = b;
  if (r < g) {
    const float t = r; r = g; g = t;
    k = -2.0f / 6.0f - k;
    min_gb = (g < b) ? g : b;
  }
  chroma = r - min_gb;
  r_hsv[0] = fabsf(k + (g - b) / (6.0f * chroma + 1e-20f));
  r_hsv[1] = chroma / (r + 1e-20f);
  r_hsv[2] = r;
}

/* hsv_to_rgb() (blenlib/intern/math_color.c) */
void pg_hsv_to_rgb(const float hsv[3], float r_rgb[3])
{
  const float h = hsv[0], sat = hsv[1], v = hsv[2];
  float nr = fabsf(h * 6.0f - 3.0f) - 1.0f;
  float ng = 2.0f - fabsf(h * 6.0f - 2.0f);
  float nb = 2.0f - fabsf(h * 6.0f - 4.0f);
  nr = pge_clampf(nr, 0.0f, 1.0f);
  nb = pge_clampf(nb, 0.0f, 1.0f);
  ng = pge_clampf(ng, 0.0f, 1.0f);
  r_rgb[0] = ((nr - 1.0f) * sat + 1.0f) * v;
  r_rgb[1] = ((ng - 1.0f) * sat + 1.0f) * v;
  r_rgb[2] = ((nb - 1.0f) * sat + 1.0f) * v;
}

static float pge_fractf(float a)
{
  return a - floorf(a); /* fractf() */
}

static void pge_apply_hsv(float color[3], const float factor[3])
{
  float hsv[3];
  pg_rgb_to_hsv(color, hsv);
  hsv[0] = pge_fractf(hsv[0] + factor[0] + 0.5f);
  hsv[1] = pge_clampf(hsv[1] * factor[1], 0.0f, 1.0f);
  hsv[2] = hsv[2] * factor[2];
  pg_hsv_to_rgb(hsv, color);
}

int pg_gp_modstroke_color(bGPdata *gpd, bGPDstroke *gps, int modify_color, const float f[3])
{
  if (gpd == NULL || gps == NULL || gps->points == NULL || f == NULL || !isfinite(f[0]) ||
      !isfinite(f[1]) || !isfinite(f[2]) || modify_color < PG_MODIFY_COLOR_BOTH ||
      modify_color > PG_MODIFY_COLOR_FILL)
  {
    return 0;
  }
  MaterialGPencilStyle *gp_style = pge_material_style(gpd, gps->mat_nr + 1);
  /* Fill */
  if (modify_color != PG_MODIFY_COLOR_STROKE) {
    if ((gp_style != NULL) && (gps->vert_color_fill[3] == 0.0f) && (gp_style->fill_rgba[3] > 0.0f)) {
      memcpy(gps->vert_color_fill, gp_style->fill_rgba, sizeof(float[4]));
      gps->vert_color_fill[3] = 1.0f;
    }
    pge_apply_hsv(gps->vert_color_fill, f);
  }
  /* Stroke */
  if (modify_color != PG_MODIFY_COLOR_FILL) {
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      if ((gp_style != NULL) && (pt->vert_color[3] == 0.0f) && (gp_style->stroke_rgba[3] > 0.0f)) {
        memcpy(pt->vert_color, gp_style->stroke_rgba, sizeof(float[4]));
        pt->vert_color[3] = 1.0f;
      }
      pge_apply_hsv(pt->vert_color, f);
    }
  }
  return 1;
}

int pg_gp_mod_color(bGPdata *gpd, const bGPDlayer *only_layer, int modify_color, const float f[3])
{
  if (gpd == NULL || f == NULL || !isfinite(f[0]) || !isfinite(f[1]) || !isfinite(f[2]) ||
      modify_color < PG_MODIFY_COLOR_BOTH || modify_color > PG_MODIFY_COLOR_FILL)
  {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) {
      continue;
    }
    changed |= pg_gp_modstroke_color(gpd, gps, modify_color, f);
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* ---------------------------------------------------------------------------------------- */
/* Stroke operators. These follow the behaviour of the 3.6.23 operators named below; they   */
/* are written for this data model, not copied from the operator code.                      */

static bool pge_stroke_selected(const bGPDstroke *gps)
{
  return (gps->flag & GP_STROKE_SELECT) != 0;
}

/* list helpers (BLI_remlink / BLI_insertlinkafter / BLI_insertlinkbefore semantics) */
static void pge_insert_after(ListBase *lb, Link *prev, Link *link)
{
  if (prev == NULL) { /* insert at head */
    link->prev = NULL;
    link->next = lb->first;
    if (lb->first) ((Link *)lb->first)->prev = link; else lb->last = link;
    lb->first = link;
    return;
  }
  link->prev = prev;
  link->next = prev->next;
  if (prev->next) prev->next->prev = link; else lb->last = link;
  prev->next = link;
}

/* GPENCIL_OT_stroke_arrange: later strokes draw on top, so "top" is the list tail. */
int pg_gp_stroke_arrange(bGPdata *gpd, const bGPDlayer *only_layer, int direction)
{
  if (gpd == NULL || direction < PG_ARRANGE_TOP || direction > PG_ARRANGE_BOTTOM) {
    return 0;
  }
  int changed = 0;
  const bool is_multiedit = (gpd->flag & GP_DATA_STROKE_MULTIEDIT) != 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!pge_layer_in_scope(gpl, only_layer) || !BKE_gpencil_layer_is_editable(gpl)) {
      continue;
    }
    for (bGPDframe *gpf = is_multiedit ? gpl->frames.first : gpl->actframe; gpf; gpf = gpf->next) {
      if (!((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && is_multiedit))) {
        continue;
      }
      ListBase *lb = &gpf->strokes;
      if (direction == PG_ARRANGE_TOP || direction == PG_ARRANGE_BOTTOM) {
        /* collect selected strokes in order, then move them as a block keeping their order */
        ListBase moved = {NULL, NULL};
        for (bGPDstroke *gps = lb->first, *next; gps; gps = next) {
          next = gps->next;
          if (pge_stroke_selected(gps) && pge_stroke_material_editable(gpd, gpl, gps)) {
            BLI_remlink(lb, gps);
            pge_insert_after(&moved, moved.last, (Link *)gps);
          }
        }
        if (moved.first == NULL) {
          continue;
        }
        Link *anchor = (direction == PG_ARRANGE_TOP) ? (Link *)lb->last : NULL;
        for (Link *link = moved.first, *next; link; link = next) {
          next = link->next;
          pge_insert_after(lb, anchor, link);
          anchor = link;
        }
        changed = 1;
      }
      else if (direction == PG_ARRANGE_UP) {
        /* from the end: a selected stroke swaps with an unselected next one */
        for (bGPDstroke *gps = lb->last, *prev; gps; gps = prev) {
          prev = gps->prev;
          bGPDstroke *next = gps->next;
          if (pge_stroke_selected(gps) && pge_stroke_material_editable(gpd, gpl, gps) && next &&
              !pge_stroke_selected(next))
          {
            BLI_remlink(lb, gps);
            pge_insert_after(lb, (Link *)next, (Link *)gps);
            changed = 1;
          }
        }
      }
      else { /* PG_ARRANGE_DOWN: from the start, swap with an unselected previous one */
        for (bGPDstroke *gps = lb->first, *next; gps; gps = next) {
          next = gps->next;
          bGPDstroke *prev = gps->prev;
          if (pge_stroke_selected(gps) && pge_stroke_material_editable(gpd, gpl, gps) && prev &&
              !pge_stroke_selected(prev))
          {
            BLI_remlink(lb, gps);
            pge_insert_after(lb, (Link *)prev->prev, (Link *)gps);
            changed = 1;
          }
        }
      }
    }
    if (!is_multiedit && only_layer != NULL) {
      continue;
    }
  }
  return changed;
}

/* GPENCIL_OT_stroke_change_color: assign the active material to the selected strokes. */
int pg_gp_stroke_set_material(bGPdata *gpd, const bGPDlayer *only_layer, int mat_nr)
{
  if (gpd == NULL || mat_nr < 0 || mat_nr >= (int)gpd->totcol) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (pge_stroke_selected(gps) && gps->mat_nr != mat_nr) {
      gps->mat_nr = mat_nr;
      changed = 1;
    }
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* GPENCIL_OT_stroke_reset_vertex_color */
int pg_gp_stroke_reset_vertex_color(bGPdata *gpd, const bGPDlayer *only_layer, int mode)
{
  if (gpd == NULL || mode < PG_PAINT_MODE_STROKE || mode > PG_PAINT_MODE_BOTH) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!pge_stroke_selected(gps)) {
      continue;
    }
    if (mode != PG_PAINT_MODE_FILL && gps->points != NULL) {
      for (int i = 0; i < gps->totpoints; i++) {
        memset(gps->points[i].vert_color, 0, sizeof(float[4]));
      }
    }
    if (mode != PG_PAINT_MODE_STROKE) {
      memset(gps->vert_color_fill, 0, sizeof(float[4]));
    }
    changed = 1;
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* GPENCIL_OT_stroke_flip */
int pg_gp_stroke_flip(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (pge_stroke_selected(gps) && gps->totpoints > 1) {
      BKE_gpencil_stroke_flip(gps);
      changed = 1;
    }
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* GPENCIL_OT_stroke_cyclical_set (without the "create geometry" option) */
int pg_gp_stroke_cyclical_set(bGPdata *gpd, const bGPDlayer *only_layer, int type)
{
  if (gpd == NULL || type < PG_CYCLIC_CLOSE || type > PG_CYCLIC_TOGGLE) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!pge_stroke_selected(gps) || gps->totpoints < 3) {
      continue;
    }
    const int before = gps->flag;
    switch (type) {
      case PG_CYCLIC_CLOSE: gps->flag |= GP_STROKE_CYCLIC; break;
      case PG_CYCLIC_OPEN: gps->flag &= ~GP_STROKE_CYCLIC; break;
      case PG_CYCLIC_TOGGLE: gps->flag ^= GP_STROKE_CYCLIC; break;
    }
    if (gps->flag != before) {
      BKE_gpencil_stroke_geometry_update(gpd, gps);
      changed = 1;
    }
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* GPENCIL_OT_snap_to_grid: selected points to the nearest grid intersection (canvas grid). */
int pg_gp_snap_to_grid(bGPdata *gpd, const bGPDlayer *only_layer, float grid)
{
  if (gpd == NULL || !isfinite(grid) || grid <= 0.0f) {
    return 0;
  }
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!pge_stroke_selected(gps) || gps->points == NULL) {
      continue;
    }
    bool touched = false;
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      if (!(pt->flag & GP_SPOINT_SELECT)) {
        continue;
      }
      const float sx = grid * roundf(pt->x / grid);
      const float sy = grid * roundf(pt->y / grid);
      if (sx != pt->x || sy != pt->y) {
        pt->x = sx;
        pt->y = sy;
        touched = true;
      }
    }
    if (touched) {
      BKE_gpencil_stroke_geometry_update(gpd, gps);
      changed = 1;
    }
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

/* ---------------------------------------------------------------------------------------- */
/* Point/stroke structure operators (behaviour of the named 3.6.23 operators, using BKE).    */

static bool pge_any_point_selected(const bGPDstroke *gps)
{
  for (int i = 0; i < gps->totpoints; i++) {
    if (gps->points[i].flag & GP_SPOINT_SELECT) return true;
  }
  return false;
}

/* Copy `gps`, keep only its selected points (each selected run becomes its own stroke through
 * BKE_gpencil_stroke_delete_tagged_points), insert after `gps`, and return the first copy. */
static bGPDstroke *pge_copy_selected_runs(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps)
{
  bGPDstroke *dup = BKE_gpencil_stroke_duplicate(gps, true, true);
  if (dup == NULL) {
    return NULL;
  }
  pge_insert_after(&gpf->strokes, (Link *)gps, (Link *)dup);
  for (int i = 0; i < dup->totpoints; i++) {
    if (dup->points[i].flag & GP_SPOINT_SELECT) dup->points[i].flag &= ~GP_SPOINT_TAG;
    else dup->points[i].flag |= GP_SPOINT_TAG;
  }
  bGPDstroke *next = dup->next;
  bool any_tag = false;
  for (int i = 0; i < dup->totpoints; i++) any_tag |= (dup->points[i].flag & GP_SPOINT_TAG) != 0;
  if (any_tag) {
    BKE_gpencil_stroke_delete_tagged_points(gpd, gpf, dup, next, GP_SPOINT_TAG, false, false, 0);
  }
  /* copies (now between gps and next) end up selected */
  bGPDstroke *first = gps->next != next ? gps->next : NULL;
  for (bGPDstroke *c = first; c && c != next; c = c->next) {
    c->flag |= GP_STROKE_SELECT;
    for (int i = 0; i < c->totpoints; i++) c->points[i].flag |= GP_SPOINT_SELECT;
    BKE_gpencil_stroke_geometry_update(gpd, c);
  }
  return first;
}

int pg_gp_duplicate(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) return 0;
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!pge_stroke_selected(gps) || gps->points == NULL || !pge_any_point_selected(gps)) continue;
    if (pge_copy_selected_runs(gpd, gpf, gps) != NULL) {
      /* deselect the original, select the duplicate */
      for (int i = 0; i < gps->totpoints; i++) gps->points[i].flag &= ~GP_SPOINT_SELECT;
      gps->flag &= ~GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_reset(gps);
      changed = 1;
    }
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

int pg_gp_dissolve(bGPdata *gpd, const bGPDlayer *only_layer, int type)
{
  if (gpd == NULL || type < PG_DISSOLVE_POINTS || type > PG_DISSOLVE_UNSELECT) return 0;
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!pge_stroke_selected(gps) || gps->points == NULL) continue;
    int first = -1, last = -1;
    for (int i = 0; i < gps->totpoints; i++) {
      if (gps->points[i].flag & GP_SPOINT_SELECT) { if (first < 0) first = i; last = i; }
    }
    if (first < 0) continue;
    int keep = 0;
    for (int i = 0; i < gps->totpoints; i++) {
      const bool sel = (gps->points[i].flag & GP_SPOINT_SELECT) != 0;
      bool remove;
      switch (type) {
        case PG_DISSOLVE_POINTS: remove = sel; break;
        case PG_DISSOLVE_BETWEEN: remove = !sel && i > first && i < last; break;
        default: remove = !sel; break;
      }
      if (!remove) {
        /* vertex weights move with their point */
        if (gps->dvert != NULL) gps->dvert[keep] = gps->dvert[i];
        gps->points[keep++] = gps->points[i];
      }
      else if (gps->dvert != NULL) {
        MEM_SAFE_FREE(gps->dvert[i].dw); /* weights of a removed point */
      }
    }
    if (keep == gps->totpoints) continue;
    changed = 1;
    if (keep <= 0) {
      /* nothing left: delete the stroke */
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
      continue;
    }
    gps->totpoints = keep;
    BKE_gpencil_stroke_sync_selection(gpd, gps);
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

int pg_gp_split(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) return 0;
  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!pge_stroke_selected(gps) || gps->points == NULL || !pge_any_point_selected(gps)) continue;
    bool all = true;
    for (int i = 0; i < gps->totpoints; i++) all &= (gps->points[i].flag & GP_SPOINT_SELECT) != 0;
    if (all) continue; /* nothing to split off */
    if (pge_copy_selected_runs(gpd, gpf, gps) == NULL) continue;
    /* remove the selected points from the original (it may split into several strokes) */
    BKE_gpencil_stroke_delete_tagged_points(gpd, gpf, gps, gps->next, GP_SPOINT_SELECT, false, false, 0);
    changed = 1;
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}

int pg_gp_join(bGPdata *gpd, const bGPDlayer *only_layer, int leave_gaps)
{
  if (gpd == NULL) return 0;
  int changed = 0;
  bGPDstroke *target = NULL;
  bGPDframe *target_frame = NULL;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!pge_stroke_selected(gps) || gps->points == NULL) continue;
    if (target == NULL) { target = gps; target_frame = gpf; continue; }
    if (gpf != target_frame) continue; /* joins stay within one frame */
    BKE_gpencil_stroke_join(target, gps, leave_gaps != 0, true, false, true);
    BLI_remlink(&gpf->strokes, gps);
    BKE_gpencil_free_stroke(gps);
    changed = 1;
  }
  PGE_EDITABLE_STROKES_END;
  if (changed) {
    BKE_gpencil_stroke_geometry_update(gpd, target);
  }
  return changed;
}

/* ---------------------------------------------------------------------------------------- */
/* Vertex Paint mode (behaviour of editors/gpencil_legacy/gpencil_vertex_paint.c brushes).  */
/* Influence = strength * falloff; the falloff uses Blender's default "Smooth" brush curve   */
/* (3t^2 - 2t^3 of t = 1 - d/r). Pen pressure is folded into `strength` by the caller.      */

static float pge_vpaint_influence(const PGVertexPaint *vp, float px, float py)
{
  const float d = hypotf(px - vp->x, py - vp->y);
  if (d >= vp->radius) return 0.0f;
  const float t = 1.0f - d / vp->radius;
  return pge_clampf(vp->strength, 0.0f, 1.0f) * (t * t * (3.0f - 2.0f * t));
}

static void copy_v3_v3_pge(float r[3], const float a[3])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
}

static void pge_ensure_vcolor(float col[4], const float *material_rgba, const float fallback_rgb[3])
{
  /* points without vertex color start from the material color, like the modifiers; without a
   * material style (or a transparent one) they start from the paint color itself */
  if (col[3] == 0.0f) {
    if (material_rgba != NULL && material_rgba[3] > 0.0f) {
      memcpy(col, material_rgba, sizeof(float[4]));
    }
    else {
      copy_v3_v3_pge(col, fallback_rgb);
    }
    col[3] = 0.0f; /* alpha is the vertex-color mix factor; it grows with painting */
  }
}

static void pge_mix_toward(float col[4], const float target[3], float f)
{
  pge_interp_v3(col, col, target, f);
  col[3] = col[3] + (1.0f - col[3]) * f; /* coverage of the vertex color */
}

int pg_gp_vertex_paint(bGPdata *gpd, const bGPDlayer *only_layer, const PGVertexPaint *vp)
{
  if (gpd == NULL || vp == NULL || !isfinite(vp->x) || !isfinite(vp->y) || !(vp->radius > 0.0f) ||
      !isfinite(vp->strength) || vp->brush < PG_VPAINT_DRAW || vp->brush > PG_VPAINT_REPLACE ||
      vp->target < PG_PAINT_MODE_STROKE || vp->target > PG_PAINT_MODE_BOTH)
  {
    return 0;
  }
  const bool do_stroke = vp->target != PG_PAINT_MODE_FILL;
  const bool do_fill = vp->target != PG_PAINT_MODE_STROKE;

  /* Average brush: mean color of the affected points first (gpencil_vertexpaint_average_brush). */
  float avg[3] = {0, 0, 0};
  int avg_n = 0;
  if (vp->brush == PG_VPAINT_AVERAGE) {
    PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
      if (gps->points == NULL) continue;
      for (int i = 0; i < gps->totpoints; i++) {
        const bGPDspoint *pt = &gps->points[i];
        if (pge_vpaint_influence(vp, pt->x, pt->y) > 0.0f && pt->vert_color[3] > 0.0f) {
          avg[0] += pt->vert_color[0]; avg[1] += pt->vert_color[1]; avg[2] += pt->vert_color[2];
          avg_n++;
        }
      }
    }
    PGE_EDITABLE_STROKES_END;
    if (avg_n == 0) return 0;
    avg[0] /= avg_n; avg[1] /= avg_n; avg[2] /= avg_n;
  }

  int changed = 0;
  PGE_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (gps->points == NULL || gps->totpoints == 0) continue;
    MaterialGPencilStyle *gp_style = pge_material_style(gpd, gps->mat_nr + 1);
    float stroke_max = 0.0f;
    /* colors before this dab, for blur/smear neighbours */
    float (*orig)[4] = NULL;
    if (vp->brush == PG_VPAINT_BLUR || vp->brush == PG_VPAINT_SMEAR) {
      orig = malloc(sizeof(float[4]) * (size_t)gps->totpoints);
      if (orig == NULL) continue;
      for (int i = 0; i < gps->totpoints; i++) memcpy(orig[i], gps->points[i].vert_color, sizeof(float[4]));
    }
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      const float f = pge_vpaint_influence(vp, pt->x, pt->y);
      if (f <= 0.0f) continue;
      if (f > stroke_max) stroke_max = f;
      if (!do_stroke) continue;
      switch (vp->brush) {
        case PG_VPAINT_DRAW:
          pge_ensure_vcolor(pt->vert_color, gp_style ? gp_style->stroke_rgba : NULL, vp->rgb);
          pge_mix_toward(pt->vert_color, vp->rgb, f);
          changed = 1;
          break;
        case PG_VPAINT_REPLACE:
          /* only points that already carry vertex color */
          if (pt->vert_color[3] > 0.0f) {
            pge_interp_v3(pt->vert_color, pt->vert_color, vp->rgb, f);
            changed = 1;
          }
          break;
        case PG_VPAINT_AVERAGE:
          if (pt->vert_color[3] > 0.0f) {
            pge_interp_v3(pt->vert_color, pt->vert_color, avg, f);
            changed = 1;
          }
          break;
        case PG_VPAINT_BLUR: {
          /* mean of the point and its stroke neighbours (before this dab) */
          float sum[4] = {0, 0, 0, 0};
          int n = 0;
          for (int k = i - 1; k <= i + 1; k++) {
            if (k < 0 || k >= gps->totpoints) continue;
            for (int c = 0; c < 4; c++) sum[c] += orig[k][c];
            n++;
          }
          if (sum[3] > 0.0f) {
            const float mean[3] = {sum[0] / n, sum[1] / n, sum[2] / n};
            pge_interp_v3(pt->vert_color, pt->vert_color, mean, f);
            pt->vert_color[3] = pt->vert_color[3] + (sum[3] / n - pt->vert_color[3]) * f;
            changed = 1;
          }
          break;
        }
        case PG_VPAINT_SMEAR: {
          /* take the color from the neighbour behind the drag direction */
          if (vp->dx == 0.0f && vp->dy == 0.0f) break;
          int src = -1;
          float best = 0.0f;
          for (int k = i - 1; k <= i + 1; k += 2) {
            if (k < 0 || k >= gps->totpoints) continue;
            /* neighbour lying against the drag: (pk - pi) . drag < 0 */
            const float dot = (gps->points[k].x - pt->x) * vp->dx + (gps->points[k].y - pt->y) * vp->dy;
            if (dot < best) { best = dot; src = k; }
          }
          if (src >= 0 && orig[src][3] > 0.0f) {
            pge_interp_v3(pt->vert_color, pt->vert_color, orig[src], f);
            pt->vert_color[3] = pt->vert_color[3] + (orig[src][3] - pt->vert_color[3]) * f;
            changed = 1;
          }
          break;
        }
      }
    }
    free(orig);
    /* fill color follows the strongest influence on the stroke (Draw/Replace only) */
    if (do_fill && stroke_max > 0.0f &&
        (vp->brush == PG_VPAINT_DRAW || (vp->brush == PG_VPAINT_REPLACE && gps->vert_color_fill[3] > 0.0f)))
    {
      if (vp->brush == PG_VPAINT_DRAW) {
        pge_ensure_vcolor(gps->vert_color_fill, gp_style ? gp_style->fill_rgba : NULL, vp->rgb);
        pge_mix_toward(gps->vert_color_fill, vp->rgb, stroke_max);
      }
      else {
        pge_interp_v3(gps->vert_color_fill, gps->vert_color_fill, vp->rgb, stroke_max);
      }
      changed = 1;
    }
  }
  PGE_EDITABLE_STROKES_END;
  return changed;
}
/* ---------------------------------------------------------------------------------------- */

int pg_gp_edit_dispatch(bGPdata *gpd,
                        bGPDlayer *active_layer,
                        int command,
                        const float *args,
                        int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) {
    return 0;
  }
  for (int i = 0; i < arg_count; i++) {
    if (!isfinite(args[i])) {
      return 0;
    }
  }

  /* Same scoping rule as the selection commands: the active layer. */
  const bGPDlayer *scope = active_layer;
  int changed = 0;

  switch (command) {
    case PG_EDIT_CMD_PICK:
      if (arg_count < 5) {
        return 0;
      }
      changed = pg_gp_edit_pick(gpd,
                                scope,
                                args[0],
                                args[1],
                                (int)lroundf(args[2]),
                                (int)lroundf(args[3]),
                                (int)lroundf(args[4]));
      break;
    case PG_EDIT_CMD_TRANSLATE:
      if (arg_count < 2) {
        return 0;
      }
      changed = pg_gp_edit_translate(gpd, scope, args[0], args[1]);
      break;
    case PG_EDIT_CMD_ROTATE:
      if (arg_count < 1) {
        return 0;
      }
      changed = pg_gp_edit_rotate(gpd, scope, args[0], arg_count >= 3 ? &args[1] : NULL);
      break;
    case PG_EDIT_CMD_SCALE:
      if (arg_count < 2) {
        return 0;
      }
      changed = pg_gp_edit_scale(gpd, scope, args[0], args[1], arg_count >= 4 ? &args[2] : NULL);
      break;
    case PG_EDIT_CMD_MIRROR:
      if (arg_count < 2) {
        return 0;
      }
      changed = pg_gp_edit_mirror(
          gpd, scope, args[0] != 0.0f, args[1] != 0.0f, arg_count >= 4 ? &args[2] : NULL);
      break;
    case PG_EDIT_CMD_DELETE_STROKES:
      changed = pg_gp_edit_delete_strokes(gpd, scope);
      break;
    case PG_EDIT_CMD_DELETE_POINTS:
      changed = pg_gp_edit_delete_points(gpd, scope);
      break;
    case PG_EDIT_CMD_MOD_THICKNESS:
      if (arg_count < 3) {
        return 0;
      }
      changed = pg_gp_mod_thickness(gpd, scope, args[0] != 0.0f, (int)lroundf(args[1]), args[2]);
      break;
    case PG_EDIT_CMD_MOD_OPACITY:
      if (arg_count < 4) {
        return 0;
      }
      changed = pg_gp_mod_opacity(
          gpd, scope, (int)lroundf(args[0]), args[1], args[2] != 0.0f, args[3]);
      break;
    case PG_EDIT_CMD_MOD_LENGTH: {
      if (arg_count < 9) {
        return 0;
      }
      const PGLengthParams lp = {(int)lroundf(args[0]), args[1], args[2], args[3],
                                 args[4] != 0.0f, args[5], args[6], args[7], args[8] != 0.0f};
      changed = pg_gp_mod_length(gpd, scope, &lp);
      break;
    }
    case PG_EDIT_CMD_MOD_TINT: {
      if (arg_count < 5) {
        return 0;
      }
      const float rgb[3] = {args[2], args[3], args[4]};
      changed = pg_gp_mod_tint(gpd, scope, (int)lroundf(args[0]), args[1], rgb);
      break;
    }
    case PG_EDIT_CMD_MOD_COLOR: {
      if (arg_count < 4) {
        return 0;
      }
      const float hsv[3] = {args[1], args[2], args[3]};
      changed = pg_gp_mod_color(gpd, scope, (int)lroundf(args[0]), hsv);
      break;
    }
    case PG_EDIT_CMD_ARRANGE:
      if (arg_count < 1) return 0;
      changed = pg_gp_stroke_arrange(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT_CMD_SET_MATERIAL:
      if (arg_count < 1) return 0;
      changed = pg_gp_stroke_set_material(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT_CMD_RESET_VCOLOR:
      if (arg_count < 1) return 0;
      changed = pg_gp_stroke_reset_vertex_color(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT_CMD_FLIP:
      changed = pg_gp_stroke_flip(gpd, scope);
      break;
    case PG_EDIT_CMD_CYCLIC:
      if (arg_count < 1) return 0;
      changed = pg_gp_stroke_cyclical_set(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT_CMD_SNAP_GRID:
      if (arg_count < 1) return 0;
      changed = pg_gp_snap_to_grid(gpd, scope, args[0]);
      break;
    case PG_EDIT_CMD_DUPLICATE:
      changed = pg_gp_duplicate(gpd, scope);
      break;
    case PG_EDIT_CMD_DISSOLVE:
      if (arg_count < 1) return 0;
      changed = pg_gp_dissolve(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT_CMD_SPLIT:
      changed = pg_gp_split(gpd, scope);
      break;
    case PG_EDIT_CMD_JOIN:
      changed = pg_gp_join(gpd, scope, arg_count >= 1 && args[0] != 0.0f);
      break;
    case PG_EDIT_CMD_VERTEX_PAINT: {
      if (arg_count < 11) return 0;
      const PGVertexPaint vp = {(int)lroundf(args[0]), args[1], args[2], args[3], args[4],
                                {args[5], args[6], args[7]}, (int)lroundf(args[8]), args[9], args[10]};
      changed = pg_gp_vertex_paint(gpd, scope, &vp);
      break;
    }
    default:
      return 0;
  }

  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed ? 1 : 0;
}
