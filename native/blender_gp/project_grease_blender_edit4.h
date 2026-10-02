/* SPDX-License-Identifier: GPL-2.0-or-later
 * Fourth batch of Legacy GP operators: segment select (ED_gpencil_select_stroke_segment), material
 * slot removal, per-layer onion skinning and onion fade. Linked against the real BKE / BLI code.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDlayer;

enum {
  PG_EDIT4_CMD_SELECT_SEGMENT = 76, /* args: x, y, radius_squared, flags (PG_PICK_*) */
  PG_EDIT4_CMD_MATERIAL_REMOVE = 77,/* args: material index */
  PG_EDIT4_CMD_ONION_LAYER = 78,    /* args: layer index, enabled */
  PG_EDIT4_CMD_ONION_FADE = 79,     /* args: enabled */
};
#define PG_EDIT4_CMD_FIRST 76
#define PG_EDIT4_CMD_LAST 79

/* Blender's default GP_Sculpt_Settings.isect_threshold (versioning_280.c), the "scale" that
 * ED_gpencil_select_stroke_segment() widens the stroke extremes with. */
#define PG_SEGMENT_ISECT_THRESHOLD 0.1f

/* Click select in segment mode (gpencil_select_exec() with GP_SELECTMODE_SEGMENT): selects the
 * nearest point like point mode, then expands the selection to the run of points between the
 * nearest intersections with other strokes of the hit layer's active frame. Returns 1 when the
 * selection changed. */
int pg_gp_select_segment_pick(struct bGPdata *gpd, const struct bGPDlayer *only_layer, float x,
                              float y, int radius_squared, int flags);

/* Removes material slot `index`: strokes using it are deleted, strokes with a higher mat_nr move
 * down one slot (BKE_gpencil_material_index_reassign) and the palette shrinks. Returns the number
 * of strokes removed + 1, or 0 when the slot does not exist or is the last one. */
int pg_gp_material_slot_remove(struct bGPdata *gpd, int index);

/* bGPDlayer GP_LAYER_ONIONSKIN of layer `index`. */
int pg_gp_layer_onion_set(struct bGPdata *gpd, int index, int enabled);

/* gpencil_cache_utils.c gpencil_layer_final_tint_and_alpha_get(): ghost opacity of a frame
 * `onion_id` keyframes away (negative before, positive after). */
float pg_gp_onion_alpha(int onion_id, int use_fade, float onion_factor);

int pg_gp_edit4_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);

#ifdef __cplusplus
}
#endif
