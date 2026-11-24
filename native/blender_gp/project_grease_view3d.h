/* 3D viewport maths: a port of Blender 3.6.23's RegionView3D navigation and projection.
 *
 *   editors/space_view3d/view3d_view.c      view3d_viewmatrix_set, view3d_winmatrix_set
 *   blenkernel/intern/camera.c              BKE_camera_params_from_view3d / compute_viewplane
 *   editors/space_view3d/view3d_navigate_rotate.c  viewrotate_apply (turntable)
 *   editors/space_view3d/view3d_navigate.cc viewmove_apply, axis_set_view, ED_view3d_persp_ensure
 *   editors/space_view3d/view3d_navigate_zoom.c    view_zoom_to_window_xy_3d
 *   editors/space_view3d/view3d_edit.c      viewpersportho_exec
 *   editors/space_view3d/view3d_project.c   ED_view3d_win_to_delta / win_to_vector / global_to_vector
 *   editors/gpencil_legacy/gpencil_utils.c  ED_gpencil_drawing_reference_get,
 *                                           ED_gpencil_project_stroke_to_plane
 *   editors/gpencil_legacy/gpencil_paint.c  gpencil_stroke_convertcoords
 *
 * Window coordinates are Blender's: pixels from the bottom-left of the region, y up.
 * World units are Blender units. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* RegionView3D.persp */
enum { PG_RV3D_ORTHO = 0, PG_RV3D_PERSP = 1, PG_RV3D_CAMOB = 2 };
/* RegionView3D.view (RV3D_VIEW_*) */
enum {
  PG_RV3D_VIEW_USER = 0,
  PG_RV3D_VIEW_FRONT = 1,
  PG_RV3D_VIEW_BACK = 2,
  PG_RV3D_VIEW_LEFT = 3,
  PG_RV3D_VIEW_RIGHT = 4,
  PG_RV3D_VIEW_TOP = 5,
  PG_RV3D_VIEW_BOTTOM = 6,
};
/* ToolSettings.gp_sculpt.lock_axis (GP_LOCKAXIS_*): the drawing plane. */
enum {
  PG_LOCKAXIS_VIEW = 0,   /* View */
  PG_LOCKAXIS_X = 1,      /* Side (Y-Z) */
  PG_LOCKAXIS_Y = 2,      /* Front (X-Z) */
  PG_LOCKAXIS_Z = 3,      /* Top (X-Y) */
  PG_LOCKAXIS_CURSOR = 4, /* Cursor */
};
/* ToolSettings.gpencil_v3d_align (GP_PROJECT_*): stroke placement. */
enum {
  PG_PROJECT_VIEWSPACE = 1 << 0,
  PG_PROJECT_DEPTH_VIEW = 1 << 2,
  PG_PROJECT_CURSOR = 1 << 5,
};

typedef struct PGView3D {
  /* RegionView3D */
  float ofs[3], viewquat[4], dist;
  int persp, view, lpersp;
  /* View3D */
  float lens, clip_start, clip_end, grid;
  int gridsubdiv;
  /* Derived by pg_view3d_update. */
  int winx, winy, is_persp;
  float viewmat[4][4], viewinv[4][4], winmat[4][4], persmat[4][4], persinv[4][4];
} PGView3D;

/* View3D / RegionView3D defaults (DNA_view3d_defaults.h: lens 50, clip 0.01 .. 1000, grid 1,
 * subdivisions 10), a perspective user view. */
void pg_view3d_init(PGView3D *v);
/* view3d_winmatrix_set + view3d_viewmatrix_set for a winx x winy region. */
void pg_view3d_update(PGView3D *v, int winx, int winy);

/* viewrotate_apply, turntable (U.view_rotate_sensitivity_turntable = 0.4 degrees per pixel,
 * divided by the UI scale). dx, dy: window-coordinate motion of the pointer (y up). */
void pg_view3d_orbit(PGView3D *v, float dx, float dy, float ui_scale);
/* viewmove_apply: the view follows a pointer moved by dx, dy (window coordinates). */
void pg_view3d_pan(PGView3D *v, float dx, float dy);
/* view_zoom_to_window_xy_3d: dist *= dfac (clamped to ED_view3d_dist_range_get), towards the
 * window point mval when non-null. */
void pg_view3d_zoom(PGView3D *v, float dfac, const float mval[2]);
/* axis_set_view (with U.uiflag USER_AUTOPERSP): Front / Back / Left / Right / Top / Bottom. */
void pg_view3d_set_axis_view(PGView3D *v, int view);
/* viewpersportho_exec */
void pg_view3d_toggle_persp(PGView3D *v);

/* ED_view3d_project_float_global: world -> window. Returns 0 behind the view (w <= 0). */
int pg_view3d_project(const PGView3D *v, const float co[3], float r_mval[2]);
float pg_view3d_calc_zfac(const PGView3D *v, const float co[3]);
void pg_view3d_win_to_delta(const PGView3D *v, const float xy_delta[2], float zfac, float r_out[3]);

/* ED_gpencil_drawing_reference_get: the point strokes are placed through. */
void pg_gpencil_drawing_reference_get(int align_flag, const float ob_loc[3], const float cursor[3], float r_vec[3]);
/* gpencil_stroke_convertcoords (no depth): window point -> 3D on the view plane through ref, then
 * ED_gpencil_project_stroke_to_plane on the lock_axis plane through origin (axis planes in world
 * space, the GP object unrotated; Cursor uses the cursor rotation, here unrotated). */
void pg_gpencil_project_point(const PGView3D *v,
                              const float mval[2],
                              const float ref[3],
                              int lock_axis,
                              const float origin[3],
                              float r_out[3]);

#ifdef __cplusplus
}
#endif
