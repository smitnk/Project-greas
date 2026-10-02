/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_blender_edit5.h. */
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit5.h"

int ED_gpencil_select_stroke_segment(bGPdata *gpd,
                                     bGPDlayer *gpl,
                                     bGPDstroke *gps,
                                     bGPDspoint *pt,
                                     bool select,
                                     bool insert,
                                     const float scale,
                                     float r_hita[3],
                                     float r_hitb[3]);

/* BKE_gpencil_stroke_2d_flat_ref() builds its 2D frame from the reference stroke's first points;
 * for a perfectly straight stroke (common here: Line tool, imports) that frame degenerates and
 * every stroke projects onto one line, so every segment "collides". Project Grease strokes lie in
 * the XY plane, which is the plane flat_ref recovers for any non-straight stroke, so the region
 * below uses the same projection on fixed X/Y axes (same origin, same extreme scaling). */
static void pg4_stroke_2d_xy(const bGPDspoint *ref_points,
                             int ref_totpoints,
                             const bGPDspoint *points,
                             int totpoints,
                             float (*points2d)[2],
                             const float scale,
                             int *r_direction)
{
  (void)ref_totpoints;
  const bGPDspoint *pt0 = &ref_points[0];
  for (int i = 0; i < totpoints; i++) {
    const bGPDspoint *pt = &points[i];
    float v1[3], vn[3] = {0.0f, 0.0f, 0.0f};
    if (i == 0 || i == totpoints - 1) {
      const bGPDspoint *other = (i == 0) ? &points[i + 1] : &points[i - 1];
      sub_v3_v3v3(vn, &pt->x, &other->x);
      normalize_v3(vn);
      mul_v3_fl(vn, scale / 10.0f);
      add_v3_v3v3(v1, &pt->x, vn);
    }
    else {
      copy_v3_v3(v1, &pt->x);
    }
    points2d[i][0] = v1[0] - pt0->x;
    points2d[i][1] = v1[1] - pt0->y;
  }
  *r_direction = 0;
}
#define BKE_gpencil_stroke_2d_flat_ref pg4_stroke_2d_xy

/* gpencil_utils.c: collision helpers and ED_gpencil_select_stroke_segment(). */
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
static bool gpencil_check_collision(bGPDstroke *gps,
                                    bGPDstroke **gps_array,
                                    GHash *all_2d,
                                    int totstrokes,
                                    const float p2d_a1[2],
                                    const float p2d_a2[2],
                                    float r_hit[2])
{
  bool hit = false;
  /* check segment with all segments of all strokes */
  for (int s = 0; s < totstrokes; s++) {
    bGPDstroke *gps_iter = gps_array[s];
    if (gps_iter->totpoints < 2) {
      continue;
    }
    /* get stroke 2d version */
    float(*points2d)[2] = BLI_ghash_lookup(all_2d, gps_iter);

    for (int i2 = 0; i2 < gps_iter->totpoints - 1; i2++) {
      float p2d_b1[2], p2d_b2[2];
      copy_v2_v2(p2d_b1, points2d[i2]);
      copy_v2_v2(p2d_b2, points2d[i2 + 1]);

      /* don't self check */
      if (gps == gps_iter) {
        if (equals_v2v2(p2d_a1, p2d_b1) || equals_v2v2(p2d_a1, p2d_b2)) {
          continue;
        }
        if (equals_v2v2(p2d_a2, p2d_b1) || equals_v2v2(p2d_a2, p2d_b2)) {
          continue;
        }
      }
      /* check collision */
      int check = isect_seg_seg_v2_point(p2d_a1, p2d_a2, p2d_b1, p2d_b2, r_hit);
      if (check > 0) {
        hit = true;
        break;
      }
    }

    if (hit) {
      break;
    }
  }

  if (!hit) {
    zero_v2(r_hit);
  }

  return hit;
}

static void gpencil_copy_points(
    bGPDstroke *gps, bGPDspoint *pt, bGPDspoint *pt_final, int i, int i2)
{
  /* don't copy same point */
  if (i == i2) {
    return;
  }

  copy_v3_v3(&pt_final->x, &pt->x);
  pt_final->pressure = pt->pressure;
  pt_final->strength = pt->strength;
  pt_final->time = pt->time;
  pt_final->flag = pt->flag;
  pt_final->uv_fac = pt->uv_fac;
  pt_final->uv_rot = pt->uv_rot;
  copy_v4_v4(pt_final->vert_color, pt->vert_color);

  if (gps->dvert != NULL) {
    MDeformVert *dvert = &gps->dvert[i];
    MDeformVert *dvert_final = &gps->dvert[i2];
    MEM_SAFE_FREE(dvert_final->dw);

    dvert_final->totweight = dvert->totweight;
    if (dvert->dw == NULL) {
      dvert_final->dw = NULL;
      dvert_final->totweight = 0;
    }
    else {
      dvert_final->dw = MEM_dupallocN(dvert->dw);
    }
  }
}

static void gpencil_insert_point(bGPdata *gpd,
                                 bGPDstroke *gps,
                                 bGPDspoint *a_pt,
                                 bGPDspoint *b_pt,
                                 const float co_a[3],
                                 const float co_b[3])
{
  bGPDspoint *temp_points;
  int totnewpoints, oldtotpoints;

  totnewpoints = gps->totpoints;
  if (a_pt) {
    totnewpoints++;
  }
  if (b_pt) {
    totnewpoints++;
  }

  /* duplicate points in a temp area */
  temp_points = MEM_dupallocN(gps->points);
  oldtotpoints = gps->totpoints;

  /* look index of base points because memory is changed when resize points array */
  int a_idx = -1;
  int b_idx = -1;
  for (int i = 0; i < oldtotpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    if (pt == a_pt) {
      a_idx = i;
    }
    if (pt == b_pt) {
      b_idx = i;
    }
  }

  /* resize the points arrays */
  gps->totpoints = totnewpoints;
  gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
  if (gps->dvert != NULL) {
    gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
  }

  /* copy all points */
  int i2 = 0;
  for (int i = 0; i < oldtotpoints; i++) {
    bGPDspoint *pt = &temp_points[i];
    bGPDspoint *pt_final = &gps->points[i2];
    gpencil_copy_points(gps, pt, pt_final, i, i2);

    /* create new point duplicating point and copy location */
    if (ELEM(i, a_idx, b_idx)) {
      i2++;
      pt_final = &gps->points[i2];
      gpencil_copy_points(gps, pt, pt_final, i, i2);
      copy_v3_v3(&pt_final->x, (i == a_idx) ? co_a : co_b);

      /* Un-select. */
      pt_final->flag &= ~GP_SPOINT_SELECT;
      /* tag to avoid more checking with this point */
      pt_final->flag |= GP_SPOINT_TAG;
    }

    i2++;
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  MEM_SAFE_FREE(temp_points);
}

static float gpencil_calc_factor(const float p2d_a1[2],
                                 const float p2d_a2[2],
                                 const float r_hit2d[2])
{
  float dist1 = len_squared_v2v2(p2d_a1, p2d_a2);
  float dist2 = len_squared_v2v2(p2d_a1, r_hit2d);
  float f = dist1 > 0.0f ? dist2 / dist1 : 0.0f;

  /* apply a correction factor */
  float v1[2];
  interp_v2_v2v2(v1, p2d_a1, p2d_a2, f);
  float dist3 = len_squared_v2v2(p2d_a1, v1);
  float f1 = dist1 > 0.0f ? dist3 / dist1 : 0.0f;
  f = f + (f - f1);

  return f;
}

int ED_gpencil_select_stroke_segment(bGPdata *gpd,
                                     bGPDlayer *gpl,
                                     bGPDstroke *gps,
                                     bGPDspoint *pt,
                                     bool select,
                                     bool insert,
                                     const float scale,
                                     float r_hita[3],
                                     float r_hitb[3])
{
  if (gps->totpoints < 2) {
    return 0;
  }
  const float min_factor = 0.0015f;
  bGPDspoint *pta1 = NULL;
  bGPDspoint *pta2 = NULL;
  float f = 0.0f;
  int i2 = 0;

  bGPDlayer *gpl_orig = (gpl->runtime.gpl_orig) ? gpl->runtime.gpl_orig : gpl;
  bGPDframe *gpf = gpl_orig->actframe;
  if (gpf == NULL) {
    return 0;
  }

  int memsize = BLI_listbase_count(&gpf->strokes);
  bGPDstroke **gps_array = MEM_callocN(sizeof(bGPDstroke *) * memsize, __func__);

  /* save points */
  bGPDspoint *oldpoints = MEM_dupallocN(gps->points);

  /* Save list of strokes to check */
  int totstrokes = 0;
  LISTBASE_FOREACH (bGPDstroke *, gps_iter, &gpf->strokes) {
    if (gps_iter->totpoints < 2) {
      continue;
    }
    gps_array[totstrokes] = gps_iter;
    totstrokes++;
  }

  if (totstrokes == 0) {
    return 0;
  }

  /* look for index of the current point */
  int cur_idx = -1;
  for (int i = 0; i < gps->totpoints; i++) {
    pta1 = &gps->points[i];
    if (pta1 == pt) {
      cur_idx = i;
      break;
    }
  }
  if (cur_idx < 0) {
    return 0;
  }

  /* Convert all gps points to 2d and save in a hash to avoid recalculation. */
  int direction = 0;
  float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * gps->totpoints,
                                    "GP Stroke temp 2d points");
  BKE_gpencil_stroke_2d_flat_ref(
      gps->points, gps->totpoints, gps->points, gps->totpoints, points2d, scale, &direction);

  GHash *all_2d = BLI_ghash_ptr_new(__func__);

  for (int s = 0; s < totstrokes; s++) {
    bGPDstroke *gps_iter = gps_array[s];
    float(*points2d_iter)[2] = MEM_mallocN(sizeof(*points2d_iter) * gps_iter->totpoints, __func__);

    /* the extremes of the stroke are scaled to improve collision detection
     * for near lines */
    BKE_gpencil_stroke_2d_flat_ref(gps->points,
                                   gps->totpoints,
                                   gps_iter->points,
                                   gps_iter->totpoints,
                                   points2d_iter,
                                   scale,
                                   &direction);
    BLI_ghash_insert(all_2d, gps_iter, points2d_iter);
  }

  bool hit_a = false;
  bool hit_b = false;
  float p2d_a1[2] = {0.0f, 0.0f};
  float p2d_a2[2] = {0.0f, 0.0f};
  float r_hit2d[2];
  bGPDspoint *hit_pointa = NULL;
  bGPDspoint *hit_pointb = NULL;

  /* analyze points before current */
  if (cur_idx > 0) {
    for (int i = cur_idx; i >= 0; i--) {
      pta1 = &gps->points[i];
      copy_v2_v2(p2d_a1, points2d[i]);

      i2 = i - 1;
      CLAMP_MIN(i2, 0);
      pta2 = &gps->points[i2];
      copy_v2_v2(p2d_a2, points2d[i2]);

      hit_a = gpencil_check_collision(gps, gps_array, all_2d, totstrokes, p2d_a1, p2d_a2, r_hit2d);

      if (select) {
        pta1->flag |= GP_SPOINT_SELECT;
      }
      else {
        pta1->flag &= ~GP_SPOINT_SELECT;
      }

      if (hit_a) {
        f = gpencil_calc_factor(p2d_a1, p2d_a2, r_hit2d);
        interp_v3_v3v3(r_hita, &pta1->x, &pta2->x, f);
        if (f > min_factor) {
          hit_pointa = pta2; /* first point is second (inverted loop) */
        }
        else {
          pta1->flag &= ~GP_SPOINT_SELECT;
        }
        break;
      }
    }
  }

  /* analyze points after current */
  for (int i = cur_idx; i < gps->totpoints; i++) {
    pta1 = &gps->points[i];
    copy_v2_v2(p2d_a1, points2d[i]);

    i2 = i + 1;
    CLAMP_MAX(i2, gps->totpoints - 1);
    pta2 = &gps->points[i2];
    copy_v2_v2(p2d_a2, points2d[i2]);

    hit_b = gpencil_check_collision(gps, gps_array, all_2d, totstrokes, p2d_a1, p2d_a2, r_hit2d);

    if (select) {
      pta1->flag |= GP_SPOINT_SELECT;
    }
    else {
      pta1->flag &= ~GP_SPOINT_SELECT;
    }

    if (hit_b) {
      f = gpencil_calc_factor(p2d_a1, p2d_a2, r_hit2d);
      interp_v3_v3v3(r_hitb, &pta1->x, &pta2->x, f);
      if (f > min_factor) {
        hit_pointb = pta1;
      }
      else {
        pta1->flag &= ~GP_SPOINT_SELECT;
      }
      break;
    }
  }

  /* insert new point in the collision points */
  if (insert) {
    gpencil_insert_point(gpd, gps, hit_pointa, hit_pointb, r_hita, r_hitb);
  }

  /* free memory */
  if (all_2d) {
    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, all_2d) {
      float(*p2d)[2] = BLI_ghashIterator_getValue(&gh_iter);
      MEM_SAFE_FREE(p2d);
    }
    BLI_ghash_free(all_2d, NULL, NULL);
  }

  /* if no hit, reset selection flag */
  if ((!hit_a) && (!hit_b)) {
    for (int i = 0; i < gps->totpoints; i++) {
      pta1 = &gps->points[i];
      pta2 = &oldpoints[i];
      pta1->flag = pta2->flag;
    }
  }

  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(gps_array);
  MEM_SAFE_FREE(oldpoints);

  /* return type of hit */
  if ((hit_a) && (hit_b)) {
    return 3;
  }
  if (hit_a) {
    return 1;
  }
  if (hit_b) {
    return 2;
  }
  return 0;
}
/* END VERBATIM */
#undef BKE_gpencil_stroke_2d_flat_ref

/* ---- segment pick ------------------------------------------------------------------------ */
static bool pg4_material_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  if (gpd->mat == NULL || gps->mat_nr < 0 || gps->mat_nr >= gpd->totcol || gpd->mat[gps->mat_nr] == NULL) {
    return true;
  }
  const MaterialGPencilStyle *gp_style = gpd->mat[gps->mat_nr]->gp_style;
  if (gp_style == NULL) {
    return true;
  }
  if (gp_style->flag & GP_MATERIAL_HIDE) {
    return false;
  }
  return !(((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (gp_style->flag & GP_MATERIAL_LOCKED));
}

int pg_gp_select_segment_pick(bGPdata *gpd, const bGPDlayer *only_layer, float x, float y,
                              int radius_squared, int flags)
{
  if (gpd == NULL || !isfinite(x) || !isfinite(y) || radius_squared < 0) {
    return 0;
  }
  /* The point pick of gpencil_select_exec() (pg_gp_edit_pick, point mode) ... */
  int changed = pg_gp_edit_pick(gpd, only_layer, x, y, radius_squared, flags & ~PG_PICK_ENTIRE,
                                0 /* GP_SELECTMODE_POINT */);
  /* ... finds the same hit: nearest point by manhattan distance on the active frames of the
   * editable layers (first pass of gpencil_select_exec()). */
  const int mval[2] = {(int)lroundf(x), (int)lroundf(y)};
  bGPDlayer *hit_layer = NULL;
  bGPDstroke *hit_stroke = NULL;
  bGPDspoint *hit_point = NULL;
  int hit_distance = radius_squared;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if ((only_layer && gpl != only_layer) || !BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) {
      continue;
    }
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      if (gps->points == NULL || !pg4_material_editable(gpd, gpl, gps)) {
        continue;
      }
      for (int i = 0; i < gps->totpoints; i++) {
        bGPDspoint *pt = &gps->points[i];
        if (!isfinite(pt->x) || !isfinite(pt->y) || fabsf(pt->x) > 1.0e6f || fabsf(pt->y) > 1.0e6f) {
          continue;
        }
        const int d = abs(mval[0] - (int)lroundf(pt->x)) + abs(mval[1] - (int)lroundf(pt->y));
        if (d <= radius_squared && d < hit_distance) {
          hit_layer = gpl;
          hit_stroke = gps;
          hit_point = pt;
          hit_distance = d;
        }
      }
    }
  }
  if (hit_point == NULL || (flags & PG_PICK_DESELECT)) {
    return changed;
  }
  /* gpencil_select_exec(): "expand selection to segment" */
  float r_hita[3], r_hitb[3];
  bool hit_select = (bool)(hit_point->flag & GP_SPOINT_SELECT);
  if (!hit_select) {
    return changed;
  }
  /* Count selected points before/after to report a change. */
  int before = 0, after = 0;
  for (int i = 0; i < hit_stroke->totpoints; i++) before += (hit_stroke->points[i].flag & GP_SPOINT_SELECT) != 0;
  ED_gpencil_select_stroke_segment(
      gpd, hit_layer, hit_stroke, hit_point, hit_select, false, PG_SEGMENT_ISECT_THRESHOLD, r_hita, r_hitb);
  for (int i = 0; i < hit_stroke->totpoints; i++) after += (hit_stroke->points[i].flag & GP_SPOINT_SELECT) != 0;
  BKE_gpencil_stroke_sync_selection(gpd, hit_stroke);
  return (changed || before != after) ? 1 : 0;
}

/* ---- material slot removal ---------------------------------------------------------------- */
int pg_gp_material_slot_remove(bGPdata *gpd, int index)
{
  if (gpd == NULL || gpd->mat == NULL || index < 0 || index >= gpd->totcol || gpd->totcol <= 1) {
    return 0;
  }
  int removed = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->mat_nr == index) {
          BLI_remlink(&gpf->strokes, gps);
          BKE_gpencil_free_stroke(gps);
          removed++;
        }
      }
    }
  }
  /* BKE_object_material_slot_remove(): shift the slots down, then fix the stroke indices. */
  Material *ma = gpd->mat[index];
  if (ma) {
    MEM_SAFE_FREE(ma->gp_style);
    MEM_freeN(ma);
  }
  for (int a = index + 1; a < gpd->totcol; a++) {
    gpd->mat[a - 1] = gpd->mat[a];
  }
  gpd->totcol--;
  gpd->mat[gpd->totcol] = NULL;
  BKE_gpencil_material_index_reassign(gpd, gpd->totcol + 1, index);
  return removed + 1;
}

/* ---- onion skinning ----------------------------------------------------------------------- */
int pg_gp_layer_onion_set(bGPdata *gpd, int index, int enabled)
{
  bGPDlayer *gpl = gpd ? BLI_findlink(&gpd->layers, index) : NULL;
  if (gpl == NULL) {
    return 0;
  }
  const short before = gpl->onion_flag;
  SET_FLAG_FROM_TEST(gpl->onion_flag, enabled, GP_LAYER_ONIONSKIN);
  return before != gpl->onion_flag;
}

float pg_gp_onion_alpha(int onion_id, int use_fade, float onion_factor)
{
  /* Stand-ins carrying the names the pinned expression reads. */
  const bool use_onion_fade = use_fade != 0;
  struct { struct { float onion_id; } runtime; } gpf_ = {{(float)onion_id}}, *gpf = &gpf_;
  struct { float onion_factor; } gpd_ = {onion_factor}, *gpd = &gpd_;
  float alpha_ = 0.0f, *r_alpha = &alpha_;
/* BEGIN VERBATIM source/blender/draw/engines/gpencil/gpencil_cache_utils.c */
    *r_alpha = use_onion_fade ? (1.0f / abs(gpf->runtime.onion_id)) : 0.5f;
    *r_alpha *= gpd->onion_factor;
    *r_alpha = (gpd->onion_factor > 0.0f) ? clamp_f(*r_alpha, 0.1f, 1.0f) :
                                            clamp_f(*r_alpha, 0.01f, 1.0f);
/* END VERBATIM */
  return alpha_;
}

/* ---- dispatch ------------------------------------------------------------------------------ */
int pg_gp_edit5_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args,
                         int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) {
    if (!isfinite(args[i])) return 0;
  }
  int changed = 0;
#define NEED(n) if (arg_count < (n)) return 0
  switch (command) {
    case PG_EDIT5_CMD_SELECT_SEGMENT:
      NEED(4);
      changed = pg_gp_select_segment_pick(gpd, active_layer, args[0], args[1], (int)lroundf(args[2]),
                                          (int)lroundf(args[3]));
      break;
    case PG_EDIT5_CMD_MATERIAL_REMOVE:
      NEED(1);
      changed = pg_gp_material_slot_remove(gpd, (int)lroundf(args[0])) > 0;
      break;
    case PG_EDIT5_CMD_ONION_LAYER:
      NEED(2);
      changed = pg_gp_layer_onion_set(gpd, (int)lroundf(args[0]), args[1] != 0.0f);
      break;
    case PG_EDIT5_CMD_ONION_FADE: {
      NEED(1);
      const short before = gpd->onion_flag;
      SET_FLAG_FROM_TEST(gpd->onion_flag, args[0] != 0.0f, GP_ONION_FADE);
      changed = before != gpd->onion_flag;
      break;
    }
    default:
      return 0;
  }
#undef NEED
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed ? 1 : 0;
}
