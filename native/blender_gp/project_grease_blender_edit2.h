/* SPDX-License-Identifier: GPL-2.0-or-later
 * Second batch of selection-aware Legacy GP operators (kept separate from project_grease_blender_edit.c).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDlayer;
struct MDeformVert;

enum {
  PG_EDIT2_CMD_MOD_THICKNESS_VGROUP = 58, /* args: def_nr, invert, normalize, thickness, thickness_fac */
  PG_EDIT2_CMD_SELECT_VCOLOR = 59,        /* args: r, g, b, threshold (0..1 hue), extend */
  PG_EDIT2_CMD_NORMALIZE = 60,            /* args: mode (0 thickness, 1 opacity), value */
  PG_EDIT2_CMD_SIMPLIFY_FIXED = 61,       /* args: steps */
  PG_EDIT2_CMD_SAMPLE = 62,               /* args: length, sharp_threshold */
  PG_EDIT2_CMD_EXTRUDE = 63,              /* args: - */
};

/* get_modifier_point_weight() (MOD_gpencil_legacy_util.c): -1 means "skip this point". */
float pg_gp_modifier_point_weight(const struct MDeformVert *dvert, int inverse, int def_nr);

/* Thickness modifier with a vertex group (MOD_gpencil_legacy_thick.c deformStroke, no curve). */
int pg_gp_mod_thickness_vgroup(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int def_nr,
                               int invert, int normalize, int thickness, float thickness_fac);
/* GPENCIL_OT_select_vertex_color: points whose vertex-color hue is within threshold. */
int pg_gp_select_vertex_color(struct bGPdata *gpd, const struct bGPDlayer *only_layer,
                              const float rgb[3], float threshold, int extend);
/* GPENCIL_OT_stroke_normalize: set thickness (pressure) or opacity (strength) of selected points. */
int pg_gp_stroke_normalize(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mode, float value);
/* GPENCIL_OT_stroke_simplify_fixed: BKE_gpencil_stroke_simplify_fixed `steps` times. */
int pg_gp_stroke_simplify_fixed(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int steps);
/* GPENCIL_OT_stroke_sample: BKE_gpencil_stroke_sample. */
int pg_gp_stroke_sample(struct bGPdata *gpd, const struct bGPDlayer *only_layer, float length,
                        float sharp_threshold);
/* GPENCIL_OT_extrude for stroke end points: a new point is added at each selected end and selected. */
int pg_gp_extrude(struct bGPdata *gpd, const struct bGPDlayer *only_layer);

int pg_gp_edit2_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);

#ifdef __cplusplus
}
#endif
