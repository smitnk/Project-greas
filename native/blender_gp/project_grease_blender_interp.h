/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Interpolation ported from Blender 3.6.23 gpencil_interpolate.c (GPENCIL_OT_interpolate_sequence
 * and the stroke pairing / flip of GPENCIL_OT_interpolate): strokes pair by select_index (multiframe)
 * and then by position, unpaired strokes are skipped, point counts are matched with
 * BKE_gpencil_stroke_uniform_subdivide, flip is none / always / auto (gpencil_stroke_need_flip) and
 * the in-betweens are breakdown keys smoothed with BKE_gpencil_stroke_smooth. The canvas plane is
 * the 2D space (identity layer matrices). */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;
struct bGPDstroke;

enum { PG_INTERP_NOFLIP = 0, PG_INTERP_FLIP = 1, PG_INTERP_FLIPAUTO = 2 }; /* eGP_InterpolateFlipMode */

typedef struct PGInterpSettings {
  int step;               /* frames between in-betweens (sequence) */
  int flipmode;           /* PG_INTERP_* */
  int only_selected;      /* GP_TOOLFLAG_INTERPOLATE_ONLY_SELECTED */
  int exclude_breakdowns; /* skip breakdown keys when looking for the extremes */
  int all_layers;
  int easing_type, easing_mode; /* pg_gp_interpolate_easing (PG_EASE_*); linear when type is 0 */
  float smooth_factor;
  int smooth_steps;
  int single;             /* 1: only the current frame (GPENCIL_OT_interpolate confirm), 0: sequence */
  float factor;           /* single: the interpolation factor (< 0 uses the frame position) */
} PGInterpSettings;

/* gpencil_stroke_need_flip() in canvas space. */
int pg_interp_need_flip(const struct bGPDstroke *gps_from, const struct bGPDstroke *gps_to);
/* Creates the in-betweens around frame cfra. Returns the number of strokes created. */
int pg_gp_interpolate_run(struct bGPdata *gpd, struct bGPDlayer *active, int cfra, const PGInterpSettings *s);

/* Edit command routed by the bridge: cfra, step, flipmode, only selected, exclude breakdowns,
 * all layers, easing type, easing mode, smooth factor, smooth steps, single[, factor] */
#define PG_INTERP_CMD 126
int pg_gp_interp_dispatch(struct bGPdata *gpd, struct bGPDlayer *active, const float *args, int arg_count);
#ifdef __cplusplus
}
#endif
