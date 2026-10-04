/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Annotation data (see project_grease_annotations.h). Written for this data model after
 * Blender 3.6.23 annotate_paint.c; it owns its frames/strokes with MEM_callocN/MEM_freeN and only
 * relies on the Legacy GP DNA layout.
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"

#include "project_grease_annotations.h"

/* Private stroke flag: the stroke pg_annot_begin() opened. bGPDstroke.flag is a short and Blender's
 * eGPDstroke_Flag uses bits 0-3 and 7-15, so bit 5 is free. (It was 1 << 30, which the short
 * truncated to 0: the stroke was never open, every point of a drag was dropped and the annotation
 * stayed a zero-point stroke that drew nothing.) */
#define PG_ANNOT_STROKE_OPEN (1 << 5)
_Static_assert(PG_ANNOT_STROKE_OPEN <= 0x7FFF, "the open flag must fit bGPDstroke.flag (short)");
#define PG_ANNOT_POINT_CHUNK 64

static void pa_link_append(ListBase *list, void *vlink)
{
  Link *link = (Link *)vlink;
  link->next = NULL;
  link->prev = (Link *)list->last;
  if (list->last) ((Link *)list->last)->next = link;
  else list->first = link;
  list->last = link;
}

static void pa_link_insert_before(ListBase *list, void *vnext, void *vlink)
{
  Link *next = (Link *)vnext, *link = (Link *)vlink;
  if (next == NULL) { pa_link_append(list, link); return; }
  link->next = next;
  link->prev = next->prev;
  if (next->prev) next->prev->next = link;
  else list->first = link;
  next->prev = link;
}

static bGPDlayer *pa_layer(const bGPdata *annot)
{
  return annot ? (bGPDlayer *)annot->layers.first : NULL;
}

static void pa_stroke_free(bGPDstroke *gps)
{
  if (gps->points) MEM_freeN(gps->points);
  MEM_freeN(gps);
}

static void pa_frame_free(bGPDframe *gpf)
{
  for (bGPDstroke *gps = (bGPDstroke *)gpf->strokes.first, *next; gps; gps = next) {
    next = gps->next;
    pa_stroke_free(gps);
  }
  MEM_freeN(gpf);
}

static void pa_frames_free(bGPDlayer *gpl)
{
  for (bGPDframe *gpf = (bGPDframe *)gpl->frames.first, *next; gpf; gpf = next) {
    next = gpf->next;
    pa_frame_free(gpf);
  }
  gpl->frames.first = gpl->frames.last = NULL;
  gpl->actframe = NULL;
}

/* BKE_gpencil_layer_frame_get(..., GP_GETFRAME_ADD_NEW) for the annotation layer: the frame at
 * `framenum`, inserted in order when missing. */
static bGPDframe *pa_frame_ensure(bGPDlayer *gpl, int framenum)
{
  bGPDframe *at = (bGPDframe *)gpl->frames.first;
  while (at && at->framenum < framenum) at = at->next;
  if (at && at->framenum == framenum) return at;
  bGPDframe *gpf = (bGPDframe *)MEM_callocN(sizeof(bGPDframe), "pg annotation frame");
  if (!gpf) return NULL;
  gpf->framenum = framenum;
  pa_link_insert_before(&gpl->frames, at, gpf);
  return gpf;
}

static bGPDstroke *pa_stroke_new(int capacity, short thickness)
{
  bGPDstroke *gps = (bGPDstroke *)MEM_callocN(sizeof(bGPDstroke), "pg annotation stroke");
  if (!gps) return NULL;
  if (capacity < 1) capacity = 1;
  gps->points = (bGPDspoint *)MEM_callocN(sizeof(bGPDspoint) * (size_t)capacity, "pg annotation points");
  if (!gps->points) { MEM_freeN(gps); return NULL; }
  gps->thickness = thickness;
  return gps;
}

static void pa_point_set(bGPDspoint *pt, float x, float y)
{
  memset(pt, 0, sizeof(*pt));
  pt->x = x;
  pt->y = y;
  pt->pressure = 1.0f;
  pt->strength = 1.0f;
}

bGPdata *pg_annot_create(void)
{
  bGPdata *annot = (bGPdata *)MEM_callocN(sizeof(bGPdata), "pg annotations");
  if (!annot) return NULL;
  bGPDlayer *gpl = (bGPDlayer *)MEM_callocN(sizeof(bGPDlayer), "pg annotation layer");
  if (!gpl) { MEM_freeN(annot); return NULL; }
  strncpy(gpl->info, "Note", sizeof(gpl->info) - 1);
  gpl->opacity = 1.0f;
  /* annotation layer defaults (BKE_gpencil_layer_addnew for annotations: thickness 3) */
  gpl->color[0] = 0.0f;
  gpl->color[1] = 0.6f;
  gpl->color[2] = 1.0f;
  gpl->color[3] = 1.0f;
  gpl->thickness = 3;
  pa_link_append(&annot->layers, gpl);
  return annot;
}

void pg_annot_free(bGPdata *annot)
{
  if (!annot) return;
  for (bGPDlayer *gpl = (bGPDlayer *)annot->layers.first, *next; gpl; gpl = next) {
    next = gpl->next;
    pa_frames_free(gpl);
    MEM_freeN(gpl);
  }
  MEM_freeN(annot);
}

void pg_annot_set_style(bGPdata *annot, const float rgba[4], float thickness_px)
{
  bGPDlayer *gpl = pa_layer(annot);
  if (!gpl) return;
  if (rgba) {
    for (int i = 0; i < 4; i++) {
      const float v = rgba[i];
      gpl->color[i] = isfinite(v) ? (v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)) : gpl->color[i];
    }
  }
  if (isfinite(thickness_px)) {
    /* annotation thickness range (rna_gpencil_legacy.c: 1..10 px; the touch UI allows up to 20) */
    const float t = thickness_px < 1.0f ? 1.0f : (thickness_px > 20.0f ? 20.0f : thickness_px);
    gpl->thickness = (short)lroundf(t);
  }
}

int pg_annot_get_style(const bGPdata *annot, float out_rgba[4], float *out_thickness)
{
  const bGPDlayer *gpl = pa_layer(annot);
  if (!gpl) return 0;
  if (out_rgba) memcpy(out_rgba, gpl->color, sizeof(float) * 4);
  if (out_thickness) *out_thickness = (float)gpl->thickness;
  return 1;
}

static bGPDstroke *pa_open_stroke(const bGPdata *annot)
{
  const bGPDlayer *gpl = pa_layer(annot);
  if (!gpl || !gpl->actframe) return NULL;
  bGPDstroke *gps = (bGPDstroke *)gpl->actframe->strokes.last;
  return (gps && (gps->flag & PG_ANNOT_STROKE_OPEN)) ? gps : NULL;
}

int pg_annot_begin(bGPdata *annot, int frame)
{
  bGPDlayer *gpl = pa_layer(annot);
  if (!gpl || frame < 0) return 0;
  pg_annot_cancel(annot);
  bGPDframe *gpf = pa_frame_ensure(gpl, frame);
  if (!gpf) return 0;
  bGPDstroke *gps = pa_stroke_new(PG_ANNOT_POINT_CHUNK, gpl->thickness);
  if (!gps) return 0;
  gps->flag = PG_ANNOT_STROKE_OPEN;
  pa_link_append(&gpf->strokes, gps);
  gpl->actframe = gpf;
  return 1;
}

int pg_annot_add_point(bGPdata *annot, float x, float y)
{
  bGPDstroke *gps = pa_open_stroke(annot);
  if (!gps || !isfinite(x) || !isfinite(y)) return 0;
  if (gps->totpoints > 0 && gps->totpoints % PG_ANNOT_POINT_CHUNK == 0) {
    bGPDspoint *grown = (bGPDspoint *)MEM_callocN(
        sizeof(bGPDspoint) * (size_t)(gps->totpoints + PG_ANNOT_POINT_CHUNK), "pg annotation points");
    if (!grown) return 0;
    memcpy(grown, gps->points, sizeof(bGPDspoint) * (size_t)gps->totpoints);
    MEM_freeN(gps->points);
    gps->points = grown;
  }
  pa_point_set(&gps->points[gps->totpoints++], x, y);
  return 1;
}

static void pa_remove_empty_frame(bGPDlayer *gpl, bGPDframe *gpf)
{
  if (gpf && gpf->strokes.first == NULL) {
    if (gpl->actframe == gpf) gpl->actframe = NULL;
    BLI_remlink(&gpl->frames, gpf);
    pa_frame_free(gpf);
  }
}

int pg_annot_end(bGPdata *annot)
{
  bGPDstroke *gps = pa_open_stroke(annot);
  if (!gps) return 0;
  if (gps->totpoints < 1) { pg_annot_cancel(annot); return 0; }
  gps->flag &= ~PG_ANNOT_STROKE_OPEN;
  return 1;
}

void pg_annot_cancel(bGPdata *annot)
{
  bGPDlayer *gpl = pa_layer(annot);
  bGPDstroke *gps = pa_open_stroke(annot);
  if (!gps) return;
  BLI_remlink(&gpl->actframe->strokes, gps);
  pa_stroke_free(gps);
  /* A frame that only existed for this stroke goes too, so cancel leaves no trace. */
  pa_remove_empty_frame(gpl, gpl->actframe);
}

const bGPDframe *pg_annot_frame_at(const bGPdata *annot, int frame)
{
  const bGPDlayer *gpl = pa_layer(annot);
  if (!gpl) return NULL;
  const bGPDframe *shown = NULL;
  for (const bGPDframe *gpf = (const bGPDframe *)gpl->frames.first; gpf && gpf->framenum <= frame; gpf = gpf->next) {
    shown = gpf;
  }
  return shown;
}

int pg_annot_erase(bGPdata *annot, int frame, float x, float y, float radius)
{
  bGPDlayer *gpl = pa_layer(annot);
  bGPDframe *gpf = (bGPDframe *)pg_annot_frame_at(annot, frame);
  if (!gpl || !gpf || !(radius > 0.0f) || !isfinite(x) || !isfinite(y)) return 0;
  const float r2 = radius * radius;
  int changed = 0;
  for (bGPDstroke *gps = (bGPDstroke *)gpf->strokes.first, *next; gps; gps = next) {
    next = gps->next;
    if (gps->flag & PG_ANNOT_STROKE_OPEN) continue;
    int hit = 0;
    for (int i = 0; i < gps->totpoints; i++) {
      const float dx = gps->points[i].x - x, dy = gps->points[i].y - y;
      if (dx * dx + dy * dy <= r2) { hit = 1; break; }
    }
    if (!hit) continue;
    changed = 1;
    /* Split into the runs of points outside the radius; runs of two or more points survive. */
    int i = 0;
    while (i < gps->totpoints) {
      while (i < gps->totpoints) {
        const float dx = gps->points[i].x - x, dy = gps->points[i].y - y;
        if (dx * dx + dy * dy > r2) break;
        i++;
      }
      const int start = i;
      while (i < gps->totpoints) {
        const float dx = gps->points[i].x - x, dy = gps->points[i].y - y;
        if (dx * dx + dy * dy <= r2) break;
        i++;
      }
      const int count = i - start;
      if (count >= 2) {
        bGPDstroke *part = pa_stroke_new(count, gps->thickness);
        if (part) {
          memcpy(part->points, &gps->points[start], sizeof(bGPDspoint) * (size_t)count);
          part->totpoints = count;
          pa_link_insert_before(&gpf->strokes, gps, part);
        }
      }
    }
    BLI_remlink(&gpf->strokes, gps);
    pa_stroke_free(gps);
  }
  return changed;
}

int pg_annot_clear(bGPdata *annot)
{
  bGPDlayer *gpl = pa_layer(annot);
  if (!gpl || gpl->frames.first == NULL) return 0;
  pa_frames_free(gpl);
  return 1;
}

int pg_annot_stroke_count(const bGPdata *annot)
{
  const bGPDlayer *gpl = pa_layer(annot);
  int n = 0;
  if (!gpl) return 0;
  for (const bGPDframe *gpf = (const bGPDframe *)gpl->frames.first; gpf; gpf = gpf->next) {
    for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) {
      if (!(gps->flag & PG_ANNOT_STROKE_OPEN)) n++;
    }
  }
  return n;
}

int pg_annot_dump(const bGPdata *annot, float *out, int capacity)
{
  const bGPDlayer *gpl = pa_layer(annot);
  if (!gpl) return 0;
  int need = 1, frames = 0;
  for (const bGPDframe *gpf = (const bGPDframe *)gpl->frames.first; gpf; gpf = gpf->next) {
    frames++;
    need += 2;
    for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) {
      if (!(gps->flag & PG_ANNOT_STROKE_OPEN)) need += 1 + gps->totpoints * 2;
    }
  }
  if (!out || capacity < need) return need;
  int k = 0;
  out[k++] = (float)frames;
  for (const bGPDframe *gpf = (const bGPDframe *)gpl->frames.first; gpf; gpf = gpf->next) {
    out[k++] = (float)gpf->framenum;
    const int count_at = k++;
    int strokes = 0;
    for (const bGPDstroke *gps = (const bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) {
      if (gps->flag & PG_ANNOT_STROKE_OPEN) continue;
      strokes++;
      out[k++] = (float)gps->totpoints;
      for (int i = 0; i < gps->totpoints; i++) {
        out[k++] = gps->points[i].x;
        out[k++] = gps->points[i].y;
      }
    }
    out[count_at] = (float)strokes;
  }
  return need;
}

/* Walks a dump; with gpl == NULL it only validates. */
static int pa_load_walk(bGPDlayer *gpl, const float *data, int count)
{
  int k = 0;
  if (count < 1 || !isfinite(data[0]) || data[0] < 0.0f) return 0;
  const int frames = (int)data[k++];
  for (int f = 0; f < frames; f++) {
    if (k + 2 > count) return 0;
    const float framenum = data[k++], strokes_f = data[k++];
    if (!isfinite(framenum) || framenum < 0.0f || !isfinite(strokes_f) || strokes_f < 0.0f) return 0;
    bGPDframe *gpf = gpl ? pa_frame_ensure(gpl, (int)framenum) : NULL;
    if (gpl && !gpf) return 0;
    const int strokes = (int)strokes_f;
    for (int s = 0; s < strokes; s++) {
      if (k + 1 > count || !isfinite(data[k]) || data[k] < 0.0f) return 0;
      const int points = (int)data[k++];
      if (points > (count - k) / 2) return 0;
      if (gpl && points > 0) {
        bGPDstroke *gps = pa_stroke_new(points, gpl->thickness);
        if (!gps) return 0;
        for (int i = 0; i < points; i++) pa_point_set(&gps->points[i], data[k + i * 2], data[k + i * 2 + 1]);
        gps->totpoints = points;
        pa_link_append(&gpf->strokes, gps);
      }
      k += points * 2;
    }
  }
  return k == count;
}

int pg_annot_load(bGPdata *annot, const float *data, int count)
{
  bGPDlayer *gpl = pa_layer(annot);
  if (!gpl || !data || !pa_load_walk(NULL, data, count)) return 0;
  pa_frames_free(gpl);
  return pa_load_walk(gpl, data, count);
}
