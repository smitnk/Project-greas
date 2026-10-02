/* Host tests for the shader-effect data model, the pass construction (gpencil_shader_fx.c) and the
 * CPU port of gpencil_vfx_frag.glsl (project_grease_shader_fx.c). The expected numbers are worked
 * out by hand from the pinned Blender GLSL / C, not taken from the implementation. The GL shaders
 * are compared with this executor in test_render.cc. See tools/run_native_shader_fx_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "project_grease_shader_fx.h"

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)
static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

static PGFxEntry entry(int type) {
  PGFxEntry e;
  pg_fx_entry_init(&e, type);
  return e;
}
static PGFxView view(int w, int h, float scale = 1.0f) {
  PGFxView v;
  v.width = w; v.height = h; v.scale = scale; v.origin_x = 0; v.origin_y = 0; v.canvas_w = w; v.canvas_h = h;
  return v;
}

static void test_type_info() {
  const int types[] = {PG_FX_BLUR, PG_FX_FLIP, PG_FX_PIXEL, PG_FX_SWIRL, PG_FX_WAVE, PG_FX_RIM,
                       PG_FX_COLORIZE, PG_FX_SHADOW, PG_FX_GLOW};
  for (int t : types) {
    CHECK(pg_fx_valid_type(t));
    CHECK(pg_fx_param_count(t) > 0 && pg_fx_param_count(t) <= PG_FX_MAX_PARAMS);
    CHECK(pg_fx_name(t)[0] != 0);
  }
  CHECK(!pg_fx_valid_type(0) && !pg_fx_valid_type(3) && !pg_fx_valid_type(11) && !pg_fx_valid_type(-1));
  /* Blender's initData() defaults */
  float p[PG_FX_MAX_PARAMS];
  pg_fx_defaults(PG_FX_BLUR, p);
  CHECK(p[PG_FXP_BLUR_RADIUS_X] == 50 && p[PG_FXP_BLUR_RADIUS_Y] == 50 && p[PG_FXP_BLUR_SAMPLES] == 8);
  pg_fx_defaults(PG_FX_SHADOW, p);
  CHECK(p[PG_FXP_SHADOW_OFFSET_X] == 15 && p[PG_FXP_SHADOW_OFFSET_Y] == 20 && near(p[PG_FXP_SHADOW_COLOR + 3], 0.8f));
  CHECK(p[PG_FXP_SHADOW_BLUR_X] == 5 && p[PG_FXP_SHADOW_SAMPLES] == 2 && p[PG_FXP_SHADOW_SCALE_X] == 1);
  pg_fx_defaults(PG_FX_RIM, p);
  CHECK(p[PG_FXP_RIM_OFFSET_X] == 50 && p[PG_FXP_RIM_OFFSET_Y] == -100 && p[PG_FXP_RIM_MODE] == 1);
  pg_fx_defaults(PG_FX_GLOW, p);
  CHECK(near(p[PG_FXP_GLOW_THRESHOLD], 0.1f) && p[PG_FXP_GLOW_COLOR] == 0.75f && p[PG_FXP_GLOW_SAMPLES] == 8);
  pg_fx_defaults(PG_FX_COLORIZE, p);
  CHECK(near(p[PG_FXP_COLORIZE_FACTOR], 0.5f) && p[PG_FXP_COLORIZE_MODE] == 0);
  pg_fx_defaults(PG_FX_FLIP, p);
  CHECK(p[PG_FXP_FLIP_HORIZONTAL] == 1 && p[PG_FXP_FLIP_VERTICAL] == 0);
  /* sanitising */
  PGFxEntry e = entry(PG_FX_SHADOW);
  e.params[PG_FXP_SHADOW_SCALE_X] = 0.0f;       /* would divide by zero */
  e.params[PG_FXP_SHADOW_BLUR_X] = NAN;
  e.params[PG_FXP_SHADOW_SAMPLES] = 9999;
  e.params[PG_FXP_SHADOW_ORIENTATION] = 7;
  pg_fx_sanitize(PG_FX_SHADOW, e.params);
  CHECK(e.params[PG_FXP_SHADOW_SCALE_X] >= 0.01f && std::isfinite(e.params[PG_FXP_SHADOW_BLUR_X]));
  CHECK(e.params[PG_FXP_SHADOW_SAMPLES] == 32 && e.params[PG_FXP_SHADOW_ORIENTATION] == 1);
  PGFxEntry c = entry(PG_FX_COLORIZE);
  c.params[PG_FXP_COLORIZE_MODE] = 99;
  c.params[PG_FXP_COLORIZE_FACTOR] = 5;
  pg_fx_sanitize(PG_FX_COLORIZE, c.params);
  CHECK(c.params[PG_FXP_COLORIZE_MODE] == 4 && c.params[PG_FXP_COLORIZE_FACTOR] == 1);
}

static void test_pass_construction() {
  PGFxPass ps[PG_FX_MAX_PASSES];
  const PGFxView v = view(64, 48);
  /* Blur: H then V, offsets (r cos, r sin) and (-r sin, r cos), samples min(8, radius) */
  PGFxEntry blur = entry(PG_FX_BLUR);
  CHECK(pg_fx_build_passes(&blur, &v, ps) == 2);
  CHECK(ps[0].kind == PGFX_PASS_BLUR && near(ps[0].offset[0], 50) && near(ps[0].offset[1], 0) && ps[0].samp_count == 8);
  CHECK(near(ps[1].offset[0], 0) && near(ps[1].offset[1], 50));
  blur.params[PG_FXP_BLUR_ROTATION] = 1.5707964f;
  pg_fx_build_passes(&blur, &v, ps);
  CHECK(near(ps[0].offset[0], 0, 1e-3f) && near(ps[0].offset[1], 50, 1e-3f));
  blur.params[PG_FXP_BLUR_ROTATION] = 0;
  blur.params[PG_FXP_BLUR_RADIUS_X] = 3; /* samples = min(8, 3) */
  const PGFxView v2 = view(64, 48, 2.0f); /* sizes are canvas pixels: x scale */
  pg_fx_build_passes(&blur, &v2, ps);
  CHECK(near(ps[0].offset[0], 6) && ps[0].samp_count == 6 && near(ps[1].offset[1], 100) && ps[1].samp_count == 8);
  blur.params[PG_FXP_BLUR_RADIUS_Y] = 0;
  CHECK(pg_fx_build_passes(&blur, &v, ps) == 1);
  blur.params[PG_FXP_BLUR_RADIUS_X] = 0;
  CHECK(pg_fx_build_passes(&blur, &v, ps) == 0);
  PGFxEntry off = entry(PG_FX_BLUR);
  off.enabled = 0;
  CHECK(pg_fx_build_passes(&off, &v, ps) == 0);
  off.enabled = 1;
  off.params[PG_FXP_BLUR_SAMPLES] = 0;
  CHECK(pg_fx_build_passes(&off, &v, ps) == 0);

  /* Pixelate: the pixel size in uv, samples 2 above 3 px, none for nearest, nothing at 1 px */
  PGFxEntry pix = entry(PG_FX_PIXEL);
  pix.params[PG_FXP_PIXEL_SIZE_X] = 4;
  pix.params[PG_FXP_PIXEL_SIZE_Y] = 2;
  CHECK(pg_fx_build_passes(&pix, &v, ps) == 2);
  CHECK(near(ps[0].target_pixel_size[0], 4.0f / 64) && near(ps[0].target_pixel_size[1], 1.0f / 48) && ps[0].samp_count == 2);
  CHECK(near(ps[1].target_pixel_size[0], 1.0f / 64) && near(ps[1].target_pixel_size[1], 2.0f / 48) && ps[1].samp_count == 1);
  /* the grid is anchored at the canvas center, shifted half a texel */
  CHECK(near(ps[0].target_pixel_offset[0], 0.5f - 0.5f * 4.0f / 64) && near(ps[0].target_pixel_offset[1], 0.5f - 0.5f * 2.0f / 48));
  pix.params[PG_FXP_PIXEL_NEAREST] = 1;
  pg_fx_build_passes(&pix, &v, ps);
  CHECK(ps[0].samp_count == 0 && ps[1].samp_count == 0);
  pix.params[PG_FXP_PIXEL_SIZE_X] = 1;
  pix.params[PG_FXP_PIXEL_SIZE_Y] = 1;
  CHECK(pg_fx_build_passes(&pix, &v, ps) == 0);

  /* Wave (horizontal): dir (1,0) / (period * S) * viewport, offset (0,-1) * amplitude * S / viewport */
  PGFxEntry wave = entry(PG_FX_WAVE);
  wave.params[PG_FXP_WAVE_ORIENTATION] = 0;
  wave.params[PG_FXP_WAVE_PERIOD] = 16;
  wave.params[PG_FXP_WAVE_AMPLITUDE] = 6;
  wave.params[PG_FXP_WAVE_PHASE] = 0.25f;
  CHECK(pg_fx_build_passes(&wave, &v, ps) == 1);
  CHECK(ps[0].kind == PGFX_PASS_TRANSFORM && near(ps[0].wave_dir[0], 64.0f / 16) && near(ps[0].wave_dir[1], 0));
  CHECK(near(ps[0].wave_offset[0], 0) && near(ps[0].wave_offset[1], -6.0f / 48));
  CHECK(near(ps[0].wave_phase, 0.25f - 0.5f * 4.0f)); /* phase starts at the canvas center */

  /* Flip and Swirl */
  PGFxEntry flip = entry(PG_FX_FLIP);
  flip.params[PG_FXP_FLIP_VERTICAL] = 1;
  pg_fx_build_passes(&flip, &v, ps);
  CHECK(ps[0].axis_flip[0] == -1 && ps[0].axis_flip[1] == -1 && ps[0].swirl_radius == 0);
  PGFxEntry swirl = entry(PG_FX_SWIRL);
  swirl.params[PG_FXP_SWIRL_RADIUS] = 20;
  swirl.params[PG_FXP_SWIRL_CENTER_X] = 0.25f;
  swirl.params[PG_FXP_SWIRL_CENTER_Y] = 0.25f; /* a quarter down from the top: y up in uv pixels */
  CHECK(pg_fx_build_passes(&swirl, &v, ps) == 1);
  CHECK(near(ps[0].swirl_center[0], 16) && near(ps[0].swirl_center[1], 48 - 12) && near(ps[0].swirl_radius, 20));
  swirl.params[PG_FXP_SWIRL_RADIUS] = 0;
  CHECK(pg_fx_build_passes(&swirl, &v, ps) == 0);

  /* Colorize */
  PGFxEntry col = entry(PG_FX_COLORIZE);
  col.params[PG_FXP_COLORIZE_MODE] = 2;
  CHECK(pg_fx_build_passes(&col, &v, ps) == 1 && ps[0].mode == 2 && near(ps[0].factor, 0.5f) && near(ps[0].high_color[0], 1));

  /* Shadow: first pass copies, second blends premultiplied onto the start buffer */
  PGFxEntry sh = entry(PG_FX_SHADOW);
  CHECK(pg_fx_build_passes(&sh, &v, ps) == 2);
  CHECK(ps[0].first_pass == 1 && ps[0].blend == PGFX_BLEND_NONE && ps[1].first_pass == 0);
  CHECK(ps[1].blend == PGFX_BLEND_PREMUL && ps[1].dst == PGFX_DST_START && near(ps[1].shadow_color[3], 0.8f));
  /* offset (15, 20) px in uv; with scale 1 and no rotation the uv transform is a translation */
  CHECK(near(ps[0].uv_rot_x[0], 1) && near(ps[0].uv_rot_x[1], 0) && near(ps[0].uv_rot_y[0], 0) && near(ps[0].uv_rot_y[1], 1));
  CHECK(near(ps[0].uv_offset[0], -15.0f / 64, 1e-4f) && near(ps[0].uv_offset[1], -20.0f / 48, 1e-4f));
  CHECK(near(ps[1].uv_offset[0], 0) && near(ps[1].uv_offset[1], 0));
  CHECK(near(ps[0].wave_dir[0], 0) && near(ps[0].wave_offset[0], 0)); /* wave off by default */
  sh.params[PG_FXP_SHADOW_USE_WAVE] = 1;
  pg_fx_build_passes(&sh, &v, ps);
  CHECK(ps[0].wave_offset[0] != 0 || ps[0].wave_offset[1] != 0);
  CHECK(near(ps[1].wave_offset[0], 0) && near(ps[1].wave_offset[1], 0)); /* the V pass has no wave offset */

  /* Rim: modes pick the blend state; overlay adds a second additive blend of the same sources */
  PGFxEntry rim = entry(PG_FX_RIM);
  rim.params[PG_FXP_RIM_MODE] = 1;
  CHECK(pg_fx_build_passes(&rim, &v, ps) == 3);
  CHECK(ps[1].blend == PGFX_BLEND_MUL && ps[1].blend_mode == 1 && ps[2].blend == PGFX_BLEND_ADD && ps[2].blend_mode == 999);
  CHECK(ps[2].src == PGFX_SRC_PREV_SRC && ps[2].dst == PGFX_DST_PREV);
  CHECK(near(ps[0].uv_offset[0], 50.0f / 64) && near(ps[0].uv_offset[1], -100.0f / 48) && ps[0].samp_count == 1);
  rim.params[PG_FXP_RIM_MODE] = 2;
  CHECK(pg_fx_build_passes(&rim, &v, ps) == 2 && ps[1].blend == PGFX_BLEND_ADD);
  rim.params[PG_FXP_RIM_MODE] = 3;
  pg_fx_build_passes(&rim, &v, ps);
  CHECK(ps[1].blend == PGFX_BLEND_SUB);
  rim.params[PG_FXP_RIM_MODE] = 0;
  pg_fx_build_passes(&rim, &v, ps);
  CHECK(ps[1].blend == PGFX_BLEND_PREMUL);

  /* Glow: luminance packs only the threshold, color packs the selected rgb and the threshold */
  PGFxEntry glow = entry(PG_FX_GLOW);
  CHECK(pg_fx_build_passes(&glow, &v, ps) == 2);
  CHECK(near(ps[0].threshold[0], 0.1f) && ps[0].threshold[1] == -1 && ps[0].samp_count == 8);
  CHECK(ps[1].blend == PGFX_BLEND_PREMUL && ps[1].threshold[0] == -1 && near(ps[1].glow_color[3], 1));
  glow.params[PG_FXP_GLOW_MODE] = 1;
  glow.params[PG_FXP_GLOW_SELECT] = 0.5f;
  pg_fx_build_passes(&glow, &v, ps);
  CHECK(near(ps[0].threshold[0], 0.5f) && near(ps[0].threshold[3], 0.1f));
  glow.params[PG_FXP_GLOW_BLEND] = 2;
  pg_fx_build_passes(&glow, &v, ps);
  CHECK(ps[1].blend == PGFX_BLEND_ADD);
  glow.params[PG_FXP_GLOW_BLEND] = 4;
  pg_fx_build_passes(&glow, &v, ps);
  CHECK(ps[1].blend == PGFX_BLEND_MUL);
}

/* an image of w x h with one premultiplied pixel set */
static PGFxImage image_with(int w, int h, std::vector<float> &premult) {
  premult.assign(static_cast<size_t>(w) * h * 4, 0.0f);
  PGFxImage im = pg_fx_image_new(w, h);
  pg_fx_image_from_premult(&im, premult.data());
  return im;
}
static void set_px(PGFxImage &im, int x, int y, float r, float g, float b, float a) {
  const size_t o = (static_cast<size_t>(y) * im.width + x) * 4;
  im.color[o] = r; im.color[o + 1] = g; im.color[o + 2] = b; im.color[o + 3] = a;
  im.reveal[o] = im.reveal[o + 1] = im.reveal[o + 2] = 1.0f - a; im.reveal[o + 3] = 1.0f;
}
static const float *C(const PGFxImage &im, int x, int y) { return &im.color[(static_cast<size_t>(y) * im.width + x) * 4]; }
static const float *R(const PGFxImage &im, int x, int y) { return &im.reveal[(static_cast<size_t>(y) * im.width + x) * 4]; }

static void test_colorize_math() {
  const PGFxView v = view(4, 4);
  std::vector<float> buf;
  /* a red opaque pixel: C = (1,0,0,1), R = 0 */
  auto run = [&](int mode, float factor, float low[3], float high[3]) {
    PGFxImage im = image_with(4, 4, buf);
    set_px(im, 1, 1, 1, 0, 0, 1);
    PGFxEntry e = entry(PG_FX_COLORIZE);
    e.params[PG_FXP_COLORIZE_MODE] = float(mode);
    e.params[PG_FXP_COLORIZE_FACTOR] = factor;
    for (int i = 0; i < 3; i++) { e.params[PG_FXP_COLORIZE_LOW + i] = low[i]; e.params[PG_FXP_COLORIZE_HIGH + i] = high[i]; }
    pg_fx_run_cpu(&e, 1, &v, &im);
    PGFxImage out = im;
    return out;
  };
  float lo[3] = {0, 0, 1}, hi[3] = {1, 1, 0};
  { /* grayscale, factor 1: luma = 0.2126 (Blender's dot uses 0.2126, 0.7152, 0.723) */
    PGFxImage o = run(0, 1.0f, lo, hi);
    CHECK(near(C(o, 1, 1)[0], 0.2126f) && near(C(o, 1, 1)[1], 0.2126f) && near(C(o, 1, 1)[2], 0.2126f));
    CHECK(near(R(o, 1, 1)[0], 0.0f) && near(C(o, 1, 1)[3], 1.0f));
    pg_fx_image_free(&o);
  }
  { /* the blue weight of Blender's luma dot is 0.723 (sic), so a blue pixel turns 0.723 gray */
    PGFxImage im = image_with(4, 4, buf);
    set_px(im, 2, 2, 0, 0, 1, 1);
    PGFxEntry e = entry(PG_FX_COLORIZE);
    e.params[PG_FXP_COLORIZE_FACTOR] = 1.0f;
    pg_fx_run_cpu(&e, 1, &v, &im);
    CHECK(near(C(im, 2, 2)[0], 0.723f) && near(C(im, 2, 2)[2], 0.723f));
    pg_fx_image_free(&im);
  }
  { /* grayscale, factor 0.5: halfway between red and its luma */
    PGFxImage o = run(0, 0.5f, lo, hi);
    CHECK(near(C(o, 1, 1)[0], 0.5f * (1 + 0.2126f)) && near(C(o, 1, 1)[1], 0.5f * 0.2126f));
    pg_fx_image_free(&o);
  }
  { /* sepia, factor 1: sepia_mat * (1,0,0) = first column (0.393, 0.349, 0.272) */
    PGFxImage o = run(1, 1.0f, lo, hi);
    CHECK(near(C(o, 1, 1)[0], 0.393f) && near(C(o, 1, 1)[1], 0.349f) && near(C(o, 1, 1)[2], 0.272f));
    pg_fx_image_free(&o);
  }
  { /* duotone: luma (0.2126) <= factor (0.5): luma * low */
    PGFxImage o = run(2, 0.5f, lo, hi);
    CHECK(near(C(o, 1, 1)[0], 0.0f) && near(C(o, 1, 1)[2], 0.2126f));
    PGFxImage o2 = run(2, 0.1f, lo, hi); /* luma > factor: luma * high */
    CHECK(near(C(o2, 1, 1)[0], 0.2126f) && near(C(o2, 1, 1)[1], 0.2126f) && near(C(o2, 1, 1)[2], 0.0f));
    pg_fx_image_free(&o); pg_fx_image_free(&o2);
  }
  { /* custom: mix(rgb, luma * low, factor) */
    PGFxImage o = run(3, 1.0f, lo, hi);
    CHECK(near(C(o, 1, 1)[0], 0.0f) && near(C(o, 1, 1)[2], 0.2126f));
    pg_fx_image_free(&o);
  }
  { /* transparent: color * factor, revealage = mix(1, R, factor) = 0.5 for an opaque pixel */
    PGFxImage o = run(4, 0.5f, lo, hi);
    CHECK(near(C(o, 1, 1)[0], 0.5f) && near(R(o, 1, 1)[0], 0.5f));
    /* composite onto white: 1 * 0.5 + 0.5 */
    float frame[4 * 4 * 3];
    for (float &f : frame) f = 1.0f;
    pg_fx_composite_cpu(&o, frame);
    const float *px = &frame[(1 * 4 + 1) * 3];
    CHECK(near(px[0], 1.0f) && near(px[1], 0.5f) && near(px[2], 0.5f)); /* white * R + C */
    pg_fx_image_free(&o);
  }
}

static void test_transform_math() {
  const PGFxView v = view(8, 4);
  std::vector<float> buf;
  { /* flip horizontal: x -> 7 - x, rows unchanged */
    PGFxImage im = image_with(8, 4, buf);
    set_px(im, 2, 1, 1, 0, 0, 1);
    PGFxEntry e = entry(PG_FX_FLIP);
    CHECK(pg_fx_run_cpu(&e, 1, &v, &im) == 1);
    CHECK(near(C(im, 5, 1)[0], 1.0f) && near(C(im, 2, 1)[0], 0.0f) && near(R(im, 5, 1)[0], 0.0f) && near(R(im, 2, 1)[0], 1.0f));
    pg_fx_image_free(&im);
  }
  { /* flip vertical */
    PGFxImage im = image_with(8, 4, buf);
    set_px(im, 2, 1, 0, 1, 0, 1);
    PGFxEntry e = entry(PG_FX_FLIP);
    e.params[PG_FXP_FLIP_HORIZONTAL] = 0;
    e.params[PG_FXP_FLIP_VERTICAL] = 1;
    pg_fx_run_cpu(&e, 1, &v, &im);
    CHECK(near(C(im, 2, 2)[1], 1.0f) && near(C(im, 2, 1)[1], 0.0f));
    pg_fx_image_free(&im);
  }
  { /* wave with zero amplitude and swirl with zero angle are the identity */
    PGFxImage im = image_with(8, 4, buf);
    set_px(im, 3, 2, 0.5f, 0.25f, 1, 1);
    PGFxEntry w = entry(PG_FX_WAVE);
    w.params[PG_FXP_WAVE_AMPLITUDE] = 0;
    PGFxEntry s = entry(PG_FX_SWIRL);
    s.params[PG_FXP_SWIRL_ANGLE] = 0;
    PGFxEntry both[2] = {w, s};
    CHECK(pg_fx_run_cpu(both, 2, &v, &im) == 2);
    CHECK(near(C(im, 3, 2)[0], 0.5f) && near(C(im, 3, 2)[1], 0.25f) && near(R(im, 3, 2)[0], 0.0f));
    pg_fx_image_free(&im);
  }
  { /* wave: a vertical pattern is shifted along y by sin(wave_time + phase) * amplitude (uv) */
    const PGFxView vv = view(16, 16);
    PGFxImage im = image_with(16, 16, buf);
    for (int y = 0; y < 16; y++) set_px(im, 8, y, 1, 1, 1, 1); /* a vertical line at x = 8 */
    PGFxEntry w = entry(PG_FX_WAVE);
    w.params[PG_FXP_WAVE_ORIENTATION] = 0; /* horizontal wave: displaces y by sin(x ...) */
    w.params[PG_FXP_WAVE_AMPLITUDE] = 4;
    w.params[PG_FXP_WAVE_PERIOD] = 8;
    w.params[PG_FXP_WAVE_PHASE] = 0;
    pg_fx_run_cpu(&w, 1, &vv, &im);
    /* a line parallel to the displacement direction stays a line: x = 8 still carries the color */
    CHECK(C(im, 8, 8)[0] > 0.5f);
    pg_fx_image_free(&im);
  }
}

static void test_blur_pixelate() {
  std::vector<float> buf;
  { /* blur: a single bright pixel spreads symmetrically, normalised weights never exceed the peak */
    const PGFxView v = view(15, 15);
    PGFxImage im = image_with(15, 15, buf);
    set_px(im, 7, 7, 1, 1, 1, 1);
    PGFxEntry b = entry(PG_FX_BLUR);
    b.params[PG_FXP_BLUR_RADIUS_X] = 4;
    b.params[PG_FXP_BLUR_RADIUS_Y] = 4;
    b.params[PG_FXP_BLUR_SAMPLES] = 4;
    CHECK(pg_fx_run_cpu(&b, 1, &v, &im) == 2);
    CHECK(near(C(im, 5, 7)[0], C(im, 9, 7)[0], 1e-4f) && near(C(im, 7, 5)[0], C(im, 7, 9)[0], 1e-4f));
    CHECK(C(im, 7, 7)[0] > C(im, 6, 7)[0] && C(im, 6, 7)[0] > 0.0f && C(im, 7, 7)[0] < 1.0f);
    pg_fx_image_free(&im);
  }
  { /* pixelate (nearest): every 4x4 cell takes one color; the grid is anchored at the canvas center */
    const PGFxView v = view(16, 16);
    PGFxImage im = image_with(16, 16, buf);
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) set_px(im, x, y, (x % 8) / 8.0f, 0, 0, 1);
    PGFxEntry p = entry(PG_FX_PIXEL);
    p.params[PG_FXP_PIXEL_SIZE_X] = 4;
    p.params[PG_FXP_PIXEL_SIZE_Y] = 4;
    p.params[PG_FXP_PIXEL_NEAREST] = 1;
    CHECK(pg_fx_run_cpu(&p, 1, &v, &im) == 2);
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        /* Blender's grid: the canvas center (8) is the CENTER of a cell, so the cells are [2,6), [6,10)... */
        const int cell = static_cast<int>(std::floor((x - 2) / 4.0f)) * 4 + 2;
        const int first = cell < 0 ? 0 : cell;
        CHECK(near(C(im, x, y)[0], C(im, first, y)[0], 1e-4f));
      }
    }
    pg_fx_image_free(&im);
  }
}

static void test_shadow_math() {
  const PGFxView v = view(16, 16);
  std::vector<float> buf;
  PGFxImage im = image_with(16, 16, buf);
  for (int y = 4; y < 8; y++) for (int x = 4; x < 8; x++) set_px(im, x, y, 1, 0, 0, 1); /* a red opaque square */
  PGFxEntry sh = entry(PG_FX_SHADOW);
  sh.params[PG_FXP_SHADOW_OFFSET_X] = 4;   /* shadow 4 px to the right */
  sh.params[PG_FXP_SHADOW_OFFSET_Y] = 0;
  sh.params[PG_FXP_SHADOW_BLUR_X] = 0;
  sh.params[PG_FXP_SHADOW_BLUR_Y] = 0;
  sh.params[PG_FXP_SHADOW_COLOR] = 0.0f; sh.params[PG_FXP_SHADOW_COLOR + 1] = 0.0f;
  sh.params[PG_FXP_SHADOW_COLOR + 2] = 1.0f; sh.params[PG_FXP_SHADOW_COLOR + 3] = 0.8f; /* blue, 80 % */
  CHECK(pg_fx_run_cpu(&sh, 1, &v, &im) == 2);
  /* original square untouched */
  CHECK(near(C(im, 5, 5)[0], 1.0f) && near(R(im, 5, 5)[0], 0.0f));
  /* the shadow lands where the layer is transparent: color = shadowRGB * 0.8, revealage 1 - 0.8 */
  CHECK(near(C(im, 9, 5)[2], 0.8f, 1e-3f) && near(C(im, 9, 5)[0], 0.0f, 1e-3f));
  CHECK(near(R(im, 9, 5)[0], 0.2f, 1e-3f));
  /* away from both: nothing */
  CHECK(near(C(im, 13, 13)[2], 0.0f) && near(R(im, 13, 13)[0], 1.0f));
  /* where the square itself is: alpha-under, the shadow does not paint over it */
  float frame[16 * 16 * 3];
  for (float &f : frame) f = 1.0f;
  pg_fx_composite_cpu(&im, frame);
  const float *over = &frame[(5 * 16 + 5) * 3];
  CHECK(near(over[0], 1.0f) && near(over[1], 0.0f) && near(over[2], 0.0f));
  pg_fx_image_free(&im);
}

static void test_glow_rim_math() {
  std::vector<float> buf;
  { /* glow with an impossible luminance threshold adds nothing but the (empty) glow */
    const PGFxView v = view(16, 16);
    PGFxImage im = image_with(16, 16, buf);
    for (int y = 6; y < 10; y++) for (int x = 6; x < 10; x++) set_px(im, x, y, 0.1f, 0.1f, 0.1f, 1);
    PGFxEntry g = entry(PG_FX_GLOW);
    g.params[PG_FXP_GLOW_THRESHOLD] = 0.9f; /* the square is darker: nothing glows */
    g.params[PG_FXP_GLOW_BLUR_X] = 3; g.params[PG_FXP_GLOW_BLUR_Y] = 3;
    PGFxImage ref = pg_fx_image_new(16, 16);
    memcpy(ref.color, im.color, sizeof(float) * 16 * 16 * 4);
    memcpy(ref.reveal, im.reveal, sizeof(float) * 16 * 16 * 4);
    CHECK(pg_fx_run_cpu(&g, 1, &v, &im) == 2);
    CHECK(near(C(im, 7, 7)[0], C(ref, 7, 7)[0], 0.02f) && near(R(im, 2, 2)[0], 1.0f, 0.02f));
    pg_fx_image_free(&im); pg_fx_image_free(&ref);
  }
  { /* glow of a bright square leaks light around it (alpha over), the square stays */
    const PGFxView v = view(16, 16);
    PGFxImage im = image_with(16, 16, buf);
    for (int y = 6; y < 10; y++) for (int x = 6; x < 10; x++) set_px(im, x, y, 1, 1, 1, 1);
    PGFxEntry g = entry(PG_FX_GLOW);
    g.params[PG_FXP_GLOW_THRESHOLD] = 0.1f;
    g.params[PG_FXP_GLOW_BLUR_X] = 3; g.params[PG_FXP_GLOW_BLUR_Y] = 3;
    g.params[PG_FXP_GLOW_SAMPLES] = 3;
    pg_fx_run_cpu(&g, 1, &v, &im);
    CHECK(C(im, 5, 8)[0] > 0.0f && R(im, 5, 8)[0] < 1.0f); /* glow next to the square */
    CHECK(C(im, 12, 8)[0] < C(im, 5, 8)[0] + 1e-4f);       /* fades with distance */
    pg_fx_image_free(&im);
  }
  { /* rim, add mode, no blur: the layer's own mask (foreground alpha) keeps the rim off the object */
    const PGFxView v = view(16, 16);
    PGFxImage im = image_with(16, 16, buf);
    for (int y = 6; y < 10; y++) for (int x = 6; x < 10; x++) set_px(im, x, y, 0.5f, 0.5f, 0.5f, 1);
    PGFxEntry r = entry(PG_FX_RIM);
    r.params[PG_FXP_RIM_MODE] = 2; /* add */
    r.params[PG_FXP_RIM_OFFSET_X] = 2; r.params[PG_FXP_RIM_OFFSET_Y] = 0;
    r.params[PG_FXP_RIM_BLUR_X] = 0; r.params[PG_FXP_RIM_BLUR_Y] = 0;
    r.params[PG_FXP_RIM_COLOR] = 1; r.params[PG_FXP_RIM_COLOR + 1] = 0; r.params[PG_FXP_RIM_COLOR + 2] = 0;
    CHECK(pg_fx_run_cpu(&r, 1, &v, &im) == 2);
    /* inside the square the foreground mask is 1 - 0.5 (mean color) = 0.5: some red is added there,
     * outside it (mask 1) the shifted silhouette is empty or full as in Blender's formula */
    CHECK(C(im, 8, 8)[0] >= 0.5f - 1e-3f);
    pg_fx_image_free(&im);
  }
}

int main() {
  test_type_info();
  test_pass_construction();
  test_colorize_math();
  test_transform_math();
  test_blur_pixelate();
  test_shadow_math();
  test_glow_rim_math();
  if (failures) {
    printf("%d FAILURES\n", failures);
    return 1;
  }
  printf("shader fx tests passed\n");
  return 0;
}
