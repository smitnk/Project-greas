/* Render tests for the focused Android GLES presenter (android_gp_presentation.cpp), run on a
 * software GLES2 context (Mesa llvmpipe through EGL surfaceless pbuffers) and compared pixel by
 * pixel. See tools/run_native_render_tests.sh. */
#include <EGL/egl.h>
#include <GLES2/gl2.h>

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
#include "DNA_meshdata_types.h"
#include "MEM_guardedalloc.h"
#include "project_grease_shader_fx.h"
#include "project_grease_blender_edit6.h"

extern "C" int project_grease_android_present_gp_document(const bGPdata *gpd, int frame_number);
extern "C" void project_grease_android_present_set_canvas_size(int width, int height);
extern "C" void project_grease_android_present_set_view_transform(float zoom, float pan_x, float pan_y);
extern "C" void project_grease_android_present_reset();
extern "C" void project_grease_android_present_set_weight_view(int group);
extern "C" void project_grease_android_present_set_export_mode(int mode);
extern "C" void project_grease_android_present_set_selection_overlay(int enabled);
extern "C" void project_grease_android_present_set_fill_draw_mode(int mode);
extern "C" void project_grease_android_present_set_fill_extend(float factor);
extern "C" int project_grease_android_present_set_material_texture(int slot, int fill, const unsigned char *rgba, int w, int h);
extern "C" int project_grease_android_present_gp_fill_mask(const bGPdata *gpd, int frame_number);
extern "C" int project_grease_fx_pass_count(const PGFxEntry *, int, const PGFxView *);
extern "C" int project_grease_fx_begin_layer(int, int);
extern "C" int project_grease_fx_end_layer(const PGFxEntry *, int, const PGFxView *, unsigned char *, unsigned char *);
extern "C" void project_grease_android_set_fx_provider(
    int (*)(void *, const bGPDlayer *, const PGFxEntry **), void *);

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
                       EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                       EGL_STENCIL_SIZE, 8, EGL_NONE};
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

/* Device report: thick strokes showed wedge "slashes" on the outer side of bends because every
 * segment was an independent quad. A 90 deg bend of a 40 px stroke must be solid at its outer
 * corner (miter join), and the round end cap must be drawn. */
static void test_thick_bend()
{
  Doc d = make_doc();
  set_color(d, 1, 0, 0);
  bGPDlayer *l = add_layer(d, "A");
  bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
  bGPDstroke *s = BKE_gpencil_stroke_add(f, 0, 3, 40, false);
  const float xy[3][2] = {{40, 90}, {100, 90}, {100, 30}};
  for (int i = 0; i < 3; i++) {
    s->points[i].x = xy[i][0];
    s->points[i].y = xy[i][1];
    s->points[i].pressure = 1.0f;
    s->points[i].strength = 1.0f;
  }
  present(d);
  CHECK(near_rgb(pixel_at_canvas(112, 102), 255, 0, 0)); /* outer corner wedge: was a gap */
  CHECK(near_rgb(pixel_at_canvas(100, 90), 255, 0, 0));
  CHECK(near_rgb(pixel_at_canvas(26, 90), 255, 0, 0));   /* round cap beyond the first point */
  CHECK(near_rgb(pixel_at_canvas(70, 60), 245, 245, 245)); /* inside the bend, off the stroke */
}

static Rgba raw_pixel(int x, int y) /* y from the top */
{
  const uint8_t *p = &g_pixels[(static_cast<size_t>(H - 1 - y) * W + x) * 4];
  return {p[0], p[1], p[2], p[3]};
}

/* PNG export path: canvas mapped 1:1 onto the target, transparent background, straight colors. */
static void test_export_transparent()
{
  Doc d = make_doc();
  set_color(d, 1, 0, 0);
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 20, 180, 60, 20);
  add_bar(l, 20, 180, 100, 10, 0.5f); /* half strength */
  project_grease_android_present_set_view_transform(1.0f / 0.92f, 0.0f, 0.0f);
  project_grease_android_present_set_export_mode(2);
  present(d);
  project_grease_android_present_set_export_mode(0);
  project_grease_android_present_set_view_transform(1.0f, 0.0f, 0.0f);
  CHECK(raw_pixel(5, 5).a == 0);            /* no canvas background */
  CHECK(raw_pixel(100, 30).a == 0);
  const Rgba on = raw_pixel(100, 60);
  CHECK(on.a == 255 && near_rgb(on, 255, 0, 0));
  CHECK(raw_pixel(100, 72).a == 0);         /* 1:1: the 20 px bar ends at y = 70 */
  CHECK(raw_pixel(100, 68).a == 255);
  const Rgba half = raw_pixel(100, 100);
  CHECK(half.a > 110 && half.a < 145);      /* alpha kept in the target, not squared */
}

/* Fill boundary "Edit Lines": the mask holds 1 px center lines instead of the full thickness. */
static void test_fill_mask_modes()
{
  Doc d = make_doc();
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 20, 180, 60, 20);
  glViewport(0, 0, W, H);
  project_grease_android_present_set_fill_draw_mode(0);
  CHECK(project_grease_android_present_gp_fill_mask(d.gpd, 1) == 1);
  read_back();
  CHECK(pixel_at_canvas(100, 66).r > 200);   /* inside the 20 px stroke */
  project_grease_android_present_set_fill_draw_mode(2);
  CHECK(project_grease_android_present_gp_fill_mask(d.gpd, 1) == 1);
  read_back();
  CHECK(pixel_at_canvas(100, 66).r < 50);    /* only the center line */
  CHECK(pixel_at_canvas(100, 60).r > 200);
  project_grease_android_present_set_fill_draw_mode(0);
}

/* Onion skin: overlay switch + per-layer GP_LAYER_ONIONSKIN; ghost alpha follows GP_ONION_FADE. */
static void test_onion()
{
  Doc d = make_doc();
  set_color(d, 0, 0, 1);
  bGPDlayer *l = add_layer(d, "A");           /* frame 1: empty */
  bGPDframe *f2 = BKE_gpencil_frame_addnew(l, 2);
  bGPDstroke *s = BKE_gpencil_stroke_add(f2, 0, 2, 20, false);
  s->points[0].x = 20; s->points[1].x = 180;
  for (int i = 0; i < 2; i++) { s->points[i].y = 60; s->points[i].pressure = 1; s->points[i].strength = 1; }
  d.gpd->gstep = 1; d.gpd->gstep_next = 1; d.gpd->onion_factor = 0.5f;
  d.gpd->onion_flag = GP_ONION_FADE;
  l->onion_flag |= GP_LAYER_ONIONSKIN;
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 60), 245, 245, 245)); /* overlay switch off: no ghost */
  d.gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
  present(d);
  const Rgba faded = pixel_at_canvas(100, 60);  /* fade: 1/1 * 0.5 = 0.5 */
  CHECK(faded.b > 240 && faded.r > 100 && faded.r < 140);
  d.gpd->onion_flag = 0;                         /* no fade: 0.5 * 0.5 = 0.25 */
  present(d);
  const Rgba flat = pixel_at_canvas(100, 60);
  CHECK(flat.r > 170 && flat.r < 200);
  l->onion_flag &= ~GP_LAYER_ONIONSKIN;          /* per-layer switch off */
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 60), 245, 245, 245));
}

/* Edit-mode overlay: a selected point shows in Blender's vertex-select orange, an unselected point
 * of an editable stroke as a dark dot, nothing without the overlay. */
static void test_selection_overlay()
{
  Doc d = make_doc();
  set_color(d, 0.9f, 0.9f, 0.9f);
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 40, 160, 60, 2);
  bGPDstroke *s = static_cast<bGPDstroke *>(static_cast<bGPDframe *>(l->frames.first)->strokes.first);
  s->points[1].flag |= GP_SPOINT_SELECT;
  s->flag |= GP_STROKE_SELECT;
  present(d);
  CHECK(!near_rgb(pixel_at_canvas(160, 60), 255, 133, 0, 30)); /* no overlay */
  project_grease_android_present_set_selection_overlay(1);
  present(d);
  const Rgba sel = pixel_at_canvas(160, 60);
  CHECK(near_rgb(sel, 255, 133, 0, 30));                 /* selected point: orange */
  const Rgba unsel = pixel_at_canvas(40, 60);
  CHECK(unsel.r < 80 && unsel.g < 80 && unsel.b < 80);   /* unselected point: dark */
  project_grease_android_present_set_selection_overlay(0);
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

/* Weight Paint view: a stroke whose weights in group 1 run 0 -> 1 is tinted blue -> red, a stroke
 * without weights is blue (weight 0). Blender's ramp: weight 0 = (0,0,.5), 1 = (1,0,0) at the ends. */
static void test_weight_view()
{
  Doc d = make_doc();
  set_color(d, 0, 1, 0); /* the material color is NOT used in the weight view */
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 20, 180, 40, 16);
  add_bar(l, 20, 180, 90, 16);
  bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
  bGPDstroke *weighted = static_cast<bGPDstroke *>(f->strokes.first);
  weighted->dvert = static_cast<MDeformVert *>(MEM_callocN(sizeof(MDeformVert) * 2, "dvert"));
  for (int i = 0; i < 2; i++) {
    weighted->dvert[i].dw = static_cast<MDeformWeight *>(MEM_callocN(sizeof(MDeformWeight), "dw"));
    weighted->dvert[i].totweight = 1;
    weighted->dvert[i].dw[0].def_nr = 1;
    weighted->dvert[i].dw[0].weight = static_cast<float>(i); /* point 0 -> 0, point 1 -> 1 */
  }
  project_grease_android_present_set_weight_view(1);
  present(d);
  project_grease_android_present_set_weight_view(-1);
  /* the middle of the weighted bar has weight 0.5: Blender's ramp gives (r,g,b) = (0, 1, 0) * blend,
   * blend = 0.75 -> a green; the ends are blue-ish (0) and red-ish (1) */
  const Rgba left = pixel_at_canvas(30, 40), mid = pixel_at_canvas(100, 40), right = pixel_at_canvas(170, 40);
  CHECK(left.b > left.r && left.r < 80);
  CHECK(right.r > right.b && right.b < 80);
  CHECK(mid.g > mid.r && mid.g > mid.b);
  /* an unweighted stroke is weight 0: blue, not the material's green */
  const Rgba plain = pixel_at_canvas(100, 90);
  CHECK(plain.b > plain.g && plain.b > plain.r);
  /* the normal view is back: the material color shows */
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 90), 0, 255, 0));
}

/* Device report: Strength 100% drew grey. A stroke with point strength 1 shows the material colour
 * exactly (alpha applied once: material alpha x point strength). */
static void test_strength_full()
{
  Doc d = make_doc();
  set_color(d, 0.0f, 0.0f, 0.0f);
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 20, 180, 60, 20, 1.0f);
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 60), 0, 0, 0, 2));
}

/* A stroke at half strength that crosses itself is blended once per pixel: the crossing is not
 * darker than the rest of the stroke (Blender draws a stroke as one surface). */
static void test_self_overlap()
{
  Doc d = make_doc();
  set_color(d, 0.0f, 0.0f, 0.0f);
  bGPDlayer *l = add_layer(d, "A");
  bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
  /* a figure "Z" folded back: (20,30) -> (180,90) -> (20,90) -> (180,30); segments 1 and 3 cross */
  const float xy[4][2] = {{20, 30}, {180, 90}, {20, 90}, {180, 30}};
  bGPDstroke *s = BKE_gpencil_stroke_add(f, 0, 4, 12, false);
  for (int i = 0; i < 4; i++) {
    s->points[i].x = xy[i][0];
    s->points[i].y = xy[i][1];
    s->points[i].pressure = 1.0f;
    s->points[i].strength = 0.5f;
  }
  present(d);
  const Rgba cross = pixel_at_canvas(100, 60), single = pixel_at_canvas(60, 45);
  CHECK(cross.r < 200);                       /* drawn */
  CHECK(std::abs(cross.r - single.r) <= 3);   /* not darkened by the second pass */
}

/* Device report: a thick rectangle (cyclic primitive) showed offset blocks with misaligned corners.
 * The four corners of a cyclic square are solid miter joins, the edges continuous, the inside empty. */
static void test_cyclic_square()
{
  Doc d = make_doc();
  set_color(d, 1, 0, 0);
  bGPDlayer *l = add_layer(d, "A");
  bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
  const float xy[4][2] = {{50, 30}, {150, 30}, {150, 90}, {50, 90}};
  bGPDstroke *s = BKE_gpencil_stroke_add(f, 0, 4, 16, false);
  s->flag |= GP_STROKE_CYCLIC;
  for (int i = 0; i < 4; i++) {
    s->points[i].x = xy[i][0];
    s->points[i].y = xy[i][1];
    s->points[i].pressure = 1.0f;
    s->points[i].strength = 1.0f;
  }
  present(d);
  for (int i = 0; i < 4; i++) {
    const float ox = xy[i][0] < 100 ? -6.0f : 6.0f, oy = xy[i][1] < 60 ? -6.0f : 6.0f;
    CHECK(near_rgb(pixel_at_canvas(xy[i][0] + ox, xy[i][1] + oy), 255, 0, 0)); /* outer corner */
    CHECK(near_rgb(pixel_at_canvas(xy[i][0], xy[i][1]), 255, 0, 0));
  }
  /* the closing edge (last -> first point) exists */
  CHECK(near_rgb(pixel_at_canvas(50, 60), 255, 0, 0));
  CHECK(near_rgb(pixel_at_canvas(100, 30), 255, 0, 0));
  CHECK(near_rgb(pixel_at_canvas(100, 60), 245, 245, 245)); /* hollow */
  CHECK(near_rgb(pixel_at_canvas(30, 60), 245, 245, 245));  /* nothing outside */
}

static void test_weight_view_smooth()
{
  /* weight display is continuous along a stroke: neighbouring pixels differ by a small step,
   * not by the jumps of per-segment blocks */
  Doc d = make_doc();
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 20, 180, 60, 16);
  bGPDstroke *s = static_cast<bGPDstroke *>(static_cast<bGPDframe *>(l->frames.first)->strokes.first);
  s->dvert = static_cast<MDeformVert *>(MEM_callocN(sizeof(MDeformVert) * 2, "dvert"));
  for (int i = 0; i < 2; i++) {
    s->dvert[i].dw = static_cast<MDeformWeight *>(MEM_callocN(sizeof(MDeformWeight), "dw"));
    s->dvert[i].totweight = 1;
    s->dvert[i].dw[0].def_nr = 0;
    s->dvert[i].dw[0].weight = static_cast<float>(i);
  }
  project_grease_android_present_set_weight_view(0);
  present(d);
  project_grease_android_present_set_weight_view(-1);
  int max_step = 0;
  for (int x = 30; x < 170; x++) {
    const Rgba a = pixel_at_canvas(float(x), 60), b = pixel_at_canvas(float(x + 1), 60);
    max_step = std::max({max_step, std::abs(a.r - b.r), std::abs(a.g - b.g), std::abs(a.b - b.b)});
  }
  CHECK(max_step <= 24);
  /* the stroke edge follows the normal outline (round cap), not a square block */
  CHECK(near_rgb(pixel_at_canvas(14, 54), 245, 245, 245));
}

/* Weight view honours layer masks like the colour view: A (masked by M) shows no weight colour
 * outside M's footprint. */
static void test_weight_view_masked()
{
  bGPDlayer *a, *m;
  Doc d = mask_doc(&a, &m);
  use_mask(a, m);
  project_grease_android_present_set_weight_view(0);
  present(d);
  project_grease_android_present_set_weight_view(-1);
  CHECK(near_rgb(pixel_at_canvas(140, 60), 245, 245, 245)); /* clipped outside M */
  const Rgba in = pixel_at_canvas(60, 60);
  CHECK(in.b > in.r && in.b > 60);                          /* weight 0 (blue) inside M */
}

/* Onion modes and ghost colours: Selected mode shows only selected keyframes; a ghost with its
 * custom colour switched on is drawn in gcolor_prev / gcolor_next instead of the material colour. */
static void test_onion_modes_and_colors()
{
  Doc d = make_doc();
  set_color(d, 0, 0, 0);
  bGPDlayer *l = add_layer(d, "A");                  /* frame 1: empty (current) */
  for (int fn : {2, 3}) {
    bGPDframe *f = BKE_gpencil_frame_addnew(l, fn);
    bGPDstroke *s = BKE_gpencil_stroke_add(f, 0, 2, 16, false);
    s->points[0].x = 20; s->points[1].x = 180;
    const float y = fn == 2 ? 40.0f : 80.0f;
    for (int i = 0; i < 2; i++) { s->points[i].y = y; s->points[i].pressure = 1; s->points[i].strength = 1; }
  }
  d.gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
  l->onion_flag |= GP_LAYER_ONIONSKIN;
  d.gpd->gstep = 0; d.gpd->gstep_next = 5; d.gpd->onion_factor = 1.0f;
  d.gpd->onion_mode = GP_ONION_MODE_RELATIVE;
  d.gpd->onion_flag = GP_ONION_GHOST_NEXTCOL;
  d.gpd->gcolor_next[0] = 0; d.gpd->gcolor_next[1] = 0; d.gpd->gcolor_next[2] = 1;
  present(d);
  const Rgba g2 = pixel_at_canvas(100, 40), g3 = pixel_at_canvas(100, 80);
  CHECK(g2.b > g2.r + 40 && g3.b > g3.r + 40);       /* both next ghosts, tinted blue */
  /* Selected mode: only frame 3 (selected) is a ghost */
  for (bGPDframe *f = static_cast<bGPDframe *>(l->frames.first); f; f = f->next)
    f->flag = f->framenum == 3 ? GP_FRAME_SELECT : 0;
  d.gpd->onion_mode = GP_ONION_MODE_SELECTED;
  d.gpd->onion_flag = 0;                              /* colours off: material black */
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 40), 245, 245, 245));
  const Rgba s3 = pixel_at_canvas(100, 80);
  CHECK(s3.r < 200 && std::abs(s3.r - s3.b) < 8);     /* grey ghost, no tint */
}

/* Fill extend lines: an open stroke prolonged at its ends in the boundary mask (fill_extend_fac). */
static void test_fill_extend()
{
  Doc d = make_doc();
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 60, 140, 60, 4); /* length 80 */
  present(d); /* the mask uses the canvas mapping of the last presented frame */
  project_grease_android_present_set_fill_extend(0.0f);
  CHECK(project_grease_android_present_gp_fill_mask(d.gpd, 1) == 1);
  read_back();
  CHECK(pixel_at_canvas(40, 60).r < 50);    /* nothing beyond the end without extension */
  project_grease_android_present_set_fill_extend(0.5f); /* +40 at each end */
  CHECK(project_grease_android_present_gp_fill_mask(d.gpd, 1) == 1);
  read_back();
  CHECK(pixel_at_canvas(30, 60).r > 200 && pixel_at_canvas(170, 60).r > 200);
  CHECK(pixel_at_canvas(15, 60).r < 50);    /* not past 60 - 40 */
  project_grease_android_present_set_fill_extend(0.0f);
}

/* Stroke / fill textures: a two-colour image along a textured stroke, the material colour back at
 * mix 1, and a solid image on a textured fill (gpencil_frag.glsl texture * (1 - mix) + colour * mix). */
static void test_material_textures()
{
  Doc d = make_doc();
  set_color(d, 0, 0, 0);
  MaterialGPencilStyle *st = d.gpd->mat[0]->gp_style;
  /* 2x1 image: left half red, right half blue */
  const unsigned char rb[8] = {255, 0, 0, 255, 0, 0, 255, 255};
  CHECK(project_grease_android_present_set_material_texture(0, 0, rb, 2, 1) == 1);
  st->stroke_style = GP_MATERIAL_STROKE_STYLE_TEXTURE;
  st->texture_pixsize = 100.0f;
  st->mix_stroke_factor = 0.0f;
  bGPDlayer *l = add_layer(d, "A");
  add_bar(l, 20, 180, 40, 20);
  present(d);
  int red = 0, blue = 0;
  for (int x = 30; x < 170; x += 2) {
    const Rgba c = pixel_at_canvas(float(x), 40);
    if (c.r > 150 && c.b < 100) red++;
    if (c.b > 150 && c.r < 100) blue++;
  }
  CHECK(red > 3 && blue > 3);               /* the image repeats along the stroke */
  st->mix_stroke_factor = 1.0f;             /* full mix: material colour only */
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 40), 0, 0, 0, 3));
  /* fill texture: closed square with a solid green image */
  const unsigned char g[4] = {0, 255, 0, 255};
  CHECK(project_grease_android_present_set_material_texture(0, 1, g, 1, 1) == 1);
  st->flag |= GP_MATERIAL_FILL_SHOW;
  st->fill_style = GP_MATERIAL_FILL_STYLE_TEXTURE;
  st->fill_rgba[3] = 1.0f; /* the texture alpha is multiplied by the fill colour alpha */
  st->mix_factor = 0.0f;
  st->texture_scale[0] = st->texture_scale[1] = 1.0f;
  bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
  bGPDstroke *sq = BKE_gpencil_stroke_add(f, 0, 4, 2, false);
  const float xy[4][2] = {{60, 70}, {140, 70}, {140, 110}, {60, 110}};
  for (int i = 0; i < 4; i++) { sq->points[i].x = xy[i][0]; sq->points[i].y = xy[i][1]; sq->points[i].pressure = 1; sq->points[i].strength = 1; }
  sq->flag |= GP_STROKE_CYCLIC;
  BKE_gpencil_stroke_geometry_update(d.gpd, sq);
  present(d);
  CHECK(near_rgb(pixel_at_canvas(100, 90), 0, 255, 0, 8));
  CHECK(project_grease_android_present_set_material_texture(0, 0, nullptr, 0, 0) == 1);
  CHECK(project_grease_android_present_set_material_texture(0, 1, nullptr, 0, 0) == 1);
}

static PGFxEntry fxe(int type, std::initializer_list<std::pair<int, float>> set = {}) {
  PGFxEntry e;
  pg_fx_entry_init(&e, type);
  for (auto &kv : set) e.params[kv.first] = kv.second;
  return e;
}

/* ---- shader effects through the presenter: the layer is drawn offscreen, the effects run, the result is composited */
static std::vector<PGFxEntry> g_provider_entries;
static const bGPDlayer *g_provider_layer = nullptr;
static int fx_provider(void *, const bGPDlayer *layer, const PGFxEntry **out) {
  if (layer != g_provider_layer || g_provider_entries.empty()) return 0;
  *out = g_provider_entries.data();
  return static_cast<int>(g_provider_entries.size());
}

static void test_fx_through_presenter() {
  Doc d = make_doc();
  set_color(d, 1, 0, 0);
  bGPDlayer *under = add_layer(d, "Under");
  add_bar(under, 20, 180, 100, 10, 1.0f, 1); /* a blue bar the effect layer must not disturb */
  bGPDlayer *fxl = add_layer(d, "FX");
  add_bar(fxl, 20, 80, 60, 20);              /* red bar in the left part of the canvas */
  g_provider_layer = fxl;
  project_grease_android_set_fx_provider(fx_provider, nullptr);

  g_provider_entries = {};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(50, 60), 255, 0, 0));
  CHECK(near_rgb(pixel_at_canvas(150, 60), 245, 245, 245));

  /* Flip horizontal mirrors the layer about the canvas center; layers below stay as they were */
  g_provider_entries = {fxe(PG_FX_FLIP)};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(150, 60), 255, 0, 0));
  CHECK(near_rgb(pixel_at_canvas(50, 60), 245, 245, 245));
  CHECK(near_rgb(pixel_at_canvas(50, 100), 0, 0, 255)); /* the unaffected layer under it is intact */
  CHECK(near_rgb(pixel_at_canvas(150, 100), 0, 0, 255));

  /* Colorize grayscale, factor 1: the red bar turns gray 0.2126 */
  g_provider_entries = {fxe(PG_FX_COLORIZE, {{PG_FXP_COLORIZE_FACTOR, 1.0f}})};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(50, 60), 54, 54, 54, 8));

  /* disabled / no working pass: the direct path, same pixels as without effects */
  PGFxEntry off = fxe(PG_FX_FLIP);
  off.enabled = 0;
  g_provider_entries = {off};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(50, 60), 255, 0, 0));
  g_provider_entries = {fxe(PG_FX_BLUR, {{PG_FXP_BLUR_RADIUS_X, 0}, {PG_FXP_BLUR_RADIUS_Y, 0}})};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(50, 60), 255, 0, 0));

  /* a shadow lands next to the bar and its pixels stay below the bar's own color */
  g_provider_entries = {fxe(PG_FX_SHADOW, {{PG_FXP_SHADOW_OFFSET_X, 0}, {PG_FXP_SHADOW_OFFSET_Y, -30},
                                           {PG_FXP_SHADOW_BLUR_X, 0}, {PG_FXP_SHADOW_BLUR_Y, 0}})};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(50, 60), 255, 0, 0));
  const Rgba sh = pixel_at_canvas(50, 90); /* 30 canvas px below (y up offset -30 = down on screen) */
  CHECK(sh.r < 120 && sh.g < 120 && sh.b < 120); /* a dark shadow over the light canvas */

  project_grease_android_set_fx_provider(nullptr, nullptr);
  g_provider_entries = {};
  g_provider_layer = nullptr;
}

/* ---- shader effects: the GLSL passes against the CPU port (project_grease_shader_fx.c) -------- */

struct Rect { int x, y, w, h; float r, g, b, a; }; /* premultiplied color, GL row order (y up) */
static const int FW = 64, FH = 48;
static const std::vector<Rect> &scene() {
  static const std::vector<Rect> rects = {
      {10, 12, 20, 18, 1.0f, 0.0f, 0.0f, 1.0f},      /* opaque red */
      {24, 20, 26, 20, 0.0f, 0.5f, 0.0f, 0.5f},      /* half transparent green over the red */
      {40, 5, 20, 10, 0.1f, 0.1f, 0.4f, 0.5f},       /* dim blue at 50 % */
      {4, 36, 8, 8, 0.9f, 0.9f, 0.9f, 1.0f},         /* bright square (glow, rim) */
  };
  return rects;
}
static unsigned char to_byte(float v) { return static_cast<unsigned char>(std::lround(std::fmin(1.0f, std::fmax(0.0f, v)) * 255.0f)); }

/* Runs `entries` through the GL engine and through the CPU port; returns the largest byte difference
 * over both planes (color and revealage). */
static int fx_max_diff(const std::vector<PGFxEntry> &entries, const char *name, std::vector<unsigned char> *gl_out = nullptr) {
  PGFxView view{FW, FH, 1.0f, 0.0f, 0.0f, FW, FH};
  /* GL: paint the scene into the layer buffer with scissored clears (exact premultiplied values) */
  CHECK(project_grease_fx_begin_layer(FW, FH) == 1);
  glEnable(GL_SCISSOR_TEST);
  for (const Rect &r : scene()) {
    glScissor(r.x, r.y, r.w, r.h);
    glClearColor(r.r, r.g, r.b, r.a);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glDisable(GL_SCISSOR_TEST);
  std::vector<unsigned char> col(FW * FH * 4), rev(FW * FH * 4);
  CHECK(project_grease_fx_end_layer(entries.data(), static_cast<int>(entries.size()), &view, col.data(), rev.data()) == 1);

  /* CPU: the same scene as floats (the layer buffer is 8 bit) */
  std::vector<float> premult(FW * FH * 4, 0.0f);
  for (const Rect &r : scene()) {
    for (int y = r.y; y < r.y + r.h; y++) {
      for (int x = r.x; x < r.x + r.w; x++) {
        float *p = &premult[(static_cast<size_t>(y) * FW + x) * 4];
        p[0] = to_byte(r.r) / 255.0f; p[1] = to_byte(r.g) / 255.0f; p[2] = to_byte(r.b) / 255.0f; p[3] = to_byte(r.a) / 255.0f;
      }
    }
  }
  PGFxImage im = pg_fx_image_new(FW, FH);
  pg_fx_image_from_premult(&im, premult.data());
  pg_fx_run_cpu(entries.data(), static_cast<int>(entries.size()), &view, &im);
  int worst = 0, wx = 0, wy = 0, wc = 0;
  for (int y = 0; y < FH; y++) {
    for (int x = 0; x < FW; x++) {
      for (int c = 0; c < 4; c++) {
        const size_t o = (static_cast<size_t>(y) * FW + x) * 4 + c;
        const int dc = std::abs(static_cast<int>(col[o]) - static_cast<int>(to_byte(im.color[o])));
        const int dr = std::abs(static_cast<int>(rev[o]) - static_cast<int>(to_byte(im.reveal[o])));
        if (std::max(dc, dr) > worst) { worst = std::max(dc, dr); wx = x; wy = y; wc = c; }
      }
    }
  }
  pg_fx_image_free(&im);
  if (gl_out) *gl_out = col;
  printf("  fx %-28s max diff %3d/255 at (%d,%d) ch %d\n", name, worst, wx, wy, wc);
  return worst;
}

/* Effect targets: a Flip aimed at fills only mirrors the fill of a closed stroke and leaves its
 * outline in place; aimed at strokes only it mirrors the outline and leaves the fill. */
static void test_fx_targets() {
  Doc d = make_doc();
  set_color(d, 1, 0, 0);
  MaterialGPencilStyle *st = d.gpd->mat[0]->gp_style;
  st->flag |= GP_MATERIAL_FILL_SHOW;
  st->fill_rgba[0] = 0; st->fill_rgba[1] = 1; st->fill_rgba[2] = 0; st->fill_rgba[3] = 1;
  bGPDlayer *l = add_layer(d, "FX");
  bGPDframe *f = static_cast<bGPDframe *>(l->frames.first);
  bGPDstroke *sq = BKE_gpencil_stroke_add(f, 0, 4, 4, false);
  const float xy[4][2] = {{20, 30}, {80, 30}, {80, 90}, {20, 90}}; /* left part of the canvas */
  for (int i = 0; i < 4; i++) { sq->points[i].x = xy[i][0]; sq->points[i].y = xy[i][1]; sq->points[i].pressure = 1; sq->points[i].strength = 1; }
  sq->flag |= GP_STROKE_CYCLIC;
  BKE_gpencil_stroke_geometry_update(d.gpd, sq);
  g_provider_layer = l;
  project_grease_android_set_fx_provider(fx_provider, nullptr);
  PGFxEntry flip = fxe(PG_FX_FLIP);
  flip.target = PG_FX_TARGET_FILLS;
  g_provider_entries = {flip};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(150, 60), 0, 255, 0, 8));  /* the fill moved right */
  CHECK(near_rgb(pixel_at_canvas(20, 60), 255, 0, 0, 8));   /* the outline stayed left */
  CHECK(near_rgb(pixel_at_canvas(50, 60), 245, 245, 245));  /* no fill left behind */
  flip.target = PG_FX_TARGET_STROKES;
  g_provider_entries = {flip};
  present(d);
  CHECK(near_rgb(pixel_at_canvas(50, 60), 0, 255, 0, 8));   /* the fill stayed left */
  CHECK(near_rgb(pixel_at_canvas(180, 60), 255, 0, 0, 8));  /* the outline moved right */
  g_provider_entries = {};
  project_grease_android_set_fx_provider(nullptr, nullptr);
}

static void test_shader_fx_gl() {
  const int tol = 12;
  std::vector<unsigned char> baseline;
  fx_max_diff({}, "no effects", &baseline);
  auto check = [&](const char *name, std::vector<PGFxEntry> e) {
    std::vector<unsigned char> out;
    const int d = fx_max_diff(e, name, &out);
    if (d > tol) printf("FAIL %s: GL and CPU differ by %d/255 (> %d)\n", name, d, tol);
    CHECK(d <= tol);
    /* the effect must actually change the layer (a shared identity bug would pass the comparison) */
    long changed = 0;
    for (size_t i = 0; i < out.size(); i++) changed += std::abs(int(out[i]) - int(baseline[i]));
    if (changed < 200) printf("FAIL %s: the effect changed almost nothing (%ld)\n", name, changed);
    CHECK(changed >= 200);
  };
  for (int mode = 0; mode < 5; mode++) {
    char n[40];
    std::snprintf(n, sizeof n, "colorize mode %d", mode);
    check(n, {fxe(PG_FX_COLORIZE, {{PG_FXP_COLORIZE_MODE, float(mode)}, {PG_FXP_COLORIZE_FACTOR, 0.6f},
                                   {PG_FXP_COLORIZE_LOW, 0.1f}, {PG_FXP_COLORIZE_LOW + 2, 0.9f}, {PG_FXP_COLORIZE_HIGH + 2, 0.2f}})});
  }
  check("blur", {fxe(PG_FX_BLUR, {{PG_FXP_BLUR_RADIUS_X, 6}, {PG_FXP_BLUR_RADIUS_Y, 4}, {PG_FXP_BLUR_SAMPLES, 4}, {PG_FXP_BLUR_ROTATION, 0.5f}})});
  check("flip horizontal", {fxe(PG_FX_FLIP)});
  check("flip both", {fxe(PG_FX_FLIP, {{PG_FXP_FLIP_VERTICAL, 1}})});
  check("wave horizontal", {fxe(PG_FX_WAVE, {{PG_FXP_WAVE_ORIENTATION, 0}, {PG_FXP_WAVE_AMPLITUDE, 3}, {PG_FXP_WAVE_PERIOD, 12}})});
  check("wave vertical", {fxe(PG_FX_WAVE, {{PG_FXP_WAVE_AMPLITUDE, 3}, {PG_FXP_WAVE_PERIOD, 12}, {PG_FXP_WAVE_PHASE, 1.0f}})});
  check("swirl", {fxe(PG_FX_SWIRL, {{PG_FXP_SWIRL_RADIUS, 22}, {PG_FXP_SWIRL_ANGLE, 2.0f}})});
  check("pixelate", {fxe(PG_FX_PIXEL, {{PG_FXP_PIXEL_SIZE_X, 6}, {PG_FXP_PIXEL_SIZE_Y, 4}})});
  check("pixelate nearest", {fxe(PG_FX_PIXEL, {{PG_FXP_PIXEL_SIZE_X, 6}, {PG_FXP_PIXEL_SIZE_Y, 4}, {PG_FXP_PIXEL_NEAREST, 1}})});
  check("shadow", {fxe(PG_FX_SHADOW, {{PG_FXP_SHADOW_OFFSET_X, 6}, {PG_FXP_SHADOW_OFFSET_Y, -5}, {PG_FXP_SHADOW_BLUR_X, 3}, {PG_FXP_SHADOW_BLUR_Y, 3}})});
  check("shadow wave rot scale", {fxe(PG_FX_SHADOW, {{PG_FXP_SHADOW_USE_WAVE, 1}, {PG_FXP_SHADOW_ROTATION, 0.3f},
                                                     {PG_FXP_SHADOW_SCALE_X, 1.2f}, {PG_FXP_SHADOW_SCALE_Y, 0.8f}, {PG_FXP_SHADOW_AMPLITUDE, 2}})});
  for (int mode = 0; mode < 6; mode++) {
    char n[40];
    std::snprintf(n, sizeof n, "rim mode %d", mode);
    check(n, {fxe(PG_FX_RIM, {{PG_FXP_RIM_MODE, float(mode)}, {PG_FXP_RIM_OFFSET_X, 4}, {PG_FXP_RIM_OFFSET_Y, -3},
                              {PG_FXP_RIM_BLUR_X, 2}, {PG_FXP_RIM_BLUR_Y, 2}})});
  }
  check("glow luminance", {fxe(PG_FX_GLOW, {{PG_FXP_GLOW_BLUR_X, 5}, {PG_FXP_GLOW_BLUR_Y, 5}, {PG_FXP_GLOW_SAMPLES, 4}, {PG_FXP_GLOW_THRESHOLD, 0.3f}})});
  check("glow color", {fxe(PG_FX_GLOW, {{PG_FXP_GLOW_MODE, 1}, {PG_FXP_GLOW_SELECT, 1.0f}, {PG_FXP_GLOW_SELECT + 1, 0.0f},
                                         {PG_FXP_GLOW_THRESHOLD, 0.3f}, {PG_FXP_GLOW_BLUR_X, 4}, {PG_FXP_GLOW_BLUR_Y, 4}, {PG_FXP_GLOW_SAMPLES, 4}})});
  check("glow under", {fxe(PG_FX_GLOW, {{PG_FXP_GLOW_USE_ALPHA, 1}, {PG_FXP_GLOW_BLUR_X, 5}, {PG_FXP_GLOW_BLUR_Y, 5}, {PG_FXP_GLOW_SAMPLES, 4}})});
  for (int blend : {2, 3, 4, 5}) {
    char n[40];
    std::snprintf(n, sizeof n, "glow blend %d", blend);
    check(n, {fxe(PG_FX_GLOW, {{PG_FXP_GLOW_BLEND, float(blend)}, {PG_FXP_GLOW_BLUR_X, 4}, {PG_FXP_GLOW_BLUR_Y, 4}, {PG_FXP_GLOW_SAMPLES, 3}})});
  }
  check("chain blur+colorize+flip", {fxe(PG_FX_BLUR, {{PG_FXP_BLUR_RADIUS_X, 4}, {PG_FXP_BLUR_RADIUS_Y, 4}, {PG_FXP_BLUR_SAMPLES, 3}}),
                                     fxe(PG_FX_COLORIZE, {{PG_FXP_COLORIZE_MODE, 1}, {PG_FXP_COLORIZE_FACTOR, 1.0f}}), fxe(PG_FX_FLIP)});
  check("chain shadow+glow", {fxe(PG_FX_SHADOW, {{PG_FXP_SHADOW_BLUR_X, 2}, {PG_FXP_SHADOW_BLUR_Y, 2}}),
                              fxe(PG_FX_GLOW, {{PG_FXP_GLOW_BLUR_X, 3}, {PG_FXP_GLOW_BLUR_Y, 3}, {PG_FXP_GLOW_SAMPLES, 3}})});
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
  test_thick_bend();
  test_export_transparent();
  test_fill_mask_modes();
  test_fill_extend();
  test_material_textures();
  test_onion();
  test_onion_modes_and_colors();
  test_selection_overlay();
  test_masks();
  test_weight_view();
  test_strength_full();
  test_self_overlap();
  test_cyclic_square();
  test_weight_view_smooth();
  test_weight_view_masked();
  test_shader_fx_gl();
  test_fx_through_presenter();
  test_fx_targets();
  project_grease_android_present_reset();
  if (failures) {
    printf("%d FAILURES\n", failures);
    return 1;
  }
  printf("render tests passed\n");
  return 0;
}
