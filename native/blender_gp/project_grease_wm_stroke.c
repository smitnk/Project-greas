/* SPDX-License-Identifier: GPL-2.0-or-later
 * Stroke and material operators of gpencil_edit.c / gpencil_select.c / gpencil_data.c (Blender
 * 3.6.23) on the operator layer (project_grease_wm.h). Stroke bodies are the ported functions
 * the app already used; material bodies follow gpencil_data.c with bGPdata::mat[] for the
 * object's slots and PGOpContext::actcol for ob->actcol. */
#include <math.h>
#include <string.h>

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "MEM_guardedalloc.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit2.h"
#include "project_grease_blender_edit3.h"
#include "project_grease_blender_edit4.h"
#include "project_grease_blender_edit6.h"
#include "project_grease_blender_edit7.h"
#include "project_grease_blender_select.h"
#include "project_grease_blender_interp.h"
#include "project_grease_blender_edit10.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include <stdlib.h>
#include "project_grease_wm.h"

static int prop_i(const float *props, int n, int i, int def)
{
  return (i < n && isfinite(props[i])) ? (int)lroundf(props[i]) : def;
}
static float prop_f(const float *props, int n, int i, float def)
{
  return (i < n && isfinite(props[i])) ? props[i] : def;
}

/* gpencil_stroke_edit_poll */
static bool poll_stroke_edit(const PGOpContext *C)
{
  if (C->gpd == NULL) return false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (BKE_gpencil_layer_is_editable(gpl) && gpl->actframe && !BLI_listbase_is_empty(&gpl->actframe->strokes))
      return true;
  }
  return false;
}
static bool poll_gpd(const PGOpContext *C) { return C->gpd != NULL; }
static bool poll_active_layer(const PGOpContext *C) { return C->gpd && BKE_gpencil_layer_active_get(C->gpd); }
/* gpencil_active_material_poll: the object has material slots */
static bool poll_material(const PGOpContext *C) { return C->gpd && C->gpd->totcol > 0; }

static MaterialGPencilStyle *slot_style(const bGPdata *gpd, int slot /* 1-based */)
{
  if (slot < 1 || slot > gpd->totcol || gpd->mat == NULL || gpd->mat[slot - 1] == NULL) return NULL;
  return gpd->mat[slot - 1]->gp_style;
}

/* ---- rewired stroke operators: each finishes on valid props, changed or not ---------------- */
static int exec_flip(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  pg_gp_stroke_flip(C->gpd, NULL);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_cyclical_set: type CLOSE 1 / OPEN 2 / TOGGLE 3 (default) */
static int exec_cyclical(PGOpContext *C, const float *p, int n)
{
  const int type = prop_i(p, n, 0, PG_CYCLIC_TOGGLE);
  if (type < PG_CYCLIC_CLOSE || type > PG_CYCLIC_TOGGLE) return PG_OPERATOR_CANCELLED;
  pg_gp_stroke_cyclical_set(C->gpd, NULL, type);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_caps_set: type BOTH 0 / START 1 / END 2 / DEFAULT 3 */
static int exec_caps(PGOpContext *C, const float *p, int n)
{
  const int type = prop_i(p, n, 0, PG_CAPS_TOGGLE_BOTH);
  if (type < PG_CAPS_TOGGLE_BOTH || type > PG_CAPS_DEFAULT) return PG_OPERATOR_CANCELLED;
  pg_gp_caps_set(C->gpd, NULL, type);
  return PG_OPERATOR_FINISHED;
}
static int exec_start_set(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  pg_gp_start_set(C->gpd, NULL);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_merge_by_distance: threshold (0.001), use_unselected (false) */
static int exec_merge_distance(PGOpContext *C, const float *p, int n)
{
  const float t = prop_f(p, n, 0, 0.001f);
  if (t <= 0.0f) return PG_OPERATOR_CANCELLED;
  pg_gp_merge_distance(C->gpd, NULL, t, prop_i(p, n, 1, 0));
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_normalize: mode THICKNESS 0 / OPACITY 1, value */
static int exec_normalize(PGOpContext *C, const float *p, int n)
{
  const int mode = prop_i(p, n, 0, 0);
  if (mode != 0 && mode != 1) return PG_OPERATOR_CANCELLED;
  pg_gp_stroke_normalize(C->gpd, NULL, mode, prop_f(p, n, 1, mode == 0 ? 10.0f : 1.0f));
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_simplify_fixed: step (1, 1..100) */
static int exec_simplify_fixed(PGOpContext *C, const float *p, int n)
{
  const int steps = prop_i(p, n, 0, 1);
  if (steps < 1 || steps > 100) return PG_OPERATOR_CANCELLED;
  pg_gp_stroke_simplify_fixed(C->gpd, NULL, steps);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_sample: length (0.1), sharp_threshold (0.1) */
static int exec_sample(PGOpContext *C, const float *p, int n)
{
  const float len = prop_f(p, n, 0, 0.1f);
  if (len <= 0.0f) return PG_OPERATOR_CANCELLED;
  pg_gp_stroke_sample(C->gpd, NULL, len, prop_f(p, n, 1, 0.1f));
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_select_random: ratio (0.5), seed (0), action SELECT 1 / DESELECT 0 */
static int exec_select_random(PGOpContext *C, const float *p, int n)
{
  const float ratio = prop_f(p, n, 0, 0.5f);
  if (ratio < 0.0f || ratio > 1.0f) return PG_OPERATOR_CANCELLED;
  pg_gp_select_random(C->gpd, NULL, ratio, (unsigned int)prop_i(p, n, 1, 0), prop_i(p, n, 2, 1));
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_select_first / _last: only_selected_strokes (false), extend (false) */
static int exec_select_first(PGOpContext *C, const float *p, int n)
{
  pg_gp_select_first(C->gpd, prop_i(p, n, 0, 0), prop_i(p, n, 1, 0), NULL);
  return PG_OPERATOR_FINISHED;
}
static int exec_select_last(PGOpContext *C, const float *p, int n)
{
  pg_gp_select_last(C->gpd, prop_i(p, n, 0, 0), prop_i(p, n, 1, 0), NULL);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_change_color with no material name: selected strokes take the active material */
static int exec_change_color(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  if (slot_style(C->gpd, C->actcol) == NULL) return PG_OPERATOR_CANCELLED;
  pg_gp_stroke_set_material(C->gpd, NULL, C->actcol - 1);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_outline: thickness (1), subdivisions (3).
 * shortcut: view_mode / material_mode / keep / length are not ported; the outline is in canvas view */
static int exec_outline(PGOpContext *C, const float *p, int n)
{
  const int thickness = prop_i(p, n, 0, 1), subdiv = prop_i(p, n, 1, 3);
  if (thickness < 1 || subdiv < 1 || subdiv > 64) return PG_OPERATOR_CANCELLED;
  pg_gp_outline(C->gpd, NULL, thickness, subdiv);
  return PG_OPERATOR_FINISHED;
}

/* ---- material operators (gpencil_data.c) ------------------------------------------------ */
static void set_all_material_flag(bGPdata *gpd, int flag, bool on)
{
  for (int i = 1; i <= gpd->totcol; i++) {
    MaterialGPencilStyle *st = slot_style(gpd, i);
    if (st) { if (on) st->flag |= flag; else st->flag &= ~flag; }
  }
}
/* GPENCIL_OT_material_hide: unselected (false): hides the active material, or all the others */
static int exec_material_hide(PGOpContext *C, const float *p, int n)
{
  MaterialGPencilStyle *active = slot_style(C->gpd, C->actcol);
  if (active == NULL) return PG_OPERATOR_CANCELLED;
  if (prop_i(p, n, 0, 0)) {
    for (int i = 1; i <= C->gpd->totcol; i++) {
      MaterialGPencilStyle *st = slot_style(C->gpd, i);
      if (st && st != active) st->flag |= GP_MATERIAL_HIDE;
    }
  }
  else active->flag |= GP_MATERIAL_HIDE;
  return PG_OPERATOR_FINISHED;
}
static int exec_material_reveal(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  set_all_material_flag(C->gpd, GP_MATERIAL_HIDE, false);
  return PG_OPERATOR_FINISHED;
}
static int exec_material_lock_all(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  set_all_material_flag(C->gpd, GP_MATERIAL_LOCKED, true);
  return PG_OPERATOR_FINISHED;
}
static int exec_material_unlock_all(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  set_all_material_flag(C->gpd, GP_MATERIAL_LOCKED, false);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_material_lock_unused: lock all, then unlock the materials of selected strokes */
static int exec_material_lock_unused(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  set_all_material_flag(C->gpd, GP_MATERIAL_LOCKED, true);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      MaterialGPencilStyle *st = (gps->flag & GP_STROKE_SELECT) ? slot_style(C->gpd, gps->mat_nr + 1) : NULL;
      if (st) st->flag &= ~GP_MATERIAL_LOCKED;
    }
  }
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_material_isolate: affect_visibility (false); locks (and hides) the other materials,
 * or clears those flags on all when they already have them */
static int exec_material_isolate(PGOpContext *C, const float *p, int n)
{
  MaterialGPencilStyle *active = slot_style(C->gpd, C->actcol);
  if (active == NULL) return PG_OPERATOR_CANCELLED; /* "No active color to isolate" */
  int flags = GP_MATERIAL_LOCKED;
  if (prop_i(p, n, 0, 0)) flags |= GP_MATERIAL_HIDE;
  bool isolate = false;
  for (int i = 1; i <= C->gpd->totcol; i++) {
    MaterialGPencilStyle *st = slot_style(C->gpd, i);
    if (st && st != active && (st->flag & flags) == 0) { isolate = true; break; }
  }
  for (int i = 1; i <= C->gpd->totcol; i++) {
    MaterialGPencilStyle *st = slot_style(C->gpd, i);
    if (st == NULL) continue;
    if (isolate) { if (st != active) st->flag |= flags; }
    else st->flag &= ~flags;
  }
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_material_select: deselect (false); strokes of the active material on the active frames
 * (all selected frames in multiframe editing) */
static int exec_material_select(PGOpContext *C, const float *p, int n)
{
  if (slot_style(C->gpd, C->actcol) == NULL) return PG_OPERATOR_CANCELLED;
  const bool deselect = prop_i(p, n, 0, 0) != 0;
  const bool multi = GPENCIL_MULTIEDIT_SESSIONS_ON(C->gpd);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl)) continue;
    for (bGPDframe *gpf = multi ? gpl->frames.first : gpl->actframe; gpf; gpf = multi ? gpf->next : NULL) {
      if (gpf != gpl->actframe && !(gpf->flag & GP_FRAME_SELECT)) continue;
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        const MaterialGPencilStyle *st = slot_style(C->gpd, gps->mat_nr + 1);
        /* ED_gpencil_stroke_material_editable */
        if (st && ((st->flag & GP_MATERIAL_HIDE) ||
                   (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (st->flag & GP_MATERIAL_LOCKED))))
          continue;
        if (C->actcol != gps->mat_nr + 1) continue;
        if (!deselect) { gps->flag |= GP_STROKE_SELECT; BKE_gpencil_stroke_select_index_set(C->gpd, gps); }
        else { gps->flag &= ~GP_STROKE_SELECT; BKE_gpencil_stroke_select_index_reset(gps); }
        for (int i = 0; i < gps->totpoints; i++) {
          if (!deselect) gps->points[i].flag |= GP_SPOINT_SELECT;
          else gps->points[i].flag &= ~GP_SPOINT_SELECT;
        }
      }
    }
  }
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_material_set: slot (1-based) becomes active */
static int exec_material_set(PGOpContext *C, const float *p, int n)
{
  const int slot = prop_i(p, n, 0, 0);
  if (slot < 1 || slot > C->gpd->totcol) return PG_OPERATOR_CANCELLED; /* "non-existent material" */
  C->actcol = slot;
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_set_active_material: the first selected stroke's material becomes active */
static int exec_set_active_material(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      if (gps->flag & GP_STROKE_SELECT) { C->actcol = gps->mat_nr + 1; return PG_OPERATOR_FINISHED; }
    }
  }
  return PG_OPERATOR_FINISHED;
}

/* ---- GPENCIL_OT_hide / GPENCIL_OT_reveal (layers) ---------------------------------------- */
/* unselected (false): hides the active layer, or every other layer (the active one shown) */
static int exec_hide(PGOpContext *C, const float *p, int n)
{
  bGPDlayer *layer = BKE_gpencil_layer_active_get(C->gpd);
  if (prop_i(p, n, 0, 0)) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
      if (gpl != layer) gpl->flag |= GP_LAYER_HIDE;
      else gpl->flag &= ~GP_LAYER_HIDE;
    }
  }
  else layer->flag |= GP_LAYER_HIDE;
  return PG_OPERATOR_FINISHED;
}
/* select (true): in edit mode the revealed layers' active-frame strokes are selected, else their
 * strokes on every frame deselected */
static int exec_reveal(PGOpContext *C, const float *p, int n)
{
  const bool select = prop_i(p, n, 0, 1) != 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (!(gpl->flag & GP_LAYER_HIDE)) continue;
    gpl->flag &= ~GP_LAYER_HIDE;
    if (!(C->gpd->flag & GP_DATA_STROKE_EDITMODE)) continue;
    for (bGPDframe *gpf = select ? gpl->actframe : gpl->frames.first; gpf; gpf = select ? NULL : gpf->next) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        for (int i = 0; i < gps->totpoints; i++) SET_FLAG_FROM_TEST(gps->points[i].flag, select, GP_SPOINT_SELECT);
        SET_FLAG_FROM_TEST(gps->flag, select, GP_STROKE_SELECT);
      }
    }
  }
  return PG_OPERATOR_FINISHED;
}

/* ---- batch 9 ---------------------------------------------------------------------------- */
/* GP_EDITABLE_STROKES_BEGIN: editable layers, the active frame (every selected frame in multiframe) */
#define PGW_EDITABLE_STROKES_BEGIN(gpd_, gpl, gps)                                                   \
  {                                                                                                  \
    const bool multi_ = GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_);                                         \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &(gpd_)->layers) {                                           \
      if (!BKE_gpencil_layer_is_editable(gpl)) continue;                                             \
      for (bGPDframe *gpf_ = multi_ ? gpl->frames.first : gpl->actframe; gpf_;                       \
           gpf_ = multi_ ? gpf_->next : NULL) {                                                      \
        if (gpf_ != gpl->actframe && !(gpf_->flag & GP_FRAME_SELECT)) continue;                      \
        LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf_->strokes) {
#define PGW_EDITABLE_STROKES_END }}}}

/* GPENCIL_OT_stroke_simplify: factor (0) - Ramer-Douglas-Peucker on each selected stroke */
static int exec_simplify(PGOpContext *C, const float *p, int n)
{
  const float factor = prop_f(p, n, 0, 0.0f);
  if (factor < 0.0f) return PG_OPERATOR_CANCELLED;
  bool changed = false;
  PGW_EDITABLE_STROKES_BEGIN (C->gpd, gpl, gps) {
    (void)gpl;
    if (gps->flag & GP_STROKE_SELECT) { BKE_gpencil_stroke_simplify_adaptive(C->gpd, gps, factor); changed = true; }
  } PGW_EDITABLE_STROKES_END
  if (changed) BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_stroke_smooth: repeat (2), factor (1), only_selected (true), smooth_position (true),
 * smooth_thickness (true), smooth_strength (false), smooth_uv (false) - gpencil_smooth_stroke() */
static void smooth_selected(PGOpContext *C, int repeat, float factor, bool only_selected, bool pos, bool thick,
                            bool strength, bool uv)
{
  if (factor == 0.0f) return;
  PGW_EDITABLE_STROKES_BEGIN (C->gpd, gpl, gps) {
    (void)gpl;
    if (!(gps->flag & GP_STROKE_SELECT)) continue;
    bGPDstroke gps_old = *gps;
    gps_old.points = (bGPDspoint *)MEM_dupallocN(gps->points);
    bool need_update = false;
    for (int r = 0; r < repeat; r++) {
      for (int i = 0; i < gps->totpoints; i++) {
        if (only_selected && (gps->points[i].flag & GP_SPOINT_SELECT) == 0) continue;
        if (pos) { BKE_gpencil_stroke_smooth_point(&gps_old, i, factor, 1, false, false, gps); need_update = true; }
        if (strength) BKE_gpencil_stroke_smooth_strength(&gps_old, i, factor, 1, gps);
        if (thick) BKE_gpencil_stroke_smooth_thickness(&gps_old, i, 1.0f - factor, 1, gps);
        if (uv) { BKE_gpencil_stroke_smooth_uv(&gps_old, i, factor, 1, gps); need_update = true; }
      }
      if (r < repeat - 1) memcpy(gps_old.points, gps->points, sizeof(bGPDspoint) * gps->totpoints);
    }
    MEM_freeN(gps_old.points);
    if (need_update) BKE_gpencil_stroke_geometry_update(C->gpd, gps);
  } PGW_EDITABLE_STROKES_END
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
}
static int exec_smooth(PGOpContext *C, const float *p, int n)
{
  const int repeat = prop_i(p, n, 0, 2);
  const float factor = prop_f(p, n, 1, 1.0f);
  if (repeat < 1 || repeat > 50 || factor < 0.0f || factor > 2.0f) return PG_OPERATOR_CANCELLED;
  smooth_selected(C, repeat, factor, prop_i(p, n, 2, 1), prop_i(p, n, 3, 1), prop_i(p, n, 4, 1), prop_i(p, n, 5, 0),
                  prop_i(p, n, 6, 0));
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_active_frame_delete: the active layer's frame shown at cfra */
static int exec_active_frame_delete(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  bGPDframe *gpf = gpl ? BKE_gpencil_layer_frame_get(gpl, C->cfra, GP_GETFRAME_USE_PREV) : NULL;
  if (gpf == NULL) return PG_OPERATOR_CANCELLED; /* "No active frame to delete" */
  BKE_gpencil_layer_frame_delete(gpl, gpf);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_active_frames_delete_all: that frame on every editable layer */
static int exec_active_frames_delete_all(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  bool success = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl)) continue;
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, C->cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) continue;
    BKE_gpencil_layer_frame_delete(gpl, gpf);
    success = true;
  }
  return success ? PG_OPERATOR_FINISHED : PG_OPERATOR_CANCELLED; /* "No active frame(s) to delete" */
}

/* Vertex group operators (gpencil_data.c). shortcut: the active group (ob->actdef) and the assign
 * weight (ts->vgroup_weight) come as props 0 and 1; move them into PGOpContext with a settings struct. */
static int vg_index(const PGOpContext *C, const float *p, int n)
{
  const int def_nr = prop_i(p, n, 0, -1);
  return (def_nr >= 0 && def_nr < BLI_listbase_count(&C->gpd->vertex_group_names)) ? def_nr : -1;
}
static int exec_vg_assign(PGOpContext *C, const float *p, int n)
{
  const int g = vg_index(C, p, n);
  if (g < 0) return PG_OPERATOR_CANCELLED;
  pg_gp_vgroup_assign(C->gpd, NULL, g, fminf(fmaxf(prop_f(p, n, 1, 1.0f), 0.0f), 1.0f));
  return PG_OPERATOR_FINISHED;
}
static int exec_vg_remove_from(PGOpContext *C, const float *p, int n)
{
  const int g = vg_index(C, p, n);
  if (g < 0) return PG_OPERATOR_CANCELLED;
  pg_gp_vgroup_remove(C->gpd, NULL, g);
  return PG_OPERATOR_FINISHED;
}
static int exec_vg_select(PGOpContext *C, const float *p, int n)
{
  const int g = vg_index(C, p, n);
  if (g < 0) return PG_OPERATOR_CANCELLED;
  pg_gp_vgroup_select(C->gpd, NULL, g, 1);
  return PG_OPERATOR_FINISHED;
}
static int exec_vg_deselect(PGOpContext *C, const float *p, int n)
{
  const int g = vg_index(C, p, n);
  if (g < 0) return PG_OPERATOR_CANCELLED;
  pg_gp_vgroup_select(C->gpd, NULL, g, 0);
  return PG_OPERATOR_FINISHED;
}
static int exec_vg_invert(PGOpContext *C, const float *p, int n)
{
  const int g = vg_index(C, p, n);
  if (g < 0) return PG_OPERATOR_CANCELLED;
  pg_gp_vgroup_invert(C->gpd, NULL, g);
  return PG_OPERATOR_FINISHED;
}
static int exec_vg_normalize(PGOpContext *C, const float *p, int n)
{
  const int g = vg_index(C, p, n);
  if (g < 0) return PG_OPERATOR_CANCELLED;
  pg_gp_vgroup_normalize(C->gpd, NULL, g);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_snap_to_grid: the canvas grid size as prop 0 (Blender reads the viewport grid) */
static int exec_snap_to_grid(PGOpContext *C, const float *p, int n)
{
  const float grid = prop_f(p, n, 0, 0.0f);
  if (grid <= 0.0f) return PG_OPERATOR_CANCELLED;
  pg_gp_snap_to_grid(C->gpd, NULL, grid);
  return PG_OPERATOR_FINISHED;
}

/* ---- batch 10: selection tools and interpolation (exec with their properties) ---------- */
/* shortcut: the select mode (ts->gpencil_selectmode_edit) is the last prop; read it from tool settings
 * once PGOpContext carries them. Coordinates are canvas units. */
static bool valid_sel_op(int op) { return op >= 1 && op <= 5; } /* ADD 1, SUB 2, SET 3, AND 4, XOR 5 */

/* GPENCIL_OT_select_box: xmin, ymin, xmax, ymax, mode, selectmode */
static int exec_select_box(PGOpContext *C, const float *p, int n)
{
  if (n < 6) return PG_OPERATOR_CANCELLED;
  const int op = prop_i(p, n, 4, 3);
  if (!valid_sel_op(op)) return PG_OPERATOR_CANCELLED;
  pg_gp_select_box(C->gpd, prop_i(p, n, 0, 0), prop_i(p, n, 1, 0), prop_i(p, n, 2, 0), prop_i(p, n, 3, 0), op,
                   prop_i(p, n, 5, 0), NULL);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_select_lasso: mode, selectmode, then the path as x y pairs */
static int exec_select_lasso(PGOpContext *C, const float *p, int n)
{
  const int count = (n - 2) / 2;
  const int op = prop_i(p, n, 0, 0);
  if (count < 3 || !valid_sel_op(op)) return PG_OPERATOR_CANCELLED; /* Blender: no path, nothing done */
  int(*coords)[2] = malloc(sizeof(int[2]) * (size_t)count);
  if (coords == NULL) return PG_OPERATOR_CANCELLED;
  for (int i = 0; i < count; i++) {
    coords[i][0] = prop_i(p, n, 2 + i * 2, 0);
    coords[i][1] = prop_i(p, n, 3 + i * 2, 0);
  }
  pg_gp_select_lasso(C->gpd, (const int(*)[2])coords, count, op, prop_i(p, n, 1, 0), NULL);
  free(coords);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_select_circle: x, y, radius, mode, is_first (ED_select_op_modal), selectmode */
static int exec_select_circle(PGOpContext *C, const float *p, int n)
{
  if (n < 3) return PG_OPERATOR_CANCELLED;
  const int op = prop_i(p, n, 3, 0), radius = prop_i(p, n, 2, 25);
  if (!valid_sel_op(op) || radius < 1) return PG_OPERATOR_CANCELLED;
  pg_gp_select_circle(C->gpd, prop_i(p, n, 0, 0), prop_i(p, n, 1, 0), radius, op, prop_i(p, n, 4, 1),
                      prop_i(p, n, 5, 0), NULL);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_interpolate_sequence / GPENCIL_OT_interpolate (its confirm step): step (1), layers
 * ACTIVE 0 / ALL 1, interpolate_selected_only, exclude_breakdowns, flip (AUTO 2), smooth_steps (1),
 * smooth_factor (0), type (easing preset, 0 linear), easing (mode), then factor for the single one */
static int interpolate(PGOpContext *C, const float *p, int n, int single)
{
  PGInterpSettings st;
  memset(&st, 0, sizeof(st));
  st.step = prop_i(p, n, 0, 1);
  st.all_layers = prop_i(p, n, 1, 0);
  st.only_selected = prop_i(p, n, 2, 0);
  st.exclude_breakdowns = prop_i(p, n, 3, 0);
  st.flipmode = prop_i(p, n, 4, PG_INTERP_FLIPAUTO);
  st.smooth_steps = prop_i(p, n, 5, 1);
  st.smooth_factor = prop_f(p, n, 6, 0.0f);
  st.easing_type = prop_i(p, n, 7, 0);
  st.easing_mode = prop_i(p, n, 8, 0);
  st.single = single;
  st.factor = prop_f(p, n, 9, -1.0f);
  if (st.step < 1 || st.flipmode < PG_INTERP_NOFLIP || st.flipmode > PG_INTERP_FLIPAUTO ||
      st.smooth_steps < 1 || st.smooth_steps > 3 || st.smooth_factor < 0.0f || st.smooth_factor > 2.0f)
    return PG_OPERATOR_CANCELLED;
  /* Blender reports "Cannot find valid keyframes to interpolate" and cancels */
  return pg_gp_interpolate_run(C->gpd, BKE_gpencil_layer_active_get(C->gpd), C->cfra, &st) > 0 ?
             PG_OPERATOR_FINISHED : PG_OPERATOR_CANCELLED;
}
static int exec_interpolate_sequence(PGOpContext *C, const float *p, int n) { return interpolate(C, p, n, 0); }
static int exec_interpolate(PGOpContext *C, const float *p, int n) { return interpolate(C, p, n, 1); }

/* ---- batch 11 ------------------------------------------------------------------------- */
/* BEGIN VERBATIM gpencil_edit.c (Blender 3.6.23): gpencil_count_subdivision_cuts / gpencil_stroke_subdivide */
static int gpencil_count_subdivision_cuts(bGPDstroke *gps)
{
  bGPDspoint *pt;
  int i;
  int totnewpoints = 0;
  for (i = 0, pt = gps->points; i < gps->totpoints && pt; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      if (i + 1 < gps->totpoints) {
        if (gps->points[i + 1].flag & GP_SPOINT_SELECT) {
          totnewpoints++;
        }
      }
    }
  }

  if ((gps->flag & GP_STROKE_CYCLIC) && (gps->points[0].flag & GP_SPOINT_SELECT) &&
      (gps->points[gps->totpoints - 1].flag & GP_SPOINT_SELECT))
  {
    totnewpoints++;
  }

  return totnewpoints;
}

static void gpencil_stroke_subdivide(bGPDstroke *gps, const int cuts)
{
  bGPDspoint *temp_points;
  int totnewpoints, oldtotpoints;
  int i2;
  /* loop as many times as cuts */
  for (int s = 0; s < cuts; s++) {
    totnewpoints = gpencil_count_subdivision_cuts(gps);
    if (totnewpoints == 0) {
      continue;
    }
    /* duplicate points in a temp area */
    temp_points = MEM_dupallocN(gps->points);
    oldtotpoints = gps->totpoints;

    MDeformVert *temp_dverts = NULL;
    MDeformVert *dvert_final = NULL;
    MDeformVert *dvert = NULL;
    MDeformVert *dvert_next = NULL;
    if (gps->dvert != NULL) {
      temp_dverts = MEM_dupallocN(gps->dvert);
    }

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
    if (gps->dvert != NULL) {
      gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
    }

    /* loop and interpolate */
    i2 = 0;
    for (int i = 0; i < oldtotpoints; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[i2];

      /* copy current point */
      copy_v3_v3(&pt_final->x, &pt->x);
      pt_final->pressure = pt->pressure;
      pt_final->strength = pt->strength;
      pt_final->time = pt->time;
      pt_final->flag = pt->flag;
      copy_v4_v4(pt_final->vert_color, pt->vert_color);

      if (gps->dvert != NULL) {
        dvert = &temp_dverts[i];
        dvert_final = &gps->dvert[i2];
        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = dvert->dw;
      }
      i2++;

      /* if next point is selected add a half way point */
      if (pt->flag & GP_SPOINT_SELECT) {
        if (i + 1 < oldtotpoints) {
          if (temp_points[i + 1].flag & GP_SPOINT_SELECT) {
            pt_final = &gps->points[i2];
            if (gps->dvert != NULL) {
              dvert_final = &gps->dvert[i2];
            }
            /* Interpolate all values */
            bGPDspoint *next = &temp_points[i + 1];
            interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
            pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
            pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
            CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
            interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);
            pt_final->time = interpf(pt->time, next->time, 0.5f);
            pt_final->flag |= GP_SPOINT_SELECT;

            /* interpolate weights */
            if (gps->dvert != NULL) {
              dvert = &temp_dverts[i];
              dvert_next = &temp_dverts[i + 1];
              dvert_final = &gps->dvert[i2];

              dvert_final->totweight = dvert->totweight;
              dvert_final->dw = MEM_dupallocN(dvert->dw);

              /* interpolate weight values */
              for (int d = 0; d < dvert->totweight; d++) {
                MDeformWeight *dw_a = &dvert->dw[d];
                if (dvert_next->totweight > d) {
                  MDeformWeight *dw_b = &dvert_next->dw[d];
                  MDeformWeight *dw_final = &dvert_final->dw[d];
                  dw_final->weight = interpf(dw_a->weight, dw_b->weight, 0.5f);
                }
              }
            }

            i2++;
          }
        }
      }
    }

    /* Subdivide between last and first point. */
    if (gps->flag & GP_STROKE_CYCLIC) {
      bGPDspoint *pt = &temp_points[oldtotpoints - 1];
      bGPDspoint *next = &temp_points[0];
      if ((pt->flag & GP_SPOINT_SELECT) && (next->flag & GP_SPOINT_SELECT)) {
        bGPDspoint *pt_final = &gps->points[i2];
        if (gps->dvert != NULL) {
          dvert_final = &gps->dvert[i2];
        }
        /* Interpolate all values */
        interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
        pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
        pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
        CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
        interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);
        pt_final->time = interpf(pt->time, next->time, 0.5f);
        pt_final->flag |= GP_SPOINT_SELECT;

        /* interpolate weights */
        if (gps->dvert != NULL) {
          dvert = &temp_dverts[oldtotpoints - 1];
          dvert_next = &temp_dverts[0];
          dvert_final = &gps->dvert[i2];

          dvert_final->totweight = dvert->totweight;
          dvert_final->dw = MEM_dupallocN(dvert->dw);

          /* interpolate weight values */
          for (int d = 0; d < dvert->totweight; d++) {
            MDeformWeight *dw_a = &dvert->dw[d];
            if (dvert_next->totweight > d) {
              MDeformWeight *dw_b = &dvert_next->dw[d];
              MDeformWeight *dw_final = &dvert_final->dw[d];
              dw_final->weight = interpf(dw_a->weight, dw_b->weight, 0.5f);
            }
          }
        }
      }
    }

    /* free temp memory */
    MEM_SAFE_FREE(temp_points);
    MEM_SAFE_FREE(temp_dverts);
  }
}
/* END VERBATIM */

/* GPENCIL_OT_stroke_subdivide: number_cuts (1, 1..10), then the smooth props factor (0), repeat (1),
 * only_selected (true), smooth_position, smooth_thickness (true), smooth_strength, smooth_uv (false) */
static int exec_subdivide(PGOpContext *C, const float *p, int n)
{
  const int cuts = prop_i(p, n, 0, 1), repeat = prop_i(p, n, 2, 1);
  const float factor = prop_f(p, n, 1, 0.0f);
  if (cuts < 1 || cuts > 10 || repeat < 1 || repeat > 10 || factor < 0.0f || factor > 2.0f) return PG_OPERATOR_CANCELLED;
  bool changed = false;
  PGW_EDITABLE_STROKES_BEGIN (C->gpd, gpl, gps) {
    (void)gpl;
    if (gps->flag & GP_STROKE_SELECT) {
      gpencil_stroke_subdivide(gps, cuts);
      BKE_gpencil_stroke_geometry_update(C->gpd, gps);
      changed = true;
    }
  } PGW_EDITABLE_STROKES_END
  if (changed) {
    smooth_selected(C, repeat, factor, prop_i(p, n, 3, 1), prop_i(p, n, 4, 1), prop_i(p, n, 5, 1), prop_i(p, n, 6, 0),
                    prop_i(p, n, 7, 0));
    BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  }
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_stroke_trim: each selected stroke trimmed at its own self-intersection */
static int exec_trim(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  PGW_EDITABLE_STROKES_BEGIN (C->gpd, gpl, gps) {
    (void)gpl;
    if (gps->flag & GP_STROKE_SELECT) BKE_gpencil_stroke_trim(C->gpd, gps);
  } PGW_EDITABLE_STROKES_END
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_stroke_cutter: flat_caps, then the lasso path (canvas units). No path: CANCELLED. */
static int exec_cutter(PGOpContext *C, const float *p, int n)
{
  const int count = (n - 1) / 2;
  if (count < 3) return PG_OPERATOR_CANCELLED;
  int(*co)[2] = malloc(sizeof(int[2]) * (size_t)count);
  if (co == NULL) return PG_OPERATOR_CANCELLED;
  for (int i = 0; i < count; i++) {
    co[i][0] = prop_i(p, n, 1 + i * 2, 0);
    co[i][1] = prop_i(p, n, 2 + i * 2, 0);
  }
  pg_gp_cutter(C->gpd, (const int(*)[2])co, count, prop_i(p, n, 0, 0));
  free(co);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_select (click): x, y, radius_squared, flags (PG_PICK_*: extend, deselect, toggle,
 * entire_strokes, deselect_all, ...), selectmode */
static int exec_select_pick(PGOpContext *C, const float *p, int n)
{
  if (n < 5 || !isfinite(p[0]) || !isfinite(p[1])) return PG_OPERATOR_CANCELLED;
  pg_gp_edit_pick(C->gpd, NULL, p[0], p[1], prop_i(p, n, 2, 0), prop_i(p, n, 3, 0), prop_i(p, n, 4, 0));
  return PG_OPERATOR_FINISHED;
}

/* ---- batch 12 ------------------------------------------------------------------------- */
/* gpencil_selected_hue_table(): rounded hues of the selected points' vertex colours */
static void selected_hue_table(bGPdata *gpd, int threshold, GHash *hue_table)
{
  const float range = powf(10.0f, 5 - threshold);
  float hsv[3];
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl)) continue;
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        const MaterialGPencilStyle *st = slot_style(gpd, gps->mat_nr + 1);
        if (st && ((st->flag & GP_MATERIAL_HIDE) || (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (st->flag & GP_MATERIAL_LOCKED))))
          continue;
        if ((gps->flag & GP_STROKE_SELECT) == 0) continue;
        for (int i = 0; i < gps->totpoints; i++) {
          const bGPDspoint *pt = &gps->points[i];
          if (((pt->flag & GP_SPOINT_SELECT) == 0) || (pt->vert_color[3] == 0.0f)) continue;
          rgb_to_hsv_compat_v(pt->vert_color, hsv);
          const unsigned int key = (unsigned int)truncf(hsv[0] * range);
          if (!BLI_ghash_haskey(hue_table, POINTER_FROM_INT(key)))
            BLI_ghash_insert(hue_table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
        }
      }
    }
  }
}
/* GPENCIL_OT_select_vertex_color: threshold (0, 0..5), selectmode (vertex paint select mode as the
 * last prop): every stroke point whose vertex colour hue matches a selected point's is selected */
static int exec_select_vertex_color(PGOpContext *C, const float *p, int n)
{
  const int threshold = prop_i(p, n, 0, 0);
  if (threshold < 0 || threshold > 5) return PG_OPERATOR_CANCELLED;
  const int selectmode = prop_i(p, n, 1, 0);
  const float range = powf(10.0f, 5 - threshold);
  GHash *hue_table = BLI_ghash_int_new(__func__);
  selected_hue_table(C->gpd, threshold, hue_table);
  if (BLI_ghash_len(hue_table) == 0) { /* "Select before some Vertex to use as a filter color" */
    BLI_ghash_free(hue_table, NULL, NULL);
    return PG_OPERATOR_CANCELLED;
  }
  PGW_EDITABLE_STROKES_BEGIN (C->gpd, gpl, gps) {
    (void)gpl;
    bool gps_selected = false;
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      if (pt->vert_color[3] == 0.0f) continue;
      float hsv[3];
      rgb_to_hsv_compat_v(pt->vert_color, hsv);
      const unsigned int key = (unsigned int)truncf(hsv[0] * range);
      if (BLI_ghash_haskey(hue_table, POINTER_FROM_INT(key))) { pt->flag |= GP_SPOINT_SELECT; gps_selected = true; }
    }
    if (gps_selected) {
      gps->flag |= GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_set(C->gpd, gps);
      if (selectmode == GP_SELECTMODE_STROKE)
        for (int i = 0; i < gps->totpoints; i++) gps->points[i].flag |= GP_SPOINT_SELECT;
    }
  } PGW_EDITABLE_STROKES_END
  BLI_ghash_free(hue_table, NULL, NULL);
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

#define RU (PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO)
static const PGOperatorType pg_stroke_operators[] = {
    {"GPENCIL_OT_stroke_flip", RU, poll_stroke_edit, exec_flip},
    {"GPENCIL_OT_stroke_cyclical_set", RU, poll_stroke_edit, exec_cyclical},
    {"GPENCIL_OT_stroke_caps_set", RU, poll_stroke_edit, exec_caps},
    {"GPENCIL_OT_stroke_start_set", RU, poll_stroke_edit, exec_start_set},
    {"GPENCIL_OT_stroke_merge_by_distance", RU, poll_stroke_edit, exec_merge_distance},
    {"GPENCIL_OT_stroke_normalize", RU, poll_stroke_edit, exec_normalize},
    {"GPENCIL_OT_stroke_simplify_fixed", RU, poll_stroke_edit, exec_simplify_fixed},
    {"GPENCIL_OT_stroke_sample", RU, poll_stroke_edit, exec_sample},
    {"GPENCIL_OT_select_random", RU, poll_gpd, exec_select_random},
    {"GPENCIL_OT_select_first", RU, poll_gpd, exec_select_first},
    {"GPENCIL_OT_select_last", RU, poll_gpd, exec_select_last},
    {"GPENCIL_OT_stroke_change_color", RU, poll_active_layer, exec_change_color},
    {"GPENCIL_OT_stroke_outline", RU, poll_stroke_edit, exec_outline},
    {"GPENCIL_OT_material_hide", RU, poll_material, exec_material_hide},
    {"GPENCIL_OT_material_reveal", RU, poll_material, exec_material_reveal},
    {"GPENCIL_OT_material_lock_all", RU, poll_material, exec_material_lock_all},
    {"GPENCIL_OT_material_unlock_all", RU, poll_material, exec_material_unlock_all},
    {"GPENCIL_OT_material_lock_unused", RU, poll_active_layer, exec_material_lock_unused},
    {"GPENCIL_OT_material_isolate", RU, poll_material, exec_material_isolate},
    {"GPENCIL_OT_material_select", RU, poll_material, exec_material_select},
    {"GPENCIL_OT_material_set", RU, poll_material, exec_material_set},
    {"GPENCIL_OT_set_active_material", RU, poll_material, exec_set_active_material},
    {"GPENCIL_OT_hide", RU, poll_active_layer, exec_hide},
    {"GPENCIL_OT_reveal", RU, poll_gpd, exec_reveal},
    {"GPENCIL_OT_stroke_simplify", RU, poll_stroke_edit, exec_simplify},
    {"GPENCIL_OT_stroke_smooth", RU, poll_stroke_edit, exec_smooth},
    {"GPENCIL_OT_active_frame_delete", RU, poll_active_layer, exec_active_frame_delete},
    {"GPENCIL_OT_active_frames_delete_all", RU, poll_gpd, exec_active_frames_delete_all},
    {"GPENCIL_OT_vertex_group_assign", RU, poll_gpd, exec_vg_assign},
    {"GPENCIL_OT_vertex_group_remove_from", RU, poll_gpd, exec_vg_remove_from},
    {"GPENCIL_OT_vertex_group_select", RU, poll_gpd, exec_vg_select},
    {"GPENCIL_OT_vertex_group_deselect", RU, poll_gpd, exec_vg_deselect},
    {"GPENCIL_OT_vertex_group_invert", RU, poll_gpd, exec_vg_invert},
    {"GPENCIL_OT_vertex_group_normalize", RU, poll_gpd, exec_vg_normalize},
    {"GPENCIL_OT_snap_to_grid", RU, poll_stroke_edit, exec_snap_to_grid},
    {"GPENCIL_OT_select_box", RU, poll_gpd, exec_select_box},
    {"GPENCIL_OT_select_lasso", RU, poll_gpd, exec_select_lasso},
    {"GPENCIL_OT_select_circle", RU, poll_gpd, exec_select_circle},
    {"GPENCIL_OT_interpolate_sequence", RU, poll_active_layer, exec_interpolate_sequence},
    {"GPENCIL_OT_interpolate", RU, poll_active_layer, exec_interpolate},
    {"GPENCIL_OT_stroke_subdivide", RU, poll_active_layer, exec_subdivide},
    {"GPENCIL_OT_stroke_trim", RU, poll_stroke_edit, exec_trim},
    {"GPENCIL_OT_stroke_cutter", RU, poll_gpd, exec_cutter},
    {"GPENCIL_OT_select", RU, poll_gpd, exec_select_pick},
    {"GPENCIL_OT_select_vertex_color", RU, poll_gpd, exec_select_vertex_color},
};

const PGOperatorType *pg_wm_stroke_operator_find(const char *idname)
{
  if (idname == NULL) return NULL;
  for (size_t i = 0; i < sizeof(pg_stroke_operators) / sizeof(pg_stroke_operators[0]); i++)
    if (strcmp(pg_stroke_operators[i].idname, idname) == 0) return &pg_stroke_operators[i];
  return NULL;
}
