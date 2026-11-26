/* L8 part 2 render test: Blender 3.6.23's Grease Pencil draw engine (draw/engines/gpencil and
 * draw_cache_impl_gpencil.cc, unmodified) through Project Grease's GLES 3.2 draw-manager adapter
 * (native/blender_gp/gles), on a software OpenGL ES 3.2 context (Mesa llvmpipe, EGL surfaceless).
 *
 * What is checked is what the engine's own math says the image must be (stroke width =
 * thickness * pixfactor / 1000 * scale, colours of the materials, layer opacity through the
 * layer-blend pass, fill inside its outline). A pixel comparison with an image Blender rendered
 * is NOT part of this test (UNVERIFIED: needs a Blender 3.6.23 render of the same document). */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "ED_gpencil_legacy.h"
#include "MEM_guardedalloc.h"

#include "gles/pg_gpu_gles.h"
#include "project_grease_shader_fx.h"
#include "project_grease_armature.h"
#include "DNA_armature_types.h"

extern "C" void project_grease_android_present_set_live_stroke(int active, float thickness, float hardness, float ax, float ay, int caps, int material);
extern "C" void project_grease_android_present_reset();
extern "C" int project_grease_android_present_gp_document(const bGPdata *gpd, int frame_number);
extern "C" void project_grease_view3d_set_active(int active);
extern "C" void project_grease_view3d_reset();
extern "C" void project_grease_view3d_set_axis_view(int view);
extern "C" void project_grease_view3d_orbit(float dx, float dy, float ui_scale);
extern "C" void project_grease_view3d_zoom(float dfac, float sx, float sy);
extern "C" void project_grease_view3d_set_placement(int align, int lock_axis);
extern "C" void project_grease_view3d_stroke_done(bGPdata *gpd, bGPDstroke *gps);
extern "C" int project_grease_view3d_canvas_to_screen(float x, float y, float z, float *out);
extern "C" void project_grease_android_present_set_canvas_size(int width, int height);
extern "C" void project_grease_android_present_set_data_revision(uint64_t revision);
extern "C" int project_grease_android_present_engine_active();
extern "C" const char *project_grease_android_present_last_frame();
extern "C" int project_grease_android_present_selftest_runs();
extern "C" long project_grease_android_present_engine_finishes();
extern "C" void project_grease_android_present_engine_reset();
extern "C" void project_grease_android_set_fx_provider(int (*fn)(void *, const bGPDlayer *, const PGFxEntry **), void *user);
static PGFxEntry g_test_fx;
static int test_fx_provider(void *, const bGPDlayer *, const PGFxEntry **out) { *out = &g_test_fx; return 1; }
extern "C" int project_grease_android_present_set_reference(const unsigned char *rgba, int w, int h);
extern "C" void project_grease_android_present_set_reference_style(int visible, float opacity);

static const int W = 200, H = 120;
extern "C" void project_grease_android_present_set_view_rotation(float radians);
static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static bool init_gl()
{
  EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj, min;
  if (!eglInitialize(d, &maj, &min)) return false;
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                       EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                       EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
  EGLConfig cfg;
  EGLint n = 0;
  if (!eglChooseConfig(d, ca, &cfg, 1, &n) || n == 0) return false;
  const EGLint pa[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
  EGLSurface s = eglCreatePbufferSurface(d, cfg, pa);
  const char *es30 = std::getenv("PG_FORCE_GLES30");
  const EGLint minor = (es30 && es30[0] == '1') ? 0 : 2;
  const EGLint xa[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, minor, EGL_NONE};
  EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, xa);
  if (s == EGL_NO_SURFACE || c == EGL_NO_CONTEXT) return false;
  if (!eglMakeCurrent(d, s, s, c)) return false;
  std::printf("GL_VERSION %s | GL_RENDERER %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
  return true;
}

/* -------------------------------------------------------------------- */

static Material *new_material(float r, float g, float b, bool stroke, bool fill)
{
  Material *ma = static_cast<Material *>(MEM_callocN(sizeof(Material), "mat"));
  MaterialGPencilStyle *st = static_cast<MaterialGPencilStyle *>(MEM_callocN(sizeof(*st), "style"));
  ma->gp_style = st;
  /* BKE_gpencil_material_attr_init defaults */
  st->stroke_rgba[3] = 1.0f;
  st->fill_rgba[3] = 1.0f;
  st->mix_rgba[0] = st->mix_rgba[1] = st->mix_rgba[2] = st->mix_rgba[3] = 1.0f;
  st->texture_scale[0] = st->texture_scale[1] = 1.0f;
  st->texture_offset[0] = -0.5f;
  st->texture_pixsize = 100.0f;
  st->mix_factor = 0.5f;
  st->stroke_rgba[0] = st->fill_rgba[0] = r;
  st->stroke_rgba[1] = st->fill_rgba[1] = g;
  st->stroke_rgba[2] = st->fill_rgba[2] = b;
  st->flag = (stroke ? GP_MATERIAL_STROKE_SHOW : 0) | (fill ? GP_MATERIAL_FILL_SHOW : 0);
  return ma;
}

static bGPDstroke *add_stroke(bGPdata *gpd, bGPDframe *f, int mat, const float (*xy)[2], int n, short thickness, bool cyclic)
{
  bGPDstroke *gps = BKE_gpencil_stroke_new(mat, n, thickness);
  for (int i = 0; i < n; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->x = xy[i][0];
    pt->y = xy[i][1];
    pt->z = 0.0f;
    pt->pressure = 1.0f;
    pt->strength = 1.0f;
  }
  gps->hardeness = 1.0f;
  gps->aspect_ratio[0] = gps->aspect_ratio[1] = 1.0f;
  if (cyclic) gps->flag |= GP_STROKE_CYCLIC;
  BLI_addtail(&f->strokes, gps);
  BKE_gpencil_stroke_geometry_update(gpd, gps);
  return gps;
}

static bGPdata *make_doc()
{
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
  gpd->totcol = 3;
  gpd->mat = static_cast<Material **>(MEM_callocN(sizeof(Material *) * 3, "mats"));
  gpd->mat[0] = new_material(0.0f, 0.0f, 0.0f, true, false); /* black line */
  gpd->mat[1] = new_material(1.0f, 0.0f, 0.0f, false, true); /* red fill only */
  gpd->mat[2] = new_material(0.0f, 0.0f, 1.0f, true, false); /* blue line */
  return gpd;
}

/* -------------------------------------------------------------------- */

static std::vector<float> g_px;
static void read_result()
{
  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pg_gp_engine_result_texture(), 0);
  g_px.assign((size_t)W * H * 4, -1.0f);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, g_px.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo);
}
/* canvas pixel (x, y down) with view origin (0, 0), scale 1 -> GL row from the bottom */
static const float *px(int x, int y)
{
  return &g_px[((size_t)(H - 1 - y) * W + (size_t)x) * 4];
}
static bool near4(const float *c, float r, float g, float b, float a, float tol = 0.02f)
{
  return std::fabs(c[0] - r) <= tol && std::fabs(c[1] - g) <= tol && std::fabs(c[2] - b) <= tol &&
         std::fabs(c[3] - a) <= tol;
}
static void dump(const char *what, int x, int y)
{
  const float *c = px(x, y);
  std::printf("  %s (%d,%d) = %.3f %.3f %.3f %.3f\n", what, x, y, c[0], c[1], c[2], c[3]);
}

/* 8b + L10 9: the 3D view and the bone overlay; run on the engine path (GLES 3.2 / 3.1) and on the
 * presenter path (PG_FORCE_GLES30: a GLES 3.0 context, engine off, as on the CI emulator). */
static void view3d_and_rig(bGPdata *gpd)
{
  /* 8b. The 3D viewport (android_gp_view3d.inc): the Camera view is the 2D frame, pixel for pixel;
   * the 3D view draws the grid and the strokes through the view matrices; a stroke drawn in 3D is
   * placed on its drawing plane. */
  {
    std::vector<unsigned char> cam0((size_t)W * H * 4), cam1((size_t)W * H * 4), v3((size_t)W * H * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, W, H);
    CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, cam0.data());
    project_grease_view3d_set_active(1);
    CHECK(project_grease_android_present_gp_document(gpd, 1) == 1); /* sizes the region */
    project_grease_view3d_zoom(0.15f, W * 0.5f, H * 0.5f); /* the 0.2 unit wide canvas fills the view */
    for (int view : {1 /* Front */, 0 /* user perspective */}) {
      if (view) project_grease_view3d_set_axis_view(view); else project_grease_view3d_orbit(40.0f, 25.0f, 1.0f);
      CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
      glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, v3.data());
      float s[2] = {0, 0};
      CHECK(project_grease_view3d_canvas_to_screen(50, 30, 0, s) == 1); /* on the line, off the cursor */
      const int sx = int(s[0]), sy = H - 1 - int(s[1]);
      unsigned char c[4] = {};
      int darkest = 255;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          if (sx + dx < 0 || sx + dx >= W || sy + dy < 0 || sy + dy >= H) continue;
          glReadPixels(sx + dx, sy + dy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
          darkest = std::min(darkest, int(c[0]));
        }
      std::printf("  3D view %d: line at %.1f %.1f darkest %d (background 61)\n", view, s[0], s[1], darkest);
      CHECK(darkest < 40);
      CHECK(std::memcmp(v3.data(), cam0.data(), v3.size()) != 0);
      /* the grid's X axis line (red, colorGridAxisX) */
      int red = 0;
      for (size_t i = 0; i < v3.size(); i += 4) red += (v3[i] > v3[i + 1] + 30 && v3[i] > v3[i + 2] + 30) ? 1 : 0;
      std::printf("  3D view %d: %d red axis pixels\n", view, red);
      CHECK(red > 20);
    }
    /* Front X-Z / Top X-Y planes through the origin (Origin placement): a stroke sampled through the
     * 2D map lands under the same screen points, on the plane. */
    {
      bGPDstroke s3{};
      bGPDspoint pts[2] = {};
      pts[0].x = 60; pts[0].y = 40; pts[1].x = 140; pts[1].y = 90;
      s3.points = pts; s3.totpoints = 2;
      for (int lock : {2 /* Y: Front */, 3 /* Z: Top */}) {
        pts[0].x = 60; pts[0].y = 40; pts[0].z = 0; pts[1].x = 140; pts[1].y = 90; pts[1].z = 0;
        project_grease_view3d_set_placement(1 /* GP_PROJECT_VIEWSPACE */, lock);
        project_grease_view3d_stroke_done(gpd, &s3);
        std::printf("  plane %d: %.2f %.2f %.2f / %.2f %.2f %.2f\n", lock, pts[0].x, pts[0].y, pts[0].z, pts[1].x, pts[1].y, pts[1].z);
        if (lock == 2) CHECK(std::fabs(pts[0].z) < 1e-2f && std::fabs(pts[1].z) < 1e-2f);
        if (lock == 3) CHECK(std::fabs(pts[0].y - H * 0.5f) < 1e-2f && std::fabs(pts[1].y - H * 0.5f) < 1e-2f);
      }
      s3.points = nullptr;
    }
    project_grease_view3d_set_active(0);
    CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, cam1.data());
    CHECK(std::memcmp(cam0.data(), cam1.data(), cam0.size()) == 0);
  }

  /* L10 9. Armature overlay (android_gp_armature_overlay.inc, overlay_armature.cc): a 3-bone rig in
   * Octahedral and Stick display, 2D Camera view and 3D view. Bone 2 is active + selected, bone 1
   * selected, bone 0 neither and lies on the black stroke (y 30) for In Front. Colours are the
   * default theme's (DRW_globals_update). */
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W, H);
    PGArmature *a = pg_arm_new();
    const float ys[3] = {30, 60, 90};
    for (float y : ys) {
      float hp[3] = {60, y, 0}, tp[3] = {140, y, 0}, h3[3], t3[3];
      pg_arm_canvas_to_arm(hp, h3);
      pg_arm_canvas_to_arm(tp, t3);
      pg_arm_bone_add(a, "Bone", h3, t3);
    }
    pg_arm_register(gpd, a);
    std::vector<unsigned char> img((size_t)W * H * 4);
    float ox = 0, oy = 0, sc = 1;
    bool v3d = false;
    auto frame = [&]() {
      CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
      glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
      int pw, ph;
      if (const char *dir = std::getenv("PG_RIG_DUMP"); dir && *dir) {
        static int k = 0;
        char path[512];
        std::snprintf(path, sizeof path, "%s/rig%02d.ppm", dir, k++);
        if (FILE *fp = std::fopen(path, "wb")) {
          std::fprintf(fp, "P6 %d %d 255\n", W, H);
          for (int y = H - 1; y >= 0; y--)
            for (int x = 0; x < W; x++) std::fwrite(&img[((size_t)y * W + x) * 4], 1, 3, fp);
          std::fclose(fp);
        }
      }
      if (!v3d) CHECK(std::sscanf(std::strstr(project_grease_android_present_last_frame(), "paper "), "paper %dx%d at %f,%f x%f", &pw, &ph, &ox, &oy, &sc) == 5);
    };
    auto screen = [&](float cx, float cy, float r[2]) { /* canvas px -> window px, y down */
      if (v3d) CHECK(project_grease_view3d_canvas_to_screen(cx, cy, 0, r) == 1);
      else r[0] = ox + cx * sc, r[1] = oy + cy * sc;
    };
    /* pixels near colour c (tolerance 14) within 9 px of bone b */
    auto count = [&](int b, const int c[3]) {
      float s0[2], s1[2];
      screen(60, ys[b], s0);
      screen(140, ys[b], s1);
      int n = 0;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          const float dx = s1[0] - s0[0], dy = s1[1] - s0[1], l2 = dx * dx + dy * dy;
          float t = ((x - s0[0]) * dx + (y - s0[1]) * dy) / l2;
          t = std::clamp(t, 0.0f, 1.0f);
          if (std::hypot(x - s0[0] - t * dx, y - s0[1] - t * dy) > 9.0f) continue;
          const unsigned char *p = &img[((size_t)(H - 1 - y) * W + x) * 4];
          if (std::abs(p[0] - c[0]) <= 14 && std::abs(p[1] - c[1]) <= 14 && std::abs(p[2] - c[2]) <= 14) n++;
        }
      return n;
    };
    auto centre = [&](int b) { /* the pixel at the bone's widest point (local y 0.1) */
      float s[2];
      screen(68, ys[b], s);
      return &img[((size_t)(H - 1 - int(s[1] + 0.5f)) * W + int(s[0] + 0.5f)) * 4];
    };
    const int C_ACT[3] = {255, 220, 60}, C_SEL[3] = {235, 140, 0}, C_VSEL[3] = {255, 122, 0};
    const int C_PACT[3] = {140, 255, 255}, C_PSEL[3] = {80, 200, 255};
    auto select_rig = [&]() {
      if (pg_arm_mode(a) == PG_ARM_MODE_EDIT) {
        pg_arm_edit_select(a, 1, BONE_SELECTED, false);
        pg_arm_edit_select(a, 2, BONE_SELECTED, true);
      }
      else {
        pg_arm_pose_select(a, 1, false);
        pg_arm_pose_select(a, 2, true);
      }
      CHECK(pg_arm_active_bone(a) == 2);
    };
    for (int pass = 0; pass < 2; pass++) {
      v3d = pass == 1;
      if (v3d) {
        project_grease_view3d_set_active(1);
        CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
        project_grease_view3d_set_axis_view(1); /* Front */
        project_grease_view3d_zoom(0.4f, W * 0.5f, H * 0.5f);
      }
      for (int drawtype : {ARM_OCTA, ARM_LINE}) {
        pg_arm_set_display(a, drawtype, true); /* over the earlier tests' strokes */
        /* edit mode */
        pg_arm_set_mode(a, PG_ARM_MODE_EDIT);
        select_rig();
        frame();
        const int e_act[3] = {count(0, C_ACT), count(1, C_ACT), count(2, C_ACT)};
        const int e_sel[3] = {count(0, C_SEL), count(1, C_SEL), count(2, C_SEL)};
        const int e_vs[3] = {count(0, C_VSEL), count(1, C_VSEL), count(2, C_VSEL)};
        std::printf("  rig %s %s edit: active %d %d %d, select %d %d %d, joint select %d %d %d\n", v3d ? "3D" : "2D",
                    drawtype == ARM_OCTA ? "octahedral" : "stick", e_act[0], e_act[1], e_act[2], e_sel[0], e_sel[1],
                    e_sel[2], e_vs[0], e_vs[1], e_vs[2]);
        CHECK(e_act[0] == 0 && e_sel[0] == 0 && e_vs[0] == 0); /* the normal bone: black wire, grey solid */
        CHECK(e_vs[1] > 3 && e_vs[2] > 3);                     /* selected head / tail joints */
        if (drawtype == ARM_OCTA) {
          CHECK(e_act[2] > 10 && e_act[1] == 0); /* active outline */
          CHECK(e_sel[1] > 10 && e_sel[2] == 0); /* selected outline */
          const unsigned char *c = centre(1);
          std::printf("    solid at bone 1: %d %d %d\n", c[0], c[1], c[2]);
          CHECK(std::abs(c[0] - c[1]) < 6 && std::abs(c[1] - c[2]) < 6 && c[0] > 60 && c[0] < 200); /* lit bone_solid */
        }
        else {
          CHECK(e_act[1] > 10 && e_act[2] > 10); /* draw_bone_line: a selected edit bone is color_bone_active */
        }
        /* pose mode */
        pg_arm_set_mode(a, PG_ARM_MODE_POSE);
        select_rig();
        frame();
        const int p_act[3] = {count(0, C_PACT), count(1, C_PACT), count(2, C_PACT)};
        const int p_sel[3] = {count(0, C_PSEL), count(1, C_PSEL), count(2, C_PSEL)};
        std::printf("  rig %s %s pose: active %d %d %d, select %d %d %d\n", v3d ? "3D" : "2D",
                    drawtype == ARM_OCTA ? "octahedral" : "stick", p_act[0], p_act[1], p_act[2], p_sel[0], p_sel[1], p_sel[2]);
        CHECK(p_act[2] > 10 && p_act[0] == 0 && p_act[1] == 0);
        CHECK(p_sel[1] > 10 && p_sel[0] == 0 && p_sel[2] == 0);
      }
      /* In Front: bone 0 lies under the black stroke unless it is drawn in front */
      pg_arm_set_display(a, ARM_OCTA, false);
      frame();
      const unsigned char *c = centre(0);
      std::printf("  rig %s in front off: bone 0 centre %d %d %d\n", v3d ? "3D" : "2D", c[0], c[1], c[2]);
      CHECK(c[0] < 40);
      pg_arm_set_display(a, ARM_OCTA, true);
      frame();
      c = centre(0);
      std::printf("  rig %s in front on: bone 0 centre %d %d %d\n", v3d ? "3D" : "2D", c[0], c[1], c[2]);
      CHECK(c[0] > 60);
      /* L10 B-Bone (draw_bone_box with draw_bone_update_disp_matrix_bbone): bone 1 with 4 segments
       * bent by Curve In / Out X. The middle of segment 1 (local y 0.5 of its display matrix, from
       * BKE_pchan_bbone_spline_setup) has the selected outline around it; the straight bone's middle is now empty. Wire
       * (draw_bone_wire) draws the same curve in the selected pose wire colour. */
      {
        pg_arm_set_mode(a, PG_ARM_MODE_POSE);
        select_rig();
        float bb[PG_ARM_BBONE] = {1, 1, 0, 0, 0.02f, 0, 0.02f, 0, 1, 1, 1, 1, 1, 1};
        CHECK(pg_arm_bone_set_bbone_size(a, 1, 0.003f, 0.003f) == 1);
        CHECK(pg_arm_bone_set_bbone(a, 1, 4, bb) == 1);
        float segm[32][4][4];
        CHECK(pg_arm_bone_bbone_matrices(a, 1, segm, 32) == 4);
        float mid[3], cpx[3], sm[2], ss[2];
        for (int k = 0; k < 3; k++) mid[k] = segm[1][3][k] + 0.5f * segm[1][1][k];
        pg_arm_arm_to_canvas(mid, cpx);
        std::printf("  rig %s b-bone: segment 1 middle at canvas %.1f %.1f (straight y %.0f)\n", v3d ? "3D" : "2D", cpx[0], cpx[1], ys[1]);
        CHECK(std::fabs(cpx[1] - ys[1]) > 8.0f); /* bent off the straight line by more than the box */
        auto at = [&](const float s2[2]) { return &img[((size_t)(H - 1 - int(s2[1] + 0.5f)) * W + int(s2[0] + 0.5f)) * 4]; };
        /* selected pose outline (bone_pose) pixels within 4 px: the background here is grey too */
        auto blue = [&](const float s2[2]) {
          int n = 0;
          for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++) {
              const int x = int(s2[0] + 0.5f) + dx, y = H - 1 - int(s2[1] + 0.5f) - dy;
              if (x < 0 || x >= W || y < 0 || y >= H) continue;
              const unsigned char *q = &img[((size_t)y * W + x) * 4];
              if (std::abs(q[0] - C_PSEL[0]) <= 20 && std::abs(q[1] - C_PSEL[1]) <= 20 && std::abs(q[2] - C_PSEL[2]) <= 20) n++;
            }
          return n;
        };
        pg_arm_set_display(a, ARM_B_BONE, true);
        frame();
        screen(cpx[0], cpx[1], sm);
        screen(100, ys[1], ss);
        int nb = blue(sm), n0 = blue(ss);
        std::printf("    b-bone outline px: at the curve middle %d, at the straight middle %d\n", nb, n0);
        CHECK(nb > 3 && n0 == 0);
        pg_arm_set_display(a, ARM_OCTA, true); /* the octahedral bone stays straight */
        frame();
        nb = blue(sm);
        n0 = blue(ss);
        std::printf("    octahedral outline px: at the curve middle %d, at the straight middle %d\n", nb, n0);
        CHECK(nb == 0 && n0 > 3);
        pg_arm_set_display(a, ARM_WIRE, true);
        frame();
        const unsigned char *q = at(sm);
        std::printf("    wire mid %d %d %d\n", q[0], q[1], q[2]);
        CHECK(std::abs(q[0] - C_PSEL[0]) <= 20 && std::abs(q[1] - C_PSEL[1]) <= 20 && std::abs(q[2] - C_PSEL[2]) <= 20);
        const float flat[PG_ARM_BBONE] = {1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1};
        CHECK(pg_arm_bone_set_bbone(a, 1, 1, flat) == 1);
        pg_arm_set_display(a, ARM_OCTA, true);
      }
    }
    project_grease_view3d_set_active(0);
    pg_arm_register(gpd, nullptr);
    pg_arm_free(a);
    CHECK(glGetError() == GL_NO_ERROR);
  }
}

int main()
{
  if (!init_gl()) {
    std::printf("FAIL: no OpenGL ES 3.2 context (EGL surfaceless)\n");
    return 1;
  }
  if (const char *es30 = std::getenv("PG_FORCE_GLES30"); es30 && es30[0] == '1') {
    /* GLES 3.0 (SwiftShader on the CI emulator): no engine, the presenter draws the 3D view and bones */
    CHECK(pg_gles_api() == 0);
    bGPdata *gpd = make_doc();
    bGPDframe *f = BKE_gpencil_frame_addnew(BKE_gpencil_layer_addnew(gpd, "Lines", true, false), 1);
    const float line[2][2] = {{20, 30}, {180, 30}};
    add_stroke(gpd, f, 0, line, 2, 20, false);
    project_grease_android_present_set_canvas_size(W, H);
    glViewport(0, 0, W, H);
    CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
    CHECK(project_grease_android_present_engine_active() == 0);
    view3d_and_rig(gpd);
    /* the editor surface comes back with a new context (every activity on the emulator): the bones and
     * the 3D view must draw again (stale bone programs drew nothing) */
    project_grease_android_present_reset();
    EGLDisplay d = eglGetCurrentDisplay();
    EGLContext old_ctx = eglGetCurrentContext();
    EGLSurface surf = eglGetCurrentSurface(EGL_DRAW);
    EGLConfig cfg;
    EGLint n = 0;
    const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                         EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
    eglChooseConfig(d, ca, &cfg, 1, &n);
    const EGLint xa[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 0, EGL_NONE};
    EGLContext fresh = eglCreateContext(d, cfg, EGL_NO_CONTEXT, xa);
    CHECK(fresh != EGL_NO_CONTEXT && eglMakeCurrent(d, surf, surf, fresh));
    eglDestroyContext(d, old_ctx);
    glViewport(0, 0, W, H);
    std::printf("  after a new context:\n");
    project_grease_view3d_reset(); /* the view of the first pass (zoom / orbit) as the app's nativeReset */
    view3d_and_rig(gpd);
    std::printf(failures ? "%d FAILURES (GLES 3.0 presenter)\n" : "OK (GLES 3.0 presenter)\n", failures);
    return failures ? 1 : 0;
  }
  {
    const char *force = std::getenv("PG_FORCE_GLES31");
    const int api = pg_gles_api();
    std::printf("  engine GL ES API: %d\n", api);
    CHECK(api == ((force && force[0] == '1') ? 31 : 32));
  }
  const float paper[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  PGGpEngineView view = {W, H, 0.0f, 0.0f, 1.0f, {paper[0], paper[1], paper[2], paper[3]}, 1};
  view.weight_group = -1;
  view.vertex_opacity = 1.0f;

  bGPdata *gpd = make_doc();
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "Lines", true, false);
  bGPDframe *f = BKE_gpencil_frame_addnew(gpl, 1);
  const float line[2][2] = {{20, 30}, {180, 30}};
  add_stroke(gpd, f, 0, line, 2, 20, false);

  /* 1. Every engine shader compiles and links; the frame draws. */
  const int ok = pg_gp_engine_draw(gpd, 1, &view);
  if (!ok) std::printf("  error: %s\n", pg_gpu_gles_last_error());
  CHECK(ok == 1);
  CHECK(glGetError() == GL_NO_ERROR);
  read_result();
  dump("line centre", 100, 30);
  dump("background", 100, 90);
  /* 2. A black line on white paper, the paper elsewhere. */
  CHECK(near4(px(100, 30), 0, 0, 0, 1));
  CHECK(near4(px(100, 90), 1, 1, 1, 1));
  /* 3. Width: thickness 20 * pixfactor 1000 / 1000 * scale 1 = 20 px (gpencil_stroke_thickness_modulate). */
  {
    int nonwhite = 0, minx = W, maxx = -1, miny = H, maxy = -1;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
      if (px(x, y)[0] < 0.999f) { nonwhite++; minx = std::min(minx, x); maxx = std::max(maxx, x); miny = std::min(miny, y); maxy = std::max(maxy, y); }
    }
    std::printf("  non-white pixels %d in x %d..%d y %d..%d\n", nonwhite, minx, maxx, miny, maxy);
    std::printf("  column x=100 y=20..40:");
    for (int y = 20; y <= 40; y++) std::printf(" %.2f", px(100, y)[0]);
    std::printf("\n");
  }
  int dark = 0;
  for (int y = 0; y < H; y++) dark += px(100, y)[0] < 0.5f ? 1 : 0;
  std::printf("  covered rows at x=100: %d (expected 20)\n", dark);
  CHECK(dark >= 19 && dark <= 21);
  /* 4. Round caps by default: the line reaches 10 px beyond its end points, not further. */
  CHECK(px(15, 30)[0] < 0.5f);
  CHECK(px(8, 30)[0] > 0.5f);

  /* 5. A filled square (fill only material) is red inside. */
  const float sq[4][2] = {{40, 60}, {100, 60}, {100, 110}, {40, 110}};
  add_stroke(gpd, f, 1, sq, 4, 10, true);
  CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
  read_result();
  dump("fill inside", 70, 85);
  dump("fill outside", 150, 85);
  CHECK(near4(px(70, 85), 1, 0, 0, 1));
  CHECK(near4(px(150, 85), 1, 1, 1, 1));

  /* 6. Layer opacity 0.5 goes through the layer-blend pass: half blue over white, linear. */
  bGPDlayer *gpl2 = BKE_gpencil_layer_addnew(gpd, "Half", true, false);
  bGPDframe *f2 = BKE_gpencil_frame_addnew(gpl2, 1);
  const float vline[2][2] = {{160, 50}, {160, 110}};
  add_stroke(gpd, f2, 2, vline, 2, 20, false);
  gpl2->opacity = 0.5f;
  CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
  read_result();
  dump("half blue", 160, 80);
  CHECK(near4(px(160, 80), 0.5f, 0.5f, 1.0f, 1.0f, 0.03f));

  /* Update tagging: without data_changed the batch cache is kept (a new stroke is not drawn yet);
   * with it the cache is rebuilt. */
  const float probe[2][2] = {{20, 100}, {30, 100}};
  add_stroke(gpd, f, 0, probe, 2, 10, false);
  view.data_changed = 0;
  CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
  read_result();
  CHECK(px(25, 100)[0] > 0.9f); /* cached: not there yet */
  view.data_changed = 1;
  CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
  read_result();
  dump("probe after tag", 25, 100);
  CHECK(px(25, 100)[0] < 0.1f);

  /* Two-finger view rotation in the view matrix: a quarter turn about the viewport centre (100, 60)
   * takes the line y = 30 to the column x = 130; the width stays 20 px. */
  view.rotation = 1.5707964f;
  view.data_changed = 1;
  CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
  read_result();
  dump("rotated line", 130, 100);
  CHECK(px(130, 100)[0] < 0.1f);
  CHECK(px(150, 30)[0] > 0.9f);
  {
    int dark = 0;
    for (int x = 0; x < W; x++) dark += px(x, 100)[0] < 0.5f ? 1 : 0;
    std::printf("  rotated line width at y=100: %d\n", dark);
    CHECK(dark >= 19 && dark <= 21);
  }
  view.rotation = 0.0f;
  /* Onion skin by the engine (BKE_gpencil_visible_stroke_advanced_iter, do_onion): at frame 2 the
   * frame-1 line shows as a ghost, fainter than the frame's own stroke. */
  {
    bGPDframe *f2 = BKE_gpencil_frame_addnew(gpl, 2);
    const float l2[2][2] = {{20, 10}, {60, 10}};
    add_stroke(gpd, f2, 0, l2, 2, 6, false);
    gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
    gpl->onion_flag |= GP_LAYER_ONIONSKIN;
    gpd->onion_mode = GP_ONION_MODE_RELATIVE;
    gpd->gstep = 1;
    gpd->gstep_next = 1;
    gpd->onion_factor = 0.5f;
    view.data_changed = 1;
    CHECK(pg_gp_engine_draw(gpd, 2, &view) == 1);
    read_result();
    dump("frame 2 stroke", 40, 10);
    dump("onion ghost of frame 1", 100, 30);
    CHECK(px(40, 10)[0] < 0.1f);
    CHECK(px(100, 30)[0] > 0.05f && px(100, 30)[0] < 0.95f);
    gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
    BLI_remlink(&gpl->frames, f2);
    BKE_gpencil_free_strokes(f2);
    MEM_freeN(f2);
    view.data_changed = 1;
    CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
  }

  /* Shader effects by the engine (gpencil_shader_fx.c) in the Rendered viewport: a Colorize
   * Grayscale effect on the drawing turns the red fill grey; without Rendered they are skipped
   * (simplify_fx), as in Blender's Material Preview. */
  {
    ColorizeShaderFxData *fx = static_cast<ColorizeShaderFxData *>(MEM_callocN(sizeof(ColorizeShaderFxData), "fx"));
    fx->shaderfx.type = eShaderFxType_Colorize;
    fx->shaderfx.mode = eShaderFxMode_Realtime | eShaderFxMode_Render | eShaderFxMode_Editmode;
    fx->mode = eShaderFxColorizeMode_GrayScale;
    fx->factor = 1.0f;
    ListBase list = {nullptr, nullptr};
    BLI_addtail(&list, fx);
    view.shader_fx = &list;
    view.rendered = 1;
    view.data_changed = 1;
    CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
    read_result();
    dump("fill under grayscale", 70, 85);
    const float *g = px(70, 85);
    CHECK(std::fabs(g[0] - g[1]) < 0.02f && std::fabs(g[1] - g[2]) < 0.02f && g[0] < 0.9f);
    view.rendered = 0;
    CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
    read_result();
    CHECK(near4(px(70, 85), 1, 0, 0, 1)); /* Material Preview: no effects */
    view.shader_fx = nullptr;
    MEM_freeN(fx);
  }

  /* Weight Paint view: Blender's overlay edit lines / points in weight colours (overlay_gpencil.cc,
   * doWeightColor): weight 1 is red on Blender's default ramp. */
  {
    bDeformGroup *dg = static_cast<bDeformGroup *>(MEM_callocN(sizeof(bDeformGroup), "dg"));
    std::strcpy(dg->name, "Group");
    BLI_addtail(&gpd->vertex_group_names, dg);
    bGPDstroke *gps = static_cast<bGPDstroke *>(f->strokes.first); /* the line y = 30 */
    gps->dvert = static_cast<MDeformVert *>(MEM_callocN(sizeof(MDeformVert) * gps->totpoints, "dvert"));
    for (int i = 0; i < gps->totpoints; i++) {
      gps->dvert[i].dw = static_cast<MDeformWeight *>(MEM_callocN(sizeof(MDeformWeight), "dw"));
      gps->dvert[i].totweight = 1;
      gps->dvert[i].dw[0].def_nr = 0;
      gps->dvert[i].dw[0].weight = 1.0f;
    }
    view.weight_group = 0;
    view.data_changed = 1;
    CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
    if (glGetError() != GL_NO_ERROR) std::printf("  GL error after the weight overlay\n");
    read_result();
    dump("weight 1 point (overlay)", 20, 30);
    dump("weight 1 edit line", 100, 30);
    CHECK(px(100, 30)[0] > 0.5f && px(100, 30)[1] < 0.3f && px(100, 30)[2] < 0.3f);
    view.weight_group = -1;
    view.data_changed = 1;
    CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
    read_result();
    CHECK(near4(px(100, 30), 0, 0, 0, 1)); /* normal view again */
  }

  /* 6b. Vertex colour (Draw mode "Per stroke" colour): a red vertex colour over the black material
   * shows red at Vertex Paint Opacity 1, and the material black at opacity 0 (overlay
   * gpencil_vertex_paint_opacity, gpencil_cache_utils.c vert_col_opacity). */
  {
    bGPDstroke *vs = static_cast<bGPDstroke *>(f->strokes.first);
    for (int i = 0; i < vs->totpoints; i++) {
      vs->points[i].vert_color[0] = 1; vs->points[i].vert_color[1] = 0; vs->points[i].vert_color[2] = 0;
      vs->points[i].vert_color[3] = 1;
    }
    view.data_changed = 1;
    CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
    read_result();
    dump("vertex colour red", 100, 30);
    CHECK(px(100, 30)[0] > 0.8f && px(100, 30)[1] < 0.2f && px(100, 30)[2] < 0.2f);
    view.vertex_opacity = 0.0f;
    view.data_changed = 1;
    CHECK(pg_gp_engine_draw(gpd, 1, &view) == 1);
    read_result();
    CHECK(near4(px(100, 30), 0, 0, 0, 1));
    view.vertex_opacity = 1.0f;
    for (int i = 0; i < vs->totpoints; i++) vs->points[i].vert_color[3] = 0;
    view.data_changed = 1;
  }

  /* 7. Present: Standard view transform (linear 0.5 -> sRGB 0.735). */
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  CHECK(pg_gp_engine_present(W, H) == 1);
  unsigned char rgba[4] = {};
  glReadPixels(160, H - 1 - 80, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  std::printf("  presented half blue = %d %d %d %d (expected ~188 188 255 255)\n", rgba[0], rgba[1], rgba[2], rgba[3]);
  CHECK(std::abs(rgba[0] - 188) <= 4 && rgba[2] >= 250);

  /* 8. The Android presenter switches its canvas to the engine on a GLES 3.2 context. */
  project_grease_android_present_set_canvas_size(W, H);
  project_grease_android_present_set_data_revision(42);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, W, H);
  CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
  CHECK(project_grease_android_present_engine_active() == 1);
  {
    const float sc = 0.92f, ox = (W - W * sc) * 0.5f, oy = (H - H * sc) * 0.5f;
    unsigned char c[4] = {};
    glReadPixels(int(ox + 100 * sc), H - 1 - int(oy + 30 * sc), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
    std::printf("  presenter line centre = %d %d %d\n", c[0], c[1], c[2]);
    CHECK(c[0] < 40);
    glReadPixels(int(ox + 130 * sc), H - 1 - int(oy + 50 * sc), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
    std::printf("  presenter paper = %d %d %d (expected 245)\n", c[0], c[1], c[2]);
    CHECK(std::abs(c[0] - 245) <= 3);
  }

  view3d_and_rig(gpd);

  /* 9. A photo reference on the engine path (reported: imported photos never showed): a red image
   * at full opacity covers the paper, the engine drawing (clear background) stays on top. */
  {
    const unsigned char red[4 * 4] = {255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255};
    CHECK(project_grease_android_present_set_reference(red, 2, 2) == 1);
    project_grease_android_present_set_reference_style(1, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, W, H);
    CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
    CHECK(project_grease_android_present_engine_active() == 1);
    const float sc = 0.92f, ox = (W - W * sc) * 0.5f, oy = (H - H * sc) * 0.5f;
    unsigned char c[4] = {};
    glReadPixels(int(ox + 130 * sc), H - 1 - int(oy + 50 * sc), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
    std::printf("  reference under the drawing = %d %d %d (expected red)\n", c[0], c[1], c[2]);
    CHECK(c[0] > 200 && c[1] < 60 && c[2] < 60);
    glReadPixels(int(ox + 100 * sc), H - 1 - int(oy + 30 * sc), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
    std::printf("  line over the reference = %d %d %d (expected black)\n", c[0], c[1], c[2]);
    CHECK(c[0] < 60);
    /* reported: with the view turned and a photo shown the canvas went black */
    project_grease_android_present_set_view_rotation(0.02f);
    CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
    glReadPixels(int(ox + 130 * sc), H - 1 - int(oy + 50 * sc), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
    std::printf("  turned view: reference under the drawing = %d %d %d (expected red)\n", c[0], c[1], c[2]);
    CHECK(c[0] > 200 && c[1] < 60 && c[2] < 60);
    project_grease_android_present_set_view_rotation(0.0f);
    project_grease_android_present_set_reference(nullptr, 0, 0);
  }

  /* The engine self-test runs once per GPU per process: a latch reset (context recovery on the same
   * GPU) reuses its verdict instead of drawing and reading back again on the next first frame. */
  {
    CHECK(project_grease_android_present_selftest_runs() == 1);
    project_grease_android_present_engine_reset();
    glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, W, H);
    CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
    CHECK(project_grease_android_present_engine_active() == 1);
    std::printf("  self-test runs after a reset: %d (expected 1)\n", project_grease_android_present_selftest_runs());
    CHECK(project_grease_android_present_selftest_runs() == 1);
    /* only the context's first engine frame stalls on glFinish, not every redrawn frame since */
    std::printf("  engine glFinish calls: %ld (expected 1)\n", project_grease_android_present_engine_finishes());
    CHECK(project_grease_android_present_engine_finishes() == 1);
  }

  /* Batch 6: a frame with unchanged effects reuses the last engine image (effects were re-run on
   * every frame); changing an effect setting draws again. */
  {
    std::memset(&g_test_fx, 0, sizeof g_test_fx);
    g_test_fx.type = PG_FX_COLORIZE; g_test_fx.enabled = 1;
    g_test_fx.params[PG_FXP_COLORIZE_MODE] = 0; g_test_fx.params[PG_FXP_COLORIZE_FACTOR] = 1.0f;
    project_grease_android_set_fx_provider(test_fx_provider, nullptr);
    auto draw = [&](std::vector<unsigned char> &px) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, W, H);
      CHECK(project_grease_android_present_gp_document(gpd, 1) == 1);
      px.resize((size_t)W * H * 4); glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
      return std::strstr(project_grease_android_present_last_frame(), "reused last image") != nullptr;
    };
    std::vector<unsigned char> a, b, c;
    draw(a);
    std::printf("  effects drawn: %s\n", project_grease_android_present_last_frame());
    const bool reused = draw(b);
    std::printf("  unchanged effects: %s\n", project_grease_android_present_last_frame());
    CHECK(reused);
    CHECK(a == b);
    g_test_fx.params[PG_FXP_COLORIZE_FACTOR] = 0.0f;
    CHECK(!draw(c));
    project_grease_android_set_fx_provider(nullptr, nullptr);
  }

  /* A9: the stroke being drawn looks like the stroke it is committed as (hardness, flat caps, aspect),
   * because both go through Blender's gpencil engine. */
  {
    const int N = 12;
    const float hard = 0.3f, ax = 1.0f, ay = 0.5f;
    const short thick = 14;
    auto fill_pts = [&](float (*xy)[2]) { for (int i = 0; i < N; i++) { xy[i][0] = 30.0f + 12.0f * i; xy[i][1] = 60.0f + 6.0f * std::sin(i * 0.6f); } };
    float xy[N][2];
    fill_pts(xy);
    auto frame_pixels = [&](bGPdata *d, uint64_t rev) {
      project_grease_android_present_set_canvas_size(W, H);
      project_grease_android_present_set_data_revision(rev);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, W, H);
      CHECK(project_grease_android_present_gp_document(d, 1) == 1);
      std::vector<unsigned char> out((size_t)W * H * 4);
      glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
      return out;
    };
    auto diff = [&](const std::vector<unsigned char> &a, const std::vector<unsigned char> &b, int tol, int *max_d) {
      int n = 0; *max_d = 0;
      for (size_t i = 0; i < a.size(); i++) {
        const int dd = std::abs((int)a[i] - (int)b[i]);
        *max_d = std::max(*max_d, dd);
        if (dd > tol && (i & 3) != 3) n++;
      }
      return n;
    };
    auto new_doc = [&]() {
      bGPdata *d = make_doc();
      bGPDlayer *l = BKE_gpencil_layer_addnew(d, "live", true, false);
      BKE_gpencil_frame_addnew(l, 1);
      return d;
    };
    auto commit = [&](bGPdata *d, float hardness, short caps) {
      bGPDlayer *l = BKE_gpencil_layer_active_get(d);
      bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
      bGPDstroke *g = add_stroke(d, f, 0, xy, N, thick, false);
      g->hardeness = hardness;
      g->aspect_ratio[0] = ax;
      g->aspect_ratio[1] = ay;
      g->caps[0] = g->caps[1] = caps;
      BKE_gpencil_stroke_geometry_update(d, g);
    };
    /* live: the same points in the paint buffer + the brush style */
    bGPdata *live = new_doc();
    tGPspoint *sb = static_cast<tGPspoint *>(MEM_callocN(sizeof(tGPspoint) * N, "sbuffer"));
    for (int i = 0; i < N; i++) { sb[i].m_xy[0] = xy[i][0]; sb[i].m_xy[1] = xy[i][1]; sb[i].pressure = 1.0f; sb[i].strength = 1.0f; }
    live->runtime.sbuffer = sb;
    live->runtime.sbuffer_size = N;
    live->runtime.sbuffer_used = N;
    project_grease_android_present_set_live_stroke(1, thick, hard, ax, ay, 1, 0);
    const std::vector<unsigned char> img_live = frame_pixels(live, 900);
    /* committed with that style */
    project_grease_android_present_set_live_stroke(0, 1, 1, 1, 1, 0, 0);
    bGPdata *done = new_doc();
    commit(done, hard, GP_STROKE_CAP_FLAT);
    const std::vector<unsigned char> img_done = frame_pixels(done, 901);
    /* the same stroke with the defaults (hardness 1, round caps, aspect 1) must look different */
    bGPdata *plain = new_doc();
    commit(plain, 1.0f, GP_STROKE_CAP_ROUND);
    bGPDstroke *pg = static_cast<bGPDstroke *>(static_cast<bGPDframe *>(BKE_gpencil_layer_active_get(plain)->frames.first)->strokes.first);
    pg->aspect_ratio[0] = pg->aspect_ratio[1] = 1.0f;
    BKE_gpencil_stroke_geometry_update(plain, pg);
    const std::vector<unsigned char> img_plain = frame_pixels(plain, 902);
    int dark = 0;
    for (size_t i = 0; i < img_done.size(); i += 4) dark += img_done[i] < 120;
    std::printf("  committed stroke dark pixels: %d\n", dark);
    CHECK(dark > 100);
    int md = 0;
    const int n_live = diff(img_live, img_done, 6, &md);
    int md2 = 0;
    const int n_plain = diff(img_plain, img_done, 20, &md2);
    std::printf("  live vs committed: %d channel values differ > 6 (max %d); defaults vs committed: %d differ > 20\n", n_live, md, n_plain);
    CHECK(n_live == 0);
    CHECK(n_plain > 50); /* the style is visible: the test cannot pass with a style-less preview */
    live->runtime.sbuffer_used = 0;
    MEM_freeN(sb);
    live->runtime.sbuffer = nullptr;
  }

  /* Editor surface recreated (log 18): the presenter resets the engine while the old context is
   * current, a new context replaces it, and the next frame still draws the strokes. */
  {
    EGLDisplay d = eglGetCurrentDisplay();
    EGLContext old_ctx = eglGetCurrentContext();
    EGLSurface surf = eglGetCurrentSurface(EGL_DRAW);
    EGLConfig cfg;
    EGLint n = 0;
    const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                         EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
    eglChooseConfig(d, ca, &cfg, 1, &n);
    pg_gp_engine_reset();
    const EGLint xa[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_NONE};
    EGLContext fresh = eglCreateContext(d, cfg, EGL_NO_CONTEXT, xa);
    CHECK(fresh != EGL_NO_CONTEXT && eglMakeCurrent(d, surf, surf, fresh));
    eglDestroyContext(d, old_ctx);
    gpd->runtime.gpencil_cache = nullptr; /* the presenter drops its cache with the context */
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    PGGpEngineView v2 = {W, H, 0.0f, 0.0f, 1.0f, {paper[0], paper[1], paper[2], paper[3]}, 1};
    v2.weight_group = -1;
    v2.vertex_opacity = 1.0f;
    CHECK(pg_gp_engine_draw(gpd, 1, &v2) == 1);
    read_result();
    dump("line after a new context", 100, 30);
    CHECK(near4(px(100, 30), 0, 0, 0, 1));
  }

  /* A8: a reset frees the engine's context object (it used to leak one per reset). */
  {
    CHECK(pg_gp_engine_live_contexts() == 1); /* the one the draws above made */
    for (int i = 0; i < 3; i++) {
      pg_gp_engine_reset();
      CHECK(pg_gp_engine_live_contexts() == 0);
      gpd->runtime.gpencil_cache = nullptr;
      gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
      PGGpEngineView rv = {W, H, 0.0f, 0.0f, 1.0f, {paper[0], paper[1], paper[2], paper[3]}, 1};
      rv.weight_group = -1;
      rv.vertex_opacity = 1.0f;
      CHECK(pg_gp_engine_draw(gpd, 1, &rv) == 1);
      CHECK(pg_gp_engine_live_contexts() == 1);
    }
  }

  /* A6: pooled render targets of a size no longer drawn are freed; the next draw works. */
  {
    PGGpEngineView big = {W + 40, H + 40, 0.0f, 0.0f, 1.0f, {paper[0], paper[1], paper[2], paper[3]}, 1};
    big.weight_group = -1;
    big.vertex_opacity = 1.0f;
    CHECK(pg_gp_engine_draw(gpd, 1, &big) == 1); /* an "export size" frame */
    PGGpEngineView small = {W, H, 0.0f, 0.0f, 1.0f, {paper[0], paper[1], paper[2], paper[3]}, 1};
    small.weight_group = -1;
    small.vertex_opacity = 1.0f;
    CHECK(pg_gp_engine_draw(gpd, 1, &small) == 1); /* back to the screen size: the big ones are unused */
    const int freed = pg_gp_engine_trim_pool(0);
    std::printf("  trim after export size: %d textures freed\n", freed);
    CHECK(freed > 0);
    CHECK(pg_gp_engine_trim_pool(0) == 0); /* what the last draw used stays */
    CHECK(pg_gp_engine_draw(gpd, 1, &small) == 1);
    read_result();
    CHECK(near4(px(100, 30), 0, 0, 0, 1)); /* still draws the line */
    CHECK(pg_gp_engine_trim_pool(1) > 0); /* background: everything */
    CHECK(pg_gp_engine_draw(gpd, 1, &small) == 1);
    read_result();
    CHECK(near4(px(100, 30), 0, 0, 0, 1));
  }

  {
    const char *rep = pg_gles_shader_report();
    std::printf("  shader report: %s\n", rep);
    CHECK(std::strncmp(rep, "17/17 shaders compile", 21) == 0);
  }
  CHECK(glGetError() == GL_NO_ERROR);
  std::printf(failures ? "%d FAILURES\n" : "L8 engine render tests passed\n", failures);
  return failures ? 1 : 0;
}
