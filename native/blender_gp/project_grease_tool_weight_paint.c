/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Weight Paint mode brushes for the native tool session: gpencil_weight_paint.c (Blender 3.6.23).
 *
 * Verbatim: the operator context, the selected-point and find-nearest buffers (kd-tree), the
 * Draw, Average, Blur and Smear callbacks and the per-sample selection of the points under the
 * brush. Adapted (no bContext / depsgraph / RNA / armature): vertex group ensure (the session is
 * given the group), auto-normalize (no bone-deformed groups, so never active), do_frame,
 * apply_to_layers, brush_apply, init and exit. The view is the canvas plane
 * (project_grease_tool_util.c), matrices are identity.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_tool_brushes.h"
#include "project_grease_tool_view.h"

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_weight_paint.c */
/* ************************************************ */
/* General Brush Editing Context */
#define GP_SELECT_BUFFER_CHUNK 256
#define GP_FIND_NEAREST_BUFFER_CHUNK 1024
#define GP_FIND_NEAREST_EPSILON 1e-6f
#define GP_STROKE_HASH_BITSHIFT 16

/* Grid of Colors for Smear. */
typedef struct tGP_Grid {
  /** Lower right corner of rectangle of grid cell. */
  float bottom[2];
  /** Upper left corner of rectangle of grid cell. */
  float top[2];
  /** Average Color */
  float color[4];
  /** Total points included. */
  int totcol;

} tGP_Grid;

/* List of points affected by brush. */
typedef struct tGP_Selected {
  /** Referenced stroke. */
  bGPDstroke *gps;
  /** Point index in points array. */
  int pt_index;
  /** Position. */
  int pc[2];
  /** Color. */
  float color[4];
  /** Weight. */
  float weight;
} tGP_Selected;

/* Context for brush operators */
typedef struct tGP_BrushWeightpaintData {
  struct Main *bmain;
  Scene *scene;
  Object *object;

  ARegion *region;

  /* Current GPencil datablock */
  bGPdata *gpd;

  Brush *brush;

  /* Space Conversion Data */
  GP_SpaceConversion gsc;

  /* Is the brush currently painting? */
  bool is_painting;

  /* Start of new paint */
  bool first;

  /* Is multi-frame editing enabled, and are we using falloff for that? */
  bool is_multiframe;
  bool use_multiframe_falloff;

  /* Draw tool: add or subtract? */
  bool subtract;

  /* Auto-normalize weights of bone-deformed vertices? */
  bool auto_normalize;

  /* Active vertex group */
  int vrgroup;

  /* Brush Runtime Data: */
  /* - position and pressure
   * - the *_prev variants are the previous values
   */
  float mouse[2], mouse_prev[2];
  float pressure;

  /* - Brush direction. */
  float brush_dir[2];
  bool brush_dir_is_set;

  /* - Multi-frame falloff factor. */
  float mf_falloff;

  /* Brush geometry (bounding box). */
  rcti brush_rect;

  /* Temp data to save selected points */
  /** Stroke buffer. */
  tGP_Selected *pbuffer;
  /** Number of elements currently used in cache. */
  int pbuffer_used;
  /** Number of total elements available in cache. */
  int pbuffer_size;
  /** Average weight of elements in cache (used for average tool). */
  float pbuffer_avg_weight;

  /* Temp data for find-nearest-points, used by blur and smear tool. */
  bool use_find_nearest;
  /** Buffer of stroke points during one mouse swipe. */
  tGP_Selected *fn_pbuffer;
  /** Hash table of added points (to avoid duplicate entries). */
  GHash *fn_added;
  /** KDtree for finding nearest points. */
  KDTree_2d *fn_kdtree;
  /** Number of points used in find-nearest set. */
  uint fn_used;
  /** Number of points available in find-nearest set. */
  uint fn_size;
  /** Flag for balancing kdtree. */
  bool fn_do_balance;

  /* Temp data for auto-normalize weights used by deforming bones. */
  /** Boolean array of locked vertex groups. */
  bool *vgroup_locked;
  /** Boolean array of vertex groups deformed by bones. */
  bool *vgroup_bone_deformed;
  /** Number of vertex groups in object. */
  int vgroup_tot;

} tGP_BrushWeightpaintData;

/* Ensure the buffer to hold temp selected point size is enough to save all points selected. */
static void gpencil_select_buffer_ensure(tGP_BrushWeightpaintData *gso, const bool clear)
{
  /* By default a buffer is created with one block with a predefined number of free slots,
   * if the size is not enough, the cache is reallocated adding a new block of free slots.
   * This is done in order to keep cache small and improve speed. */
  if ((gso->pbuffer_used + 1) > gso->pbuffer_size) {
    if ((gso->pbuffer_size == 0) || (gso->pbuffer == NULL)) {
      gso->pbuffer = MEM_callocN(sizeof(struct tGP_Selected) * GP_SELECT_BUFFER_CHUNK, __func__);
      gso->pbuffer_size = GP_SELECT_BUFFER_CHUNK;
    }
    else {
      gso->pbuffer_size += GP_SELECT_BUFFER_CHUNK;
      gso->pbuffer = MEM_recallocN(gso->pbuffer, sizeof(struct tGP_Selected) * gso->pbuffer_size);
    }
  }

  /* Clear old data. */
  if (clear) {
    gso->pbuffer_used = 0;
    if (gso->pbuffer != NULL) {
      memset(gso->pbuffer, 0, sizeof(tGP_Selected) * gso->pbuffer_size);
    }
  }

  /* Create or enlarge buffer for find-nearest-points. */
  if (gso->use_find_nearest && ((gso->fn_used + 1) > gso->fn_size)) {
    gso->fn_size += GP_FIND_NEAREST_BUFFER_CHUNK;

    /* Stroke point buffer. */
    if (gso->fn_pbuffer == NULL) {
      gso->fn_pbuffer = MEM_callocN(sizeof(struct tGP_Selected) * gso->fn_size, __func__);
    }
    else {
      gso->fn_pbuffer = MEM_recallocN(gso->fn_pbuffer, sizeof(struct tGP_Selected) * gso->fn_size);
    }

    /* Stroke point hash table (for duplicate checking.) */
    if (gso->fn_added == NULL) {
      gso->fn_added = BLI_ghash_int_new("GP weight paint find nearest");
    }

    /* KDtree of stroke points. */
    bool do_tree_rebuild = false;
    if (gso->fn_kdtree != NULL) {
      BLI_kdtree_2d_free(gso->fn_kdtree);
      do_tree_rebuild = true;
    }
    gso->fn_kdtree = BLI_kdtree_2d_new(gso->fn_size);

    if (do_tree_rebuild) {
      for (int i = 0; i < gso->fn_used; i++) {
        float pc_f[2];
        copy_v2fl_v2i(pc_f, gso->fn_pbuffer[i].pc);
        BLI_kdtree_2d_insert(gso->fn_kdtree, i, pc_f);
      }
    }
  }
}
/* END VERBATIM */

/* gpencil_vertex_group_ensure(): the session is started with an existing vertex group. */
static void gpencil_vertex_group_ensure(tGP_BrushWeightpaintData *gso)
{
  (void)gso;
}

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_weight_paint.c */
static void gpencil_select_buffer_avg_weight_set(tGP_BrushWeightpaintData *gso)
{
  if (gso->pbuffer_used == 0) {
    gso->pbuffer_avg_weight = 0.0f;
    return;
  }
  float sum = 0;
  for (int i = 0; i < gso->pbuffer_used; i++) {
    tGP_Selected *selected = &gso->pbuffer[i];
    sum += selected->weight;
  }
  gso->pbuffer_avg_weight = sum / gso->pbuffer_used;
  CLAMP(gso->pbuffer_avg_weight, 0.0f, 1.0f);
}
/* END VERBATIM */

/* do_weight_paint_normalize_try(): auto-normalize only applies to groups deformed by armature
 * bones; Project Grease has no armatures, so auto_normalize is never set. */
static void do_weight_paint_normalize_try(MDeformVert *dvert, tGP_BrushWeightpaintData *gso)
{
  (void)dvert;
  (void)gso;
}

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_weight_paint.c */
/* Compute strength of effect. */
static float brush_influence_calc(tGP_BrushWeightpaintData *gso, const int radius, const int co[2])
{
  Brush *brush = gso->brush;

  /* basic strength factor from brush settings */
  float influence = brush->alpha;

  /* use pressure? */
  if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
    influence *= gso->pressure;
  }

  /* distance fading */
  int mouse_i[2];
  round_v2i_v2fl(mouse_i, gso->mouse);
  float distance = (float)len_v2v2_int(mouse_i, co);

  /* Apply Brush curve. */
  float brush_falloff = BKE_brush_curve_strength(brush, distance, (float)radius);
  influence *= brush_falloff;

  /* apply multi-frame falloff */
  influence *= gso->mf_falloff;

  /* return influence */
  return influence;
}

/* Compute effect vector for directional brushes. */
static void brush_calc_brush_dir_2d(tGP_BrushWeightpaintData *gso)
{
  sub_v2_v2v2(gso->brush_dir, gso->mouse, gso->mouse_prev);

  /* Skip tiny changes in direction, we want the bigger movements only. */
  if (len_squared_v2(gso->brush_dir) < 9.0f) {
    return;
  }

  normalize_v2(gso->brush_dir);

  gso->brush_dir_is_set = true;
  copy_v2_v2(gso->mouse_prev, gso->mouse);
}

/* ************************************************ */
/* Brush Callbacks
 * This section defines the callbacks used by each brush to perform their magic.
 * These are called on each point within the brush's radius. */

/* Draw Brush */
static bool brush_draw_apply(tGP_BrushWeightpaintData *gso,
                             bGPDstroke *gps,
                             int pt_index,
                             const int radius,
                             const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == NULL) {
    return false;
  }

  /* Apply brush weight. */
  float bweight = (gso->subtract) ? -gso->brush->weight : gso->brush->weight;
  dw->weight = interpf(bweight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* Average Brush */
static bool brush_average_apply(tGP_BrushWeightpaintData *gso,
                                bGPDstroke *gps,
                                int pt_index,
                                const int radius,
                                const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == NULL) {
    return false;
  }

  /* Blend weight with average weight under the brush. */
  dw->weight = interpf(gso->pbuffer_avg_weight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* Blur Brush */
static bool brush_blur_apply(tGP_BrushWeightpaintData *gso,
                             bGPDstroke *gps,
                             int pt_index,
                             const int radius,
                             const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == NULL) {
    return false;
  }

  /* Find the 5 nearest points (this includes the to-be-blurred point itself). */
  KDTreeNearest_2d nearest[5];
  float pc_f[2];
  copy_v2fl_v2i(pc_f, co);
  const int tot = BLI_kdtree_2d_find_nearest_n(gso->fn_kdtree, pc_f, nearest, 5);

  /* Calculate the average (=blurred) weight. */
  float blur_weight = 0.0f, dist_sum = 0.0f;
  int count = 0;
  for (int i = 0; i < tot; i++) {
    dist_sum += nearest[i].dist;
    count++;
  }
  if (count <= 1) {
    return false;
  }
  for (int i = 0; i < tot; i++) {
    /* Weighted average, based on distance to point. */
    blur_weight += (1.0f - nearest[i].dist / dist_sum) * gso->fn_pbuffer[nearest[i].index].weight;
  }
  blur_weight /= (count - 1);

  /* Blend weight with blurred weight. */
  dw->weight = interpf(blur_weight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* Smear Brush */
static bool brush_smear_apply(tGP_BrushWeightpaintData *gso,
                              bGPDstroke *gps,
                              int pt_index,
                              const int radius,
                              const int co[2])
{
  MDeformVert *dvert = gps->dvert + pt_index;

  /* Get current weight. */
  MDeformWeight *dw = BKE_defvert_ensure_index(dvert, gso->vrgroup);
  if (dw == NULL) {
    return false;
  }

  /* Find the 8 nearest points (this includes the to-be-blurred point itself). */
  KDTreeNearest_2d nearest[8];
  float pc_f[2];
  copy_v2fl_v2i(pc_f, co);
  const int tot = BLI_kdtree_2d_find_nearest_n(gso->fn_kdtree, pc_f, nearest, 8);

  /* For smearing a weight from point A to point B, we look for a point A 'behind' the brush,
   * matching the brush angle best and with the shortest distance to B. */
  float point_dot[8] = {0};
  float point_dir[2];
  float score_max = 0.0f, dist_min = FLT_MAX, dist_max = 0.0f;
  int i_max = -1, count = 0;

  for (int i = 0; i < tot; i++) {
    /* Skip the point we are about to smear. */
    if (nearest[i].dist > GP_FIND_NEAREST_EPSILON) {
      sub_v2_v2v2(point_dir, pc_f, nearest[i].co);
      normalize_v2(point_dir);

      /* Match A-B direction with brush direction. */
      point_dot[i] = dot_v2v2(point_dir, gso->brush_dir);
      if (point_dot[i] > 0.0f) {
        count++;
        float dist = nearest[i].dist;
        if (dist < dist_min) {
          dist_min = dist;
        }
        if (dist > dist_max) {
          dist_max = dist;
        }
      }
    }
  }
  if (count == 0) {
    return false;
  }

  /* Find best match in angle and distance. */
  float dist_f = (dist_min == dist_max) ? 1.0f : 0.95f / (dist_max - dist_min);
  for (int i = 0; i < tot; i++) {
    if (point_dot[i] > 0.0f) {
      float score = point_dot[i] * (1.0f - (nearest[i].dist - dist_min) * dist_f);
      if (score > score_max) {
        score_max = score;
        i_max = i;
      }
    }
  }
  if (i_max == -1) {
    return false;
  }

  /* Compute strength of effect. */
  float inf = brush_influence_calc(gso, radius, co);

  /* Smear the weight. */
  dw->weight = interpf(gso->fn_pbuffer[nearest[i_max].index].weight, dw->weight, inf);
  CLAMP(dw->weight, 0.0f, 1.0f);

  /* Perform auto-normalize. */
  if (gso->auto_normalize) {
    do_weight_paint_normalize_try(dvert, gso);
  }

  return true;
}

/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_weight_paint.c */
/* Helper to save the points selected by the brush. */
static void gpencil_save_selected_point(tGP_BrushWeightpaintData *gso,
                                        bGPDstroke *gps,
                                        const int gps_index,
                                        const int index,
                                        const int pc[2],
                                        const bool within_brush)
{
  tGP_Selected *selected;
  bGPDspoint *pt = &gps->points[index];

  /* Ensure the array to save the list of selected points is big enough. */
  gpencil_select_buffer_ensure(gso, false);

  /* Copy point data. */
  if (within_brush) {
    selected = &gso->pbuffer[gso->pbuffer_used];
    selected->gps = gps;
    selected->pt_index = index;
    copy_v2_v2_int(selected->pc, pc);
    copy_v4_v4(selected->color, pt->vert_color);
    gso->pbuffer_used++;
  }

  /* Ensure vertex group and dvert. */
  gpencil_vertex_group_ensure(gso);
  BKE_gpencil_dvert_ensure(gps);

  /* Copy current weight. */
  MDeformVert *dvert = gps->dvert + index;
  MDeformWeight *dw = BKE_defvert_find_index(dvert, gso->vrgroup);
  if (within_brush && (dw != NULL)) {
    selected->weight = dw->weight;
  }

  /* Store point for finding nearest points (blur, smear). */
  if (gso->use_find_nearest) {
    /* Create hash key, assuming there are no more than 65536 strokes in a frame
     * and 65536 points in a stroke. */
    const int point_hash = (gps_index << GP_STROKE_HASH_BITSHIFT) + index;

    /* Prevent duplicate points in buffer. */
    if (!BLI_ghash_haskey(gso->fn_added, POINTER_FROM_INT(point_hash))) {
      /* Add stroke point to find-nearest buffer. */
      selected = &gso->fn_pbuffer[gso->fn_used];
      copy_v2_v2_int(selected->pc, pc);
      selected->weight = (dw == NULL) ? 0.0f : dw->weight;

      BLI_ghash_insert(
          gso->fn_added, POINTER_FROM_INT(point_hash), POINTER_FROM_INT(gso->fn_used));

      float pc_f[2];
      copy_v2_fl2(pc_f, (float)pc[0], (float)pc[1]);
      BLI_kdtree_2d_insert(gso->fn_kdtree, gso->fn_used, pc_f);

      gso->fn_do_balance = true;
      gso->fn_used++;
    }
    else {
      /* Update weight of point in buffer. */
      int *idx = BLI_ghash_lookup(gso->fn_added, POINTER_FROM_INT(point_hash));
      selected = &gso->fn_pbuffer[POINTER_AS_INT(idx)];
      selected->weight = (dw == NULL) ? 0.0f : dw->weight;
    }
  }
}

/* Select points in this stroke and add to an array to be used later. */
static void gpencil_weightpaint_select_stroke(tGP_BrushWeightpaintData *gso,
                                              bGPDstroke *gps,
                                              const int gps_index,
                                              const float diff_mat[4][4],
                                              const float bound_mat[4][4])
{
  GP_SpaceConversion *gsc = &gso->gsc;
  rcti *rect = &gso->brush_rect;
  Brush *brush = gso->brush;
  /* For the blur tool, look a bit wider than the brush itself,
   * because we need the weight of surrounding points to perform the blur. */
  const bool widen_brush = (gso->brush->gpencil_weight_tool == GPWEIGHT_TOOL_BLUR);
  int radius_brush = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;
  int radius_wide = (widen_brush) ? radius_brush * 1.3f : radius_brush;
  bool within_brush = true;

  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = NULL;

  bGPDspoint *pt1, *pt2;
  bGPDspoint *pt = NULL;
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int index;
  bool include_last = false;

  /* Check if the stroke collides with brush. */
  if (!ED_gpencil_stroke_check_collision(gsc, gps, gso->mouse, radius_wide, bound_mat)) {
    return;
  }

  if (gps->totpoints == 1) {
    bGPDspoint pt_temp;
    pt = &gps->points[0];
    gpencil_point_to_world_space(gps->points, diff_mat, &pt_temp);
    gpencil_point_to_xy(gsc, gps, &pt_temp, &pc1[0], &pc1[1]);

    pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
    /* Do bound-box check first. */
    if (!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
      /* only check if point is inside */
      int mouse_i[2];
      round_v2i_v2fl(mouse_i, gso->mouse);
      int mlen = len_v2v2_int(mouse_i, pc1);
      if (mlen <= radius_wide) {
        /* apply operation to this point */
        if (pt_active != NULL) {
          if (widen_brush) {
            within_brush = (mlen <= radius_brush);
          }
          gpencil_save_selected_point(gso, gps_active, gps_index, 0, pc1, within_brush);
        }
      }
    }
  }
  else {
    /* Loop over the points in the stroke, checking for intersections
     * - an intersection means that we touched the stroke
     */
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* Get points to work with */
      pt1 = gps->points + i;
      pt2 = gps->points + i + 1;

      bGPDspoint npt;
      gpencil_point_to_world_space(pt1, diff_mat, &npt);
      gpencil_point_to_xy(gsc, gps, &npt, &pc1[0], &pc1[1]);

      gpencil_point_to_world_space(pt2, diff_mat, &npt);
      gpencil_point_to_xy(gsc, gps, &npt, &pc2[0], &pc2[1]);

      /* Check that point segment of the bound-box of the selection stroke */
      if ((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
          (!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1]) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1])))
      {
        /* Check if point segment of stroke had anything to do with
         * brush region  (either within stroke painted, or on its lines)
         * - this assumes that line-width is irrelevant.
         */
        if (gpencil_stroke_inside_circle(gso->mouse, radius_wide, pc1[0], pc1[1], pc2[0], pc2[1]))
        {
          if (widen_brush) {
            within_brush = (gpencil_stroke_inside_circle(
                gso->mouse, radius_brush, pc1[0], pc1[1], pc2[0], pc2[1]));
          }

          /* To each point individually... */
          pt = &gps->points[i];
          pt_active = pt->runtime.pt_orig;
          if (pt_active != NULL) {
            index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
            gpencil_save_selected_point(gso, gps_active, gps_index, index, pc1, within_brush);
          }

          /* Only do the second point if this is the last segment,
           * and it is unlikely that the point will get handled
           * otherwise.
           *
           * NOTE: There is a small risk here that the second point wasn't really
           *       actually in-range. In that case, it only got in because
           *       the line linking the points was!
           */
          if (i + 1 == gps->totpoints - 1) {
            pt = &gps->points[i + 1];
            pt_active = pt->runtime.pt_orig;
            if (pt_active != NULL) {
              index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i + 1;
              gpencil_save_selected_point(gso, gps_active, gps_index, index, pc2, within_brush);
              include_last = false;
            }
          }
          else {
            include_last = true;
          }
        }
        else if (include_last) {
          /* This case is for cases where for whatever reason the second vert (1st here)
           * doesn't get included because the whole edge isn't in bounds,
           * but it would've qualified since it did with the previous step
           * (but wasn't added then, to avoid double-ups).
           */
          pt = &gps->points[i];
          pt_active = pt->runtime.pt_orig;
          if (pt_active != NULL) {
            index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
            gpencil_save_selected_point(gso, gps_active, gps_index, index, pc1, true);
            include_last = false;
          }
        }
      }
    }
  }
}
/* END VERBATIM */

/* ---- Project Grease adaptation of the operator loop ---------------------------------------- */

static bool pgt_wpaint_do_frame(tGP_BrushWeightpaintData *gso, bGPDframe *gpf)
{
  char tool = gso->brush->gpencil_weight_tool;
  const int radius = (gso->brush->flag & GP_BRUSH_USE_PRESSURE) ?
                         gso->brush->size * gso->pressure :
                         gso->brush->size;
  tGP_Selected *selected = NULL;
  gso->fn_do_balance = false;
  float diff_mat[4][4], bound_mat[4][4];
  unit_m4(diff_mat);
  unit_m4(bound_mat);

  /* First step: select the points affected (ED_gpencil_stroke_material_visible: not hidden). */
  int gps_index;
  LISTBASE_FOREACH_INDEX (bGPDstroke *, gps, &gpf->strokes, gps_index) {
    if (gps->points == NULL || gps->totpoints <= 0) {
      continue;
    }
    if (pg_tool_material_style(gso->gpd, gps->mat_nr + 1)->flag & GP_MATERIAL_HIDE) {
      continue;
    }
    gpencil_weightpaint_select_stroke(gso, gps, gps_index, diff_mat, bound_mat);
  }

  bDeformGroup *defgroup = BLI_findlink(&gso->gpd->vertex_group_names, gso->vrgroup);
  if ((defgroup == NULL) || (defgroup->flag & DG_LOCK_WEIGHT)) {
    gpencil_select_buffer_ensure(gso, true);
    return false;
  }

  /* Second step: calculations on selected points. */
  if (tool == GPWEIGHT_TOOL_AVERAGE) {
    gpencil_select_buffer_avg_weight_set(gso);
  }
  if (gso->use_find_nearest && gso->fn_do_balance) {
    BLI_kdtree_2d_balance(gso->fn_kdtree);
  }

  /* Third step: apply effect. */
  bool changed = false;
  for (int i = 0; i < gso->pbuffer_used; i++) {
    selected = &gso->pbuffer[i];
    switch (tool) {
      case GPWEIGHT_TOOL_DRAW:
        changed |= brush_draw_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      case GPWEIGHT_TOOL_AVERAGE:
        changed |= brush_average_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      case GPWEIGHT_TOOL_BLUR:
        changed |= brush_blur_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      case GPWEIGHT_TOOL_SMEAR:
        changed |= brush_smear_apply(gso, selected->gps, selected->pt_index, radius, selected->pc);
        break;
      default:
        break;
    }
  }
  gpencil_select_buffer_ensure(gso, true);
  return changed;
}

static bool pgt_wpaint_apply_to_layers(tGP_BrushWeightpaintData *gso)
{
  bool changed = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gso->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }
    if (gso->is_multiframe) {
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        if ((gpf == gpl->actframe) || (gpf->flag & GP_FRAME_SELECT)) {
          gso->mf_falloff = 1.0f;
          changed |= pgt_wpaint_do_frame(gso, gpf);
        }
      }
    }
    else {
      gso->mf_falloff = 1.0f;
      changed |= pgt_wpaint_do_frame(gso, gpl->actframe);
    }
  }
  return changed;
}

struct PGWeightPaintSession {
  tGP_BrushWeightpaintData gso;
  Brush brush;
  BrushGpencilSettings settings;
  PGToolView view;
};

/* gpencil_weightpaint_brush_init() with the weight presets of BKE_gpencil_brush_preset_set(). */
PGWeightPaintSession *pg_wpaint_session_begin(bGPdata *gpd, const PGToolBrushParams *params)
{
  if (gpd == NULL || params == NULL || params->brush < GPWEIGHT_TOOL_DRAW ||
      params->brush > GPWEIGHT_TOOL_SMEAR || !(params->radius > 0.0f) || params->target < 0 ||
      BLI_findlink(&gpd->vertex_group_names, params->target) == NULL)
  {
    return NULL;
  }
  PGWeightPaintSession *s = MEM_callocN(sizeof(PGWeightPaintSession), "PGWeightPaintSession");
  tGP_BrushWeightpaintData *gso = &s->gso;
  Brush *brush = &s->brush;
  brush->gpencil_settings = &s->settings;
  brush->gpencil_weight_tool = (char)params->brush;
  brush->curve_preset = BRUSH_CURVE_SMOOTH;
  s->settings.flag |= GP_BRUSH_USE_PRESSURE;
  pg_tool_view_init(&s->view, &gso->gsc, params->px_per_unit);
  brush->size = max_ii(1, (int)lroundf(params->radius * s->view.px_per_unit));
  brush->alpha = clamp_f(params->strength, 0.0f, 1.0f);
  brush->weight = clamp_f(params->weight, 0.0f, 1.0f);

  gso->brush = brush;
  gso->gpd = gpd;
  gso->gsc.gpd = gpd;
  gso->scene = &s->view.scene;
  gso->object = &s->view.ob;
  gso->region = &s->view.region;
  gso->is_painting = false;
  gso->first = true;
  gso->vrgroup = params->target;
  /* Draw: add, or subtract when inverted (BRUSH_DIR_IN / Ctrl). */
  gso->subtract = params->invert != 0;
  gso->auto_normalize = false;
  gso->is_multiframe = GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  gso->use_find_nearest = ELEM(brush->gpencil_weight_tool, GPWEIGHT_TOOL_BLUR, GPWEIGHT_TOOL_SMEAR);
  gso->fn_pbuffer = NULL;
  gso->fn_added = NULL;
  gso->fn_kdtree = NULL;
  gso->fn_used = 0;
  gso->fn_size = 0;
  gso->fn_do_balance = false;
  pg_tool_link_runtime(gpd, true);
  return s;
}

/* gpencil_weightpaint_brush_apply(): one input event. */
int pg_wpaint_session_sample(PGWeightPaintSession *s, float x, float y, float pressure)
{
  if (s == NULL || !isfinite(x) || !isfinite(y)) {
    return 0;
  }
  tGP_BrushWeightpaintData *gso = &s->gso;
  Brush *brush = gso->brush;
  const float scale = s->view.px_per_unit;
  int mouse[2];
  gso->mouse[0] = mouse[0] = (int)(x * scale);
  gso->mouse[1] = mouse[1] = (int)(y * scale);
  gso->pressure = (pressure >= 0.99f) ? 1.0f : clamp_f(pressure, 0.0f, 1.0f);
  const int radius = ((brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                              gso->brush->size);
  if (gso->first) {
    gso->mouse_prev[0] = gso->mouse[0];
    gso->mouse_prev[1] = gso->mouse[1];
    gso->brush_dir_is_set = false;
  }
  gso->first = false;
  gso->brush_rect.xmin = mouse[0] - radius;
  gso->brush_rect.ymin = mouse[1] - radius;
  gso->brush_rect.xmax = mouse[0] + radius;
  gso->brush_rect.ymax = mouse[1] + radius;
  if (gso->brush->gpencil_weight_tool == GPWEIGHT_TOOL_SMEAR) {
    brush_calc_brush_dir_2d(gso);
    if (!gso->brush_dir_is_set) {
      return 0;
    }
  }
  const bool changed = pgt_wpaint_apply_to_layers(gso);
  if (changed) {
    gso->gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gso->gpd);
  }
  return changed ? 1 : 0;
}

/* gpencil_weightpaint_brush_exit(). */
void pg_wpaint_session_end(PGWeightPaintSession *s)
{
  if (s == NULL) {
    return;
  }
  tGP_BrushWeightpaintData *gso = &s->gso;
  MEM_SAFE_FREE(gso->pbuffer);
  MEM_SAFE_FREE(gso->fn_pbuffer);
  if (gso->fn_added != NULL) {
    BLI_ghash_free(gso->fn_added, NULL, NULL);
  }
  if (gso->fn_kdtree != NULL) {
    BLI_kdtree_2d_free(gso->fn_kdtree);
  }
  pg_tool_link_runtime(gso->gpd, false);
  MEM_freeN(s);
}
