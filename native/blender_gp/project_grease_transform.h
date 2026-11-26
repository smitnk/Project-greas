/* SPDX-License-Identifier: GPL-2.0-or-later
 * Grease Pencil edit-mode transform (Blender 3.6.23 editors/transform), as a modal session:
 * begin = createTransGPencil (transform_convert_gpencil_legacy.c) + set_prop_dist
 * (transform_convert.c) + calculatePropRatio / calculateCenter (transform_generics.c);
 * apply = applyTranslation / applyRotation / applyResize / applyMirror (transform_mode_*.c),
 * always from the initial positions (td->iloc) with the gesture's total value, like the modal;
 * end = confirm (keep) or cancel (restoreTransObjects).
 * Constraints (transform_constraints.c) and numeric input (editors/util/numinput.c, see
 * project_grease_numinput.h) are set on the open session.
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
/* The same with three values and the input kind: PG_TFM_INPUT_MOUSE is the gesture (the constraint
 * projects it, typed numeric input overrides it), PG_TFM_INPUT_FINAL is an operator's exec with
 * given values (T_INPUT_IS_VALUES_FINAL: values in the orientation space, no snapping, no flip).
 * Rotation uses values[0] only; resize's third value is Z. */
enum { PG_TFM_INPUT_MOUSE = 0, PG_TFM_INPUT_FINAL = 1 };
int pg_transform_apply_ex(PGTransInfo *t, const float values[3], int input);

/* Axis constraint (setUserConstraint): axes is a mask of the constrained axes, X = 1, Y = 2, Z = 4
 * (one axis = an axis lock; two = a plane lock, Shift+the excluded axis); 0 removes it
 * (stopConstraint). orient is V3D_ORIENT_GLOBAL (0) or V3D_ORIENT_LOCAL (1). The last value is
 * applied again. Mirror ignores it. Returns 1 when set. */
enum { PG_TFM_ORIENT_GLOBAL = 0, PG_TFM_ORIENT_LOCAL = 1 };
int pg_transform_set_constraint(PGTransInfo *t, int axes, int orient);
/* One key of numeric input (handleNumInput: an ASCII character or PG_NUM_KEY_*), then the last value
 * is applied again with the typed values. text (may be NULL) gets outputNumInput's text. Returns 1
 * when the key was used. */
int pg_transform_numinput(PGTransInfo *t, int key, char *text, int text_len);
/* Typed values exist (hasNumInput). */
int pg_transform_has_numinput(const PGTransInfo *t);

/* The data was freed under the session (undo, new document): free it without touching the points. */
void pg_transform_discard(PGTransInfo *t);

/* snap_increment_apply_ex (transform_snap.cc) with aspect 1: inc * roundf(v / inc). */
float pg_transform_snap_increment(float v, float inc);
/* calculatePropRatio's falloff for (prop_size - rdist) / prop_size, PROP_RANDOM excluded. */
float pg_transform_prop_factor(int prop_mode, float rdist, float prop_size);

#ifdef __cplusplus
}
#endif
