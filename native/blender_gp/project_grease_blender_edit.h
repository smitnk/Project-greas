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
struct bGPDstroke;

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
  /* Legacy GP modifiers applied ("baked") to the selected strokes. */
  PG_EDIT_CMD_MOD_THICKNESS = 40, /* args: normalize, thickness, thickness_factor */
  PG_EDIT_CMD_MOD_OPACITY = 41,   /* args: modify_color, factor, normalize, hardness */
  /* args: mode, start, end, overshoot, use_curvature, point_density, segment_influence,
   * max_angle, invert_curvature */
  PG_EDIT_CMD_MOD_LENGTH = 42,
  PG_EDIT_CMD_MOD_TINT = 43, /* args: vertex_mode, factor, r, g, b */
  PG_EDIT_CMD_MOD_COLOR = 44, /* args: modify_color (BOTH/STROKE/FILL), hue, saturation, value */
  /* Stroke operators of the Legacy GP editor (gpencil_edit.c / gpencil_data.c) */
  PG_EDIT_CMD_ARRANGE = 45,      /* args: direction (PG_ARRANGE_*) */
  PG_EDIT_CMD_SET_MATERIAL = 46, /* args: material index (mat_nr) */
  PG_EDIT_CMD_RESET_VCOLOR = 47, /* args: mode (PG_PAINT_MODE_*) */
  PG_EDIT_CMD_FLIP = 48,         /* args: - */
  PG_EDIT_CMD_CYCLIC = 49,       /* args: type (PG_CYCLIC_*) */
  PG_EDIT_CMD_SNAP_GRID = 50,    /* args: grid size */
  PG_EDIT_CMD_DUPLICATE = 51,    /* args: - */
  PG_EDIT_CMD_DISSOLVE = 52,     /* args: type (PG_DISSOLVE_*) */
  PG_EDIT_CMD_SPLIT = 53,        /* args: - */
  PG_EDIT_CMD_JOIN = 54,         /* args: leave_gaps */
  /* args: brush, x, y, radius, strength, r, g, b, target (PG_PAINT_MODE_*), dx, dy */
  PG_EDIT_CMD_VERTEX_PAINT = 55,
  PG_EDIT_CMD_MIRROR_COPY = 56,  /* args: axis_x, axis_y, pivot_x, pivot_y */
  PG_EDIT_CMD_WEIGHT_PAINT = 57, /* args: def_nr, x, y, radius, strength, target_weight */
};
#define PG_EDIT_CMD_FIRST 31
#define PG_EDIT_CMD_LAST 95 /* 58..65: edit2.c, 66..75: edit3.c, 76..85: edit4.c, 86..89: edit5.c (routed by the bridge) */

/* GPENCIL_OT_dissolve types */
enum { PG_DISSOLVE_POINTS = 0, PG_DISSOLVE_BETWEEN = 1, PG_DISSOLVE_UNSELECT = 2 };

/* GPENCIL_OT_duplicate: each run of selected points becomes a new selected stroke. */
int pg_gp_duplicate(struct bGPdata *gpd, const struct bGPDlayer *only_layer);
/* GPENCIL_OT_dissolve: remove points without splitting the stroke. */
int pg_gp_dissolve(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int type);
/* GPENCIL_OT_stroke_split: selected points move to new strokes. */
int pg_gp_split(struct bGPdata *gpd, const struct bGPDlayer *only_layer);
/* GPENCIL_OT_stroke_join (JOIN): selected strokes merge into the first selected one. */
int pg_gp_join(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int leave_gaps);

/* GPENCIL_OT_stroke_arrange directions */
enum { PG_ARRANGE_TOP = 0, PG_ARRANGE_UP = 1, PG_ARRANGE_DOWN = 2, PG_ARRANGE_BOTTOM = 3 };
/* GPENCIL_OT_stroke_cyclical_set types */
enum { PG_CYCLIC_CLOSE = 1, PG_CYCLIC_OPEN = 2, PG_CYCLIC_TOGGLE = 3 };

int pg_gp_stroke_arrange(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int direction);
int pg_gp_stroke_set_material(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mat_nr);
int pg_gp_stroke_reset_vertex_color(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mode);
int pg_gp_stroke_flip(struct bGPdata *gpd, const struct bGPDlayer *only_layer);
int pg_gp_stroke_cyclical_set(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int type);
int pg_gp_snap_to_grid(struct bGPdata *gpd, const struct bGPDlayer *only_layer, float grid);

/* MOD_gpencil_legacy_color.c deformStroke() (Hue/Saturation), no curve. hsv defaults 0.5, 1, 1. */
int pg_gp_mod_color(struct bGPdata *gpd, const struct bGPDlayer *only_layer,
                    int modify_color, const float hsv_factor[3]);
/* rgb_to_hsv()/hsv_to_rgb() of blenlib math_color.c, exposed for tests. */
void pg_rgb_to_hsv(const float rgb[3], float r_hsv[3]);
void pg_hsv_to_rgb(const float hsv[3], float r_rgb[3]);

/* eGp_Vertex_Mode */
enum { PG_PAINT_MODE_STROKE = 0, PG_PAINT_MODE_FILL = 1, PG_PAINT_MODE_BOTH = 2 };

/* MOD_gpencil_legacy_tint.c deformStroke(), uniform type, no vertex group or curve. */
int pg_gp_mod_tint(struct bGPdata *gpd, const struct bGPDlayer *only_layer,
                   int vertex_mode, float factor, const float rgb[3]);

/* eLengthGpencil_Type */
enum { PG_LENGTH_RELATIVE = 0, PG_LENGTH_ABSOLUTE = 1 };

typedef struct PGLengthParams {
  int mode;
  float start_fac, end_fac, overshoot_fac;
  int use_curvature;
  float point_density, segment_influence, max_angle;
  int invert_curvature;
} PGLengthParams;

/* MOD_gpencil_legacy_length.c applyLength() without the random offsets. */
int pg_gp_mod_length(struct bGPdata *gpd, const struct bGPDlayer *only_layer, const PGLengthParams *p);

/* eModifyColorGpencil_Flag (DNA_gpencil_modifier_types.h) */
enum {
  PG_MODIFY_COLOR_BOTH = 0,
  PG_MODIFY_COLOR_STROKE = 1,
  PG_MODIFY_COLOR_FILL = 2,
  PG_MODIFY_COLOR_HARDNESS = 3,
};

/* MOD_gpencil_legacy_thick.c deformStroke() without vertex groups or custom curve. */
int pg_gp_mod_thickness(struct bGPdata *gpd, const struct bGPDlayer *only_layer,
                        int normalize, int thickness, float thickness_fac);
/* MOD_gpencil_legacy_opacity.c deformStroke() without vertex groups or custom curve. */
int pg_gp_mod_opacity(struct bGPdata *gpd, const struct bGPDlayer *only_layer,
                      int modify_color, float factor, int normalize, float hardness);

/* eBezTriple_Easing / interpolation easing types used by gpencil_interpolate.c */
enum {
  PG_EASE_LINEAR = 0, PG_EASE_QUAD, PG_EASE_CUBIC, PG_EASE_QUART, PG_EASE_QUINT,
  PG_EASE_SINE, PG_EASE_EXPO, PG_EASE_CIRC, PG_EASE_BACK, PG_EASE_BOUNCE,
};
enum { PG_EASE_IN = 0, PG_EASE_OUT = 1, PG_EASE_IN_OUT = 2 };

/* Interpolation factor for t in 0..1 (BLI_easing_*_ease_in/out/in_out(t, 0, 1, 1)).
 * `back` is the overshoot of the Back type (Blender default 1.70158). Returns t when unknown. */
float pg_gp_interpolate_easing(int type, int mode, float t, float back);

/* Mirror modifier baked as copies: selected strokes are duplicated and mirrored about the pivot. */
int pg_gp_mirror_copy(struct bGPdata *gpd, const struct bGPDlayer *only_layer,
                      int axis_x, int axis_y, float pivot_x, float pivot_y);

/* Weight Paint "Draw" dab: moves the def_nr weight of points under the brush toward `target`. */
int pg_gp_weight_paint(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int def_nr,
                       float x, float y, float radius, float strength, float target);

/* eGP_Vertex_SculptTool (DNA_brush_enums.h) */
enum {
  PG_VPAINT_DRAW = 0,
  PG_VPAINT_BLUR = 1,
  PG_VPAINT_AVERAGE = 2,
  PG_VPAINT_SMEAR = 3,
  PG_VPAINT_REPLACE = 4,
};

typedef struct PGVertexPaint {
  int brush;
  float x, y, radius, strength;
  float rgb[3];
  int target; /* PG_PAINT_MODE_STROKE / FILL / BOTH */
  float dx, dy; /* drag delta, used by smear */
} PGVertexPaint;

/* One dab of Vertex Paint mode (gpencil_vertex_paint.c brushes) on the editable strokes. */
int pg_gp_vertex_paint(struct bGPdata *gpd, const struct bGPDlayer *only_layer, const PGVertexPaint *vp);

/* Per-stroke forms of the five modifiers above, used by the live modifier stack. They apply to the
 * given stroke regardless of selection or layer state; the pg_gp_mod_* functions above call them
 * for each selected stroke. Return 1 when the stroke may have changed, 0 otherwise. */
int pg_gp_modstroke_thickness(struct bGPDstroke *gps, int normalize, int thickness, float thickness_fac);
/* Same with a vertex group (def_nr -1 = none), see pg_gp_modifier_point_weight(). */
int pg_gp_modstroke_thickness_vgroup(struct bGPDstroke *gps, int def_nr, int invert, int normalize,
                                     int thickness, float thickness_fac);
int pg_gp_modstroke_opacity(struct bGPDstroke *gps, int modify_color, float factor, int normalize,
                            float hardness);
int pg_gp_modstroke_tint(struct bGPdata *gpd, struct bGPDstroke *gps, int vertex_mode, float factor,
                         const float rgb[3]);
int pg_gp_modstroke_color(struct bGPdata *gpd, struct bGPDstroke *gps, int modify_color,
                          const float hsv_factor[3]);
int pg_gp_modstroke_length(struct bGPdata *gpd, struct bGPDstroke *gps, const PGLengthParams *p);

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
