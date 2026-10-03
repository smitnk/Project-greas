/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Legacy GP shader effects as a 2D post-pass for Project Grease.
 *
 * Blender 3.6.23 stores shader effects on the object and renders them in the GP draw engine
 * (draw/engines/gpencil/gpencil_shader_fx.c builds the passes and their uniforms,
 * shaders/gpencil_vfx_frag.glsl is the per-pixel math). Project Grease has no object, camera or
 * depsgraph, so the adaptation is:
 *   - the effect list is per LAYER (PGFxEntry {type, enabled, params}), saved with the document;
 *   - a layer that has enabled effects is drawn into an offscreen buffer, the effects run in list
 *     order on it (ping-pong buffers), and the result is composited onto the frame; layers without
 *     effects keep the direct path;
 *   - sizes (blur radius, pixel size, shadow offset, wave period/amplitude, swirl radius, rim
 *     offset) are CANVAS PIXELS; the object-to-pixel factor of Blender (distance_factor) becomes the
 *     canvas-to-viewport scale, so effects scale with zoom like the drawing does;
 *   - where Blender uses the object's origin (pixelate grid, wave/shadow phase and rotation
 *     center) the canvas center is used; the swirl center is a parameter (fraction of the canvas)
 *     because Blender takes it from another object;
 *   - screen directions follow Blender's: +x right, +y up on screen; angles are radians.
 *
 * Blender renders each layer into a colour buffer C (premultiplied rgb) and a revealage buffer R
 * (per-channel transmittance, 1 - alpha for plain strokes). Effects read and write both; the
 * result is composited as `frame = frame * R + C`. This module keeps that representation.
 *
 * The data model, the translation of parameters to pass uniforms (pg_fx_build_passes) and a CPU
 * executor of the per-pixel math (pg_fx_run_cpu, a C port of gpencil_vfx_frag.glsl used by the host
 * tests to pin the formulas) live here without any GL. android_gp_shader_fx.cpp runs the same
 * passes with GLSL ES shaders.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ShaderFxType values of DNA_shader_fx_types.h. */
enum {
  PG_FX_NONE = 0,
  PG_FX_BLUR = 1,
  PG_FX_FLIP = 2,
  PG_FX_PIXEL = 4,
  PG_FX_SWIRL = 5,
  PG_FX_WAVE = 6,
  PG_FX_RIM = 7,
  PG_FX_COLORIZE = 8,
  PG_FX_SHADOW = 9,
  PG_FX_GLOW = 10,
};

#define PG_FX_MAX_PARAMS 24
#define PG_FX_MAX_STACK 16

/* Parameter indices per type (flat floats; booleans 0/1, enums and counts stored as floats). */
enum { /* BlurShaderFxData (DOF mode is not supported: there is no camera) */
  PG_FXP_BLUR_RADIUS_X = 0, PG_FXP_BLUR_RADIUS_Y = 1, PG_FXP_BLUR_SAMPLES = 2,
  PG_FXP_BLUR_ROTATION = 3, PG_FXP_BLUR_COUNT = 4 };
enum { /* mode: 0 grayscale, 1 sepia, 2 duotone, 3 custom, 4 transparent */
  PG_FXP_COLORIZE_MODE = 0, PG_FXP_COLORIZE_LOW = 1 /* rgb */, PG_FXP_COLORIZE_HIGH = 4 /* rgb */,
  PG_FXP_COLORIZE_FACTOR = 7, PG_FXP_COLORIZE_COUNT = 8 };
enum { PG_FXP_FLIP_HORIZONTAL = 0, PG_FXP_FLIP_VERTICAL = 1, PG_FXP_FLIP_COUNT = 2 };
enum { /* GlowShaderFxData: mode 0 luminance / 1 color; blend_mode is eGplBlendMode (0 regular, 2 add, 3 sub, 4 mul, 5 divide) */
  PG_FXP_GLOW_COLOR = 0 /* rgba */, PG_FXP_GLOW_SELECT = 4 /* rgb */, PG_FXP_GLOW_THRESHOLD = 7,
  PG_FXP_GLOW_MODE = 8, PG_FXP_GLOW_BLUR_X = 9, PG_FXP_GLOW_BLUR_Y = 10, PG_FXP_GLOW_SAMPLES = 11,
  PG_FXP_GLOW_ROTATION = 12, PG_FXP_GLOW_BLEND = 13, PG_FXP_GLOW_USE_ALPHA = 14,
  PG_FXP_GLOW_COUNT = 15 };
enum { PG_FXP_PIXEL_SIZE_X = 0, PG_FXP_PIXEL_SIZE_Y = 1, PG_FXP_PIXEL_NEAREST = 2,
       PG_FXP_PIXEL_COUNT = 3 };
enum { /* mode: 0 normal, 1 overlay, 2 add, 3 subtract, 4 multiply, 5 divide */
  PG_FXP_RIM_OFFSET_X = 0, PG_FXP_RIM_OFFSET_Y = 1, PG_FXP_RIM_COLOR = 2 /* rgb */,
  PG_FXP_RIM_MASK = 5 /* rgb */, PG_FXP_RIM_MODE = 8, PG_FXP_RIM_BLUR_X = 9, PG_FXP_RIM_BLUR_Y = 10,
  PG_FXP_RIM_SAMPLES = 11, PG_FXP_RIM_COUNT = 12 };
enum { /* orientation: 0 horizontal, 1 vertical; the shadow pivot is the canvas center */
  PG_FXP_SHADOW_OFFSET_X = 0, PG_FXP_SHADOW_OFFSET_Y = 1, PG_FXP_SHADOW_COLOR = 2 /* rgba */,
  PG_FXP_SHADOW_USE_WAVE = 6, PG_FXP_SHADOW_AMPLITUDE = 7, PG_FXP_SHADOW_PERIOD = 8,
  PG_FXP_SHADOW_PHASE = 9, PG_FXP_SHADOW_ORIENTATION = 10, PG_FXP_SHADOW_SCALE_X = 11,
  PG_FXP_SHADOW_SCALE_Y = 12, PG_FXP_SHADOW_ROTATION = 13, PG_FXP_SHADOW_BLUR_X = 14,
  PG_FXP_SHADOW_BLUR_Y = 15, PG_FXP_SHADOW_SAMPLES = 16, PG_FXP_SHADOW_COUNT = 17 };
enum { /* center is a fraction of the canvas (Blender takes the center from another object) */
  PG_FXP_SWIRL_CENTER_X = 0, PG_FXP_SWIRL_CENTER_Y = 1, PG_FXP_SWIRL_RADIUS = 2,
  PG_FXP_SWIRL_ANGLE = 3, PG_FXP_SWIRL_COUNT = 4 };
enum { PG_FXP_WAVE_AMPLITUDE = 0, PG_FXP_WAVE_PERIOD = 1, PG_FXP_WAVE_PHASE = 2,
       PG_FXP_WAVE_ORIENTATION = 3, PG_FXP_WAVE_COUNT = 4 };

typedef struct PGFxEntry {
  int type;    /* PG_FX_* */
  int enabled; /* 0 = skipped */
  int target;  /* PG_FX_TARGET_*: what of the layer the effect applies to */
  float params[PG_FX_MAX_PARAMS];
} PGFxEntry;

/* Effect target: the whole layer (Blender's behaviour), only its strokes or only its fills (the layer is
 * then rendered in two passes, strokes and fills, and the effect runs on its pass only). */
enum { PG_FX_TARGET_LAYER = 0, PG_FX_TARGET_STROKES = 1, PG_FX_TARGET_FILLS = 2 };

int pg_fx_valid_type(int type);
int pg_fx_param_count(int type);
const char *pg_fx_name(int type);
/* Blender's initData() defaults for the type (rest zeroed). */
void pg_fx_defaults(int type, float params[PG_FX_MAX_PARAMS]);
/* Clamps enums/counts, replaces non-finite values by the defaults. */
void pg_fx_sanitize(int type, float params[PG_FX_MAX_PARAMS]);
void pg_fx_entry_init(PGFxEntry *entry, int type);

/* ---------------------------------------------------------------------------------------- */
/* Passes                                                                                    */

/* Where the layer is drawn on the frame: viewport size and the canvas->viewport mapping
 * (viewport = origin + canvas * scale, y down from the top like the presenter's). */
typedef struct PGFxView {
  int width, height;           /* viewport (= offscreen buffer) size in pixels */
  float scale;                 /* viewport pixels per canvas pixel */
  float origin_x, origin_y;    /* viewport position of canvas (0, 0) */
  int canvas_w, canvas_h;
} PGFxView;

enum { PGFX_PASS_BLUR, PGFX_PASS_COLORIZE, PGFX_PASS_TRANSFORM, PGFX_PASS_PIXELIZE, PGFX_PASS_GLOW,
       PGFX_PASS_RIM, PGFX_PASS_SHADOW };
/* GL blend state of the pass: `out = f(src, dst)` on both buffers (DRW_STATE_BLEND_*). */
enum { PGFX_BLEND_NONE = 0, PGFX_BLEND_PREMUL, PGFX_BLEND_ADD, PGFX_BLEND_SUB, PGFX_BLEND_MUL };
/* Which buffer a pass reads and blends onto, in Blender's ping-pong order. */
enum { PGFX_SRC_PREV = 0,      /* the previous pass's result */
       PGFX_SRC_PREV_SRC = 1 };/* the source of the previous pass (Rim overlay's second blend) */
enum { PGFX_DST_NONE = 0, PGFX_DST_START = 1, /* the buffers before the effect started */
       PGFX_DST_PREV = 2 };                   /* the previous pass's result */

typedef struct PGFxPass {
  int kind, blend, src, dst;
  /* uniforms (names follow gpencil_vfx_frag.glsl); all uvs are 0..1 over the viewport */
  float offset[2];
  int samp_count;
  float low_color[3], high_color[3], factor;
  int mode;
  float axis_flip[2], wave_dir[2], wave_offset[2], wave_phase;
  float swirl_radius, swirl_center[2] /* viewport pixels */, swirl_angle;
  float target_pixel_size[2], target_pixel_offset[2], accum_offset[2];
  float threshold[4], glow_color[4];
  int glow_under, first_pass, blend_mode;
  float blur_dir[2], uv_offset[2], uv_rot_x[2], uv_rot_y[2];
  float mask_color[3], rim_color[3], shadow_color[4];
} PGFxPass;

#define PG_FX_MAX_PASSES 4

/* The passes of one effect (gpencil_vfx_* of gpencil_shader_fx.c); 0 when the effect does nothing. */
int pg_fx_build_passes(const PGFxEntry *entry, const PGFxView *view, PGFxPass out[PG_FX_MAX_PASSES]);

/* ---------------------------------------------------------------------------------------- */
/* CPU executor (tests): the same per-pixel math as the GLSL shaders on float images.        */

typedef struct PGFxImage {
  int width, height;
  float *color;   /* rgba per pixel: premultiplied colour, alpha */
  float *reveal;  /* rgba per pixel: per-channel revealage, a */
} PGFxImage;

PGFxImage pg_fx_image_new(int width, int height);
void pg_fx_image_free(PGFxImage *image);
/* A layer buffer from a premultiplied rgba image: C = rgba, R = (1 - a, 1 - a, 1 - a, 1). */
void pg_fx_image_from_premult(PGFxImage *image, const float *premult_rgba);
/* Runs the enabled effects on `image` in place; returns the number of passes run. */
int pg_fx_run_cpu(const PGFxEntry *entries, int count, const PGFxView *view, PGFxImage *image);
/* The composite onto a frame: frame.rgb = frame.rgb * R.rgb + C.rgb. */
void pg_fx_composite_cpu(const PGFxImage *image, float *frame_rgb);

#ifdef __cplusplus
}
#endif
