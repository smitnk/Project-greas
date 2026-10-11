/* L7 parity harness: every 2D Grease Pencil modifier Project Grease implements, run through
 * Blender 3.6.23's real MOD_gpencil_legacy_*.c (deformStroke via its GpencilModifierTypeInfo,
 * modifier data from its own initData = DNA defaults) and through Project Grease
 * (pg_mod_eval_frame), on the same strokes. Points are compared in PG canvas space: Blender runs in
 * object space (PG_MOD_PIXELS_PER_UNIT canvas pixels per unit, y up), as PG's copied cores do. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <vector>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "MEM_guardedalloc.h"
#include "MOD_gpencil_legacy_modifiertypes.h"
#include "project_grease_modifier_stack.h"

extern "C" float pg_l7_cfra;
extern "C" Scene *pg_l7_scene;

static int failures = 0, cases = 0, known_count = 0;
static char worst_what[160];

static void add_stroke(bGPDframe *gpf, int n, float x0, float y0, float wobble)
{
  bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, 0, n, 10, false);
  for (int i = 0; i < n; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->x = x0 + 14.0f * i;
    pt->y = y0 + wobble * std::sin(0.8f * i) + 2.0f * (i % 3);
    pt->pressure = 0.3f + 0.06f * i;
    pt->strength = 0.9f - 0.03f * i;
    pt->time = 0.02f * i;
    pt->vert_color[0] = 0.1f;
    pt->vert_color[1] = 0.5f;
    pt->vert_color[2] = 0.8f;
    pt->vert_color[3] = 0.4f;
  }
  gps->vert_color_fill[0] = 0.3f;
  gps->vert_color_fill[3] = 0.6f;
}

static void space(bGPDstroke *gps, bool to_object)
{
  const float s = to_object ? 1.0f / PG_MOD_PIXELS_PER_UNIT : PG_MOD_PIXELS_PER_UNIT;
  for (int i = 0; i < gps->totpoints; i++) {
    gps->points[i].x *= s;
    gps->points[i].y = -gps->points[i].y * s;
    gps->points[i].z *= s;
  }
}

struct Case {
  const char *name;
  int pg_type;
  const GpencilModifierTypeInfo *mti;
  std::function<void(float *p)> pg_set;          /* PG parameter variant */
  std::function<void(GpencilModifierData *)> bl_set; /* the same variant on Blender's data */
  const char *uv_not_compared = nullptr; /* reason, when uv_fac depends on PG's canvas unit */
  /* Known difference still to fix (listed, not hidden): reported in the log, not counted as a
   * failure; if the case starts matching, that IS a failure, so this list cannot go stale. */
  const char *known = nullptr;
};

static bool skip_uv = false;
static double compare(const bGPDframe *a, const bGPDframe *b, const char *name)
{
  double worst = 0.0;
  const bGPDstroke *s = (const bGPDstroke *)a->strokes.first, *t = (const bGPDstroke *)b->strokes.first;
  int k = 0;
  for (; s && t; s = s->next, t = t->next, k++) {
    if (s->totpoints != t->totpoints) {
      std::printf("  diff %s: stroke %d has %d points in PG, %d in Blender. Order PG:", name, k, s->totpoints, t->totpoints);
      LISTBASE_FOREACH (const bGPDstroke *, x, &a->strokes) std::printf(" %d", x->totpoints);
      std::printf(" | Blender:");
      LISTBASE_FOREACH (const bGPDstroke *, x, &b->strokes) std::printf(" %d", x->totpoints);
      std::printf("\n");
      return INFINITY;
    }
    worst = fmax(worst, fabs(s->thickness - t->thickness));
    for (int c = 0; c < 4; c++) {
      worst = fmax(worst, fabs(s->vert_color_fill[c] - t->vert_color_fill[c]));
    }
    for (int i = 0; i < s->totpoints; i++) {
      const bGPDspoint &p = s->points[i], &q = t->points[i];
      const double d[] = {p.x - q.x, p.y - q.y, p.z - q.z, p.pressure - q.pressure, p.strength - q.strength,
                          p.uv_fac - q.uv_fac, p.uv_rot - q.uv_rot, p.vert_color[0] - q.vert_color[0],
                          p.vert_color[1] - q.vert_color[1], p.vert_color[2] - q.vert_color[2],
                          p.vert_color[3] - q.vert_color[3]};
      static const char *fields[] = {"x", "y", "z", "pressure", "strength", "uv_fac", "uv_rot", "r", "g", "b", "a"};
      for (int f = 0; f < 11; f++) {
        if (skip_uv && f == 5) continue;
        if (fabs(d[f]) > worst) {
          worst = fabs(d[f]);
          std::snprintf(worst_what, sizeof(worst_what), "stroke %d point %d %s: PG %.6g Blender %.6g", k, i, fields[f],
                        (&p.x)[0] * 0 + (f == 0 ? p.x : f == 1 ? p.y : f == 2 ? p.z : f == 3 ? p.pressure : f == 4 ? p.strength : f == 5 ? p.uv_fac : f == 6 ? p.uv_rot : p.vert_color[f - 7]),
                        (f == 0 ? q.x : f == 1 ? q.y : f == 2 ? q.z : f == 3 ? q.pressure : f == 4 ? q.strength : f == 5 ? q.uv_fac : f == 6 ? q.uv_rot : q.vert_color[f - 7]));
        }
      }
    }
  }
  if (s || t) {
    std::printf("  diff %s: stroke count differs\n", name);
    return INFINITY;
  }
  return worst;
}

int main()
{
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "Layer", true, false);
  gpd->pixfactor = GP_DEFAULT_PIX_FACTOR; /* as BKE_gpencil_data_addnew and PG's document */
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, 1);
  add_stroke(gpf, 12, 10.0f, 50.0f, 18.0f);
  add_stroke(gpf, 7, 40.0f, 140.0f, 6.0f);
  add_stroke(gpf, 3, 5.0f, 200.0f, 2.0f);
  /* One material showing stroke and fill, kept in bGPdata::mat[] as Project Grease does. */
  Material *ma = static_cast<Material *>(MEM_callocN(sizeof(Material), "ma"));
  ma->gp_style = static_cast<MaterialGPencilStyle *>(MEM_callocN(sizeof(MaterialGPencilStyle), "gp_style"));
  ma->gp_style->flag = GP_MATERIAL_STROKE_SHOW | GP_MATERIAL_FILL_SHOW;
  copy_v4_fl4(ma->gp_style->stroke_rgba, 0.2f, 0.3f, 0.4f, 1.0f);
  copy_v4_fl4(ma->gp_style->fill_rgba, 0.6f, 0.5f, 0.4f, 1.0f);
  gpd->mat = static_cast<Material **>(MEM_callocN(sizeof(Material *), "mat"));
  gpd->mat[0] = ma;
  gpd->totcol = 1;
  /* Real strokes carry geometry data (uv_fac, triangles) from when they were drawn. */
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) BKE_gpencil_stroke_geometry_update(gpd, gps);
  pg_l7_scene = static_cast<Scene *>(MEM_callocN(sizeof(Scene), "scene"));
  pg_l7_scene->r.sfra = 1;
  pg_l7_scene->r.efra = 250;
  Object *ob = static_cast<Object *>(MEM_callocN(sizeof(Object), "ob"));
  ob->type = OB_GPENCIL_LEGACY;
  std::snprintf(ob->id.name, sizeof(ob->id.name), "OB%s", PG_MOD_OBJECT_NAME); /* PG's seed name */
  ob->data = gpd;
  unit_m4(ob->object_to_world);
  unit_m4(ob->world_to_object);

  const auto none = [](float *) {};
  const auto bnone = [](GpencilModifierData *) {};
  std::vector<Case> all = {
      {"Thickness default", PG_MOD_THICKNESS, &modifierType_Gpencil_Thick, none, bnone},
      {"Thickness normalize 12", PG_MOD_THICKNESS, &modifierType_Gpencil_Thick,
       [](float *p) { p[PG_P_THICK_NORMALIZE] = 1; p[PG_P_THICK_THICKNESS] = 12; },
       [](GpencilModifierData *m) { auto *d = (ThickGpencilModifierData *)m; d->flag |= GP_THICK_NORMALIZE; d->thickness = 12; }},
      {"Opacity default", PG_MOD_OPACITY, &modifierType_Gpencil_Opacity, none, bnone},
      {"Opacity 0.4 stroke", PG_MOD_OPACITY, &modifierType_Gpencil_Opacity,
       [](float *p) { p[PG_P_OPACITY_FACTOR] = 0.4f; p[PG_P_OPACITY_MODIFY] = GP_MODIFY_COLOR_STROKE; },
       [](GpencilModifierData *m) { auto *d = (OpacityGpencilModifierData *)m; d->factor = 0.4f; d->modify_color = GP_MODIFY_COLOR_STROKE; }},
      {"Tint default", PG_MOD_TINT, &modifierType_Gpencil_Tint, none, bnone},
      {"Hue/Saturation default", PG_MOD_COLOR, &modifierType_Gpencil_Color, none, bnone},
      {"Hue/Saturation 0.2 1.5 0.7", PG_MOD_COLOR, &modifierType_Gpencil_Color,
       [](float *p) { p[PG_P_COLOR_H] = 0.2f; p[PG_P_COLOR_S] = 1.5f; p[PG_P_COLOR_V] = 0.7f; },
       [](GpencilModifierData *m) { auto *d = (ColorGpencilModifierData *)m; d->hsv[0] = 0.2f; d->hsv[1] = 1.5f; d->hsv[2] = 0.7f; }},
      {"Smooth default", PG_MOD_SMOOTH, &modifierType_Gpencil_Smooth, none, bnone},
      {"Smooth 0.6 x3 all", PG_MOD_SMOOTH, &modifierType_Gpencil_Smooth,
       [](float *p) { p[PG_P_SMOOTH_FACTOR] = 0.6f; p[PG_P_SMOOTH_STEP] = 3; p[PG_P_SMOOTH_STRENGTH] = 1; p[PG_P_SMOOTH_THICKNESS] = 1; },
       [](GpencilModifierData *m) { auto *d = (SmoothGpencilModifierData *)m; d->factor = 0.6f; d->step = 3; d->flag |= GP_SMOOTH_MOD_STRENGTH | GP_SMOOTH_MOD_THICKNESS; }},
      {"Simplify default", PG_MOD_SIMPLIFY, &modifierType_Gpencil_Simplify, none, bnone},
      {"Simplify adaptive 0.3", PG_MOD_SIMPLIFY, &modifierType_Gpencil_Simplify,
       [](float *p) { p[PG_P_SIMPLIFY_MODE] = GP_SIMPLIFY_ADAPTIVE; p[PG_P_SIMPLIFY_FACTOR] = 0.3f; },
       [](GpencilModifierData *m) { auto *d = (SimplifyGpencilModifierData *)m; d->mode = GP_SIMPLIFY_ADAPTIVE; d->factor = 0.3f; }},
      {"Subdivide default", PG_MOD_SUBDIV, &modifierType_Gpencil_Subdiv, none, bnone},
      {"Subdivide simple 2", PG_MOD_SUBDIV, &modifierType_Gpencil_Subdiv,
       [](float *p) { p[PG_P_SUBDIV_TYPE] = GP_SUBDIV_SIMPLE; p[PG_P_SUBDIV_LEVEL] = 2; },
       [](GpencilModifierData *m) { auto *d = (SubdivGpencilModifierData *)m; d->type = GP_SUBDIV_SIMPLE; d->level = 2; }},
      {"Offset default", PG_MOD_OFFSET, &modifierType_Gpencil_Offset, none, bnone},
      {"Offset loc/rot/scale", PG_MOD_OFFSET, &modifierType_Gpencil_Offset,
       /* PG's Offset location is in pixels, applied after the y-up flip: 0.3 / -0.2 units. */
       [](float *p) { p[PG_P_OFFSET_LOC] = 0.3f * PG_MOD_PIXELS_PER_UNIT; p[PG_P_OFFSET_LOC + 1] = -0.2f * PG_MOD_PIXELS_PER_UNIT; p[PG_P_OFFSET_ROT + 2] = 0.4f; p[PG_P_OFFSET_SCALE] = 0.5f; },
       [](GpencilModifierData *m) { auto *d = (OffsetGpencilModifierData *)m; d->loc[0] = 0.3f; d->loc[1] = -0.2f; d->rot[2] = 0.4f; d->scale[0] = 0.5f; }},
      {"Noise default", PG_MOD_NOISE, &modifierType_Gpencil_Noise, none, bnone},
      {"Noise all channels", PG_MOD_NOISE, &modifierType_Gpencil_Noise,
       [](float *p) { p[PG_P_NOISE_STRENGTH] = 0.4f; p[PG_P_NOISE_THICKNESS] = 0.3f; p[PG_P_NOISE_UVS] = 0.2f; p[PG_P_NOISE_SEED] = 7; },
       [](GpencilModifierData *m) { auto *d = (NoiseGpencilModifierData *)m; d->factor_strength = 0.4f; d->factor_thickness = 0.3f; d->factor_uvs = 0.2f; d->seed = 7; }},
      {"Offset random", PG_MOD_OFFSET, &modifierType_Gpencil_Offset,
       [](float *p) { p[PG_P_OFFSET_RND_ROT + 2] = 0.5f; p[PG_P_OFFSET_RND_SCALE] = 0.3f; p[PG_P_OFFSET_SEED] = 3; },
       [](GpencilModifierData *m) { auto *d = (OffsetGpencilModifierData *)m; d->rnd_rot[2] = 0.5f; d->rnd_scale[0] = 0.3f; d->seed = 3; }},
      {"Texture default (constant length)", PG_MOD_TEXTURE, &modifierType_Gpencil_Texture, none, bnone,
       "constant-length uv_fac is a length: canvas pixels in PG, object units in Blender (L8 draw sets the unit)"},
      {"Texture fit stroke, scale 2, offset 0.3", PG_MOD_TEXTURE, &modifierType_Gpencil_Texture,
       [](float *p) { p[PG_P_TEXTURE_FIT] = GP_TEX_FIT_STROKE; p[PG_P_TEXTURE_UV_SCALE] = 2; p[PG_P_TEXTURE_UV_OFFSET] = 0.3f; p[PG_P_TEXTURE_ALIGN_ROT] = 0.2f; },
       [](GpencilModifierData *m) { auto *d = (TextureGpencilModifierData *)m; d->fit_method = GP_TEX_FIT_STROKE; d->uv_scale = 2; d->uv_offset = 0.3f; d->alignment_rotation = 0.2f; }},
      {"Texture fill", PG_MOD_TEXTURE, &modifierType_Gpencil_Texture,
       [](float *p) { p[PG_P_TEXTURE_MODE] = 2; p[PG_P_TEXTURE_FILL_ROT] = 0.4f; p[PG_P_TEXTURE_FILL_SCALE] = 1.5f; p[PG_P_TEXTURE_FILL_OFFSET_X] = 0.1f; },
       [](GpencilModifierData *m) { auto *d = (TextureGpencilModifierData *)m; d->mode = 2; d->fill_rotation = 0.4f; d->fill_scale = 1.5f; d->fill_offset[0] = 0.1f; },
       "stroke+fill mode keeps the constant-length uv_fac (a length; L8 draw sets the unit)"},
      {"Envelope deform", PG_MOD_ENVELOPE, &modifierType_Gpencil_Envelope,
       [](float *p) { p[PG_P_ENVELOPE_MODE] = GP_ENVELOPE_DEFORM; p[PG_P_ENVELOPE_SPREAD] = 4; },
       [](GpencilModifierData *m) { auto *d = (EnvelopeGpencilModifierData *)m; d->mode = GP_ENVELOPE_DEFORM; d->spread = 4; }},
      {"Length default", PG_MOD_LENGTH, &modifierType_Gpencil_Length, none, bnone},
      {"Mirror X about origin", PG_MOD_MIRROR, &modifierType_Gpencil_Mirror,
       [](float *p) { p[PG_P_MIRROR_PX] = 0; p[PG_P_MIRROR_PY] = 0; }, bnone},
      {"Mirror X+Y about a pivot object", PG_MOD_MIRROR, &modifierType_Gpencil_Mirror,
       [](float *p) { p[PG_P_MIRROR_Y] = 1; p[PG_P_MIRROR_PX] = 1.5f * PG_MOD_PIXELS_PER_UNIT; p[PG_P_MIRROR_PY] = 0.8f * PG_MOD_PIXELS_PER_UNIT; },
       [](GpencilModifierData *m) { auto *d = (MirrorGpencilModifierData *)m; d->flag |= GP_MIRROR_AXIS_Y;
         static Object piv; std::memset(&piv, 0, sizeof(piv)); unit_m4(piv.object_to_world);
         piv.object_to_world[3][0] = 1.5f; piv.object_to_world[3][1] = -0.8f; d->object = &piv; }},
      {"Array default (relative)", PG_MOD_ARRAY, &modifierType_Gpencil_Array, none, bnone},
      {"Array 3, constant offset 1 unit", PG_MOD_ARRAY, &modifierType_Gpencil_Array,
       [](float *p) { p[PG_P_ARRAY_COUNT_N] = 3; p[PG_P_ARRAY_USE_RELATIVE] = 0; p[PG_P_ARRAY_USE_OFFSET] = 1; p[PG_P_ARRAY_OX] = PG_MOD_PIXELS_PER_UNIT; },
       [](GpencilModifierData *m) { auto *d = (ArrayGpencilModifierData *)m; d->count = 3; d->flag = (d->flag & ~GP_ARRAY_USE_RELATIVE) | GP_ARRAY_USE_OFFSET; d->offset[0] = 1; }},
      {"Array 4, random offset/rotation/scale", PG_MOD_ARRAY, &modifierType_Gpencil_Array,
       [](float *p) { p[PG_P_ARRAY_COUNT_N] = 4; p[PG_P_ARRAY_RND_OX] = 0.3f * PG_MOD_PIXELS_PER_UNIT; p[PG_P_ARRAY_RND_OY] = -0.2f * PG_MOD_PIXELS_PER_UNIT; p[PG_P_ARRAY_RND_ROT] = 0.5f;
                      p[PG_P_ARRAY_RND_SX] = 0.3f; p[PG_P_ARRAY_RND_SY] = 0.2f; p[PG_P_ARRAY_SEED] = 7; },
       [](GpencilModifierData *m) { auto *d = (ArrayGpencilModifierData *)m; d->count = 4; d->rnd_offset[0] = 0.3f; d->rnd_offset[1] = 0.2f;
         d->rnd_rot[2] = 0.5f; d->rnd_scale[0] = 0.3f; d->rnd_scale[1] = 0.2f; d->seed = 7; }},
      {"Multiply default", PG_MOD_MULTIPLY, &modifierType_Gpencil_Multiply, none, bnone},
      {"Multiply 2, offset, fading", PG_MOD_MULTIPLY, &modifierType_Gpencil_Multiply,
       [](float *p) { p[PG_P_MULTIPLY_DUPLICATIONS] = 2; p[PG_P_MULTIPLY_OFFSET] = 0.3f; p[PG_P_MULTIPLY_FADE] = 1; p[PG_P_MULTIPLY_FADE_CENTER] = 0.2f; },
       [](GpencilModifierData *m) { auto *d = (MultiplyGpencilModifierData *)m; d->duplications = 2; d->offset = 0.3f; d->flags |= GP_MULTIPLY_ENABLE_FADING; d->fading_center = 0.2f; }},
      {"Dash default", PG_MOD_DASH, &modifierType_Gpencil_Dash, none, bnone},
      {"Dash 3/2, offset -1, radius, opacity, cyclic", PG_MOD_DASH, &modifierType_Gpencil_Dash,
       [](float *p) { p[PG_P_DASH_DASH] = 3; p[PG_P_DASH_GAP] = 2; p[PG_P_DASH_OFFSET] = -1; p[PG_P_DASH_RADIUS] = 0.5f; p[PG_P_DASH_OPACITY] = 0.7f; p[PG_P_DASH_CYCLIC] = 1; },
       [](GpencilModifierData *m) { auto *d = (DashGpencilModifierData *)m; d->dash_offset = -1; d->segments[0].dash = 3; d->segments[0].gap = 2;
         d->segments[0].radius = 0.5f; d->segments[0].opacity = 0.7f; d->segments[0].flag |= GP_DASH_USE_CYCLIC; }},
      {"Envelope segments default", PG_MOD_ENVELOPE, &modifierType_Gpencil_Envelope, none, bnone},
      {"Build default", PG_MOD_BUILD, &modifierType_Gpencil_Build, none, bnone},
      {"Length random", PG_MOD_LENGTH, &modifierType_Gpencil_Length,
       [](float *p) { p[PG_P_LENGTH_USE_RANDOM] = 1; p[PG_P_LENGTH_RAND_START] = 0.3f; p[PG_P_LENGTH_RAND_END] = 0.2f; p[PG_P_LENGTH_SEED] = 5; },
       [](GpencilModifierData *m) { auto *d = (LengthGpencilModifierData *)m; d->flag |= GP_LENGTH_USE_RANDOM; d->rand_start_fac = 0.3f; d->rand_end_fac = 0.2f; d->seed = 5; }},
  };

  for (const Case &c : all) {
    for (float cfra : {1.0f, 7.0f}) {
      pg_l7_cfra = cfra;
      skip_uv = c.uv_not_compared != nullptr;
      if (skip_uv && cfra == 1.0f) std::printf("  NOT COMPARED %s uv_fac: %s\n", c.name, c.uv_not_compared);
      /* Project Grease */
      PGModEntry e;
      std::memset(&e, 0, sizeof(e));
      pg_mod_entry_init(&e, c.pg_type);
      c.pg_set(e.params);
      pg_mod_sanitize(c.pg_type, e.params);
      bGPDframe pg;
      pg_mod_eval_frame(gpd, gpl, gpf, &e, 1, (int)cfra, &pg);
      /* Blender */
      GpencilModifierData *md = static_cast<GpencilModifierData *>(MEM_callocN(c.mti->struct_size, "md"));
      c.mti->initData(md);
      std::snprintf(md->name, sizeof(md->name), "%s", pg_mod_name(c.pg_type)); /* PG's seed name */
      c.bl_set(md);
      /* As BKE_gpencil_modifiers_calc: on the evaluated copy, generateStrokes (once per object, at
       * the scene frame) then deformStroke on every stroke of the frame. */
      pg_l7_scene->r.cfra = (int)cfra;
      bGPdata *ev = static_cast<bGPdata *>(MEM_dupallocN(gpd));
      BLI_listbase_clear(&ev->layers);
      bGPDlayer *el = BKE_gpencil_layer_duplicate(gpl, true, true);
      BLI_addtail(&ev->layers, el);
      bGPDframe *bl = static_cast<bGPDframe *>(el->frames.first);
      el->actframe = bl;
      LISTBASE_FOREACH (bGPDstroke *, gps, &bl->strokes) {
        space(gps, true);
        BKE_gpencil_stroke_geometry_update(ev, gps); /* Blender's data: uv_fac in the same units */
      }
      ob->data = ev;
      if (c.mti->generateStrokes) c.mti->generateStrokes(md, nullptr, ob);
      if (c.mti->deformStroke) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &bl->strokes) c.mti->deformStroke(md, nullptr, ob, el, bl, gps);
      }
      ob->data = gpd;
      LISTBASE_FOREACH (bGPDstroke *, gps, &bl->strokes) {
        space(gps, false);
        /* PG's data is in canvas pixels: like pg_mod_eval_frame's final pass, redo the geometry data
         * in pixel space, keeping uv_fac / uv_rot after Texture Mapping. */
        std::vector<float> uv;
        if (c.pg_type == PG_MOD_TEXTURE) {
          for (int i = 0; i < gps->totpoints; i++) { uv.push_back(gps->points[i].uv_fac); uv.push_back(gps->points[i].uv_rot); }
        }
        BKE_gpencil_stroke_geometry_update(gpd, gps);
        for (size_t i = 0; i < uv.size() / 2; i++) { gps->points[i].uv_fac = uv[2 * i]; gps->points[i].uv_rot = uv[2 * i + 1]; }
      }
      char name[96];
      std::snprintf(name, sizeof(name), "%s @frame %g", c.name, cfra);
      const double d = compare(&pg, bl, name);
      cases++;
      if (c.known) {
        if (d <= 1e-4) {
          std::printf("FAIL %s: listed as a known difference but now matches Blender: remove it from the list\n", name);
          failures++;
        }
        else {
          std::printf("  KNOWN DIFFERENCE %s: %s\n", name, c.known);
          known_count++;
        }
      }
      else if (!(d <= 1e-4)) {
        if (std::isfinite(d)) std::printf("FAIL %s: max difference %.6g at %s\n", name, d, worst_what);
        else std::printf("FAIL %s: structure differs (see diff above)\n", name);
        failures++;
      }
      else {
        std::printf("  ok %s (max diff %.2g)\n", name, d);
      }
      pg_mod_eval_free(&pg);
      BKE_gpencil_free_layers(&ev->layers);
      MEM_freeN(ev);
      if (c.mti->freeData) c.mti->freeData(md);
      MEM_freeN(md);
    }
  }
  std::printf("%d modifier cases compared with Blender 3.6.23 MOD_gpencil_legacy_*.c, %d known differences listed\n", cases, known_count);
  std::printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
