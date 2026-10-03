/* SPDX-License-Identifier: GPL-2.0-or-later
 * Sixth batch (edit5 one-go) (Project Grease adaptations of the named Blender 3.6.23 behaviour; see header). */
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit6.h"
#include "project_grease_blender_edit7.h"

static bool pe6_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  if (gpd->mat == NULL || gps->mat_nr < 0 || gps->mat_nr >= (int)gpd->totcol || gpd->mat[gps->mat_nr] == NULL ||
      gpd->mat[gps->mat_nr]->gp_style == NULL)
    return true;
  const MaterialGPencilStyle *st = gpd->mat[gps->mat_nr]->gp_style;
  if (st->flag & GP_MATERIAL_HIDE) return false;
  if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (st->flag & GP_MATERIAL_LOCKED)) return false;
  return true;
}

/* ---- Outline ------------------------------------------------------------------------- */
static void pe6_dir(const bGPDspoint *a, const bGPDspoint *b, float r[2])
{
  float dx = b->x - a->x, dy = b->y - a->y, l = sqrtf(dx * dx + dy * dy);
  if (l < 1e-8f) { r[0] = 1; r[1] = 0; return; }
  r[0] = dx / l; r[1] = dy / l;
}

int pg_gp_outline(bGPdata *gpd, const bGPDlayer *only, int outline_thickness, int cap_segments)
{
  if (gpd == NULL || outline_thickness < 1 || cap_segments < 1 || cap_segments > 64) return 0;
  int changed = 0;
  const bool multi = (gpd->flag & GP_DATA_STROKE_MULTIEDIT) != 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if ((only != NULL && gpl != only) || !BKE_gpencil_layer_is_editable(gpl)) continue;
    for (bGPDframe *gpf = multi ? gpl->frames.first : gpl->actframe; gpf; gpf = gpf->next) {
      if (!((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && multi))) continue;
      for (bGPDstroke *gps = gpf->strokes.first, *next; gps; gps = next) {
        next = gps->next;
        if (!(gps->flag & GP_STROKE_SELECT) || gps->totpoints < 2 || gps->dvert != NULL ||
            (gps->flag & GP_STROKE_CYCLIC) || !pe6_editable(gpd, gpl, gps))
          continue;
        const int n = gps->totpoints;
        const int total = 2 * n + 2 * (cap_segments - 1);
        bGPDspoint *out = MEM_callocN(sizeof(bGPDspoint) * (size_t)total, "pg_outline");
        if (out == NULL) continue;
        int k = 0;
        const bGPDspoint *p = gps->points;
#define PE6_RAD(i) (0.5f * (float)gps->thickness * p[i].pressure)
        /* left side, forward */
        for (int i = 0; i < n; i++) {
          float d[2];
          pe6_dir(&p[i > 0 ? i - 1 : 0], &p[i + 1 < n ? i + 1 : n - 1], d);
          out[k] = p[i];
          out[k].x = p[i].x - d[1] * PE6_RAD(i);
          out[k].y = p[i].y + d[0] * PE6_RAD(i);
          k++;
        }
        /* round cap at the end, then right side backward, then round cap at the start */
        for (int cap = 0; cap < 2; cap++) {
          const int i = cap == 0 ? n - 1 : 0;
          float d[2];
          if (cap == 0) pe6_dir(&p[n - 2], &p[n - 1], d); else pe6_dir(&p[1], &p[0], d);
          const float a0 = atan2f(d[0], -d[1]); /* angle of the left normal */
          for (int s = 1; s < cap_segments; s++) {
            const float a = a0 - (float)M_PI * (float)s / (float)cap_segments;
            out[k] = p[i];
            out[k].x = p[i].x + cosf(a) * PE6_RAD(i);
            out[k].y = p[i].y + sinf(a) * PE6_RAD(i);
            k++;
          }
          if (cap == 0) {
            for (int j = n - 1; j >= 0; j--) {
              float dd[2];
              pe6_dir(&p[j > 0 ? j - 1 : 0], &p[j + 1 < n ? j + 1 : n - 1], dd);
              out[k] = p[j];
              out[k].x = p[j].x + dd[1] * PE6_RAD(j);
              out[k].y = p[j].y - dd[0] * PE6_RAD(j);
              k++;
            }
          }
        }
#undef PE6_RAD
        for (int i = 0; i < k; i++) { out[i].pressure = 1.0f; out[i].flag &= ~GP_SPOINT_SELECT; }
        MEM_freeN(gps->points);
        gps->points = out;
        gps->totpoints = k;
        gps->thickness = (short)outline_thickness;
        gps->flag |= GP_STROKE_CYCLIC;
        gps->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_geometry_update(gpd, gps);
        changed = 1;
      }
      if (!multi) break;
    }
  }
  return changed;
}

/* ---- interpolation of unequal strokes --------------------------------------------- */
bGPDstroke *pg_gp_interpolate_strokes(bGPdata *gpd, const bGPDstroke *from, const bGPDstroke *to, float t)
{
  if (gpd == NULL || from == NULL || to == NULL || from->totpoints < 1 || to->totpoints < 1 || !isfinite(t))
    return NULL;
  bGPDstroke *a = BKE_gpencil_stroke_duplicate((bGPDstroke *)from, true, true);
  bGPDstroke *b = BKE_gpencil_stroke_duplicate((bGPDstroke *)to, true, true);
  if (a == NULL || b == NULL) {
    if (a) BKE_gpencil_free_stroke(a);
    if (b) BKE_gpencil_free_stroke(b);
    return NULL;
  }
  /* gpencil_interpolate.c: match the point counts first */
  if (a->totpoints > b->totpoints) BKE_gpencil_stroke_uniform_subdivide(gpd, b, (unsigned int)a->totpoints, false);
  else if (b->totpoints > a->totpoints) BKE_gpencil_stroke_uniform_subdivide(gpd, a, (unsigned int)b->totpoints, false);
  const int n = a->totpoints < b->totpoints ? a->totpoints : b->totpoints;
  for (int i = 0; i < n; i++) {
    bGPDspoint *p = &a->points[i];
    const bGPDspoint *q = &b->points[i];
    p->x += (q->x - p->x) * t;
    p->y += (q->y - p->y) * t;
    p->z += (q->z - p->z) * t;
    p->pressure += (q->pressure - p->pressure) * t;
    p->strength += (q->strength - p->strength) * t;
  }
  a->totpoints = n;
  a->thickness = (short)lroundf((float)from->thickness + ((float)to->thickness - (float)from->thickness) * t);
  BKE_gpencil_free_stroke(b);
  BKE_gpencil_stroke_geometry_update(gpd, a);
  return a;
}

/* ---- onion skin ghosts ------------------------------------------------------------- */
static float pe6_alpha(int use_fade, float factor, int distance)
{
  float a = factor;
  if (use_fade) a = factor / (float)(distance < 1 ? 1 : distance);
  return a < 0.1f ? 0.1f : (a > 1.0f ? 1.0f : a);
}

int pg_onion_ghosts(int mode, const int *keys, const unsigned char *selected, int nkeys, int current,
                    int before, int after, int use_fade, float factor, int *out_frames, float *out_alpha, int max_out)
{
  if (keys == NULL || nkeys <= 0 || out_frames == NULL || out_alpha == NULL || max_out <= 0) return 0;
  int n = 0;
  /* index of the current keyframe: last key <= current */
  int cur = -1;
  for (int i = 0; i < nkeys; i++) if (keys[i] <= current) cur = i;
  for (int i = 0; i < nkeys && n < max_out; i++) {
    if (cur >= 0 && i == cur) continue; /* the frame on screen is not a ghost */
    int take = 0, distance = 0;
    switch (mode) {
      case PG_ONION_RELATIVE: /* N keyframes before / after */
        distance = i - (cur < 0 ? -1 : cur);
        if (distance < 0) take = -distance <= before;
        else take = distance <= after && distance > 0;
        distance = abs(distance);
        break;
      case PG_ONION_ABSOLUTE: /* keyframes within N frames */
        distance = keys[i] - current;
        take = (distance < 0) ? (-distance <= before) : (distance > 0 && distance <= after);
        distance = abs(distance);
        break;
      case PG_ONION_SELECTED:
        take = selected != NULL && selected[i];
        distance = abs(i - (cur < 0 ? 0 : cur));
        break;
      default: return 0;
    }
    if (!take) continue;
    out_frames[n] = keys[i];
    out_alpha[n] = pe6_alpha(use_fade, factor, distance);
    n++;
  }
  return n;
}

/* ---- fill extend lines -------------------------------------------------------------- */
int pg_fill_extend_segments(const bGPDstroke *gps, float extend_fac, float out[8])
{
  if (gps == NULL || out == NULL || gps->totpoints < 2 || (gps->flag & GP_STROKE_CYCLIC) ||
      !isfinite(extend_fac) || extend_fac <= 0.0f)
    return 0;
  const bGPDspoint *p = gps->points;
  const int n = gps->totpoints;
  float len = 0.0f;
  for (int i = 1; i < n; i++) len += hypotf(p[i].x - p[i - 1].x, p[i].y - p[i - 1].y);
  if (len < 1e-6f) return 0;
  const float ext = len * extend_fac;
  float d[2];
  pe6_dir(&p[1], &p[0], d); /* outward at the start */
  out[0] = p[0].x; out[1] = p[0].y; out[2] = p[0].x + d[0] * ext; out[3] = p[0].y + d[1] * ext;
  pe6_dir(&p[n - 2], &p[n - 1], d); /* outward at the end */
  out[4] = p[n - 1].x; out[5] = p[n - 1].y; out[6] = p[n - 1].x + d[0] * ext; out[7] = p[n - 1].y + d[1] * ext;
  return 1;
}

int pg_gp_material_texture_set(bGPdata *gpd, int index, int fill, int enabled, float mix,
                               const float scale[2], const float offset[2], float angle, float pixsize)
{
  if (gpd == NULL || gpd->mat == NULL || index < 0 || index >= (int)gpd->totcol || gpd->mat[index] == NULL ||
      gpd->mat[index]->gp_style == NULL)
    return 0;
  MaterialGPencilStyle *st = gpd->mat[index]->gp_style;
  const float m = mix < 0.0f ? 0.0f : (mix > 1.0f ? 1.0f : mix);
  if (fill) {
    st->fill_style = enabled ? GP_MATERIAL_FILL_STYLE_TEXTURE : GP_MATERIAL_FILL_STYLE_SOLID;
    st->mix_factor = m;
    st->texture_scale[0] = scale[0]; st->texture_scale[1] = scale[1];
    st->texture_offset[0] = offset[0]; st->texture_offset[1] = offset[1];
    st->texture_angle = angle;
  }
  else {
    st->stroke_style = enabled ? GP_MATERIAL_STROKE_STYLE_TEXTURE : GP_MATERIAL_STROKE_STYLE_SOLID;
    st->mix_stroke_factor = m;
    st->texture_pixsize = pixsize > 0.0f ? pixsize : 100.0f; /* Blender default 100 */
  }
  return 1;
}

int pg_gp_edit6_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args, int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) if (!isfinite(args[i])) return 0;
  int changed = 0;
  switch (command) {
    case PG_EDIT6_CMD_OUTLINE:
      if (arg_count < 2) return 0;
      changed = pg_gp_outline(gpd, active_layer, (int)lroundf(args[0]), (int)lroundf(args[1]));
      break;
    case PG_EDIT6_CMD_ONION_STYLE: {
      /* bGPdata.onion_mode and the ghost colours with their GP_ONION_GHOST_PREVCOL / NEXTCOL
       * switches (Onion Skinning panel: Mode, "Use Custom Colors" before / after). */
      if (arg_count < 9) return 0;
      const int mode = (int)lroundf(args[0]);
      if (mode < GP_ONION_MODE_ABSOLUTE || mode > GP_ONION_MODE_SELECTED) return 0;
      gpd->onion_mode = mode;
      if (args[1] != 0.0f) gpd->onion_flag |= GP_ONION_GHOST_PREVCOL;
      else gpd->onion_flag &= ~GP_ONION_GHOST_PREVCOL;
      if (args[2] != 0.0f) gpd->onion_flag |= GP_ONION_GHOST_NEXTCOL;
      else gpd->onion_flag &= ~GP_ONION_GHOST_NEXTCOL;
      for (int i = 0; i < 3; i++) {
        gpd->gcolor_prev[i] = args[3 + i] < 0.0f ? 0.0f : (args[3 + i] > 1.0f ? 1.0f : args[3 + i]);
        gpd->gcolor_next[i] = args[6 + i] < 0.0f ? 0.0f : (args[6 + i] > 1.0f ? 1.0f : args[6 + i]);
      }
      changed = 1;
      break;
    }
    case PG_EDIT6_CMD_MATERIAL_TEXTURE: {
      if (arg_count < 10) return 0;
      const float scale[2] = {args[4], args[5]}, offset[2] = {args[6], args[7]};
      if (scale[0] == 0.0f || scale[1] == 0.0f) return 0;
      changed = pg_gp_material_texture_set(gpd, (int)lroundf(args[0]), args[1] != 0.0f, args[2] != 0.0f, args[3],
                                           scale, offset, args[8], args[9]);
      break;
    }
    default: return pg_gp_edit7_dispatch(gpd, active_layer, command, args, arg_count);
  }
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed;
}
