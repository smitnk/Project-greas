/* SPDX-License-Identifier: GPL-2.0-or-later
 * Grease Pencil edit-mode transform (Blender 3.6.23 editors/transform), as a modal session:
 * begin = createTransGPencil (transform_convert_gpencil_legacy.c) + set_prop_dist
 * (transform_convert.c) + calculatePropRatio / calculateCenter (transform_generics.c);
 * apply = applyTranslation / applyRotation / applyResize / applyMirror (transform_mode_*.c),
 * always from the initial positions (td->iloc) with the gesture's total value, like the modal;
 * end = confirm (keep) or cancel (restoreTransObjects).
 * The canvas is the XY plane seen from +Z (a top view), the layer matrices are identity. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
typedef struct PGTransInfo PGTransInfo;

/* t->mode */
enum { PG_TFM_TRANSLATION = 0, PG_TFM_ROTATION = 1, PG_TFM_RESIZE = 2, PG_TFM_MIRROR = 3 };
/* ts->transform_pivot_point, in the app's order (PG_PIVOT_* of project_grease_blender_edit9.h) */
enum { PG_TFM_PIVOT_MEDIAN = 0, PG_TFM_PIVOT_BOUNDS = 1, PG_TFM_PIVOT_INDIVIDUAL = 2, PG_TFM_PIVOT_CURSOR = 3 };

typedef struct PGTransformOptions {
  int pivot;              /* PG_TFM_PIVOT_* */
  float cursor[2];        /* scene->cursor.location for PG_TFM_PIVOT_CURSOR */
  int prop_edit;          /* T_PROP_EDIT */
  int prop_connected;     /* T_PROP_CONNECTED */
  int prop_mode;          /* eProportionalFalloff PROP_* (PROP_RANDOM is seeded by the clock, as Blender) */
  float prop_size;        /* t->prop_size */
  int multiframe_falloff; /* GP_SCULPT_SETT_FLAG_FRAME_FALLOFF (used with multi-frame editing on) */
  int snap;               /* SCE_SNAP with SCE_SNAP_MODE_INCREMENT */
  float snap_translate;   /* t->snap[0] for translation: the 2D grid step in canvas units */
} PGTransformOptions;

/* NULL: nothing to transform (no selected point / curve point in the editable data). */
PGTransInfo *pg_transform_begin(struct bGPdata *gpd, int mode, const PGTransformOptions *o);
/* values: translation (dx, dy); rotation (angle, Blender's sign: positive turns clockwise in a
 * y-up view); resize (sx, sy); mirror (axis: 0 = X, 1 = Y). Returns 1 when applied. */
int pg_transform_apply(PGTransInfo *t, const float values[2]);
/* cancel: restore the initial positions and handle types. The session is freed. */
void pg_transform_end(PGTransInfo *t, int cancel);
/* The data was freed under the session (undo, new document): free it without touching the points. */
void pg_transform_discard(PGTransInfo *t);

/* snap_increment_apply_ex (transform_snap.cc) with aspect 1: inc * roundf(v / inc). */
float pg_transform_snap_increment(float v, float inc);
/* calculatePropRatio's falloff for (prop_size - rdist) / prop_size, PROP_RANDOM excluded. */
float pg_transform_prop_factor(int prop_mode, float rdist, float prop_size);

#ifdef __cplusplus
}
#endif
