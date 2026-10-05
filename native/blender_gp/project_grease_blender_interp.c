/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Port of Blender 3.6.23 gpencil_interpolate.c (see project_grease_blender_interp.h). */
#include <limits.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_interp.h"

#define GPENCIL_STRENGTH_MIN 0.003f

int pg_interp_need_flip(const bGPDstroke *gps_from, const bGPDstroke *gps_to)
{
  if (gps_from == NULL || gps_to == NULL || gps_from->totpoints < 1 || gps_to->totpoints < 1) return 0;
  float v_from_start[2], v_to_start[2], v_from_end[2], v_to_end[2];
  copy_v2_v2(v_from_start, &gps_from->points[0].x);
  copy_v2_v2(v_to_start, &gps_to->points[0].x);
  copy_v2_v2(v_from_end, &gps_from->points[gps_from->totpoints - 1].x);
  copy_v2_v2(v_to_end, &gps_to->points[gps_to->totpoints - 1].x);

  const bool isect_lines = (isect_seg_seg_v2(v_from_start, v_to_start, v_from_end, v_to_end) ==
                            ISECT_LINE_LINE_CROSS);
  if (isect_lines) {
    float v1[2], v2[2];
    sub_v2_v2v2(v1, v_to_start, v_from_start);
    sub_v2_v2v2(v2, v_to_end, v_from_end);
    float angle = angle_v2v2(v1, v2);
    if (angle < DEG2RADF(15.0f)) {
      float dist_start = len_squared_v2v2(v_from_start, v_to_start);
      float dist_end = len_squared_v2v2(v_from_end, v_to_start);
      if (dist_start >= dist_end) {
        dist_start = len_squared_v2v2(v_from_end, v_to_start);
        dist_end = len_squared_v2v2(v_from_end, v_to_end);
        return (dist_start >= dist_end);
      }
      dist_start = len_squared_v2v2(v_from_start, v_to_start);
      dist_end = len_squared_v2v2(v_from_start, v_to_end);
      return (dist_start < dist_end);
    }
    return true;
  }
  float v1[2], v2[2];
  sub_v2_v2v2(v1, v_from_end, v_from_start);
  sub_v2_v2v2(v2, v_to_end, v_to_start);
  mul_v2_v2v2(v1, v1, v2);
  if ((v1[0] < 0.0f) && (v1[1] < 0.0f)) return true;
  return false;
}

static bGPDstroke *gpencil_stroke_get_related(GHash *used_strokes, bGPDframe *gpf, const int reference_index)
{
  bGPDstroke *gps_found = NULL;
  int lower_index = INT_MAX;
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    if (gps->select_index > reference_index) {
      if (!BLI_ghash_haskey(used_strokes, gps)) {
        if (gps->select_index < lower_index) {
          lower_index = gps->select_index;
          gps_found = gps;
        }
      }
    }
  }
  if (gps_found) BLI_ghash_insert(used_strokes, gps_found, gps_found);
  return gps_found;
}

static bGPDframe *gpencil_get_previous_keyframe(bGPDlayer *gpl, int cfra, const bool exclude_breakdowns)
{
  if (gpl->actframe != NULL && gpl->actframe->framenum < cfra) {
    if ((!exclude_breakdowns) || ((exclude_breakdowns) && (gpl->actframe->key_type != BEZT_KEYTYPE_BREAKDOWN))) {
      return gpl->actframe;
    }
  }
  LISTBASE_FOREACH_BACKWARD (bGPDframe *, gpf, &gpl->frames) {
    if ((exclude_breakdowns) && (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN)) continue;
    if (gpf->framenum >= cfra) continue;
    return gpf;
  }
  return NULL;
}

static bGPDframe *gpencil_get_next_keyframe(bGPDlayer *gpl, int cfra, const bool exclude_breakdowns)
{
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if ((exclude_breakdowns) && (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN)) continue;
    if (gpf->framenum <= cfra) continue;
    return gpf;
  }
  return NULL;
}

static void gpencil_interpolate_update_points(const bGPDstroke *gps_from, const bGPDstroke *gps_to,
                                              bGPDstroke *new_stroke, float factor)
{
  for (int i = 0; i < new_stroke->totpoints; i++) {
    const bGPDspoint *prev = &gps_from->points[i];
    const bGPDspoint *next = &gps_to->points[i];
    bGPDspoint *pt = &new_stroke->points[i];
    interp_v3_v3v3(&pt->x, &prev->x, &next->x, factor);
    pt->pressure = interpf(prev->pressure, next->pressure, 1.0f - factor);
    pt->strength = interpf(prev->strength, next->strength, 1.0f - factor);
    CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
  }
}

/* ED_gpencil_stroke_material_editable on the canvas. */
static bool pi_material_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  if (gpd->mat == NULL || gps->mat_nr < 0 || gps->mat_nr >= gpd->totcol || gpd->mat[gps->mat_nr] == NULL ||
      gpd->mat[gps->mat_nr]->gp_style == NULL)
    return true;
  const MaterialGPencilStyle *st = gpd->mat[gps->mat_nr]->gp_style;
  if (st->flag & GP_MATERIAL_HIDE) return false;
  if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (st->flag & GP_MATERIAL_LOCKED)) return false;
  return true;
}

int pg_gp_interpolate_run(bGPdata *gpd, bGPDlayer *active_gpl, int cfra, const PGInterpSettings *s)
{
  if (gpd == NULL || active_gpl == NULL || s == NULL) return 0;
  const int step = s->step < 1 ? 1 : s->step;
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const bool exclude_breakdowns = s->exclude_breakdowns != 0;
  const int flipmode = s->flipmode;
  if (gpencil_get_previous_keyframe(active_gpl, cfra, exclude_breakdowns) == NULL ||
      gpencil_get_next_keyframe(active_gpl, cfra, exclude_breakdowns) == NULL)
    return 0;
  int created = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if ((!s->all_layers) && (gpl != active_gpl)) continue;
    if (!BKE_gpencil_layer_is_editable(gpl)) continue;
    bGPDframe *gpf_prv = gpencil_get_previous_keyframe(gpl, cfra, exclude_breakdowns);
    bGPDframe *gpf_next = gpencil_get_next_keyframe(gpl, cfra, exclude_breakdowns);
    if ((gpf_prv == NULL) || (gpf_next == NULL)) continue;
    bGPDframe *prevFrame = BKE_gpencil_frame_duplicate(gpf_prv, true);
    bGPDframe *nextFrame = BKE_gpencil_frame_duplicate(gpf_next, true);

    ListBase selected_strokes = {NULL};
    GHash *used_strokes = BLI_ghash_ptr_new(__func__);
    GHash *pair_strokes = BLI_ghash_ptr_new(__func__);
    LISTBASE_FOREACH (bGPDstroke *, gps_from, &prevFrame->strokes) {
      bGPDstroke *gps_to = NULL;
      if ((s->only_selected) && ((gps_from->flag & GP_STROKE_SELECT) == 0)) continue;
      if (pi_material_editable(gpd, gpl, gps_from) == false) continue;
      if ((is_multiedit) && (gps_from->select_index > 0)) {
        gps_to = gpencil_stroke_get_related(used_strokes, nextFrame, gps_from->select_index);
      }
      if (gps_to == NULL) {
        int fFrame = BLI_findindex(&prevFrame->strokes, gps_from);
        gps_to = BLI_findlink(&nextFrame->strokes, fFrame);
      }
      if (ELEM(NULL, gps_from, gps_to)) continue;
      if ((gps_from->totpoints == 0) || (gps_to->totpoints == 0)) continue;
      if (gps_from->totpoints > gps_to->totpoints) {
        BKE_gpencil_stroke_uniform_subdivide(gpd, gps_to, gps_from->totpoints, true);
      }
      if (gps_to->totpoints > gps_from->totpoints) {
        BKE_gpencil_stroke_uniform_subdivide(gpd, gps_from, gps_to->totpoints, true);
      }
      if (flipmode == PG_INTERP_FLIP) {
        BKE_gpencil_stroke_flip(gps_to);
      }
      else if (flipmode == PG_INTERP_FLIPAUTO) {
        if (pg_interp_need_flip(gps_from, gps_to)) BKE_gpencil_stroke_flip(gps_to);
      }
      BLI_addtail(&selected_strokes, BLI_genericNodeN(gps_from));
      BLI_ghash_insert(pair_strokes, gps_from, gps_to);
    }

    const int first = s->single ? cfra : prevFrame->framenum + step;
    const int last = s->single ? cfra : nextFrame->framenum - 1;
    for (int cframe = first; cframe <= last && cframe < nextFrame->framenum; cframe += step) {
      if (cframe <= prevFrame->framenum) continue;
      float framerange = nextFrame->framenum - prevFrame->framenum;
      CLAMP_MIN(framerange, 1.0f);
      float factor = (float)(cframe - prevFrame->framenum) / framerange;
      if (s->single && s->factor >= 0.0f) factor = s->factor;
      if (s->easing_type > 0) factor = pg_gp_interpolate_easing(s->easing_type, s->easing_mode, factor, 1.70158f);
      /* an existing key at cframe is kept (Blender's sequence only fills between keys) */
      bGPDframe *existing = BKE_gpencil_layer_frame_find(gpl, cframe);
      if (existing != NULL && !s->single) continue;
      LISTBASE_FOREACH (LinkData *, link, &selected_strokes) {
        bGPDstroke *gps_from = link->data;
        if (!BLI_ghash_haskey(pair_strokes, gps_from)) continue;
        bGPDstroke *gps_to = (bGPDstroke *)BLI_ghash_lookup(pair_strokes, gps_from);
        bGPDstroke *new_stroke = BKE_gpencil_stroke_duplicate(gps_from, true, true);
        new_stroke->flag &= ~GP_STROKE_TAG;
        new_stroke->select_index = 0;
        gpencil_interpolate_update_points(gps_from, gps_to, new_stroke, factor);
        BKE_gpencil_stroke_smooth(new_stroke, s->smooth_factor, s->smooth_steps, true, true, false, false, true, NULL);
        BKE_gpencil_stroke_geometry_update(gpd, new_stroke);
        bGPDframe *interFrame = BKE_gpencil_layer_frame_get(gpl, cframe, GP_GETFRAME_ADD_NEW);
        interFrame->key_type = BEZT_KEYTYPE_BREAKDOWN;
        BLI_addtail(&interFrame->strokes, new_stroke);
        created++;
      }
    }
    BLI_freelistN(&selected_strokes);
    BLI_ghash_free(used_strokes, NULL, NULL);
    BLI_ghash_free(pair_strokes, NULL, NULL);
    BKE_gpencil_free_strokes(prevFrame);
    BKE_gpencil_free_strokes(nextFrame);
    MEM_freeN(prevFrame);
    MEM_freeN(nextFrame);
  }
  if (created) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return created;
}

int pg_gp_interp_dispatch(bGPdata *gpd, bGPDlayer *active, const float *args, int arg_count)
{
  if (gpd == NULL || args == NULL || arg_count < 11) return 0;
  for (int i = 0; i < arg_count; i++) if (!isfinite(args[i])) return 0;
  PGInterpSettings st;
  memset(&st, 0, sizeof(st));
  const int cfra = (int)lroundf(args[0]);
  st.step = (int)lroundf(args[1]);
  st.flipmode = (int)lroundf(args[2]);
  st.only_selected = args[3] != 0.0f;
  st.exclude_breakdowns = args[4] != 0.0f;
  st.all_layers = args[5] != 0.0f;
  st.easing_type = (int)lroundf(args[6]);
  st.easing_mode = (int)lroundf(args[7]);
  st.smooth_factor = args[8] < 0.0f ? 0.0f : (args[8] > 2.0f ? 2.0f : args[8]);       /* rna 0..2 */
  st.smooth_steps = (int)lroundf(args[9]) < 1 ? 1 : ((int)lroundf(args[9]) > 3 ? 3 : (int)lroundf(args[9])); /* rna 1..3 */
  st.single = args[10] != 0.0f;
  st.factor = arg_count > 11 ? args[11] : -1.0f;
  if (st.flipmode < PG_INTERP_NOFLIP || st.flipmode > PG_INTERP_FLIPAUTO || st.step < 1) return 0;
  return pg_gp_interpolate_run(gpd, active, cfra, &st) > 0;
}
