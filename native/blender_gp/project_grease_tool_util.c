/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Native tool session (project_grease_tool_session.h): helpers shared by the brush ports.
 *
 * Blender's sculpt / vertex paint / weight paint operators test points in region (screen) pixels:
 * points are projected with ED_view3d_project_int_global() and the brush radius is in pixels.
 * Project Grease strokes lie in the canvas plane (z = 0, canvas units), so the "view" here is the
 * canvas scaled by the on-screen pixels per canvas unit: screen = canvas * px_per_unit, and a
 * screen delta maps back by 1 / px_per_unit. The view3d entry points below implement exactly that
 * (no perspective, identity object and layer matrices). The gpencil_utils.c helpers, Blender's
 * edge_inside_circle(), BKE_brush_curve_strength() and BKE_boundbox_init_from_minmax() are
 * carried verbatim.
 */
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_gpencil_geom_legacy.h"

#include "project_grease_tool_view.h"

/* ---- Project Grease view: canvas plane scaled by px_per_unit --------------------------------- */
/* Every region handed to the ported code is the one embedded in a PGToolView. */
static float pgt_scale(const ARegion *region)
{
  const PGToolView *view = (const PGToolView *)((const char *)region - offsetof(PGToolView, region));
  return (view->px_per_unit > 0.0f) ? view->px_per_unit : 1.0f;
}

void pg_tool_view_init(PGToolView *view, GP_SpaceConversion *gsc, float px_per_unit)
{
  memset(view, 0, sizeof(*view));
  view->px_per_unit = (isfinite(px_per_unit) && px_per_unit > 0.0f) ? px_per_unit : 1.0f;
  view->region.regiondata = &view->rv3d; /* read as RegionView3D by the ports */
  view->region.winx = (short)min_ff(100.0f * view->px_per_unit, 32000.0f);
  view->region.winy = view->region.winx;
  view->area.spacetype = SPACE_VIEW3D;
  unit_m4(view->rv3d.persinv);
  unit_m4(view->ob.object_to_world);
  unit_m4(view->ob.world_to_object);
  view->scene.toolsettings = &view->ts;
  memset(gsc, 0, sizeof(*gsc));
  gsc->scene = &view->scene;
  gsc->ob = &view->ob;
  gsc->area = &view->area;
  gsc->region = &view->region;
  gsc->v2d = &view->region.v2d;
  unit_m4(gsc->mat);
}

eV3DProjStatus ED_view3d_project_int_global(const ARegion *region,
                                            const float co[3],
                                            int r_co[2],
                                            const eV3DProjTest flag)
{
  (void)flag;
  const float s = pgt_scale(region);
  r_co[0] = (int)(co[0] * s);
  r_co[1] = (int)(co[1] * s);
  return V3D_PROJ_RET_OK;
}

eV3DProjStatus ED_view3d_project_float_global(const ARegion *region,
                                              const float co[3],
                                              float r_co[2],
                                              const eV3DProjTest flag)
{
  (void)flag;
  const float s = pgt_scale(region);
  r_co[0] = co[0] * s;
  r_co[1] = co[1] * s;
  return V3D_PROJ_RET_OK;
}

float ED_view3d_calc_zfac(const RegionView3D *rv3d, const float co[3])
{
  (void)rv3d;
  (void)co;
  return 1.0f;
}

float ED_view3d_calc_zfac_ex(const RegionView3D *rv3d, const float co[3], bool *r_flip)
{
  (void)rv3d;
  (void)co;
  if (r_flip) {
    *r_flip = false;
  }
  return 1.0f;
}

void ED_view3d_win_to_delta(const ARegion *region, const float xy_delta[2], float zfac, float r_out[3])
{
  (void)zfac;
  const float s = pgt_scale(region);
  r_out[0] = xy_delta[0] / s;
  r_out[1] = xy_delta[1] / s;
  r_out[2] = 0.0f;
}

/* 2D-space strokes (GP_STROKE_2DSPACE, image/sequencer editors) do not exist in Project Grease. */
void UI_view2d_view_to_region_clip(const View2D *v2d, float x, float y, int *r_region_x, int *r_region_y)
{
  (void)v2d;
  *r_region_x = (int)x;
  *r_region_y = (int)y;
}

void UI_view2d_region_to_view(const View2D *v2d, float x, float y, float *r_view_x, float *r_view_y)
{
  (void)v2d;
  *r_view_x = x;
  *r_view_y = y;
}

/* bGPdata::mat[] holds the materials (no Object): BKE_gpencil_material_settings(ob, act). */
static MaterialGPencilStyle pgt_default_style;
MaterialGPencilStyle *pg_tool_material_style(const bGPdata *gpd, short act)
{
  const int index = act - 1;
  if (gpd != NULL && gpd->mat != NULL && index >= 0 && index < gpd->totcol && gpd->mat[index] != NULL &&
      gpd->mat[index]->gp_style != NULL)
  {
    return gpd->mat[index]->gp_style;
  }
  return &pgt_default_style;
}

/* Custom brush curves are not used: the sessions set a curve preset (BRUSH_CURVE_SMOOTH). */
#define BKE_curvemapping_evaluateF(cumap, cur, value) (1.0f)

/* view3d_select.cc */
/* BEGIN VERBATIM source/blender/editors/space_view3d/view3d_select.cc */
bool edge_inside_circle(const float cent[2],
                        float radius,
                        const float screen_co_a[2],
                        const float screen_co_b[2])
{
  const float radius_squared = radius * radius;
  return (dist_squared_to_line_segment_v2(cent, screen_co_a, screen_co_b) < radius_squared);
}
/* END VERBATIM */

/* brush.cc */
/* BEGIN VERBATIM source/blender/blenkernel/intern/brush.cc */
float BKE_brush_curve_strength(const Brush *br, float p, const float len)
{
  float strength = 1.0f;

  if (p >= len) {
    return 0;
  }

  p = p / len;
  p = 1.0f - p;

  switch (br->curve_preset) {
    case BRUSH_CURVE_CUSTOM:
      strength = BKE_curvemapping_evaluateF(br->curve, 0, 1.0f - p);
      break;
    case BRUSH_CURVE_SHARP:
      strength = p * p;
      break;
    case BRUSH_CURVE_SMOOTH:
      strength = 3.0f * p * p - 2.0f * p * p * p;
      break;
    case BRUSH_CURVE_SMOOTHER:
      strength = pow3f(p) * (p * (p * 6.0f - 15.0f) + 10.0f);
      break;
    case BRUSH_CURVE_ROOT:
      strength = sqrtf(p);
      break;
    case BRUSH_CURVE_LIN:
      strength = p;
      break;
    case BRUSH_CURVE_CONSTANT:
      strength = 1.0f;
      break;
    case BRUSH_CURVE_SPHERE:
      strength = sqrtf(2 * p - p * p);
      break;
    case BRUSH_CURVE_POW4:
      strength = p * p * p * p;
      break;
    case BRUSH_CURVE_INVSQUARE:
      strength = p * (2.0f - p);
      break;
  }

  return strength;
}
/* END VERBATIM */

/* object.cc */
/* BEGIN VERBATIM source/blender/blenkernel/intern/object.cc */
void BKE_boundbox_init_from_minmax(BoundBox *bb, const float min[3], const float max[3])
{
  bb->vec[0][0] = bb->vec[1][0] = bb->vec[2][0] = bb->vec[3][0] = min[0];
  bb->vec[4][0] = bb->vec[5][0] = bb->vec[6][0] = bb->vec[7][0] = max[0];

  bb->vec[0][1] = bb->vec[1][1] = bb->vec[4][1] = bb->vec[5][1] = min[1];
  bb->vec[2][1] = bb->vec[3][1] = bb->vec[6][1] = bb->vec[7][1] = max[1];

  bb->vec[0][2] = bb->vec[3][2] = bb->vec[4][2] = bb->vec[7][2] = min[2];
  bb->vec[1][2] = bb->vec[2][2] = bb->vec[5][2] = bb->vec[6][2] = max[2];
}
/* END VERBATIM */

/* gpencil_utils.c */
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
bool gpencil_stroke_inside_circle(const float mval[2], int rad, int x0, int y0, int x1, int y1)
{
  /* simple within-radius check for now */
  const float screen_co_a[2] = {x0, y0};
  const float screen_co_b[2] = {x1, y1};

  if (edge_inside_circle(mval, rad, screen_co_a, screen_co_b)) {
    return true;
  }

  /* not inside */
  return false;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
void gpencil_point_to_world_space(const bGPDspoint *pt,
                                  const float diff_mat[4][4],
                                  bGPDspoint *r_pt)
{
  mul_v3_m4v3(&r_pt->x, diff_mat, &pt->x);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
void gpencil_point_to_xy(
    const GP_SpaceConversion *gsc, const bGPDstroke *gps, const bGPDspoint *pt, int *r_x, int *r_y)
{
  const ARegion *region = gsc->region;
  const View2D *v2d = gsc->v2d;
  const rctf *subrect = gsc->subrect;
  int xyval[2];

  /* sanity checks */
  BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->area->spacetype == SPACE_VIEW3D));
  BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->area->spacetype != SPACE_VIEW3D));

  if (gps->flag & GP_STROKE_3DSPACE) {
    if (ED_view3d_project_int_global(region, &pt->x, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK)
    {
      *r_x = xyval[0];
      *r_y = xyval[1];
    }
    else {
      *r_x = V2D_IS_CLIPPED;
      *r_y = V2D_IS_CLIPPED;
    }
  }
  else if (gps->flag & GP_STROKE_2DSPACE) {
    float vec[3] = {pt->x, pt->y, 0.0f};
    mul_m4_v3(gsc->mat, vec);
    UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], r_x, r_y);
  }
  else {
    if (subrect == NULL) {
      /* normal 3D view (or view space) */
      *r_x = (int)(pt->x / 100 * region->winx);
      *r_y = (int)(pt->y / 100 * region->winy);
    }
    else {
      /* camera view, use subrect */
      *r_x = (int)((pt->x / 100) * BLI_rctf_size_x(subrect)) + subrect->xmin;
      *r_y = (int)((pt->y / 100) * BLI_rctf_size_y(subrect)) + subrect->ymin;
    }
  }
}

void gpencil_point_to_xy_fl(const GP_SpaceConversion *gsc,
                            const bGPDstroke *gps,
                            const bGPDspoint *pt,
                            float *r_x,
                            float *r_y)
{
  const ARegion *region = gsc->region;
  const View2D *v2d = gsc->v2d;
  const rctf *subrect = gsc->subrect;
  float xyval[2];

  /* sanity checks */
  BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->area->spacetype == SPACE_VIEW3D));
  BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->area->spacetype != SPACE_VIEW3D));

  if (gps->flag & GP_STROKE_3DSPACE) {
    if (ED_view3d_project_float_global(region, &pt->x, xyval, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK) {
      *r_x = xyval[0];
      *r_y = xyval[1];
    }
    else {
      *r_x = 0.0f;
      *r_y = 0.0f;
    }
  }
  else if (gps->flag & GP_STROKE_2DSPACE) {
    float vec[3] = {pt->x, pt->y, 0.0f};
    int t_x, t_y;

    mul_m4_v3(gsc->mat, vec);
    UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], &t_x, &t_y);

    if ((t_x == t_y) && (t_x == V2D_IS_CLIPPED)) {
      /* XXX: Or should we just always use the values as-is? */
      *r_x = 0.0f;
      *r_y = 0.0f;
    }
    else {
      *r_x = (float)t_x;
      *r_y = (float)t_y;
    }
  }
  else {
    if (subrect == NULL) {
      /* normal 3D view (or view space) */
      *r_x = (pt->x / 100.0f * region->winx);
      *r_y = (pt->y / 100.0f * region->winy);
    }
    else {
      /* camera view, use subrect */
      *r_x = ((pt->x / 100.0f) * BLI_rctf_size_x(subrect)) + subrect->xmin;
      *r_y = ((pt->y / 100.0f) * BLI_rctf_size_y(subrect)) + subrect->ymin;
    }
  }
}

void gpencil_point_3d_to_xy(const GP_SpaceConversion *gsc,
                            const short flag,
                            const float pt[3],
                            float xy[2])
{
  const ARegion *region = gsc->region;
  const View2D *v2d = gsc->v2d;
  const rctf *subrect = gsc->subrect;
  float xyval[2];

  /* sanity checks */
  BLI_assert(gsc->area->spacetype == SPACE_VIEW3D);

  if (flag & GP_STROKE_3DSPACE) {
    if (ED_view3d_project_float_global(region, pt, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
      xy[0] = xyval[0];
      xy[1] = xyval[1];
    }
    else {
      xy[0] = 0.0f;
      xy[1] = 0.0f;
    }
  }
  else if (flag & GP_STROKE_2DSPACE) {
    float vec[3] = {pt[0], pt[1], 0.0f};
    int t_x, t_y;

    mul_m4_v3(gsc->mat, vec);
    UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], &t_x, &t_y);

    if ((t_x == t_y) && (t_x == V2D_IS_CLIPPED)) {
      /* XXX: Or should we just always use the values as-is? */
      xy[0] = 0.0f;
      xy[1] = 0.0f;
    }
    else {
      xy[0] = (float)t_x;
      xy[1] = (float)t_y;
    }
  }
  else {
    if (subrect == NULL) {
      /* normal 3D view (or view space) */
      xy[0] = (pt[0] / 100.0f * region->winx);
      xy[1] = (pt[1] / 100.0f * region->winy);
    }
    else {
      /* camera view, use subrect */
      xy[0] = ((pt[0] / 100.0f) * BLI_rctf_size_x(subrect)) + subrect->xmin;
      xy[1] = ((pt[1] / 100.0f) * BLI_rctf_size_y(subrect)) + subrect->ymin;
    }
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
void ED_gpencil_projected_2d_bound_box(const GP_SpaceConversion *gsc,
                                       const bGPDstroke *gps,
                                       const float diff_mat[4][4],
                                       float r_min[2],
                                       float r_max[2])
{
  float bounds[8][2];
  BoundBox bb;
  BKE_boundbox_init_from_minmax(&bb, gps->boundbox_min, gps->boundbox_max);

  /* Project 8 vertices in 2D. */
  for (int i = 0; i < 8; i++) {
    bGPDspoint pt_dummy, pt_dummy_ps;
    copy_v3_v3(&pt_dummy.x, bb.vec[i]);
    gpencil_point_to_world_space(&pt_dummy, diff_mat, &pt_dummy_ps);
    gpencil_point_to_xy_fl(gsc, gps, &pt_dummy_ps, &bounds[i][0], &bounds[i][1]);
  }

  /* Take extremes. */
  INIT_MINMAX2(r_min, r_max);
  for (int i = 0; i < 8; i++) {
    minmax_v2v2_v2(r_min, r_max, bounds[i]);
  }

  /* Ensure the bounding box is oriented to axis. */
  if (r_max[0] < r_min[0]) {
    SWAP(float, r_min[0], r_max[0]);
  }
  if (r_max[1] < r_min[1]) {
    SWAP(float, r_min[1], r_max[1]);
  }
}

bool ED_gpencil_stroke_check_collision(const GP_SpaceConversion *gsc,
                                       bGPDstroke *gps,
                                       const float mval[2],
                                       const int radius,
                                       const float diff_mat[4][4])
{
  const int offset = (int)ceil(sqrt((radius * radius) * 2));
  float boundbox_min[2];
  float boundbox_max[2];

  /* Check we have something to use (only for old files). */
  if (is_zero_v3(gps->boundbox_min)) {
    BKE_gpencil_stroke_boundingbox_calc(gps);
  }

  ED_gpencil_projected_2d_bound_box(gsc, gps, diff_mat, boundbox_min, boundbox_max);

  rcti rect_stroke = {boundbox_min[0], boundbox_max[0], boundbox_min[1], boundbox_max[1]};

  /* For mouse, add a small offset to avoid false negative in corners. */
  rcti rect_mouse = {mval[0] - offset, mval[0] + offset, mval[1] - offset, mval[1] + offset};

  /* Check collision between both rectangles. */
  return BLI_rcti_isect(&rect_stroke, &rect_mouse, NULL);
}

bool ED_gpencil_stroke_point_is_inside(const bGPDstroke *gps,
                                       const GP_SpaceConversion *gsc,
                                       const int mval[2],
                                       const float diff_mat[4][4])
{
  bool hit = false;
  if (gps->totpoints == 0) {
    return hit;
  }

  int(*mcoords)[2] = NULL;
  int len = gps->totpoints;
  mcoords = MEM_mallocN(sizeof(int[2]) * len, __func__);

  /* Convert stroke to 2D array of points. */
  const bGPDspoint *pt;
  int i;
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    bGPDspoint pt2;
    gpencil_point_to_world_space(pt, diff_mat, &pt2);
    gpencil_point_to_xy(gsc, gps, &pt2, &mcoords[i][0], &mcoords[i][1]);
  }

  /* Compute bound-box of lasso (for faster testing later). */
  rcti rect;
  BLI_lasso_boundbox(&rect, mcoords, len);

  /* Test if point inside stroke. */
  hit = (!ELEM(V2D_IS_CLIPPED, mval[0], mval[1]) && BLI_rcti_isect_pt(&rect, mval[0], mval[1]) &&
         BLI_lasso_is_point_inside(mcoords, len, mval[0], mval[1], INT_MAX));

  /* Free memory. */
  MEM_SAFE_FREE(mcoords);

  return hit;
}
/* END VERBATIM */

/* ---- runtime links (see project_grease_tool_view.h) ----------------------------------------- */
void pg_tool_link_runtime(bGPdata *gpd, bool link)
{
  if (gpd == NULL) {
    return;
  }
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        gps->runtime.gps_orig = NULL;
        for (int i = 0; i < gps->totpoints; i++) {
          gps->points[i].runtime.pt_orig = link ? &gps->points[i] : NULL;
          gps->points[i].runtime.idx_orig = link ? i : 0;
        }
      }
    }
  }
}
