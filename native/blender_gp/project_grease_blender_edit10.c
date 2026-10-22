/* Project Grease: the eraser marks and lasso holes (holdout materials) are app-internal strokes, so
 * select / edit / sculpt / cutter tools skip them like hidden materials. */
#define PG_MATERIAL_UNEDITABLE (GP_MATERIAL_HIDE | GP_MATERIAL_IS_STROKE_HOLDOUT | GP_MATERIAL_IS_FILL_HOLDOUT)
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Edit command ids 135-139. GPENCIL_OT_stroke_separate (modes POINT / STROKE) is ported in
 * project_grease_blender_edit4.c (pg_gp_stroke_separate).
 */
#include <math.h>
#include <stddef.h>

#include "DNA_gpencil_legacy_types.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit4.h"
#include "project_grease_blender_edit10.h"
#include <limits.h>
#include <stdlib.h>
#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BKE_gpencil_geom_legacy.h"
#include "DNA_material_types.h"

int ED_gpencil_select_stroke_segment(bGPdata *gpd, bGPDlayer *gpl, bGPDstroke *gps, bGPDspoint *pt,
                                     bool select, bool insert, const float scale, float r_hita[3],
                                     float r_hitb[3]);

/* Blender's default GP_Sculpt_Settings.isect_threshold (PG_SEGMENT_ISECT_THRESHOLD). */
#define PG_CUTTER_ISECT_THRESHOLD 0.1f

/* gpencil_edit.c gpencil_cutter_dissolve(). */
static void pg_cutter_dissolve(bGPdata *gpd, bGPDlayer *hit_layer, bGPDstroke *hit_stroke,
                               const bool flat_caps)
{
  bGPDspoint *pt = NULL;
  bGPDspoint *pt1 = NULL;
  int i;

  bGPDstroke *gpsn = hit_stroke->next;

  int totselect = 0;
  for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      totselect++;
    }
  }

  /* if all points selected delete or only 2 points and 1 selected */
  if (((totselect == 1) && (hit_stroke->totpoints == 2)) || (hit_stroke->totpoints == totselect)) {
    BLI_remlink(&hit_layer->actframe->strokes, hit_stroke);
    BKE_gpencil_free_stroke(hit_stroke);
    hit_stroke = NULL;
  }

  /* if very small distance delete */
  if ((hit_stroke) && (hit_stroke->totpoints == 2)) {
    pt = &hit_stroke->points[0];
    pt1 = &hit_stroke->points[1];
    if (len_v3v3(&pt->x, &pt1->x) < 0.001f) {
      BLI_remlink(&hit_layer->actframe->strokes, hit_stroke);
      BKE_gpencil_free_stroke(hit_stroke);
      hit_stroke = NULL;
    }
  }

  if (hit_stroke) {
    /* tag and dissolve (untag new points) */
    for (i = 0, pt = hit_stroke->points; i < hit_stroke->totpoints; i++, pt++) {
      if (pt->flag & GP_SPOINT_SELECT) {
        pt->flag &= ~GP_SPOINT_SELECT;
        pt->flag |= GP_SPOINT_TAG;
      }
      else if (pt->flag & GP_SPOINT_TAG) {
        pt->flag &= ~GP_SPOINT_TAG;
      }
    }
    /* If flat caps mode check extremes. */
    if (flat_caps) {
      if (hit_stroke->points[0].flag & GP_SPOINT_TAG) {
        hit_stroke->caps[0] = GP_STROKE_CAP_FLAT;
      }

      if (hit_stroke->points[hit_stroke->totpoints - 1].flag & GP_SPOINT_TAG) {
        hit_stroke->caps[1] = GP_STROKE_CAP_FLAT;
      }
    }

    BKE_gpencil_stroke_delete_tagged_points(
        gpd, hit_layer->actframe, hit_stroke, gpsn, GP_SPOINT_TAG, false, false, 1);
  }
}

static bool pg_cutter_material_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  if (gpd->mat == NULL || gps->mat_nr < 0 || gps->mat_nr >= gpd->totcol || gpd->mat[gps->mat_nr] == NULL) return true;
  const MaterialGPencilStyle *st = gpd->mat[gps->mat_nr]->gp_style;
  if (st == NULL) return true;
  if (st->flag & PG_MATERIAL_UNEDITABLE) return false;
  return (gpl->flag & GP_LAYER_UNLOCK_COLOR) || !(st->flag & GP_MATERIAL_LOCKED);
}

/* gpencil_edit.c gpencil_cutter_lasso_select() (active frames; the lasso test is in canvas units). */
int pg_gp_cutter(bGPdata *gpd, const int (*mcoords)[2], int mcoords_len, int flat_caps)
{
  if (gpd == NULL || mcoords == NULL || mcoords_len < 3) return 0;
  const float scale = PG_CUTTER_ISECT_THRESHOLD;
  bGPDspoint *pt;
  bool changed = false;

  /* Deselect all strokes. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (gps->flag & GP_STROKE_SELECT) {
        int i;
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
        gps->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_reset(gps);
      }
    }
  }

  /* Select points */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if ((gpl->flag & GP_LAYER_LOCKED) || (gpl->flag & GP_LAYER_HIDE)) {
      continue;
    }
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (gps->points == NULL || !pg_cutter_material_editable(gpd, gpl, gps)) {
        continue;
      }
      int tot_inside = 0;
      const int oldtot = gps->totpoints;
      /* Project Grease: once the segment search found no crossing on this stroke it finds none from
       * any other point of it (it walks the whole stroke both ways) and only leaves that point
       * selected, so the rest are selected directly with the same result; repeating the search for
       * every point inside a long lasso froze the app for seconds on ~1000-point drawings. */
      bool no_crossing = false;
      for (int i = 0; i < gps->totpoints; i++) {
        pt = &gps->points[i];
        if ((pt->flag & GP_SPOINT_SELECT) || (pt->flag & GP_SPOINT_TAG)) {
          continue;
        }
        const bool is_inside = BLI_lasso_is_point_inside(
            mcoords, (unsigned int)mcoords_len, (int)lroundf(pt->x), (int)lroundf(pt->y), INT_MAX);
        if (is_inside && no_crossing) {
          tot_inside++;
          pt->flag |= GP_SPOINT_SELECT; /* what the search leaves when it finds no crossing */
          continue;
        }
        if (is_inside) {
          tot_inside++;
          changed = true;
          pt->flag |= GP_SPOINT_SELECT;
          gps->flag |= GP_STROKE_SELECT;
          BKE_gpencil_stroke_select_index_set(gpd, gps);
          float r_hita[3], r_hitb[3];
          if (gps->totpoints > 1) {
            if (ED_gpencil_select_stroke_segment(gpd, gpl, gps, pt, true, true, scale, r_hita, r_hitb) == 0) {
              no_crossing = true;
            }
          }
          /* avoid infinite loops */
          if (gps->totpoints > oldtot) {
            break;
          }
        }
      }
      /* if mark all points inside lasso set to remove all stroke */
      if ((tot_inside == oldtot) || ((tot_inside == 1) && (oldtot == 2))) {
        for (int i = 0; i < gps->totpoints; i++) {
          pt = &gps->points[i];
          pt->flag |= GP_SPOINT_SELECT;
        }
      }
    }
  }

  /* Dissolve selected points. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = gpl->actframe;
    if (gpf == NULL) continue;
    LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
      if (gps->flag & GP_STROKE_SELECT) {
        pg_cutter_dissolve(gpd, gpl, gps, flat_caps != 0);
      }
    }
  }
  return changed ? 1 : 0;
}

int pg_gp_edit10_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args,
                          int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) {
    if (!isfinite(args[i])) return 0;
  }
  int changed = 0;
  switch (command) {
    case PG_EDIT10_CMD_SEPARATE:
      if (arg_count < 1) return 0;
      changed = pg_gp_stroke_separate(gpd, active_layer, (int)lroundf(args[0]));
      break;
    case PG_EDIT10_CMD_CUTTER: {
      if (arg_count < 1 + 6) return 0;
      const int n = (arg_count - 1) / 2;
      int(*co)[2] = malloc(sizeof(int[2]) * (size_t)n);
      if (co == NULL) return 0;
      for (int i = 0; i < n; i++) {
        co[i][0] = (int)lroundf(args[1 + i * 2]);
        co[i][1] = (int)lroundf(args[2 + i * 2]);
      }
      changed = pg_gp_cutter(gpd, (const int(*)[2])co, n, args[0] != 0.0f);
      free(co);
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
