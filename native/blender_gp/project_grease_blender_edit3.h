/* SPDX-License-Identifier: GPL-2.0-or-later
 * Third batch of Legacy GP operators (frames, cleanup, random select, vertex-color operators).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDlayer;

enum {
  PG_EDIT3_CMD_SELECT_RANDOM = 66,   /* args: ratio, seed, select (1) / deselect (0) */
  PG_EDIT3_CMD_BLANK_FRAME = 67,     /* args: cframe */
  PG_EDIT3_CMD_FILL_COLOR = 68,      /* args: material index, r, g, b, a */
  PG_EDIT3_CMD_CLEAN_LOOSE = 69,     /* args: limit */
  PG_EDIT3_CMD_CLEAN_DUP_FRAMES = 70,/* args: - (active layer) */
  PG_EDIT3_CMD_VCOLOR_SET = 71,      /* args: mode, r, g, b, factor */
  PG_EDIT3_CMD_VCOLOR_INVERT = 72,   /* args: mode */
  PG_EDIT3_CMD_VCOLOR_BC = 73,       /* args: mode, brightness, contrast */
  PG_EDIT3_CMD_VCOLOR_HSV = 74,      /* args: mode, h, s, v */
  PG_EDIT3_CMD_VCOLOR_LEVELS = 75,   /* args: mode, offset, gain */
};


int pg_gp_select_random(struct bGPdata *gpd, const struct bGPDlayer *only_layer, float ratio,
                        unsigned int seed, int select);
int pg_gp_blank_frame_add(struct bGPdata *gpd, struct bGPDlayer *gpl, int cframe);
int pg_gp_material_fill_color(struct bGPdata *gpd, int mat_nr, const float rgba[4]);
int pg_gp_frame_clean_loose(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int limit);
int pg_gp_frame_clean_duplicate(struct bGPdata *gpd, struct bGPDlayer *gpl);
/* Vertex-color operators on the selected points (mode: PG_PAINT_MODE_STROKE/FILL/BOTH). */
int pg_gp_vcolor_set(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mode,
                     const float rgb[3], float factor);
int pg_gp_vcolor_invert(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mode);
int pg_gp_vcolor_brightness_contrast(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mode,
                                     float brightness, float contrast);
int pg_gp_vcolor_hsv(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mode, float h,
                     float s, float v);
int pg_gp_vcolor_levels(struct bGPdata *gpd, const struct bGPDlayer *only_layer, int mode,
                        float offset, float gain);

int pg_gp_edit3_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);

#ifdef __cplusplus
}
#endif
