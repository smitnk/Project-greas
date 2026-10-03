/* SPDX-License-Identifier: GPL-2.0-or-later
 * Sixth batch (edit5 one-go): Outline (baked), interpolation of unequal strokes, onion-skin ghost frames,
 * fill extend lines. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;
struct bGPDstroke;

enum {
  PG_EDIT6_CMD_OUTLINE = 90,     /* args: outline thickness, cap segments */
  PG_EDIT6_CMD_ONION_STYLE = 91, /* args: Blender onion_mode (GP_ONION_MODE_*), use prev color,
                                  * use next color, prev r,g,b, next r,g,b */
  PG_EDIT6_CMD_MATERIAL_TEXTURE = 92, /* args: material, target (0 stroke, 1 fill), enabled, mix,
                                       * scale x, scale y, offset x, offset y, angle, pixel size */
};

/* Stroke / fill texture settings of material slot `index` (MaterialGPencilStyle: stroke_style
 * GP_MATERIAL_STROKE_STYLE_TEXTURE + mix_stroke_factor + texture_pixsize for strokes; fill_style
 * GP_MATERIAL_FILL_STYLE_TEXTURE + mix_factor + texture_scale / offset / angle for fills). The image
 * itself is held by the presenter (no Blender Image datablock on Android). */
int pg_gp_material_texture_set(struct bGPdata *gpd, int index, int fill, int enabled, float mix,
                               const float scale[2], const float offset[2], float angle, float pixsize);

/* Outline modifier, 2D: each selected stroke becomes the closed perimeter of its thick shape. */
int pg_gp_outline(struct bGPdata *gpd, const struct bGPDlayer *only, int outline_thickness, int cap_segments);

/* gpencil_interpolate.c: a new stroke between `from` and `to` at factor t. When point counts
 * differ the copy with fewer points is resampled with BKE_gpencil_stroke_uniform_subdivide. */
struct bGPDstroke *pg_gp_interpolate_strokes(struct bGPdata *gpd, const struct bGPDstroke *from,
                                             const struct bGPDstroke *to, float t);

/* Onion skin ghost selection (GP_ONION_MODE_RELATIVE/ABSOLUTE/SELECTED). keys: sorted keyframe
 * numbers; selected may be NULL. Returns ghost count written to out_frames/out_alpha. */
enum { PG_ONION_RELATIVE = 0, PG_ONION_ABSOLUTE = 1, PG_ONION_SELECTED = 2 };
int pg_onion_ghosts(int mode, const int *keys, const unsigned char *selected, int nkeys, int current,
                    int before, int after, int use_fade, float factor, int *out_frames,
                    float *out_alpha, int max_out);

/* Fill "extend lines": for an open stroke, the two extension segments of its ends
 * (out: start_from, start_to, end_from, end_to as x,y pairs = 8 floats). Returns 0 if none. */
int pg_fill_extend_segments(const struct bGPDstroke *gps, float extend_fac, float out[8]);

int pg_gp_edit6_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);
#ifdef __cplusplus
}
#endif
