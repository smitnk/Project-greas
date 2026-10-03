/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Live (non-destructive) Legacy GP modifier stack for Project Grease.
 *
 * A layer owns an ordered list of PGModEntry values {type, enabled, params}. The entries live on
 * the document (Backend), never on strokes. Evaluation duplicates the active frame's strokes,
 * runs every enabled entry's deform function in order on the copies and hands the copies to the
 * presenter; the original strokes are never modified (pg_mod_eval_frame). "Apply" bakes one entry
 * into the originals of the layer (pg_mod_apply), as Blender's modifier Apply does.
 *
 * Parameters are a flat float array per type (the index enums below); the same layout crosses
 * JNI, is saved in the project file and is mirrored in ProjectModifierStack.kt.
 *
 * Deform functions: Thickness, Opacity, Tint, Color and Length reuse the per-stroke functions of
 * project_grease_blender_edit.c (the "mirrors deformStroke()" code). Smooth, Simplify, Subdivide,
 * Offset and Noise carry the bodies of the pinned Blender 3.6.23 deformStroke() functions as
 * BEGIN/END VERBATIM regions in project_grease_modifier_stack.c (checked by
 * tools/verify_blender_verbatim.py); the glue around them is adapted and marked.
 *
 * Canvas mapping for Offset and Noise (they are 3D object-space modifiers in Blender):
 *   - Canvas y points down, Blender y up: the ported code runs on (x, -y, z) and the result is
 *     mapped back, so +Y moves up on screen and a positive Z rotation turns counter-clockwise.
 *   - Offset runs in canvas pixels: location / random offset values are canvas pixels, rotation is
 *     radians, scale is a factor.
 *   - Noise runs in units of PG_MOD_PIXELS_PER_UNIT canvas pixels (Blender's 0.1 * factor
 *     displacement becomes 0.1 * factor * PG_MOD_PIXELS_PER_UNIT pixels).
 *   - Points have z = 0 and the presenter ignores z, as for an orthographic front view.
 *   - The seed hashes Blender mixes in are the object name (PG_MOD_OBJECT_NAME) and the modifier
 *     name (pg_mod_name()).
 *   - Noise depends on time: it is evaluated with the current frame number.
 * Vertex groups: the Thickness entry has its own use_vgroup/vgroup/invert; every entry also has the
 * common filter block (vertex group, material, passes) and a custom curve, applied around the
 * deform function by blending the point attributes before/after with the point influence
 * (pg_mod_point_influence) when the point count did not change. Weight Proximity / Angle write the
 * weights of a target group on the evaluated copies, so later entries (Thickness with a group) see them.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;

enum {
  PG_MOD_NONE = 0,
  PG_MOD_THICKNESS = 1,
  PG_MOD_OPACITY = 2,
  PG_MOD_TINT = 3,
  PG_MOD_COLOR = 4, /* Hue/Saturation */
  PG_MOD_LENGTH = 5,
  PG_MOD_SMOOTH = 6,
  PG_MOD_SIMPLIFY = 7,
  PG_MOD_SUBDIV = 8,
  PG_MOD_OFFSET = 9,
  PG_MOD_NOISE = 10,
  /* Batch 21: generators and 2D deforms (project_grease_modifier_stack2.c) */
  PG_MOD_BUILD = 11,
  PG_MOD_TIME = 12,
  PG_MOD_HOOK = 13,
  PG_MOD_LATTICE = 14,
  PG_MOD_ENVELOPE = 15,
  PG_MOD_WEIGHT_PROX = 16,
  PG_MOD_WEIGHT_ANGLE = 17,
  PG_MOD_DASH = 18,
  PG_MOD_OUTLINE = 19,
  PG_MOD_MIRROR = 20,
  PG_MOD_ARRAY = 21,
  PG_MOD_MULTIPLY = 22,
  PG_MOD_TYPE_LAST = 22,
};

/* Every entry stores PG_MOD_MAX_PARAMS floats: the type's own parameters from index 0 (PG_P_*_COUNT
 * of them), a custom curve block at PG_P_CURVE_BASE and an influence filter block at
 * PG_P_FILTER_BASE (Blender's is_stroke_affected_by_modifier / vertex group / use_custom_curve). All
 * zero means "no curve, no filter", so stacks saved before these blocks existed load unchanged. */
#define PG_MOD_MAX_PARAMS 120
#define PG_MOD_MAX_STACK 32
#define PG_MOD_PIXELS_PER_UNIT 100.0f
#define PG_MOD_OBJECT_NAME "Project Grease"

/* Parameter indices per type. Booleans are 0/1, enums and counts are stored as floats. */
enum { /* THICKNESS (MOD_gpencil_legacy_thick.c) */
  PG_P_THICK_NORMALIZE = 0, PG_P_THICK_THICKNESS = 1, PG_P_THICK_FACTOR = 2,
  PG_P_THICK_USE_VGROUP = 3, PG_P_THICK_VGROUP = 4, PG_P_THICK_INVERT_VGROUP = 5, PG_P_THICK_COUNT = 6 };
enum { /* OPACITY: modify_color is PG_MODIFY_COLOR_* */
  PG_P_OPACITY_MODIFY = 0, PG_P_OPACITY_FACTOR = 1, PG_P_OPACITY_NORMALIZE = 2,
  PG_P_OPACITY_HARDNESS = 3, PG_P_OPACITY_COUNT = 4 };
enum { /* TINT: mode is PG_PAINT_MODE_* */
  PG_P_TINT_MODE = 0, PG_P_TINT_FACTOR = 1, PG_P_TINT_R = 2, PG_P_TINT_G = 3, PG_P_TINT_B = 4,
  PG_P_TINT_COUNT = 5 };
enum { /* COLOR: modify_color is PG_MODIFY_COLOR_*, hsv are the hue/saturation/value factors */
  PG_P_COLOR_MODIFY = 0, PG_P_COLOR_H = 1, PG_P_COLOR_S = 2, PG_P_COLOR_V = 3, PG_P_COLOR_COUNT = 4 };
enum { /* LENGTH: same layout as PGLengthParams */
  PG_P_LENGTH_MODE = 0, PG_P_LENGTH_START = 1, PG_P_LENGTH_END = 2, PG_P_LENGTH_OVERSHOOT = 3,
  PG_P_LENGTH_USE_CURVATURE = 4, PG_P_LENGTH_POINT_DENSITY = 5, PG_P_LENGTH_SEGMENT_INFLUENCE = 6,
  PG_P_LENGTH_MAX_ANGLE = 7, PG_P_LENGTH_INVERT_CURVATURE = 8, PG_P_LENGTH_COUNT = 9 };
enum { /* SMOOTH */
  PG_P_SMOOTH_FACTOR = 0, PG_P_SMOOTH_STEP = 1, PG_P_SMOOTH_LOCATION = 2, PG_P_SMOOTH_STRENGTH = 3,
  PG_P_SMOOTH_THICKNESS = 4, PG_P_SMOOTH_UV = 5, PG_P_SMOOTH_KEEP_SHAPE = 6, PG_P_SMOOTH_COUNT = 7 };
enum { /* SIMPLIFY: mode is GP_SIMPLIFY_FIXED/ADAPTIVE/SAMPLE/MERGE (0..3) */
  PG_P_SIMPLIFY_MODE = 0, PG_P_SIMPLIFY_STEP = 1, PG_P_SIMPLIFY_FACTOR = 2, PG_P_SIMPLIFY_LENGTH = 3,
  PG_P_SIMPLIFY_SHARP_THRESHOLD = 4, PG_P_SIMPLIFY_DISTANCE = 5, PG_P_SIMPLIFY_COUNT = 6 };
enum { /* SUBDIV: type is GP_SUBDIV_CATMULL (0) / GP_SUBDIV_SIMPLE (1) */
  PG_P_SUBDIV_LEVEL = 0, PG_P_SUBDIV_TYPE = 1, PG_P_SUBDIV_COUNT = 2 };
enum { /* OFFSET: mode is GP_OFFSET_RANDOM/LAYER/MATERIAL/STROKE (0..3) */
  PG_P_OFFSET_MODE = 0, PG_P_OFFSET_LOC = 1, PG_P_OFFSET_ROT = 4, PG_P_OFFSET_SCALE = 7,
  PG_P_OFFSET_RND_OFFSET = 10, PG_P_OFFSET_RND_ROT = 13, PG_P_OFFSET_RND_SCALE = 16,
  PG_P_OFFSET_SEED = 19, PG_P_OFFSET_STROKE_STEP = 20, PG_P_OFFSET_START_OFFSET = 21,
  PG_P_OFFSET_UNIFORM_SCALE = 22, PG_P_OFFSET_COUNT = 23 };
enum { /* NOISE: mode is GP_NOISE_RANDOM_STEP (0) / GP_NOISE_RANDOM_KEYFRAME (1) */
  PG_P_NOISE_FACTOR = 0, PG_P_NOISE_STRENGTH = 1, PG_P_NOISE_THICKNESS = 2, PG_P_NOISE_UVS = 3,
  PG_P_NOISE_SCALE = 4, PG_P_NOISE_OFFSET = 5, PG_P_NOISE_SEED = 6, PG_P_NOISE_STEP = 7,
  PG_P_NOISE_USE_RANDOM = 8, PG_P_NOISE_MODE = 9, PG_P_NOISE_COUNT = 10 };

enum { /* BUILD (MOD_gpencil_legacy_build.c): mode 0 sequential / 1 concurrent, transition 0 grow /
        * 1 shrink, start delay and length in frames counted from the keyframe (pg_build_visible) */
  PG_P_BUILD_MODE = 0, PG_P_BUILD_TRANSITION = 1, PG_P_BUILD_START = 2, PG_P_BUILD_LENGTH = 3,
  PG_P_BUILD_COUNT = 4 };
enum { /* TIME (MOD_gpencil_legacy_time.c): the layer shows frame pg_time_offset_frame() */
  PG_P_TIME_MODE = 0, PG_P_TIME_OFFSET = 1, PG_P_TIME_SCALE = 2, PG_P_TIME_USE_RANGE = 3,
  PG_P_TIME_SFRA = 4, PG_P_TIME_EFRA = 5, PG_P_TIME_LOOP = 6, PG_P_TIME_COUNT = 7 };
enum { /* HOOK (2D, pg_hook_deform): canvas center, offset, rotation, scale, radius, falloff */
  PG_P_HOOK_CX = 0, PG_P_HOOK_CY = 1, PG_P_HOOK_DX = 2, PG_P_HOOK_DY = 3, PG_P_HOOK_ANGLE = 4,
  PG_P_HOOK_SCALE = 5, PG_P_HOOK_RADIUS = 6, PG_P_HOOK_FALLOFF = 7, PG_P_HOOK_STRENGTH = 8,
  PG_P_HOOK_COUNT = 9 };
#define PG_LATTICE_MAX 6
enum { /* LATTICE (2D, pg_lattice_deform): rect, nu x nv grid (2..6), strength, grid offsets */
  PG_P_LATTICE_X0 = 0, PG_P_LATTICE_Y0 = 1, PG_P_LATTICE_X1 = 2, PG_P_LATTICE_Y1 = 3,
  PG_P_LATTICE_NU = 4, PG_P_LATTICE_NV = 5, PG_P_LATTICE_STRENGTH = 6, PG_P_LATTICE_OFFSETS = 7,
  PG_P_LATTICE_COUNT = 7 + PG_LATTICE_MAX * PG_LATTICE_MAX * 2 };
enum { /* ENVELOPE (MOD_gpencil_legacy_envelope.c): mode 0 deform / 1 segments / 2 fills */
  PG_P_ENVELOPE_MODE = 0, PG_P_ENVELOPE_SPREAD = 1, PG_P_ENVELOPE_SKIP = 2,
  PG_P_ENVELOPE_THICKNESS = 3, PG_P_ENVELOPE_STRENGTH = 4, PG_P_ENVELOPE_MAT = 5,
  PG_P_ENVELOPE_COUNT = 6 };
enum { /* WEIGHT_PROX (MOD_gpencil_legacy_weight_proximity.c): target group, canvas point */
  PG_P_WPROX_TARGET = 0, PG_P_WPROX_X = 1, PG_P_WPROX_Y = 2, PG_P_WPROX_DIST_START = 3,
  PG_P_WPROX_DIST_END = 4, PG_P_WPROX_MIN_WEIGHT = 5, PG_P_WPROX_INVERT = 6, PG_P_WPROX_MULTIPLY = 7,
  PG_P_WPROX_COUNT = 8 };
enum { /* WEIGHT_ANGLE (MOD_gpencil_legacy_weight_angle.c): target group, reference angle */
  PG_P_WANGLE_TARGET = 0, PG_P_WANGLE_ANGLE = 1, PG_P_WANGLE_MIN_WEIGHT = 2, PG_P_WANGLE_INVERT = 3,
  PG_P_WANGLE_MULTIPLY = 4, PG_P_WANGLE_COUNT = 5 };
enum { /* DASH (one segment: dash / gap points, offset) */
  PG_P_DASH_DASH = 0, PG_P_DASH_GAP = 1, PG_P_DASH_OFFSET = 2, PG_P_DASH_COUNT = 3 };
enum { /* OUTLINE (thickness px, cap segments) */
  PG_P_OUTLINE_THICKNESS = 0, PG_P_OUTLINE_SUBDIV = 1, PG_P_OUTLINE_COUNT = 2 };
enum { /* MIRROR (axes, canvas pivot) */
  PG_P_MIRROR_X = 0, PG_P_MIRROR_Y = 1, PG_P_MIRROR_PX = 2, PG_P_MIRROR_PY = 3, PG_P_MIRROR_COUNT = 4 };
enum { /* ARRAY (count, constant offset) */
  PG_P_ARRAY_COUNT_N = 0, PG_P_ARRAY_OX = 1, PG_P_ARRAY_OY = 2, PG_P_ARRAY_COUNT = 3 };
enum { /* MULTIPLY (duplications, distance) */
  PG_P_MULTIPLY_DUPLICATIONS = 0, PG_P_MULTIPLY_DISTANCE = 1, PG_P_MULTIPLY_COUNT = 2 };

/* Custom curve (use_custom_curve / curve_intensity): point i of n gets factor curve(i / (n - 1)). */
#define PG_P_CURVE_BASE 96
enum { PG_P_CURVE_USE = 0, PG_P_CURVE_COUNT_PTS = 1, PG_P_CURVE_XY = 2, PG_P_CURVE_MAX_PTS = 7 };
/* Influence filters: material slot / material pass / layer pass (+1, 0 = off) with invert flags,
 * vertex group (+1, 0 = off) with invert. */
#define PG_P_FILTER_BASE 112
enum { PG_P_FILTER_MATERIAL = 0, PG_P_FILTER_INVERT_MATERIAL = 1, PG_P_FILTER_PASS = 2,
       PG_P_FILTER_INVERT_PASS = 3, PG_P_FILTER_LAYER_PASS = 4, PG_P_FILTER_INVERT_LAYER_PASS = 5,
       PG_P_FILTER_VGROUP = 6, PG_P_FILTER_INVERT_VGROUP = 7 };

typedef struct PGModEntry {
  int type;     /* PG_MOD_* */
  int enabled;  /* 0 = skipped by evaluation */
  float params[PG_MOD_MAX_PARAMS];
} PGModEntry;

typedef struct PGModContext {
  struct bGPdata *gpd;
  struct bGPDlayer *gpl; /* the layer in gpd->layers (used by Offset's layer mode) */
  struct bGPDframe *gpf; /* frame whose ->strokes contains the stroke being deformed */
  int cfra;              /* current frame number (Noise) */
} PGModContext;

/* Type information */
int pg_mod_valid_type(int type);
/* Stored parameter count: PG_MOD_MAX_PARAMS for every valid type (own + curve + filter blocks). */
int pg_mod_param_count(int type);
/* The type's own parameters (PG_P_*_COUNT). */
int pg_mod_own_param_count(int type);
const char *pg_mod_name(int type); /* Blender's default modifier name, e.g. "Noise" */
/* Fills params with Blender's DNA defaults for the type (rest zeroed). */
void pg_mod_defaults(int type, float params[PG_MOD_MAX_PARAMS]);
/* Clamps enums/counts to valid ranges and replaces non-finite values with the defaults. */
void pg_mod_sanitize(int type, float params[PG_MOD_MAX_PARAMS]);
void pg_mod_entry_init(PGModEntry *entry, int type);

/* Runs one modifier's deform function on one stroke. Returns 1 when it may have changed it. */
int pg_mod_deform_stroke(const PGModContext *ctx, const PGModEntry *entry, struct bGPDstroke *gps);

/* Evaluates the stack on a COPY of the frame's strokes. `r_eval` is initialised here; the copies
 * are in r_eval->strokes (same order as gpf->strokes unless Simplify/Merge removed some).
 * Returns the number of strokes. Free with pg_mod_eval_free(). The originals are not touched. */
int pg_mod_eval_frame(struct bGPdata *gpd, struct bGPDlayer *gpl, struct bGPDframe *gpf,
                      const PGModEntry *entries, int count, int cfra, struct bGPDframe *r_eval);
void pg_mod_eval_free(struct bGPDframe *eval);

/* Bakes one entry into every frame of the layer (the originals). Returns 1 when anything changed.
 * Removing the entry from the list is the caller's job. */
int pg_mod_apply(struct bGPdata *gpd, struct bGPDlayer *gpl, const PGModEntry *entry, int cfra);

/* ---- project_grease_modifier_stack2.c ---- */
/* Entries that work on the whole frame (generators, Build); the rest deform one stroke at a time. */
int pg_mod2_is_frame_level(int type);
int pg_mod2_run_frame(const PGModContext *ctx, const PGModEntry *entry);
int pg_mod2_deform_stroke(const PGModContext *ctx, const PGModEntry *entry, struct bGPDstroke *gps);
void pg_mod2_defaults(int type, float params[PG_MOD_MAX_PARAMS]);
void pg_mod2_sanitize(int type, float params[PG_MOD_MAX_PARAMS]);
/* Time Offset: the frame number a layer shows at `cfra` after every enabled Time entry, in order. */
int pg_mod_time_frame(const PGModEntry *entries, int count, int cfra);
/* Influence: 0 when the filter block excludes the stroke. */
int pg_mod_stroke_affected(const PGModContext *ctx, const PGModEntry *entry, const struct bGPDstroke *gps);
/* Per-point influence from the vertex-group filter and the custom curve (1 when both are off). */
float pg_mod_point_influence(const PGModEntry *entry, const struct bGPDstroke *gps, int index);
int pg_mod_has_point_influence(const PGModEntry *entry);

#ifdef __cplusplus
}
#endif
