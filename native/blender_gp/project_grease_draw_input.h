/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Input side of the Blender 3.6.23 Legacy GP draw operator (gpencil_paint.c), run natively by the
 * tool session for the Draw tool. Traced functions: gpencil_stroke_filtermval, gpencil_draw_apply
 * (lazy mouse), gpencil_stroke_addpoint, gpencil_brush_jitter, gpencil_brush_angle,
 * gpencil_brush_angle_segment, gpencil_smooth_buffer, gpencil_smooth_segment,
 * gpencil_add_fake_points, gpencil_add_arc_points and the trailing low-pressure truncation of
 * gpencil_stroke_newfrombuffer.
 *
 * Blender edits its stroke buffer in place: an arc REPLACES the last buffered point and the
 * smoothing passes rewrite up to the last four points. The native sbuffer is filled append-only,
 * so the engine keeps the newest points private until no later event can change them and only
 * then releases them; pg_draw_input_end() flushes the rest. The released stream equals the final
 * buffer exactly.
 *
 * This is the former Kotlin LegacyGpBrushStrokeEngine moved to native unchanged (same float
 * operations); tools/draw_input_golden holds that Kotlin reference and the generator of the
 * golden data the native test compares against. Known approximations (as before): power curves
 * instead of CurveMapping, a local random generator for jitter, interpolated arc point times.
 */
#ifndef PROJECT_GREASE_DRAW_INPUT_H
#define PROJECT_GREASE_DRAW_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#define PG_DRAW_MAX_INPUT_SAMPLES 10       /* GP_MAX_INPUT_SAMPLES */
#define PG_DRAW_ALPHA_OPACITY_THRESH 0.001f /* GPENCIL_ALPHA_OPACITY_THRESH */
#define PG_DRAW_STRENGTH_MIN 0.003f         /* GPENCIL_STRENGTH_MIN */
#define PG_DRAW_SMOOTH_HOLD_BACK 4

typedef struct PGDrawSettings {
  float draw_strength;       /* 1 */
  int use_pressure;          /* 1 */
  int use_strength_pressure; /* 0 */
  float pressure_curve;      /* 1 */
  float strength_curve;      /* 1 */
  float active_smooth;       /* 0 */
  int input_samples;         /* 0 */
  int lazy_enabled;          /* 0 */
  float smooth_stroke_radius;
  float smooth_stroke_factor;
  int disable_stabilizer;
  int manhattan_threshold;   /* 1 */
  float euclidean_threshold; /* 1 */
  float jitter;
  float draw_angle_factor;
  float draw_angle;
  int synthesize_fast_points; /* 1 */
} PGDrawSettings;

typedef struct PGDrawPoint {
  float x, y, pressure, strength, time;
} PGDrawPoint;

typedef struct PGDrawInput {
  PGDrawSettings settings;
  int active;
  PGDrawPoint *buffer;
  int used, capacity;
  int released;
  float last_input_x, last_input_y;
  float initial_time, previous_time;
  unsigned int rng;
} PGDrawInput;

void pg_draw_settings_default(PGDrawSettings *s);
void pg_draw_input_begin(PGDrawInput *d, const PGDrawSettings *s);
/* Feeds one sample; writes the points that became final to *r_out (owned by d, valid until the
 * next call) and returns how many. */
int pg_draw_input_add(PGDrawInput *d, float x, float y, float pressure, float time_seconds,
                      const PGDrawPoint **r_out);
int pg_draw_input_end(PGDrawInput *d, const PGDrawPoint **r_out);
void pg_draw_input_cancel(PGDrawInput *d);
void pg_draw_input_free(PGDrawInput *d);
/* BLI_math_base interpf(): `target` weighted by `fac`. */
float pg_draw_interpf(float target, float origin, float fac);

#ifdef __cplusplus
}
#endif

#endif
