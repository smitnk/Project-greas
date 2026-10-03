/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Fourth batch of Legacy GP operations. Dash/Multiply/Array apply the behaviour of the 3.6.23
 * generator modifiers once to the selected strokes (Project Grease adaptation: baked, 2D);
 * the operators follow the named Blender operators and use BKE where Blender has a function.
 */
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

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit4.h"

static MaterialGPencilStyle pe4_default_style;
static MaterialGPencilStyle *pe4_style(const bGPdata *gpd, int mat_nr)
{
  if (gpd->mat != NULL && mat_nr >= 0 && mat_nr < (int)gpd->totcol && gpd->mat[mat_nr] != NULL &&
      gpd->mat[mat_nr]->gp_style != NULL)
    return gpd->mat[mat_nr]->gp_style;
  return &pe4_default_style;
}
static bool pe4_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  const MaterialGPencilStyle *st = pe4_style(gpd, gps->mat_nr);
  if (st->flag & GP_MATERIAL_HIDE) return false;
  if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (st->flag & GP_MATERIAL_LOCKED)) return false;
  return true;
}
static bool pe4_sel(const bGPDstroke *gps) { return (gps->flag & GP_STROKE_SELECT) != 0; }

#define PE4_STROKES_BEGIN(gpd_, only_, gpl, gpf, gps) \
  { \
    const bool multi_ = ((gpd_)->flag & GP_DATA_STROKE_MULTIEDIT) != 0; \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd_)->layers) { \
      if (((only_) != NULL && gpl != (only_)) || !BKE_gpencil_layer_is_editable(gpl)) continue; \
      for (bGPDframe *gpf = multi_ ? (bGPDframe *)gpl->frames.first : gpl->actframe; gpf; gpf = gpf->next) { \
        if (!((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && multi_))) continue; \
        bGPDstroke *next_; \
        for (bGPDstroke *gps = (bGPDstroke *)gpf->strokes.first; gps; gps = next_) { \
          next_ = gps->next; \
          if (!pe4_editable((gpd_), gpl, gps)) continue;
#define PE4_STROKES_END \
        } \
        if (!multi_) break; \
      } \
    } \
  } \
  (void)0

static void pe4_insert_after(ListBase *lb, bGPDstroke *prev, bGPDstroke *s)
{
  s->prev = prev;
  s->next = prev ? prev->next : (bGPDstroke *)lb->first;
  if (s->next) s->next->prev = s; else lb->last = s;
  if (prev) prev->next = s; else lb->first = s;
}

static void pe4_deselect(bGPDstroke *gps)
{
  gps->flag &= ~GP_STROKE_SELECT;
  for (int i = 0; i < gps->totpoints; i++) gps->points[i].flag &= ~GP_SPOINT_SELECT;
}

/* copy of points [a, b] of `src` as a new stroke (duplicate, then trim the point array) */
static bGPDstroke *pe4_copy_range(bGPDstroke *src, int a, int b)
{
  bGPDstroke *d = BKE_gpencil_stroke_duplicate(src, true, true);
  if (d == NULL) return NULL;
  const int n = b - a + 1;
  memmove(d->points, d->points + a, sizeof(bGPDspoint) * (size_t)n);
  d->totpoints = n;
  d->flag &= ~GP_STROKE_CYCLIC;
  d->dvert = NULL; /* weights are not carried into generated pieces */
  return d;
}

/* ---- 1. Dash (MOD_gpencil_legacy_dash.c, one segment: dash points on, gap points off) ---- */
int pg_gp_dash(bGPdata *gpd, const bGPDlayer *only, int dash, int gap, int offset)
{
  if (gpd == NULL || dash < 1 || gap < 0 || (gap == 0)) return 0;
  int changed = 0;
  PE4_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!pe4_sel(gps) || gps->points == NULL || gps->totpoints < 2 || gps->dvert != NULL) continue;
    const int period = dash + gap;
    bGPDstroke *after = gps;
    int made = 0;
    int i = 0;
    while (i < gps->totpoints) {
      const int phase = ((i + offset) % period + period) % period;
      if (phase >= dash) { i++; continue; }
      int a = i, b = i;
      while (b + 1 < gps->totpoints && ((b + 1 + offset) % period + period) % period < dash) b++;
      if (b > a) { /* a dash needs at least two points to draw */
        bGPDstroke *piece = pe4_copy_range(gps, a, b);
        if (piece) {
          pe4_insert_after(&gpf->strokes, after, piece);
          BKE_gpencil_stroke_geometry_update(gpd, piece);
          after = piece;
          made++;
        }
      }
      i = b + 1;
    }
    if (made > 0) {
      BLI_remlink(&gpf->strokes, gps);
      BKE_gpencil_free_stroke(gps);
      changed = 1;
    }
  }
  PE4_STROKES_END;
  return changed;
}

/* per-point 2D normal of a stroke (average of the adjacent segment normals) */
static void pe4_normal(const bGPDstroke *s, int i, float r[2])
{
  const bGPDspoint *p = s->points;
  const int a = i > 0 ? i - 1 : i, b = i + 1 < s->totpoints ? i + 1 : i;
  float dx = p[b].x - p[a].x, dy = p[b].y - p[a].y;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1e-8f) { r[0] = 0; r[1] = 0; return; }
  r[0] = -dy / len; r[1] = dx / len;
}

/* ---- 2. Multiply (MOD_gpencil_legacy_multiply.c duplication along the stroke normal; 2D) ---- */
int pg_gp_multiply(bGPdata *gpd, const bGPDlayer *only, int duplications, float distance)
{
  if (gpd == NULL || duplications < 1 || duplications > 100 || !isfinite(distance)) return 0;
  int changed = 0;
  PE4_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!pe4_sel(gps) || gps->points == NULL || gps->totpoints < 2) continue;
    bGPDstroke *after = gps;
    for (int k = 1; k <= duplications; k++) {
      /* copies alternate sides: +d, -d, +2d, -2d ... */
      const float shift = distance * (float)((k + 1) / 2) * ((k % 2) ? 1.0f : -1.0f);
      bGPDstroke *c = BKE_gpencil_stroke_duplicate(gps, true, true);
      if (c == NULL) break;
      for (int i = 0; i < c->totpoints; i++) {
        float nrm[2];
        pe4_normal(gps, i, nrm);
        c->points[i].x = gps->points[i].x + nrm[0] * shift;
        c->points[i].y = gps->points[i].y + nrm[1] * shift;
      }
      pe4_deselect(c);
      pe4_insert_after(&gpf->strokes, after, c);
      BKE_gpencil_stroke_geometry_update(gpd, c);
      after = c;
      changed = 1;
    }
  }
  PE4_STROKES_END;
  return changed;
}

/* ---- 3. Array (MOD_gpencil_legacy_array.c constant offset) ---- */
int pg_gp_array(bGPdata *gpd, const bGPDlayer *only, int count, float ox, float oy)
{
  if (gpd == NULL || count < 2 || count > 1000 || !isfinite(ox) || !isfinite(oy)) return 0;
  int changed = 0;
  PE4_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!pe4_sel(gps) || gps->points == NULL) continue;
    bGPDstroke *after = gps;
    for (int k = 1; k < count; k++) {
      bGPDstroke *c = BKE_gpencil_stroke_duplicate(gps, true, true);
      if (c == NULL) break;
      for (int i = 0; i < c->totpoints; i++) { c->points[i].x += ox * k; c->points[i].y += oy * k; }
      pe4_deselect(c);
      pe4_insert_after(&gpf->strokes, after, c);
      BKE_gpencil_stroke_geometry_update(gpd, c);
      after = c;
      changed = 1;
    }
  }
  PE4_STROKES_END;
  return changed;
}

/* ---- 4. Merge by distance (GPENCIL_OT_stroke_merge_by_distance) ---- */
int pg_gp_merge_distance(bGPdata *gpd, const bGPDlayer *only, float threshold, int use_unselected)
{
  if (gpd == NULL || !isfinite(threshold) || threshold <= 0.0f) return 0;
  int changed = 0;
  PE4_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!pe4_sel(gps) || gps->totpoints < 2) continue;
    const int before = gps->totpoints;
    BKE_gpencil_stroke_merge_distance(gpd, gpf, gps, threshold, use_unselected != 0);
    if (gps->totpoints != before) changed = 1;
  }
  PE4_STROKES_END;
  return changed;
}

/* ---- 5. Caps (GPENCIL_OT_stroke_caps_set) ---- */
int pg_gp_caps_set(bGPdata *gpd, const bGPDlayer *only, int type)
{
  if (gpd == NULL || type < PG_CAPS_TOGGLE_BOTH || type > PG_CAPS_DEFAULT) return 0;
  int changed = 0;
  PE4_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!pe4_sel(gps)) continue;
    const short c0 = gps->caps[0], c1 = gps->caps[1];
    if (type == PG_CAPS_TOGGLE_BOTH || type == PG_CAPS_TOGGLE_START) gps->caps[0] = gps->caps[0] ? 0 : 1;
    if (type == PG_CAPS_TOGGLE_BOTH || type == PG_CAPS_TOGGLE_END) gps->caps[1] = gps->caps[1] ? 0 : 1;
    if (type == PG_CAPS_DEFAULT) { gps->caps[0] = 0; gps->caps[1] = 0; }
    if (gps->caps[0] != c0 || gps->caps[1] != c1) changed = 1;
  }
  PE4_STROKES_END;
  return changed;
}

/* ---- 6. Start point (GPENCIL_OT_stroke_start_set: cyclic strokes start at the selected point) ---- */
int pg_gp_start_set(bGPdata *gpd, const bGPDlayer *only)
{
  if (gpd == NULL) return 0;
  int changed = 0;
  PE4_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!pe4_sel(gps) || !(gps->flag & GP_STROKE_CYCLIC) || gps->totpoints < 3 || gps->dvert != NULL) continue;
    int start = -1;
    for (int i = 0; i < gps->totpoints; i++) if (gps->points[i].flag & GP_SPOINT_SELECT) { start = i; break; }
    if (start <= 0) continue;
    /* rotate left by `start` using three reversals (no allocation) */
    bGPDspoint *p = gps->points;
    const int n = gps->totpoints;
#define PE4_REV(lo, hi) for (int l = (lo), h = (hi); l < h; l++, h--) { bGPDspoint t = p[l]; p[l] = p[h]; p[h] = t; }
    PE4_REV(0, start - 1);
    PE4_REV(start, n - 1);
    PE4_REV(0, n - 1);
#undef PE4_REV
    BKE_gpencil_stroke_geometry_update(gpd, gps);
    changed = 1;
  }
  PE4_STROKES_END;
  return changed;
}

/* move the selected strokes of `src`'s active frame into `dst` (frame with the same number) */
static int pe4_move_selected(bGPdata *gpd, bGPDlayer *src, bGPDlayer *dst)
{
  if (src->actframe == NULL) return 0;
  bGPDframe *from = src->actframe;
  bGPDframe *to = NULL;
  LISTBASE_FOREACH (bGPDframe *, f, &dst->frames) if (f->framenum == from->framenum) { to = f; break; }
  int moved = 0;
  for (bGPDstroke *gps = from->strokes.first, *next; gps; gps = next) {
    next = gps->next;
    if (!pe4_sel(gps) || !pe4_editable(gpd, src, gps)) continue;
    if (to == NULL) {
      to = BKE_gpencil_frame_addnew(dst, from->framenum);
      if (to == NULL) return moved;
    }
    BLI_remlink(&from->strokes, gps);
    BLI_addtail(&to->strokes, gps);
    moved = 1;
  }
  if (moved) dst->actframe = to;
  return moved;
}

/* ---- 7. Separate to a new layer (GPENCIL_OT_stroke_separate, mode LAYER) ---- */
int pg_gp_separate_to_layer(bGPdata *gpd, bGPDlayer *src)
{
  if (gpd == NULL || src == NULL || !BKE_gpencil_layer_is_editable(src) || src->actframe == NULL) return 0;
  bool any = false;
  LISTBASE_FOREACH (bGPDstroke *, gps, &src->actframe->strokes) any |= pe4_sel(gps);
  if (!any) return 0;
  bGPDlayer *dst = BKE_gpencil_layer_addnew(gpd, "Separated", false, false);
  if (dst == NULL) return 0;
  return pe4_move_selected(gpd, src, dst);
}

/* ---- 8. Move to layer (GPENCIL_OT_move_to_layer) ---- */
int pg_gp_move_to_layer(bGPdata *gpd, bGPDlayer *src, int target_index)
{
  if (gpd == NULL || src == NULL || !BKE_gpencil_layer_is_editable(src)) return 0;
  bGPDlayer *dst = BLI_findlink(&gpd->layers, target_index);
  if (dst == NULL || dst == src || !BKE_gpencil_layer_is_editable(dst)) return 0;
  return pe4_move_selected(gpd, src, dst);
}

/* ---- 9/10. Copy / paste (GPENCIL_OT_copy / GPENCIL_OT_paste; one clipboard per process) ---- */
static ListBase pe4_clipboard = {NULL, NULL};

void pg_gp_clipboard_free(void)
{
  for (bGPDstroke *s = pe4_clipboard.first, *n; s; s = n) { n = s->next; BKE_gpencil_free_stroke(s); }
  pe4_clipboard.first = pe4_clipboard.last = NULL;
}

int pg_gp_copy(bGPdata *gpd, const bGPDlayer *only)
{
  if (gpd == NULL) return 0;
  ListBase fresh = {NULL, NULL};
  PE4_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!pe4_sel(gps) || gps->points == NULL) continue;
    bGPDstroke *c = BKE_gpencil_stroke_duplicate(gps, true, true);
    if (c) BLI_addtail(&fresh, c);
  }
  PE4_STROKES_END;
  if (fresh.first == NULL) return 0; /* keep the old clipboard when nothing is selected */
  pg_gp_clipboard_free();
  pe4_clipboard = fresh;
  return 0; /* copying does not change the document */
}

int pg_gp_paste(bGPdata *gpd, bGPDlayer *target)
{
  if (gpd == NULL || target == NULL || target->actframe == NULL || pe4_clipboard.first == NULL ||
      !BKE_gpencil_layer_is_editable(target))
    return 0;
  /* deselect what is there; pasted strokes become the selection */
  LISTBASE_FOREACH (bGPDstroke *, gps, &target->actframe->strokes) pe4_deselect(gps);
  LISTBASE_FOREACH (bGPDstroke *, s, &pe4_clipboard) {
    bGPDstroke *c = BKE_gpencil_stroke_duplicate(s, true, true);
    if (c == NULL) continue;
    if (c->mat_nr >= (int)gpd->totcol) c->mat_nr = 0; /* material slot missing in this document */
    c->flag |= GP_STROKE_SELECT;
    for (int i = 0; i < c->totpoints; i++) c->points[i].flag |= GP_SPOINT_SELECT;
    BLI_addtail(&target->actframe->strokes, c);
    BKE_gpencil_stroke_geometry_update(gpd, c);
  }
  return 1;
}

int pg_gp_edit4_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args, int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) if (!isfinite(args[i])) return 0;
  const bGPDlayer *scope = active_layer;
  int changed = 0;
#define NEED(n) if (arg_count < (n)) return 0
#define I(k) ((int)lroundf(args[k]))
  switch (command) {
    case PG_EDIT4_CMD_DASH: NEED(3); changed = pg_gp_dash(gpd, scope, I(0), I(1), I(2)); break;
    case PG_EDIT4_CMD_MULTIPLY: NEED(2); changed = pg_gp_multiply(gpd, scope, I(0), args[1]); break;
    case PG_EDIT4_CMD_ARRAY: NEED(3); changed = pg_gp_array(gpd, scope, I(0), args[1], args[2]); break;
    case PG_EDIT4_CMD_MERGE_DISTANCE: NEED(2); changed = pg_gp_merge_distance(gpd, scope, args[0], args[1] != 0.0f); break;
    case PG_EDIT4_CMD_CAPS: NEED(1); changed = pg_gp_caps_set(gpd, scope, I(0)); break;
    case PG_EDIT4_CMD_START_SET: changed = pg_gp_start_set(gpd, scope); break;
    case PG_EDIT4_CMD_SEPARATE_LAYER: changed = pg_gp_separate_to_layer(gpd, active_layer); break;
    case PG_EDIT4_CMD_MOVE_TO_LAYER: NEED(1); changed = pg_gp_move_to_layer(gpd, active_layer, I(0)); break;
    case PG_EDIT4_CMD_COPY: pg_gp_copy(gpd, scope); return 1; /* report success; nothing changed */
    case PG_EDIT4_CMD_PASTE: changed = pg_gp_paste(gpd, active_layer); break;
    default: return 0;
  }
#undef I
#undef NEED
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed ? 1 : 0;
}
