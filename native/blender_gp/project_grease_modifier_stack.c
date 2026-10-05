/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Live modifier stack for Project Grease; see project_grease_modifier_stack.h for the design and
 * the canvas mapping. Regions between "BEGIN VERBATIM" / "END VERBATIM" are byte-identical copies
 * of the pinned Blender 3.6.23 modifier files (tools/verify_blender_verbatim.py checks them in
 * CI). Everything else here is adapted glue, and each adaptation is marked "ADAPTED":
 *   - deformStroke() signature, GpencilModifierData / Object / Depsgraph access and
 *     is_stroke_affected_by_modifier() (no layer/material/pass filters on a per-layer stack),
 *   - vertex groups (def_nr = -1, so every point weight is 1) and custom curves (not supported),
 *   - ob->id.name / md->name seed hashing (fixed names), DEG_get_ctime() (the current frame),
 *   - the y-flip / unit mapping around Offset and Noise.
 */

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_modifier_stack.h"

/* ---------------------------------------------------------------------------------------- */
/* Type information                                                                          */

int pg_mod_valid_type(int type)
{
  return type >= PG_MOD_THICKNESS && type <= PG_MOD_TYPE_LAST;
}

int pg_mod_param_count(int type)
{
  return pg_mod_valid_type(type) ? PG_MOD_MAX_PARAMS : 0;
}

int pg_mod_own_param_count(int type)
{
  switch (type) {
    case PG_MOD_BUILD: return PG_P_BUILD_COUNT;
    case PG_MOD_TIME: return PG_P_TIME_COUNT;
    case PG_MOD_HOOK: return PG_P_HOOK_COUNT;
    case PG_MOD_LATTICE: return PG_P_LATTICE_COUNT;
    case PG_MOD_ENVELOPE: return PG_P_ENVELOPE_COUNT;
    case PG_MOD_WEIGHT_PROX: return PG_P_WPROX_COUNT;
    case PG_MOD_WEIGHT_ANGLE: return PG_P_WANGLE_COUNT;
    case PG_MOD_DASH: return PG_P_DASH_COUNT;
    case PG_MOD_OUTLINE: return PG_P_OUTLINE_COUNT;
    case PG_MOD_MIRROR: return PG_P_MIRROR_COUNT;
    case PG_MOD_ARRAY: return PG_P_ARRAY_COUNT;
    case PG_MOD_MULTIPLY: return PG_P_MULTIPLY_COUNT;
    case PG_MOD_TEXTURE: return PG_P_TEXTURE_COUNT;
    case PG_MOD_THICKNESS: return PG_P_THICK_COUNT;
    case PG_MOD_OPACITY: return PG_P_OPACITY_COUNT;
    case PG_MOD_TINT: return PG_P_TINT_COUNT;
    case PG_MOD_COLOR: return PG_P_COLOR_COUNT;
    case PG_MOD_LENGTH: return PG_P_LENGTH_COUNT;
    case PG_MOD_SMOOTH: return PG_P_SMOOTH_COUNT;
    case PG_MOD_SIMPLIFY: return PG_P_SIMPLIFY_COUNT;
    case PG_MOD_SUBDIV: return PG_P_SUBDIV_COUNT;
    case PG_MOD_OFFSET: return PG_P_OFFSET_COUNT;
    case PG_MOD_NOISE: return PG_P_NOISE_COUNT;
    default: return 0;
  }
}

/* Blender's default modifier names (the modifier type's UI name). Offset and Noise hash theirs. */
const char *pg_mod_name(int type)
{
  switch (type) {
    case PG_MOD_THICKNESS: return "Thickness";
    case PG_MOD_OPACITY: return "Opacity";
    case PG_MOD_TINT: return "Tint";
    case PG_MOD_COLOR: return "Hue/Saturation";
    case PG_MOD_LENGTH: return "Length";
    case PG_MOD_SMOOTH: return "Smooth";
    case PG_MOD_SIMPLIFY: return "Simplify";
    case PG_MOD_SUBDIV: return "Subdivide";
    case PG_MOD_OFFSET: return "Offset";
    case PG_MOD_NOISE: return "Noise";
    case PG_MOD_BUILD: return "Build";
    case PG_MOD_TIME: return "Time Offset";
    case PG_MOD_HOOK: return "Hook";
    case PG_MOD_LATTICE: return "Lattice";
    case PG_MOD_ENVELOPE: return "Envelope";
    case PG_MOD_WEIGHT_PROX: return "Vertex Weight Proximity";
    case PG_MOD_WEIGHT_ANGLE: return "Vertex Weight Angle";
    case PG_MOD_DASH: return "Dot Dash";
    case PG_MOD_OUTLINE: return "Outline";
    case PG_MOD_MIRROR: return "Mirror";
    case PG_MOD_ARRAY: return "Array";
    case PG_MOD_MULTIPLY: return "MultipleStrokes";
    case PG_MOD_TEXTURE: return "TextureMapping";
    default: return "";
  }
}

/* Defaults follow DNA_gpencil_modifier_defaults.h of the pinned tree. */
void pg_mod_defaults(int type, float p[PG_MOD_MAX_PARAMS])
{
  memset(p, 0, sizeof(float) * PG_MOD_MAX_PARAMS);
  switch (type) {
    case PG_MOD_THICKNESS: /* thickness_fac 1, thickness 30 */
      p[PG_P_THICK_THICKNESS] = 30.0f;
      p[PG_P_THICK_FACTOR] = 1.0f;
      break;
    case PG_MOD_OPACITY: /* factor 1, modify_color BOTH, hardness 1 */
      p[PG_P_OPACITY_MODIFY] = PG_MODIFY_COLOR_BOTH;
      p[PG_P_OPACITY_FACTOR] = 1.0f;
      p[PG_P_OPACITY_HARDNESS] = 1.0f;
      break;
    case PG_MOD_TINT: /* mode BOTH, factor 0.5, rgb (1, 1, 1) */
      p[PG_P_TINT_MODE] = PG_PAINT_MODE_BOTH;
      p[PG_P_TINT_FACTOR] = 0.5f;
      p[PG_P_TINT_R] = p[PG_P_TINT_G] = p[PG_P_TINT_B] = 1.0f;
      break;
    case PG_MOD_COLOR: /* hsv (0.5, 1, 1), modify_color BOTH */
      p[PG_P_COLOR_MODIFY] = PG_MODIFY_COLOR_BOTH;
      p[PG_P_COLOR_H] = 0.5f;
      p[PG_P_COLOR_S] = 1.0f;
      p[PG_P_COLOR_V] = 1.0f;
      break;
    case PG_MOD_LENGTH: /* start/end/overshoot 0.1, use curvature, density 30, max angle 170 deg */
      p[PG_P_LENGTH_MODE] = PG_LENGTH_RELATIVE;
      p[PG_P_LENGTH_START] = 0.1f;
      p[PG_P_LENGTH_END] = 0.1f;
      p[PG_P_LENGTH_OVERSHOOT] = 0.1f;
      p[PG_P_LENGTH_USE_CURVATURE] = 1.0f;
      p[PG_P_LENGTH_POINT_DENSITY] = 30.0f;
      p[PG_P_LENGTH_MAX_ANGLE] = 170.0f * (float)M_PI / 180.0f;
      break;
    case PG_MOD_SMOOTH: /* factor 1, step 1, smooth location */
      p[PG_P_SMOOTH_FACTOR] = 1.0f;
      p[PG_P_SMOOTH_STEP] = 1.0f;
      p[PG_P_SMOOTH_LOCATION] = 1.0f;
      break;
    case PG_MOD_SIMPLIFY: /* mode fixed, step 1, length 0.1, distance 0.1 */
      p[PG_P_SIMPLIFY_MODE] = GP_SIMPLIFY_FIXED;
      p[PG_P_SIMPLIFY_STEP] = 1.0f;
      p[PG_P_SIMPLIFY_LENGTH] = 0.1f;
      p[PG_P_SIMPLIFY_DISTANCE] = 0.1f;
      break;
    case PG_MOD_SUBDIV: /* level 1, Catmull-Clark */
      p[PG_P_SUBDIV_LEVEL] = 1.0f;
      p[PG_P_SUBDIV_TYPE] = GP_SUBDIV_CATMULL;
      break;
    case PG_MOD_OFFSET: /* random mode, stroke step 1 */
      p[PG_P_OFFSET_MODE] = GP_OFFSET_RANDOM;
      p[PG_P_OFFSET_STROKE_STEP] = 1.0f;
      break;
    case PG_MOD_NOISE: /* factor 0.5, step 4, seed 1, random on */
      p[PG_P_NOISE_FACTOR] = 0.5f;
      p[PG_P_NOISE_STEP] = 4.0f;
      p[PG_P_NOISE_SEED] = 1.0f;
      p[PG_P_NOISE_USE_RANDOM] = 1.0f;
      p[PG_P_NOISE_MODE] = GP_NOISE_RANDOM_STEP;
      break;
    default:
      pg_mod2_defaults(type, p);
      break;
  }
}

static float pgm_clamp(float v, float lo, float hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/* Whole-number parameter within [lo, hi]. */
static float pgm_int(float v, float lo, float hi)
{
  return pgm_clamp(floorf(v + 0.5f), lo, hi);
}

static float pgm_flag(float v)
{
  return v != 0.0f ? 1.0f : 0.0f;
}

void pg_mod_sanitize(int type, float p[PG_MOD_MAX_PARAMS])
{
  float def[PG_MOD_MAX_PARAMS];
  const int n = pg_mod_own_param_count(type);
  pg_mod_defaults(type, def);
  for (int i = 0; i < PG_MOD_MAX_PARAMS; i++) {
    if (i >= n && i < PG_P_CURVE_BASE) {
      p[i] = 0.0f; /* unused slots are always zero */
    }
    else if (!isfinite(p[i])) {
      p[i] = def[i];
    }
  }
  switch (type) {
    case PG_MOD_THICKNESS:
      p[PG_P_THICK_NORMALIZE] = pgm_flag(p[PG_P_THICK_NORMALIZE]);
      p[PG_P_THICK_THICKNESS] = pgm_int(p[PG_P_THICK_THICKNESS], 0.0f, 32767.0f);
      p[PG_P_THICK_FACTOR] = pgm_clamp(p[PG_P_THICK_FACTOR], 0.0f, 100.0f);
      p[PG_P_THICK_USE_VGROUP] = pgm_flag(p[PG_P_THICK_USE_VGROUP]);
      p[PG_P_THICK_VGROUP] = pgm_int(p[PG_P_THICK_VGROUP], 0.0f, 1023.0f);
      p[PG_P_THICK_INVERT_VGROUP] = pgm_flag(p[PG_P_THICK_INVERT_VGROUP]);
      break;
    case PG_MOD_OPACITY:
      p[PG_P_OPACITY_MODIFY] = pgm_int(p[PG_P_OPACITY_MODIFY], PG_MODIFY_COLOR_BOTH, PG_MODIFY_COLOR_HARDNESS);
      p[PG_P_OPACITY_FACTOR] = pgm_clamp(p[PG_P_OPACITY_FACTOR], 0.0f, 2.0f);
      p[PG_P_OPACITY_NORMALIZE] = pgm_flag(p[PG_P_OPACITY_NORMALIZE]);
      p[PG_P_OPACITY_HARDNESS] = pgm_clamp(p[PG_P_OPACITY_HARDNESS], 0.0f, 1.0f);
      break;
    case PG_MOD_TINT:
      p[PG_P_TINT_MODE] = pgm_int(p[PG_P_TINT_MODE], PG_PAINT_MODE_STROKE, PG_PAINT_MODE_BOTH);
      p[PG_P_TINT_FACTOR] = pgm_clamp(p[PG_P_TINT_FACTOR], 0.0f, 2.0f);
      p[PG_P_TINT_R] = pgm_clamp(p[PG_P_TINT_R], 0.0f, 1.0f);
      p[PG_P_TINT_G] = pgm_clamp(p[PG_P_TINT_G], 0.0f, 1.0f);
      p[PG_P_TINT_B] = pgm_clamp(p[PG_P_TINT_B], 0.0f, 1.0f);
      break;
    case PG_MOD_COLOR:
      p[PG_P_COLOR_MODIFY] = pgm_int(p[PG_P_COLOR_MODIFY], PG_MODIFY_COLOR_BOTH, PG_MODIFY_COLOR_FILL);
      p[PG_P_COLOR_H] = pgm_clamp(p[PG_P_COLOR_H], 0.0f, 1.0f);
      p[PG_P_COLOR_S] = pgm_clamp(p[PG_P_COLOR_S], 0.0f, 2.0f);
      p[PG_P_COLOR_V] = pgm_clamp(p[PG_P_COLOR_V], 0.0f, 2.0f);
      break;
    case PG_MOD_LENGTH:
      p[PG_P_LENGTH_MODE] = pgm_int(p[PG_P_LENGTH_MODE], PG_LENGTH_RELATIVE, PG_LENGTH_ABSOLUTE);
      p[PG_P_LENGTH_START] = pgm_clamp(p[PG_P_LENGTH_START], -100.0f, 100.0f);
      p[PG_P_LENGTH_END] = pgm_clamp(p[PG_P_LENGTH_END], -100.0f, 100.0f);
      p[PG_P_LENGTH_OVERSHOOT] = pgm_clamp(p[PG_P_LENGTH_OVERSHOOT], 0.0f, 1.0f);
      p[PG_P_LENGTH_USE_CURVATURE] = pgm_flag(p[PG_P_LENGTH_USE_CURVATURE]);
      p[PG_P_LENGTH_POINT_DENSITY] = pgm_clamp(p[PG_P_LENGTH_POINT_DENSITY], 0.1f, 1000.0f);
      p[PG_P_LENGTH_SEGMENT_INFLUENCE] = pgm_clamp(p[PG_P_LENGTH_SEGMENT_INFLUENCE], -2.0f, 3.0f);
      p[PG_P_LENGTH_MAX_ANGLE] = pgm_clamp(p[PG_P_LENGTH_MAX_ANGLE], 0.0f, (float)M_PI);
      p[PG_P_LENGTH_INVERT_CURVATURE] = pgm_flag(p[PG_P_LENGTH_INVERT_CURVATURE]);
      break;
    case PG_MOD_SMOOTH:
      p[PG_P_SMOOTH_FACTOR] = pgm_clamp(p[PG_P_SMOOTH_FACTOR], 0.0f, 2.0f);
      p[PG_P_SMOOTH_STEP] = pgm_int(p[PG_P_SMOOTH_STEP], 1.0f, 1000.0f);
      p[PG_P_SMOOTH_LOCATION] = pgm_flag(p[PG_P_SMOOTH_LOCATION]);
      p[PG_P_SMOOTH_STRENGTH] = pgm_flag(p[PG_P_SMOOTH_STRENGTH]);
      p[PG_P_SMOOTH_THICKNESS] = pgm_flag(p[PG_P_SMOOTH_THICKNESS]);
      p[PG_P_SMOOTH_UV] = pgm_flag(p[PG_P_SMOOTH_UV]);
      p[PG_P_SMOOTH_KEEP_SHAPE] = pgm_flag(p[PG_P_SMOOTH_KEEP_SHAPE]);
      break;
    case PG_MOD_SIMPLIFY:
      p[PG_P_SIMPLIFY_MODE] = pgm_int(p[PG_P_SIMPLIFY_MODE], GP_SIMPLIFY_FIXED, GP_SIMPLIFY_MERGE);
      p[PG_P_SIMPLIFY_STEP] = pgm_int(p[PG_P_SIMPLIFY_STEP], 1.0f, 100.0f);
      p[PG_P_SIMPLIFY_FACTOR] = pgm_clamp(p[PG_P_SIMPLIFY_FACTOR], 0.0f, 100.0f);
      p[PG_P_SIMPLIFY_LENGTH] = pgm_clamp(p[PG_P_SIMPLIFY_LENGTH], 0.0f, 100.0f);
      p[PG_P_SIMPLIFY_SHARP_THRESHOLD] = pgm_clamp(p[PG_P_SIMPLIFY_SHARP_THRESHOLD], 0.0f, (float)M_PI);
      p[PG_P_SIMPLIFY_DISTANCE] = pgm_clamp(p[PG_P_SIMPLIFY_DISTANCE], 0.0f, 100.0f);
      break;
    case PG_MOD_SUBDIV:
      p[PG_P_SUBDIV_LEVEL] = pgm_int(p[PG_P_SUBDIV_LEVEL], 0.0f, 6.0f);
      p[PG_P_SUBDIV_TYPE] = pgm_int(p[PG_P_SUBDIV_TYPE], GP_SUBDIV_CATMULL, GP_SUBDIV_SIMPLE);
      break;
    case PG_MOD_OFFSET:
      p[PG_P_OFFSET_MODE] = pgm_int(p[PG_P_OFFSET_MODE], GP_OFFSET_RANDOM, GP_OFFSET_STROKE);
      for (int i = PG_P_OFFSET_LOC; i < PG_P_OFFSET_SEED; i++) {
        p[i] = pgm_clamp(p[i], -100000.0f, 100000.0f);
      }
      p[PG_P_OFFSET_SEED] = pgm_int(p[PG_P_OFFSET_SEED], 0.0f, 32767.0f);
      p[PG_P_OFFSET_STROKE_STEP] = pgm_int(p[PG_P_OFFSET_STROKE_STEP], 1.0f, 1000.0f);
      p[PG_P_OFFSET_START_OFFSET] = pgm_int(p[PG_P_OFFSET_START_OFFSET], -1000.0f, 1000.0f);
      p[PG_P_OFFSET_UNIFORM_SCALE] = pgm_flag(p[PG_P_OFFSET_UNIFORM_SCALE]);
      break;
    case PG_MOD_NOISE:
      p[PG_P_NOISE_FACTOR] = pgm_clamp(p[PG_P_NOISE_FACTOR], 0.0f, 1.0f);
      p[PG_P_NOISE_STRENGTH] = pgm_clamp(p[PG_P_NOISE_STRENGTH], 0.0f, 1.0f);
      p[PG_P_NOISE_THICKNESS] = pgm_clamp(p[PG_P_NOISE_THICKNESS], 0.0f, 1.0f);
      p[PG_P_NOISE_UVS] = pgm_clamp(p[PG_P_NOISE_UVS], 0.0f, 1.0f);
      p[PG_P_NOISE_SCALE] = pgm_clamp(p[PG_P_NOISE_SCALE], 0.0f, 1.0f);
      p[PG_P_NOISE_OFFSET] = pgm_clamp(p[PG_P_NOISE_OFFSET], 0.0f, 100.0f);
      p[PG_P_NOISE_SEED] = pgm_int(p[PG_P_NOISE_SEED], 0.0f, 32767.0f);
      p[PG_P_NOISE_STEP] = pgm_int(p[PG_P_NOISE_STEP], 1.0f, 100.0f); /* >= 1: Noise divides by it */
      p[PG_P_NOISE_USE_RANDOM] = pgm_flag(p[PG_P_NOISE_USE_RANDOM]);
      p[PG_P_NOISE_MODE] = pgm_int(p[PG_P_NOISE_MODE], GP_NOISE_RANDOM_STEP, GP_NOISE_RANDOM_KEYFRAME);
      break;
    default:
      break;
  }
  pg_mod2_sanitize(type, p); /* batch 21 types, curve and filter blocks */
}

void pg_mod_entry_init(PGModEntry *entry, int type)
{
  memset(entry, 0, sizeof(*entry));
  entry->type = type;
  entry->enabled = 1;
  pg_mod_defaults(type, entry->params);
}

/* ---------------------------------------------------------------------------------------- */
/* Adapted helpers shared by the ported deform functions                                     */

/* ADAPTED: get_modifier_point_weight() of MOD_gpencil_legacy_util.c with def_nr == -1, i.e. no
 * vertex group: the weight is 1 for every point. The ported code below calls it unchanged. */
static float pg_modifier_point_weight(MDeformVert *dvert, bool inverse, int def_nr)
{
  (void)dvert;
  (void)inverse;
  (void)def_nr;
  return 1.0f;
}
#define get_modifier_point_weight pg_modifier_point_weight

/* ADAPTED: canvas <-> object space (y flip and unit scale); see the header. */
static void pg_stroke_to_object_space(bGPDstroke *gps, float inv_scale)
{
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->x *= inv_scale;
    pt->y = -pt->y * inv_scale;
    pt->z *= inv_scale;
  }
}

static void pg_stroke_from_object_space(bGPDstroke *gps, float scale)
{
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->x *= scale;
    pt->y = -pt->y * scale;
    pt->z *= scale;
  }
}

/* ---------------------------------------------------------------------------------------- */
/* Noise (MOD_gpencil_legacy_noise.c)                                                         */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
static float *noise_table(int len, int offset, int seed)
{
  float *table = MEM_callocN(sizeof(float) * len, __func__);
  for (int i = 0; i < len; i++) {
    table[i] = BLI_hash_int_01(BLI_hash_int_2d(seed, i + offset + 1));
  }
  return table;
}

BLI_INLINE float table_sample(float *table, float x)
{
  return interpf(table[(int)ceilf(x)], table[(int)floor(x)], fractf(x));
}
/* END VERBATIM */

static void pg_deform_noise(const PGModContext *ctx, const float *p, bGPDstroke *gps)
{
  /* ADAPTED: the modifier data is built from the flat parameters. */
  NoiseGpencilModifierData local;
  memset(&local, 0, sizeof(local));
  local.factor = p[PG_P_NOISE_FACTOR];
  local.factor_strength = p[PG_P_NOISE_STRENGTH];
  local.factor_thickness = p[PG_P_NOISE_THICKNESS];
  local.factor_uvs = p[PG_P_NOISE_UVS];
  local.noise_scale = p[PG_P_NOISE_SCALE];
  local.noise_offset = p[PG_P_NOISE_OFFSET];
  local.seed = (int)p[PG_P_NOISE_SEED];
  local.step = (int)p[PG_P_NOISE_STEP];
  local.noise_mode = (short)p[PG_P_NOISE_MODE];
  local.flag = GP_NOISE_FULL_STROKE | (p[PG_P_NOISE_USE_RANDOM] != 0.0f ? GP_NOISE_USE_RANDOM : 0);
  NoiseGpencilModifierData *mmd = &local;
  bGPDframe *gpf = ctx->gpf;
  if (mmd->step < 1) {
    return; /* ADAPTED: Blender's UI keeps step >= 1; the code below divides by it. */
  }

  /* ADAPTED: run in object space (y up, PG_MOD_PIXELS_PER_UNIT canvas pixels per unit). */
  pg_stroke_to_object_space(gps, 1.0f / PG_MOD_PIXELS_PER_UNIT);

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
  MDeformVert *dvert = NULL;
  /* Noise value in range [-1..1] */
  float normal[3];
  float vec1[3], vec2[3];
/* END VERBATIM */
  const int def_nr = -1; /* ADAPTED: no vertex groups */
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
  const bool invert_group = (mmd->flag & GP_NOISE_INVERT_VGROUP) != 0;
/* END VERBATIM */
  const int cfra = ctx->cfra; /* ADAPTED: DEG_get_ctime(depsgraph) */
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
  const bool is_keyframe = (mmd->noise_mode == GP_NOISE_RANDOM_KEYFRAME);
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
  int seed = mmd->seed;
  /* FIXME(fclem): This is really slow. We should get the stroke index in another way. */
  int stroke_seed = BLI_findindex(&gpf->strokes, gps);
  seed += stroke_seed;
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
  /* Make sure different modifiers get different seeds. */
/* END VERBATIM */
  seed += BLI_hash_string(PG_MOD_OBJECT_NAME);       /* ADAPTED: ob->id.name + 2 */
  seed += BLI_hash_string(pg_mod_name(PG_MOD_NOISE)); /* ADAPTED: md->name */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
  if (mmd->flag & GP_NOISE_USE_RANDOM) {
    if (!is_keyframe) {
      seed += cfra / mmd->step;
    }
    else {
      /* If change every keyframe, use the last keyframe. */
      seed += gpf->framenum;
    }
  }
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
  /* Sanitize as it can create out of bound reads. */
  float noise_scale = clamp_f(mmd->noise_scale, 0.0f, 1.0f);

  int len = ceilf(gps->totpoints * noise_scale) + 2;
  float *noise_table_position = (mmd->factor > 0.0f) ?
                                    noise_table(len, (int)floor(mmd->noise_offset), seed + 2) :
                                    NULL;
  float *noise_table_strength = (mmd->factor_strength > 0.0f) ?
                                    noise_table(len, (int)floor(mmd->noise_offset), seed + 3) :
                                    NULL;
  float *noise_table_thickness = (mmd->factor_thickness > 0.0f) ?
                                     noise_table(len, (int)floor(mmd->noise_offset), seed) :
                                     NULL;
  float *noise_table_uvs = (mmd->factor_uvs > 0.0f) ?
                               noise_table(len, (int)floor(mmd->noise_offset), seed + 4) :
                               NULL;

  /* Calculate stroke normal. */
  if (gps->totpoints > 2) {
    BKE_gpencil_stroke_normal(gps, normal);
    if (is_zero_v3(normal)) {
      copy_v3_fl(normal, 1.0f);
    }
  }
  else {
    copy_v3_fl(normal, 1.0f);
  }

  /* move points */
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    /* verify vertex group */
    dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
    float weight = get_modifier_point_weight(dvert, invert_group, def_nr);
    if (weight < 0.0f) {
      continue;
    }

/* END VERBATIM */
    /* ADAPTED: the custom intensity curve (use_curve) is not supported. */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_noise.c */
    if (mmd->factor > 0.0f) {
      /* Offset point randomly around the bi-normal vector. */
      if (gps->totpoints == 1) {
        copy_v3_fl3(vec1, 1.0f, 0.0f, 0.0f);
      }
      else if (i != gps->totpoints - 1) {
        /* Initial vector (p1 -> p0). */
        sub_v3_v3v3(vec1, &gps->points[i].x, &gps->points[i + 1].x);
        /* if vec2 is zero, set to something */
        if (len_squared_v3(vec1) < 1e-8f) {
          copy_v3_fl3(vec1, 1.0f, 0.0f, 0.0f);
        }
      }
      else {
        /* Last point reuse the penultimate normal (still stored in vec1)
         * because the previous point is already modified. */
      }
      /* Vector orthogonal to normal. */
      cross_v3_v3v3(vec2, vec1, normal);
      normalize_v3(vec2);

      float noise = table_sample(noise_table_position,
                                 i * noise_scale + fractf(mmd->noise_offset));
      madd_v3_v3fl(&pt->x, vec2, (noise * 2.0f - 1.0f) * weight * mmd->factor * 0.1f);
    }

    if (mmd->factor_thickness > 0.0f) {
      float noise = table_sample(noise_table_thickness,
                                 i * noise_scale + fractf(mmd->noise_offset));
      pt->pressure *= max_ff(1.0f + (noise * 2.0f - 1.0f) * weight * mmd->factor_thickness, 0.0f);
      CLAMP_MIN(pt->pressure, GPENCIL_STRENGTH_MIN);
    }

    if (mmd->factor_strength > 0.0f) {
      float noise = table_sample(noise_table_strength,
                                 i * noise_scale + fractf(mmd->noise_offset));
      pt->strength *= max_ff(1.0f - noise * weight * mmd->factor_strength, 0.0f);
      CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
    }

    if (mmd->factor_uvs > 0.0f) {
      float noise = table_sample(noise_table_uvs, i * noise_scale + fractf(mmd->noise_offset));
      pt->uv_rot += (noise * 2.0f - 1.0f) * weight * mmd->factor_uvs * M_PI_2;
      CLAMP(pt->uv_rot, -M_PI_2, M_PI_2);
    }
  }

  MEM_SAFE_FREE(noise_table_position);
  MEM_SAFE_FREE(noise_table_strength);
  MEM_SAFE_FREE(noise_table_thickness);
  MEM_SAFE_FREE(noise_table_uvs);
/* END VERBATIM */

  pg_stroke_from_object_space(gps, PG_MOD_PIXELS_PER_UNIT);
}

/* ---------------------------------------------------------------------------------------- */
/* Offset (MOD_gpencil_legacy_offset.c)                                                       */

static void pg_deform_offset(const PGModContext *ctx, const float *p, bGPDstroke *gps)
{
  /* ADAPTED: the modifier data is built from the flat parameters. */
  OffsetGpencilModifierData local;
  memset(&local, 0, sizeof(local));
  local.mode = (int)p[PG_P_OFFSET_MODE];
  memcpy(local.loc, &p[PG_P_OFFSET_LOC], sizeof(float[3]));
  memcpy(local.rot, &p[PG_P_OFFSET_ROT], sizeof(float[3]));
  memcpy(local.scale, &p[PG_P_OFFSET_SCALE], sizeof(float[3]));
  memcpy(local.rnd_offset, &p[PG_P_OFFSET_RND_OFFSET], sizeof(float[3]));
  memcpy(local.rnd_rot, &p[PG_P_OFFSET_RND_ROT], sizeof(float[3]));
  memcpy(local.rnd_scale, &p[PG_P_OFFSET_RND_SCALE], sizeof(float[3]));
  local.seed = (int)p[PG_P_OFFSET_SEED];
  local.stroke_step = (int)p[PG_P_OFFSET_STROKE_STEP];
  local.stroke_start_offset = (int)p[PG_P_OFFSET_START_OFFSET];
  local.flag = p[PG_P_OFFSET_UNIFORM_SCALE] != 0.0f ? GP_OFFSET_UNIFORM_RANDOM_SCALE : 0;
  OffsetGpencilModifierData *mmd = &local;
  bGPDlayer *gpl = ctx->gpl;
  bGPDframe *gpf = ctx->gpf;
  bGPdata *gpd = ctx->gpd; /* ADAPTED: ob->data */
  const int def_nr = -1;   /* ADAPTED: no vertex groups */

  /* ADAPTED: run in object space (y up); Offset's values are canvas pixels, so no unit scale. */
  pg_stroke_to_object_space(gps, 1.0f);

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_offset.c */
  float mat[4][4];
  float loc[3], rot[3], scale[3];
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_offset.c */
  const bool is_randomized = !(is_zero_v3(mmd->rnd_offset) && is_zero_v3(mmd->rnd_rot) &&
                               is_zero_v3(mmd->rnd_scale));
  const bool is_general = !(is_zero_v3(mmd->loc) && is_zero_v3(mmd->rot) &&
                            is_zero_v3(mmd->scale));
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_offset.c */
  int seed = mmd->seed;
  /* Make sure different modifiers get different seeds. */
/* END VERBATIM */
  seed += BLI_hash_string(PG_MOD_OBJECT_NAME);         /* ADAPTED: ob->id.name + 2 */
  seed += BLI_hash_string(pg_mod_name(PG_MOD_OFFSET)); /* ADAPTED: md->name */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_offset.c */
  float rand[3][3];
  float rand_offset = BLI_hash_int_01(seed);
/* END VERBATIM */
  /* ADAPTED: the next line is the pinned `bGPdata *gpd = ob->data;` (gpd is set above). */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_offset.c */
  if (is_randomized && mmd->mode == GP_OFFSET_RANDOM) {
    /* Get stroke index for random offset. */
    int rnd_index = BLI_findindex(&gpf->strokes, gps);
    for (int j = 0; j < 3; j++) {
      const uint primes[3] = {2, 3, 7};
      double offset[3] = {0.0f, 0.0f, 0.0f};
      double r[3];
      /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
      BLI_halton_3d(primes, offset, rnd_index, r);

      if ((mmd->flag & GP_OFFSET_UNIFORM_RANDOM_SCALE) && j == 2) {
        float rand_value;
        rand_value = fmodf(r[0] * 2.0f - 1.0f + rand_offset, 1.0f);
        rand_value = fmodf(sin(rand_value * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f);
        copy_v3_fl(rand[j], rand_value);
      }
      else {
        for (int i = 0; i < 3; i++) {
          rand[j][i] = fmodf(r[i] * 2.0f - 1.0f + rand_offset, 1.0f);
          rand[j][i] = fmodf(sin(rand[j][i] * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f);
        }
      }
    }
  }
  else {
    if (is_randomized) {
      const int step = max_ii(mmd->stroke_step, 1);
      const int start_offset = mmd->stroke_start_offset;
      int offset_index;
      int offset_size;
      float offset_factor;
      switch (mmd->mode) {
        case GP_OFFSET_STROKE:
          offset_size = max_ii(BLI_listbase_count(&gpf->strokes), 1);
          offset_index = max_ii(BLI_findindex(&gpf->strokes, gps), 0);
          break;
        case GP_OFFSET_MATERIAL:
          offset_size = max_ii(gpd->totcol, 1);
          offset_index = max_ii(gps->mat_nr, 0);
          break;
        case GP_OFFSET_LAYER:
          offset_size = max_ii(BLI_listbase_count(&gpd->layers), 1);
          offset_index = max_ii(BLI_findindex(&gpd->layers, gpl), 0);
          break;
      }

      offset_factor = ((offset_size - (offset_index / step + start_offset % offset_size) %
                                          offset_size * step % offset_size) -
                       1) /
                      (float)offset_size;
      for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
          rand[j][i] = offset_factor;
        }
      }
    }
  }
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    /* Verify vertex group. */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_OFFSET_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    /* Calculate Random matrix. */
    if (is_randomized) {
      float mat_rnd[4][4];
      float rnd_loc[3], rnd_rot[3], rnd_scale_weight[3];
      float rnd_scale[3] = {1.0f, 1.0f, 1.0f};

      mul_v3_v3fl(rnd_loc, rand[0], weight);
      mul_v3_v3fl(rnd_rot, rand[1], weight);
      mul_v3_v3fl(rnd_scale_weight, rand[2], weight);

      mul_v3_v3v3(rnd_loc, mmd->rnd_offset, rnd_loc);
      mul_v3_v3v3(rnd_rot, mmd->rnd_rot, rnd_rot);
      madd_v3_v3v3(rnd_scale, mmd->rnd_scale, rnd_scale_weight);

      loc_eul_size_to_mat4(mat_rnd, rnd_loc, rnd_rot, rnd_scale);
      /* Apply randomness matrix. */
      mul_m4_v3(mat_rnd, &pt->x);
    }

    /* Calculate matrix. */
    if (is_general) {
      mul_v3_v3fl(loc, mmd->loc, weight);
      mul_v3_v3fl(rot, mmd->rot, weight);
      mul_v3_v3fl(scale, mmd->scale, weight);
      add_v3_fl(scale, 1.0f);
      loc_eul_size_to_mat4(mat, loc, rot, scale);

      /* Apply scale to thickness. */
      float unit_scale = (fabsf(scale[0]) + fabsf(scale[1]) + fabsf(scale[2])) / 3.0f;
      pt->pressure *= unit_scale;

      mul_m4_v3(mat, &pt->x);
    }
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
/* END VERBATIM */

  /* ADAPTED: back to canvas space; the geometry update above ran on mirrored points, so redo it. */
  pg_stroke_from_object_space(gps, 1.0f);
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/* ---------------------------------------------------------------------------------------- */
/* Smooth, Simplify, Subdivide (MOD_gpencil_legacy_{smooth,simplify,subdiv}.c)               */

static void pg_deform_smooth(const float *p, bGPDstroke *gps)
{
  SmoothGpencilModifierData local;
  memset(&local, 0, sizeof(local));
  local.factor = p[PG_P_SMOOTH_FACTOR];
  local.step = (int)p[PG_P_SMOOTH_STEP];
  local.flag = (p[PG_P_SMOOTH_LOCATION] != 0.0f ? GP_SMOOTH_MOD_LOCATION : 0) |
               (p[PG_P_SMOOTH_STRENGTH] != 0.0f ? GP_SMOOTH_MOD_STRENGTH : 0) |
               (p[PG_P_SMOOTH_THICKNESS] != 0.0f ? GP_SMOOTH_MOD_THICKNESS : 0) |
               (p[PG_P_SMOOTH_UV] != 0.0f ? GP_SMOOTH_MOD_UV : 0) |
               (p[PG_P_SMOOTH_KEEP_SHAPE] != 0.0f ? GP_SMOOTH_KEEP_SHAPE : 0);
  SmoothGpencilModifierData *mmd = &local;

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_smooth.c */
  if (mmd->factor <= 0.0f || mmd->step <= 0) {
    return;
  }
/* END VERBATIM */

  /* ADAPTED: no vertex groups or custom curve, so no per-point weights. */
  float *weights = NULL;
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_smooth.c */
  BKE_gpencil_stroke_smooth(gps,
                            mmd->factor,
                            mmd->step,
                            mmd->flag & GP_SMOOTH_MOD_LOCATION,
                            mmd->flag & GP_SMOOTH_MOD_STRENGTH,
                            mmd->flag & GP_SMOOTH_MOD_THICKNESS,
                            mmd->flag & GP_SMOOTH_MOD_UV,
                            mmd->flag & GP_SMOOTH_KEEP_SHAPE,
                            weights);
  MEM_SAFE_FREE(weights);
/* END VERBATIM */
}

static void pg_deform_simplify(const PGModContext *ctx, const float *p, bGPDstroke *gps)
{
  SimplifyGpencilModifierData local;
  memset(&local, 0, sizeof(local));
  local.mode = (short)p[PG_P_SIMPLIFY_MODE];
  local.step = (short)p[PG_P_SIMPLIFY_STEP];
  local.factor = p[PG_P_SIMPLIFY_FACTOR];
  local.length = p[PG_P_SIMPLIFY_LENGTH];
  local.sharp_threshold = p[PG_P_SIMPLIFY_SHARP_THRESHOLD];
  local.distance = p[PG_P_SIMPLIFY_DISTANCE];
  SimplifyGpencilModifierData *mmd = &local;
  bGPdata *gpd = ctx->gpd; /* ADAPTED: ob->data */
  bGPDframe *gpf = ctx->gpf;

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_simplify.c */
  /* Select simplification mode. */
  switch (mmd->mode) {
    case GP_SIMPLIFY_FIXED: {
      for (int i = 0; i < mmd->step; i++) {
        BKE_gpencil_stroke_simplify_fixed(gpd, gps);
      }
      break;
    }
    case GP_SIMPLIFY_ADAPTIVE: {
      /* simplify stroke using Ramer-Douglas-Peucker algorithm */
      BKE_gpencil_stroke_simplify_adaptive(gpd, gps, mmd->factor);
      break;
    }
    case GP_SIMPLIFY_SAMPLE: {
      BKE_gpencil_stroke_sample(gpd, gps, mmd->length, false, mmd->sharp_threshold);
      break;
    }
    case GP_SIMPLIFY_MERGE: {
      BKE_gpencil_stroke_merge_distance(gpd, gpf, gps, mmd->distance, true);
      break;
    }
    default:
      break;
  }
/* END VERBATIM */
}

static void pg_deform_subdiv(const PGModContext *ctx, const float *p, bGPDstroke *gps)
{
  SubdivGpencilModifierData local;
  memset(&local, 0, sizeof(local));
  local.level = (int)p[PG_P_SUBDIV_LEVEL];
  local.type = (short)p[PG_P_SUBDIV_TYPE];
  SubdivGpencilModifierData *mmd = &local;
  bGPdata *gpd = ctx->gpd; /* ADAPTED: ob->data */

/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_subdiv.c */
  /* For strokes with less than 3 points, only the Simple Subdivision makes sense. */
  short type = gps->totpoints < 3 ? GP_SUBDIV_SIMPLE : mmd->type;

  BKE_gpencil_stroke_subdivide(gpd, gps, mmd->level, type);
/* END VERBATIM */
}

/* ---------------------------------------------------------------------------------------- */
/* Dispatch, evaluation and apply                                                            */

int pg_mod_deform_stroke(const PGModContext *ctx, const PGModEntry *e, bGPDstroke *gps)
{
  if (ctx == NULL || ctx->gpd == NULL || e == NULL || gps == NULL || gps->points == NULL ||
      gps->totpoints <= 0)
  {
    return 0;
  }
  const float *p = e->params;
  switch (e->type) {
    case PG_MOD_THICKNESS:
      /* The optional vertex group: weights stay with the strokes (dvert), so evaluation on the
       * copies and Apply on the originals see the same weights. */
      return pg_gp_modstroke_thickness_vgroup(
          gps, p[PG_P_THICK_USE_VGROUP] != 0.0f ? (int)p[PG_P_THICK_VGROUP] : -1,
          p[PG_P_THICK_INVERT_VGROUP] != 0.0f, p[PG_P_THICK_NORMALIZE] != 0.0f,
          (int)p[PG_P_THICK_THICKNESS], p[PG_P_THICK_FACTOR]);
    case PG_MOD_OPACITY:
      return pg_gp_modstroke_opacity(gps, (int)p[PG_P_OPACITY_MODIFY], p[PG_P_OPACITY_FACTOR],
                                     p[PG_P_OPACITY_NORMALIZE] != 0.0f, p[PG_P_OPACITY_HARDNESS]);
    case PG_MOD_TINT: {
      const float rgb[3] = {p[PG_P_TINT_R], p[PG_P_TINT_G], p[PG_P_TINT_B]};
      return pg_gp_modstroke_tint(ctx->gpd, gps, (int)p[PG_P_TINT_MODE], p[PG_P_TINT_FACTOR], rgb);
    }
    case PG_MOD_COLOR: {
      const float hsv[3] = {p[PG_P_COLOR_H], p[PG_P_COLOR_S], p[PG_P_COLOR_V]};
      return pg_gp_modstroke_color(ctx->gpd, gps, (int)p[PG_P_COLOR_MODIFY], hsv);
    }
    case PG_MOD_LENGTH: {
      PGLengthParams lp;
      lp.mode = (int)p[PG_P_LENGTH_MODE];
      lp.start_fac = p[PG_P_LENGTH_START];
      lp.end_fac = p[PG_P_LENGTH_END];
      lp.overshoot_fac = p[PG_P_LENGTH_OVERSHOOT];
      lp.use_curvature = p[PG_P_LENGTH_USE_CURVATURE] != 0.0f;
      lp.point_density = p[PG_P_LENGTH_POINT_DENSITY];
      lp.segment_influence = p[PG_P_LENGTH_SEGMENT_INFLUENCE];
      lp.max_angle = p[PG_P_LENGTH_MAX_ANGLE];
      lp.invert_curvature = p[PG_P_LENGTH_INVERT_CURVATURE] != 0.0f;
      return pg_gp_modstroke_length(ctx->gpd, gps, &lp);
    }
    case PG_MOD_SMOOTH:
      pg_deform_smooth(p, gps);
      return 1;
    case PG_MOD_SIMPLIFY:
      pg_deform_simplify(ctx, p, gps);
      return 1;
    case PG_MOD_SUBDIV:
      pg_deform_subdiv(ctx, p, gps);
      return 1;
    case PG_MOD_OFFSET:
      pg_deform_offset(ctx, p, gps);
      return 1;
    case PG_MOD_NOISE:
      pg_deform_noise(ctx, p, gps);
      return 1;
    default:
      return pg_mod2_deform_stroke(ctx, e, gps);
  }
}

/* Runs one entry on every stroke of `gpf`. A deform function may remove the stroke it is given
 * (Simplify/Merge), so the next pointer is read first. */
/* Batch 21: strokes outside the entry's filter are skipped; with a vertex-group filter or a custom
 * curve the point attributes are blended between before and after by the point influence. */
static int pg_mod_run_on_frame(const PGModContext *ctx, const PGModEntry *entry)
{
  if (pg_mod2_is_frame_level(entry->type)) {
    return pg_mod2_run_frame(ctx, entry);
  }
  int any = 0;
  const int blend = pg_mod_has_point_influence(entry);
  for (bGPDstroke *gps = ctx->gpf->strokes.first, *next; gps != NULL; gps = next) {
    next = gps->next;
    if (!pg_mod_stroke_affected(ctx, entry, gps)) {
      continue;
    }
    bGPDspoint *before = NULL;
    const int n = gps->totpoints;
    if (blend && n > 0 && gps->points != NULL) {
      before = MEM_dupallocN(gps->points);
    }
    const int changed = pg_mod_deform_stroke(ctx, entry, gps);
    if (before != NULL) {
      /* the deform may have removed the stroke (Simplify/Merge): only blend when it is still here */
      int alive = 0;
      for (bGPDstroke *s = ctx->gpf->strokes.first; s != NULL; s = s->next) {
        if (s == gps) { alive = 1; break; }
      }
      if (alive && changed && gps->totpoints == n) {
        for (int i = 0; i < n; i++) {
          const float w = pg_mod_point_influence(entry, gps, i);
          bGPDspoint *pt = &gps->points[i];
          const bGPDspoint *o = &before[i];
          pt->x = o->x + (pt->x - o->x) * w;
          pt->y = o->y + (pt->y - o->y) * w;
          pt->z = o->z + (pt->z - o->z) * w;
          pt->pressure = o->pressure + (pt->pressure - o->pressure) * w;
          pt->strength = o->strength + (pt->strength - o->strength) * w;
          for (int c = 0; c < 4; c++) {
            pt->vert_color[c] = o->vert_color[c] + (pt->vert_color[c] - o->vert_color[c]) * w;
          }
        }
      }
      MEM_freeN(before);
    }
    any |= changed;
  }
  return any;
}

int pg_mod_eval_frame(bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf, const PGModEntry *entries,
                      int count, int cfra, bGPDframe *r_eval)
{
  if (r_eval == NULL) {
    return 0;
  }
  memset(r_eval, 0, sizeof(*r_eval));
  if (gpd == NULL || gpf == NULL) {
    return 0;
  }
  r_eval->framenum = gpf->framenum;
  r_eval->flag = gpf->flag;

  int n = 0;
  for (bGPDstroke *gps = gpf->strokes.first; gps != NULL; gps = gps->next) {
    bGPDstroke *copy = BKE_gpencil_stroke_duplicate(gps, true, true);
    if (copy == NULL) {
      continue;
    }
    BLI_addtail(&r_eval->strokes, copy);
    n++;
  }

  PGModContext ctx = {gpd, gpl, r_eval, cfra, gpf};
  for (int i = 0; i < count && entries != NULL; i++) {
    if (entries[i].enabled && pg_mod_valid_type(entries[i].type)) {
      pg_mod_run_on_frame(&ctx, &entries[i]);
    }
  }
  /* Recompute triangulation and bounding data of the evaluated copies. Texture Mapping sets
   * uv_fac / uv_rot after its own geometry update (MOD_gpencil_legacy_texture.c), and Blender does
   * not recompute them afterwards, so they survive this pass. */
  int keep_uv = 0;
  for (int i = 0; i < count && entries != NULL; i++) {
    if (entries[i].enabled && entries[i].type == PG_MOD_TEXTURE) keep_uv = 1;
  }
  n = 0;
  for (bGPDstroke *gps = r_eval->strokes.first; gps != NULL; gps = gps->next) {
    float *uv = NULL;
    if (keep_uv && gps->totpoints > 0) {
      uv = MEM_malloc_arrayN((size_t)gps->totpoints * 2, sizeof(float), "pg_keep_uv");
      for (int i = 0; i < gps->totpoints; i++) { uv[2 * i] = gps->points[i].uv_fac; uv[2 * i + 1] = gps->points[i].uv_rot; }
    }
    BKE_gpencil_stroke_geometry_update(gpd, gps);
    if (uv) {
      for (int i = 0; i < gps->totpoints; i++) { gps->points[i].uv_fac = uv[2 * i]; gps->points[i].uv_rot = uv[2 * i + 1]; }
      MEM_freeN(uv);
    }
    n++;
  }
  return n;
}

void pg_mod_eval_free(bGPDframe *eval)
{
  if (eval == NULL) {
    return;
  }
  for (bGPDstroke *gps = eval->strokes.first, *next; gps != NULL; gps = next) {
    next = gps->next;
    BKE_gpencil_free_stroke(gps);
  }
  BLI_listbase_clear(&eval->strokes);
}

int pg_mod_apply(bGPdata *gpd, bGPDlayer *gpl, const PGModEntry *entry, int cfra)
{
  if (gpd == NULL || gpl == NULL || entry == NULL || !pg_mod_valid_type(entry->type)) {
    return 0;
  }
  int any = 0;
  for (bGPDframe *gpf = gpl->frames.first; gpf != NULL; gpf = gpf->next) {
    PGModContext ctx = {gpd, gpl, gpf, cfra, gpf};
    const int changed = pg_mod_run_on_frame(&ctx, entry);
    if (changed) {
      for (bGPDstroke *gps = gpf->strokes.first; gps != NULL; gps = gps->next) {
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }
    }
    any |= changed;
  }
  return any;
}
