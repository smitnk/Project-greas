/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * L7: the live generators Mirror, Array, Multiple Strokes and Dot Dash, as Blender 3.6.23's
 * MOD_gpencil_legacy_{mirror,array,multiply,dash}.c. Their geometry functions are VERBATIM regions;
 * the glue is ADAPTED:
 *   - the modifier data is built from PG's flat parameters (canvas px -> object units);
 *   - Object is a stack Object with only what the regions read (identity object_to_world, id.name
 *     PG_MOD_OBJECT_NAME, data = a one-layer view of the document, totcol);
 *   - is_stroke_affected_by_modifier is PG's influence filter (pg_mod_stroke_affected) and
 *     BKE_gpencil_frame_retime_get is the evaluated frame;
 *   - the regions run in object space (y up, PG_MOD_PIXELS_PER_UNIT px per unit).
 */
#include <float.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_modifier_stack.h"
#include "project_grease_modifier_gen.h"

/* ADAPTED: the filter and the frame of the entry being evaluated (set by pg_mod_gen_run). */
static _Thread_local const PGModContext *pg_gen_ctx;
static _Thread_local const PGModEntry *pg_gen_entry;
#define is_stroke_affected_by_modifier(ob, ln, mat, pi, lp, minp, gpl, gps, a, b, c, d) \
  pg_mod_stroke_affected(pg_gen_ctx, pg_gen_entry, (gps))
#define BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl) (pg_gen_ctx->gpf)
typedef struct Depsgraph Depsgraph;
typedef struct Scene Scene;

#define generate_geometry mirror_generate_geometry /* ADAPTED: one file holds four modifiers */
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_mirror.c */
/* Mirror is using current object as origin. */
static void update_mirror_local(bGPDstroke *gps, int axis)
{
  int i;
  bGPDspoint *pt;
  float factor[3] = {1.0f, 1.0f, 1.0f};
  factor[axis] = -1.0f;

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    mul_v3_v3(&pt->x, factor);
  }
}

/* Mirror is using other object as origin. */
static void update_mirror_object(Object *ob,
                                 MirrorGpencilModifierData *mmd,
                                 bGPDstroke *gps,
                                 int axis)
{
  float mtx[4][4];
  unit_m4(mtx);
  mtx[axis][axis] = -1.0f;

  float tmp[4][4];
  float itmp[4][4];
  invert_m4_m4(tmp, mmd->object->object_to_world);
  mul_m4_m4m4(tmp, tmp, ob->object_to_world);
  invert_m4_m4(itmp, tmp);
  mul_m4_series(mtx, itmp, mtx, tmp);

  for (int i = 0; i < gps->totpoints; i++) {
    mul_m4_v3(mtx, &gps->points[i].x);
  }
}

static void update_position(Object *ob, MirrorGpencilModifierData *mmd, bGPDstroke *gps, int axis)
{
  if (mmd->object == NULL) {
    update_mirror_local(gps, axis);
  }
  else {
    update_mirror_object(ob, mmd, gps, axis);
  }
}

static void generate_geometry(
    GpencilModifierData *md, Object *ob, bGPDlayer *gpl, bGPDframe *gpf, const bool update)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;
  bGPdata *gpd = ob->data;
  bGPDstroke *gps, *gps_new = NULL;
  int tot_strokes;
  int i;

  /* check each axis for mirroring */
  for (int xi = 0; xi < 3; xi++) {
    if (mmd->flag & (GP_MIRROR_AXIS_X << xi)) {

      /* count strokes to avoid infinite loop after adding new strokes to tail of listbase */
      tot_strokes = BLI_listbase_count(&gpf->strokes);

      for (i = 0, gps = gpf->strokes.first; i < tot_strokes; i++, gps = gps->next) {
        if (is_stroke_affected_by_modifier(ob,
                                           mmd->layername,
                                           mmd->material,
                                           mmd->pass_index,
                                           mmd->layer_pass,
                                           1,
                                           gpl,
                                           gps,
                                           mmd->flag & GP_MIRROR_INVERT_LAYER,
                                           mmd->flag & GP_MIRROR_INVERT_PASS,
                                           mmd->flag & GP_MIRROR_INVERT_LAYERPASS,
                                           mmd->flag & GP_MIRROR_INVERT_MATERIAL))
        {
          gps_new = BKE_gpencil_stroke_duplicate(gps, true, true);
          update_position(ob, mmd, gps_new, xi);
          if (update) {
            BKE_gpencil_stroke_geometry_update(gpd, gps_new);
          }
          BLI_addtail(&gpf->strokes, gps_new);
        }
      }
    }
  }
}
/* END VERBATIM */
#undef generate_geometry

#define generate_geometry array_generate_geometry /* ADAPTED: one file holds four modifiers */
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_array.c */
typedef struct tmpStrokes {
  struct tmpStrokes *next, *prev;
  bGPDframe *gpf;
  bGPDstroke *gps;
} tmpStrokes;
/* END VERBATIM */
#undef generate_geometry

#define generate_geometry array_generate_geometry /* ADAPTED: one file holds four modifiers */
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_array.c */
/* -------------------------------- */
/* helper function for per-instance positioning */
static void BKE_gpencil_instance_modifier_instance_tfm(Object *ob,
                                                       ArrayGpencilModifierData *mmd,
                                                       const int elem_idx,
                                                       float r_mat[4][4],
                                                       float r_offset[4][4])
{
  float offset[3], rot[3], scale[3];
  ARRAY_SET_ITEMS(scale, 1.0f, 1.0f, 1.0f);
  zero_v3(rot);

  if (mmd->flag & GP_ARRAY_USE_OFFSET) {
    offset[0] = mmd->offset[0] * elem_idx;
    offset[1] = mmd->offset[1] * elem_idx;
    offset[2] = mmd->offset[2] * elem_idx;
  }
  else {
    zero_v3(offset);
  }

  /* Calculate matrix */
  loc_eul_size_to_mat4(r_mat, offset, rot, scale);
  copy_m4_m4(r_offset, r_mat);

  /* offset object */
  if ((mmd->flag & GP_ARRAY_USE_OB_OFFSET) && (mmd->object)) {
    float mat_offset[4][4];
    float obinv[4][4];

    unit_m4(mat_offset);
    if (mmd->flag & GP_ARRAY_USE_OFFSET) {
      add_v3_v3(mat_offset[3], mmd->offset);
    }
    invert_m4_m4(obinv, ob->object_to_world);

    mul_m4_series(r_offset, mat_offset, obinv, mmd->object->object_to_world);
    copy_m4_m4(mat_offset, r_offset);

    /* clear r_mat locations to avoid double transform */
    zero_v3(r_mat[3]);
  }
}
static bool gpencil_data_selected_minmax(ArrayGpencilModifierData *mmd,
                                         Object *ob,
                                         float r_min[3],
                                         float r_max[3])
{
  bGPdata *gpd = (bGPdata *)ob->data;
  bool changed = false;

  INIT_MINMAX(r_min, r_max);

  if (gpd == NULL) {
    return changed;
  }

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = gpl->actframe;

    if (gpf != NULL) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (is_stroke_affected_by_modifier(ob,
                                           mmd->layername,
                                           mmd->material,
                                           mmd->pass_index,
                                           mmd->layer_pass,
                                           1,
                                           gpl,
                                           gps,
                                           mmd->flag & GP_ARRAY_INVERT_LAYER,
                                           mmd->flag & GP_ARRAY_INVERT_PASS,
                                           mmd->flag & GP_ARRAY_INVERT_LAYERPASS,
                                           mmd->flag & GP_ARRAY_INVERT_MATERIAL))
        {
          changed |= BKE_gpencil_stroke_minmax(gps, false, r_min, r_max);
        }
      }
    }
  }

  return changed;
}
/* array modifier - generate geometry callback (for viewport/rendering) */
static void generate_geometry(GpencilModifierData *md,
                              Depsgraph *depsgraph,
                              Scene *scene,
                              Object *ob)
{
  ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;
  ListBase stroke_cache = {NULL, NULL};
  /* Load the strokes to be duplicated. */
  bGPdata *gpd = (bGPdata *)ob->data;
  bool found = false;

  /* Get bound-box for relative offset. */
  float size[3] = {0.0f, 0.0f, 0.0f};
  if (mmd->flag & GP_ARRAY_USE_RELATIVE) {
    float min[3];
    float max[3];
    if (gpencil_data_selected_minmax(mmd, ob, min, max)) {
      sub_v3_v3v3(size, max, min);
      /* Need a minimum size (for flat drawings). */
      CLAMP3_MIN(size, 0.01f);
    }
  }

  int seed = mmd->seed;
  /* Make sure different modifiers get different seeds. */
  seed += BLI_hash_string(ob->id.name + 2);
  seed += BLI_hash_string(md->name);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
    if (gpf == NULL) {
      continue;
    }
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (is_stroke_affected_by_modifier(ob,
                                         mmd->layername,
                                         mmd->material,
                                         mmd->pass_index,
                                         mmd->layer_pass,
                                         1,
                                         gpl,
                                         gps,
                                         mmd->flag & GP_ARRAY_INVERT_LAYER,
                                         mmd->flag & GP_ARRAY_INVERT_PASS,
                                         mmd->flag & GP_ARRAY_INVERT_LAYERPASS,
                                         mmd->flag & GP_ARRAY_INVERT_MATERIAL))
      {
        tmpStrokes *tmp = MEM_callocN(sizeof(tmpStrokes), __func__);
        tmp->gpf = gpf;
        tmp->gps = gps;
        BLI_addtail(&stroke_cache, tmp);

        found = true;
      }
    }
  }

  if (found) {
    /* Generate new instances of all existing strokes,
     * keeping each instance together so they maintain
     * the correct ordering relative to each other
     */
    float current_offset[4][4];
    unit_m4(current_offset);

    float rand_offset = BLI_hash_int_01(seed);

    for (int x = 0; x < mmd->count; x++) {
      /* original strokes are at index = 0 */
      if (x == 0) {
        continue;
      }

      /* Compute transforms for this instance */
      float mat[4][4];
      float mat_offset[4][4];
      BKE_gpencil_instance_modifier_instance_tfm(ob, mmd, x, mat, mat_offset);

      if ((mmd->flag & GP_ARRAY_USE_OB_OFFSET) && (mmd->object)) {
        /* recalculate cumulative offset here */
        mul_m4_m4m4(current_offset, current_offset, mat_offset);
      }
      else {
        copy_m4_m4(current_offset, mat);
      }

      /* Apply relative offset. */
      if (mmd->flag & GP_ARRAY_USE_RELATIVE) {
        float relative[3];
        mul_v3_v3v3(relative, mmd->shift, size);
        madd_v3_v3fl(current_offset[3], relative, x);
      }

      float rand[3][3];
      for (int j = 0; j < 3; j++) {
        const uint primes[3] = {2, 3, 7};
        double offset[3] = {0.0, 0.0, 0.0};
        double r[3];
        /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
        BLI_halton_3d(primes, offset, x, r);

        if ((mmd->flag & GP_ARRAY_UNIFORM_RANDOM_SCALE) && j == 2) {
          float rand_value;
          rand_value = fmodf(r[0] * 2.0 - 1.0 + rand_offset, 1.0f);
          rand_value = fmodf(sin(rand_value * 12.9898 + j * 78.233) * 43758.5453, 1.0f);
          copy_v3_fl(rand[j], rand_value);
        }
        else {
          for (int i = 0; i < 3; i++) {
            rand[j][i] = fmodf(r[i] * 2.0 - 1.0 + rand_offset, 1.0f);
            rand[j][i] = fmodf(sin(rand[j][i] * 12.9898 + j * 78.233) * 43758.5453, 1.0f);
          }
        }
      }
      /* Calculate Random matrix. */
      float mat_rnd[4][4];
      float loc[3], rot[3];
      float scale[3] = {1.0f, 1.0f, 1.0f};
      mul_v3_v3v3(loc, mmd->rnd_offset, rand[0]);
      mul_v3_v3v3(rot, mmd->rnd_rot, rand[1]);
      madd_v3_v3v3(scale, mmd->rnd_scale, rand[2]);

      loc_eul_size_to_mat4(mat_rnd, loc, rot, scale);

      /* Duplicate original strokes to create this instance. */
      LISTBASE_FOREACH_BACKWARD (tmpStrokes *, iter, &stroke_cache) {
        /* Duplicate stroke */
        bGPDstroke *gps_dst = BKE_gpencil_stroke_duplicate(iter->gps, true, true);

        /* Move points */
        for (int i = 0; i < iter->gps->totpoints; i++) {
          bGPDspoint *pt = &gps_dst->points[i];
          /* Apply randomness matrix. */
          mul_m4_v3(mat_rnd, &pt->x);

          /* Apply object local transform (Rot/Scale). */
          if ((mmd->flag & GP_ARRAY_USE_OB_OFFSET) && (mmd->object)) {
            mul_m4_v3(mat, &pt->x);
          }
          /* Global Rotate and scale. */
          mul_mat3_m4_v3(current_offset, &pt->x);
          /* Global translate. */
          add_v3_v3(&pt->x, current_offset[3]);
        }

        /* If replace material, use new one. */
        if ((mmd->mat_rpl > 0) && (mmd->mat_rpl <= ob->totcol)) {
          gps_dst->mat_nr = mmd->mat_rpl - 1;
        }

        /* Add new stroke. */
        BLI_addhead(&iter->gpf->strokes, gps_dst);
        /* Calc bounding box. */
        BKE_gpencil_stroke_boundingbox_calc(gps_dst);
      }
    }

    /* Free temp data. */
    LISTBASE_FOREACH_MUTABLE (tmpStrokes *, tmp, &stroke_cache) {
      BLI_freelinkN(&stroke_cache, tmp);
    }
  }
}
/* END VERBATIM */
#undef generate_geometry

#define generate_geometry multiply_generate_geometry /* ADAPTED: one file holds four modifiers */
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_multiply.c */
static void minter_v3_v3v3v3_ref(
    float *result, float *prev, float *curr, float *next, float *stroke_normal)
{
  float vec[3], inter1[3], inter2[3];
  ARRAY_SET_ITEMS(inter1, 0.0f, 0.0f, 0.0f);
  ARRAY_SET_ITEMS(inter2, 0.0f, 0.0f, 0.0f);

  float minter[3];
  if (prev) {
    sub_v3_v3v3(vec, curr, prev);
    cross_v3_v3v3(inter1, stroke_normal, vec);
  }
  if (next) {
    sub_v3_v3v3(vec, next, curr);
    cross_v3_v3v3(inter2, stroke_normal, vec);
  }
  if (!prev) {
    normalize_v3(inter2);
    copy_v3_v3(result, inter2);
    return;
  }
  if (!next) {
    normalize_v3(inter1);
    copy_v3_v3(result, inter1);
    return;
  }
  interp_v3_v3v3(minter, inter1, inter2, 0.5);
  normalize_v3(minter);
  copy_v3_v3(result, minter);
}

static void duplicateStroke(Object *ob,
                            bGPDstroke *gps,
                            int count,
                            float dist,
                            float offset,
                            ListBase *results,
                            int fading,
                            float fading_center,
                            float fading_thickness,
                            float fading_opacity)
{
  bGPdata *gpd = ob->data;
  int i;
  bGPDstroke *new_gps = NULL;
  float stroke_normal[3];
  bGPDspoint *pt;
  float thickness_factor;
  float opacity_factor;

  /* Apply object scale to offset distance. */
  offset *= mat4_to_scale(ob->object_to_world);

  BKE_gpencil_stroke_normal(gps, stroke_normal);
  if (len_v3(stroke_normal) < FLT_EPSILON) {
    add_v3_fl(stroke_normal, 1);
    normalize_v3(stroke_normal);
  }

  float *t1_array = MEM_callocN(sizeof(float[3]) * gps->totpoints,
                                "duplicate_temp_result_array_1");
  float *t2_array = MEM_callocN(sizeof(float[3]) * gps->totpoints,
                                "duplicate_temp_result_array_2");

  pt = gps->points;

  for (int j = 0; j < gps->totpoints; j++) {
    float minter[3];
    if (j == 0) {
      minter_v3_v3v3v3_ref(minter, NULL, &pt[j].x, &pt[j + 1].x, stroke_normal);
    }
    else if (j == gps->totpoints - 1) {
      minter_v3_v3v3v3_ref(minter, &pt[j - 1].x, &pt[j].x, NULL, stroke_normal);
    }
    else {
      minter_v3_v3v3v3_ref(minter, &pt[j - 1].x, &pt[j].x, &pt[j + 1].x, stroke_normal);
    }
    mul_v3_fl(minter, dist);
    add_v3_v3v3(&t1_array[j * 3], &pt[j].x, minter);
    sub_v3_v3v3(&t2_array[j * 3], &pt[j].x, minter);
  }

  /* This ensures the original stroke is the last one
   * to be processed, since we duplicate its data. */
  for (i = count - 1; i >= 0; i--) {
    if (i != 0) {
      new_gps = BKE_gpencil_stroke_duplicate(gps, true, true);
      BLI_addtail(results, new_gps);
    }
    else {
      new_gps = gps;
    }

    pt = new_gps->points;

    float offset_fac = (count == 1) ? 0.5f : (i / (float)(count - 1));

    if (fading) {
      thickness_factor = interpf(1.0f - fading_thickness, 1.0f, fabsf(offset_fac - fading_center));
      opacity_factor = interpf(1.0f - fading_opacity, 1.0f, fabsf(offset_fac - fading_center));
    }

    for (int j = 0; j < new_gps->totpoints; j++) {
      float fac = interpf(1 + offset, offset, offset_fac);
      interp_v3_v3v3(&pt[j].x, &t1_array[j * 3], &t2_array[j * 3], fac);
      if (fading) {
        pt[j].pressure = gps->points[j].pressure * thickness_factor;
        pt[j].strength = gps->points[j].strength * opacity_factor;
      }
    }
  }
  /* Calc geometry data. */
  if (new_gps != NULL) {
    BKE_gpencil_stroke_geometry_update(gpd, new_gps);
  }
  MEM_freeN(t1_array);
  MEM_freeN(t2_array);
}

/* -------------------------------- */
static void generate_geometry(GpencilModifierData *md, Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
  MultiplyGpencilModifierData *mmd = (MultiplyGpencilModifierData *)md;
  bGPDstroke *gps;
  ListBase duplicates = {0};
  for (gps = gpf->strokes.first; gps; gps = gps->next) {
    if (!is_stroke_affected_by_modifier(ob,
                                        mmd->layername,
                                        mmd->material,
                                        mmd->pass_index,
                                        mmd->layer_pass,
                                        1,
                                        gpl,
                                        gps,
                                        mmd->flag & GP_MIRROR_INVERT_LAYER,
                                        mmd->flag & GP_MIRROR_INVERT_PASS,
                                        mmd->flag & GP_MIRROR_INVERT_LAYERPASS,
                                        mmd->flag & GP_MIRROR_INVERT_MATERIAL))
    {
      continue;
    }
    if (mmd->duplications > 0) {
      duplicateStroke(ob,
                      gps,
                      mmd->duplications,
                      mmd->distance,
                      mmd->offset,
                      &duplicates,
                      mmd->flags & GP_MULTIPLY_ENABLE_FADING,
                      mmd->fading_center,
                      mmd->fading_thickness,
                      mmd->fading_opacity);
    }
  }
  if (!BLI_listbase_is_empty(&duplicates)) {
    BLI_movelisttolist(&gpf->strokes, &duplicates);
  }
}
/* END VERBATIM */
#undef generate_geometry

#define generate_geometry dash_generate_geometry /* ADAPTED: one file holds four modifiers */
/* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_dash.c */
/**
 * Gap==0 means to start the next segment at the immediate next point, which will leave a visual
 * gap of "1 point". This makes the algorithm give the same visual appearance as displayed on the
 * UI and also simplifies the check for "no-length" situation where SEG==0 (which will not produce
 * any effective dash).
 */
static int real_gap(const DashGpencilModifierSegment *ds)
{
  return ds->gap - 1;
}

static bool stroke_dash(const bGPDstroke *gps,
                        const DashGpencilModifierData *dmd,
                        ListBase *r_strokes)
{
  int new_stroke_offset = 0;
  int trim_start = 0;

  int sequence_length = 0;
  for (int i = 0; i < dmd->segments_len; i++) {
    sequence_length += dmd->segments[i].dash + real_gap(&dmd->segments[i]);
  }
  if (sequence_length < 1) {
    /* This means the whole segment has no length, can't do dot-dash. */
    return false;
  }

  const DashGpencilModifierSegment *const first_segment = &dmd->segments[0];
  const DashGpencilModifierSegment *const last_segment = &dmd->segments[dmd->segments_len - 1];
  const DashGpencilModifierSegment *ds = first_segment;

  /* Determine starting configuration using offset. */
  int offset_trim = dmd->dash_offset;
  while (offset_trim < 0) {
    ds = (ds == first_segment) ? last_segment : ds - 1;
    offset_trim += ds->dash + real_gap(ds);
  }

  /* This segment is completely removed from view by the index offset, ignore it. */
  while (ds->dash + real_gap(ds) < offset_trim) {
    offset_trim -= ds->dash + real_gap(ds);
    ds = (ds == last_segment) ? first_segment : ds + 1;
  }

  /* This segment is partially visible at the beginning of the stroke. */
  if (ds->dash > offset_trim) {
    trim_start = offset_trim;
  }
  else {
    /* This segment is not visible but the gap immediately after this segment is partially visible,
     * use next segment's dash. */
    new_stroke_offset += ds->dash + real_gap(ds) - offset_trim;
    ds = (ds == last_segment) ? first_segment : ds + 1;
  }

  while (new_stroke_offset < gps->totpoints - 1) {
    const int seg = ds->dash - trim_start;
    if (!(seg || real_gap(ds))) {
      ds = (ds == last_segment) ? first_segment : ds + 1;
      continue;
    }

    const int size = MIN2(gps->totpoints - new_stroke_offset, seg);
    if (size == 0) {
      continue;
    }

    bGPDstroke *stroke = BKE_gpencil_stroke_new(
        ds->mat_nr < 0 ? gps->mat_nr : ds->mat_nr, size, gps->thickness);
    stroke->runtime.gps_orig = gps->runtime.gps_orig;
    if (ds->flag & GP_DASH_USE_CYCLIC) {
      stroke->flag |= GP_STROKE_CYCLIC;
    }

    for (int is = 0; is < size; is++) {
      bGPDspoint *p = &gps->points[new_stroke_offset + is];
      stroke->points[is].x = p->x;
      stroke->points[is].y = p->y;
      stroke->points[is].z = p->z;
      stroke->points[is].pressure = p->pressure * ds->radius;
      stroke->points[is].strength = p->strength * ds->opacity;
      /* Assign original point pointers. */
      stroke->points[is].runtime.idx_orig = p->runtime.idx_orig;
      stroke->points[is].runtime.pt_orig = p->runtime.pt_orig;
      copy_v4_v4(stroke->points[is].vert_color, p->vert_color);
    }
    BLI_addtail(r_strokes, stroke);

    if (gps->dvert) {
      BKE_gpencil_dvert_ensure(stroke);
      for (int di = 0; di < stroke->totpoints; di++) {
        MDeformVert *dv = &gps->dvert[new_stroke_offset + di];
        if (dv && dv->totweight && dv->dw) {
          MDeformWeight *dw = (MDeformWeight *)MEM_callocN(sizeof(MDeformWeight) * dv->totweight,
                                                           __func__);
          memcpy(dw, dv->dw, sizeof(MDeformWeight) * dv->totweight);
          stroke->dvert[di].dw = dw;
          stroke->dvert[di].totweight = dv->totweight;
          stroke->dvert[di].flag = dv->flag;
        }
      }
    }

    new_stroke_offset += seg + real_gap(ds);
    ds = (ds == last_segment) ? first_segment : ds + 1;
    trim_start = 0;
  }

  return true;
}

static void apply_dash_for_frame(
    Object *ob, bGPDlayer *gpl, bGPdata *gpd, bGPDframe *gpf, DashGpencilModifierData *dmd)
{
  if (dmd->segments_len == 0) {
    return;
  }

  ListBase result = {NULL, NULL};

  LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
    if (is_stroke_affected_by_modifier(ob,
                                       dmd->layername,
                                       dmd->material,
                                       dmd->pass_index,
                                       dmd->layer_pass,
                                       1,
                                       gpl,
                                       gps,
                                       dmd->flag & GP_LENGTH_INVERT_LAYER,
                                       dmd->flag & GP_LENGTH_INVERT_PASS,
                                       dmd->flag & GP_LENGTH_INVERT_LAYERPASS,
                                       dmd->flag & GP_LENGTH_INVERT_MATERIAL))
    {
      if (stroke_dash(gps, dmd, &result)) {
        BLI_remlink(&gpf->strokes, gps);
        BKE_gpencil_free_stroke(gps);
      }
    }
  }
  bGPDstroke *gps_dash;
  while ((gps_dash = BLI_pophead(&result))) {
    BLI_addtail(&gpf->strokes, gps_dash);
    BKE_gpencil_stroke_geometry_update(gpd, gps_dash);
  }
}
/* END VERBATIM */
#undef generate_geometry

/* ---------------------------------------------------------------------------------------- */
/* ADAPTED glue: generateStrokes for the one evaluated frame of the entry's layer.            */

static void pg_gen_frame_space(bGPDframe *gpf, float s, float ys)
{
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    for (int i = 0; i < gps->totpoints; i++) {
      gps->points[i].x *= s;
      gps->points[i].y *= ys;
      gps->points[i].z *= s;
    }
  }
}

int pg_mod_gen_run(const PGModContext *ctx, const PGModEntry *e)
{
  const float *p = e->params;
  const float inv = 1.0f / PG_MOD_PIXELS_PER_UNIT;
  /* a one-layer view of the document whose active frame is the evaluated frame */
  bGPdata view = *ctx->gpd;
  BLI_listbase_clear(&view.layers);
  bGPDlayer layer;
  memset(&layer, 0, sizeof(layer));
  if (ctx->gpl != NULL) {
    layer = *ctx->gpl;
    layer.next = layer.prev = NULL;
  }
  layer.actframe = ctx->gpf;
  BLI_addtail(&view.layers, &layer);
  Object ob;
  memset(&ob, 0, sizeof(ob));
  unit_m4(ob.object_to_world);
  BLI_strncpy(ob.id.name, "OB" PG_MOD_OBJECT_NAME, sizeof(ob.id.name));
  ob.data = &view;
  ob.totcol = (short)ctx->gpd->totcol;

  pg_gen_ctx = ctx;
  pg_gen_entry = e;
  pg_gen_frame_space(ctx->gpf, inv, -inv);
  switch (e->type) {
    case PG_MOD_MIRROR: {
      MirrorGpencilModifierData mmd;
      memset(&mmd, 0, sizeof(mmd));
      mmd.flag = (p[PG_P_MIRROR_X] != 0.0f ? GP_MIRROR_AXIS_X : 0) | (p[PG_P_MIRROR_Y] != 0.0f ? GP_MIRROR_AXIS_Y : 0);
      Object pivot; /* the mirror object: a canvas point; (0, 0) is the object origin (no object) */
      if (p[PG_P_MIRROR_PX] != 0.0f || p[PG_P_MIRROR_PY] != 0.0f) {
        memset(&pivot, 0, sizeof(pivot));
        unit_m4(pivot.object_to_world);
        pivot.object_to_world[3][0] = p[PG_P_MIRROR_PX] / PG_MOD_PIXELS_PER_UNIT;
        pivot.object_to_world[3][1] = -p[PG_P_MIRROR_PY] / PG_MOD_PIXELS_PER_UNIT;
        mmd.object = &pivot;
      }
      mirror_generate_geometry(&mmd.modifier, &ob, &layer, ctx->gpf, false);
      break;
    }
    case PG_MOD_ARRAY: {
      ArrayGpencilModifierData amd;
      memset(&amd, 0, sizeof(amd));
      BLI_strncpy(amd.modifier.name, pg_mod_name(PG_MOD_ARRAY), sizeof(amd.modifier.name));
      amd.count = (int)p[PG_P_ARRAY_COUNT_N];
      amd.flag = (p[PG_P_ARRAY_USE_OFFSET] != 0.0f ? GP_ARRAY_USE_OFFSET : 0) |
                 (p[PG_P_ARRAY_USE_RELATIVE] != 0.0f ? GP_ARRAY_USE_RELATIVE : 0) |
                 (p[PG_P_ARRAY_UNIFORM] != 0.0f ? GP_ARRAY_UNIFORM_RANDOM_SCALE : 0);
      amd.offset[0] = p[PG_P_ARRAY_OX] / PG_MOD_PIXELS_PER_UNIT;
      amd.offset[1] = -p[PG_P_ARRAY_OY] / PG_MOD_PIXELS_PER_UNIT;
      amd.shift[0] = p[PG_P_ARRAY_SHIFT_X];
      amd.shift[1] = p[PG_P_ARRAY_SHIFT_Y];
      amd.rnd_offset[0] = p[PG_P_ARRAY_RND_OX] / PG_MOD_PIXELS_PER_UNIT;
      amd.rnd_offset[1] = -p[PG_P_ARRAY_RND_OY] / PG_MOD_PIXELS_PER_UNIT;
      amd.rnd_rot[2] = p[PG_P_ARRAY_RND_ROT]; /* the canvas plane is the object's XY plane */
      amd.rnd_scale[0] = p[PG_P_ARRAY_RND_SX];
      amd.rnd_scale[1] = p[PG_P_ARRAY_RND_SY];
      amd.seed = (int)p[PG_P_ARRAY_SEED];
      amd.mat_rpl = (int)p[PG_P_ARRAY_MAT_RPL];
      array_generate_geometry(&amd.modifier, NULL, NULL, &ob);
      break;
    }
    case PG_MOD_MULTIPLY: {
      MultiplyGpencilModifierData mmd;
      memset(&mmd, 0, sizeof(mmd));
      mmd.duplications = (int)p[PG_P_MULTIPLY_DUPLICATIONS];
      mmd.distance = p[PG_P_MULTIPLY_DISTANCE] / PG_MOD_PIXELS_PER_UNIT;
      mmd.offset = p[PG_P_MULTIPLY_OFFSET];
      mmd.flags = p[PG_P_MULTIPLY_FADE] != 0.0f ? GP_MULTIPLY_ENABLE_FADING : 0;
      mmd.fading_center = p[PG_P_MULTIPLY_FADE_CENTER];
      mmd.fading_thickness = p[PG_P_MULTIPLY_FADE_THICKNESS];
      mmd.fading_opacity = p[PG_P_MULTIPLY_FADE_OPACITY];
      multiply_generate_geometry(&mmd.modifier, &ob, &layer, ctx->gpf);
      break;
    }
    case PG_MOD_DASH: {
      DashGpencilModifierSegment segs[PG_DASH_MAX_SEGMENTS];
      memset(segs, 0, sizeof(segs));
      DashGpencilModifierData dmd;
      memset(&dmd, 0, sizeof(dmd));
      int nseg = (int)p[PG_P_DASH_SEGMENTS];
      if (nseg < 1) nseg = 1;
      if (nseg > PG_DASH_MAX_SEGMENTS) nseg = PG_DASH_MAX_SEGMENTS;
      for (int i = 0; i < nseg; i++) {
        /* segment 1 is the original fields; 2.. at PG_P_DASH_SEG2 in the same order */
        const float *q = i == 0 ? NULL : p + PG_P_DASH_SEG2 + 6 * (i - 1);
        DashGpencilModifierSegment *seg = &segs[i];
        seg->dash = (int)(q ? q[0] : p[PG_P_DASH_DASH]);
        seg->gap = (int)(q ? q[1] : p[PG_P_DASH_GAP]);
        seg->radius = q ? q[2] : p[PG_P_DASH_RADIUS];
        seg->opacity = q ? q[3] : p[PG_P_DASH_OPACITY];
        seg->mat_nr = (int)(q ? q[4] : p[PG_P_DASH_MAT]);
        seg->flag = (q ? q[5] : p[PG_P_DASH_CYCLIC]) != 0.0f ? GP_DASH_USE_CYCLIC : 0;
        seg->dmd = &dmd;
      }
      dmd.dash_offset = (int)p[PG_P_DASH_OFFSET];
      dmd.segments = segs;
      dmd.segments_len = nseg;
      apply_dash_for_frame(&ob, &layer, &view, ctx->gpf, &dmd);
      break;
    }
    default:
      break;
  }
  pg_gen_frame_space(ctx->gpf, PG_MOD_PIXELS_PER_UNIT, -PG_MOD_PIXELS_PER_UNIT);
  pg_gen_ctx = NULL;
  pg_gen_entry = NULL;
  return 1;
}
