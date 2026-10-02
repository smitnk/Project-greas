/* Render tests for the focused Android GLES presenter (android_gp_presentation.cpp), run on a
 * software GLES2 context (Mesa llvmpipe through EGL surfaceless pbuffers) and compared pixel by
 * pixel. See tools/run_native_render_tests.sh. */
#include <EGL/egl.h>
#include <GLES2/gl2.h>

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
#include "MEM_guardedalloc.h"

extern "C" int project_grease_android_present_gp_document(const bGPdata *gpd, int frame_number);
extern "C" void project_grease_android_present_set_canvas_size(int width, int height);
extern "C" void project_grease_android_present_set_view_transform(float zoom, float pan_x, float pan_y);
extern "C" void project_grease_android_present_reset();

static const int W = 200, H = 120;
static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static bool init_gl()
{
  EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj, min;
  if (!eglInitialize(d, &maj, &min)) return false;
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                       EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
  EGLConfig cfg;
  EGLint n = 0;
  if (!eglChooseConfig(d, ca, &cfg, 1, &n) || n == 0) return false;
  const EGLint pa[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
  EGLSurface s = eglCreatePbufferSurface(d, cfg, pa);
  const EGLint xa[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, xa);
  if (s == EGL_NO_SURFACE || c == EGL_NO_CONTEXT) return false;
  if (!eglMakeCurrent(d, s, s, c)) return false;
  glViewport(0, 0, W, H);
  return true;
}

struct Rgba {
  int r, g, b, a;
};
static std::vector<uint8_t> g_pixels;
static void read_back()
{
  g_pixels.assign(static_cast<size_t>(W) * H * 4, 0);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, g_pixels.data());
}
/* canvas coordinates (canvas == viewport size) -> pixel, as update_canvas_map() lays them out. */
static Rgba pixel_at_canvas(float cx, float cy)
{
  const float scale = 0.92f;
  const float ox = (W - W * scale) * 0.5f, oy = (H - H * scale) * 0.5f;
  const int px = static_cast<int>(ox + cx * scale);
  const int py = static_cast<int>(oy + cy * scale);
  const int row = H - 1 - py; /* GL rows start at the bottom */
  const uint8_t *p = &g_pixels[(static_cast<size_t>(row) * W + px) * 4];
  return {p[0], p[1], p[2], p[3]};
}
static bool near_rgb(const Rgba &c, int r, int g, int b, int tol = 6)
{
  return std::abs(c.r - r) <= tol && std::abs(c.g - g) <= tol && std::abs(c.b - b) <= tol;
}

struct Doc {
  bGPdata *gpd;
};
static Doc make_doc()
{
  Doc d;
  d.gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "render test gpd"));
  d.gpd->totcol = 2; /* material 0: stroke color set by set_color(); material 1: blue */
  d.gpd->mat = static_cast<Material **>(MEM_callocN(sizeof(Material *) * 2, "mat array"));
  for (int i = 0; i < 2; i++) {
    Material *ma = static_cast<Material *>(MEM_callocN(sizeof(Material), "mat"));
    ma->gp_style = static_cast<MaterialGPencilStyle *>(MEM_callocN(sizeof(MaterialGPencilStyle), "style"));
    ma->gp_style->stroke_rgba[3] = 1.0f;
    ma->gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
    d.gpd->mat[i] = ma;
  }
  d.gpd->mat[1]->gp_style->stroke_rgba[2] = 1.0f;
  return d;
}
static bGPDlayer *add_layer(Doc &d, const char *name)
{
  bGPDlayer *l = BKE_gpencil_layer_addnew(d.gpd, name, true, false);
  BKE_gpencil_frame_addnew(l, 1);
  return l;
}
/* A horizontal stroke from (x0,y) to (x1,y), thickness in canvas px. */
static void add_bar(bGPDlayer *l, float x0, float x1, float y, float thickness, float strength = 1.0f, int material = 0)
{
  bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
  bGPDstroke *s = BKE_gpencil_stroke_add(f, material, 2, static_cast<short>(thickness), false);
  s->points[0].x = x0;
  s->points[1].x = x1;
  for (int i = 0; i < 2; i++) {
    s->points[i].y = y;
    s->points[i].pressure = 1.0f;
    s->points[i].strength = strength;
  }
}
static void set_color(Doc &d, float r, float g, float b)
{
  d.gpd->mat[0]->gp_style->stroke_rgba[0] = r;
  d.gpd->mat[0]->gp_style->stroke_rgba[1] = g;
  d.gpd->mat[0]->gp_style->stroke_rgba[2] = b;
}
static void present(Doc &d)
{
  glViewport(0, 0, W, H);
  CHECK(project_grease_android_present_gp_document(d.gpd, 1) == 1);
  read_back();
}

static void test_baseline()
{
  Doc d = make_doc();
  set_color(d, 1, 0, 0);
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 20, 180, 60, 20);
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 60), 255, 0, 0));        /* on the stroke */
  CHECK(near_rgb(pixel_at_canvas(100, 100), 245, 245, 245));   /* canvas background (0.96) */
  CHECK(near_rgb(pixel_at_canvas(100, 60 + 30), 245, 245, 245)); /* beyond the stroke edge */
}

static void use_mask(bGPDlayer *l, const bGPDlayer *mask_layer, int flags = 0)
{
  l->flag |= GP_LAYER_USE_MASK;
  bGPDlayer_Mask *m = BKE_gpencil_layer_mask_add(l, mask_layer->info);
  m->flag = static_cast<short>(flags);
}

/* The layer M (blue) is a wide bar at y = 60 over x 20..100; A (red) is a bar at y = 60 over the
 * whole width. A masked by M shows only inside M's footprint. */
static Doc mask_doc(bGPDlayer **out_a, bGPDlayer **out_m)
{
  Doc d = make_doc();
  bGPDlayer *m = add_layer(d, "M");
  add_bar(m, 20, 100, 60, 40, 1.0f, 1); /* blue */
  bGPDlayer *a = add_layer(d, "A");
  add_bar(a, 20, 180, 60, 20);
  *out_a = a;
  *out_m = m;
  return d;
}

static void test_masks()
{
  bGPDlayer *a, *m;
  /* the material color is shared: both layers draw red; the mask footprint is what we look at */
  {
    Doc d = mask_doc(&a, &m);
    set_color(d, 1, 0, 0);
    present(d); /* no mask: both bars visible */
    CHECK(near_rgb(pixel_at_canvas(60, 60), 255, 0, 0));   /* A over M */
    CHECK(near_rgb(pixel_at_canvas(140, 60), 255, 0, 0));
    CHECK(near_rgb(pixel_at_canvas(60, 45), 0, 0, 255));   /* M alone */
  }
  {
    Doc d = mask_doc(&a, &m);
    set_color(d, 1, 0, 0);
    use_mask(a, m);
    present(d); /* A is clipped to M: right half of A is gone */
    CHECK(near_rgb(pixel_at_canvas(60, 60), 255, 0, 0));
    CHECK(near_rgb(pixel_at_canvas(140, 60), 245, 245, 245));
    CHECK(near_rgb(pixel_at_canvas(60, 45), 0, 0, 255)); /* M itself is still drawn */
  }
  {
    Doc d = mask_doc(&a, &m);
    set_color(d, 1, 0, 0);
    use_mask(a, m, GP_MASK_INVERT);
    present(d); /* inverted: A shows only outside M */
    CHECK(near_rgb(pixel_at_canvas(140, 60), 255, 0, 0));
    CHECK(near_rgb(pixel_at_canvas(60, 60), 0, 0, 255)); /* inside M, A is gone: M's blue shows */
  }
  {
    Doc d = mask_doc(&a, &m);
    set_color(d, 1, 0, 0);
    use_mask(a, m);
    a->flag &= ~GP_LAYER_USE_MASK; /* use-mask off: the mask list is ignored */
    present(d);
    CHECK(near_rgb(pixel_at_canvas(140, 60), 255, 0, 0));
  }
  {
    Doc d = mask_doc(&a, &m);
    set_color(d, 1, 0, 0);
    use_mask(a, m, GP_MASK_HIDE); /* hidden entry: no valid mask, so A is drawn unmasked */
    present(d);
    CHECK(near_rgb(pixel_at_canvas(140, 60), 255, 0, 0));
    m->flag |= GP_LAYER_HIDE;
    static_cast<bGPDlayer_Mask *>(a->mask_layers.first)->flag = 0; /* visible entry, hidden layer */
    present(d);
    CHECK(near_rgb(pixel_at_canvas(140, 60), 255, 0, 0)); /* hidden mask layer is ignored too */
  }
  {
    /* two masks: the union of both footprints */
    Doc d = make_doc();
    set_color(d, 1, 0, 0);
    bGPDlayer *m1 = add_layer(d, "M1");
    add_bar(m1, 20, 60, 100, 20); /* away from A; coverage of x 20..60 around y 100 */
    bGPDlayer *m2 = add_layer(d, "M2");
    add_bar(m2, 120, 160, 100, 20);
    bGPDlayer *act = add_layer(d, "A2");
    /* A2 is a tall block at y 100 (thick 20) over the whole width */
    add_bar(act, 20, 180, 100, 20);
    use_mask(act, m1);
    use_mask(act, m2);
    present(d);
    /* M1/M2 are drawn red themselves, so look at a row where only A2 and no mask overlap: none.
     * Instead check the gap between the masks (x = 90): A2 is clipped there. */
    CHECK(near_rgb(pixel_at_canvas(90, 100), 245, 245, 245));
    CHECK(near_rgb(pixel_at_canvas(40, 100), 255, 0, 0));
    CHECK(near_rgb(pixel_at_canvas(140, 100), 255, 0, 0));
  }
  {
    /* a half-transparent mask: A shows at half strength through it */
    Doc d = make_doc();
    set_color(d, 1, 0, 0);
    bGPDlayer *m1 = add_layer(d, "M");
    add_bar(m1, 20, 180, 20, 20, 0.5f);    /* y = 20, strength .5, away from A */
    bGPDlayer *act = add_layer(d, "A");
    add_bar(act, 20, 180, 20, 8);          /* A over the same line, thinner */
    use_mask(act, m1);
    present(d);
    /* the pixel shows M (50% red over background) with A (50% via the mask) on top: both at 0.5 */
    const Rgba c = pixel_at_canvas(100, 20);
    CHECK(c.r > 240 && c.g > 60 && c.g < 190);
  }
}

int main()
{
  if (!init_gl()) {
    if (std::getenv("PG_REQUIRE_GL")) {
      printf("no GLES context (set up Mesa EGL, EGL_PLATFORM=surfaceless)\n");
      return 1;
    }
    printf("SKIP render tests (no GLES context)\n");
    return 0;
  }
  project_grease_android_present_set_canvas_size(W, H);
  project_grease_android_present_set_view_transform(1.0f, 0.0f, 0.0f);
  test_baseline();
  test_masks();
  project_grease_android_present_reset();
  if (failures) {
    printf("%d FAILURES\n", failures);
    return 1;
  }
  printf("render tests passed\n");
  return 0;
}
