/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_blender_edit9.h. Proportional editing follows transform_generics.c
 * calculatePropRatio (Blender 3.6.23): selected points weigh 1, others by the falloff of
 * (size - distance) / size, nothing beyond the radius; connected mode measures along the stroke. */
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "project_grease_blender_edit9.h"

float pg_prop_falloff(int f, float d, float size)
{
  /* td->rdist > t->prop_size -> 0 (strictly greater: at d == size Constant still gives 1) */
  if (size <= 0 || d > size) return 0.0f;
  float t = (size - d) / size; /* "dist" in calculatePropRatio: 1 at the selection, 0 at the edge */
  if (t < 0.0f) t = 0.0f;
  switch (f) {
    case PG_PROP_SHARP: return t * t;
    case PG_PROP_SMOOTH: return 3.0f * t * t - 2.0f * t * t * t;
    case PG_PROP_ROOT: return sqrtf(t);
    case PG_PROP_LIN: return t;
    case PG_PROP_CONST: return 1.0f;
    case PG_PROP_SPHERE: return sqrtf(2.0f * t - t * t);
    case PG_PROP_INVSQUARE: return t * (2.0f - t);
    default: return 1.0f; /* calculatePropRatio's default */
  }
}

static bool pe9_layer_ok(const bGPDlayer *gpl, const bGPDlayer *only)
{
  return (only == NULL || gpl == only) && BKE_gpencil_layer_is_editable(gpl) && gpl->actframe;
}

static void pe9_apply(const PGTransform *t, float px, float py, float w, float cx, float cy, float *rx, float *ry)
{
  float x = px, y = py;
  if (t->type == PG_XFORM_TRANSLATE) { x += t->a * w; y += t->b * w; }
  else if (t->type == PG_XFORM_ROTATE) {
    float a = t->a * w, c = cosf(a), s = sinf(a), dx = px - cx, dy = py - cy;
    x = cx + dx * c - dy * s; y = cy + dx * s + dy * c;
  }
  else { /* scale: interpolate the factor by the weight */
    float sx = 1.0f + (t->a - 1.0f) * w, sy = 1.0f + (t->b - 1.0f) * w;
    x = cx + (px - cx) * sx; y = cy + (py - cy) * sy;
  }
  *rx = x; *ry = y;
}

int pg_gp_transform(bGPdata *gpd, const bGPDlayer *only, const PGTransform *t)
{
  if (gpd == NULL || t == NULL || !isfinite(t->a) || !isfinite(t->b) || t->type < 0 || t->type > 2) return 0;
  /* pivot over all selected points */
  double sx = 0, sy = 0; long n = 0;
  float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!pe9_layer_ok(gpl, only)) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      if (!(gps->flag & GP_STROKE_SELECT)) continue;
      for (int i = 0; i < gps->totpoints; i++) {
        const bGPDspoint *p = &gps->points[i];
        if (!(p->flag & GP_SPOINT_SELECT)) continue;
        sx += p->x; sy += p->y; n++;
        minx = fminf(minx, p->x); maxx = fmaxf(maxx, p->x); miny = fminf(miny, p->y); maxy = fmaxf(maxy, p->y);
      }
    }
  }
  if (n == 0) return 0;
  float cx = (float)(sx / n), cy = (float)(sy / n);
  if (t->pivot == PG_PIVOT_BOUNDS) { cx = 0.5f * (minx + maxx); cy = 0.5f * (miny + maxy); }
  else if (t->pivot == PG_PIVOT_CURSOR) { cx = t->cursor[0]; cy = t->cursor[1]; }

  int changed = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!pe9_layer_ok(gpl, only)) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      const bool sel = (gps->flag & GP_STROKE_SELECT) != 0;
      if (!sel && !t->proportional) continue;
      float lcx = cx, lcy = cy;
      if (t->pivot == PG_PIVOT_INDIVIDUAL && sel) { /* each stroke about its own selected-points median */
        double ax = 0, ay = 0; int m = 0;
        for (int i = 0; i < gps->totpoints; i++) if (gps->points[i].flag & GP_SPOINT_SELECT) { ax += gps->points[i].x; ay += gps->points[i].y; m++; }
        if (m) { lcx = (float)(ax / m); lcy = (float)(ay / m); }
      }
      /* proportional weights: distance to the nearest selected point (straight, or along the stroke) */
      float *w = malloc(sizeof(float) * (size_t)gps->totpoints);
      if (w == NULL) continue;
      for (int i = 0; i < gps->totpoints; i++) w[i] = (gps->points[i].flag & GP_SPOINT_SELECT) && sel ? 1.0f : 0.0f;
      if (t->proportional) {
        for (int i = 0; i < gps->totpoints; i++) {
          if (w[i] == 1.0f) continue;
          float best = 1e30f;
          if (t->connected) {
            if (!sel) continue; /* connected: only within strokes that have selected points */
            float acc = 0; /* walk both directions along the stroke */
            for (int j = i - 1; j >= 0; j--) { acc += hypotf(gps->points[j + 1].x - gps->points[j].x, gps->points[j + 1].y - gps->points[j].y);
              if (gps->points[j].flag & GP_SPOINT_SELECT) { best = fminf(best, acc); break; } }
            acc = 0;
            for (int j = i + 1; j < gps->totpoints; j++) { acc += hypotf(gps->points[j].x - gps->points[j - 1].x, gps->points[j].y - gps->points[j - 1].y);
              if (gps->points[j].flag & GP_SPOINT_SELECT) { best = fminf(best, acc); break; } }
          }
          else {
            LISTBASE_FOREACH (bGPDlayer *, l2, &gpd->layers) {
              if (!pe9_layer_ok(l2, only)) continue;
              LISTBASE_FOREACH (bGPDstroke *, s2, &l2->actframe->strokes) {
                if (!(s2->flag & GP_STROKE_SELECT)) continue;
                for (int j = 0; j < s2->totpoints; j++) if (s2->points[j].flag & GP_SPOINT_SELECT)
                  best = fminf(best, hypotf(s2->points[j].x - gps->points[i].x, s2->points[j].y - gps->points[i].y));
              }
            }
          }
          w[i] = pg_prop_falloff(t->falloff, best, t->size);
        }
      }
      bool touched = false;
      for (int i = 0; i < gps->totpoints; i++) {
        if (w[i] <= 0.0f) continue;
        pe9_apply(t, gps->points[i].x, gps->points[i].y, w[i], lcx, lcy, &gps->points[i].x, &gps->points[i].y);
        touched = true;
      }
      free(w);
      if (touched) { BKE_gpencil_stroke_geometry_update(gpd, gps); changed = 1; }
    }
  }
  return changed;
}

/* ---- dope sheet ---------------------------------------------------------------------- */
#define PE7_LAYERS(gpd, only, gpl) LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd)->layers) if (((only) == NULL || gpl == (only)) && BKE_gpencil_layer_is_editable(gpl))

int pg_gp_frames_select_range(bGPdata *gpd, bGPDlayer *only, int fmin, int fmax, int extend)
{
  if (gpd == NULL || fmax < fmin) return 0;
  int changed = 0;
  PE7_LAYERS (gpd, only, gpl) {
    LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames) {
      const int was = f->flag & GP_FRAME_SELECT;
      if (f->framenum >= fmin && f->framenum <= fmax) f->flag |= GP_FRAME_SELECT;
      else if (!extend) f->flag &= ~GP_FRAME_SELECT;
      if ((f->flag & GP_FRAME_SELECT) != was) changed = 1;
    }
  }
  return changed;
}

/* keep frames sorted after renumbering; Blender merges a moved frame onto an occupied number
 * by keeping the moved one (the old one is deleted) */
static void pe9_resolve(bGPDlayer *gpl)
{
  bool sorted = false;
  while (!sorted) {
    sorted = true;
    for (bGPDframe *f = gpl->frames.first; f && f->next; f = f->next) {
      bGPDframe *g = f->next;
      if (g->framenum < f->framenum) { BLI_remlink(&gpl->frames, g); g->prev = f->prev; g->next = f;
        if (f->prev) f->prev->next = g; else gpl->frames.first = g; f->prev = g; sorted = false; break; }
      if (g->framenum == f->framenum) { bGPDframe *loser = (f->flag & GP_FRAME_SELECT) ? g : f;
        if (gpl->actframe == loser) gpl->actframe = (loser == f) ? g : f;
        BKE_gpencil_layer_frame_delete(gpl, loser); sorted = false; break; }
    }
  }
}

int pg_gp_frames_move(bGPdata *gpd, bGPDlayer *only, int offset)
{
  if (gpd == NULL || offset == 0) return 0;
  int changed = 0;
  PE7_LAYERS (gpd, only, gpl) {
    bool any = false;
    LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames) if (f->flag & GP_FRAME_SELECT) {
      int nf = f->framenum + offset; f->framenum = nf < 0 ? 0 : nf; any = true; }
    if (any) { pe9_resolve(gpl); changed = 1; }
  }
  return changed;
}

int pg_gp_frames_scale(bGPdata *gpd, bGPDlayer *only, int center, float factor)
{
  if (gpd == NULL || !isfinite(factor) || factor <= 0.0f || factor == 1.0f) return 0;
  int changed = 0;
  PE7_LAYERS (gpd, only, gpl) {
    bool any = false;
    LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames) if (f->flag & GP_FRAME_SELECT) {
      int nf = (int)lroundf(center + (f->framenum - center) * factor); f->framenum = nf < 0 ? 0 : nf; any = true; }
    if (any) { pe9_resolve(gpl); changed = 1; }
  }
  return changed;
}

static ListBase pe9_clip = {NULL, NULL}; /* copies of frames (framenum relative to the first) */

void pg_gp_frames_clipboard_free(void)
{
  for (bGPDframe *f = pe9_clip.first, *n; f; f = n) {
    n = f->next;
    for (bGPDstroke *s = f->strokes.first, *sn; s; s = sn) { sn = s->next; BKE_gpencil_free_stroke(s); }
    MEM_freeN(f);
  }
  pe9_clip.first = pe9_clip.last = NULL;
}

int pg_gp_frames_copy(bGPdata *gpd, bGPDlayer *only)
{
  if (gpd == NULL) return 0;
  ListBase fresh = {NULL, NULL};
  int first = -1;
  PE7_LAYERS (gpd, only, gpl) {
    LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames) {
      if (!(f->flag & GP_FRAME_SELECT)) continue;
      if (first < 0 || f->framenum < first) first = f->framenum;
      bGPDframe *c = MEM_callocN(sizeof(bGPDframe), "pg_frame_clip");
      if (c == NULL) continue;
      c->framenum = f->framenum;
      LISTBASE_FOREACH (bGPDstroke *, s, &f->strokes) { bGPDstroke *d = BKE_gpencil_stroke_duplicate(s, true, true); if (d) BLI_addtail(&c->strokes, d); }
      BLI_addtail(&fresh, c);
    }
  }
  if (fresh.first == NULL) return 0;
  pg_gp_frames_clipboard_free();
  for (bGPDframe *f = fresh.first; f; f = f->next) f->framenum -= first;
  pe9_clip = fresh;
  return 0; /* copying changes nothing */
}

int pg_gp_frames_paste(bGPdata *gpd, bGPDlayer *target, int at)
{
  if (gpd == NULL || target == NULL || pe9_clip.first == NULL || at < 0 || !BKE_gpencil_layer_is_editable(target)) return 0;
  LISTBASE_FOREACH (bGPDframe *, f, &target->frames) f->flag &= ~GP_FRAME_SELECT;
  LISTBASE_FOREACH (bGPDframe *, c, &pe9_clip) {
    const int num = at + c->framenum;
    bGPDframe *dst = NULL;
    LISTBASE_FOREACH (bGPDframe *, f, &target->frames) if (f->framenum == num) dst = f;
    if (dst) { /* paste replaces the drawing of an existing frame (overwrite mode) */
      for (bGPDstroke *s = dst->strokes.first, *n; s; s = n) { n = s->next; BLI_remlink(&dst->strokes, s); BKE_gpencil_free_stroke(s); }
    }
    else dst = BKE_gpencil_frame_addnew(target, num);
    if (dst == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, s, &c->strokes) { bGPDstroke *d = BKE_gpencil_stroke_duplicate(s, true, true); if (d) BLI_addtail(&dst->strokes, d); }
    dst->flag |= GP_FRAME_SELECT;
  }
  return 1;
}

/* ---- dash with segments --------------------------------------------------------------- */
int pg_gp_dash_segments(bGPdata *gpd, const bGPDlayer *only, const int *dash, const int *gap, int nseg, int offset)
{
  if (gpd == NULL || dash == NULL || gap == NULL || nseg < 1 || nseg > 32) return 0;
  int period = 0;
  for (int k = 0; k < nseg; k++) { if (dash[k] < 1 || gap[k] < 0) return 0; period += dash[k] + gap[k]; }
  int changed = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!pe9_layer_ok(gpl, only)) continue;
    bGPDframe *gpf = gpl->actframe;
    for (bGPDstroke *gps = gpf->strokes.first, *next; gps; gps = next) {
      next = gps->next;
      if (!(gps->flag & GP_STROKE_SELECT) || gps->totpoints < 2 || gps->dvert != NULL) continue;
      bGPDstroke *after = gps; int made = 0;
      int i = 0;
      while (i < gps->totpoints) {
        /* position in the pattern: which segment and whether in its dash part */
        int ph = ((i + offset) % period + period) % period, k = 0;
        while (ph >= dash[k] + gap[k]) { ph -= dash[k] + gap[k]; k++; }
        if (ph >= dash[k]) { i++; continue; }
        int run = dash[k] - ph, b = i + run - 1;
        if (b >= gps->totpoints) b = gps->totpoints - 1;
        if (b > i) {
          bGPDstroke *d = BKE_gpencil_stroke_duplicate(gps, true, true);
          if (d) {
            const int cnt = b - i + 1;
            memmove(d->points, d->points + i, sizeof(bGPDspoint) * (size_t)cnt);
            d->totpoints = cnt; d->flag &= ~GP_STROKE_CYCLIC;
            d->prev = after; d->next = after->next;
            if (after->next) after->next->prev = d; else gpf->strokes.last = d;
            after->next = d; after = d; made++;
            BKE_gpencil_stroke_geometry_update(gpd, d);
          }
        }
        i = b + 1;
      }
      if (made) { BLI_remlink(&gpf->strokes, gps); BKE_gpencil_free_stroke(gps); changed = 1; }
    }
  }
  return changed;
}

/* ---- command dispatch (routed by the bridge, ids PG_EDIT9_CMD_FIRST..LAST) ------------- */
static float pe9_snap(float v, float inc) { return inc > 0.0f ? roundf(v / inc) * inc : v; }

static MaterialGPencilStyle *pe9_style(bGPdata *gpd, int slot)
{
  if (gpd == NULL || gpd->mat == NULL || slot < 0 || slot >= (int)gpd->totcol || gpd->mat[slot] == NULL) return NULL;
  return gpd->mat[slot]->gp_style;
}

int pg_gp_material_gradient_set(bGPdata *gpd, int slot, int enabled, int type, const float mix_rgba[4],
                                float mix_factor, float angle, const float scale[2], const float offset[2], int flip)
{
  MaterialGPencilStyle *st = pe9_style(gpd, slot);
  if (st == NULL || type < GP_MATERIAL_GRADIENT_LINEAR || type > GP_MATERIAL_GRADIENT_RADIAL) return 0;
  st->fill_style = enabled ? GP_MATERIAL_FILL_STYLE_GRADIENT : GP_MATERIAL_FILL_STYLE_SOLID;
  st->gradient_type = type;
  for (int c = 0; c < 4; c++) st->mix_rgba[c] = mix_rgba[c] < 0.0f ? 0.0f : (mix_rgba[c] > 1.0f ? 1.0f : mix_rgba[c]);
  st->mix_factor = mix_factor < 0.0f ? 0.0f : (mix_factor > 1.0f ? 1.0f : mix_factor);
  /* rna: texture_angle -2pi..2pi, texture_scale 0.01..100, texture_offset -100..100 */
  st->texture_angle = angle < (float)(-2.0 * M_PI) ? (float)(-2.0 * M_PI) : (angle > (float)(2.0 * M_PI) ? (float)(2.0 * M_PI) : angle);
  for (int k = 0; k < 2; k++) {
    st->texture_scale[k] = scale[k] < 0.01f ? 0.01f : (scale[k] > 100.0f ? 100.0f : scale[k]);
    st->texture_offset[k] = offset[k] < -100.0f ? -100.0f : (offset[k] > 100.0f ? 100.0f : offset[k]);
  }
  if (flip) st->flag |= GP_MATERIAL_FLIP_FILL; else st->flag &= ~GP_MATERIAL_FLIP_FILL;
  return 1;
}

int pg_gp_material_options_set(bGPdata *gpd, int slot, int stroke_holdout, int fill_holdout, int self_overlap)
{
  MaterialGPencilStyle *st = pe9_style(gpd, slot);
  if (st == NULL) return 0;
  const int before = st->flag;
  if (stroke_holdout) st->flag |= GP_MATERIAL_IS_STROKE_HOLDOUT; else st->flag &= ~GP_MATERIAL_IS_STROKE_HOLDOUT;
  if (fill_holdout) st->flag |= GP_MATERIAL_IS_FILL_HOLDOUT; else st->flag &= ~GP_MATERIAL_IS_FILL_HOLDOUT;
  if (self_overlap) st->flag |= GP_MATERIAL_DISABLE_STENCIL; else st->flag &= ~GP_MATERIAL_DISABLE_STENCIL;
  return st->flag != before;
}

int pg_gp_edit9_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args, int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) if (!isfinite(args[i])) return 0;
#define NEED(n) if (arg_count < (n)) return 0
#define I(k) (int)lroundf(args[k])
  /* the "all layers" flag selects every editable layer instead of the active one */
#define ONLY(k) (arg_count > (k) && args[k] != 0.0f ? NULL : active_layer)
  int changed = 0;
  switch (command) {
    case PG_EDIT9_CMD_TRANSFORM: {
      NEED(10);
      PGTransform t;
      t.type = I(0); t.a = args[1]; t.b = args[2]; t.pivot = I(3);
      t.cursor[0] = args[4]; t.cursor[1] = args[5];
      t.proportional = args[6] != 0.0f; t.connected = args[7] != 0.0f;
      t.falloff = I(8); t.size = args[9];
      if (t.pivot < PG_PIVOT_MEDIAN || t.pivot > PG_PIVOT_CURSOR) return 0;
      /* increment snapping (snap_increment): translate by grid steps, rotate by 5 degree steps */
      const float inc = arg_count > 10 ? args[10] : 0.0f;
      if (inc > 0.0f && t.type == PG_XFORM_TRANSLATE) { t.a = pe9_snap(t.a, inc); t.b = pe9_snap(t.b, inc); }
      else if (inc > 0.0f && t.type == PG_XFORM_ROTATE) t.a = pe9_snap(t.a, (float)(M_PI / 36.0));
      changed = pg_gp_transform(gpd, ONLY(11), &t);
      break;
    }
    case PG_EDIT9_CMD_FRAMES_SELECT_RANGE:
      NEED(2);
      changed = pg_gp_frames_select_range(gpd, ONLY(3), I(0), I(1), arg_count > 2 && args[2] != 0.0f);
      break;
    case PG_EDIT9_CMD_FRAMES_MOVE:
      NEED(1);
      changed = pg_gp_frames_move(gpd, ONLY(1), I(0));
      break;
    case PG_EDIT9_CMD_FRAMES_SCALE:
      NEED(2);
      changed = pg_gp_frames_scale(gpd, ONLY(2), I(0), args[1]);
      break;
    case PG_EDIT9_CMD_FRAMES_COPY:
      return pg_gp_frames_copy(gpd, ONLY(0));
    case PG_EDIT9_CMD_FRAMES_PASTE:
      NEED(1);
      changed = pg_gp_frames_paste(gpd, active_layer, I(0));
      break;
    case PG_EDIT9_CMD_MATERIAL_GRADIENT: {
      NEED(14);
      const float mix[4] = {args[3], args[4], args[5], args[6]};
      const float sc[2] = {args[9], args[10]}, of[2] = {args[11], args[12]};
      return pg_gp_material_gradient_set(gpd, I(0), args[1] != 0.0f, I(2), mix, args[7], args[8], sc, of, args[13] != 0.0f);
    }
    case PG_EDIT9_CMD_MATERIAL_OPTIONS:
      NEED(4);
      return pg_gp_material_options_set(gpd, I(0), args[1] != 0.0f, args[2] != 0.0f, args[3] != 0.0f);
    case PG_EDIT9_CMD_DASH_SEGMENTS: {
      NEED(2);
      const int n = I(1);
      if (n < 1 || n > 32 || arg_count < 2 + 2 * n) return 0;
      int dash[32], gap[32];
      for (int k = 0; k < n; k++) { dash[k] = I(2 + 2 * k); gap[k] = I(3 + 2 * k); }
      changed = pg_gp_dash_segments(gpd, active_layer, dash, gap, n, I(0));
      break;
    }
    default:
      return 0;
  }
#undef ONLY
#undef I
#undef NEED
  return changed;
}
