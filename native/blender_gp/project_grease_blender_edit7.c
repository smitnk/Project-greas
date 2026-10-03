/* SPDX-License-Identifier: GPL-2.0-or-later
 * Seventh batch (edit6 one-go): behaviour of the named Blender 3.6.23 operators (vertex groups: gpencil_data.c
 * vertex_group_*; layers: layer_merge / layer_isolate / lock_all / unlock_all), written for this
 * data model. */
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit7.h"

#define PE7_STROKES_BEGIN(gpd_, only_, gpl, gpf, gps) \
  { \
    const bool multi_ = ((gpd_)->flag & GP_DATA_STROKE_MULTIEDIT) != 0; \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd_)->layers) { \
      if (((only_) != NULL && gpl != (only_)) || !BKE_gpencil_layer_is_editable(gpl)) continue; \
      for (bGPDframe *gpf = multi_ ? (bGPDframe *)gpl->frames.first : gpl->actframe; gpf; gpf = gpf->next) { \
        if (!((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && multi_))) continue; \
        for (bGPDstroke *gps = (bGPDstroke *)gpf->strokes.first; gps; gps = gps->next) { \
          if (gps->points == NULL) continue;
#define PE7_STROKES_END \
        } \
        if (!multi_) break; \
      } \
    } \
  } \
  (void)0

static MDeformWeight *pe7_find(MDeformVert *dv, int def_nr)
{
  for (int i = 0; i < dv->totweight; i++) if ((int)dv->dw[i].def_nr == def_nr) return &dv->dw[i];
  return NULL;
}

static MDeformWeight *pe7_ensure(MDeformVert *dv, int def_nr)
{
  MDeformWeight *dw = pe7_find(dv, def_nr);
  if (dw) return dw;
  MDeformWeight *n = MEM_callocN(sizeof(MDeformWeight) * (size_t)(dv->totweight + 1), "pg_vg");
  if (n == NULL) return NULL;
  if (dv->dw) { memcpy(n, dv->dw, sizeof(MDeformWeight) * (size_t)dv->totweight); MEM_freeN(dv->dw); }
  dv->dw = n;
  dv->dw[dv->totweight].def_nr = (unsigned int)def_nr;
  dv->dw[dv->totweight].weight = 0.0f;
  return &dv->dw[dv->totweight++];
}

static void pe7_remove(MDeformVert *dv, int def_nr)
{
  for (int i = 0; i < dv->totweight; i++) {
    if ((int)dv->dw[i].def_nr == def_nr) {
      dv->dw[i] = dv->dw[dv->totweight - 1]; /* order of entries is not meaningful */
      dv->totweight--;
      if (dv->totweight == 0) { MEM_freeN(dv->dw); dv->dw = NULL; }
      return;
    }
  }
}

static bool pe7_dvert_ensure(bGPDstroke *gps)
{
  if (gps->dvert == NULL) gps->dvert = MEM_callocN(sizeof(MDeformVert) * (size_t)gps->totpoints, "gp_stroke_weights");
  return gps->dvert != NULL;
}

int pg_gp_vgroup_assign(bGPdata *gpd, const bGPDlayer *only, int def_nr, float weight)
{
  if (gpd == NULL || def_nr < 0 || !isfinite(weight)) return 0;
  weight = weight < 0 ? 0 : (weight > 1 ? 1 : weight);
  int changed = 0;
  PE7_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT)) continue;
    for (int i = 0; i < gps->totpoints; i++) {
      if (!(gps->points[i].flag & GP_SPOINT_SELECT)) continue;
      if (!pe7_dvert_ensure(gps)) break;
      MDeformWeight *dw = pe7_ensure(&gps->dvert[i], def_nr);
      if (dw) { dw->weight = weight; changed = 1; }
    }
  }
  PE7_STROKES_END;
  return changed;
}

int pg_gp_vgroup_remove(bGPdata *gpd, const bGPDlayer *only, int def_nr)
{
  if (gpd == NULL || def_nr < 0) return 0;
  int changed = 0;
  PE7_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->dvert == NULL) continue;
    for (int i = 0; i < gps->totpoints; i++) {
      if (!(gps->points[i].flag & GP_SPOINT_SELECT) || pe7_find(&gps->dvert[i], def_nr) == NULL) continue;
      pe7_remove(&gps->dvert[i], def_nr);
      changed = 1;
    }
  }
  PE7_STROKES_END;
  return changed;
}

int pg_gp_vgroup_select(bGPdata *gpd, const bGPDlayer *only, int def_nr, int select)
{
  if (gpd == NULL || def_nr < 0) return 0;
  int changed = 0;
  PE7_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (gps->dvert == NULL) continue;
    bool touched = false;
    for (int i = 0; i < gps->totpoints; i++) {
      if (pe7_find(&gps->dvert[i], def_nr) == NULL) continue;
      const int was = gps->points[i].flag & GP_SPOINT_SELECT;
      if (select) gps->points[i].flag |= GP_SPOINT_SELECT; else gps->points[i].flag &= ~GP_SPOINT_SELECT;
      if ((gps->points[i].flag & GP_SPOINT_SELECT) != was) touched = true;
    }
    if (touched) { BKE_gpencil_stroke_sync_selection(gpd, gps); changed = 1; }
  }
  PE7_STROKES_END;
  return changed;
}

int pg_gp_vgroup_invert(bGPdata *gpd, const bGPDlayer *only, int def_nr)
{
  if (gpd == NULL || def_nr < 0) return 0;
  int changed = 0;
  PE7_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || !pe7_dvert_ensure(gps)) continue;
    for (int i = 0; i < gps->totpoints; i++) {
      MDeformWeight *dw = pe7_find(&gps->dvert[i], def_nr);
      if (dw == NULL) { dw = pe7_ensure(&gps->dvert[i], def_nr); if (dw) dw->weight = 1.0f; } /* no weight = 0 -> 1 */
      else dw->weight = 1.0f - dw->weight;
      changed = 1;
    }
  }
  PE7_STROKES_END;
  return changed;
}

int pg_gp_vgroup_normalize(bGPdata *gpd, const bGPDlayer *only, int def_nr)
{
  if (gpd == NULL || def_nr < 0) return 0;
  int changed = 0;
  PE7_STROKES_BEGIN (gpd, only, gpl, gpf, gps) {
    if (!(gps->flag & GP_STROKE_SELECT) || gps->dvert == NULL) continue;
    float maxw = 0.0f;
    for (int i = 0; i < gps->totpoints; i++) {
      MDeformWeight *dw = pe7_find(&gps->dvert[i], def_nr);
      if (dw && dw->weight > maxw) maxw = dw->weight;
    }
    if (maxw <= 0.0f || maxw == 1.0f) continue;
    for (int i = 0; i < gps->totpoints; i++) {
      MDeformWeight *dw = pe7_find(&gps->dvert[i], def_nr);
      if (dw) dw->weight /= maxw;
    }
    changed = 1;
  }
  PE7_STROKES_END;
  return changed;
}

static bGPDframe *pe7_frame_at(bGPDlayer *gpl, int num)
{
  LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames) if (f->framenum == num) return f;
  return NULL;
}
static bGPDframe *pe7_frame_before(bGPDlayer *gpl, int num)
{
  bGPDframe *best = NULL;
  LISTBASE_FOREACH (bGPDframe *, f, &gpl->frames) if (f->framenum < num) best = f;
  return best;
}

int pg_gp_layer_merge_down(bGPdata *gpd, bGPDlayer *active, bGPDlayer **r_active)
{
  if (r_active) *r_active = active;
  if (gpd == NULL || active == NULL || active->prev == NULL) return 0; /* nothing below */
  bGPDlayer *below = active->prev;
  for (bGPDframe *f = active->frames.first; f; f = f->next) {
    bGPDframe *dst = pe7_frame_at(below, f->framenum);
    if (dst == NULL) {
      /* keep what the lower layer showed at this frame (its previous keyframe, held) */
      bGPDframe *held = pe7_frame_before(below, f->framenum);
      dst = BKE_gpencil_frame_addnew(below, f->framenum);
      if (dst == NULL) return 0;
      if (held) {
        LISTBASE_FOREACH (bGPDstroke *, s, &held->strokes) {
          bGPDstroke *c = BKE_gpencil_stroke_duplicate(s, true, true);
          if (c) BLI_addtail(&dst->strokes, c);
        }
      }
    }
    /* active layer draws on top: its strokes go after the lower layer's */
    while (f->strokes.first) {
      bGPDstroke *s = f->strokes.first;
      BLI_remlink(&f->strokes, s);
      BLI_addtail(&dst->strokes, s);
    }
  }
  if (below->actframe == NULL) below->actframe = below->frames.first;
  BKE_gpencil_layer_delete(gpd, active);
  if (r_active) *r_active = below;
  return 1;
}

int pg_gp_layer_isolate(bGPdata *gpd, bGPDlayer *active)
{
  if (gpd == NULL || active == NULL) return 0;
  /* if every other layer is already hidden, show them all; otherwise hide them */
  bool others_hidden = true;
  LISTBASE_FOREACH (bGPDlayer *, l, &gpd->layers) if (l != active && !(l->flag & GP_LAYER_HIDE)) others_hidden = false;
  int changed = 0;
  LISTBASE_FOREACH (bGPDlayer *, l, &gpd->layers) {
    if (l == active) { if (l->flag & GP_LAYER_HIDE) { l->flag &= ~GP_LAYER_HIDE; changed = 1; } continue; }
    const int was = l->flag;
    if (others_hidden) l->flag &= ~GP_LAYER_HIDE; else l->flag |= GP_LAYER_HIDE;
    if (l->flag != was) changed = 1;
  }
  return changed;
}

int pg_gp_layers_lock_all(bGPdata *gpd, int lock)
{
  if (gpd == NULL) return 0;
  int changed = 0;
  LISTBASE_FOREACH (bGPDlayer *, l, &gpd->layers) {
    const int was = l->flag;
    if (lock) l->flag |= GP_LAYER_LOCKED; else l->flag &= ~GP_LAYER_LOCKED;
    if (l->flag != was) changed = 1;
  }
  return changed;
}

int pg_gp_edit7_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args, int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) if (!isfinite(args[i])) return 0;
  int changed = 0;
#define NEED(n) if (arg_count < (n)) return 0
#define G (int)lroundf(args[0])
  switch (command) {
    case PG_EDIT7_CMD_VG_ASSIGN: NEED(2); changed = pg_gp_vgroup_assign(gpd, active_layer, G, args[1]); break;
    case PG_EDIT7_CMD_VG_REMOVE: NEED(1); changed = pg_gp_vgroup_remove(gpd, active_layer, G); break;
    case PG_EDIT7_CMD_VG_SELECT: NEED(1); changed = pg_gp_vgroup_select(gpd, active_layer, G, 1); break;
    case PG_EDIT7_CMD_VG_DESELECT: NEED(1); changed = pg_gp_vgroup_select(gpd, active_layer, G, 0); break;
    case PG_EDIT7_CMD_VG_INVERT: NEED(1); changed = pg_gp_vgroup_invert(gpd, active_layer, G); break;
    case PG_EDIT7_CMD_VG_NORMALIZE: NEED(1); changed = pg_gp_vgroup_normalize(gpd, active_layer, G); break;
    case PG_EDIT7_CMD_LAYER_MERGE: changed = pg_gp_layer_merge_down(gpd, active_layer, NULL); break;
    case PG_EDIT7_CMD_LAYER_ISOLATE: changed = pg_gp_layer_isolate(gpd, active_layer); break;
    case PG_EDIT7_CMD_LOCK_ALL: changed = pg_gp_layers_lock_all(gpd, 1); break;
    case PG_EDIT7_CMD_UNLOCK_ALL: changed = pg_gp_layers_lock_all(gpd, 0); break;
    default: return 0;
  }
#undef G
#undef NEED
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed;
}
