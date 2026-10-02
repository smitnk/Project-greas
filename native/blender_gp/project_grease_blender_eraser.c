/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Blender 3.6.23 Legacy GP eraser (source/blender/editors/gpencil_legacy/gpencil_paint.c):
 * gpencil_stroke_eraser_dostroke(), gpencil_stroke_eraser_calc_influence(),
 * gpencil_stroke_soft_refine() and the rect/radius setup of gpencil_stroke_doeraser().
 *
 * gpencil_stroke_soft_refine() is a byte-identical copy (checked in CI by
 * tools/verify_blender_verbatim.py). The rest mirrors the original statement by statement;
 * what differs is only what Blender gets from the 3D view and operator context:
 *
 *  - Screen space is the canvas: points project with an identity transform, rounded to int.
 *  - No depth buffer, so the occlusion tests (GP_BRUSH_OCCLUDE_ERASER) are always "visible".
 *  - No select-mask (GP_PAINTFLAG_SELECTMASK) and no layer/object transform.
 *  - Deleting tagged points / freeing the stroke is left to the caller, which uses Blender's
 *    BKE_gpencil_stroke_delete_tagged_points() / BKE_gpencil_free_stroke() directly.
 *  - The stroke bounding-box pre-check (ED_gpencil_stroke_check_collision) is only a speed
 *    filter and is omitted.
 */

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#include "BLI_math_geom.h"
#include "DNA_gpencil_legacy_types.h"

#include "project_grease_blender_eraser.h"

#define PG_V2D_IS_CLIPPED 12000
/* GPENCIL_ALPHA_OPACITY_THRESH (BKE_gpencil_legacy.h) */
#define PG_ALPHA_OPACITY_THRESH 0.001f

typedef struct PGRect {
  int xmin, xmax, ymin, ymax;
} PGRect;

/* gpencil_point_to_xy() with the identity projection. */
static void pg_point_to_xy(const bGPDspoint *pt, int r_co[2])
{
  if (!isfinite(pt->x) || !isfinite(pt->y) || fabsf(pt->x) > 1.0e6f || fabsf(pt->y) > 1.0e6f) {
    r_co[0] = PG_V2D_IS_CLIPPED;
    r_co[1] = PG_V2D_IS_CLIPPED;
    return;
  }
  r_co[0] = (int)lroundf(pt->x);
  r_co[1] = (int)lroundf(pt->y);
}

static bool pg_clipped(const int co[2])
{
  return co[0] == PG_V2D_IS_CLIPPED || co[1] == PG_V2D_IS_CLIPPED;
}

/* BLI_rcti_isect_pt() */
static bool pg_rect_isect_pt(const PGRect *rect, const int co[2])
{
  return !(co[0] < rect->xmin || co[0] > rect->xmax || co[1] < rect->ymin || co[1] > rect->ymax);
}

/* len_v2v2_int() */
static float pg_len_v2v2_int(const int a[2], const int b[2])
{
  const float dx = (float)(a[0] - b[0]);
  const float dy = (float)(a[1] - b[1]);
  return sqrtf(dx * dx + dy * dy);
}

/* gpencil_stroke_inside_circle() -> edge_inside_circle() */
static bool pg_stroke_inside_circle(const float mval[2], int rad, const int a[2], const int b[2])
{
  const float screen_co_a[2] = {(float)a[0], (float)a[1]};
  const float screen_co_b[2] = {(float)b[0], (float)b[1]};
  const float radius_squared = (float)rad * (float)rad;
  return dist_squared_to_line_segment_v2(mval, screen_co_a, screen_co_b) < radius_squared;
}

/* gpencil_stroke_eraser_calc_influence(): apply a falloff effect to brush strength. */
static float pg_calc_influence(const PGEraserParams *p,
                               const int mval_i[2],
                               const int radius,
                               const int co[2])
{
  /* Linear Falloff... */
  float distance = pg_len_v2v2_int(mval_i, co);
  float fac;

  if (distance < 0.0f) {
    distance = 0.0f;
  }
  else if (distance > (float)radius) {
    distance = (float)radius;
  }
  fac = 1.0f - (distance / (float)radius);

  /* apply strength factor */
  fac *= p->draw_strength;

  /* Control this further using pen pressure */
  if (p->use_pressure) {
    fac *= p->pressure;
  }
  /* Return influence factor computed here */
  return fac;
}

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */
static void gpencil_stroke_soft_refine(bGPDstroke *gps)
{
  bGPDspoint *pt = NULL;
  bGPDspoint *pt2 = NULL;
  int i;

  /* Check if enough points. */
  if (gps->totpoints < 3) {
    return;
  }

  /* loop all points to untag any point that next is not tagged */
  pt = gps->points;
  for (i = 1; i < gps->totpoints - 1; i++, pt++) {
    if (pt->flag & GP_SPOINT_TAG) {
      pt2 = &gps->points[i + 1];
      if ((pt2->flag & GP_SPOINT_TAG) == 0) {
        pt->flag &= ~GP_SPOINT_TAG;
      }
    }
  }

  /* loop reverse all points to untag any point that previous is not tagged */
  pt = &gps->points[gps->totpoints - 1];
  for (i = gps->totpoints - 1; i > 0; i--, pt--) {
    if (pt->flag & GP_SPOINT_TAG) {
      pt2 = &gps->points[i - 1];
      if ((pt2->flag & GP_SPOINT_TAG) == 0) {
        pt->flag &= ~GP_SPOINT_TAG;
      }
    }
  }
}
/* END VERBATIM */

/* eraser tool - evaluation per stroke (gpencil_stroke_eraser_dostroke) */
int pg_eraser_dostroke(bGPDstroke *gps, const PGEraserParams *p)
{
  bGPDspoint *pt0, *pt1, *pt2;
  int pc0[2] = {0};
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int result = 0;

  if (gps == NULL || p == NULL || p->radius <= 0) {
    return 0;
  }

  const int radius = p->radius;
  const float *mval = p->mval;

  /* gpencil_stroke_doeraser(): rect is rectangle of eraser */
  PGRect rect;
  rect.xmin = (int)(mval[0] - (float)radius);
  rect.ymin = (int)(mval[1] - (float)radius);
  rect.xmax = (int)(mval[0] + (float)radius);
  rect.ymax = (int)(mval[1] + (float)radius);

  int mval_i[2];
  mval_i[0] = (int)lroundf(mval[0]); /* round_v2i_v2fl() */
  mval_i[1] = (int)lroundf(mval[1]);

  if (gps->totpoints == 0) {
    /* just free stroke */
    return PG_ERASER_FREE;
  }
  if (gps->totpoints == 1) {
    pg_point_to_xy(gps->points, pc1);
    /* Do bound-box check first. */
    if (!pg_clipped(pc1) && pg_rect_isect_pt(&rect, pc1)) {
      /* only check if point is inside */
      if (pg_len_v2v2_int(mval_i, pc1) <= (float)radius) {
        /* free stroke */
        return PG_ERASER_FREE;
      }
    }
    return 0;
  }
  if (p->mode == PG_ERASER_STROKE) {
    for (i = 0; (i + 1) < gps->totpoints; i++) {
      /* get points to work with */
      pt1 = gps->points + i;
      pg_point_to_xy(pt1, pc1);

      /* Do bound-box check first. */
      if (!pg_clipped(pc1) && pg_rect_isect_pt(&rect, pc1)) {
        /* only check if point is inside */
        if (pg_len_v2v2_int(mval_i, pc1) <= (float)radius) {
          /* free stroke */
          return PG_ERASER_FREE;
        }
      }
    }
    return 0;
  }

  /* Pressure threshold at which stroke should be culled */
  const float cull_thresh = 0.005f;

  /* Amount to decrease the pressure of each point with each stroke */
  const float strength = 0.1f;

  /* Perform culling? */
  bool do_cull = false;

  /* Clear Tags (the temp occlusion tags are never set here). */
  for (i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->flag &= ~GP_SPOINT_TAG;
  }

  /* First Pass: Loop over the points in the stroke
   *   1) Thin out parts of the stroke under the brush
   *   2) Tag "too thin" parts for removal (in second pass)
   */
  for (i = 0; (i + 1) < gps->totpoints; i++) {
    /* get points to work with */
    pt0 = i > 0 ? gps->points + i - 1 : NULL;
    pt1 = gps->points + i;
    pt2 = gps->points + i + 1;

    float inf1 = 0.0f;
    float inf2 = 0.0f;

    pg_point_to_xy(pt1, pc1);
    pg_point_to_xy(pt2, pc2);

    if (pt0) {
      pg_point_to_xy(pt0, pc0);
    }
    else {
      /* avoid null values */
      pc0[0] = pc1[0];
      pc0[1] = pc1[1];
    }

    /* Check that point segment of the bound-box of the eraser stroke. */
    if ((!pg_clipped(pc0) && pg_rect_isect_pt(&rect, pc0)) ||
        (!pg_clipped(pc1) && pg_rect_isect_pt(&rect, pc1)) ||
        (!pg_clipped(pc2) && pg_rect_isect_pt(&rect, pc2)))
    {
      /* Check if point segment of stroke had anything to do with
       * eraser region  (either within stroke painted, or on its lines)
       * - this assumes that line-width is irrelevant.
       */
      const int a[2] = {pc0[0], pc0[1]};
      const int b[2] = {pc2[0], pc2[1]};
      if (pg_stroke_inside_circle(mval, radius, a, b)) {
        /* Point is affected: */
        /* Adjust thickness
         *  - Influence of eraser falls off with distance from the middle of the eraser
         *  - Second point gets less influence, as it might get hit again in the next segment
         */

        /* Adjust strength if the eraser is soft */
        if (p->mode == PG_ERASER_SOFT) {
          const float f_strength = p->soft_strength;
          const float f_thickness = p->soft_thickness;
          float influence = 0.0f;

          if (pt0) {
            influence = pg_calc_influence(p, mval_i, radius, pc0);
            pt0->strength -= influence * strength * f_strength * 0.5f;
            if (pt0->strength < 0.0f) {
              pt0->strength = 0.0f;
            }
            pt0->pressure -= influence * strength * f_thickness * 0.5f;
          }

          influence = pg_calc_influence(p, mval_i, radius, pc1);
          pt1->strength -= influence * strength * f_strength;
          if (pt1->strength < 0.0f) {
            pt1->strength = 0.0f;
          }
          pt1->pressure -= influence * strength * f_thickness;

          influence = pg_calc_influence(p, mval_i, radius, pc2);
          pt2->strength -= influence * strength * f_strength * 0.5f;
          if (pt2->strength < 0.0f) {
            pt2->strength = 0.0f;
          }
          pt2->pressure -= influence * strength * f_thickness * 0.5f;

          /* if invisible, delete point */
          if ((pt0) && ((pt0->strength <= PG_ALPHA_OPACITY_THRESH) ||
                        (pt0->pressure < cull_thresh)))
          {
            pt0->flag |= GP_SPOINT_TAG;
            do_cull = true;
          }
          if ((pt1->strength <= PG_ALPHA_OPACITY_THRESH) || (pt1->pressure < cull_thresh)) {
            pt1->flag |= GP_SPOINT_TAG;
            do_cull = true;
          }
          if ((pt2->strength <= PG_ALPHA_OPACITY_THRESH) || (pt2->pressure < cull_thresh)) {
            pt2->flag |= GP_SPOINT_TAG;
            do_cull = true;
          }

          inf1 = 1.0f;
          inf2 = 1.0f;
          result |= PG_ERASER_MODIFIED;
        }
        else {
          /* Erase point. Only erase if the eraser is on top of the point. */
          inf1 = pg_calc_influence(p, mval_i, radius, pc1);
          if (inf1 > 0.0f) {
            pt1->pressure = 0.0f;
            pt1->flag |= GP_SPOINT_TAG;
            do_cull = true;
            result |= PG_ERASER_MODIFIED;
          }
          inf2 = pg_calc_influence(p, mval_i, radius, pc2);
          if (inf2 > 0.0f) {
            pt2->pressure = 0.0f;
            pt2->flag |= GP_SPOINT_TAG;
            do_cull = true;
            result |= PG_ERASER_MODIFIED;
          }
        }

        /* 2) Tag any point with overly low influence for removal in the next pass */
        if ((inf1 > 0.0f) &&
            ((pt1->pressure < cull_thresh) || p->hard_flag || p->mode == PG_ERASER_HARD))
        {
          pt1->flag |= GP_SPOINT_TAG;
          do_cull = true;
        }
        if ((inf1 > 2.0f) &&
            ((pt2->pressure < cull_thresh) || p->hard_flag || p->mode == PG_ERASER_HARD))
        {
          pt2->flag |= GP_SPOINT_TAG;
          do_cull = true;
        }
      }
    }
  }

  /* Second Pass: Remove any points that are tagged */
  if (do_cull) {
    /* if soft eraser, must analyze points to be sure the stroke ends
     * don't get rounded */
    if (p->mode == PG_ERASER_SOFT) {
      gpencil_stroke_soft_refine(gps);
    }
    result |= PG_ERASER_CULL;
  }
  return result;
}
