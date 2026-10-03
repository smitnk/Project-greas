/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Native tool session: the interactive tools run here on the real Legacy GP data, one call per
 * input batch (JNI nativeToolSamples). Kotlin only captures touch, including every historical
 * sample, and hands the batch over; the session applies the tool sample by sample exactly like the
 * Blender operator's modal handler applies each event, tags the batch cache, and the caller
 * renders once.
 *
 *   PG_TOOL_DRAW          gpencil_paint.c input pipeline (project_grease_draw_input.c) feeding the
 *                         stroke buffer through the draw sink (begin / add point / end).
 *   PG_TOOL_SCULPT        gpencil_sculpt_paint.c brushes (project_grease_tool_sculpt.c)
 *   PG_TOOL_VERTEX_PAINT  gpencil_vertex_paint.c brushes (project_grease_tool_vertex_paint.c)
 *   PG_TOOL_WEIGHT_PAINT  gpencil_weight_paint.c brushes (project_grease_tool_weight_paint.c)
 *
 * Samples: PG_TOOL_SAMPLE_STRIDE floats each (x, y in canvas units, pressure 0..1, time in
 * seconds). On PG_TOOL_PHASE_BEGIN the floats after the samples are the tool parameters
 * (PG_TOOL_P_* for the brush tools, PG_DRAW_P_* for Draw).
 */
#ifndef PROJECT_GREASE_TOOL_SESSION_H
#define PROJECT_GREASE_TOOL_SESSION_H

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;

enum { PG_TOOL_PHASE_BEGIN = 0, PG_TOOL_PHASE_MOVE = 1, PG_TOOL_PHASE_END = 2, PG_TOOL_PHASE_CANCEL = 3 };
enum { PG_TOOL_DRAW = 0, PG_TOOL_SCULPT = 1, PG_TOOL_VERTEX_PAINT = 2, PG_TOOL_WEIGHT_PAINT = 3 };
#define PG_TOOL_SAMPLE_STRIDE 4

/* Brush tool parameters (sculpt / vertex paint / weight paint). */
enum {
  PG_TOOL_P_BRUSH = 0,   /* GPSCULPT_TOOL_* / GPVERTEX_TOOL_* / GPWEIGHT_TOOL_* */
  PG_TOOL_P_RADIUS,      /* canvas units */
  PG_TOOL_P_STRENGTH,    /* 0..1 */
  PG_TOOL_P_PX_PER_UNIT, /* on-screen pixels per canvas unit */
  PG_TOOL_P_INVERT,      /* 0 / 1 */
  PG_TOOL_P_R,
  PG_TOOL_P_G,
  PG_TOOL_P_B,
  PG_TOOL_P_TARGET,      /* vertex paint GPPAINT_MODE_*; weight paint vertex group */
  PG_TOOL_P_WEIGHT,      /* weight paint target weight */
  PG_TOOL_P_SEED,
  PG_TOOL_P_COUNT
};

/* Draw tool parameters: material, thickness, then PGDrawSettings in declaration order. */
enum {
  PG_DRAW_P_MATERIAL = 0,
  PG_DRAW_P_THICKNESS,
  PG_DRAW_P_STRENGTH,
  PG_DRAW_P_USE_PRESSURE,
  PG_DRAW_P_USE_STRENGTH_PRESSURE,
  PG_DRAW_P_PRESSURE_CURVE,
  PG_DRAW_P_STRENGTH_CURVE,
  PG_DRAW_P_ACTIVE_SMOOTH,
  PG_DRAW_P_INPUT_SAMPLES,
  PG_DRAW_P_LAZY,
  PG_DRAW_P_LAZY_RADIUS,
  PG_DRAW_P_LAZY_FACTOR,
  PG_DRAW_P_DISABLE_STABILIZER,
  PG_DRAW_P_MANHATTAN,
  PG_DRAW_P_EUCLIDEAN,
  PG_DRAW_P_JITTER,
  PG_DRAW_P_ANGLE_FACTOR,
  PG_DRAW_P_ANGLE,
  PG_DRAW_P_FAKE_POINTS,
  PG_DRAW_P_COUNT
};

/* Where Draw puts its points: the backend's stroke buffer (sbuffer). Return 0 on failure. */
typedef struct PGToolDrawSink {
  void *user;
  int (*begin)(void *user, int material, float thickness);
  int (*add_point)(void *user, float x, float y, float pressure, float strength, float time);
  int (*end)(void *user);
  void (*cancel)(void *user);
} PGToolDrawSink;

typedef struct PGToolSession PGToolSession;

PGToolSession *pg_tool_session_new(void);
void pg_tool_session_free(PGToolSession *s);

/* Result bits of pg_tool_session_samples(). */
enum {
  PG_TOOL_RESULT_OK = 1 << 0,      /* the batch was accepted */
  PG_TOOL_RESULT_CHANGED = 1 << 1, /* document data or the stroke buffer changed: render */
  PG_TOOL_RESULT_ENDED = 1 << 2,   /* the gesture ended and changed the document (undo step) */
};

/* Applies one input batch. `samples` holds count * PG_TOOL_SAMPLE_STRIDE floats, then (BEGIN
 * only) n_params parameters. A BEGIN while a gesture is open ends that gesture first. */
int pg_tool_session_samples(PGToolSession *s, struct bGPdata *gpd, int tool, const float *samples,
                            int count, int phase, const float *params, int n_params,
                            const PGToolDrawSink *sink);

/* Number of samples the session has applied in the current / last gesture (tests, diagnostics). */
int pg_tool_session_sample_count(const PGToolSession *s);

#ifdef __cplusplus
}
#endif

#endif
