/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Sculpt mode brushes for the native tool session: gpencil_sculpt_paint.c (Blender 3.6.23).
 *
 * Verbatim: the brush callbacks (smooth, thickness, strength, grab, push, pinch, twist, randomize),
 * the influence / invert checks and gpencil_sculpt_brush_do_stroke() with its per-point hit test.
 * Adapted (no bContext / depsgraph / RNA): the operator context struct, lock axis, do_frame,
 * apply_standard, brush_apply, init and exit. The view is the canvas plane (project_grease_tool_
 * util.c), object and layer matrices are identity, there are no evaluated copies (points are
 * linked to themselves, pg_tool_link_runtime) and no auto-masking or clone brush.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_tool_brushes.h"
#include "project_grease_tool_view.h"

/* Operator context (gpencil_sculpt_paint.c tGP_BrushEditData, members used by the ports). */
typedef struct tGP_BrushEditData {
  Scene *scene;
  Object *object;
  ARegion *region;
  bGPdata *gpd;
  Brush *brush;
  eGP_Sculpt_Flag flag;
  eGP_Sculpt_SelectMaskFlag mask;
  GP_SpaceConversion gsc;
  bool is_transformed;
  bool first;
  bool is_multiframe;
  float mval[2], mval_prev[2];
  float pressure, pressure_prev;
  float dvec[3];
  float rot_eval;
  float mf_falloff;
  rcti brush_rect;
  GHash *stroke_customdata;
  float inv_mat[4][4];
  RNG *rng;
} tGP_BrushEditData;

typedef bool (*GP_BrushApplyCb)(tGP_BrushEditData *gso,
                                bGPDstroke *gps,
                                float rotation,
                                int pt_index,
                                const int radius,
                                const int co[2]);

/* gpencil_sculpt_compute_lock_axis(): the canvas has no 3D cursor; X / Y / Z locks keep the
 * coordinate, VIEW and CURSOR (no cursor) leave the point as moved. */
static void gpencil_sculpt_compute_lock_axis(tGP_BrushEditData *gso,
                                             bGPDspoint *pt,
                                             const float save_pt[3])
{
  switch (gso->scene->toolsettings->gp_sculpt.lock_axis) {
    case GP_LOCKAXIS_X:
      pt->x = save_pt[0];
      break;
    case GP_LOCKAXIS_Y:
      pt->y = save_pt[1];
      break;
    case GP_LOCKAXIS_Z:
      pt->z = save_pt[2];
      break;
    default:
      break;
  }
}

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_sculpt_paint.c */
/* Invert behavior of brush? */
static bool gpencil_brush_invert_check(tGP_BrushEditData *gso)
{
  /* The basic setting is the brush's setting (from the panel) */
  bool invert = ((gso->brush->gpencil_settings->sculpt_flag & GP_SCULPT_FLAG_INVERT) != 0) ||
                (gso->brush->gpencil_settings->sculpt_flag & BRUSH_DIR_IN);
  /* During runtime, the user can hold down the Ctrl key to invert the basic behavior */
  if (gso->flag & GP_SCULPT_FLAG_INVERT) {
    invert ^= true;
  }

  /* set temporary status */
  if (invert) {
    gso->brush->gpencil_settings->sculpt_flag |= GP_SCULPT_FLAG_TMP_INVERT;
  }
  else {
    gso->brush->gpencil_settings->sculpt_flag &= ~GP_SCULPT_FLAG_TMP_INVERT;
  }

  return invert;
}

/* Compute strength of effect */
static float gpencil_brush_influence_calc(tGP_BrushEditData *gso,
                                          const int radius,
                                          const int co[2])
{
  Brush *brush = gso->brush;

  /* basic strength factor from brush settings */
  float influence = brush->alpha;

  /* use pressure? */
  if (brush->gpencil_settings->flag & GP_BRUSH_USE_PRESSURE) {
    influence *= gso->pressure;
  }

  /* distance fading */
  int mval_i[2];
  round_v2i_v2fl(mval_i, gso->mval);
  float distance = (float)len_v2v2_int(mval_i, co);

  /* Apply Brush curve. */
  float brush_falloff = BKE_brush_curve_strength(brush, distance, (float)radius);
  influence *= brush_falloff;

  /* apply multiframe falloff */
  influence *= gso->mf_falloff;

  /* return influence */
  return influence;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_sculpt_paint.c */
/* ----------------------------------------------- */
/* Smooth Brush */

/* A simple (but slower + inaccurate)
 * smooth-brush implementation to test the algorithm for stroke smoothing. */
static bool gpencil_brush_smooth_apply(tGP_BrushEditData *gso,
                                       bGPDstroke *gps,
                                       float UNUSED(rot_eval),
                                       int pt_index,
                                       const int radius,
                                       const int co[2])
{
  float inf = gpencil_brush_influence_calc(gso, radius, co);

  /* perform smoothing */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_POSITION) {
    BKE_gpencil_stroke_smooth_point(gps, pt_index, inf, 2, false, false, gps);
  }
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_STRENGTH) {
    BKE_gpencil_stroke_smooth_strength(gps, pt_index, inf, 2, gps);
  }
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_THICKNESS) {
    BKE_gpencil_stroke_smooth_thickness(gps, pt_index, inf, 2, gps);
  }
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_UV) {
    BKE_gpencil_stroke_smooth_uv(gps, pt_index, inf, 2, gps);
  }

  return true;
}

/* ----------------------------------------------- */
/* Line Thickness Brush */

/* Make lines thicker or thinner by the specified amounts */
static bool gpencil_brush_thickness_apply(tGP_BrushEditData *gso,
                                          bGPDstroke *gps,
                                          float UNUSED(rot_eval),
                                          int pt_index,
                                          const int radius,
                                          const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float inf;

  /* Compute strength of effect
   * - We divide the strength by 10, so that users can set "sane" values.
   *   Otherwise, good default values are in the range of 0.093
   */
  inf = gpencil_brush_influence_calc(gso, radius, co) / 10.0f;

  /* apply */
  /* XXX: this is much too strong,
   * and it should probably do some smoothing with the surrounding stuff. */
  if (gpencil_brush_invert_check(gso)) {
    /* make line thinner - reduce stroke pressure */
    pt->pressure -= inf;
  }
  else {
    /* make line thicker - increase stroke pressure */
    pt->pressure += inf;
  }

  /* Pressure should stay within [0.0, 1.0]
   * However, it is nice for volumetric strokes to be able to exceed
   * the upper end of this range. Therefore, we don't actually clamp
   * down on the upper end.
   */
  if (pt->pressure < 0.0f) {
    pt->pressure = 0.0f;
  }

  return true;
}

/* ----------------------------------------------- */
/* Color Strength Brush */

/* Make color more or less transparent by the specified amounts */
static bool gpencil_brush_strength_apply(tGP_BrushEditData *gso,
                                         bGPDstroke *gps,
                                         float UNUSED(rot_eval),
                                         int pt_index,
                                         const int radius,
                                         const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float inf;

  /* Compute strength of effect */
  inf = gpencil_brush_influence_calc(gso, radius, co) * 0.125f;

  /* Invert effect. */
  if (gpencil_brush_invert_check(gso)) {
    inf *= -1.0f;
  }

  pt->strength = clamp_f(pt->strength + inf, 0.0f, 1.0f);

  return true;
}

/* ----------------------------------------------- */
/* Grab Brush */

/* Custom data per stroke for the Grab Brush
 *
 * This basically defines the strength of the effect for each
 * affected stroke point that was within the initial range of
 * the brush region.
 */
typedef struct tGPSB_Grab_StrokeData {
  /* array of indices to corresponding points in the stroke */
  int *points;
  /* array of influence weights for each of the included points */
  float *weights;
  /* angles to calc transformation */
  float *rot_eval;

  /* capacity of the arrays */
  int capacity;
  /* actual number of items currently stored */
  int size;
} tGPSB_Grab_StrokeData;

/**
 * Initialize custom data for handling this stroke.
 */
static void gpencil_brush_grab_stroke_init(tGP_BrushEditData *gso, bGPDstroke *gps)
{
  tGPSB_Grab_StrokeData *data = NULL;

  BLI_assert(gps->totpoints > 0);

  /* Check if there are buffers already (from a prior run) */
  if (BLI_ghash_haskey(gso->stroke_customdata, gps)) {
    /* Ensure that the caches are empty
     * - Since we reuse these between different strokes, we don't
     *   want the previous invocation's data polluting the arrays
     */
    data = BLI_ghash_lookup(gso->stroke_customdata, gps);
    BLI_assert(data != NULL);

    data->size = 0; /* minimum requirement - so that we can repopulate again */

    memset(data->points, 0, sizeof(int) * data->capacity);
    memset(data->weights, 0, sizeof(float) * data->capacity);
    memset(data->rot_eval, 0, sizeof(float) * data->capacity);
  }
  else {
    /* Create new instance */
    data = MEM_callocN(sizeof(tGPSB_Grab_StrokeData), "GP Stroke Grab Data");

    data->capacity = gps->totpoints;
    data->size = 0;

    data->points = MEM_callocN(sizeof(int) * data->capacity, "GP Stroke Grab Indices");
    data->weights = MEM_callocN(sizeof(float) * data->capacity, "GP Stroke Grab Weights");
    data->rot_eval = MEM_callocN(sizeof(float) * data->capacity, "GP Stroke Grab Rotations");

    /* hook up to the cache */
    BLI_ghash_insert(gso->stroke_customdata, gps, data);
  }
}

/* store references to stroke points in the initial stage */
static bool gpencil_brush_grab_store_points(tGP_BrushEditData *gso,
                                            bGPDstroke *gps,
                                            float rot_eval,
                                            int pt_index,
                                            const int radius,
                                            const int co[2])
{
  tGPSB_Grab_StrokeData *data = BLI_ghash_lookup(gso->stroke_customdata, gps);
  float inf = gpencil_brush_influence_calc(gso, radius, co);

  BLI_assert(data != NULL);
  BLI_assert(data->size < data->capacity);

  /* insert this point into the set of affected points */
  data->points[data->size] = pt_index;
  data->weights[data->size] = inf;
  data->rot_eval[data->size] = rot_eval;
  data->size++;

  /* done */
  return true;
}

/* Compute effect vector for grab brush */
static void gpencil_brush_grab_calc_dvec(tGP_BrushEditData *gso)
{
  /* Convert mouse-movements to movement vector */
  RegionView3D *rv3d = gso->region->regiondata;
  float *rvec = gso->object->loc;
  const float zfac = ED_view3d_calc_zfac(rv3d, rvec);

  float mval_f[2];

  /* Convert from 2D screen-space to 3D. */
  mval_f[0] = (float)(gso->mval[0] - gso->mval_prev[0]);
  mval_f[1] = (float)(gso->mval[1] - gso->mval_prev[1]);

  /* apply evaluated data transformation */
  if (gso->rot_eval != 0.0f) {
    const float cval = cos(gso->rot_eval);
    const float sval = sin(gso->rot_eval);
    float r[2];
    r[0] = (mval_f[0] * cval) - (mval_f[1] * sval);
    r[1] = (mval_f[0] * sval) + (mval_f[1] * cval);
    copy_v2_v2(mval_f, r);
  }

  ED_view3d_win_to_delta(gso->region, mval_f, zfac, gso->dvec);
}

/* Apply grab transform to all relevant points of the affected strokes */
static void gpencil_brush_grab_apply_cached(tGP_BrushEditData *gso,
                                            bGPDstroke *gps,
                                            const float diff_mat[4][4])
{
  tGPSB_Grab_StrokeData *data = BLI_ghash_lookup(gso->stroke_customdata, gps);
  /* If a new frame is created, could be impossible find the stroke. */
  if (data == NULL) {
    return;
  }

  float matrix[4][4], inverse_diff_mat[4][4];
  copy_m4_m4(matrix, diff_mat);
  zero_axis_bias_m4(matrix);
  invert_m4_m4(inverse_diff_mat, matrix);

  /* Apply dvec to all of the stored points */
  for (int i = 0; i < data->size; i++) {
    bGPDspoint *pt = &gps->points[data->points[i]];
    float delta[3] = {0.0f};

    /* get evaluated transformation */
    gso->rot_eval = data->rot_eval[i];
    gpencil_brush_grab_calc_dvec(gso);

    /* adjust the amount of displacement to apply */
    mul_v3_v3fl(delta, gso->dvec, data->weights[i]);

    float fpt[3];
    float save_pt[3];
    copy_v3_v3(save_pt, &pt->x);
    /* apply transformation */
    mul_v3_m4v3(fpt, diff_mat, &pt->x);
    /* apply */
    add_v3_v3v3(&pt->x, fpt, delta);
    /* undo transformation to the init parent position */
    mul_m4_v3(inverse_diff_mat, &pt->x);

    /* compute lock axis */
    gpencil_sculpt_compute_lock_axis(gso, pt, save_pt);
  }
}

/* free customdata used for handling this stroke */
static void gpencil_brush_grab_stroke_free(void *ptr)
{
  tGPSB_Grab_StrokeData *data = (tGPSB_Grab_StrokeData *)ptr;

  /* free arrays */
  MEM_SAFE_FREE(data->points);
  MEM_SAFE_FREE(data->weights);
  MEM_SAFE_FREE(data->rot_eval);

  /* ... and this item itself, since it was also allocated */
  MEM_freeN(data);
}

/* ----------------------------------------------- */
/* Push Brush */
/* NOTE: Depends on gpencil_brush_grab_calc_dvec() */
static bool gpencil_brush_push_apply(tGP_BrushEditData *gso,
                                     bGPDstroke *gps,
                                     float UNUSED(rot_eval),
                                     int pt_index,
                                     const int radius,
                                     const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  float inf = gpencil_brush_influence_calc(gso, radius, co);
  float delta[3] = {0.0f};

  /* adjust the amount of displacement to apply */
  mul_v3_v3fl(delta, gso->dvec, inf);

  /* apply */
  mul_mat3_m4_v3(gso->inv_mat, delta); /* only rotation component */
  add_v3_v3(&pt->x, delta);

  /* compute lock axis */
  gpencil_sculpt_compute_lock_axis(gso, pt, save_pt);

  /* done */
  return true;
}

/* ----------------------------------------------- */
/* Pinch Brush */
/* Compute reference midpoint for the brush - this is what we'll be moving towards */
static void gpencil_brush_calc_midpoint(tGP_BrushEditData *gso)
{
  /* Convert mouse position to 3D space
   * See: gpencil_paint.c :: gpencil_stroke_convertcoords()
   */
  RegionView3D *rv3d = gso->region->regiondata;
  const float *rvec = gso->object->loc;
  const float zfac = ED_view3d_calc_zfac(rv3d, rvec);

  float mval_prj[2];

  if (ED_view3d_project_float_global(gso->region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
      V3D_PROJ_RET_OK)
  {
    float dvec[3];
    float xy_delta[2];
    sub_v2_v2v2(xy_delta, mval_prj, gso->mval);
    ED_view3d_win_to_delta(gso->region, xy_delta, zfac, dvec);
    sub_v3_v3v3(gso->dvec, rvec, dvec);
  }
  else {
    zero_v3(gso->dvec);
  }
}

/* Shrink distance between midpoint and this point... */
static bool gpencil_brush_pinch_apply(tGP_BrushEditData *gso,
                                      bGPDstroke *gps,
                                      float UNUSED(rot_eval),
                                      int pt_index,
                                      const int radius,
                                      const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float fac, inf;
  float vec[3];
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  /* Scale down standard influence value to get it more manageable...
   * - No damping = Unmanageable at > 0.5 strength
   * - Div 10     = Not enough effect
   * - Div 5      = Happy medium... (by trial and error)
   */
  inf = gpencil_brush_influence_calc(gso, radius, co) / 5.0f;

  /* 1) Make this point relative to the cursor/midpoint (dvec) */
  float fpt[3];
  mul_v3_m4v3(fpt, gso->object->object_to_world, &pt->x);
  sub_v3_v3v3(vec, fpt, gso->dvec);

  /* 2) Shrink the distance by pulling the point towards the midpoint
   *    (0.0 = at midpoint, 1 = at edge of brush region)
   *                         OR
   *    Increase the distance (if inverting the brush action!)
   */
  if (gpencil_brush_invert_check(gso)) {
    /* Inflate (inverse) */
    fac = 1.0f + (inf * inf); /* squared to temper the effect... */
  }
  else {
    /* Shrink (default) */
    fac = 1.0f - (inf * inf); /* squared to temper the effect... */
  }
  mul_v3_fl(vec, fac);

  /* 3) Translate back to original space, with the shrinkage applied */
  add_v3_v3v3(fpt, gso->dvec, vec);
  mul_v3_m4v3(&pt->x, gso->object->world_to_object, fpt);

  /* compute lock axis */
  gpencil_sculpt_compute_lock_axis(gso, pt, save_pt);

  /* done */
  return true;
}

/* ----------------------------------------------- */
/* Twist Brush - Rotate Around midpoint */
/* Take the screen-space coordinates of the point, rotate this around the brush midpoint,
 * convert the rotated point and convert it into "data" space.
 */

static bool gpencil_brush_twist_apply(tGP_BrushEditData *gso,
                                      bGPDstroke *gps,
                                      float UNUSED(rot_eval),
                                      int pt_index,
                                      const int radius,
                                      const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float angle, inf;
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  /* Angle to rotate by */
  inf = gpencil_brush_influence_calc(gso, radius, co);
  angle = DEG2RADF(1.0f) * inf;

  if (gpencil_brush_invert_check(gso)) {
    /* invert angle that we rotate by */
    angle *= -1;
  }

  /* Rotate in 2D or 3D space? */
  if (gps->flag & GP_STROKE_3DSPACE) {
    /* Perform rotation in 3D space... */
    RegionView3D *rv3d = gso->region->regiondata;
    float rmat[3][3];
    float axis[3];
    float vec[3];

    /* Compute rotation matrix - rotate around view vector by angle */
    negate_v3_v3(axis, rv3d->persinv[2]);
    normalize_v3(axis);

    axis_angle_normalized_to_mat3(rmat, axis, angle);

    /* Rotate point */
    float fpt[3];
    mul_v3_m4v3(fpt, gso->object->object_to_world, &pt->x);
    sub_v3_v3v3(vec, fpt, gso->dvec); /* make relative to center
                                       * (center is stored in dvec) */
    mul_m3_v3(rmat, vec);
    add_v3_v3v3(fpt, vec, gso->dvec); /* restore */
    mul_v3_m4v3(&pt->x, gso->object->world_to_object, fpt);

    /* compute lock axis */
    gpencil_sculpt_compute_lock_axis(gso, pt, save_pt);
  }
  else {
    const float axis[3] = {0.0f, 0.0f, 1.0f};
    float vec[3] = {0.0f};
    float rmat[3][3];

    /* Express position of point relative to cursor, ready to rotate */
    /* XXX: There is still some offset here, but it's close to working as expected. */
    vec[0] = (float)(co[0] - gso->mval[0]);
    vec[1] = (float)(co[1] - gso->mval[1]);

    /* rotate point */
    axis_angle_normalized_to_mat3(rmat, axis, angle);
    mul_m3_v3(rmat, vec);

    /* Convert back to screen-coordinates */
    vec[0] += (float)gso->mval[0];
    vec[1] += (float)gso->mval[1];

    /* Map from screen-coordinates to final coordinate space */
    if (gps->flag & GP_STROKE_2DSPACE) {
      View2D *v2d = gso->gsc.v2d;
      UI_view2d_region_to_view(v2d, vec[0], vec[1], &pt->x, &pt->y);
    }
    else {
      /* XXX */
      copy_v2_v2(&pt->x, vec);
    }
  }

  /* done */
  return true;
}

/* ----------------------------------------------- */
/* Randomize Brush */
/* Apply some random jitter to the point */
static bool gpencil_brush_randomize_apply(tGP_BrushEditData *gso,
                                          bGPDstroke *gps,
                                          float UNUSED(rot_eval),
                                          int pt_index,
                                          const int radius,
                                          const int co[2])
{
  bGPDspoint *pt = gps->points + pt_index;
  float save_pt[3];
  copy_v3_v3(save_pt, &pt->x);

  /* Amount of jitter to apply depends on the distance of the point to the cursor,
   * as well as the strength of the brush
   */
  const float inf = gpencil_brush_influence_calc(gso, radius, co) / 2.0f;
  const float fac = BLI_rng_get_float(gso->rng) * inf;

  /* apply random to position */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_POSITION) {
    /* Jitter is applied perpendicular to the mouse movement vector
     * - We compute all effects in screen-space (since it's easier)
     *   and then project these to get the points/distances in
     *   view-space as needed.
     */
    float mvec[2], svec[2];

    /* mouse movement in ints -> floats */
    mvec[0] = (float)(gso->mval[0] - gso->mval_prev[0]);
    mvec[1] = (float)(gso->mval[1] - gso->mval_prev[1]);

    /* rotate mvec by 90 degrees... */
    svec[0] = -mvec[1];
    svec[1] = mvec[0];

    /* scale the displacement by the random displacement, and apply */
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      mul_v2_fl(svec, -fac);
    }
    else {
      mul_v2_fl(svec, fac);
    }

    /* Convert to data-space. */
    if (gps->flag & GP_STROKE_3DSPACE) {
      /* 3D: Project to 3D space */
      bool flip;
      RegionView3D *rv3d = gso->region->regiondata;
      const float zfac = ED_view3d_calc_zfac_ex(rv3d, &pt->x, &flip);
      if (flip == false) {
        float dvec[3];
        ED_view3d_win_to_delta(gso->gsc.region, svec, zfac, dvec);
        add_v3_v3(&pt->x, dvec);
        /* compute lock axis */
        gpencil_sculpt_compute_lock_axis(gso, pt, save_pt);
      }
    }
  }
  /* apply random to strength */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_STRENGTH) {
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      pt->strength += fac;
    }
    else {
      pt->strength -= fac;
    }
    CLAMP_MIN(pt->strength, 0.0f);
    CLAMP_MAX(pt->strength, 1.0f);
  }
  /* apply random to thickness (use pressure) */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_THICKNESS) {
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      pt->pressure += fac;
    }
    else {
      pt->pressure -= fac;
    }
    /* only limit lower value */
    CLAMP_MIN(pt->pressure, 0.0f);
  }
  /* apply random to UV (use pressure) */
  if (gso->brush->gpencil_settings->sculpt_mode_flag & GP_SCULPT_FLAGMODE_APPLY_UV) {
    if (BLI_rng_get_float(gso->rng) > 0.5f) {
      pt->uv_rot += fac;
    }
    else {
      pt->uv_rot -= fac;
    }
    CLAMP(pt->uv_rot, -M_PI_2, M_PI_2);
  }

  /* done */
  return true;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_sculpt_paint.c */
/* Get angle of the segment relative to the original segment before any transformation
 * For strokes with one point only this is impossible to calculate because there isn't a
 * valid reference point.
 */
static float gpencil_sculpt_rotation_eval_get(tGP_BrushEditData *gso,
                                              bGPDstroke *gps_eval,
                                              bGPDspoint *pt_eval,
                                              int idx_eval)
{
  /* If multiframe or no modifiers, return 0. */
  if (GPENCIL_MULTIEDIT_SESSIONS_ON(gso->gpd) || (!gso->is_transformed)) {
    return 0.0f;
  }

  const GP_SpaceConversion *gsc = &gso->gsc;
  bGPDstroke *gps_orig = (gps_eval->runtime.gps_orig) ? gps_eval->runtime.gps_orig : gps_eval;
  bGPDspoint *pt_orig = (pt_eval->runtime.pt_orig) ? &gps_orig->points[pt_eval->runtime.idx_orig] :
                                                     pt_eval;
  bGPDspoint *pt_prev_eval = NULL;
  bGPDspoint *pt_orig_prev = NULL;
  if (idx_eval != 0) {
    pt_prev_eval = &gps_eval->points[idx_eval - 1];
  }
  else {
    if (gps_eval->totpoints > 1) {
      pt_prev_eval = &gps_eval->points[idx_eval + 1];
    }
    else {
      return 0.0f;
    }
  }

  if (pt_eval->runtime.pt_orig == NULL) {
    pt_orig_prev = pt_prev_eval;
  }
  else {
    if (pt_eval->runtime.idx_orig != 0) {
      pt_orig_prev = &gps_orig->points[pt_eval->runtime.idx_orig - 1];
    }
    else {
      if (gps_orig->totpoints > 1) {
        pt_orig_prev = &gps_orig->points[pt_eval->runtime.idx_orig + 1];
      }
      else {
        return 0.0f;
      }
    }
  }

  /* create 2D vectors of the stroke segments */
  float v_orig_a[2], v_orig_b[2], v_eval_a[2], v_eval_b[2];

  gpencil_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_orig->x, v_orig_a);
  gpencil_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_orig_prev->x, v_orig_b);
  sub_v2_v2(v_orig_a, v_orig_b);

  gpencil_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_eval->x, v_eval_a);
  gpencil_point_3d_to_xy(gsc, GP_STROKE_3DSPACE, &pt_prev_eval->x, v_eval_b);
  sub_v2_v2(v_eval_a, v_eval_b);

  return angle_v2v2(v_orig_a, v_eval_a);
}

/* Apply brush operation to points in this stroke */
static bool gpencil_sculpt_brush_do_stroke(tGP_BrushEditData *gso,
                                           bGPDstroke *gps,
                                           const float diff_mat[4][4],
                                           GP_BrushApplyCb apply)
{
  GP_SpaceConversion *gsc = &gso->gsc;
  rcti *rect = &gso->brush_rect;
  Brush *brush = gso->brush;
  const int radius = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;
  const bool is_masking = GPENCIL_ANY_SCULPT_MASK(gso->mask);

  bGPDstroke *gps_active = (gps->runtime.gps_orig) ? gps->runtime.gps_orig : gps;
  bGPDspoint *pt_active = NULL;

  bGPDspoint *pt1, *pt2;
  bGPDspoint *pt = NULL;
  int pc1[2] = {0};
  int pc2[2] = {0};
  int i;
  int index;
  bool include_last = false;
  bool changed = false;
  float rot_eval = 0.0f;

  if (gps->totpoints == 1) {
    bGPDspoint pt_temp;
    pt = &gps->points[0];
    if ((is_masking && (pt->flag & GP_SPOINT_SELECT) != 0) || (!is_masking)) {
      gpencil_point_to_world_space(gps->points, diff_mat, &pt_temp);
      gpencil_point_to_xy(gsc, gps, &pt_temp, &pc1[0], &pc1[1]);

      pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
      /* Do bound-box check first. */
      if (!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
        /* only check if point is inside */
        int mval_i[2];
        round_v2i_v2fl(mval_i, gso->mval);
        if (len_v2v2_int(mval_i, pc1) <= radius) {
          /* apply operation to this point */
          if (pt_active != NULL) {
            rot_eval = gpencil_sculpt_rotation_eval_get(gso, gps, pt, 0);
            changed = apply(gso, gps_active, rot_eval, 0, radius, pc1);
          }
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

      /* Skip if neither one is selected
       * (and we are only allowed to edit/consider selected points) */
      if (GPENCIL_ANY_SCULPT_MASK(gso->mask)) {
        if (!(pt1->flag & GP_SPOINT_SELECT) && !(pt2->flag & GP_SPOINT_SELECT)) {
          include_last = false;
          continue;
        }
      }
      bGPDspoint npt;
      gpencil_point_to_world_space(pt1, diff_mat, &npt);
      gpencil_point_to_xy(gsc, gps, &npt, &pc1[0], &pc1[1]);

      gpencil_point_to_world_space(pt2, diff_mat, &npt);
      gpencil_point_to_xy(gsc, gps, &npt, &pc2[0], &pc2[1]);

      /* Check that point segment of the bound-box of the selection stroke. */
      if ((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1]) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
          (!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1]) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1])))
      {
        /* Check if point segment of stroke had anything to do with
         * brush region  (either within stroke painted, or on its lines)
         * - this assumes that line-width is irrelevant.
         */
        if (gpencil_stroke_inside_circle(gso->mval, radius, pc1[0], pc1[1], pc2[0], pc2[1])) {
          /* Apply operation to these points */
          bool ok = false;

          /* To each point individually... */
          pt = &gps->points[i];
          if ((i != gps->totpoints - 2) && (pt->runtime.pt_orig == NULL)) {
            continue;
          }
          pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
          /* If masked and the point is not selected, skip it. */
          if (GPENCIL_ANY_SCULPT_MASK(gso->mask) && ((pt_active->flag & GP_SPOINT_SELECT) == 0)) {
            continue;
          }
          index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
          if ((pt_active != NULL) && (index < gps_active->totpoints)) {
            rot_eval = gpencil_sculpt_rotation_eval_get(gso, gps, pt, i);
            ok = apply(gso, gps_active, rot_eval, index, radius, pc1);
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
            pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
            index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i + 1;
            if ((pt_active != NULL) && (index < gps_active->totpoints)) {
              rot_eval = gpencil_sculpt_rotation_eval_get(gso, gps, pt, i + 1);
              ok |= apply(gso, gps_active, rot_eval, index, radius, pc2);
              include_last = false;
            }
          }
          else {
            include_last = true;
          }

          changed |= ok;
        }
        else if (include_last) {
          /* This case is for cases where for whatever reason the second vert (1st here)
           * doesn't get included because the whole edge isn't in bounds,
           * but it would've qualified since it did with the previous step
           * (but wasn't added then, to avoid double-ups).
           */
          pt = &gps->points[i];
          pt_active = (pt->runtime.pt_orig) ? pt->runtime.pt_orig : pt;
          index = (pt->runtime.pt_orig) ? pt->runtime.idx_orig : i;
          if ((pt_active != NULL) && (index < gps_active->totpoints)) {
            rot_eval = gpencil_sculpt_rotation_eval_get(gso, gps, pt, i);
            changed |= apply(gso, gps_active, rot_eval, index, radius, pc1);
            include_last = false;
          }
        }
      }
    }
  }

  return changed;
}
/* END VERBATIM */


/* ---- Project Grease adaptation of the operator loop ---------------------------------------- */

/* gpencil_recalc_geometry_tag() / gpencil_update_geometry() without DEG / notifiers. */
static void pgt_sculpt_update_geometry(bGPdata *gpd)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->flag & GP_STROKE_TAG) {
          BKE_gpencil_stroke_geometry_update(gpd, gps);
          gps->flag &= ~GP_STROKE_TAG;
        }
      }
    }
  }
}

/* gpencil_sculpt_brush_do_frame(): ED_gpencil_stroke_can_use() is always true on the canvas,
 * material editability uses bGPdata::mat[], no auto-masking. Geometry of changed strokes is
 * tagged and recalculated at the end of the gesture (fill strokes of the active frame at once). */
static bool pgt_sculpt_do_frame(tGP_BrushEditData *gso, bGPDlayer *gpl, bGPDframe *gpf)
{
  bool changed = false;
  bool redo_geom = false;
  bGPdata *gpd = gso->gpd;
  const char tool = gso->brush->gpencil_sculpt_tool;
  GP_SpaceConversion *gsc = &gso->gsc;
  Brush *brush = gso->brush;
  const int radius = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;
  float diff_mat[4][4], bound_mat[4][4];
  unit_m4(diff_mat);
  unit_m4(bound_mat);

  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    if (gps->points == NULL || gps->totpoints <= 0) {
      continue;
    }
    {
      const MaterialGPencilStyle *gp_style = pg_tool_material_style(gpd, gps->mat_nr + 1);
      if ((gp_style->flag & GP_MATERIAL_HIDE) ||
          (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (gp_style->flag & GP_MATERIAL_LOCKED)))
      {
        continue;
      }
    }
    if ((gps->totpoints > 1) &&
        !ED_gpencil_stroke_check_collision(gsc, gps, gso->mval, radius, bound_mat))
    {
      continue;
    }
    redo_geom = false;
    switch (tool) {
      case GPSCULPT_TOOL_SMOOTH:
        changed |= gpencil_sculpt_brush_do_stroke(gso, gps, diff_mat, gpencil_brush_smooth_apply);
        redo_geom |= changed;
        break;
      case GPSCULPT_TOOL_THICKNESS:
        changed |= gpencil_sculpt_brush_do_stroke(gso, gps, diff_mat, gpencil_brush_thickness_apply);
        break;
      case GPSCULPT_TOOL_STRENGTH:
        changed |= gpencil_sculpt_brush_do_stroke(gso, gps, diff_mat, gpencil_brush_strength_apply);
        break;
      case GPSCULPT_TOOL_GRAB:
        if (gso->first) {
          gpencil_brush_grab_stroke_init(gso, gps);
          changed |= gpencil_sculpt_brush_do_stroke(gso, gps, bound_mat,
                                                    gpencil_brush_grab_store_points);
        }
        else {
          gpencil_brush_grab_apply_cached(gso, gps, diff_mat);
          changed |= true;
        }
        redo_geom |= changed;
        break;
      case GPSCULPT_TOOL_PUSH:
        changed |= gpencil_sculpt_brush_do_stroke(gso, gps, diff_mat, gpencil_brush_push_apply);
        redo_geom |= changed;
        break;
      case GPSCULPT_TOOL_PINCH:
        changed |= gpencil_sculpt_brush_do_stroke(gso, gps, diff_mat, gpencil_brush_pinch_apply);
        redo_geom |= changed;
        break;
      case GPSCULPT_TOOL_TWIST:
        changed |= gpencil_sculpt_brush_do_stroke(gso, gps, diff_mat, gpencil_brush_twist_apply);
        redo_geom |= changed;
        break;
      case GPSCULPT_TOOL_RANDOMIZE:
        changed |= gpencil_sculpt_brush_do_stroke(gso, gps, diff_mat, gpencil_brush_randomize_apply);
        redo_geom |= changed;
        break;
      default:
        break;
    }
    if (redo_geom) {
      const MaterialGPencilStyle *gp_style = pg_tool_material_style(gpd, gps->mat_nr + 1);
      if ((gpl->actframe == gpf) && (gp_style->flag & GP_MATERIAL_FILL_SHOW)) {
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
      else {
        gps->flag |= GP_STROKE_TAG;
      }
    }
  }
  return changed;
}

/* gpencil_sculpt_brush_apply_standard(). */
static bool pgt_sculpt_apply_standard(tGP_BrushEditData *gso)
{
  bool changed = false;
  switch (gso->brush->gpencil_sculpt_tool) {
    case GPSCULPT_TOOL_GRAB:
    case GPSCULPT_TOOL_PUSH:
      gso->rot_eval = 0.0f;
      gpencil_brush_grab_calc_dvec(gso);
      break;
    case GPSCULPT_TOOL_PINCH:
    case GPSCULPT_TOOL_TWIST:
      gpencil_brush_calc_midpoint(gso);
      break;
    case GPSCULPT_TOOL_RANDOMIZE:
      gso->rot_eval = 0.0f;
      gpencil_brush_grab_calc_dvec(gso);
      break;
    default:
      break;
  }
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gso->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }
    if (gso->is_multiframe) {
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        if ((gpf == gpl->actframe) || (gpf->flag & GP_FRAME_SELECT)) {
          gso->mf_falloff = 1.0f; /* no multiframe falloff curve */
          changed |= pgt_sculpt_do_frame(gso, gpl, gpf);
        }
      }
    }
    else {
      gso->mf_falloff = 1.0f;
      changed |= pgt_sculpt_do_frame(gso, gpl, gpl->actframe);
    }
  }
  return changed;
}

struct PGSculptSession {
  tGP_BrushEditData gso;
  Brush brush;
  BrushGpencilSettings settings;
  PGToolView view;
};

/* BKE_gpencil_brush_preset_set() for the sculpt presets: size, pressure flag, affect flags. */
static void pgt_sculpt_preset(Brush *brush, int tool)
{
  brush->gpencil_sculpt_tool = (char)tool;
  brush->size = (tool == GPSCULPT_TOOL_TWIST || tool == GPSCULPT_TOOL_PINCH) ? 50 : 25;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_PRESSURE;
  if (tool == GPSCULPT_TOOL_GRAB) {
    brush->gpencil_settings->flag &= ~GP_BRUSH_USE_PRESSURE;
  }
  brush->gpencil_settings->flag |= GP_BRUSH_USE_STRENGTH_PRESSURE;
  /* Blender's default affect flags: position only (Smooth does not touch thickness, strength or
   * UV; GP_BRUSH_PRESET_SMOOTH_STROKE's sculpt_flag = APPLY_THICKNESS is the panel toggle, not
   * sculpt_mode_flag). */
  brush->gpencil_settings->sculpt_mode_flag = GP_SCULPT_FLAGMODE_APPLY_POSITION;
  brush->curve_preset = BRUSH_CURVE_SMOOTH;
  brush->alpha = 1.0f;
}

PGSculptSession *pg_sculpt_session_begin(bGPdata *gpd, const PGToolBrushParams *params)
{
  if (gpd == NULL || params == NULL || params->brush < GPSCULPT_TOOL_SMOOTH ||
      params->brush > GPSCULPT_TOOL_RANDOMIZE || params->brush == GPSCULPT_TOOL_CLONE ||
      !(params->radius > 0.0f))
  {
    return NULL;
  }
  PGSculptSession *s = MEM_callocN(sizeof(PGSculptSession), "PGSculptSession");
  tGP_BrushEditData *gso = &s->gso;
  s->brush.gpencil_settings = &s->settings;
  pgt_sculpt_preset(&s->brush, params->brush);
  pg_tool_view_init(&s->view, &gso->gsc, params->px_per_unit);
  /* The UI sets size (radius in pixels) and strength like the tool settings panel. */
  s->brush.size = max_ii(1, (int)lroundf(params->radius * s->view.px_per_unit));
  s->brush.alpha = clamp_f(params->strength, 0.0f, 1.0f);
  if (params->invert) {
    s->settings.sculpt_flag |= GP_SCULPT_FLAG_INVERT;
  }
  s->view.ts.gp_sculpt.lock_axis = GP_LOCKAXIS_VIEW;

  gso->scene = &s->view.scene;
  gso->object = &s->view.ob;
  gso->region = &s->view.region;
  gso->gpd = gpd;
  gso->gsc.gpd = gpd;
  gso->brush = &s->brush;
  gso->first = true;
  gso->mval_prev[0] = -1.0f;
  gso->is_multiframe = GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  gso->is_transformed = false;
  gso->mf_falloff = 1.0f;
  gso->stroke_customdata = BLI_ghash_ptr_new("GP Sculpt Brush - Stroke-Specific Data");
  gso->rng = BLI_rng_new(params->seed);
  unit_m4(gso->inv_mat);
  pg_tool_link_runtime(gpd, true);
  return s;
}

/* gpencil_sculpt_brush_apply(): one input event. */
int pg_sculpt_session_sample(PGSculptSession *s, float x, float y, float pressure)
{
  if (s == NULL || !isfinite(x) || !isfinite(y)) {
    return 0;
  }
  tGP_BrushEditData *gso = &s->gso;
  Brush *brush = gso->brush;
  const float scale = s->view.px_per_unit;
  int mouse[2];
  gso->mval[0] = mouse[0] = (int)(x * scale);
  gso->mval[1] = mouse[1] = (int)(y * scale);
  /* "If the mouse/pen has not moved, no reason to continue." */
  if ((gso->mval[0] == gso->mval_prev[0]) && (gso->mval[1] == gso->mval_prev[1])) {
    return 0;
  }
  gso->pressure = (pressure >= 0.99f) ? 1.0f : clamp_f(pressure, 0.0f, 1.0f);
  const int radius = (brush->flag & GP_BRUSH_USE_PRESSURE) ? gso->brush->size * gso->pressure :
                                                             gso->brush->size;
  if (gso->mval_prev[0] == -1.0f) {
    gso->mval_prev[0] = gso->mval[0];
    gso->mval_prev[1] = gso->mval[1];
    gso->pressure_prev = gso->pressure;
  }
  gso->brush_rect.xmin = mouse[0] - radius;
  gso->brush_rect.ymin = mouse[1] - radius;
  gso->brush_rect.xmax = mouse[0] + radius;
  gso->brush_rect.ymax = mouse[1] + radius;

  const bool changed = pgt_sculpt_apply_standard(gso);
  if (changed) {
    gso->gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gso->gpd);
  }
  gso->mval_prev[0] = gso->mval[0];
  gso->mval_prev[1] = gso->mval[1];
  gso->pressure_prev = gso->pressure;
  gso->first = false;
  return changed ? 1 : 0;
}

/* gpencil_sculpt_brush_exit(). */
void pg_sculpt_session_end(PGSculptSession *s)
{
  if (s == NULL) {
    return;
  }
  tGP_BrushEditData *gso = &s->gso;
  if (gso->stroke_customdata) {
    BLI_ghash_free(gso->stroke_customdata, NULL, gpencil_brush_grab_stroke_free);
  }
  if (gso->rng != NULL) {
    BLI_rng_free(gso->rng);
  }
  pg_tool_link_runtime(gso->gpd, false);
  pgt_sculpt_update_geometry(gso->gpd);
  MEM_freeN(s);
}
