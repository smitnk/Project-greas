/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Port of Blender 3.6.23 MOD_gpencil_legacy_build.c (see project_grease_blender_build.h). */
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"

#include "project_grease_blender_build.h"

enum { MODE_SEQUENTIAL = 0, MODE_CONCURRENT = 1, MODE_ADDITIVE = 2 };
enum { T_GROW = 0, T_SHRINK = 1, T_VANISH = 2 };
enum { ALIGN_START = 0, ALIGN_END = 1 };
#define PB_PSEUDOINVERSE_EPSILON 1e-8f

static float pb_interpf(float target, float origin, float fac) { return (fac * target) + (1.0f - fac) * origin; }
static float pb_ratiof(float min, float max, float pos) { const float range = max - min; return range == 0 ? 0.0f : ((pos - min) / range); }

static void clear_stroke(bGPDframe *gpf, bGPDstroke *gps)
{
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);
}

static void gpf_clear_all_strokes(bGPDframe *gpf)
{
  bGPDstroke *gps, *gps_next;
  for (gps = gpf->strokes.first; gps; gps = gps_next) {
    gps_next = gps->next;
    clear_stroke(gpf, gps);
  }
  BLI_listbase_clear(&gpf->strokes);
}

static void reduce_stroke_points(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, const int points_num, const int transition)
{
  if ((points_num == 0) || (gps->points == NULL)) {
    clear_stroke(gpf, gps);
    return;
  }
  bGPDspoint *new_points = MEM_callocN(sizeof(bGPDspoint) * points_num, __func__);
  MDeformVert *new_dvert = NULL;
  if ((gps->dvert != NULL) && (points_num > 0)) {
    new_dvert = MEM_callocN(sizeof(MDeformVert) * points_num, __func__);
  }
  switch (transition) {
    case T_GROW:
    case T_SHRINK:
      memcpy(new_points, gps->points, sizeof(bGPDspoint) * points_num);
      if (gps->dvert != NULL) {
        memcpy(new_dvert, gps->dvert, sizeof(MDeformVert) * points_num);
        for (int i = points_num; i < gps->totpoints; i++) BKE_gpencil_free_point_weights(&gps->dvert[i]);
      }
      break;
    case T_VANISH: {
      const int offset = gps->totpoints - points_num;
      memcpy(new_points, gps->points + offset, sizeof(bGPDspoint) * points_num);
      if (gps->dvert != NULL) {
        memcpy(new_dvert, gps->dvert + offset, sizeof(MDeformVert) * points_num);
        for (int i = 0; i < offset; i++) BKE_gpencil_free_point_weights(&gps->dvert[i]);
      }
      break;
    }
    default:
      break;
  }
  MEM_SAFE_FREE(gps->points);
  MEM_SAFE_FREE(gps->dvert);
  gps->points = new_points;
  gps->dvert = new_dvert;
  gps->totpoints = points_num;
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/* target_def_nr is always -1 here: the fade's vertex-group output is not supported. */
static void fade_stroke_points(bGPDstroke *gps, const int starting_index, const int ending_index,
                               const float starting_weight, const float ending_weight,
                               const float thickness_strength, const float opacity_strength)
{
  int range = ending_index - starting_index;
  if (!range) range = 1;
  for (int i = starting_index; i <= ending_index; i++) {
    float weight = pb_interpf(ending_weight, starting_weight, (float)(i - starting_index) / range);
    if (thickness_strength > 1e-5) gps->points[i].pressure *= pb_interpf(weight, 1.0f, thickness_strength);
    if (opacity_strength > 1e-5) gps->points[i].strength *= pb_interpf(weight, 1.0f, opacity_strength);
  }
}

typedef struct tStrokeBuildDetails {
  bGPDstroke *gps;
  size_t start_idx, end_idx;
  int totpoints;
} tStrokeBuildDetails;

static void build_sequential(const PGBuildParams *mmd, bGPdata *gpd, bGPDframe *gpf, int prev_strokes, float fac)
{
  size_t tot_strokes = BLI_listbase_count(&gpf->strokes);
  size_t start_stroke = 0;
  bGPDstroke *gps;
  size_t i;

  if (mmd->mode == MODE_ADDITIVE) {
    if (prev_strokes >= 0) start_stroke = (size_t)prev_strokes;
    if (start_stroke <= tot_strokes) tot_strokes = tot_strokes - start_stroke;
    else start_stroke = 0;
  }
  if (tot_strokes == 0) return;

  tStrokeBuildDetails *table = MEM_callocN(sizeof(tStrokeBuildDetails) * tot_strokes, __func__);
  size_t sumpoints = 0;
  for (gps = BLI_findlink(&gpf->strokes, (int)start_stroke), i = 0; gps; gps = gps->next, i++) {
    tStrokeBuildDetails *cell = &table[i];
    cell->gps = gps;
    cell->totpoints = gps->totpoints;
    sumpoints += cell->totpoints;
  }
  for (i = 0; i < tot_strokes; i++) {
    tStrokeBuildDetails *cell = &table[i];
    cell->start_idx = (i == 0) ? 0 : (cell - 1)->end_idx + 1;
    cell->end_idx = cell->start_idx + cell->totpoints - 1;
  }

  size_t first_visible = 0, last_visible = 0;
  int fade_start = 0, fade_end = 0;
  const float set_fade_fac = mmd->use_fading ? mmd->fade_fac : 0.0f;
  const float use_fac = pb_interpf(1 + set_fade_fac, 0, fac);
  float use_fade_fac = use_fac - set_fade_fac;
  CLAMP(use_fade_fac, 0.0f, 1.0f);

  switch (mmd->transition) {
    case T_GROW:
      first_visible = 0;
      last_visible = (size_t)roundf(sumpoints * use_fac);
      fade_start = (int)roundf(sumpoints * use_fade_fac);
      fade_end = (int)last_visible;
      break;
    case T_SHRINK:
      first_visible = 0;
      last_visible = (size_t)(sumpoints * (1.0f + set_fade_fac - use_fac));
      fade_start = (int)roundf(sumpoints * (1.0f - use_fade_fac - set_fade_fac));
      fade_end = (int)last_visible;
      break;
    case T_VANISH:
      first_visible = (size_t)(sumpoints * use_fade_fac);
      last_visible = sumpoints;
      fade_start = (int)first_visible;
      fade_end = (int)roundf(sumpoints * use_fac);
      break;
  }

  for (i = 0; i < tot_strokes; i++) {
    tStrokeBuildDetails *cell = &table[i];
    if ((cell->end_idx < first_visible) || (cell->start_idx > last_visible)) {
      clear_stroke(gpf, cell->gps);
    }
    else {
      if (fade_start != fade_end && (int)cell->start_idx < fade_end && (int)cell->end_idx > fade_start) {
        int start_index = fade_start - (int)cell->start_idx;
        int end_index = cell->totpoints + fade_end - (int)cell->end_idx - 1;
        CLAMP(start_index, 0, cell->totpoints - 1);
        CLAMP(end_index, 0, cell->totpoints - 1);
        float start_weight = pb_ratiof(fade_start, fade_end, cell->start_idx + start_index);
        float end_weight = pb_ratiof(fade_start, fade_end, cell->start_idx + end_index);
        if (mmd->transition != T_VANISH) {
          start_weight = 1.0f - start_weight;
          end_weight = 1.0f - end_weight;
        }
        fade_stroke_points(cell->gps, start_index, end_index, start_weight, end_weight,
                           mmd->fade_thickness_strength, mmd->fade_opacity_strength);
        BKE_gpencil_stroke_geometry_update(gpd, cell->gps);
      }
      if ((first_visible <= cell->start_idx) && (last_visible >= cell->end_idx)) {
        /* whole stroke visible */
      }
      else if (first_visible > cell->start_idx) {
        int points_num = (int)(cell->end_idx - first_visible);
        reduce_stroke_points(gpd, gpf, cell->gps, points_num, mmd->transition);
      }
      else {
        int points_num = (int)(last_visible - cell->start_idx);
        reduce_stroke_points(gpd, gpf, cell->gps, points_num, mmd->transition);
      }
    }
  }
  MEM_freeN(table);
}

static void build_concurrent(const PGBuildParams *mmd, bGPdata *gpd, bGPDframe *gpf, float fac)
{
  bGPDstroke *gps, *gps_next;
  int max_points = 0;
  const bool reverse = (mmd->transition != T_GROW);
  for (gps = gpf->strokes.first; gps; gps = gps->next) {
    if (gps->totpoints > max_points) max_points = gps->totpoints;
  }
  if (max_points == 0) return;

  const float set_fade_fac = mmd->use_fading ? mmd->fade_fac : 0.0f;
  float use_fac = pb_interpf(1 + set_fade_fac, 0, fac);
  use_fac = reverse ? use_fac - set_fade_fac : use_fac;
  int fade_points = (int)(set_fade_fac * max_points);

  for (gps = gpf->strokes.first; gps; gps = gps_next) {
    gps_next = gps->next;
    const float relative_len = (float)gps->totpoints / (float)max_points;
    int points_num = 0;
    switch (mmd->time_alignment) {
      case ALIGN_START: {
        const float scaled_fac = use_fac / MAX2(relative_len, PB_PSEUDOINVERSE_EPSILON);
        points_num = reverse ? (int)roundf((1.0f - scaled_fac) * gps->totpoints) : (int)roundf(scaled_fac * gps->totpoints);
        break;
      }
      case ALIGN_END: {
        const float start_fac = 1.0f - relative_len;
        const float scaled_fac = (use_fac - start_fac) / MAX2(relative_len, PB_PSEUDOINVERSE_EPSILON);
        points_num = reverse ? (int)roundf((1.0f - scaled_fac) * gps->totpoints) : (int)roundf(scaled_fac * gps->totpoints);
        break;
      }
    }
    if (points_num <= 0) {
      clear_stroke(gpf, gps);
    }
    else {
      int more_points = points_num - gps->totpoints;
      CLAMP(more_points, 0, fade_points + 1);
      /* Blender divides by fade_points (inf/nan without fading, then clamped); guard the 0 case
       * with the same clamped result so the weights never become NaN. */
      float max_weight = fade_points ? (float)(points_num + more_points) / fade_points : 1.0f;
      CLAMP(max_weight, 0.0f, 1.0f);
      const float more_w = fade_points ? (float)more_points / fade_points : 0.0f;
      int starting_index = mmd->transition == T_VANISH ? gps->totpoints - points_num - more_points :
                                                        points_num - 1 - fade_points + more_points;
      int ending_index = mmd->transition == T_VANISH ? gps->totpoints - points_num + fade_points - more_points :
                                                      points_num - 1 + more_points;
      float starting_weight = mmd->transition == T_VANISH ? more_w : max_weight;
      float ending_weight = mmd->transition == T_VANISH ? max_weight : more_w;
      CLAMP(starting_index, 0, gps->totpoints - 1);
      CLAMP(ending_index, 0, gps->totpoints - 1);
      fade_stroke_points(gps, starting_index, ending_index, starting_weight, ending_weight,
                         mmd->fade_thickness_strength, mmd->fade_opacity_strength);
      if (points_num < gps->totpoints) reduce_stroke_points(gpd, gpf, gps, points_num, mmd->transition);
    }
  }
}

int pg_build_generate(bGPdata *gpd, bGPDframe *gpf, int framenum, int prev_strokes, int next_framenum,
                      float ctime, const PGBuildParams *in)
{
  if (gpd == NULL || gpf == NULL || in == NULL || !isfinite(ctime)) return 0;
  PGBuildParams mmd = *in;
  if (mmd.mode < MODE_SEQUENTIAL || mmd.mode > MODE_ADDITIVE || mmd.transition < T_GROW || mmd.transition > T_VANISH) return 0;
  /* Prevent incompatible options at runtime. */
  if (mmd.mode == MODE_ADDITIVE) {
    mmd.transition = T_GROW;
    mmd.start_delay = 0;
  }
  const bool reverse = (mmd.transition != T_GROW);
  const bool is_percentage = mmd.percentage != 0;
  if (gpf->strokes.first == NULL) return 0;
  if (mmd.restrict_time && ((ctime < mmd.start_frame) || (ctime > mmd.end_frame))) return 0;

  const int before = BLI_listbase_count(&gpf->strokes);
  float start_frame = is_percentage ? framenum : framenum + mmd.start_delay;
  float end_frame = is_percentage ? start_frame + 9999 : start_frame + mmd.length;
  if (next_framenum >= 0) end_frame = MIN2(end_frame, (float)next_framenum);
  if (ctime < start_frame) {
    if (!reverse) gpf_clear_all_strokes(gpf);
    return before > 0 && !reverse;
  }
  if (ctime >= end_frame) {
    if (reverse) gpf_clear_all_strokes(gpf);
    return before > 0 && reverse;
  }
  const float fac = is_percentage ? mmd.percentage_fac : (ctime - start_frame) / (end_frame - start_frame);
  switch (mmd.mode) {
    case MODE_SEQUENTIAL:
    case MODE_ADDITIVE:
      build_sequential(&mmd, gpd, gpf, prev_strokes, fac);
      break;
    case MODE_CONCURRENT:
      build_concurrent(&mmd, gpd, gpf, fac);
      break;
  }
  return 1;
}
