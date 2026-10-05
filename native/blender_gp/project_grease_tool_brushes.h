/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Brush ports driven by the native tool session (project_grease_tool_session.h):
 *   project_grease_tool_sculpt.c        gpencil_sculpt_paint.c
 *   project_grease_tool_vertex_paint.c  gpencil_vertex_paint.c
 *   project_grease_tool_weight_paint.c  gpencil_weight_paint.c
 * Each begin() sets up the operator's brush context (Blender's preset for the tool, the UI's size
 * and strength), each sample() is one gpencil_*_brush_apply() call for one input event, and end()
 * does what the operator's exit does. Coordinates are canvas units; radius is in canvas units and
 * converted to pixels with px_per_unit like the screen-space operators.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;

typedef struct PGToolBrushParams {
  int brush;          /* GPSCULPT_TOOL_* / GPVERTEX_TOOL_* / GPWEIGHT_TOOL_* */
  float radius;       /* canvas units (brush->size = radius * px_per_unit) */
  float strength;     /* brush->alpha (sculpt, weight) or draw_strength (vertex paint) */
  float px_per_unit;  /* on-screen pixels per canvas unit */
  int invert;         /* pen flip / Ctrl */
  float rgb[3];       /* vertex paint color (sRGB, converted to linear like the operator) */
  int target;         /* vertex paint: GPPAINT_MODE_STROKE/FILL/BOTH; weight paint: vertex group */
  float weight;       /* weight paint target weight (brush->weight) */
  unsigned int seed;  /* RNG seed (randomize brush) */
  int automask;       /* sculpt: GP_SCULPT_SETT_FLAG_AUTOMASK_* */
  int select_mask;    /* GP_SCULPT_MASK_SELECTMODE_* (sculpt) / GP_VERTEX_MASK_SELECTMODE_* (vertex paint) */
  int curve_preset;   /* eBrushCurvePreset, 0 keeps the tool default */
  int active_material;/* 0-based */
} PGToolBrushParams;

typedef struct PGSculptSession PGSculptSession;
PGSculptSession *pg_sculpt_session_begin(struct bGPdata *gpd, const PGToolBrushParams *params);
/* One input event; returns 1 when strokes changed. */
int pg_sculpt_session_sample(PGSculptSession *s, float x, float y, float pressure);
void pg_sculpt_session_end(PGSculptSession *s);

typedef struct PGVertexPaintSession PGVertexPaintSession;
PGVertexPaintSession *pg_vpaint_session_begin(struct bGPdata *gpd, const PGToolBrushParams *params);
int pg_vpaint_session_sample(PGVertexPaintSession *s, float x, float y, float pressure);
void pg_vpaint_session_end(PGVertexPaintSession *s);

typedef struct PGWeightPaintSession PGWeightPaintSession;
PGWeightPaintSession *pg_wpaint_session_begin(struct bGPdata *gpd, const PGToolBrushParams *params);
int pg_wpaint_session_sample(PGWeightPaintSession *s, float x, float y, float pressure);
void pg_wpaint_session_end(PGWeightPaintSession *s);

#ifdef __cplusplus
}
#endif
