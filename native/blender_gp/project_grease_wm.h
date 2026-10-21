/* SPDX-License-Identifier: GPL-2.0-or-later
 * Operator layer: Blender's wmOperatorType contract (WM_types.h) and the WM_operator_call rule
 * (poll, then exec, one status), for the Grease Pencil operators the app runs. Operators are
 * called by their Blender idname with their RNA properties as numbers, in definition order. */
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;
struct PGToolSession;
struct PGToolDrawSink;

/* WM_types.h operator return flags */
enum { PG_OPERATOR_RUNNING_MODAL = 1, PG_OPERATOR_CANCELLED = 2, PG_OPERATOR_FINISHED = 4 };
/* WM_types.h operator type flags */
enum { PG_OPTYPE_REGISTER = 1, PG_OPTYPE_UNDO = 2 };

/* bContext stand-in: what the GP operators read from the context. */
typedef struct PGOpContext {
  struct bGPdata *gpd;
  struct bGPDlayer *active_layer;
  int cfra;     /* scene->r.cfra */
  bool autokey; /* IS_AUTOKEY_ON(scene) */
  int actcol;   /* ob->actcol: active material slot, 1-based (0 = none); material operators set it */
  /* the modal paint operators' state (wmOperator::customdata) and the Draw stroke sink */
  struct PGToolSession *tool_session;
  const struct PGToolDrawSink *sink;
} PGOpContext;

/* One event of a modal paint operator (GPENCIL_OT_draw / sculpt_paint / vertex_paint /
 * weight_paint, project_grease_wm_modal.c): phase PG_TOOL_PHASE_* = invoke, modal (move),
 * modal (release: confirm), cancel; samples and params as pg_tool_session_samples. Blender's rule:
 * one modal operator runs at a time, so an invoke cancels a running one first, and events of
 * another operator are refused. Returns the tool session result (0 refused); *r_status gets
 * RUNNING_MODAL, FINISHED (confirmed with a change) or CANCELLED. */
int pg_wm_modal_tool(PGOpContext *C, int tool, int phase, const float *samples, int count,
                     const float *params, int n_params, int *r_status) __attribute__((weak));
/* GPENCIL_OT_draw in eraser mode, one drag: BEGIN (invoke) -> MOVE ... -> END (confirm; FINISHED
 * when the drag erased something) or CANCEL. The erasing itself is the backend's eraser per sample. */
int pg_wm_modal_erase(PGOpContext *C, int phase, bool changed) __attribute__((weak));
/* Colours (sRGB 0xRRGGBB, hue sorted) of the palette made by the last extract_palette_vertex or
 * material_to_vertex_color call. */
int pg_wm_last_palette(const unsigned int **r_colors) __attribute__((weak));
/* Curve edit mode (project_grease_wm_stroke.c): on / off, the curve points (13 floats each), a
 * click on a point / handle, and moving the selected ones. */
int pg_curve_edit_set(struct bGPdata *gpd, bool on) __attribute__((weak));
int pg_curve_points(const struct bGPdata *gpd, float *out, int capacity) __attribute__((weak));
int pg_curve_select(struct bGPdata *gpd, int layer, int stroke, int point, int part, bool extend) __attribute__((weak));
int pg_curve_translate(struct bGPdata *gpd, float dx, float dy) __attribute__((weak));
/* Brush weight found by the last GPENCIL_OT_weight_sample. */
float pg_wm_last_weight_sample(void) __attribute__((weak));
/* idname of the running modal operator, NULL when none. */
const char *pg_wm_modal_running(void) __attribute__((weak));

/* WM_operator_call_ex: unknown idname or failed poll -> CANCELLED; else exec's status.
 * r_flag (optional) receives the type's flags (OPTYPE_UNDO: push an undo step on FINISHED). */
int pg_wm_operator_call(PGOpContext *C, const char *idname, const float *props, int prop_count, int *r_flag);
/* The poll alone (WM_operator_poll), e.g. to grey out a button. */
bool pg_wm_operator_poll(const PGOpContext *C, const char *idname);

typedef struct PGOperatorType {
  const char *idname;
  int flag;
  bool (*poll)(const PGOpContext *C);
  int (*exec)(PGOpContext *C, const float *props, int n);
} PGOperatorType;
/* Layer operators (project_grease_wm_layer.c, built against the real BKE): looked up after the
 * operators of project_grease_wm.c. Weak, so builds without that file still link. */
const PGOperatorType *pg_wm_layer_operator_find(const char *idname) __attribute__((weak));
/* Stroke and material operators (project_grease_wm_stroke.c), likewise weak. */
const PGOperatorType *pg_wm_stroke_operator_find(const char *idname) __attribute__((weak));
/* Layer operators change the layer list: the caller re-syncs what it keeps per layer. */
bool pg_wm_operator_changes_layers(const char *idname);

#ifdef __cplusplus
}
#endif
