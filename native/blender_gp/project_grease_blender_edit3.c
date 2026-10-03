/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Third batch of Legacy GP operators. These follow the behaviour of the named Blender 3.6.23
 * operators; they are written for this data model, not copied from operator code. Select random
 * draws from the real pinned BLI_rng (rand.cc), so a seed gives the same selection as Blender.
 */

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "BLI_rand.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit3.h"
#include "project_grease_blender_edit4.h"

/* ---- shared glue ----------------------------------------------------------------------- */
static float pe3_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static MaterialGPencilStyle pe3_default_style;
static MaterialGPencilStyle *pe3_style(const bGPdata *gpd, int mat_nr)
{
  if (gpd->mat != NULL && mat_nr >= 0 && mat_nr < (int)gpd->totcol && gpd->mat[mat_nr] != NULL &&
      gpd->mat[mat_nr]->gp_style != NULL)
  {
    return gpd->mat[mat_nr]->gp_style;
  }
  return &pe3_default_style;
}

static bool pe3_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  const MaterialGPencilStyle *st = pe3_style(gpd, gps->mat_nr);
  if (st->flag & GP_MATERIAL_HIDE) return false;
  if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (st->flag & GP_MATERIAL_LOCKED)) return false;
  return true;
}

#define PE3_STROKES_BEGIN(gpd_, only_, gpl, gpf, gps) \
  { \
    const bool multi_ = ((gpd_)->flag & GP_DATA_STROKE_MULTIEDIT) != 0; \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd_)->layers) { \
      if (((only_) != NULL && gpl != (only_)) || !BKE_gpencil_layer_is_editable(gpl)) continue; \
      for (bGPDframe *gpf = multi_ ? (bGPDframe *)gpl->frames.first : gpl->actframe; gpf; gpf = gpf->next) { \
        if (!((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && multi_))) continue; \
        bGPDstroke *next_; \
        for (bGPDstroke *gps = (bGPDstroke *)gpf->strokes.first; gps; gps = next_) { \
          next_ = gps->next; \
          if (!pe3_editable((gpd_), gpl, gps)) continue;
#define PE3_STROKES_END \
        } \
        if (!multi_) break; \
      } \
    } \
  } \
  (void)0

/* ---- 1. select random ------------------------------------------------------------------ */
int pg_gp_select_random(bGPdata *gpd, const bGPDlayer *only_layer, float ratio, unsigned int seed,
                        int select)
{
  if (gpd == NULL || !isfinite(ratio)) return 0;
  ratio = pe3_clampf(ratio, 0.0f, 1.0f);
  RNG *rng = BLI_rng_new(seed);
  int changed = 0;
  PE3_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (gps->points == NULL) continue;
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      if (BLI_rng_get_float(rng) >= ratio) continue;
      const int was = pt->flag & GP_SPOINT_SELECT;
      if (select) pt->flag |= GP_SPOINT_SELECT;
      else pt->flag &= ~GP_SPOINT_SELECT;
      if ((pt->flag & GP_SPOINT_SELECT) != was) changed = 1;
    }
    BKE_gpencil_stroke_sync_selection(gpd, gps);
  }
  PE3_STROKES_END;
  BLI_rng_free(rng);
  return changed;
}

/* ---- 2. insert blank keyframe ------------------------------------------------------------ */
int pg_gp_blank_frame_add(bGPdata *gpd, bGPDlayer *gpl, int cframe)
{
  if (gpd == NULL || gpl == NULL || cframe < 0 || !BKE_gpencil_layer_is_editable(gpl)) return 0;
  bool occupied = false;
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    if (gpf->framenum == cframe) { occupied = true; break; }
  }
  if (occupied) {
    /* make room: every frame at or after cframe moves one frame later */
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (gpf->framenum >= cframe) gpf->framenum += 1;
    }
  }
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, cframe);
  if (gpf == NULL) return 0;
  gpl->actframe = gpf;
  return 1;
}

/* ---- 3. separate fill color ------------------------------------------------------------ */
int pg_gp_material_fill_color(bGPdata *gpd, int mat_nr, const float rgba[4])
{
  if (gpd == NULL || rgba == NULL || mat_nr < 0 || mat_nr >= (int)gpd->totcol || gpd->mat == NULL ||
      gpd->mat[mat_nr] == NULL || gpd->mat[mat_nr]->gp_style == NULL)
  {
    return 0;
  }
  float *dst = gpd->mat[mat_nr]->gp_style->fill_rgba;
  bool changed = false;
  for (int c = 0; c < 4; c++) {
    if (!isfinite(rgba[c])) return 0;
    const float v = pe3_clampf(rgba[c], 0.0f, 1.0f);
    if (dst[c] != v) { dst[c] = v; changed = true; }
  }
  return changed;
}

/* ---- 4. clean loose points ------------------------------------------------------------- */
int pg_gp_frame_clean_loose(bGPdata *gpd, const bGPDlayer *only_layer, int limit)
{
  if (gpd == NULL || limit < 1) return 0;
  int changed = 0;
  PE3_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (gps->totpoints <= limit) {
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
      changed = 1;
    }
  }
  PE3_STROKES_END;
  return changed;
}

/* ---- 5. clean duplicate frames ----------------------------------------------------------- */
static bool pe3_frames_equal(const bGPDframe *a, const bGPDframe *b)
{
  const bGPDstroke *sa = a->strokes.first, *sb = b->strokes.first;
  for (; sa && sb; sa = sa->next, sb = sb->next) {
    if (sa->totpoints != sb->totpoints || sa->mat_nr != sb->mat_nr || sa->thickness != sb->thickness)
      return false;
    for (int i = 0; i < sa->totpoints; i++) {
      const bGPDspoint *p = &sa->points[i], *q = &sb->points[i];
      if (p->x != q->x || p->y != q->y || p->z != q->z || p->pressure != q->pressure ||
          p->strength != q->strength || memcmp(p->vert_color, q->vert_color, sizeof(float[4])) != 0)
        return false;
    }
  }
  return sa == NULL && sb == NULL;
}

int pg_gp_frame_clean_duplicate(bGPdata *gpd, bGPDlayer *gpl)
{
  if (gpd == NULL || gpl == NULL || !BKE_gpencil_layer_is_editable(gpl)) return 0;
  int changed = 0;
  bGPDframe *gpf = gpl->frames.first;
  while (gpf != NULL && gpf->next != NULL) {
    bGPDframe *next = gpf->next;
    if (pe3_frames_equal(gpf, next)) {
      if (gpl->actframe == next) gpl->actframe = gpf;
      BKE_gpencil_layer_frame_delete(gpl, next); /* keep the first of a run */
      changed = 1;
    }
    else {
      gpf = next;
    }
  }
  return changed;
}

/* ---- 6-10. vertex color operators on selected points ----------------------------------- */
typedef void (*PE3ColorFn)(float col[4], const float *p);

static int pe3_apply_vcolor(bGPdata *gpd, const bGPDlayer *only_layer, int mode, PE3ColorFn fn,
                            const float *p, bool only_painted)
{
  if (gpd == NULL || mode < PG_PAINT_MODE_STROKE || mode > PG_PAINT_MODE_BOTH) return 0;
  int changed = 0;
  PE3_STROKES_BEGIN (gpd, only_layer, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->points == NULL) continue;
    if (mode != PG_PAINT_MODE_FILL) {
      for (int i = 0; i < gps->totpoints; i++) {
        bGPDspoint *pt = &gps->points[i];
        if (!(pt->flag & GP_SPOINT_SELECT)) continue;
        if (only_painted && pt->vert_color[3] <= 0.0f) continue;
        fn(pt->vert_color, p);
        changed = 1;
      }
    }
    if (mode != PG_PAINT_MODE_STROKE && (!only_painted || gps->vert_color_fill[3] > 0.0f)) {
      fn(gps->vert_color_fill, p);
      changed = 1;
    }
  }
  PE3_STROKES_END;
  return changed;
}

static void pe3_fn_set(float col[4], const float *p) /* p: r, g, b, factor */
{
  for (int c = 0; c < 3; c++) col[c] = p[c];
  col[3] = p[3];
}
int pg_gp_vcolor_set(bGPdata *gpd, const bGPDlayer *only_layer, int mode, const float rgb[3], float factor)
{
  if (rgb == NULL || !isfinite(factor)) return 0;
  const float p[4] = {rgb[0], rgb[1], rgb[2], pe3_clampf(factor, 0.0f, 1.0f)};
  return pe3_apply_vcolor(gpd, only_layer, mode, pe3_fn_set, p, false);
}

static void pe3_fn_invert(float col[4], const float *p)
{
  (void)p;
  for (int c = 0; c < 3; c++) col[c] = 1.0f - col[c];
}
int pg_gp_vcolor_invert(bGPdata *gpd, const bGPDlayer *only_layer, int mode)
{
  return pe3_apply_vcolor(gpd, only_layer, mode, pe3_fn_invert, NULL, true);
}

/* brightness/contrast as in Blender's vertex color operators (gain/offset form) */
static void pe3_fn_bc(float col[4], const float *p) /* p: gain, offset */
{
  for (int c = 0; c < 3; c++) col[c] = pe3_clampf(p[0] * col[c] + p[1], 0.0f, 1.0f);
}
int pg_gp_vcolor_brightness_contrast(bGPdata *gpd, const bGPDlayer *only_layer, int mode,
                                     float brightness, float contrast)
{
  if (!isfinite(brightness) || !isfinite(contrast)) return 0;
  float gain, offset;
  float delta = contrast / 2.0f;
  /* See: https://en.wikipedia.org/wiki/Contrast_(vision) (comment in Blender's operator) */
  if (contrast > 0.0f) {
    gain = 1.0f - delta * 2.0f;
    gain = 1.0f / (gain > FLT_EPSILON ? gain : FLT_EPSILON);
    offset = gain * (brightness - delta);
  }
  else {
    delta *= -1.0f;
    gain = 1.0f - delta * 2.0f;
    gain = gain > 0.0f ? gain : 0.0f;
    offset = gain * brightness + delta;
  }
  const float p[2] = {gain, offset};
  return pe3_apply_vcolor(gpd, only_layer, mode, pe3_fn_bc, p, true);
}

static void pe3_fn_hsv(float col[4], const float *p) /* p: h, s, v (Blender defaults 0.5, 1, 1) */
{
  float hsv[3];
  pg_rgb_to_hsv(col, hsv);
  hsv[0] += (p[0] - 0.5f);
  if (hsv[0] > 1.0f) hsv[0] -= 1.0f;
  else if (hsv[0] < 0.0f) hsv[0] += 1.0f;
  hsv[1] *= p[1];
  hsv[2] *= p[2];
  pg_hsv_to_rgb(hsv, col);
}
int pg_gp_vcolor_hsv(bGPdata *gpd, const bGPDlayer *only_layer, int mode, float h, float s, float v)
{
  if (!isfinite(h) || !isfinite(s) || !isfinite(v)) return 0;
  const float p[3] = {h, s, v};
  return pe3_apply_vcolor(gpd, only_layer, mode, pe3_fn_hsv, p, true);
}

static void pe3_fn_levels(float col[4], const float *p) /* p: offset, gain */
{
  for (int c = 0; c < 3; c++) col[c] = pe3_clampf(p[1] * (col[c] + p[0]), 0.0f, 1.0f);
}
int pg_gp_vcolor_levels(bGPdata *gpd, const bGPDlayer *only_layer, int mode, float offset, float gain)
{
  if (!isfinite(offset) || !isfinite(gain)) return 0;
  const float p[2] = {offset, gain};
  return pe3_apply_vcolor(gpd, only_layer, mode, pe3_fn_levels, p, true);
}

/* ---- dispatch ------------------------------------------------------------------------------ */
int pg_gp_edit3_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args,
                         int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) {
    if (!isfinite(args[i])) return 0;
  }
  const bGPDlayer *scope = active_layer;
  int changed = 0;
#define NEED(n) if (arg_count < (n)) return 0
  switch (command) {
    case PG_EDIT3_CMD_SELECT_RANDOM:
      NEED(3);
      changed = pg_gp_select_random(gpd, scope, args[0], (unsigned int)lroundf(fabsf(args[1])), args[2] != 0.0f);
      break;
    case PG_EDIT3_CMD_BLANK_FRAME:
      NEED(1);
      changed = pg_gp_blank_frame_add(gpd, active_layer, (int)lroundf(args[0]));
      break;
    case PG_EDIT3_CMD_FILL_COLOR: {
      NEED(5);
      const float rgba[4] = {args[1], args[2], args[3], args[4]};
      changed = pg_gp_material_fill_color(gpd, (int)lroundf(args[0]), rgba);
      break;
    }
    case PG_EDIT3_CMD_CLEAN_LOOSE:
      NEED(1);
      changed = pg_gp_frame_clean_loose(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT3_CMD_CLEAN_DUP_FRAMES:
      changed = pg_gp_frame_clean_duplicate(gpd, active_layer);
      break;
    case PG_EDIT3_CMD_VCOLOR_SET: {
      NEED(5);
      const float rgb[3] = {args[1], args[2], args[3]};
      changed = pg_gp_vcolor_set(gpd, scope, (int)lroundf(args[0]), rgb, args[4]);
      break;
    }
    case PG_EDIT3_CMD_VCOLOR_INVERT:
      NEED(1);
      changed = pg_gp_vcolor_invert(gpd, scope, (int)lroundf(args[0]));
      break;
    case PG_EDIT3_CMD_VCOLOR_BC:
      NEED(3);
      changed = pg_gp_vcolor_brightness_contrast(gpd, scope, (int)lroundf(args[0]), args[1], args[2]);
      break;
    case PG_EDIT3_CMD_VCOLOR_HSV:
      NEED(4);
      changed = pg_gp_vcolor_hsv(gpd, scope, (int)lroundf(args[0]), args[1], args[2], args[3]);
      break;
    case PG_EDIT3_CMD_VCOLOR_LEVELS:
      NEED(3);
      changed = pg_gp_vcolor_levels(gpd, scope, (int)lroundf(args[0]), args[1], args[2]);
      break;
    default:
      return pg_gp_edit4_dispatch(gpd, active_layer, command, args, arg_count);
  }
#undef NEED
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed ? 1 : 0;
}
