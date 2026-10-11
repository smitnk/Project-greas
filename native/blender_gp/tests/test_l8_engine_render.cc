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

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "MEM_guardedalloc.h"

#include "gles/pg_gpu_gles.h"

static const int W = 200, H = 120;
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
  const EGLint xa[] = {EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_NONE};
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

int main()
{
  if (!init_gl()) {
    std::printf("FAIL: no OpenGL ES 3.2 context (EGL surfaceless)\n");
    return 1;
  }
  const float paper[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  PGGpEngineView view = {W, H, 0.0f, 0.0f, 1.0f, {paper[0], paper[1], paper[2], paper[3]}};

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

  /* 7. Present: Standard view transform (linear 0.5 -> sRGB 0.735). */
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  CHECK(pg_gp_engine_present(W, H) == 1);
  unsigned char rgba[4] = {};
  glReadPixels(160, H - 1 - 80, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  std::printf("  presented half blue = %d %d %d %d (expected ~188 188 255 255)\n", rgba[0], rgba[1], rgba[2], rgba[3]);
  CHECK(std::abs(rgba[0] - 188) <= 4 && rgba[2] >= 250);

  CHECK(glGetError() == GL_NO_ERROR);
  std::printf(failures ? "%d FAILURES\n" : "L8 engine render tests passed\n", failures);
  return failures ? 1 : 0;
}
