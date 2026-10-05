/* SPDX-License-Identifier: GPL-2.0-or-later
 * Fourth batch: generator modifiers baked into the selection, stroke/layer operators, clipboard.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;

enum {
  PG_EDIT4_CMD_DASH = 76,          /* args: dash, gap, offset (points) */
  PG_EDIT4_CMD_MULTIPLY = 77,      /* args: duplications, distance */
  PG_EDIT4_CMD_ARRAY = 78,         /* args: count, offset_x, offset_y */
  PG_EDIT4_CMD_MERGE_DISTANCE = 79,/* args: threshold, use_unselected */
  PG_EDIT4_CMD_CAPS = 80,          /* args: type (PG_CAPS_*) */
  PG_EDIT4_CMD_START_SET = 81,     /* args: - */
  PG_EDIT4_CMD_SEPARATE_LAYER = 82,/* args: - */
  PG_EDIT4_CMD_MOVE_TO_LAYER = 83, /* args: target layer index */
  PG_EDIT4_CMD_COPY = 84,          /* args: - */
  PG_EDIT4_CMD_PASTE = 85,         /* args: - */
};
/* GPENCIL_OT_stroke_caps_set types; GP_STROKE_CAP_ROUND = 0, GP_STROKE_CAP_FLAT = 1 */
enum { PG_CAPS_TOGGLE_BOTH = 0, PG_CAPS_TOGGLE_START = 1, PG_CAPS_TOGGLE_END = 2, PG_CAPS_DEFAULT = 3 };

int pg_gp_dash(struct bGPdata *gpd, const struct bGPDlayer *only, int dash, int gap, int offset);
int pg_gp_multiply(struct bGPdata *gpd, const struct bGPDlayer *only, int duplications, float distance);
int pg_gp_array(struct bGPdata *gpd, const struct bGPDlayer *only, int count, float ox, float oy);
int pg_gp_merge_distance(struct bGPdata *gpd, const struct bGPDlayer *only, float threshold, int use_unselected);
int pg_gp_caps_set(struct bGPdata *gpd, const struct bGPDlayer *only, int type);
int pg_gp_start_set(struct bGPdata *gpd, const struct bGPDlayer *only);
int pg_gp_separate_to_layer(struct bGPdata *gpd, struct bGPDlayer *src);
int pg_gp_move_to_layer(struct bGPdata *gpd, struct bGPDlayer *src, int target_index);
int pg_gp_copy(struct bGPdata *gpd, const struct bGPDlayer *only);
int pg_gp_paste(struct bGPdata *gpd, struct bGPDlayer *target);
void pg_gp_clipboard_free(void);
/* The clipboard strokes (gpencil_strokes_copypastebuf), read by the sculpt Clone brush. */
const struct ListBase *pg_gp_clipboard_strokes(void);
int pg_gp_edit4_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);
#ifdef __cplusplus
}
#endif
