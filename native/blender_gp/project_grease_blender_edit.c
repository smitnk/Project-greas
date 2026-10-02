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
  }
  PGE_EDITABLE_STROKES_END;
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
    /* Hardness (at stroke level). */
    if (modify_color == PG_MODIFY_COLOR_HARDNESS) {
      gps->hardeness *= hardness;
      gps->hardeness = pge_clampf(gps->hardeness, 0.0f, 1.0f);
      changed = 1;
      continue;
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
    default:
      return 0;
  }

  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed ? 1 : 0;
}
