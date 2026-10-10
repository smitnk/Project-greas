/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Blender 3.6.23 Legacy Grease Pencil selection for Project Grease.
 *
 * Sources traced (all pinned at 3.6.23):
 *   source/blender/editors/gpencil_legacy/gpencil_select.c   select operators
 *   source/blender/editors/gpencil_legacy/gpencil_utils.c    ED_gpencil_select_toggle_all(),
 *                                                            ED_gpencil_stroke_material_editable(),
 *                                                            ED_gpencil_stroke_point_is_inside()
 *   source/blender/editors/gpencil_legacy/gpencil_intern.h   GP_EDITABLE_STROKES_BEGIN/END
 *   source/blender/editors/util/select_utils.c               ED_select_op_action*()
 *   source/blender/editors/include/ED_select_utils.h         SEL_* / eSelectOp
 *
 * Regions between "BEGIN VERBATIM" / "END VERBATIM" are byte-identical copies of those
 * files; tools/verify_blender_verbatim.py checks them against the pinned tree in CI.
 * Everything else is the minimum glue Blender gets from bContext/RNA/the 3D view:
 *
 *  - Editable layer/stroke iteration (PG_EDITABLE_STROKES_BEGIN) follows
 *    GP_EDITABLE_STROKES_BEGIN. Multi-frame editing is keyed off the data flag alone because
 *    Project Grease has no object mode flags.
 *  - Screen space is the canvas: gpencil_point_to_xy() is an identity projection rounded to
 *    int, and the layer/object transform is the identity.
 *  - Evaluated vs. original strokes do not exist (no depsgraph), so gps_orig/pt_orig stay NULL.
 *  - Curve edit mode, segment select mode, select random and vertex-color select are not
 *    ported. The curve/segment calls inside copied code are bound to no-op macros below and
 *    are unreachable because the public API never enables those modes.
 *  - "changed" is computed by comparing the selection state before and after, so it reports
 *    real changes (Blender's own flag is set whenever an operator touched anything).
 */

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_vec_types.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_select.h"

/* BEGIN VERBATIM source/blender/editors/include/ED_select_utils.h */
enum {
  SEL_TOGGLE = 0,
  SEL_SELECT = 1,
  SEL_DESELECT = 2,
  SEL_INVERT = 3,
};
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/include/ED_select_utils.h */
typedef enum {
  SEL_OP_ADD = 1,
  SEL_OP_SUB,
  SEL_OP_SET,
  SEL_OP_AND,
  SEL_OP_XOR,
} eSelectOp;
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/include/ED_select_utils.h */
#define SEL_OP_USE_OUTSIDE(sel_op) (ELEM(sel_op, SEL_OP_AND))
#define SEL_OP_USE_PRE_DESELECT(sel_op) (ELEM(sel_op, SEL_OP_SET))
#define SEL_OP_CAN_DESELECT(sel_op) (!ELEM(sel_op, SEL_OP_ADD))
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/util/select_utils.c */
int ED_select_op_action(const eSelectOp sel_op, const bool is_select, const bool is_inside)
{
  switch (sel_op) {
    case SEL_OP_ADD:
      return (!is_select && (is_inside)) ? 1 : -1;
    case SEL_OP_SUB:
      return (is_select && is_inside) ? 0 : -1;
    case SEL_OP_SET:
      return is_inside ? 1 : 0;
    case SEL_OP_AND:
      return (is_select && is_inside) ? -1 : (is_select ? 0 : -1);
    case SEL_OP_XOR:
      return (is_select && is_inside) ? 0 : ((!is_select && is_inside) ? 1 : -1);
  }
  BLI_assert_msg(0, "invalid sel_op");
  return -1;
}
int ED_select_op_action_deselected(const eSelectOp sel_op,
                                   const bool is_select,
                                   const bool is_inside)
{
  switch (sel_op) {
    case SEL_OP_ADD:
      return (!is_select && is_inside) ? 1 : -1;
    case SEL_OP_SUB:
      return (is_select && is_inside) ? 0 : -1;
    case SEL_OP_SET:
      /* Only difference w/ function above. */
      return is_inside ? 1 : -1;
    case SEL_OP_AND:
      return (is_select && is_inside) ? -1 : (is_select ? 0 : -1);
    case SEL_OP_XOR:
      return (is_select && is_inside) ? 0 : ((!is_select && is_inside) ? 1 : -1);
  }
  BLI_assert_msg(0, "invalid sel_op");
  return -1;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/util/select_utils.c */
eSelectOp ED_select_op_modal(const eSelectOp sel_op, const bool is_first)
{
  if (sel_op == SEL_OP_SET) {
    if (is_first == false) {
      return SEL_OP_ADD;
    }
  }
  return sel_op;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
typedef struct GP_SelectUserData {
  int mx, my, radius;
  /* Bounding box rect */
  rcti rect;
  const int (*lasso_coords)[2];
  int lasso_coords_len;
} GP_SelectUserData;
/* END VERBATIM */

/* -------------------------------------------------------------------- */
/** \name Glue replacing what Blender gets from bContext / the 3D view
 * \{ */

/* Values of eGP_Selectmode (DNA_scene_types.h) and V2D_IS_CLIPPED (UI_view2d.h). */
#define GP_SELECTMODE_POINT 0
#define GP_SELECTMODE_STROKE 1
#define GP_SELECTMODE_SEGMENT 2
#define V2D_IS_CLIPPED 12000

typedef struct GP_SpaceConversion {
  void *region;
} GP_SpaceConversion;

typedef bool (*PGTestFn)(void *region,
                         const float diff_mat[4][4],
                         const float pt[3],
                         GP_SelectUserData *user_data);

static const float pg_identity_mat[4][4] = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

/* BLI_rcti_isect_pt() (blenlib/intern/rct.c). */
static bool pg_rcti_isect_pt(const rcti *rect, const int x, const int y)
{
  if (x < rect->xmin) {
    return false;
  }
  if (x > rect->xmax) {
    return false;
  }
  if (y < rect->ymin) {
    return false;
  }
  if (y > rect->ymax) {
    return false;
  }
  return true;
}

/* Identity "projection": canvas pixels are screen pixels. */
static void pg_point_to_xy(const bGPDspoint *pt, int *r_x, int *r_y)
{
  if (!isfinite(pt->x) || !isfinite(pt->y) || fabsf(pt->x) > 1.0e6f || fabsf(pt->y) > 1.0e6f) {
    *r_x = V2D_IS_CLIPPED;
    *r_y = V2D_IS_CLIPPED;
    return;
  }
  *r_x = (int)lroundf(pt->x);
  *r_y = (int)lroundf(pt->y);
}

static void pg_point_copy(const bGPDspoint *pt, bGPDspoint *r_pt)
{
  r_pt->x = pt->x;
  r_pt->y = pt->y;
  r_pt->z = pt->z;
}

/* gpencil_3d_point_to_screen_space() with the identity projection. */
static bool pg_point_to_screen(const float pt[3], int r_co[2])
{
  bGPDspoint tmp;
  tmp.x = pt[0];
  tmp.y = pt[1];
  tmp.z = pt[2];
  int screen_co[2];
  pg_point_to_xy(&tmp, &screen_co[0], &screen_co[1]);
  if (screen_co[0] == V2D_IS_CLIPPED || screen_co[1] == V2D_IS_CLIPPED) {
    r_co[0] = V2D_IS_CLIPPED;
    r_co[1] = V2D_IS_CLIPPED;
    return false;
  }
  r_co[0] = screen_co[0];
  r_co[1] = screen_co[1];
  return true;
}

/* Materials live in bGPdata::mat[] (no Object). Like BKE_gpencil_material_settings(), a
 * missing material yields the default style, never NULL. */
static MaterialGPencilStyle pg_default_style;
static MaterialGPencilStyle *pg_material_style(const bGPdata *gpd, int act)
{
  const int index = act - 1;
  if (gpd->mat != NULL && index >= 0 && index < (int)gpd->totcol && gpd->mat[index] != NULL &&
      gpd->mat[index]->gp_style != NULL)
  {
    return gpd->mat[index]->gp_style;
  }
  return &pg_default_style;
}

/* ED_gpencil_stroke_material_editable() (gpencil_utils.c), reading gpd->mat[]. */
static bool pg_stroke_material_editable(const bGPdata *gpd,
                                        const bGPDlayer *gpl,
                                        const bGPDstroke *gps)
{
  /* check if the color is editable */
  MaterialGPencilStyle *gp_style = pg_material_style(gpd, gps->mat_nr + 1);

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

/* ED_gpencil_stroke_point_is_inside() (gpencil_utils.c) with the identity projection. */
static bool pg_stroke_point_is_inside(const bGPDstroke *gps, const int mval[2])
{
  bool hit = false;
  if (gps->totpoints == 0) {
    return hit;
  }

  const int len = gps->totpoints;
  int(*mcoords)[2] = malloc(sizeof(int[2]) * (size_t)len);
  if (mcoords == NULL) {
    return false;
  }

  /* Convert stroke to 2D array of points. */
  for (int i = 0; i < len; i++) {
    pg_point_to_xy(&gps->points[i], &mcoords[i][0], &mcoords[i][1]);
  }

  /* Compute bound-box of lasso (for faster testing later). */
  rcti rect;
  BLI_lasso_boundbox(&rect, mcoords, (unsigned int)len);

  /* Test if point inside stroke. */
  hit = (!ELEM(V2D_IS_CLIPPED, mval[0], mval[1]) && pg_rcti_isect_pt(&rect, mval[0], mval[1]) &&
         BLI_lasso_is_point_inside(mcoords, (unsigned int)len, mval[0], mval[1], INT_MAX));

  free(mcoords);
  return hit;
}

/* Bind the names used inside the verbatim regions to the glue above. */
#define ED_gpencil_stroke_can_use(C, gps) (true)
#define BKE_gpencil_material_settings(ob, act) pg_material_style(gpd, (act))
#define BLI_rcti_isect_pt(rect, x, y) pg_rcti_isect_pt((rect), (x), (y))
#define gpencil_point_to_world_space(pt, diff_mat, r_pt) pg_point_copy((pt), (r_pt))
#define gpencil_point_to_xy(gsc, gps, pt, r_x, r_y) pg_point_to_xy((pt), (r_x), (r_y))
#define ED_gpencil_stroke_point_is_inside(gps, gsc, mval, diff_mat) \
  pg_stroke_point_is_inside((gps), (mval))
/* Segment select and curve edit mode: not ported (see file header). */
#define ED_gpencil_select_stroke_segment(gpd, gpl, gps, pt, select, insert, scale, hita, hitb) \
  ((void)0)
#define BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps) ((void)0)
#define select_all_curve_points(gpd, gps, gpc, deselect) ((void)0)
#define BKE_gpencil_stroke_geometry_update(gpd, gps) ((void)0)

static bool pg_is_multiedit(const bGPdata *gpd)
{
  return (gpd->flag & GP_DATA_STROKE_MULTIEDIT) != 0;
}

static bool pg_layer_in_scope(const bGPDlayer *gpl, const bGPDlayer *only_layer)
{
  return only_layer == NULL || gpl == only_layer;
}

/* Modeled on GP_EDITABLE_STROKES_BEGIN / GP_EDITABLE_STROKES_END (gpencil_intern.h). */
#define PG_EDITABLE_STROKES_BEGIN(gpd_, only_layer_, gpl, gps) \
  { \
    const bool is_multiedit_ = pg_is_multiedit(gpd_); \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd_)->layers) { \
      if (!pg_layer_in_scope(gpl, (only_layer_)) || !BKE_gpencil_layer_is_editable(gpl)) { \
        continue; \
      } \
      bGPDframe *init_gpf_ = (is_multiedit_) ? (bGPDframe *)gpl->frames.first : gpl->actframe; \
      for (bGPDframe *gpf_ = init_gpf_; gpf_; gpf_ = gpf_->next) { \
        if ((gpf_ == gpl->actframe) || ((gpf_->flag & GP_FRAME_SELECT) && is_multiedit_)) { \
          bGPDstroke *gpsn_; \
          for (bGPDstroke *gps = (bGPDstroke *)gpf_->strokes.first; gps; gps = gpsn_) { \
            gpsn_ = gps->next; \
            if (ED_gpencil_stroke_can_use(NULL, gps) == false) { \
              continue; \
            } \
            if (pg_stroke_material_editable((gpd_), gpl, gps) == false) { \
              continue; \
            } \
    /* ... Do Stuff With Strokes ... */

#define PG_EDITABLE_STROKES_END \
  } \
  } \
  if (!is_multiedit_) { \
    break; \
  } \
  } \
  } \
  } \
  (void)0

/* Exact before/after comparison of every selection flag. */
static unsigned char *pg_selection_snapshot(bGPdata *gpd, size_t *r_len)
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

static int pg_selection_changed(bGPdata *gpd,
                                const unsigned char *before,
                                size_t before_len)
{
  size_t after_len = 0;
  unsigned char *after = pg_selection_snapshot(gpd, &after_len);
  int changed = 0;
  if (after == NULL || before == NULL) {
    changed = 1;
  }
  else if (after_len != before_len || memcmp(before, after, before_len) != 0) {
    changed = 1;
  }
  free(after);
  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select All / Deselect / Invert / Toggle (ED_gpencil_select_toggle_all)
 * \{ */

static void pg_toggle_all(bGPdata *gpd, int action, const bGPDlayer *only_layer)
{
  /* for "toggle", test for existing selected strokes */
  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
      if (gps->flag & GP_STROKE_SELECT) {
        action = SEL_DESELECT;
        break; /* XXX: this only gets out of the inner loop (same as Blender). */
      }
    }
    PG_EDITABLE_STROKES_END;
  }

  /* if deselecting, we need to deselect strokes across all frames */
  if (action == SEL_DESELECT) {
    /* Set selection index to 0. */
    gpd->select_last_index = 0;

    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (!pg_layer_in_scope(gpl, only_layer) || !BKE_gpencil_layer_is_editable(gpl)) {
        continue;
      }

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
      /* deselect all strokes on all frames */
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        bGPDstroke *gps;

        for (gps = gpf->strokes.first; gps; gps = gps->next) {
          bGPDspoint *pt;
          int i;

          /* only edit strokes that are valid in this view... */
          if (ED_gpencil_stroke_can_use(C, gps)) {
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              pt->flag &= ~GP_SPOINT_SELECT;
            }

            gps->flag &= ~GP_STROKE_SELECT;
            BKE_gpencil_stroke_select_index_reset(gps);
          }
        }
      }
/* END VERBATIM */
    }
  }
  else {
    /* select or deselect all strokes */
    PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
      bGPDspoint *pt;
      int i;
      bool selected = false;

      /* Change selection status of all points, then make the stroke match */
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        switch (action) {
          case SEL_SELECT:
            pt->flag |= GP_SPOINT_SELECT;
            break;
#if 0
          case SEL_DESELECT:
           pt->flag &= ~GP_SPOINT_SELECT;
           break;
#endif
          case SEL_INVERT:
            pt->flag ^= GP_SPOINT_SELECT;
            break;
        }

        if (pt->flag & GP_SPOINT_SELECT) {
          selected = true;
        }
      }

      /* Change status of stroke */
      if (selected) {
        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);
      }
      else {
        gps->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_reset(gps);
      }
/* END VERBATIM */
    }
    PG_EDITABLE_STROKES_END;
  }
}

int pg_gp_select_all(bGPdata *gpd, int action, const bGPDlayer *only_layer)
{
  if (gpd == NULL || action < SEL_TOGGLE || action > SEL_INVERT) {
    return 0;
  }
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);
  pg_toggle_all(gpd, action, only_layer);
  const int changed = pg_selection_changed(gpd, before, before_len);
  free(before);
  return changed;
}

/* deselect_all_selected() (gpencil_select.c): used before SET-type lasso/box selection. */
static void pg_deselect_all_selected(bGPdata *gpd, const bGPDlayer *only_layer)
{
  /* Set selection index to 0. */
  gpd->select_last_index = 0;

  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
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
  PG_EDITABLE_STROKES_END;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linked / Alternate / More / Less / First / Last
 * \{ */

int pg_gp_select_linked(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);

  /* select all points in selected strokes */
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
    if (gps->totpoints < 1) {
      continue;
    }
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      if (gps->flag & GP_STROKE_SELECT) {
        bGPDspoint *pt;
        int i;

        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          pt->flag |= GP_SPOINT_SELECT;
        }
      }
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;

  const int changed = pg_selection_changed(gpd, before, before_len);
  free(before);
  return changed;
}

int pg_gp_select_alternate(bGPdata *gpd, int unselect_ends_flag, const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  const bool unselect_ends = unselect_ends_flag != 0;
  bool changed = false;
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);

  /* select all points in selected strokes */
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      if ((gps->flag & GP_STROKE_SELECT) && (gps->totpoints > 1)) {
        bGPDspoint *pt;
        int row = 0;
        int start = 0;
        if (unselect_ends) {
          start = 1;
        }

        for (int i = start; i < gps->totpoints; i++) {
          pt = &gps->points[i];
          if ((row % 2) == 0) {
            pt->flag |= GP_SPOINT_SELECT;
          }
          else {
            pt->flag &= ~GP_SPOINT_SELECT;
          }
          row++;
        }

        /* unselect start and end points */
        if (unselect_ends) {
          pt = &gps->points[0];
          pt->flag &= ~GP_SPOINT_SELECT;

          pt = &gps->points[gps->totpoints - 1];
          pt->flag &= ~GP_SPOINT_SELECT;
        }

        changed = true;
      }
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;
  (void)changed;

  const int result = pg_selection_changed(gpd, before, before_len);
  free(before);
  return result;
}

int pg_gp_select_more(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  bool changed = false;
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);

  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
    if (gps->totpoints < 1) {
      continue;
    }
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      if (gps->flag & GP_STROKE_SELECT) {
        bGPDspoint *pt;
        int i;
        bool prev_sel;

        /* First Pass: Go in forward order,
         * expanding selection if previous was selected (pre changes).
         * - This pass covers the "after" edges of selection islands
         */
        prev_sel = false;
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          if (pt->flag & GP_SPOINT_SELECT) {
            /* selected point - just set flag for next point */
            prev_sel = true;
          }
          else {
            /* unselected point - expand selection if previous was selected... */
            if (prev_sel) {
              pt->flag |= GP_SPOINT_SELECT;
              changed = true;
            }
            prev_sel = false;
          }
        }

        /* Second Pass: Go in reverse order, doing the same as before (except in opposite order)
         * - This pass covers the "before" edges of selection islands
         */
        prev_sel = false;
        for (pt -= 1; i > 0; i--, pt--) {
          if (pt->flag & GP_SPOINT_SELECT) {
            prev_sel = true;
          }
          else {
            /* unselected point - expand selection if previous was selected... */
            if (prev_sel) {
              pt->flag |= GP_SPOINT_SELECT;
              changed = true;
            }
            prev_sel = false;
          }
        }
      }
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;
  (void)changed;

  const int result = pg_selection_changed(gpd, before, before_len);
  free(before);
  return result;
}

int pg_gp_select_less(bGPdata *gpd, const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  bool changed = false;
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);

  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
    if (gps->totpoints < 1) {
      continue;
    }
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      if (gps->flag & GP_STROKE_SELECT) {
        bGPDspoint *pt;
        int i;
        bool prev_sel;

        /* First Pass: Go in forward order, shrinking selection
         * if previous was not selected (pre changes).
         * - This pass covers the "after" edges of selection islands
         */
        prev_sel = false;
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          if (pt->flag & GP_SPOINT_SELECT) {
            /* shrink if previous wasn't selected */
            if (prev_sel == false) {
              pt->flag &= ~GP_SPOINT_SELECT;
              changed = true;
            }
            prev_sel = true;
          }
          else {
            /* mark previous as being unselected - and hence, is trigger for shrinking */
            prev_sel = false;
          }
        }

        /* Second Pass: Go in reverse order, doing the same as before (except in opposite order)
         * - This pass covers the "before" edges of selection islands
         */
        prev_sel = false;
        for (pt -= 1; i > 0; i--, pt--) {
          if (pt->flag & GP_SPOINT_SELECT) {
            /* shrink if previous wasn't selected */
            if (prev_sel == false) {
              pt->flag &= ~GP_SPOINT_SELECT;
              changed = true;
            }
            prev_sel = true;
          }
          else {
            /* mark previous as being unselected - and hence, is trigger for shrinking */
            prev_sel = false;
          }
        }
      }
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;
  (void)changed;

  /* Blender leaves the stroke flag alone here; keep the flag/points in sync so a stroke whose
   * last point was removed does not stay "selected" with no selected points. */
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      BKE_gpencil_stroke_sync_selection(gpd, gps);
    }
  }
  PG_EDITABLE_STROKES_END;

  const int result = pg_selection_changed(gpd, before, before_len);
  free(before);
  return result;
}

int pg_gp_select_first(bGPdata *gpd,
                       int only_selected_strokes,
                       int extend_flag,
                       const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  const bool only_selected = only_selected_strokes != 0;
  const bool extend = extend_flag != 0;
  bool changed = false;
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);

  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
    /* skip stroke if we're only manipulating selected strokes */
    if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
      continue;
    }
/* END VERBATIM */

    /* Hardening: Blender asserts totpoints >= 1 here. */
    if (gps->totpoints < 1) {
      continue;
    }

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      gps->points->flag |= GP_SPOINT_SELECT;
      gps->flag |= GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_set(gpd, gps);

      /* deselect rest? */
      if ((extend == false) && (gps->totpoints > 1)) {
        /* start from index 1, to skip the first point that we'd just selected... */
        bGPDspoint *pt = &gps->points[1];
        int i = 1;

        for (; i < gps->totpoints; i++, pt++) {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
      }
      changed = true;
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;
  (void)changed;

  const int result = pg_selection_changed(gpd, before, before_len);
  free(before);
  return result;
}

int pg_gp_select_last(bGPdata *gpd,
                      int only_selected_strokes,
                      int extend_flag,
                      const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  const bool only_selected = only_selected_strokes != 0;
  const bool extend = extend_flag != 0;
  bool changed = false;
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);

  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
    /* skip stroke if we're only manipulating selected strokes */
    if (only_selected && !(gps->flag & GP_STROKE_SELECT)) {
      continue;
    }
/* END VERBATIM */

    /* Hardening: Blender asserts totpoints >= 1 here. */
    if (gps->totpoints < 1) {
      continue;
    }

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      gps->points[gps->totpoints - 1].flag |= GP_SPOINT_SELECT;
      gps->flag |= GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_set(gpd, gps);

      /* deselect rest? */
      if ((extend == false) && (gps->totpoints > 1)) {
        /* don't include the last point... */
        bGPDspoint *pt = gps->points;
        int i = 0;

        for (; i < gps->totpoints - 1; i++, pt++) {
          pt->flag &= ~GP_SPOINT_SELECT;
        }
      }

      changed = true;
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;
  (void)changed;

  const int result = pg_selection_changed(gpd, before, before_len);
  free(before);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Grouped (layer / material)
 * \{ */

/* On each visible layer, check for selected strokes - if found, select all others. */
static bool pg_select_same_layer(bGPdata *gpd, const bGPDlayer *only_layer)
{
  bool changed = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!pg_layer_in_scope(gpl, only_layer) || !BKE_gpencil_layer_is_editable(gpl)) {
      continue;
    }
    /* Blender uses BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_USE_PREV); the active
     * frame is the frame Project Grease's timeline selected. */
    bGPDframe *gpf = gpl->actframe;
    bGPDstroke *gps;
    bool found = false;

    if (gpf == NULL) {
      continue;
    }

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
    /* Search for a selected stroke */
    for (gps = gpf->strokes.first; gps; gps = gps->next) {
      if (ED_gpencil_stroke_can_use(C, gps)) {
        if (gps->flag & GP_STROKE_SELECT) {
          found = true;
          break;
        }
      }
    }
/* END VERBATIM */

    /* Select all if found */
    if (found) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
        for (gps = gpf->strokes.first; gps; gps = gps->next) {
          if (ED_gpencil_stroke_can_use(C, gps)) {
            bGPDspoint *pt;
            int i;

            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              pt->flag |= GP_SPOINT_SELECT;
            }

            gps->flag |= GP_STROKE_SELECT;
            BKE_gpencil_stroke_select_index_set(gpd, gps);

            changed = true;
          }
        }
/* END VERBATIM */
    }
  }
  return changed;
}

/* Replacement for Blender's GSet of material indices (BLI_ghash.c is not in the closure). */
typedef struct PGMaterialSet {
  bool *used;
  int len;
} PGMaterialSet;

static bool pg_matset_has(const PGMaterialSet *set, int mat_nr)
{
  return set != NULL && mat_nr >= 0 && mat_nr < set->len && set->used[mat_nr];
}
#define BLI_gset_haskey(set, key) pg_matset_has((set), (int)(intptr_t)(key))

/* Select all strokes with same colors as selected ones. */
static bool pg_select_same_material(bGPdata *gpd, const bGPDlayer *only_layer)
{
  PGMaterialSet material_set = {NULL, 0};
  PGMaterialSet *selected_colors = &material_set;
  bool changed = false;

  /* First, build set containing all the colors of selected strokes */
  int max_mat = 0;
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
    if (gps->mat_nr > max_mat) {
      max_mat = gps->mat_nr;
    }
  }
  PG_EDITABLE_STROKES_END;
  material_set.len = max_mat + 1;
  material_set.used = calloc((size_t)material_set.len, sizeof(bool));
  if (material_set.used == NULL) {
    return false;
  }
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
    if ((gps->flag & GP_STROKE_SELECT) && gps->mat_nr >= 0) {
      material_set.used[gps->mat_nr] = true;
    }
  }
  PG_EDITABLE_STROKES_END;

  /* Second, select any visible stroke that uses these colors */
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
      if (BLI_gset_haskey(selected_colors, POINTER_FROM_INT(gps->mat_nr))) {
        /* select this stroke */
        bGPDspoint *pt;
        int i;

        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          pt->flag |= GP_SPOINT_SELECT;
        }

        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);

        changed = true;
      }
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;

  free(material_set.used);
  return changed;
}

int pg_gp_select_grouped(bGPdata *gpd, int type, const bGPDlayer *only_layer)
{
  if (gpd == NULL || (type != PG_SELECT_GROUPED_LAYER && type != PG_SELECT_GROUPED_MATERIAL)) {
    return 0;
  }
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);
  if (type == PG_SELECT_GROUPED_LAYER) {
    pg_select_same_layer(gpd, only_layer);
  }
  else {
    pg_select_same_material(gpd, only_layer);
  }
  const int changed = pg_selection_changed(gpd, before, before_len);
  free(before);
  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select
 * \{ */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
static bool gpencil_stroke_do_circle_sel(bGPdata *gpd,
                                         bGPDlayer *gpl,
                                         bGPDstroke *gps,
                                         GP_SpaceConversion *gsc,
                                         const int mx,
                                         const int my,
                                         const int radius,
                                         const bool select,
                                         rcti *rect,
                                         const float diff_mat[4][4],
                                         const int selectmode,
                                         const float scale,
                                         const bool is_curve_edit)
{
  bGPDspoint *pt = NULL;
  int x0 = 0, y0 = 0;
  int i;
  bool changed = false;
  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = NULL;
  bool hit = false;

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;

    bGPDspoint pt_temp;
    gpencil_point_to_world_space(pt, diff_mat, &pt_temp);
    gpencil_point_to_xy(gsc, gps, &pt_temp, &x0, &y0);

    /* do boundbox check first */
    if (!ELEM(V2D_IS_CLIPPED, x0, y0) && BLI_rcti_isect_pt(rect, x0, y0)) {
      /* only check if point is inside */
      if (((x0 - mx) * (x0 - mx) + (y0 - my) * (y0 - my)) <= radius * radius) {
        hit = true;

        /* change selection */
        if (select) {
          pt_active->flag |= GP_SPOINT_SELECT;
          gps_active->flag |= GP_STROKE_SELECT;
          BKE_gpencil_stroke_select_index_set(gpd, gps_active);
        }
        else {
          pt_active->flag &= ~GP_SPOINT_SELECT;
          gps_active->flag &= ~GP_STROKE_SELECT;
          BKE_gpencil_stroke_select_index_reset(gps_active);
        }
        changed = true;
        /* if stroke mode, don't check more points */
        if ((hit) && (selectmode == GP_SELECTMODE_STROKE)) {
          break;
        }

        /* Expand selection to segment. */
        if ((hit) && (selectmode == GP_SELECTMODE_SEGMENT) && (select) && (pt_active != NULL)) {
          float r_hita[3], r_hitb[3];
          bool hit_select = (bool)(pt_active->flag & GP_SPOINT_SELECT);
          ED_gpencil_select_stroke_segment(
              gpd, gpl, gps_active, pt_active, hit_select, false, scale, r_hita, r_hitb);
        }
      }
    }
  }

  /* If stroke mode expand selection. */
  if ((hit) && (selectmode == GP_SELECTMODE_STROKE)) {
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
      if (pt_active != NULL) {
        if (select) {
          pt_active->flag |= GP_SPOINT_SELECT;
        }
        else {
          pt_active->flag &= ~GP_SPOINT_SELECT;
        }
      }
    }
  }

  /* If curve edit mode, generate the curve. */
  if (is_curve_edit && hit && gps_active->editcurve == NULL) {
    BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps_active);
    gps_active->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
    /* Select all curve points. */
    select_all_curve_points(gpd, gps_active, gps_active->editcurve, false);
    BKE_gpencil_stroke_geometry_update(gpd, gps_active);
    changed = true;
  }

  /* Ensure that stroke selection is in sync with its points. */
  BKE_gpencil_stroke_sync_selection(gpd, gps_active);

  return changed;
}
/* END VERBATIM */

int pg_gp_select_circle(bGPdata *gpd,
                        int mx,
                        int my,
                        int radius,
                        int sel_op_in,
                        int is_first,
                        int selectmode,
                        const bGPDlayer *only_layer)
{
  if (gpd == NULL || radius < 0 || sel_op_in < SEL_OP_ADD || sel_op_in > SEL_OP_XOR ||
      (selectmode != GP_SELECTMODE_POINT && selectmode != GP_SELECTMODE_STROKE ))
  {
    return 0;
  }
  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);

  /* gpencil_circle_select_exec() */
  const eSelectOp sel_op = ED_select_op_modal((eSelectOp)sel_op_in, is_first != 0);
  const bool select = (sel_op != SEL_OP_SUB);
  const float scale = 0.0f; /* gp_sculpt.isect_threshold: only used by segment mode. */

  /* For bounding `rect` around circle (for quickly intersection testing). */
  rcti rect = {0};
  rect.xmin = mx - radius;
  rect.ymin = my - radius;
  rect.xmax = mx + radius;
  rect.ymax = my + radius;

  GP_SpaceConversion gsc = {NULL};

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    pg_toggle_all(gpd, SEL_DESELECT, only_layer);
  }

  /* find visible strokes, and select if hit */
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
    gpencil_stroke_do_circle_sel(
        gpd, gpl, gps, &gsc, mx, my, radius, select, &rect, pg_identity_mat, selectmode, scale, false);
  }
  PG_EDITABLE_STROKES_END;

  const int changed = pg_selection_changed(gpd, before, before_len);
  free(before);
  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box / Lasso Select (gpencil_generic_select_exec)
 * \{ */

static bool pg_test_box(void *region,
                        const float diff_mat[4][4],
                        const float pt[3],
                        GP_SelectUserData *user_data)
{
  (void)region;
  (void)diff_mat;
  int co[2] = {0};
  if (pg_point_to_screen(pt, co)) {
    return BLI_rcti_isect_pt(&user_data->rect, co[0], co[1]);
  }
  return false;
}

static bool pg_test_lasso(void *region,
                          const float diff_mat[4][4],
                          const float pt[3],
                          GP_SelectUserData *user_data)
{
  (void)region;
  (void)diff_mat;
  int co[2] = {0};
  if (pg_point_to_screen(pt, co)) {
    /* test if in lasso boundbox + within the lasso noose */
    return (BLI_rcti_isect_pt(&user_data->rect, co[0], co[1]) &&
            BLI_lasso_is_point_inside(user_data->lasso_coords,
                                      (unsigned int)user_data->lasso_coords_len,
                                      co[0],
                                      co[1],
                                      INT_MAX));
  }
  return false;
}

/* gpencil_generic_stroke_select(): the per-stroke body is copied verbatim; the curve-edit block
 * between the two regions is intentionally left out (Project Grease has no edit-curves). */
static bool pg_generic_stroke_select(bGPdata *gpd,
                                     const bGPDlayer *only_layer,
                                     PGTestFn is_inside_fn,
                                     rcti box,
                                     GP_SelectUserData *user_data,
                                     const bool strokemode,
                                     const bool segmentmode,
                                     const eSelectOp sel_op,
                                     const float scale,
                                     const bool is_curve_edit)
{
  GP_SpaceConversion gsc = {NULL};
  bool changed = false;
  float select_mat[4][4];
  memcpy(select_mat, pg_identity_mat, sizeof(select_mat));
  (void)is_curve_edit;

  /* deselect all strokes first? */
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    pg_deselect_all_selected(gpd, only_layer);
    changed = true;
  }

  /* select/deselect points */
  PG_EDITABLE_STROKES_BEGIN (gpd, only_layer, gpl, gps) {
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
    bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
    bool whole = false;

    bGPDspoint *pt;
    int i;
    bool hit = false;
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      bGPDspoint *pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;

      /* Convert point coords to screen-space. Needs to use the evaluated point
       * to consider modifiers. */
      const bool is_inside = is_inside_fn(gsc.region, select_mat, &pt->x, user_data);
      if (strokemode == false) {
        const bool is_select = (pt_active->flag & GP_SPOINT_SELECT) != 0;
        const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
        if (sel_op_result != -1) {
          SET_FLAG_FROM_TEST(pt_active->flag, sel_op_result, GP_SPOINT_SELECT);
          changed = true;
          hit = true;

          /* Expand selection to segment. */
          if (segmentmode) {
            bool hit_select = (bool)(pt_active->flag & GP_SPOINT_SELECT);
            float r_hita[3], r_hitb[3];
            ED_gpencil_select_stroke_segment(
                gpd, gpl, gps_active, pt_active, hit_select, false, scale, r_hita, r_hitb);
          }
        }
      }
      else {
        if (is_inside) {
          hit = true;
          break;
        }
      }
    }

    /* If nothing hit, check if the mouse is inside a filled stroke using the center or
     * Box or lasso area. */
    if (!hit) {
      /* Only check filled strokes. */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0) {
        continue;
      }
      int mval[2];
      mval[0] = (box.xmax + box.xmin) / 2;
      mval[1] = (box.ymax + box.ymin) / 2;

      whole = ED_gpencil_stroke_point_is_inside(gps, &gsc, mval, gpstroke_iter.diff_mat);
    }

    /* if stroke mode expand selection. */
    if ((strokemode) || (whole)) {
      const bool is_select = BKE_gpencil_stroke_select_check(gps_active) || whole;
      const bool is_inside = hit || whole;
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          bGPDspoint *pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;

          if (sel_op_result) {
            pt_active->flag |= GP_SPOINT_SELECT;
          }
          else {
            pt_active->flag &= ~GP_SPOINT_SELECT;
          }
        }
        changed = true;
      }
    }
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_select.c */
    /* Ensure that stroke selection is in sync with its points */
    BKE_gpencil_stroke_sync_selection(gpd, gps_active);
/* END VERBATIM */
  }
  PG_EDITABLE_STROKES_END;

  return changed;
}

/* gpencil_generic_select_exec() for the non-curve, non-paint-mode case. */
static int pg_generic_select_exec(bGPdata *gpd,
                                  const bGPDlayer *only_layer,
                                  PGTestFn is_inside_fn,
                                  rcti box,
                                  GP_SelectUserData *user_data,
                                  int sel_op_in,
                                  int selectmode)
{
  if (sel_op_in < SEL_OP_ADD || sel_op_in > SEL_OP_XOR ||
      (selectmode != GP_SELECTMODE_POINT && selectmode != GP_SELECTMODE_STROKE))
  {
    return 0;
  }
  const bool strokemode = (selectmode == GP_SELECTMODE_STROKE);
  const bool segmentmode = false;
  const eSelectOp sel_op = (eSelectOp)sel_op_in;

  size_t before_len = 0;
  unsigned char *before = pg_selection_snapshot(gpd, &before_len);
  pg_generic_stroke_select(
      gpd, only_layer, is_inside_fn, box, user_data, strokemode, segmentmode, sel_op, 0.0f, false);
  const int changed = pg_selection_changed(gpd, before, before_len);
  free(before);
  return changed;
}

int pg_gp_select_box(bGPdata *gpd,
                     int xmin,
                     int ymin,
                     int xmax,
                     int ymax,
                     int sel_op,
                     int selectmode,
                     const bGPDlayer *only_layer)
{
  if (gpd == NULL) {
    return 0;
  }
  GP_SelectUserData data = {0};
  /* WM_operator_properties_border_to_rcti() */
  data.rect.xmin = xmin < xmax ? xmin : xmax;
  data.rect.xmax = xmin < xmax ? xmax : xmin;
  data.rect.ymin = ymin < ymax ? ymin : ymax;
  data.rect.ymax = ymin < ymax ? ymax : ymin;
  rcti rect = data.rect;
  return pg_generic_select_exec(gpd, only_layer, pg_test_box, rect, &data, sel_op, selectmode);
}

int pg_gp_select_lasso(bGPdata *gpd,
                       const int (*coords)[2],
                       int coords_len,
                       int sel_op,
                       int selectmode,
                       const bGPDlayer *only_layer)
{
  if (gpd == NULL || coords == NULL || coords_len < 3) {
    return 0;
  }
  GP_SelectUserData data = {0};
  data.lasso_coords = coords;
  data.lasso_coords_len = coords_len;

  /* Compute boundbox of lasso (for faster testing later). */
  BLI_lasso_boundbox(&data.rect, data.lasso_coords, (unsigned int)data.lasso_coords_len);

  rcti rect = data.rect;
  return pg_generic_select_exec(gpd, only_layer, pg_test_lasso, rect, &data, sel_op, selectmode);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public wrappers for the eSelectOp helpers and the JNI dispatcher
 * \{ */

int pg_select_op_action(int sel_op, int is_select, int is_inside)
{
  return ED_select_op_action((eSelectOp)sel_op, is_select != 0, is_inside != 0);
}

int pg_select_op_action_deselected(int sel_op, int is_select, int is_inside)
{
  return ED_select_op_action_deselected((eSelectOp)sel_op, is_select != 0, is_inside != 0);
}

int pg_select_op_modal(int sel_op, int is_first)
{
  return (int)ED_select_op_modal((eSelectOp)sel_op, is_first != 0);
}

int pg_gp_select_dispatch(bGPdata *gpd,
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

  /* The caller chooses the layer scope. The Android bridge passes NULL for Blender-style
   * edit selection across all visible, unlocked, editable layers, while tests and layer-specific
   * callers can still request a single layer. */
  const bGPDlayer *scope = active_layer;
  int changed = 0;

  switch (command) {
    case PG_SELECT_CMD_ALL:
      if (arg_count < 1) {
        return 0;
      }
      changed = pg_gp_select_all(gpd, (int)lroundf(args[0]), scope);
      break;
    case PG_SELECT_CMD_LINKED:
      changed = pg_gp_select_linked(gpd, scope);
      break;
    case PG_SELECT_CMD_ALTERNATE:
      changed = pg_gp_select_alternate(gpd, arg_count > 0 && args[0] != 0.0f, scope);
      break;
    case PG_SELECT_CMD_MORE:
      changed = pg_gp_select_more(gpd, scope);
      break;
    case PG_SELECT_CMD_LESS:
      changed = pg_gp_select_less(gpd, scope);
      break;
    case PG_SELECT_CMD_FIRST:
      changed = pg_gp_select_first(
          gpd, arg_count > 0 && args[0] != 0.0f, arg_count > 1 && args[1] != 0.0f, scope);
      break;
    case PG_SELECT_CMD_LAST:
      changed = pg_gp_select_last(
          gpd, arg_count > 0 && args[0] != 0.0f, arg_count > 1 && args[1] != 0.0f, scope);
      break;
    case PG_SELECT_CMD_GROUPED:
      if (arg_count < 1) {
        return 0;
      }
      changed = pg_gp_select_grouped(gpd, (int)lroundf(args[0]), scope);
      break;
    case PG_SELECT_CMD_LASSO: {
      if (arg_count < 2 + 3 * 2) {
        return 0;
      }
      const int count = (arg_count - 2) / 2;
      int(*coords)[2] = malloc(sizeof(int[2]) * (size_t)count);
      if (coords == NULL) {
        return 0;
      }
      for (int i = 0; i < count; i++) {
        coords[i][0] = (int)lroundf(args[2 + i * 2]);
        coords[i][1] = (int)lroundf(args[2 + i * 2 + 1]);
      }
      changed = pg_gp_select_lasso(
          gpd, (const int(*)[2])coords, count, (int)lroundf(args[0]), (int)lroundf(args[1]), scope);
      free(coords);
      break;
    }
    case PG_SELECT_CMD_BOX:
      if (arg_count < 6) {
        return 0;
      }
      changed = pg_gp_select_box(gpd,
                                 (int)lroundf(args[2]),
                                 (int)lroundf(args[3]),
                                 (int)lroundf(args[4]),
                                 (int)lroundf(args[5]),
                                 (int)lroundf(args[0]),
                                 (int)lroundf(args[1]),
                                 scope);
      break;
    case PG_SELECT_CMD_CIRCLE:
      if (arg_count < 5) {
        return 0;
      }
      changed = pg_gp_select_circle(gpd,
                                    (int)lroundf(args[2]),
                                    (int)lroundf(args[3]),
                                    (int)lroundf(args[4]),
                                    (int)lroundf(args[0]),
                                    arg_count > 5 ? (args[5] != 0.0f) : 1,
                                    (int)lroundf(args[1]),
                                    scope);
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

/** \} */
