/* L4: Project Grease modifier parameters that are Blender RNA properties, and the sanitize pass
 * that gives them Blender 3.6.23's RNA limits (table: project_grease_rna_gen.c). Parameters PG
 * adapted to its 2D canvas (pixels, canvas points) are not listed and keep PG's own limits. */
#include <math.h>
#include <string.h>

#include "DNA_gpencil_modifier_types.h"

#include "project_grease_modifier_stack.h"
#include "project_grease_rna.h"

const PGModRnaMap pg_mod_rna_map[] = {
    {PG_MOD_THICKNESS, PG_P_THICK_THICKNESS, "ThickGpencilModifier", "thickness"},
    {PG_MOD_THICKNESS, PG_P_THICK_FACTOR, "ThickGpencilModifier", "thickness_factor"},
    {PG_MOD_OPACITY, PG_P_OPACITY_MODIFY, "OpacityGpencilModifier", "modify_color"},
    {PG_MOD_OPACITY, PG_P_OPACITY_FACTOR, "OpacityGpencilModifier", "factor"},
    {PG_MOD_OPACITY, PG_P_OPACITY_HARDNESS, "OpacityGpencilModifier", "hardness"},
    {PG_MOD_TINT, PG_P_TINT_MODE, "TintGpencilModifier", "vertex_mode"},
    {PG_MOD_TINT, PG_P_TINT_FACTOR, "TintGpencilModifier", "factor"},
    {PG_MOD_COLOR, PG_P_COLOR_MODIFY, "ColorGpencilModifier", "modify_color"},
    {PG_MOD_LENGTH, PG_P_LENGTH_MODE, "LengthGpencilModifier", "mode"},
    {PG_MOD_LENGTH, PG_P_LENGTH_START, "LengthGpencilModifier", "start_factor"},
    {PG_MOD_LENGTH, PG_P_LENGTH_END, "LengthGpencilModifier", "end_factor"},
    {PG_MOD_LENGTH, PG_P_LENGTH_OVERSHOOT, "LengthGpencilModifier", "overshoot_factor"},
    {PG_MOD_LENGTH, PG_P_LENGTH_POINT_DENSITY, "LengthGpencilModifier", "point_density"},
    {PG_MOD_LENGTH, PG_P_LENGTH_SEGMENT_INFLUENCE, "LengthGpencilModifier", "segment_influence"},
    {PG_MOD_LENGTH, PG_P_LENGTH_MAX_ANGLE, "LengthGpencilModifier", "max_angle"},
    {PG_MOD_LENGTH, PG_P_LENGTH_RAND_START, "LengthGpencilModifier", "random_start_factor"},
    {PG_MOD_LENGTH, PG_P_LENGTH_RAND_END, "LengthGpencilModifier", "random_end_factor"},
    {PG_MOD_LENGTH, PG_P_LENGTH_RAND_OFFSET, "LengthGpencilModifier", "random_offset"},
    {PG_MOD_LENGTH, PG_P_LENGTH_SEED, "LengthGpencilModifier", "seed"},
    {PG_MOD_LENGTH, PG_P_LENGTH_STEP, "LengthGpencilModifier", "step"},
    {PG_MOD_SMOOTH, PG_P_SMOOTH_FACTOR, "SmoothGpencilModifier", "factor"},
    {PG_MOD_SMOOTH, PG_P_SMOOTH_STEP, "SmoothGpencilModifier", "step"},
    {PG_MOD_SIMPLIFY, PG_P_SIMPLIFY_MODE, "SimplifyGpencilModifier", "mode"},
    {PG_MOD_SIMPLIFY, PG_P_SIMPLIFY_STEP, "SimplifyGpencilModifier", "step"},
    {PG_MOD_SIMPLIFY, PG_P_SIMPLIFY_FACTOR, "SimplifyGpencilModifier", "factor"},
    {PG_MOD_SIMPLIFY, PG_P_SIMPLIFY_LENGTH, "SimplifyGpencilModifier", "length"},
    {PG_MOD_SIMPLIFY, PG_P_SIMPLIFY_SHARP_THRESHOLD, "SimplifyGpencilModifier", "sharp_threshold"},
    {PG_MOD_SIMPLIFY, PG_P_SIMPLIFY_DISTANCE, "SimplifyGpencilModifier", "distance"},
    {PG_MOD_SUBDIV, PG_P_SUBDIV_LEVEL, "SubdivGpencilModifier", "level"},
    {PG_MOD_SUBDIV, PG_P_SUBDIV_TYPE, "SubdivGpencilModifier", "subdivision_type"},
    {PG_MOD_OFFSET, PG_P_OFFSET_MODE, "OffsetGpencilModifier", "mode"},
    {PG_MOD_OFFSET, PG_P_OFFSET_SEED, "OffsetGpencilModifier", "seed"},
    {PG_MOD_OFFSET, PG_P_OFFSET_STROKE_STEP, "OffsetGpencilModifier", "stroke_step"},
    {PG_MOD_OFFSET, PG_P_OFFSET_START_OFFSET, "OffsetGpencilModifier", "stroke_start_offset"},
    {PG_MOD_NOISE, PG_P_NOISE_FACTOR, "NoiseGpencilModifier", "factor"},
    {PG_MOD_NOISE, PG_P_NOISE_STRENGTH, "NoiseGpencilModifier", "factor_strength"},
    {PG_MOD_NOISE, PG_P_NOISE_THICKNESS, "NoiseGpencilModifier", "factor_thickness"},
    {PG_MOD_NOISE, PG_P_NOISE_UVS, "NoiseGpencilModifier", "factor_uvs"},
    {PG_MOD_NOISE, PG_P_NOISE_SCALE, "NoiseGpencilModifier", "noise_scale"},
    {PG_MOD_NOISE, PG_P_NOISE_OFFSET, "NoiseGpencilModifier", "noise_offset"},
    {PG_MOD_NOISE, PG_P_NOISE_SEED, "NoiseGpencilModifier", "seed"},
    {PG_MOD_NOISE, PG_P_NOISE_STEP, "NoiseGpencilModifier", "step"},
    {PG_MOD_NOISE, PG_P_NOISE_MODE, "NoiseGpencilModifier", "random_mode"},
    {PG_MOD_BUILD, PG_P_BUILD_MODE, "BuildGpencilModifier", "mode"},
    {PG_MOD_BUILD, PG_P_BUILD_TRANSITION, "BuildGpencilModifier", "transition"},
    {PG_MOD_BUILD, PG_P_BUILD_START, "BuildGpencilModifier", "start_delay"},
    {PG_MOD_BUILD, PG_P_BUILD_LENGTH, "BuildGpencilModifier", "length"},
    {PG_MOD_BUILD, PG_P_BUILD_TIME_ALIGN, "BuildGpencilModifier", "concurrent_time_alignment"},
    {PG_MOD_BUILD, PG_P_BUILD_FADE_FAC, "BuildGpencilModifier", "fade_factor"},
    {PG_MOD_BUILD, PG_P_BUILD_FADE_THICKNESS, "BuildGpencilModifier", "fade_thickness_strength"},
    {PG_MOD_BUILD, PG_P_BUILD_FADE_OPACITY, "BuildGpencilModifier", "fade_opacity_strength"},
    {PG_MOD_TIME, PG_P_TIME_MODE, "TimeGpencilModifier", "mode"},
    {PG_MOD_TIME, PG_P_TIME_OFFSET, "TimeGpencilModifier", "offset"},
    {PG_MOD_TIME, PG_P_TIME_SCALE, "TimeGpencilModifier", "frame_scale"},
    {PG_MOD_TIME, PG_P_TIME_SFRA, "TimeGpencilModifier", "frame_start"},
    {PG_MOD_TIME, PG_P_TIME_EFRA, "TimeGpencilModifier", "frame_end"},
    {PG_MOD_HOOK, PG_P_HOOK_STRENGTH, "HookGpencilModifier", "strength"},
    {PG_MOD_LATTICE, PG_P_LATTICE_STRENGTH, "LatticeGpencilModifier", "strength"},
    {PG_MOD_ENVELOPE, PG_P_ENVELOPE_MODE, "EnvelopeGpencilModifier", "mode"},
    {PG_MOD_ENVELOPE, PG_P_ENVELOPE_SPREAD, "EnvelopeGpencilModifier", "spread"},
    {PG_MOD_ENVELOPE, PG_P_ENVELOPE_SKIP, "EnvelopeGpencilModifier", "skip"},
    {PG_MOD_ENVELOPE, PG_P_ENVELOPE_THICKNESS, "EnvelopeGpencilModifier", "thickness"},
    {PG_MOD_ENVELOPE, PG_P_ENVELOPE_STRENGTH, "EnvelopeGpencilModifier", "strength"},
    {PG_MOD_ENVELOPE, PG_P_ENVELOPE_MAT, "EnvelopeGpencilModifier", "mat_nr"},
    {PG_MOD_WEIGHT_PROX, PG_P_WPROX_DIST_START, "WeightProxGpencilModifier", "distance_start"},
    {PG_MOD_WEIGHT_PROX, PG_P_WPROX_MIN_WEIGHT, "WeightProxGpencilModifier", "minimum_weight"},
    {PG_MOD_WEIGHT_ANGLE, PG_P_WANGLE_ANGLE, "WeightAngleGpencilModifier", "angle"},
    {PG_MOD_WEIGHT_ANGLE, PG_P_WANGLE_MIN_WEIGHT, "WeightAngleGpencilModifier", "minimum_weight"},
    {PG_MOD_DASH, PG_P_DASH_DASH, "DashGpencilModifierSegment", "dash"},
    {PG_MOD_DASH, PG_P_DASH_GAP, "DashGpencilModifierSegment", "gap"},
    {PG_MOD_DASH, PG_P_DASH_OFFSET, "DashGpencilModifierData", "dash_offset"},
    {PG_MOD_OUTLINE, PG_P_OUTLINE_SUBDIV, "OutlineGpencilModifier", "subdivision"},
    {PG_MOD_ARRAY, PG_P_ARRAY_COUNT_N, "ArrayGpencilModifier", "count"},
    {PG_MOD_MULTIPLY, PG_P_MULTIPLY_DUPLICATIONS, "MultiplyGpencilModifier", "duplicates"},
    {PG_MOD_TEXTURE, PG_P_TEXTURE_MODE, "TextureGpencilModifier", "mode"},
    {PG_MOD_TEXTURE, PG_P_TEXTURE_FIT, "TextureGpencilModifier", "fit_method"},
    {PG_MOD_TEXTURE, PG_P_TEXTURE_UV_OFFSET, "TextureGpencilModifier", "uv_offset"},
    {PG_MOD_TEXTURE, PG_P_TEXTURE_UV_SCALE, "TextureGpencilModifier", "uv_scale"},
    {PG_MOD_TEXTURE, PG_P_TEXTURE_ALIGN_ROT, "TextureGpencilModifier", "alignment_rotation"},
    {PG_MOD_TEXTURE, PG_P_TEXTURE_FILL_ROT, "TextureGpencilModifier", "fill_rotation"},
    {PG_MOD_TEXTURE, PG_P_TEXTURE_FILL_SCALE, "TextureGpencilModifier", "fill_scale"},
};
const int pg_mod_rna_map_len = (int)(sizeof(pg_mod_rna_map) / sizeof(pg_mod_rna_map[0]));

/* Largest float below 2^31: PG stores int parameters as floats, and (int)2147483648.0f is undefined.
 * ADAPTED: Blender's int hard maximum INT_MAX becomes this. */
#define PG_FLOAT_INT_MAX 2147483520.0

void pg_mod_rna_sanitize(int type, float p[PG_MOD_MAX_PARAMS], const float def[PG_MOD_MAX_PARAMS])
{
  for (int i = 0; i < pg_mod_rna_map_len; i++) {
    const PGModRnaMap *m = &pg_mod_rna_map[i];
    if (m->type != type) {
      continue;
    }
    const PGRnaProperty *rp = pg_rna_find(m->srna, m->prop);
    float *v = &p[m->index];
    if (rp->type == PG_RNA_ENUM) {
      /* RNA enums only hold item values; anything else falls back to the DNA default. */
      if (!pg_rna_enum_valid(rp, (int)*v) || *v != floorf(*v)) {
        *v = def[m->index];
      }
      continue;
    }
    double lo = rp->hardmin, hi = rp->hardmax;
    if (rp->range_fn && strcmp(rp->range_fn, "rna_GpencilOpacity_range") == 0) {
      OpacityGpencilModifierData md = {0};
      md.flag = p[PG_P_OPACITY_NORMALIZE] != 0.0f ? GP_OPACITY_NORMALIZE : 0;
      float flo, fhi, slo, shi;
      pg_rna_GpencilOpacity_range(&md, &flo, &fhi, &slo, &shi);
      lo = flo;
      hi = fhi;
    }
    double x = *v;
    if (rp->type == PG_RNA_INT) {
      x = floor(x + 0.5); /* as pgm_int */
      if (hi > PG_FLOAT_INT_MAX) {
        hi = PG_FLOAT_INT_MAX;
      }
      if (lo < -PG_FLOAT_INT_MAX) {
        lo = -PG_FLOAT_INT_MAX;
      }
    }
    /* makesrna rna_clamp_value(): CLAMPIS(value, hardmin, hardmax), or the range function's.
     * shortcut: Time frame_start/frame_end get the clamp of their custom setters but not the
     * "move the other end" side effect, which needs to know the edited field; add it when PG gets
     * a per-parameter set API. */
    *v = (float)(x < lo ? lo : (x > hi ? hi : x));
  }
}
