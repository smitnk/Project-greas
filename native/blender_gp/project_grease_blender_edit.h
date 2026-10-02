/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Selection-aware editing for Project Grease on the Blender 3.6.23 Legacy GP data model:
 *  - click selection (gpencil_select_exec in editors/gpencil_legacy/gpencil_select.c)
 *  - transforming the selected points of every selected stroke about the selection median
 *  - deleting selected strokes / selected points
 *
 * Coordinates are canvas pixels. `only_layer` limits an operator to one layer (NULL = every
 * editable layer). Functions return 1 when they changed the document, 0 otherwise.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDlayer;

/* flags of pg_gp_edit_pick(); the first five are the properties of GPENCIL_OT_select. */
enum {
  PG_PICK_EXTEND = 1,       /* extend instead of replacing the selection */
  PG_PICK_DESELECT = 2,     /* deselect the picked element */
  PG_PICK_TOGGLE = 4,       /* toggle the picked element */
  PG_PICK_ENTIRE = 8,       /* "entire_strokes" */
  PG_PICK_DESELECT_ALL = 16, /* clicking nothing deselects everything */
  PG_PICK_PASSTHROUGH = 32, /* picking something already selected changes nothing */
};

/* applyEditCommand ids (selection operators use 20..30, see project_grease_blender_select.h). */
enum {
  PG_EDIT_CMD_PICK = 31,          /* args: x, y, radius_squared, flags, selectmode */
  PG_EDIT_CMD_TRANSLATE = 32,     /* args: dx, dy */
  PG_EDIT_CMD_ROTATE = 33,        /* args: radians [, cx, cy] */
  PG_EDIT_CMD_SCALE = 34,         /* args: sx, sy [, cx, cy] */
  PG_EDIT_CMD_MIRROR = 35,        /* args: mirror_x, mirror_y [, cx, cy] */
  PG_EDIT_CMD_DELETE_STROKES = 36, /* args: - */
  PG_EDIT_CMD_DELETE_POINTS = 37,  /* args: - */
};
#define PG_EDIT_CMD_FIRST 31
#define PG_EDIT_CMD_LAST 37

/* GP_SELECTMODE_* values accepted by pg_gp_edit_pick(): 0 = point, 1 = stroke. */
int pg_gp_edit_pick(struct bGPdata *gpd,
                    const struct bGPDlayer *only_layer,
                    float x,
                    float y,
                    int radius_squared,
                    int flags,
                    int selectmode);

/* Median of all selected points (editable strokes with GP_STROKE_SELECT). 0 when nothing is selected. */
int pg_gp_edit_selection_pivot(const struct bGPdata *gpd,
                               const struct bGPDlayer *only_layer,
                               float *r_x,
                               float *r_y);

/* Transforms of the selected points. Without an explicit pivot the selection median is used. */
int pg_gp_edit_translate(struct bGPdata *gpd, const struct bGPDlayer *only_layer, float dx, float dy);
int pg_gp_edit_rotate(struct bGPdata *gpd,
                      const struct bGPDlayer *only_layer,
                      float radians,
                      const float *pivot_xy /* may be NULL */);
int pg_gp_edit_scale(struct bGPdata *gpd,
                     const struct bGPDlayer *only_layer,
                     float scale_x,
                     float scale_y,
                     const float *pivot_xy /* may be NULL */);
int pg_gp_edit_mirror(struct bGPdata *gpd,
                      const struct bGPDlayer *only_layer,
                      int mirror_x,
                      int mirror_y,
                      const float *pivot_xy /* may be NULL */);

int pg_gp_edit_delete_strokes(struct bGPdata *gpd, const struct bGPDlayer *only_layer);
int pg_gp_edit_delete_points(struct bGPdata *gpd, const struct bGPDlayer *only_layer);

/* JNI applyEditCommand entry for ids 31..37; tags the GP cache dirty on change. */
int pg_gp_edit_dispatch(struct bGPdata *gpd,
                        struct bGPDlayer *active_layer,
                        int command,
                        const float *args,
                        int arg_count);

#ifdef __cplusplus
}
#endif
