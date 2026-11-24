/* Blender 3.6.23 3D viewport navigation and projection, see project_grease_view3d.h for the sources.
 * ADAPTED: the RegionView3D / View3D / CameraParams fields these functions read are gathered in
 * PGView3D; Blender's maths (BLI_math) is called unchanged. */
#include "project_grease_view3d.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include "BLI_math_base.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

/* DNA_camera_defaults.h / BKE_camera.h */
#define DEFAULT_SENSOR_WIDTH 36.0f
#define CAMERA_PARAM_ZOOM_INIT_PERSP 2.0f
/* U.view_rotate_sensitivity_turntable default (userdef_default.c: DEG2RADF(0.4f)) */
#define VIEW_ROTATE_SENSITIVITY_TURNTABLE (0.4f * (float)M_PI / 180.0f)

#define RV3D_VIEW_IS_AXIS(view) (((view) >= PG_RV3D_VIEW_FRONT) && ((view) <= PG_RV3D_VIEW_BOTTOM))

/* view3d_utils.c view3d_quat_axis[][RV3D_VIEW_AXIS_ROLL_0] (Front, Back, Left, Right, Top, Bottom). */
static const float view3d_quat_axis[6][4] = {
    {M_SQRT1_2, -M_SQRT1_2, 0.0f, 0.0f},
    {0.0f, 0.0f, -M_SQRT1_2, -M_SQRT1_2},
    {0.5f, -0.5f, 0.5f, 0.5f},
    {0.5f, -0.5f, -0.5f, -0.5f},
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, -1.0f, 0.0f, 0.0f},
};

void pg_view3d_init(PGView3D *v)
{
  memset(v, 0, sizeof(*v));
  v->lens = 50.0f;
  v->clip_start = 0.01f;
  v->clip_end = 1000.0f;
  v->grid = 1.0f;
  v->gridsubdiv = 10;
  v->persp = PG_RV3D_PERSP;
  v->view = PG_RV3D_VIEW_USER;
  /* Project choice: the drawing (about 1.3 x 0.7 units, on the Front X-Z plane) seen from the front,
   * above and to the right, like Blender's startup view. */
  copy_qt_qt(v->viewquat, view3d_quat_axis[0]);
  v->dist = 2.5f;
  pg_view3d_update(v, 100, 100);
  pg_view3d_orbit(v, -75.0f, -50.0f, 1.0f); /* 30 degrees about Z, 20 degrees down */
  v->view = PG_RV3D_VIEW_USER;
}

/* BKE_camera_params_from_view3d + BKE_camera_params_compute_viewplane (non-camera views, aspect 1,
 * sensor_fit AUTO) and view3d_winmatrix_set. */
static void view3d_winmatrix_set(PGView3D *v)
{
  float clip_start = v->clip_start, clip_end = v->clip_end, ortho_scale = 0.0f;
  const float zoom = CAMERA_PARAM_ZOOM_INIT_PERSP;
  const float sensor_size = DEFAULT_SENSOR_WIDTH; /* BKE_camera_sensor_size(AUTO, 36, 24) */
  const bool is_ortho = v->persp == PG_RV3D_ORTHO;
  if (is_ortho) {
    /* Halve, otherwise too extreme low Z-buffer quality. */
    clip_end *= 0.5f;
    clip_start = -clip_end;
    ortho_scale = v->dist * sensor_size / v->lens;
  }
  float pixsize = is_ortho ? ortho_scale : (sensor_size * clip_start) / v->lens;
  /* BKE_camera_sensor_fit(AUTO): horizontal when the region is at least as wide as it is high. */
  const float viewfac = (v->winx >= v->winy) ? (float)v->winx : (float)v->winy;
  pixsize /= viewfac;
  pixsize *= zoom;
  const float xmin = -0.5f * (float)v->winx * pixsize, xmax = 0.5f * (float)v->winx * pixsize;
  const float ymin = -0.5f * (float)v->winy * pixsize, ymax = 0.5f * (float)v->winy * pixsize;
  if (is_ortho) {
    orthographic_m4(v->winmat, xmin, xmax, ymin, ymax, clip_start, clip_end);
  }
  else {
    perspective_m4(v->winmat, xmin, xmax, ymin, ymax, clip_start, clip_end);
  }
  v->is_persp = !is_ortho;
}

void pg_view3d_update(PGView3D *v, int winx, int winy)
{
  v->winx = winx > 0 ? winx : 1;
  v->winy = winy > 0 ? winy : 1;
  view3d_winmatrix_set(v);
  /* view3d_viewmatrix_set (no camera, no lock, no ob_center) */
  quat_to_mat4(v->viewmat, v->viewquat);
  if (v->persp == PG_RV3D_PERSP) {
    v->viewmat[3][2] -= v->dist;
  }
  translate_m4(v->viewmat, v->ofs[0], v->ofs[1], v->ofs[2]);
  /* ED_view3d_update_viewmat */
  invert_m4_m4(v->viewinv, v->viewmat);
  mul_m4_m4m4(v->persmat, v->winmat, v->viewmat);
  invert_m4_m4(v->persinv, v->persmat);
}

void pg_view3d_orbit(PGView3D *v, float dx, float dy, float ui_scale)
{
  /* viewrotate_invoke: ED_view3d_persp_ensure (auto perspective leaves an axis view in
   * perspective) and vod->reverse. */
  if (v->persp != PG_RV3D_PERSP && RV3D_VIEW_IS_AXIS(v->view)) {
    v->persp = PG_RV3D_PERSP;
  }
  const float reverse = (v->persmat[2][1] < 0.0f) ? -1.0f : 1.0f;
  v->view = PG_RV3D_VIEW_USER; /* need to reset every time because of view snapping */

  float quat_local_x[4], quat_global_z[4];
  float m[3][3];
  float m_inv[3][3];
  const float zvec_global[3] = {0.0f, 0.0f, 1.0f};
  float xaxis[3];

  /* Radians per-pixel. */
  const float sensitivity = VIEW_ROTATE_SENSITIVITY_TURNTABLE / (ui_scale > 0.0f ? ui_scale : 1.0f);

  /* Get the 3x3 matrix and its inverse from the quaternion */
  quat_to_mat3(m, v->viewquat);
  invert_m3_m3(m_inv, m);

  /* Avoid Gimbal Lock (blend the rotated and the un-rotated horizons, see viewrotate_apply). */
  if (len_squared_v3v3(zvec_global, m_inv[2]) > 0.001f) {
    float fac;
    cross_v3_v3v3(xaxis, zvec_global, m_inv[2]);
    if (dot_v3v3(xaxis, m_inv[0]) < 0) {
      negate_v3(xaxis);
    }
    fac = angle_normalized_v3v3(zvec_global, m_inv[2]) / (float)M_PI;
    fac = fabsf(fac - 0.5f) * 2;
    fac = fac * fac;
    interp_v3_v3v3(xaxis, xaxis, m_inv[0], fac);
  }
  else {
    copy_v3_v3(xaxis, m_inv[0]);
  }

  /* Perform the up/down rotation */
  axis_angle_to_quat(quat_local_x, xaxis, sensitivity * -dy);
  mul_qt_qtqt(quat_local_x, v->viewquat, quat_local_x);

  /* Perform the orbital rotation */
  axis_angle_to_quat_single(quat_global_z, 'Z', sensitivity * reverse * dx);
  mul_qt_qtqt(v->viewquat, quat_local_x, quat_global_z);

  /* avoid precision loss over time */
  normalize_qt(v->viewquat);
  pg_view3d_update(v, v->winx, v->winy);
}

float pg_view3d_calc_zfac(const PGView3D *v, const float co[3])
{
  /* ED_view3d_calc_zfac_ex */
  float zfac = mul_project_m4_v3_zfac(v->persmat, co);
  if (zfac < 1.e-6f && zfac > -1.e-6f) {
    zfac = 1.0f;
  }
  if (zfac < 0.0f) {
    zfac = -zfac;
  }
  return zfac;
}

void pg_view3d_win_to_delta(const PGView3D *v, const float xy_delta[2], float zfac, float r_out[3])
{
  const float dx = 2.0f * xy_delta[0] * zfac / (float)v->winx;
  const float dy = 2.0f * xy_delta[1] * zfac / (float)v->winy;
  r_out[0] = (v->persinv[0][0] * dx + v->persinv[1][0] * dy);
  r_out[1] = (v->persinv[0][1] * dx + v->persinv[1][1] * dy);
  r_out[2] = (v->persinv[0][2] * dx + v->persinv[1][2] * dy);
}

void pg_view3d_pan(PGView3D *v, float dx, float dy)
{
  /* viewmove_apply: event_ofs = prev - current, zfac of the view centre at invoke. */
  const float event_ofs[2] = {-dx, -dy};
  float tvec[3], dvec[3];
  negate_v3_v3(tvec, v->ofs);
  const float zfac = pg_view3d_calc_zfac(v, tvec);
  pg_view3d_win_to_delta(v, event_ofs, zfac, dvec);
  sub_v3_v3(v->ofs, dvec);
  pg_view3d_update(v, v->winx, v->winy);
}

void pg_view3d_zoom(PGView3D *v, float dfac, const float mval[2])
{
  /* ED_view3d_dist_range_get */
  const float dist_range[2] = {v->grid * 0.001f, v->clip_end * 10.0f};
  /* viewzoom_apply_3d */
  if (dfac != 1.0f && dfac > 0.0f) {
    const float zfac_min = dist_range[0] / v->dist;
    const float zfac_max = dist_range[1] / v->dist;
    CLAMP(dfac, zfac_min, zfac_max);
    /* view_zoom_to_window_xy_3d */
    const float dist_new = v->dist * dfac;
    if (mval != NULL) {
      float dvec[3], tvec[3], tpos[3], xy_delta[2];
      negate_v3_v3(tpos, v->ofs);
      xy_delta[0] = mval[0] - (float)v->winx / 2.0f;
      xy_delta[1] = mval[1] - (float)v->winy / 2.0f;
      const float zfac = pg_view3d_calc_zfac(v, tpos);
      pg_view3d_win_to_delta(v, xy_delta, zfac, dvec);
      add_v3_v3v3(tvec, tpos, dvec);
      negate_v3(tvec);
      copy_v3_v3(v->ofs, tvec);
      v->dist = dist_new;
      madd_v3_v3v3fl(v->ofs, tvec, dvec, dfac);
    }
    else {
      v->dist = dist_new;
    }
  }
  CLAMP(v->dist, dist_range[0], dist_range[1]);
  pg_view3d_update(v, v->winx, v->winy);
}

void pg_view3d_set_axis_view(PGView3D *v, int view)
{
  if (!RV3D_VIEW_IS_AXIS(view)) {
    return;
  }
  /* ED_view3d_quat_from_axis_view (RV3D_VIEW_AXIS_ROLL_0) and axis_set_view with USER_AUTOPERSP;
   * ofs and dist are kept, the smooth-view animation is not ported. */
  normalize_qt_qt(v->viewquat, view3d_quat_axis[view - PG_RV3D_VIEW_FRONT]);
  v->view = view;
  v->persp = PG_RV3D_ORTHO;
  pg_view3d_update(v, v->winx, v->winy);
}

void pg_view3d_toggle_persp(PGView3D *v)
{
  v->persp = (v->persp != PG_RV3D_ORTHO) ? PG_RV3D_ORTHO : PG_RV3D_PERSP;
  pg_view3d_update(v, v->winx, v->winy);
}

int pg_view3d_project(const PGView3D *v, const float co[3], float r_mval[2])
{
  float vec4[4];
  copy_v3_v3(vec4, co);
  vec4[3] = 1.0f;
  mul_m4_v4(v->persmat, vec4);
  if (vec4[3] <= FLT_EPSILON) {
    return 0;
  }
  r_mval[0] = ((float)v->winx / 2.0f) * (1.0f + vec4[0] / vec4[3]);
  r_mval[1] = ((float)v->winy / 2.0f) * (1.0f + vec4[1] / vec4[3]);
  return 1;
}

/* ED_view3d_global_to_vector */
static void view3d_global_to_vector(const PGView3D *v, const float coord[3], float vec[3])
{
  if (v->is_persp) {
    float p1[4], p2[4];
    copy_v3_v3(p1, coord);
    p1[3] = 1.0f;
    copy_v3_v3(p2, p1);
    p2[3] = 1.0f;
    mul_m4_v4(v->viewmat, p2);
    mul_v3_fl(p2, 2.0f);
    mul_m4_v4(v->viewinv, p2);
    sub_v3_v3v3(vec, p1, p2);
  }
  else {
    copy_v3_v3(vec, v->viewinv[2]);
  }
  normalize_v3(vec);
}

void pg_gpencil_drawing_reference_get(int align_flag, const float ob_loc[3], const float cursor[3], float r_vec[3])
{
  if (align_flag & PG_PROJECT_VIEWSPACE) {
    /* GP object: the cursor, or the object location (the layer offset is zero here). */
    copy_v3_v3(r_vec, (align_flag & PG_PROJECT_CURSOR) ? cursor : ob_loc);
  }
  else {
    copy_v3_v3(r_vec, cursor);
  }
}

void pg_gpencil_project_point(const PGView3D *v,
                              const float mval[2],
                              const float ref[3],
                              int lock_axis,
                              const float origin[3],
                              float r_out[3])
{
  /* gpencil_stroke_convertcoords: the view plane through the reference point. */
  float mval_prj[2];
  const float zfac = pg_view3d_calc_zfac(v, ref);
  if (pg_view3d_project(v, ref, mval_prj)) {
    float dvec[3], xy_delta[2];
    sub_v2_v2v2(xy_delta, mval_prj, mval);
    pg_view3d_win_to_delta(v, xy_delta, zfac, dvec);
    sub_v3_v3v3(r_out, ref, dvec);
  }
  else {
    zero_v3(r_out);
  }

  /* ED_gpencil_project_stroke_to_plane (axis = lock_axis - 1) */
  const int axis = lock_axis - 1;
  float plane_normal[3] = {0.0f, 0.0f, 0.0f};
  if (axis < 0) {
    view3d_global_to_vector(v, origin, plane_normal);
  }
  else if (axis < 3) {
    plane_normal[axis] = 1.0f;
  }
  else {
    plane_normal[2] = 1.0f; /* the cursor rotation is the identity here */
  }
  float vn[3], ray[3], rpoint[3];
  view3d_global_to_vector(v, r_out, vn);
  mul_v3_fl(vn, -50.0f);
  add_v3_v3v3(ray, r_out, vn);
  if (isect_line_plane_v3(rpoint, r_out, ray, origin, plane_normal)) {
    copy_v3_v3(r_out, rpoint);
  }
}
