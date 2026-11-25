/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Draw tool input: touch samples to stroke points by Blender 3.6.23's own gpencil_paint.c code
 * (project_grease_blender_paint.c: gpencil_draw_apply, gpencil_stroke_filtermval,
 * gpencil_stroke_addpoint with pressure / strength curves, jitter, randomness, angle and active
 * smoothing, gpencil_add_fake_points / gpencil_add_arc_points, gpencil_smooth_segment and the
 * trailing low-pressure truncation of gpencil_stroke_newfrombuffer). Checked against a real
 * Blender run (tools/draw_input_golden/blender_draw_reference.py).
 *
 * This file is the canvas adapter: region pixels are (x, -y) * px_per_unit, and Blender edits
 * its sbuffer in place (arcs replace the last point, smoothing rewrites up to the last four), so
 * points are released only once no later event can change them; pg_draw_input_end() flushes
 * the rest. The released stream equals the final buffer exactly.
 */
#ifndef PROJECT_GREASE_DRAW_INPUT_H
#define PROJECT_GREASE_DRAW_INPUT_H

#include "project_grease_curvemap.h"

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
  float pressure_curve;      /* ignored: Blender has no power curve, use pressure_map */
  float strength_curve;      /* ignored: use strength_map */
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
  float px_per_unit;          /* 1: region pixels per canvas unit */
  int use_seed;               /* 0: seed like gpencil_paint_initstroke() */
  unsigned int seed;
  float random_press;         /* 0: draw_random_press */
  float random_strength;      /* 0: draw_random_strength */
  float uv_random;            /* 0: uv_random */
  unsigned int random_flag2;  /* 0: BrushGpencilSettings.flag2 (GP_BRUSH_USE_*_AT_STROKE, ...) */
  /* Brush curve_sensitivity / curve_strength; the linear default curve while !built. */
  PGCurve pressure_map;
  PGCurve strength_map;
} PGDrawSettings;

typedef struct PGDrawPoint {
  float x, y, pressure, strength, time;
} PGDrawPoint;

typedef struct PGDrawInput {
  PGDrawSettings settings;
  int active;
  /* Blender's sbuffer converted to canvas units (valid up to `used`). */
  PGDrawPoint *buffer;
  int used, capacity;
  int released;
  struct PGPaintSession *paint; /* project_grease_blender_paint.c */
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
/* BLI_rng_new(seed) / BLI_rng_get_float() (blender::RandomNumberGenerator). */
void pg_draw_rng_seed(unsigned long long *x, unsigned int seed);
float pg_draw_rng_get_float(unsigned long long *x);
/* BLI_math_base interpf(): `target` weighted by `fac`. */
float pg_draw_interpf(float target, float origin, float fac);

#ifdef __cplusplus
}
#endif

#endif
