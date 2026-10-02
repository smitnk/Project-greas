/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Project Grease entry point for the Blender 3.6.23 Legacy GP selection
 * operators (source/blender/editors/gpencil_legacy/gpencil_select.c,
 * ED_gpencil_select_toggle_all() from gpencil_utils.c and the eSelectOp helpers
 * from editors/util/select_utils.c).
 *
 * All functions work on the real bGPdata layout. Coordinates are Project Grease
 * canvas pixels (the "screen space" of Blender's tests is the canvas itself).
 * `only_layer` limits the operator to one layer (NULL = every editable layer,
 * which is Blender's behavior).
 *
 * Each function returns 1 when the selection changed ("changed" in Blender),
 * 0 otherwise.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDlayer;

/* Values of Blender's SEL_* actions (ED_select_utils.h). */
enum {
  PG_SEL_TOGGLE = 0,
  PG_SEL_SELECT = 1,
  PG_SEL_DESELECT = 2,
  PG_SEL_INVERT = 3,
};

/* Values of Blender's eSelectOp (ED_select_utils.h). */
enum {
  PG_SEL_OP_ADD = 1,
  PG_SEL_OP_SUB = 2,
  PG_SEL_OP_SET = 3,
  PG_SEL_OP_AND = 4,
  PG_SEL_OP_XOR = 5,
};

/* Values of Blender's eGP_Selectmode. Segment mode is not ported yet. */
enum {
  PG_SELECTMODE_POINT = 0,
  PG_SELECTMODE_STROKE = 1,
};

/* GPENCIL_OT_select_grouped "type". */
enum {
  PG_SELECT_GROUPED_LAYER = 0,
  PG_SELECT_GROUPED_MATERIAL = 1,
};

/* Commands understood by pg_gp_select_dispatch() (JNI applyEditCommand ids). */
enum {
  PG_SELECT_CMD_ALL = 20,       /* args: action */
  PG_SELECT_CMD_LINKED = 21,    /* args: - */
  PG_SELECT_CMD_ALTERNATE = 22, /* args: unselect_ends */
  PG_SELECT_CMD_MORE = 23,      /* args: - */
  PG_SELECT_CMD_LESS = 24,      /* args: - */
  PG_SELECT_CMD_FIRST = 25,     /* args: only_selected_strokes, extend */
  PG_SELECT_CMD_LAST = 26,      /* args: only_selected_strokes, extend */
  PG_SELECT_CMD_GROUPED = 27,   /* args: type */
  PG_SELECT_CMD_LASSO = 28,     /* args: sel_op, selectmode, x0, y0, x1, y1, ... (>= 3 points) */
  PG_SELECT_CMD_BOX = 29,       /* args: sel_op, selectmode, xmin, ymin, xmax, ymax */
  PG_SELECT_CMD_CIRCLE = 30,    /* args: sel_op, selectmode, x, y, radius, is_first */
};

/* ED_gpencil_select_toggle_all(): action is a PG_SEL_* value. */
int pg_gp_select_all(struct bGPdata *gpd, int action, const struct bGPDlayer *only_layer);
int pg_gp_select_linked(struct bGPdata *gpd, const struct bGPDlayer *only_layer);
int pg_gp_select_alternate(struct bGPdata *gpd, int unselect_ends, const struct bGPDlayer *only_layer);
int pg_gp_select_more(struct bGPdata *gpd, const struct bGPDlayer *only_layer);
int pg_gp_select_less(struct bGPdata *gpd, const struct bGPDlayer *only_layer);
int pg_gp_select_first(struct bGPdata *gpd,
                       int only_selected_strokes,
                       int extend,
                       const struct bGPDlayer *only_layer);
int pg_gp_select_last(struct bGPdata *gpd,
                      int only_selected_strokes,
                      int extend,
                      const struct bGPDlayer *only_layer);
int pg_gp_select_grouped(struct bGPdata *gpd, int type, const struct bGPDlayer *only_layer);

/* Lasso/box/circle select with Blender's eSelectOp semantics. */
int pg_gp_select_lasso(struct bGPdata *gpd,
                       const int (*coords)[2],
                       int coords_len,
                       int sel_op,
                       int selectmode,
                       const struct bGPDlayer *only_layer);
int pg_gp_select_box(struct bGPdata *gpd,
                     int xmin,
                     int ymin,
                     int xmax,
                     int ymax,
                     int sel_op,
                     int selectmode,
                     const struct bGPDlayer *only_layer);
/* `is_first` is ED_select_op_modal(): for SET, only the first dab replaces the selection. */
int pg_gp_select_circle(struct bGPdata *gpd,
                        int mx,
                        int my,
                        int radius,
                        int sel_op,
                        int is_first,
                        int selectmode,
                        const struct bGPDlayer *only_layer);

/* Direct access to the verbatim eSelectOp helpers (used by tests and Kotlin parity). */
int pg_select_op_action(int sel_op, int is_select, int is_inside);
int pg_select_op_action_deselected(int sel_op, int is_select, int is_inside);
int pg_select_op_modal(int sel_op, int is_first);

/* JNI applyEditCommand entry. Returns 1 when the selection changed. Tags the
 * GP cache dirty when it did. */
int pg_gp_select_dispatch(struct bGPdata *gpd,
                          struct bGPDlayer *active_layer,
                          int command,
                          const float *args,
                          int arg_count);

#ifdef __cplusplus
}
#endif
