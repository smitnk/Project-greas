/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Shader effects for Project Grease: data model, pass construction and the CPU executor. See
 * project_grease_shader_fx.h for the design and the adaptations. The numeric setup in
 * pg_fx_build_passes() is the arithmetic of gpencil_vfx_*() in draw/engines/gpencil/gpencil_shader_fx.c
 * with the DRW calls replaced by filling a PGFxPass; the executor is a C port of
 * shaders/gpencil_vfx_frag.glsl and of blend_mode_output() (gpencil_common_lib.glsl).
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "project_grease_shader_fx.h"

/* ---------------------------------------------------------------------------------------- */
/* Type information                                                                          */

int pg_fx_valid_type(int type)
{
  switch (type) {
    case PG_FX_BLUR:
    case PG_FX_FLIP:
    case PG_FX_PIXEL:
    case PG_FX_SWIRL:
    case PG_FX_WAVE:
    case PG_FX_RIM:
    case PG_FX_COLORIZE:
    case PG_FX_SHADOW:
    case PG_FX_GLOW:
      return 1;
    default:
      return 0;
  }
}

int pg_fx_param_count(int type)
{
  switch (type) {
    case PG_FX_BLUR: return PG_FXP_BLUR_COUNT;
    case PG_FX_COLORIZE: return PG_FXP_COLORIZE_COUNT;
    case PG_FX_FLIP: return PG_FXP_FLIP_COUNT;
    case PG_FX_GLOW: return PG_FXP_GLOW_COUNT;
    case PG_FX_PIXEL: return PG_FXP_PIXEL_COUNT;
    case PG_FX_RIM: return PG_FXP_RIM_COUNT;
    case PG_FX_SHADOW: return PG_FXP_SHADOW_COUNT;
    case PG_FX_SWIRL: return PG_FXP_SWIRL_COUNT;
    case PG_FX_WAVE: return PG_FXP_WAVE_COUNT;
    default: return 0;
  }
}

const char *pg_fx_name(int type)
{
  switch (type) {
    case PG_FX_BLUR: return "Blur";
    case PG_FX_COLORIZE: return "Colorize";
    case PG_FX_FLIP: return "Flip";
    case PG_FX_GLOW: return "Glow";
    case PG_FX_PIXEL: return "Pixelate";
    case PG_FX_RIM: return "Rim";
    case PG_FX_SHADOW: return "Shadow";
    case PG_FX_SWIRL: return "Swirl";
    case PG_FX_WAVE: return "Wave Distortion";
    default: return "";
  }
}

/* initData() of FX_shader_*.c */
void pg_fx_defaults(int type, float p[PG_FX_MAX_PARAMS])
{
  memset(p, 0, sizeof(float) * PG_FX_MAX_PARAMS);
  switch (type) {
    case PG_FX_BLUR: /* radius 50, 50; samples 8; rotation 0 */
      p[PG_FXP_BLUR_RADIUS_X] = p[PG_FXP_BLUR_RADIUS_Y] = 50.0f;
      p[PG_FXP_BLUR_SAMPLES] = 8.0f;
      break;
    case PG_FX_COLORIZE: /* grayscale, low black, high white, factor 0.5 */
      p[PG_FXP_COLORIZE_HIGH] = p[PG_FXP_COLORIZE_HIGH + 1] = p[PG_FXP_COLORIZE_HIGH + 2] = 1.0f;
      p[PG_FXP_COLORIZE_FACTOR] = 0.5f;
      break;
    case PG_FX_FLIP: /* horizontal */
      p[PG_FXP_FLIP_HORIZONTAL] = 1.0f;
      break;
    case PG_FX_GLOW: /* color (0.75, 1, 1, 1), threshold 0.1, blur 50, samples 8, regular */
      p[PG_FXP_GLOW_COLOR] = 0.75f;
      p[PG_FXP_GLOW_COLOR + 1] = p[PG_FXP_GLOW_COLOR + 2] = p[PG_FXP_GLOW_COLOR + 3] = 1.0f;
      p[PG_FXP_GLOW_THRESHOLD] = 0.1f;
      p[PG_FXP_GLOW_BLUR_X] = p[PG_FXP_GLOW_BLUR_Y] = 50.0f;
      p[PG_FXP_GLOW_SAMPLES] = 8.0f;
      break;
    case PG_FX_PIXEL: /* size 5, 5 */
      p[PG_FXP_PIXEL_SIZE_X] = p[PG_FXP_PIXEL_SIZE_Y] = 5.0f;
      break;
    case PG_FX_RIM: /* offset 50, -100; rim (1, 1, 0.5); overlay; blur 0; samples 2 */
      p[PG_FXP_RIM_OFFSET_X] = 50.0f;
      p[PG_FXP_RIM_OFFSET_Y] = -100.0f;
      p[PG_FXP_RIM_COLOR] = p[PG_FXP_RIM_COLOR + 1] = 1.0f;
      p[PG_FXP_RIM_COLOR + 2] = 0.5f;
      p[PG_FXP_RIM_MODE] = 1.0f;
      p[PG_FXP_RIM_SAMPLES] = 2.0f;
      break;
    case PG_FX_SHADOW: /* offset 15, 20; color (0, 0, 0, 0.8); wave 10 / 20, vertical; blur 5; samples 2 */
      p[PG_FXP_SHADOW_OFFSET_X] = 15.0f;
      p[PG_FXP_SHADOW_OFFSET_Y] = 20.0f;
      p[PG_FXP_SHADOW_COLOR + 3] = 0.8f;
      p[PG_FXP_SHADOW_AMPLITUDE] = 10.0f;
      p[PG_FXP_SHADOW_PERIOD] = 20.0f;
      p[PG_FXP_SHADOW_ORIENTATION] = 1.0f;
      p[PG_FXP_SHADOW_SCALE_X] = p[PG_FXP_SHADOW_SCALE_Y] = 1.0f;
      p[PG_FXP_SHADOW_BLUR_X] = p[PG_FXP_SHADOW_BLUR_Y] = 5.0f;
      p[PG_FXP_SHADOW_SAMPLES] = 2.0f;
      break;
    case PG_FX_SWIRL: /* radius 100, angle pi/2; center: canvas middle (ADAPTED) */
      p[PG_FXP_SWIRL_CENTER_X] = p[PG_FXP_SWIRL_CENTER_Y] = 0.5f;
      p[PG_FXP_SWIRL_RADIUS] = 100.0f;
      p[PG_FXP_SWIRL_ANGLE] = (float)M_PI_2;
      break;
    case PG_FX_WAVE: /* amplitude 10, period 20, vertical */
      p[PG_FXP_WAVE_AMPLITUDE] = 10.0f;
      p[PG_FXP_WAVE_PERIOD] = 20.0f;
      p[PG_FXP_WAVE_ORIENTATION] = 1.0f;
      break;
    default:
      break;
  }
}

static float pgf_clamp(float v, float lo, float hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}
static float pgf_int(float v, float lo, float hi)
{
  return pgf_clamp(floorf(v + 0.5f), lo, hi);
}
static float pgf_flag(float v)
{
  return v != 0.0f ? 1.0f : 0.0f;
}

void pg_fx_sanitize(int type, float p[PG_FX_MAX_PARAMS])
{
  float def[PG_FX_MAX_PARAMS];
  const int n = pg_fx_param_count(type);
  pg_fx_defaults(type, def);
  for (int i = 0; i < PG_FX_MAX_PARAMS; i++) {
    if (i >= n) {
      p[i] = 0.0f;
    }
    else if (!isfinite(p[i])) {
      p[i] = def[i];
    }
  }
  const float big = 10000.0f;
  switch (type) {
    case PG_FX_BLUR:
      p[PG_FXP_BLUR_RADIUS_X] = pgf_clamp(p[PG_FXP_BLUR_RADIUS_X], 0.0f, big);
      p[PG_FXP_BLUR_RADIUS_Y] = pgf_clamp(p[PG_FXP_BLUR_RADIUS_Y], 0.0f, big);
      p[PG_FXP_BLUR_SAMPLES] = pgf_int(p[PG_FXP_BLUR_SAMPLES], 0.0f, 32.0f);
      break;
    case PG_FX_COLORIZE:
      p[PG_FXP_COLORIZE_MODE] = pgf_int(p[PG_FXP_COLORIZE_MODE], 0.0f, 4.0f);
      for (int i = PG_FXP_COLORIZE_LOW; i < PG_FXP_COLORIZE_FACTOR; i++) {
        p[i] = pgf_clamp(p[i], 0.0f, 1.0f);
      }
      p[PG_FXP_COLORIZE_FACTOR] = pgf_clamp(p[PG_FXP_COLORIZE_FACTOR], 0.0f, 1.0f);
      break;
    case PG_FX_FLIP:
      p[PG_FXP_FLIP_HORIZONTAL] = pgf_flag(p[PG_FXP_FLIP_HORIZONTAL]);
      p[PG_FXP_FLIP_VERTICAL] = pgf_flag(p[PG_FXP_FLIP_VERTICAL]);
      break;
    case PG_FX_GLOW:
      for (int i = PG_FXP_GLOW_COLOR; i < PG_FXP_GLOW_THRESHOLD; i++) {
        p[i] = pgf_clamp(p[i], 0.0f, 1.0f);
      }
      p[PG_FXP_GLOW_THRESHOLD] = pgf_clamp(p[PG_FXP_GLOW_THRESHOLD], 0.0f, 1.0f);
      p[PG_FXP_GLOW_MODE] = pgf_int(p[PG_FXP_GLOW_MODE], 0.0f, 1.0f);
      p[PG_FXP_GLOW_BLUR_X] = pgf_clamp(p[PG_FXP_GLOW_BLUR_X], 0.0f, big);
      p[PG_FXP_GLOW_BLUR_Y] = pgf_clamp(p[PG_FXP_GLOW_BLUR_Y], 0.0f, big);
      p[PG_FXP_GLOW_SAMPLES] = pgf_int(p[PG_FXP_GLOW_SAMPLES], 0.0f, 32.0f);
      p[PG_FXP_GLOW_BLEND] = pgf_int(p[PG_FXP_GLOW_BLEND], 0.0f, 5.0f);
      p[PG_FXP_GLOW_USE_ALPHA] = pgf_flag(p[PG_FXP_GLOW_USE_ALPHA]);
      break;
    case PG_FX_PIXEL:
      p[PG_FXP_PIXEL_SIZE_X] = pgf_int(p[PG_FXP_PIXEL_SIZE_X], 1.0f, 2000.0f);
      p[PG_FXP_PIXEL_SIZE_Y] = pgf_int(p[PG_FXP_PIXEL_SIZE_Y], 1.0f, 2000.0f);
      p[PG_FXP_PIXEL_NEAREST] = pgf_flag(p[PG_FXP_PIXEL_NEAREST]);
      break;
    case PG_FX_RIM:
      p[PG_FXP_RIM_OFFSET_X] = pgf_int(p[PG_FXP_RIM_OFFSET_X], -big, big);
      p[PG_FXP_RIM_OFFSET_Y] = pgf_int(p[PG_FXP_RIM_OFFSET_Y], -big, big);
      for (int i = PG_FXP_RIM_COLOR; i < PG_FXP_RIM_MODE; i++) {
        p[i] = pgf_clamp(p[i], 0.0f, 1.0f);
      }
      p[PG_FXP_RIM_MODE] = pgf_int(p[PG_FXP_RIM_MODE], 0.0f, 5.0f);
      p[PG_FXP_RIM_BLUR_X] = pgf_int(p[PG_FXP_RIM_BLUR_X], 0.0f, big);
      p[PG_FXP_RIM_BLUR_Y] = pgf_int(p[PG_FXP_RIM_BLUR_Y], 0.0f, big);
      p[PG_FXP_RIM_SAMPLES] = pgf_int(p[PG_FXP_RIM_SAMPLES], 0.0f, 32.0f);
      break;
    case PG_FX_SHADOW:
      p[PG_FXP_SHADOW_OFFSET_X] = pgf_int(p[PG_FXP_SHADOW_OFFSET_X], -big, big);
      p[PG_FXP_SHADOW_OFFSET_Y] = pgf_int(p[PG_FXP_SHADOW_OFFSET_Y], -big, big);
      for (int i = PG_FXP_SHADOW_COLOR; i < PG_FXP_SHADOW_USE_WAVE; i++) {
        p[i] = pgf_clamp(p[i], 0.0f, 1.0f);
      }
      p[PG_FXP_SHADOW_USE_WAVE] = pgf_flag(p[PG_FXP_SHADOW_USE_WAVE]);
      p[PG_FXP_SHADOW_AMPLITUDE] = pgf_clamp(p[PG_FXP_SHADOW_AMPLITUDE], -big, big);
      p[PG_FXP_SHADOW_PERIOD] = pgf_clamp(p[PG_FXP_SHADOW_PERIOD], 0.0f, big);
      p[PG_FXP_SHADOW_ORIENTATION] = pgf_int(p[PG_FXP_SHADOW_ORIENTATION], 0.0f, 1.0f);
      /* a zero scale divides by zero in the uv transform */
      p[PG_FXP_SHADOW_SCALE_X] = pgf_clamp(p[PG_FXP_SHADOW_SCALE_X], 0.01f, 100.0f);
      p[PG_FXP_SHADOW_SCALE_Y] = pgf_clamp(p[PG_FXP_SHADOW_SCALE_Y], 0.01f, 100.0f);
      p[PG_FXP_SHADOW_BLUR_X] = pgf_int(p[PG_FXP_SHADOW_BLUR_X], 0.0f, big);
      p[PG_FXP_SHADOW_BLUR_Y] = pgf_int(p[PG_FXP_SHADOW_BLUR_Y], 0.0f, big);
      p[PG_FXP_SHADOW_SAMPLES] = pgf_int(p[PG_FXP_SHADOW_SAMPLES], 0.0f, 32.0f);
      break;
    case PG_FX_SWIRL:
      p[PG_FXP_SWIRL_CENTER_X] = pgf_clamp(p[PG_FXP_SWIRL_CENTER_X], -1.0f, 2.0f);
      p[PG_FXP_SWIRL_CENTER_Y] = pgf_clamp(p[PG_FXP_SWIRL_CENTER_Y], -1.0f, 2.0f);
      p[PG_FXP_SWIRL_RADIUS] = pgf_int(p[PG_FXP_SWIRL_RADIUS], 0.0f, big);
      p[PG_FXP_SWIRL_ANGLE] = pgf_clamp(p[PG_FXP_SWIRL_ANGLE], -100.0f, 100.0f);
      break;
    case PG_FX_WAVE:
      p[PG_FXP_WAVE_AMPLITUDE] = pgf_clamp(p[PG_FXP_WAVE_AMPLITUDE], -big, big);
      p[PG_FXP_WAVE_PERIOD] = pgf_clamp(p[PG_FXP_WAVE_PERIOD], 0.0f, big);
      p[PG_FXP_WAVE_ORIENTATION] = pgf_int(p[PG_FXP_WAVE_ORIENTATION], 0.0f, 1.0f);
      break;
    default:
      break;
  }
}

void pg_fx_entry_init(PGFxEntry *entry, int type)
{
  memset(entry, 0, sizeof(*entry));
  entry->type = type;
  entry->enabled = 1;
  pg_fx_defaults(type, entry->params);
}

/* ---------------------------------------------------------------------------------------- */
/* Pass construction (gpencil_shader_fx.c)                                                   */

typedef struct FxCtx {
  const PGFxView *view;
  float vp_size[2], vp_size_inv[2];
  float distance_factor; /* ADAPTED: canvas pixels -> viewport pixels (Blender: world units) */
  float center[2];       /* ADAPTED: the canvas center in uv, standing in for the object origin */
} FxCtx;

static void fx_ctx_init(FxCtx *c, const PGFxView *view)
{
  c->view = view;
  c->vp_size[0] = (float)view->width;
  c->vp_size[1] = (float)view->height;
  c->vp_size_inv[0] = 1.0f / c->vp_size[0];
  c->vp_size_inv[1] = 1.0f / c->vp_size[1];
  c->distance_factor = view->scale;
  const float cx = view->origin_x + 0.5f * (float)view->canvas_w * view->scale;
  const float cy = view->origin_y + 0.5f * (float)view->canvas_h * view->scale;
  c->center[0] = cx * c->vp_size_inv[0];
  c->center[1] = 1.0f - cy * c->vp_size_inv[1]; /* uv y points up */
}

static PGFxPass *fx_new_pass(PGFxPass *out, int *count, int kind)
{
  PGFxPass *p = &out[(*count)++];
  memset(p, 0, sizeof(*p));
  p->kind = kind;
  p->blend = PGFX_BLEND_NONE;
  p->src = PGFX_SRC_PREV;
  p->dst = PGFX_DST_NONE;
  return p;
}

static int fx_max_i(int a, int b) { return a > b ? a : b; }
static int fx_min_i(int a, int b) { return a < b ? a : b; }

static int build_blur(const float *p, const FxCtx *c, PGFxPass *out)
{
  int n = 0;
  const int samples = (int)p[PG_FXP_BLUR_SAMPLES];
  if (samples == 0 || (p[PG_FXP_BLUR_RADIUS_X] == 0.0f && p[PG_FXP_BLUR_RADIUS_Y] == 0.0f)) {
    return 0;
  }
  const float s = sinf(p[PG_FXP_BLUR_ROTATION]);
  const float co = cosf(p[PG_FXP_BLUR_ROTATION]);
  float blur_size[2] = {p[PG_FXP_BLUR_RADIUS_X], p[PG_FXP_BLUR_RADIUS_Y]};
  mul_v2_fl(blur_size, c->distance_factor);
  if (blur_size[0] > 0.0f) {
    PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_BLUR);
    q->offset[0] = blur_size[0] * co;
    q->offset[1] = blur_size[0] * s;
    q->samp_count = fx_max_i(1, fx_min_i(samples, (int)blur_size[0]));
  }
  if (blur_size[1] > 0.0f) {
    PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_BLUR);
    q->offset[0] = -blur_size[1] * s;
    q->offset[1] = blur_size[1] * co;
    q->samp_count = fx_max_i(1, fx_min_i(samples, (int)blur_size[1]));
  }
  return n;
}

static int build_colorize(const float *p, PGFxPass *out)
{
  int n = 0;
  PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_COLORIZE);
  copy_v3_v3(q->low_color, &p[PG_FXP_COLORIZE_LOW]);
  copy_v3_v3(q->high_color, &p[PG_FXP_COLORIZE_HIGH]);
  q->factor = p[PG_FXP_COLORIZE_FACTOR];
  q->mode = (int)p[PG_FXP_COLORIZE_MODE];
  return n;
}

static int build_flip(const float *p, PGFxPass *out)
{
  int n = 0;
  PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_TRANSFORM);
  q->axis_flip[0] = p[PG_FXP_FLIP_HORIZONTAL] != 0.0f ? -1.0f : 1.0f;
  q->axis_flip[1] = p[PG_FXP_FLIP_VERTICAL] != 0.0f ? -1.0f : 1.0f;
  q->wave_offset[0] = q->wave_offset[1] = 0.0f;
  q->swirl_radius = 0.0f;
  return n;
}

static int build_wave(const float *p, const FxCtx *c, PGFxPass *out)
{
  int n = 0;
  float wave_ofs[2], wave_dir[2], wave_phase;
  const float *wave_center = c->center;
  if ((int)p[PG_FXP_WAVE_ORIENTATION] == 0) {
    copy_v2_fl2(wave_dir, 1.0f, 0.0f); /* horizontal */
  }
  else {
    copy_v2_fl2(wave_dir, 0.0f, 1.0f); /* vertical */
  }
  /* Rotate 90°. */
  copy_v2_v2(wave_ofs, wave_dir);
  SWAP(float, wave_ofs[0], wave_ofs[1]);
  wave_ofs[1] *= -1.0f;
  /* Keep world space scaling and aspect ratio. */
  mul_v2_fl(wave_dir, 1.0f / (max_ff(1e-8f, p[PG_FXP_WAVE_PERIOD]) * c->distance_factor));
  mul_v2_v2(wave_dir, c->vp_size);
  mul_v2_fl(wave_ofs, p[PG_FXP_WAVE_AMPLITUDE] * c->distance_factor);
  mul_v2_v2(wave_ofs, c->vp_size_inv);
  /* Phase start at the wave center. */
  wave_phase = p[PG_FXP_WAVE_PHASE] - dot_v2v2(wave_center, wave_dir);

  PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_TRANSFORM);
  q->axis_flip[0] = q->axis_flip[1] = 1.0f;
  copy_v2_v2(q->wave_dir, wave_dir);
  copy_v2_v2(q->wave_offset, wave_ofs);
  q->wave_phase = wave_phase;
  q->swirl_radius = 0.0f;
  return n;
}

static int build_swirl(const float *p, const FxCtx *c, PGFxPass *out)
{
  int n = 0;
  float swirl_center[2];
  /* ADAPTED: the center is a fraction of the canvas (Blender: the origin of another object),
   * converted to viewport pixels with y up like the texture coordinates. */
  const float x = c->view->origin_x + p[PG_FXP_SWIRL_CENTER_X] * (float)c->view->canvas_w * c->view->scale;
  const float y = c->view->origin_y + p[PG_FXP_SWIRL_CENTER_Y] * (float)c->view->canvas_h * c->view->scale;
  swirl_center[0] = x;
  swirl_center[1] = c->vp_size[1] - y;
  float radius = p[PG_FXP_SWIRL_RADIUS] * c->distance_factor;
  if (radius < 1.0f) {
    return 0;
  }
  PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_TRANSFORM);
  q->axis_flip[0] = q->axis_flip[1] = 1.0f;
  q->wave_offset[0] = q->wave_offset[1] = 0.0f;
  copy_v2_v2(q->swirl_center, swirl_center);
  q->swirl_angle = p[PG_FXP_SWIRL_ANGLE];
  q->swirl_radius = radius;
  return n;
}

static int build_pixel(const float *p, const FxCtx *c, PGFxPass *out)
{
  int n = 0;
  float ob_center[2], pixel_size[2] = {p[PG_FXP_PIXEL_SIZE_X], p[PG_FXP_PIXEL_SIZE_Y]};
  mul_v2_v2(pixel_size, c->vp_size_inv);
  copy_v2_v2(ob_center, c->center); /* ADAPTED: canvas center for the object center */
  const bool use_antialiasing = p[PG_FXP_PIXEL_NEAREST] == 0.0f;
  /* Modify by the canvas scale (Blender: distance to camera and object scale). */
  mul_v2_fl(pixel_size, c->distance_factor);
  /* Center to texel */
  madd_v2_v2fl(ob_center, pixel_size, -0.5f);

  /* Only if pixelated effect is bigger than 1px. */
  if (pixel_size[0] > c->vp_size_inv[0]) {
    PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_PIXELIZE);
    copy_v2_fl2(q->target_pixel_size, pixel_size[0], c->vp_size_inv[1]);
    copy_v2_v2(q->target_pixel_offset, ob_center);
    copy_v2_fl2(q->accum_offset, pixel_size[0], 0.0f);
    const int samp_count = (pixel_size[0] / c->vp_size_inv[0] > 3.0f) ? 2 : 1;
    q->samp_count = use_antialiasing ? samp_count : 0;
  }
  if (pixel_size[1] > c->vp_size_inv[1]) {
    PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_PIXELIZE);
    copy_v2_fl2(q->target_pixel_size, c->vp_size_inv[0], pixel_size[1]);
    /* ADAPTED: Blender leaves the offset of this pass unset (the program keeps the last value). */
    copy_v2_v2(q->target_pixel_offset, ob_center);
    copy_v2_fl2(q->accum_offset, 0.0f, pixel_size[1]);
    const int samp_count = (pixel_size[1] / c->vp_size_inv[1] > 3.0f) ? 2 : 1;
    q->samp_count = use_antialiasing ? samp_count : 0;
  }
  return n;
}

static int blend_state_for_mode(int mode, int overlay_is_none)
{
  switch (mode) {
    case 0: return PGFX_BLEND_PREMUL;
    case 2: return PGFX_BLEND_ADD;
    case 3: return PGFX_BLEND_SUB;
    case 4:
    case 5: return PGFX_BLEND_MUL;
    default: return overlay_is_none ? PGFX_BLEND_NONE : PGFX_BLEND_MUL;
  }
}

static int build_rim(const float *p, const FxCtx *c, PGFxPass *out)
{
  int n = 0;
  float offset[2] = {p[PG_FXP_RIM_OFFSET_X], p[PG_FXP_RIM_OFFSET_Y]};
  float blur_size[2] = {p[PG_FXP_RIM_BLUR_X], p[PG_FXP_RIM_BLUR_Y]};
  const int samples = (int)p[PG_FXP_RIM_SAMPLES];
  const int mode = (int)p[PG_FXP_RIM_MODE];

  /* Modify by the canvas scale. */
  mul_v2_fl(offset, c->distance_factor);
  mul_v2_v2(offset, c->vp_size_inv);
  mul_v2_fl(blur_size, c->distance_factor);

  PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_RIM);
  copy_v2_fl2(q->blur_dir, blur_size[0] * c->vp_size_inv[0], 0.0f);
  copy_v2_v2(q->uv_offset, offset);
  q->samp_count = fx_max_i(1, fx_min_i(samples, (int)blur_size[0]));
  copy_v3_v3(q->mask_color, &p[PG_FXP_RIM_MASK]);
  q->first_pass = 1;

  /* eShaderFxRimMode: Normal premul, Add, Subtract, Multiply/Divide/Overlay multiply. */
  const int blend = (mode == 0) ? PGFX_BLEND_PREMUL :
                    (mode == 2) ? PGFX_BLEND_ADD :
                    (mode == 3) ? PGFX_BLEND_SUB :
                                  PGFX_BLEND_MUL;
  q = fx_new_pass(out, &n, PGFX_PASS_RIM);
  q->blend = blend;
  q->dst = PGFX_DST_START;
  copy_v2_fl2(q->blur_dir, 0.0f, blur_size[1] * c->vp_size_inv[1]);
  zero_v2(q->uv_offset);
  copy_v3_v3(q->rim_color, &p[PG_FXP_RIM_COLOR]);
  copy_v3_v3(q->mask_color, &p[PG_FXP_RIM_MASK]);
  q->samp_count = fx_max_i(1, fx_min_i(samples, (int)blur_size[1]));
  q->blend_mode = mode;
  q->first_pass = 0;

  if (mode == 1) {
    /* Overlay: a second additive blend of the same pass with blendMode 999. */
    PGFxPass first_blend = *q;
    q = fx_new_pass(out, &n, PGFX_PASS_RIM);
    *q = first_blend;
    q->blend = PGFX_BLEND_ADD;
    q->src = PGFX_SRC_PREV_SRC;
    q->dst = PGFX_DST_PREV;
    q->blend_mode = 999;
  }
  return n;
}

static int build_shadow(const float *p, const FxCtx *c, PGFxPass *out)
{
  int n = 0;
  const bool use_wave = p[PG_FXP_SHADOW_USE_WAVE] != 0.0f;
  float uv_mat[4][4], rot_center[2];
  float wave_ofs[2], wave_dir[2], wave_phase, blur_dir[2], tmp[2];
  float offset[2] = {p[PG_FXP_SHADOW_OFFSET_X], p[PG_FXP_SHADOW_OFFSET_Y]};
  float blur_size[2] = {p[PG_FXP_SHADOW_BLUR_X], p[PG_FXP_SHADOW_BLUR_Y]};
  const float rotation = p[PG_FXP_SHADOW_ROTATION];
  const int samples = (int)p[PG_FXP_SHADOW_SAMPLES];
  const float ratio = c->vp_size_inv[1] / c->vp_size_inv[0];

  copy_v2_v2(rot_center, c->center); /* ADAPTED: canvas center (Blender: object origin or pivot object) */

  /* Modify by the canvas scale. */
  mul_v2_fl(offset, c->distance_factor);
  mul_v2_v2(offset, c->vp_size_inv);
  mul_v2_fl(blur_size, c->distance_factor);

  /* UV transform matrix. (loc, rot, scale) Sent to shader as 2x3 matrix. */
  unit_m4(uv_mat);
  translate_m4(uv_mat, rot_center[0], rot_center[1], 0.0f);
  rescale_m4(uv_mat, (float[3]){1.0f / p[PG_FXP_SHADOW_SCALE_X], 1.0f / p[PG_FXP_SHADOW_SCALE_Y], 1.0f});
  translate_m4(uv_mat, -offset[0], -offset[1], 0.0f);
  rescale_m4(uv_mat, (float[3]){1.0f / ratio, 1.0f, 1.0f});
  rotate_m4(uv_mat, 'Z', rotation);
  rescale_m4(uv_mat, (float[3]){ratio, 1.0f, 1.0f});
  translate_m4(uv_mat, -rot_center[0], -rot_center[1], 0.0f);

  if (use_wave) {
    float dir[2];
    if ((int)p[PG_FXP_SHADOW_ORIENTATION] == 0) {
      copy_v2_fl2(dir, 1.0f, 0.0f); /* Horizontal */
    }
    else {
      copy_v2_fl2(dir, 0.0f, 1.0f); /* Vertical */
    }
    /* This is applied after rotation. Counter the rotation to keep aligned with global axis. */
    rotate_v2_v2fl(wave_dir, dir, rotation);
    /* Rotate 90°. */
    copy_v2_v2(wave_ofs, wave_dir);
    SWAP(float, wave_ofs[0], wave_ofs[1]);
    wave_ofs[1] *= -1.0f;
    /* Keep world space scaling and aspect ratio. */
    mul_v2_fl(wave_dir, 1.0f / (max_ff(1e-8f, p[PG_FXP_SHADOW_PERIOD]) * c->distance_factor));
    mul_v2_v2(wave_dir, c->vp_size);
    mul_v2_fl(wave_ofs, p[PG_FXP_SHADOW_AMPLITUDE] * c->distance_factor);
    mul_v2_v2(wave_ofs, c->vp_size_inv);
    /* Phase start at shadow center. */
    wave_phase = p[PG_FXP_SHADOW_PHASE] - dot_v2v2(rot_center, wave_dir);
  }
  else {
    zero_v2(wave_dir);
    zero_v2(wave_ofs);
    wave_phase = 0.0f;
  }

  copy_v2_fl2(blur_dir, blur_size[0] * c->vp_size_inv[0], 0.0f);

  PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_SHADOW);
  copy_v2_v2(q->blur_dir, blur_dir);
  copy_v2_v2(q->wave_dir, wave_dir);
  copy_v2_v2(q->wave_offset, wave_ofs);
  q->wave_phase = wave_phase;
  copy_v2_v2(q->uv_rot_x, uv_mat[0]);
  copy_v2_v2(q->uv_rot_y, uv_mat[1]);
  copy_v2_v2(q->uv_offset, uv_mat[3]);
  q->samp_count = fx_max_i(1, fx_min_i(samples, (int)blur_size[0]));
  q->first_pass = 1;

  unit_m4(uv_mat);
  zero_v2(wave_ofs);

  /* Reset the `uv_mat` to account for rotation in the Y-axis (Shadow-V parameter). */
  copy_v2_fl2(tmp, 0.0f, blur_size[1]);
  rotate_v2_v2fl(blur_dir, tmp, -rotation);
  mul_v2_v2(blur_dir, c->vp_size_inv);

  q = fx_new_pass(out, &n, PGFX_PASS_SHADOW);
  q->blend = PGFX_BLEND_PREMUL;
  q->dst = PGFX_DST_START;
  copy_v4_v4(q->shadow_color, &p[PG_FXP_SHADOW_COLOR]);
  copy_v2_v2(q->blur_dir, blur_dir);
  copy_v2_v2(q->wave_offset, wave_ofs);
  /* wave_dir / wave_phase keep the first pass's values (uniforms persist in the program) */
  copy_v2_v2(q->wave_dir, wave_dir);
  q->wave_phase = wave_phase;
  copy_v2_v2(q->uv_rot_x, uv_mat[0]);
  copy_v2_v2(q->uv_rot_y, uv_mat[1]);
  copy_v2_v2(q->uv_offset, uv_mat[3]);
  q->samp_count = fx_max_i(1, fx_min_i(samples, (int)blur_size[1]));
  q->first_pass = 0;
  return n;
}

static int build_glow(const float *p, const FxCtx *c, PGFxPass *out)
{
  (void)c;
  int n = 0;
  const bool use_glow_under = p[PG_FXP_GLOW_USE_ALPHA] != 0.0f;
  const float s = sinf(p[PG_FXP_GLOW_ROTATION]);
  const float co = cosf(p[PG_FXP_GLOW_ROTATION]);
  const int samples = (int)p[PG_FXP_GLOW_SAMPLES];
  const int blend_mode = (int)p[PG_FXP_GLOW_BLEND];
  float ref_col[4];

  if ((int)p[PG_FXP_GLOW_MODE] == 0) { /* Luminance: only the first value */
    ref_col[0] = p[PG_FXP_GLOW_THRESHOLD];
    ref_col[1] = ref_col[2] = ref_col[3] = -1.0f;
  }
  else { /* Color: the selected color and the threshold */
    copy_v3_v3(ref_col, &p[PG_FXP_GLOW_SELECT]);
    ref_col[3] = p[PG_FXP_GLOW_THRESHOLD];
  }

  /* NOTE: Blender does not scale the glow blur by the object distance, it is in viewport pixels. */
  PGFxPass *q = fx_new_pass(out, &n, PGFX_PASS_GLOW);
  copy_v2_fl2(q->offset, p[PG_FXP_GLOW_BLUR_X] * co, p[PG_FXP_GLOW_BLUR_X] * s);
  q->samp_count = fx_max_i(1, fx_min_i(samples, (int)p[PG_FXP_GLOW_BLUR_X]));
  copy_v4_v4(q->threshold, ref_col);
  copy_v4_v4(q->glow_color, &p[PG_FXP_GLOW_COLOR]);
  q->glow_under = use_glow_under;
  q->first_pass = 1;

  q = fx_new_pass(out, &n, PGFX_PASS_GLOW);
  q->blend = blend_state_for_mode(blend_mode, 1);
  q->dst = (q->blend == PGFX_BLEND_NONE) ? PGFX_DST_NONE : PGFX_DST_START;
  copy_v2_fl2(q->offset, -p[PG_FXP_GLOW_BLUR_Y] * s, p[PG_FXP_GLOW_BLUR_Y] * co);
  q->samp_count = fx_max_i(1, fx_min_i(samples, (int)p[PG_FXP_GLOW_BLUR_X])); /* sic: Blender uses blur[0] */
  q->threshold[0] = q->threshold[1] = q->threshold[2] = q->threshold[3] = -1.0f;
  copy_v4_fl4(q->glow_color, 1.0f, 1.0f, 1.0f, p[PG_FXP_GLOW_COLOR + 3]);
  q->glow_under = use_glow_under;
  q->first_pass = 0;
  q->blend_mode = blend_mode;
  return n;
}

int pg_fx_build_passes(const PGFxEntry *entry, const PGFxView *view, PGFxPass out[PG_FX_MAX_PASSES])
{
  if (entry == NULL || view == NULL || out == NULL || !entry->enabled || !pg_fx_valid_type(entry->type) ||
      view->width <= 0 || view->height <= 0)
  {
    return 0;
  }
  FxCtx c;
  fx_ctx_init(&c, view);
  const float *p = entry->params;
  switch (entry->type) {
    case PG_FX_BLUR: return build_blur(p, &c, out);
    case PG_FX_COLORIZE: return build_colorize(p, out);
    case PG_FX_FLIP: return build_flip(p, out);
    case PG_FX_WAVE: return build_wave(p, &c, out);
    case PG_FX_SWIRL: return build_swirl(p, &c, out);
    case PG_FX_PIXEL: return build_pixel(p, &c, out);
    case PG_FX_RIM: return build_rim(p, &c, out);
    case PG_FX_SHADOW: return build_shadow(p, &c, out);
    case PG_FX_GLOW: return build_glow(p, &c, out);
    default: return 0;
  }
}

/* ---------------------------------------------------------------------------------------- */
/* CPU executor: C port of gpencil_vfx_frag.glsl                                             */

typedef struct V4 {
  float x, y, z, w;
} V4;

static V4 v4(float x, float y, float z, float w)
{
  V4 r = {x, y, z, w};
  return r;
}
static V4 v4_add(V4 a, V4 b) { return v4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
static V4 v4_mul(V4 a, V4 b) { return v4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
static V4 v4_scale(V4 a, float s) { return v4(a.x * s, a.y * s, a.z * s, a.w * s); }
static float fx_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float fx_mixf(float a, float b, float t) { return a * (1.0f - t) + b * t; }
static float fx_dot3(V4 a, float x, float y, float z) { return a.x * x + a.y * y + a.z * z; }

PGFxImage pg_fx_image_new(int width, int height)
{
  PGFxImage im;
  im.width = width;
  im.height = height;
  im.color = (float *)calloc((size_t)width * (size_t)height * 4u, sizeof(float));
  im.reveal = (float *)calloc((size_t)width * (size_t)height * 4u, sizeof(float));
  return im;
}

void pg_fx_image_free(PGFxImage *image)
{
  free(image->color);
  free(image->reveal);
  image->color = image->reveal = NULL;
}

void pg_fx_image_from_premult(PGFxImage *image, const float *premult_rgba)
{
  const size_t n = (size_t)image->width * (size_t)image->height;
  for (size_t i = 0; i < n; i++) {
    for (int c = 0; c < 4; c++) {
      image->color[i * 4 + c] = premult_rgba[i * 4 + c];
    }
    const float r = 1.0f - premult_rgba[i * 4 + 3];
    image->reveal[i * 4 + 0] = image->reveal[i * 4 + 1] = image->reveal[i * 4 + 2] = r;
    image->reveal[i * 4 + 3] = 1.0f;
  }
}

/* texture(): bilinear, clamp to edge, texel centers at (i + 0.5) / size like GL_LINEAR. */
static V4 tex(const PGFxImage *im, const float *plane, float u, float v)
{
  const float x = u * (float)im->width - 0.5f;
  const float y = v * (float)im->height - 0.5f;
  const int x0 = (int)floorf(x), y0 = (int)floorf(y);
  const float fx = x - (float)x0, fy = y - (float)y0;
  V4 acc = v4(0, 0, 0, 0);
  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < 2; i++) {
      int xi = x0 + i, yi = y0 + j;
      xi = xi < 0 ? 0 : (xi >= im->width ? im->width - 1 : xi);
      yi = yi < 0 ? 0 : (yi >= im->height ? im->height - 1 : yi);
      const float w = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
      const float *t = &plane[((size_t)yi * (size_t)im->width + (size_t)xi) * 4u];
      acc = v4_add(acc, v4(t[0] * w, t[1] * w, t[2] * w, t[3] * w));
    }
  }
  return acc;
}

static float gaussian_weight(float x)
{
  return expf(-x * x / (2.0f * 0.35f * 0.35f));
}

/* blend_mode_output() of gpencil_common_lib.glsl */
static void blend_mode_output(int blend_mode, V4 color, float opacity, V4 *frag_color, V4 *frag_revealage)
{
  switch (blend_mode) {
    case 0: /* MODE_REGULAR */
      color = v4_scale(color, opacity);
      *frag_color = color;
      *frag_revealage = v4(0.0f, 0.0f, 0.0f, color.w);
      break;
    case 4: { /* MODE_MULTIPLY */
      color.w *= opacity;
      const V4 r = v4_add(v4(1.0f - color.w, 1.0f - color.w, 1.0f - color.w, 1.0f - color.w),
                          v4_scale(color, color.w));
      *frag_color = *frag_revealage = r;
      break;
    }
    case 5: { /* MODE_DIVIDE */
      color.w *= opacity;
      V4 r;
      const float cc[4] = {color.x, color.y, color.z, color.w};
      float o[4];
      for (int i = 0; i < 4; i++) {
        const float d = fmaxf(1e-6f, 1.0f - cc[i] * color.w);
        o[i] = fx_clampf(1.0f / d, 0.0f, 1e18f);
      }
      r = v4(o[0], o[1], o[2], o[3]);
      *frag_color = *frag_revealage = r;
      break;
    }
    case 1: { /* MODE_HARDLIGHT */
      const float f = color.w * opacity;
      const float cc[4] = {fx_mixf(0.5f, color.x, f), fx_mixf(0.5f, color.y, f), fx_mixf(0.5f, color.z, f),
                           fx_mixf(0.5f, color.w, f)};
      float o[4];
      for (int i = 0; i < 4; i++) {
        const float s = (-0.5f <= -cc[i]) ? 1.0f : 0.0f; /* step(-0.5, -color) */
        o[i] = 2.0f * s + 2.0f * cc[i] * (1.0f - s * 2.0f);
      }
      *frag_color = v4(o[0], o[1], o[2], o[3]);
      *frag_revealage = v4(fmaxf(0, o[0]), fmaxf(0, o[1]), fmaxf(0, o[2]), fmaxf(0, o[3]));
      break;
    }
    case 999: { /* MODE_HARDLIGHT_SECOND_PASS */
      const float f = color.w * opacity;
      const float cc[4] = {fx_mixf(0.5f, color.x, f), fx_mixf(0.5f, color.y, f), fx_mixf(0.5f, color.z, f),
                           fx_mixf(0.5f, color.w, f)};
      float o[4];
      for (int i = 0; i < 4; i++) {
        const float s = (-0.5f <= -cc[i]) ? 1.0f : 0.0f;
        o[i] = (-1.0f + 2.0f * cc[i]) * s;
      }
      *frag_color = v4(o[0], o[1], o[2], o[3]);
      *frag_revealage = v4(fmaxf(0, o[0]), fmaxf(0, o[1]), fmaxf(0, o[2]), fmaxf(0, o[3]));
      break;
    }
    case 2: /* MODE_ADD */
    case 3: /* MODE_SUB */
      *frag_color = v4_scale(color, color.w * opacity);
      *frag_revealage = v4(0, 0, 0, 0);
      break;
    default:
      break;
  }
}

static V4 apply_blend(int blend, V4 src, V4 dst)
{
  switch (blend) {
    case PGFX_BLEND_PREMUL: return v4_add(src, v4_scale(dst, 1.0f - src.w));
    case PGFX_BLEND_ADD: return v4_add(src, dst);
    case PGFX_BLEND_SUB: return v4(dst.x - src.x, dst.y - src.y, dst.z - src.z, dst.w - src.w);
    case PGFX_BLEND_MUL: return v4_mul(src, dst);
    default: return src;
  }
}

static V4 floor_v2_nonzero(float u, float v)
{
  /* any(not(equal(vec2(0.0), floor(uv)))) as 0/1 */
  return v4((floorf(u) != 0.0f || floorf(v) != 0.0f) ? 1.0f : 0.0f, 0, 0, 0);
}

/* One pixel of one pass: src = (colorBuf, revealBuf) images, uv in 0..1. */
static void run_pixel(const PGFxPass *p, const PGFxImage *src, float u, float v, V4 *frag_color, V4 *frag_revealage)
{
  const float *C = src->color, *R = src->reveal;
  const float pixel_size_x = 1.0f / (float)src->width, pixel_size_y = 1.0f / (float)src->height;
  *frag_color = v4(0, 0, 0, 0);
  *frag_revealage = v4(0, 0, 0, 0);

  switch (p->kind) {
    case PGFX_PASS_COLORIZE: {
      *frag_color = tex(src, C, u, v);
      *frag_revealage = tex(src, R, u, v);
      const float luma = fx_dot3(*frag_color, 0.2126f, 0.7152f, 0.723f);
      switch (p->mode) {
        case 0: /* grayscale */
          frag_color->x = fx_mixf(frag_color->x, luma, p->factor);
          frag_color->y = fx_mixf(frag_color->y, luma, p->factor);
          frag_color->z = fx_mixf(frag_color->z, luma, p->factor);
          break;
        case 1: { /* sepia: sepia_mat * rgb, columns (0.393,0.349,0.272) (0.769,0.686,0.534) (0.189,0.168,0.131) */
          const float r = frag_color->x, g = frag_color->y, b = frag_color->z;
          const float sr = 0.393f * r + 0.769f * g + 0.189f * b;
          const float sg = 0.349f * r + 0.686f * g + 0.168f * b;
          const float sb = 0.272f * r + 0.534f * g + 0.131f * b;
          frag_color->x = fx_mixf(r, sr, p->factor);
          frag_color->y = fx_mixf(g, sg, p->factor);
          frag_color->z = fx_mixf(b, sb, p->factor);
          break;
        }
        case 2: { /* duotone */
          const float *col = (luma <= p->factor) ? p->low_color : p->high_color;
          frag_color->x = luma * col[0];
          frag_color->y = luma * col[1];
          frag_color->z = luma * col[2];
          break;
        }
        case 3: /* custom */
          frag_color->x = fx_mixf(frag_color->x, luma * p->low_color[0], p->factor);
          frag_color->y = fx_mixf(frag_color->y, luma * p->low_color[1], p->factor);
          frag_color->z = fx_mixf(frag_color->z, luma * p->low_color[2], p->factor);
          break;
        default: /* transparent */
          frag_color->x *= p->factor;
          frag_color->y *= p->factor;
          frag_color->z *= p->factor;
          frag_revealage->x = fx_mixf(1.0f, frag_revealage->x, p->factor);
          frag_revealage->y = fx_mixf(1.0f, frag_revealage->y, p->factor);
          frag_revealage->z = fx_mixf(1.0f, frag_revealage->z, p->factor);
          break;
      }
      break;
    }
    case PGFX_PASS_BLUR: {
      const float ofs_x = p->offset[0] * pixel_size_x, ofs_y = p->offset[1] * pixel_size_y;
      float weight_accum = 0.0f;
      for (int i = -p->samp_count; i <= p->samp_count; i++) {
        const float x = (float)i / (float)p->samp_count;
        const float weight = gaussian_weight(x);
        weight_accum += weight;
        const float uu = u + ofs_x * x, vv = v + ofs_y * x;
        const V4 c = tex(src, C, uu, vv), r = tex(src, R, uu, vv);
        frag_color->x += c.x * weight; frag_color->y += c.y * weight; frag_color->z += c.z * weight;
        frag_revealage->x += r.x * weight; frag_revealage->y += r.y * weight; frag_revealage->z += r.z * weight;
      }
      *frag_color = v4_scale(*frag_color, 1.0f / weight_accum);
      *frag_revealage = v4_scale(*frag_revealage, 1.0f / weight_accum);
      break;
    }
    case PGFX_PASS_TRANSFORM: {
      float uu = (u - 0.5f) * p->axis_flip[0] + 0.5f;
      float vv = (v - 0.5f) * p->axis_flip[1] + 0.5f;
      /* Wave deform. */
      const float wave_time = uu * p->wave_dir[0] + vv * p->wave_dir[1];
      uu += sinf(wave_time + p->wave_phase) * p->wave_offset[0];
      vv += sinf(wave_time + p->wave_phase) * p->wave_offset[1];
      /* Swirl deform. */
      if (p->swirl_radius > 0.0f) {
        const float tw = (float)src->width, th = (float)src->height;
        const float px = uu * tw - p->swirl_center[0], py = vv * th - p->swirl_center[1];
        const float dist = sqrtf(px * px + py * py);
        const float percent = fx_clampf((p->swirl_radius - dist) / p->swirl_radius, 0.0f, 1.0f);
        const float theta = percent * percent * p->swirl_angle;
        const float s = sinf(theta), c = cosf(theta);
        /* mat2 rot = mat2(vec2(c, -s), vec2(s, c)): columns, so rot * p = (c*px + s*py, -s*px + c*py) */
        uu = (c * px + s * py + p->swirl_center[0]) / tw;
        vv = (-s * px + c * py + p->swirl_center[1]) / th;
      }
      *frag_color = tex(src, C, uu, vv);
      *frag_revealage = tex(src, R, uu, vv);
      break;
    }
    case PGFX_PASS_PIXELIZE: {
      const float px = floorf((u - p->target_pixel_offset[0]) / p->target_pixel_size[0]);
      const float py = floorf((v - p->target_pixel_offset[1]) / p->target_pixel_size[1]);
      const float uu = (px + 0.5f) * p->target_pixel_size[0] + p->target_pixel_offset[0];
      const float vv = (py + 0.5f) * p->target_pixel_size[1] + p->target_pixel_offset[1];
      for (int i = -p->samp_count; i <= p->samp_count; i++) {
        const float x = (float)i / (float)(p->samp_count + 1);
        const float ou = uu + p->accum_offset[0] * 0.5f * x, ov = vv + p->accum_offset[1] * 0.5f * x;
        *frag_color = v4_add(*frag_color, tex(src, C, ou, ov));
        *frag_revealage = v4_add(*frag_revealage, tex(src, R, ou, ov));
      }
      const float inv = 1.0f / ((float)p->samp_count * 2.0f + 1.0f);
      *frag_color = v4_scale(*frag_color, inv);
      *frag_revealage = v4_scale(*frag_revealage, inv);
      break;
    }
    case PGFX_PASS_GLOW: {
      const float ofs_x = p->offset[0] * pixel_size_x, ofs_y = p->offset[1] * pixel_size_y;
      float weight_accum = 0.0f;
      for (int i = -p->samp_count; i <= p->samp_count; i++) {
        const float x = (float)i / (float)p->samp_count;
        float weight = gaussian_weight(x);
        weight_accum += weight;
        const float uu = u + ofs_x * x, vv = v + ofs_y * x;
        const V4 col = tex(src, C, uu, vv), rev = tex(src, R, uu, vv);
        if (p->threshold[0] > -1.0f) {
          if (p->threshold[1] > -1.0f) {
            if (fabsf(col.x - p->threshold[0]) > p->threshold[3] || fabsf(col.y - p->threshold[1]) > p->threshold[3] ||
                fabsf(col.z - p->threshold[2]) > p->threshold[3])
            {
              weight = 0.0f;
            }
          }
          else if (fx_dot3(col, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f) < p->threshold[0]) {
            weight = 0.0f;
          }
        }
        frag_color->x += col.x * weight; frag_color->y += col.y * weight; frag_color->z += col.z * weight;
        frag_revealage->x += (1.0f - rev.x) * weight;
        frag_revealage->y += (1.0f - rev.y) * weight;
        frag_revealage->z += (1.0f - rev.z) * weight;
      }
      if (weight_accum > 0.0f) {
        /* fragColor *= glowColor.rgbb / weight_accum; */
        frag_color->x *= p->glow_color[0] / weight_accum;
        frag_color->y *= p->glow_color[1] / weight_accum;
        frag_color->z *= p->glow_color[2] / weight_accum;
        frag_color->w *= p->glow_color[2] / weight_accum; /* .rgbb: the fourth component is b */
        *frag_revealage = v4_scale(*frag_revealage, 1.0f / weight_accum);
      }
      *frag_revealage = v4(1.0f - frag_revealage->x, 1.0f - frag_revealage->y, 1.0f - frag_revealage->z,
                           1.0f - frag_revealage->w);
      if (p->glow_under) {
        if (p->first_pass) {
          const V4 o = tex(src, R, u, v);
          frag_revealage->w = fx_clampf(fx_dot3(o, 0.333334f, 0.333334f, 0.333334f), 0.0f, 1.0f);
        }
        else {
          frag_revealage->w = tex(src, R, u, v).w;
        }
      }
      if (!p->first_pass) {
        frag_color->w = fx_clampf(1.0f - fx_dot3(*frag_revealage, 0.333334f, 0.333334f, 0.333334f), 0.0f, 1.0f);
        frag_revealage->w *= p->glow_color[3];
        blend_mode_output(p->blend_mode, *frag_color, frag_revealage->w, frag_color, frag_revealage);
      }
      break;
    }
    case PGFX_PASS_RIM: {
      float weight_accum = 0.0f;
      for (int i = -p->samp_count; i <= p->samp_count; i++) {
        const float x = (float)i / (float)p->samp_count;
        const float weight = gaussian_weight(x);
        weight_accum += weight;
        const float uu = u + p->blur_dir[0] * x + p->uv_offset[0], vv = v + p->blur_dir[1] * x + p->uv_offset[1];
        V4 col = tex(src, R, uu, vv);
        if (floor_v2_nonzero(uu, vv).x != 0.0f) {
          col = v4(0, 0, 0, 0);
        }
        frag_revealage->x += col.x * weight; frag_revealage->y += col.y * weight; frag_revealage->z += col.z * weight;
      }
      *frag_revealage = v4_scale(*frag_revealage, 1.0f / weight_accum);
      if (p->first_pass) {
        /* In first pass we copy the reveal buffer, and add the masked color to it. */
        *frag_color = tex(src, R, u, v);
        const V4 col = tex(src, C, u, v);
        if (fabsf(col.x - p->mask_color[0]) < 0.05f && fabsf(col.y - p->mask_color[1]) < 0.05f &&
            fabsf(col.z - p->mask_color[2]) < 0.05f)
        {
          *frag_color = v4(1, 1, 1, 1);
        }
      }
      else {
        /* Premult by foreground alpha (alpha mask). */
        const V4 sc = tex(src, C, u, v);
        const float mask = 1.0f - fx_clampf(fx_dot3(sc, 0.333334f, 0.333334f, 0.333334f), 0.0f, 1.0f);
        /* fragRevealage is blurred shadow. */
        const float rim = fx_clampf(fx_dot3(*frag_revealage, 0.333334f, 0.333334f, 0.333334f), 0.0f, 1.0f);
        const V4 color = v4(p->rim_color[0], p->rim_color[1], p->rim_color[2], 1.0f);
        blend_mode_output(p->blend_mode, color, rim * mask, frag_color, frag_revealage);
      }
      break;
    }
    case PGFX_PASS_SHADOW: {
      float weight_accum = 0.0f;
      for (int i = -p->samp_count; i <= p->samp_count; i++) {
        const float x = (float)i / (float)p->samp_count;
        const float weight = gaussian_weight(x);
        weight_accum += weight;
        /* compute_uvs(x): uv = uv.x * uvRotX + uv.y * uvRotY + uvOffset; uv += blurDir * x; wave */
        float uu = u * p->uv_rot_x[0] + v * p->uv_rot_y[0] + p->uv_offset[0];
        float vv = u * p->uv_rot_x[1] + v * p->uv_rot_y[1] + p->uv_offset[1];
        uu += p->blur_dir[0] * x;
        vv += p->blur_dir[1] * x;
        const float wave_time = uu * p->wave_dir[0] + vv * p->wave_dir[1];
        uu += sinf(wave_time + p->wave_phase) * p->wave_offset[0];
        vv += sinf(wave_time + p->wave_phase) * p->wave_offset[1];
        V4 col = tex(src, R, uu, vv);
        if (floor_v2_nonzero(uu, vv).x != 0.0f) {
          col = v4(1, 1, 1, 1);
        }
        frag_revealage->x += col.x * weight; frag_revealage->y += col.y * weight; frag_revealage->z += col.z * weight;
      }
      *frag_revealage = v4_scale(*frag_revealage, 1.0f / weight_accum);
      if (p->first_pass) {
        *frag_color = tex(src, R, u, v);
      }
      else {
        float shadow_fac = 1.0f - fx_clampf(fx_dot3(*frag_revealage, 0.333334f, 0.333334f, 0.333334f), 0.0f, 1.0f);
        /* Premult by foreground revealage (alpha under). */
        const V4 orig = tex(src, C, u, v);
        shadow_fac *= fx_clampf(fx_dot3(orig, 0.333334f, 0.333334f, 0.333334f), 0.0f, 1.0f);
        shadow_fac *= p->shadow_color[3];
        *frag_color = v4(fx_mixf(0.0f, p->shadow_color[0], shadow_fac), fx_mixf(0.0f, p->shadow_color[1], shadow_fac),
                         fx_mixf(0.0f, p->shadow_color[2], shadow_fac), shadow_fac);
        *frag_revealage = v4(orig.x * (1.0f - shadow_fac), orig.y * (1.0f - shadow_fac), orig.z * (1.0f - shadow_fac), 1.0f);
      }
      break;
    }
    default:
      break;
  }
}

static void copy_plane(float *dst, const float *src, size_t n)
{
  memcpy(dst, src, n * 4u * sizeof(float));
}

int pg_fx_run_cpu(const PGFxEntry *entries, int count, const PGFxView *view, PGFxImage *image)
{
  if (entries == NULL || view == NULL || image == NULL || image->color == NULL) {
    return 0;
  }
  const size_t pixels = (size_t)image->width * (size_t)image->height;
  /* Three buffer sets like the GL engine: the pass target must differ from its source and from the
   * buffer it blends onto. */
  PGFxImage bufs[3];
  for (int i = 0; i < 3; i++) {
    bufs[i] = pg_fx_image_new(image->width, image->height);
  }
  copy_plane(bufs[0].color, image->color, pixels);
  copy_plane(bufs[0].reveal, image->reveal, pixels);
  int cur = 0, total = 0;
  for (int e = 0; e < count; e++) {
    PGFxPass passes[PG_FX_MAX_PASSES];
    const int n = pg_fx_build_passes(&entries[e], view, passes);
    if (n == 0) {
      continue;
    }
    const int start = cur;
    int prev_src = cur;
    for (int k = 0; k < n; k++) {
      const PGFxPass *p = &passes[k];
      const int src = (p->src == PGFX_SRC_PREV_SRC) ? prev_src : cur;
      const int dst = (p->dst == PGFX_DST_START) ? start : (p->dst == PGFX_DST_PREV ? cur : -1);
      int target = 0;
      while (target == src || target == dst) {
        target++;
      }
      for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
          const float u = ((float)x + 0.5f) / (float)image->width;
          const float v = ((float)y + 0.5f) / (float)image->height;
          V4 fc, fr;
          run_pixel(p, &bufs[src], u, v, &fc, &fr);
          if (dst >= 0) {
            const size_t o = ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
            const V4 dc = v4(bufs[dst].color[o], bufs[dst].color[o + 1], bufs[dst].color[o + 2], bufs[dst].color[o + 3]);
            const V4 dr = v4(bufs[dst].reveal[o], bufs[dst].reveal[o + 1], bufs[dst].reveal[o + 2], bufs[dst].reveal[o + 3]);
            fc = apply_blend(p->blend, fc, dc);
            fr = apply_blend(p->blend, fr, dr);
          }
          const size_t o = ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
          /* the 8-bit buffers of the GL engine clamp to 0..1 */
          bufs[target].color[o] = fx_clampf(fc.x, 0, 1); bufs[target].color[o + 1] = fx_clampf(fc.y, 0, 1);
          bufs[target].color[o + 2] = fx_clampf(fc.z, 0, 1); bufs[target].color[o + 3] = fx_clampf(fc.w, 0, 1);
          bufs[target].reveal[o] = fx_clampf(fr.x, 0, 1); bufs[target].reveal[o + 1] = fx_clampf(fr.y, 0, 1);
          bufs[target].reveal[o + 2] = fx_clampf(fr.z, 0, 1); bufs[target].reveal[o + 3] = fx_clampf(fr.w, 0, 1);
        }
      }
      prev_src = cur;
      cur = target;
      total++;
    }
  }
  copy_plane(image->color, bufs[cur].color, pixels);
  copy_plane(image->reveal, bufs[cur].reveal, pixels);
  for (int i = 0; i < 3; i++) {
    pg_fx_image_free(&bufs[i]);
  }
  return total;
}

void pg_fx_composite_cpu(const PGFxImage *image, float *frame_rgb)
{
  const size_t n = (size_t)image->width * (size_t)image->height;
  for (size_t i = 0; i < n; i++) {
    for (int c = 0; c < 3; c++) {
      frame_rgb[i * 3 + c] = frame_rgb[i * 3 + c] * image->reveal[i * 4 + c] + image->color[i * 4 + c];
    }
  }
}
