/* L3 host test: Blender 3.6.23's DNA defaults table (makesdna/intern/dna_defaults.c, verbatim) is
 * linked, and every Project Grease default that has a direct Blender DNA meaning is compared with
 * DNA_struct_default_get() for that struct. Parameters PG adapted to its 2D canvas (pixels, canvas
 * center, no object/vertex-group references) are listed as NOT COMPARED with the reason, never
 * silently skipped. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_brush_enums.h"

#include "project_grease_modifier_stack.h"

static int failures = 0, compared = 0, not_compared = 0;

static void cmp(const char *what, double pg, double dna)
{
  compared++;
  if (fabs(pg - dna) > 1e-6 * fmax(1.0, fabs(dna))) {
    printf("FAIL: %s: PG %.9g != Blender DNA %.9g\n", what, pg, dna);
    failures++;
  }
}
static void skip(const char *what, const char *why)
{
  not_compared++;
  printf("  NOT COMPARED %s: %s\n", what, why);
}
#define FLAG(v, f) (((v) & (f)) ? 1.0 : 0.0)

static void mod(int type, float p[PG_MOD_MAX_PARAMS])
{
  pg_mod_defaults(type, p); /* zeroes, then fills (stack2 types through pg_mod2_defaults) */
}

int main(void)
{
  float p[PG_MOD_MAX_PARAMS];

  const ThickGpencilModifierData *th = DNA_struct_default_get(ThickGpencilModifierData);
  mod(PG_MOD_THICKNESS, p);
  cmp("Thickness.thickness", p[PG_P_THICK_THICKNESS], th->thickness);
  cmp("Thickness.thickness_fac", p[PG_P_THICK_FACTOR], th->thickness_fac);
  cmp("Thickness.normalize", p[PG_P_THICK_NORMALIZE], FLAG(th->flag, GP_THICK_NORMALIZE));

  const OpacityGpencilModifierData *op = DNA_struct_default_get(OpacityGpencilModifierData);
  mod(PG_MOD_OPACITY, p);
  cmp("Opacity.modify_color", p[PG_P_OPACITY_MODIFY], op->modify_color);
  cmp("Opacity.factor", p[PG_P_OPACITY_FACTOR], op->factor);
  cmp("Opacity.hardeness", p[PG_P_OPACITY_HARDNESS], op->hardeness);
  cmp("Opacity.normalize", p[PG_P_OPACITY_NORMALIZE], FLAG(op->flag, GP_OPACITY_NORMALIZE));

  const TintGpencilModifierData *ti = DNA_struct_default_get(TintGpencilModifierData);
  mod(PG_MOD_TINT, p);
  cmp("Tint.mode", p[PG_P_TINT_MODE], ti->mode);
  cmp("Tint.factor", p[PG_P_TINT_FACTOR], ti->factor);
  for (int i = 0; i < 3; i++) {
    cmp("Tint.rgb", p[PG_P_TINT_R + i], ti->rgb[i]);
  }
  skip("Tint.type/radius/object/colorband", "PG has uniform tint only (no gradient object)");

  const ColorGpencilModifierData *co = DNA_struct_default_get(ColorGpencilModifierData);
  mod(PG_MOD_COLOR, p);
  cmp("Color.modify_color", p[PG_P_COLOR_MODIFY], co->modify_color);
  for (int i = 0; i < 3; i++) {
    cmp("Color.hsv", p[PG_P_COLOR_H + i], co->hsv[i]);
  }

  const LengthGpencilModifierData *le = DNA_struct_default_get(LengthGpencilModifierData);
  mod(PG_MOD_LENGTH, p);
  cmp("Length.mode", p[PG_P_LENGTH_MODE], le->mode);
  cmp("Length.start_fac", p[PG_P_LENGTH_START], le->start_fac);
  cmp("Length.end_fac", p[PG_P_LENGTH_END], le->end_fac);
  cmp("Length.overshoot_fac", p[PG_P_LENGTH_OVERSHOOT], le->overshoot_fac);
  cmp("Length.use_curvature", p[PG_P_LENGTH_USE_CURVATURE], FLAG(le->flag, GP_LENGTH_USE_CURVATURE));
  cmp("Length.invert_curvature", p[PG_P_LENGTH_INVERT_CURVATURE], FLAG(le->flag, GP_LENGTH_INVERT_CURVATURE));
  cmp("Length.use_random", p[PG_P_LENGTH_USE_RANDOM], FLAG(le->flag, GP_LENGTH_USE_RANDOM));
  cmp("Length.point_density", p[PG_P_LENGTH_POINT_DENSITY], le->point_density);
  cmp("Length.segment_influence", p[PG_P_LENGTH_SEGMENT_INFLUENCE], le->segment_influence);
  cmp("Length.max_angle", p[PG_P_LENGTH_MAX_ANGLE], le->max_angle);
  cmp("Length.rand_start_fac", p[PG_P_LENGTH_RAND_START], le->rand_start_fac);
  cmp("Length.rand_end_fac", p[PG_P_LENGTH_RAND_END], le->rand_end_fac);
  cmp("Length.rand_offset", p[PG_P_LENGTH_RAND_OFFSET], le->rand_offset);
  cmp("Length.seed", p[PG_P_LENGTH_SEED], le->seed);
  cmp("Length.step", p[PG_P_LENGTH_STEP], le->step);

  const SmoothGpencilModifierData *sm = DNA_struct_default_get(SmoothGpencilModifierData);
  mod(PG_MOD_SMOOTH, p);
  cmp("Smooth.factor", p[PG_P_SMOOTH_FACTOR], sm->factor);
  cmp("Smooth.step", p[PG_P_SMOOTH_STEP], sm->step);
  cmp("Smooth.use_edit_position", p[PG_P_SMOOTH_LOCATION], FLAG(sm->flag, GP_SMOOTH_MOD_LOCATION));
  cmp("Smooth.use_edit_strength", p[PG_P_SMOOTH_STRENGTH], FLAG(sm->flag, GP_SMOOTH_MOD_STRENGTH));
  cmp("Smooth.use_edit_thickness", p[PG_P_SMOOTH_THICKNESS], FLAG(sm->flag, GP_SMOOTH_MOD_THICKNESS));
  cmp("Smooth.use_edit_uv", p[PG_P_SMOOTH_UV], FLAG(sm->flag, GP_SMOOTH_MOD_UV));
  cmp("Smooth.keep_shape", p[PG_P_SMOOTH_KEEP_SHAPE], FLAG(sm->flag, GP_SMOOTH_KEEP_SHAPE));

  const SimplifyGpencilModifierData *si = DNA_struct_default_get(SimplifyGpencilModifierData);
  mod(PG_MOD_SIMPLIFY, p);
  cmp("Simplify.mode", p[PG_P_SIMPLIFY_MODE], si->mode);
  cmp("Simplify.step", p[PG_P_SIMPLIFY_STEP], si->step);
  cmp("Simplify.factor", p[PG_P_SIMPLIFY_FACTOR], si->factor);
  cmp("Simplify.length", p[PG_P_SIMPLIFY_LENGTH], si->length);
  cmp("Simplify.sharp_threshold", p[PG_P_SIMPLIFY_SHARP_THRESHOLD], si->sharp_threshold);
  cmp("Simplify.distance", p[PG_P_SIMPLIFY_DISTANCE], si->distance);

  const SubdivGpencilModifierData *sd = DNA_struct_default_get(SubdivGpencilModifierData);
  mod(PG_MOD_SUBDIV, p);
  cmp("Subdiv.level", p[PG_P_SUBDIV_LEVEL], sd->level);
  cmp("Subdiv.type", p[PG_P_SUBDIV_TYPE], sd->type);

  const OffsetGpencilModifierData *of = DNA_struct_default_get(OffsetGpencilModifierData);
  mod(PG_MOD_OFFSET, p);
  cmp("Offset.mode", p[PG_P_OFFSET_MODE], of->mode);
  for (int i = 0; i < 3; i++) {
    cmp("Offset.loc", p[PG_P_OFFSET_LOC + i], of->loc[i]);
    cmp("Offset.rot", p[PG_P_OFFSET_ROT + i], of->rot[i]);
    cmp("Offset.scale", p[PG_P_OFFSET_SCALE + i], of->scale[i]);
    cmp("Offset.rnd_offset", p[PG_P_OFFSET_RND_OFFSET + i], of->rnd_offset[i]);
    cmp("Offset.rnd_rot", p[PG_P_OFFSET_RND_ROT + i], of->rnd_rot[i]);
    cmp("Offset.rnd_scale", p[PG_P_OFFSET_RND_SCALE + i], of->rnd_scale[i]);
  }
  cmp("Offset.seed", p[PG_P_OFFSET_SEED], of->seed);
  cmp("Offset.stroke_step", p[PG_P_OFFSET_STROKE_STEP], of->stroke_step);
  cmp("Offset.stroke_start_offset", p[PG_P_OFFSET_START_OFFSET], of->stroke_start_offset);
  cmp("Offset.use_uniform_random_scale", p[PG_P_OFFSET_UNIFORM_SCALE], FLAG(of->flag, GP_OFFSET_UNIFORM_RANDOM_SCALE));

  const NoiseGpencilModifierData *no = DNA_struct_default_get(NoiseGpencilModifierData);
  mod(PG_MOD_NOISE, p);
  cmp("Noise.factor", p[PG_P_NOISE_FACTOR], no->factor);
  cmp("Noise.factor_strength", p[PG_P_NOISE_STRENGTH], no->factor_strength);
  cmp("Noise.factor_thickness", p[PG_P_NOISE_THICKNESS], no->factor_thickness);
  cmp("Noise.factor_uvs", p[PG_P_NOISE_UVS], no->factor_uvs);
  cmp("Noise.noise_scale", p[PG_P_NOISE_SCALE], no->noise_scale);
  cmp("Noise.noise_offset", p[PG_P_NOISE_OFFSET], no->noise_offset);
  cmp("Noise.seed", p[PG_P_NOISE_SEED], no->seed);
  cmp("Noise.step", p[PG_P_NOISE_STEP], no->step);
  cmp("Noise.use_random", p[PG_P_NOISE_USE_RANDOM], FLAG(no->flag, GP_NOISE_USE_RANDOM));
  cmp("Noise.random_mode", p[PG_P_NOISE_MODE], no->noise_mode);

  const BuildGpencilModifierData *bu = DNA_struct_default_get(BuildGpencilModifierData);
  mod(PG_MOD_BUILD, p);
  cmp("Build.mode", p[PG_P_BUILD_MODE], bu->mode);
  cmp("Build.transition", p[PG_P_BUILD_TRANSITION], bu->transition);
  cmp("Build.start_delay", p[PG_P_BUILD_START], bu->start_delay);
  cmp("Build.length", p[PG_P_BUILD_LENGTH], bu->length);
  cmp("Build.concurrent_time_alignment", p[PG_P_BUILD_TIME_ALIGN], bu->time_alignment);
  cmp("Build.use_fading", p[PG_P_BUILD_USE_FADE], FLAG(bu->flag, GP_BUILD_USE_FADING));
  cmp("Build.fade_factor", p[PG_P_BUILD_FADE_FAC], bu->fade_fac);
  cmp("Build.fade_thickness_strength", p[PG_P_BUILD_FADE_THICKNESS], bu->fade_thickness_strength);
  cmp("Build.fade_opacity_strength", p[PG_P_BUILD_FADE_OPACITY], bu->fade_opacity_strength);
  skip("Build.start_frame/end_frame/speed_fac/speed_maxgap", "no PG parameter (PG has no frame-range or additive/speed build)");

  const TimeGpencilModifierData *tm = DNA_struct_default_get(TimeGpencilModifierData);
  mod(PG_MOD_TIME, p);
  cmp("Time.mode", p[PG_P_TIME_MODE], tm->mode);
  cmp("Time.offset", p[PG_P_TIME_OFFSET], tm->offset);
  cmp("Time.frame_scale", p[PG_P_TIME_SCALE], tm->frame_scale);
  cmp("Time.use_custom_frame_range", p[PG_P_TIME_USE_RANGE], FLAG(tm->flag, GP_TIME_CUSTOM_RANGE));
  cmp("Time.frame_start", p[PG_P_TIME_SFRA], tm->sfra);
  cmp("Time.frame_end", p[PG_P_TIME_EFRA], tm->efra);
  cmp("Time.use_keep_loop", p[PG_P_TIME_LOOP], FLAG(tm->flag, GP_TIME_KEEP_LOOP));

  const EnvelopeGpencilModifierData *en = DNA_struct_default_get(EnvelopeGpencilModifierData);
  mod(PG_MOD_ENVELOPE, p);
  cmp("Envelope.mode", p[PG_P_ENVELOPE_MODE], en->mode);
  cmp("Envelope.spread", p[PG_P_ENVELOPE_SPREAD], en->spread);
  cmp("Envelope.skip", p[PG_P_ENVELOPE_SKIP], en->skip);
  cmp("Envelope.thickness", p[PG_P_ENVELOPE_THICKNESS], en->thickness);
  cmp("Envelope.strength", p[PG_P_ENVELOPE_STRENGTH], en->strength);
  cmp("Envelope.mat_nr", p[PG_P_ENVELOPE_MAT], en->mat_nr);

  const WeightAngleGpencilModifierData *wa = DNA_struct_default_get(WeightAngleGpencilModifierData);
  mod(PG_MOD_WEIGHT_ANGLE, p);
  cmp("WeightAngle.angle", p[PG_P_WANGLE_ANGLE], wa->angle);
  cmp("WeightAngle.minimum_weight", p[PG_P_WANGLE_MIN_WEIGHT], wa->min_weight);
  cmp("WeightAngle.use_invert_output", p[PG_P_WANGLE_INVERT], FLAG(wa->flag, GP_WEIGHT_INVERT_OUTPUT));
  cmp("WeightAngle.use_multiply", p[PG_P_WANGLE_MULTIPLY], FLAG(wa->flag, GP_WEIGHT_MULTIPLY_DATA));
  skip("WeightAngle.axis/space", "PG angle is measured in the 2D canvas plane");

  const DashGpencilModifierSegment *ds = DNA_struct_default_get(DashGpencilModifierSegment);
  mod(PG_MOD_DASH, p);
  cmp("DashSegment.dash", p[PG_P_DASH_DASH], ds->dash);
  cmp("DashSegment.gap", p[PG_P_DASH_GAP], ds->gap);
  const DashGpencilModifierData *dd = DNA_struct_default_get(DashGpencilModifierData);
  cmp("Dash.dash_offset", p[PG_P_DASH_OFFSET], dd->dash_offset);
  cmp("DashSegment.radius", p[PG_P_DASH_RADIUS], ds->radius);
  cmp("DashSegment.opacity", p[PG_P_DASH_OPACITY], ds->opacity);
  cmp("DashSegment.mat_nr", p[PG_P_DASH_MAT], ds->mat_nr);
  cmp("DashSegment.use_cyclic", p[PG_P_DASH_CYCLIC], FLAG(ds->flag, GP_DASH_USE_CYCLIC));
  skip("Dash multiple segments", "PG has one segment");

  const OutlineGpencilModifierData *ou = DNA_struct_default_get(OutlineGpencilModifierData);
  mod(PG_MOD_OUTLINE, p);
  cmp("Outline.subdivision", p[PG_P_OUTLINE_SUBDIV], ou->subdiv);
  skip("Outline.thickness", "PG thickness is in canvas px (Blender: object space, needs a camera)");

  const MirrorGpencilModifierData *mi = DNA_struct_default_get(MirrorGpencilModifierData);
  mod(PG_MOD_MIRROR, p);
  cmp("Mirror.use_axis_x", p[PG_P_MIRROR_X], FLAG(mi->flag, GP_MIRROR_AXIS_X));
  cmp("Mirror.use_axis_y", p[PG_P_MIRROR_Y], FLAG(mi->flag, GP_MIRROR_AXIS_Y));
  skip("Mirror pivot", "PG pivot is a canvas point (Blender: mirror object or origin)");

  const ArrayGpencilModifierData *ar = DNA_struct_default_get(ArrayGpencilModifierData);
  mod(PG_MOD_ARRAY, p);
  cmp("Array.count", p[PG_P_ARRAY_COUNT_N], ar->count);
  cmp("Array.use_constant_offset", p[PG_P_ARRAY_USE_OFFSET], FLAG(ar->flag, GP_ARRAY_USE_OFFSET));
  cmp("Array.use_relative_offset", p[PG_P_ARRAY_USE_RELATIVE], FLAG(ar->flag, GP_ARRAY_USE_RELATIVE));
  cmp("Array.use_uniform_random_scale", p[PG_P_ARRAY_UNIFORM], FLAG(ar->flag, GP_ARRAY_UNIFORM_RANDOM_SCALE));
  cmp("Array.constant_offset[0] (px)", p[PG_P_ARRAY_OX], ar->offset[0] * PG_MOD_PIXELS_PER_UNIT);
  cmp("Array.constant_offset[1] (px)", p[PG_P_ARRAY_OY], -ar->offset[1] * PG_MOD_PIXELS_PER_UNIT);
  cmp("Array.relative_offset[0]", p[PG_P_ARRAY_SHIFT_X], ar->shift[0]);
  cmp("Array.relative_offset[1]", p[PG_P_ARRAY_SHIFT_Y], ar->shift[1]);
  cmp("Array.random_offset[0] (px)", p[PG_P_ARRAY_RND_OX], ar->rnd_offset[0] * PG_MOD_PIXELS_PER_UNIT);
  cmp("Array.random_offset[1] (px)", p[PG_P_ARRAY_RND_OY], -ar->rnd_offset[1] * PG_MOD_PIXELS_PER_UNIT);
  cmp("Array.random_rotation[2]", p[PG_P_ARRAY_RND_ROT], ar->rnd_rot[2]);
  cmp("Array.random_scale[0]", p[PG_P_ARRAY_RND_SX], ar->rnd_scale[0]);
  cmp("Array.random_scale[1]", p[PG_P_ARRAY_RND_SY], ar->rnd_scale[1]);
  cmp("Array.seed", p[PG_P_ARRAY_SEED], ar->seed);
  cmp("Array.replace_material", p[PG_P_ARRAY_MAT_RPL], ar->mat_rpl);
  skip("Array z components, offset object", "2D canvas: no z, no objects");

  const MultiplyGpencilModifierData *mu = DNA_struct_default_get(MultiplyGpencilModifierData);
  mod(PG_MOD_MULTIPLY, p);
  cmp("Multiply.duplicates", p[PG_P_MULTIPLY_DUPLICATIONS], mu->duplications);
  cmp("Multiply.distance (px)", p[PG_P_MULTIPLY_DISTANCE], mu->distance * PG_MOD_PIXELS_PER_UNIT);
  cmp("Multiply.offset", p[PG_P_MULTIPLY_OFFSET], mu->offset);
  cmp("Multiply.use_fade", p[PG_P_MULTIPLY_FADE], FLAG(mu->flags, GP_MULTIPLY_ENABLE_FADING));
  cmp("Multiply.fading_center", p[PG_P_MULTIPLY_FADE_CENTER], mu->fading_center);
  cmp("Multiply.fading_thickness", p[PG_P_MULTIPLY_FADE_THICKNESS], mu->fading_thickness);
  cmp("Multiply.fading_opacity", p[PG_P_MULTIPLY_FADE_OPACITY], mu->fading_opacity);

  const TextureGpencilModifierData *tx = DNA_struct_default_get(TextureGpencilModifierData);
  mod(PG_MOD_TEXTURE, p);
  cmp("Texture.mode", p[PG_P_TEXTURE_MODE], tx->mode);
  cmp("Texture.fit_method", p[PG_P_TEXTURE_FIT], tx->fit_method);
  cmp("Texture.uv_offset", p[PG_P_TEXTURE_UV_OFFSET], tx->uv_offset);
  cmp("Texture.uv_scale", p[PG_P_TEXTURE_UV_SCALE], tx->uv_scale);
  cmp("Texture.alignment_rotation", p[PG_P_TEXTURE_ALIGN_ROT], tx->alignment_rotation);
  cmp("Texture.fill_rotation", p[PG_P_TEXTURE_FILL_ROT], tx->fill_rotation);
  cmp("Texture.fill_offset[0]", p[PG_P_TEXTURE_FILL_OFFSET_X], tx->fill_offset[0]);
  cmp("Texture.fill_offset[1]", p[PG_P_TEXTURE_FILL_OFFSET_Y], tx->fill_offset[1]);
  cmp("Texture.fill_scale", p[PG_P_TEXTURE_FILL_SCALE], tx->fill_scale);

  printf("%d values compared with Blender's DNA defaults, %d groups not comparable (2D-adapted)\n",
         compared, not_compared);
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
