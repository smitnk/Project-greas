/* Host test of project_grease_view3d.c against Blender 3.6.23's view maths: window / view matrices
 * (view3d_winmatrix_set, view3d_viewmatrix_set), turntable orbit, pan, zoom, axis views, the
 * persp toggle and the Grease Pencil drawing planes (ED_gpencil_project_stroke_to_plane). */
#include <cmath>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "project_grease_view3d.h"
}

static int g_fail = 0;
#define CHECK(c)                                                         \
  do {                                                                   \
    if (!(c)) {                                                          \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);            \
      g_fail++;                                                          \
    }                                                                    \
  } while (0)
static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps * (1.0f + std::fabs(b)); }
/* Same rotation (q and -q). */
static bool same_rot(const float a[4], const float b[4])
{
  return std::fabs(std::fabs(dot_qtqt(a, b)) - 1.0f) < 1e-4f;
}

/* view3d_quat_axis, RV3D_VIEW_AXIS_ROLL_0 (view3d_utils.c) */
static const float Q_FRONT[4] = {(float)M_SQRT1_2, -(float)M_SQRT1_2, 0.0f, 0.0f};
static const float Q_LEFT[4] = {0.5f, -0.5f, 0.5f, 0.5f};
static const float Q_RIGHT[4] = {0.5f, -0.5f, -0.5f, -0.5f};
static const float Q_TOP[4] = {1.0f, 0.0f, 0.0f, 0.0f};
static const float Q_BOTTOM[4] = {0.0f, -1.0f, 0.0f, 0.0f};

static void test_winmat()
{
  PGView3D v;
  pg_view3d_init(&v);
  v.persp = PG_RV3D_PERSP;
  pg_view3d_update(&v, 1000, 500);
  /* BKE_camera_params_compute_viewplane: pixsize = 36 * 0.01 / 50 / 1000 * 2 = 1.44e-5,
   * viewplane x +-0.0072, y +-0.0036; perspective_m4 [0][0] = 2n / dx. */
  CHECK(near(v.winmat[0][0], 0.02f / 0.0144f));
  CHECK(near(v.winmat[1][1], 0.02f / 0.0072f));
  CHECK(near(v.winmat[2][2], -(1000.0f + 0.01f) / (1000.0f - 0.01f)));
  CHECK(v.winmat[2][3] == -1.0f && v.winmat[3][3] == 0.0f && v.is_persp);
  /* Portrait: vertical sensor fit (viewfac = winy). */
  pg_view3d_update(&v, 500, 1000);
  CHECK(near(v.winmat[1][1], 0.02f / 0.0144f));
  /* Ortho: ortho_scale = dist * 36 / lens, clip -500 .. 500. */
  v.persp = PG_RV3D_ORTHO;
  v.dist = 10.0f;
  pg_view3d_update(&v, 1000, 500);
  CHECK(near(v.winmat[0][0], 2.0f / (2.0f * 7.2f)));
  CHECK(near(v.winmat[1][1], 2.0f / 7.2f));
  CHECK(near(v.winmat[2][2], -2.0f / 1000.0f));
  CHECK(v.winmat[3][3] == 1.0f && !v.is_persp);
}

static void test_viewmat_axis_views()
{
  PGView3D v;
  pg_view3d_init(&v);
  pg_view3d_update(&v, 800, 600);
  /* Auto perspective: an axis view is orthographic. */
  pg_view3d_set_axis_view(&v, PG_RV3D_VIEW_FRONT);
  CHECK(v.persp == PG_RV3D_ORTHO && v.view == PG_RV3D_VIEW_FRONT);
  /* Front: world X right, Z up, +Y away from the viewer (view -Z). */
  float x[3] = {1, 0, 0}, z[3] = {0, 0, 1}, y[3] = {0, 1, 0};
  mul_mat3_m4_v3(v.viewmat, x);
  mul_mat3_m4_v3(v.viewmat, z);
  mul_mat3_m4_v3(v.viewmat, y);
  CHECK(near(x[0], 1) && near(z[1], 1) && near(y[2], -1));
  pg_view3d_set_axis_view(&v, PG_RV3D_VIEW_RIGHT);
  float xr[3] = {1, 0, 0};
  mul_mat3_m4_v3(v.viewmat, xr);
  CHECK(near(xr[2], 1)); /* Right: +X towards the viewer */
  pg_view3d_set_axis_view(&v, PG_RV3D_VIEW_TOP);
  float zt[3] = {0, 0, 1};
  mul_mat3_m4_v3(v.viewmat, zt);
  CHECK(near(zt[2], 1)); /* Top: +Z towards the viewer */
  /* Perspective view matrix: rotation, then -dist on view Z, then ofs (view3d_viewmatrix_set). */
  v.persp = PG_RV3D_PERSP;
  v.dist = 5.0f;
  v.ofs[0] = 1.0f;
  pg_view3d_update(&v, 800, 600);
  float m[4][4];
  quat_to_mat4(m, v.viewquat);
  m[3][2] -= 5.0f;
  translate_m4(m, 1.0f, 0.0f, 0.0f);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) CHECK(near(v.viewmat[i][j], m[i][j]));
  }
  /* The persp toggle (viewpersportho_exec). */
  pg_view3d_toggle_persp(&v);
  CHECK(v.persp == PG_RV3D_ORTHO);
  pg_view3d_toggle_persp(&v);
  CHECK(v.persp == PG_RV3D_PERSP);
}

static void test_turntable()
{
  PGView3D v;
  pg_view3d_init(&v);
  pg_view3d_update(&v, 800, 600);
  pg_view3d_set_axis_view(&v, PG_RV3D_VIEW_FRONT);
  /* 0.4 degrees per pixel: 225 px is 90 degrees about world Z. Leaving the axis view restores
   * perspective (ED_view3d_persp_ensure). */
  pg_view3d_orbit(&v, 225.0f, 0.0f, 1.0f);
  CHECK(v.view == PG_RV3D_VIEW_USER && v.persp == PG_RV3D_PERSP);
  CHECK(same_rot(v.viewquat, Q_LEFT));
  pg_view3d_orbit(&v, -450.0f, 0.0f, 1.0f);
  CHECK(same_rot(v.viewquat, Q_RIGHT));
  /* UI scale divides the sensitivity. */
  pg_view3d_orbit(&v, 450.0f, 0.0f, 2.0f);
  CHECK(same_rot(v.viewquat, Q_FRONT));
  /* Up / down about the view X axis: from Front, 90 degrees reaches Top or Bottom. */
  pg_view3d_set_axis_view(&v, PG_RV3D_VIEW_FRONT);
  pg_view3d_orbit(&v, 0.0f, -225.0f, 1.0f);
  CHECK(same_rot(v.viewquat, Q_TOP));
  pg_view3d_set_axis_view(&v, PG_RV3D_VIEW_FRONT);
  pg_view3d_orbit(&v, 0.0f, 225.0f, 1.0f);
  CHECK(same_rot(v.viewquat, Q_BOTTOM));
  /* Start view: in front of the drawing (-Y) and above the floor. */
  pg_view3d_init(&v);
  pg_view3d_update(&v, 800, 600);
  CHECK(v.viewinv[3][1] < 0.0f && v.viewinv[3][2] > 0.0f);
}

static void test_pan_zoom(int persp)
{
  PGView3D v;
  pg_view3d_init(&v);
  v.persp = persp;
  pg_view3d_update(&v, 800, 600);
  /* viewmove_apply: the view centre follows the pointer. */
  float centre[3], before[2], after[2];
  negate_v3_v3(centre, v.ofs);
  pg_view3d_project(&v, centre, before);
  pg_view3d_pan(&v, 30.0f, -20.0f);
  pg_view3d_project(&v, centre, after);
  CHECK(near(after[0], before[0] + 30.0f, 1e-3f) && near(after[1], before[1] - 20.0f, 1e-3f));
  /* view_zoom_to_window_xy_3d: the point of the view-centre plane under the pointer stays. */
  const float mval[2] = {600.0f, 400.0f};
  float c2[3], ref[3], p[2];
  negate_v3_v3(c2, v.ofs);
  pg_gpencil_project_point(&v, mval, c2, PG_LOCKAXIS_VIEW, c2, ref);
  const float dist0 = v.dist;
  pg_view3d_zoom(&v, 0.5f, mval);
  CHECK(near(v.dist, dist0 * 0.5f));
  pg_view3d_project(&v, ref, p);
  CHECK(near(p[0], mval[0], 1e-3f) && near(p[1], mval[1], 1e-3f));
  /* ED_view3d_dist_range_get clamps (grid * 0.001 .. clip_end * 10). */
  pg_view3d_zoom(&v, 1e-9f, nullptr);
  CHECK(near(v.dist, 0.001f));
  pg_view3d_zoom(&v, 1e12f, nullptr);
  CHECK(near(v.dist, 10000.0f));
}

static void test_planes(int persp)
{
  PGView3D v;
  pg_view3d_init(&v);
  v.persp = persp;
  pg_view3d_update(&v, 800, 600);
  const float origin[3] = {0.2f, -0.1f, 0.3f};
  const float mvals[3][2] = {{400, 300}, {120, 500}, {700, 80}};
  for (const auto &mval : mvals) {
    for (int axis = PG_LOCKAXIS_VIEW; axis <= PG_LOCKAXIS_CURSOR; axis++) {
      float out[3], back[2];
      pg_gpencil_project_point(&v, mval, origin, axis, origin, out);
      /* The point is under the finger ... */
      const int ok = pg_view3d_project(&v, out, back);
      /* The Side plane is seen almost edge-on from the start view: a pointer ray far from it meets it
       * behind the eye, as Blender's isect_line_plane_v3 does (the stroke then lands there). */
      CHECK(ok || axis == PG_LOCKAXIS_X);
      if (ok) CHECK(near(back[0], mval[0], 1e-2f) && near(back[1], mval[1], 1e-2f));
      /* ... and on its plane through the origin. */
      if (axis == PG_LOCKAXIS_X) CHECK(near(out[0], origin[0], 1e-4f));
      if (axis == PG_LOCKAXIS_Y) CHECK(near(out[1], origin[1], 1e-4f));
      if (axis == PG_LOCKAXIS_Z || axis == PG_LOCKAXIS_CURSOR) CHECK(near(out[2], origin[2], 1e-4f));
      if (axis == PG_LOCKAXIS_VIEW) {
        /* normal: the view direction at the origin (ED_view3d_global_to_vector) */
        float d[3], n[3];
        sub_v3_v3v3(d, out, origin);
        if (v.is_persp) sub_v3_v3v3(n, origin, v.viewinv[3]);
        else copy_v3_v3(n, v.viewinv[2]);
        normalize_v3(n);
        CHECK(std::fabs(dot_v3v3(d, n)) < 1e-4f);
      }
    }
  }
  /* ED_gpencil_drawing_reference_get */
  const float ob[3] = {1, 2, 3}, cur[3] = {4, 5, 6};
  float r[3];
  pg_gpencil_drawing_reference_get(PG_PROJECT_VIEWSPACE, ob, cur, r);
  CHECK(r[0] == 1.0f);
  pg_gpencil_drawing_reference_get(PG_PROJECT_VIEWSPACE | PG_PROJECT_CURSOR, ob, cur, r);
  CHECK(r[0] == 4.0f);
  pg_gpencil_drawing_reference_get(0, ob, cur, r);
  CHECK(r[0] == 4.0f);
}

int main()
{
  test_winmat();
  test_viewmat_axis_views();
  test_turntable();
  test_pan_zoom(PG_RV3D_PERSP);
  test_pan_zoom(PG_RV3D_ORTHO);
  test_planes(PG_RV3D_PERSP);
  test_planes(PG_RV3D_ORTHO);
  if (g_fail) {
    std::printf("test_view3d: %d failures\n", g_fail);
    return 1;
  }
  std::printf("test_view3d: OK\n");
  return 0;
}
