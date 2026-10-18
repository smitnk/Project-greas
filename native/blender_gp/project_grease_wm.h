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
} PGOpContext;

/* WM_operator_call_ex: unknown idname or failed poll -> CANCELLED; else exec's status.
 * r_flag (optional) receives the type's flags (OPTYPE_UNDO: push an undo step on FINISHED). */
int pg_wm_operator_call(PGOpContext *C, const char *idname, const float *props, int prop_count, int *r_flag);
/* The poll alone (WM_operator_poll), e.g. to grey out a button. */
bool pg_wm_operator_poll(const PGOpContext *C, const char *idname);

#ifdef __cplusplus
}
#endif
