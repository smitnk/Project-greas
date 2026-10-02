/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Blender 3.6.23 Legacy GP eraser: the per-stroke evaluation of
 * gpencil_stroke_eraser_dostroke() (editors/gpencil_legacy/gpencil_paint.c) without the
 * parts that need bContext/depth buffers. It tags/modifies points and reports what the
 * caller must do (cull tagged points with BKE_gpencil_stroke_delete_tagged_points, or free
 * the whole stroke). Coordinates are canvas pixels.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPDstroke;

enum {
  PG_ERASER_HARD = 0,
  PG_ERASER_SOFT = 1,
  PG_ERASER_STROKE = 2,
};

enum {
  PG_ERASER_MODIFIED = 1, /* points were changed (strength/pressure/tags) */
  PG_ERASER_CULL = 2,     /* tagged points must now be deleted */
  PG_ERASER_FREE = 4,     /* the whole stroke must be freed */
};

typedef struct PGEraserParams {
  float mval[2];       /* eraser centre */
  int radius;          /* calc_radius of gpencil_stroke_doeraser() */
  int mode;            /* PG_ERASER_* */
  float draw_strength; /* brush->gpencil_settings->draw_strength */
  int use_pressure;    /* GP_BRUSH_USE_PRESSURE */
  float pressure;      /* p->pressure */
  float soft_strength; /* era_strength_f / 100 */
  float soft_thickness; /* era_thickness_f / 100 */
  int hard_flag;       /* GP_PAINTFLAG_HARD_ERASER (shift) */
} PGEraserParams;

/* Returns a combination of PG_ERASER_* flags. */
int pg_eraser_dostroke(struct bGPDstroke *gps, const PGEraserParams *params);

#ifdef __cplusplus
}
#endif
