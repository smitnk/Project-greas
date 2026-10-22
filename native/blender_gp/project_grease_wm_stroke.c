/* Project Grease: the eraser marks and lasso holes (holdout materials) are app-internal strokes, so
 * select / edit / sculpt / cutter tools skip them like hidden materials. */
#define PG_MATERIAL_UNEDITABLE (GP_MATERIAL_HIDE | GP_MATERIAL_IS_STROKE_HOLDOUT | GP_MATERIAL_IS_FILL_HOLDOUT)
/* SPDX-License-Identifier: GPL-2.0-or-later
 * Stroke and material operators of gpencil_edit.c / gpencil_select.c / gpencil_data.c (Blender
 * 3.6.23) on the operator layer (project_grease_wm.h). Stroke bodies are the ported functions
 * the app already used; material bodies follow gpencil_data.c with bGPdata::mat[] for the
 * object's slots and PGOpContext::actcol for ob->actcol. */
#include <float.h>
#include <math.h>
#include <string.h>

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_deform.h"
#include "BKE_curve.h"
#include "BKE_gpencil_curve_legacy.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
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
#include "project_grease_blender_primitive.h"
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
        if (st && ((st->flag & PG_MATERIAL_UNEDITABLE) ||
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
        if (st && ((st->flag & PG_MATERIAL_UNEDITABLE) || (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (st->flag & GP_MATERIAL_LOCKED))))
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

/* ---- layer masks (gpencil_data.c). shortcut: the app picks masks by index, so the mask layer
 * (name prop) and the active mask (act_mask) come as indexes in props 0 / 0 and 1. */
/* GPENCIL_OT_layer_mask_add: the layer at index prop 0 masks the active layer */
static int exec_layer_mask_add(PGOpContext *C, const float *p, int n)
{
  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(C->gpd);
  bGPDlayer *gpl = BLI_findlink(&C->gpd->layers, prop_i(p, n, 0, -1));
  if (gpl == NULL) return PG_OPERATOR_CANCELLED;                      /* "Unable to find layer to add" */
  if (gpl == gpl_active) return PG_OPERATOR_CANCELLED;                /* "Cannot add active layer as mask" */
  if (BKE_gpencil_layer_mask_named_get(gpl_active, gpl->info)) return PG_OPERATOR_CANCELLED; /* "Layer already added" */
  if (gpl_active->act_mask == 256) return PG_OPERATOR_CANCELLED;     /* "Maximum number of masking layers reached" */
  BKE_gpencil_layer_mask_add(gpl_active, gpl->info);
  BKE_gpencil_layer_mask_sort(C->gpd, gpl_active);
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_layer_mask_remove: the active layer's mask at index prop 0 (made act_mask) */
static int exec_layer_mask_remove(PGOpContext *C, const float *p, int n)
{
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  const int index = prop_i(p, n, 0, gpl->act_mask - 1);
  if (index >= 0) gpl->act_mask = index + 1;
  if (gpl->act_mask > 0) {
    bGPDlayer_Mask *mask = BLI_findlink(&gpl->mask_layers, gpl->act_mask - 1);
    if (mask != NULL) {
      BKE_gpencil_layer_mask_remove(gpl, mask);
      if ((gpl->mask_layers.first != NULL) && (gpl->act_mask == 0)) gpl->act_mask = 1;
    }
  }
  BKE_gpencil_layer_mask_sort(C->gpd, gpl);
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_layer_mask_move: mask index prop 0 (made act_mask), type UP -1 / DOWN 1 */
static int exec_layer_mask_move(PGOpContext *C, const float *p, int n)
{
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  const int index = prop_i(p, n, 0, gpl->act_mask - 1), direction = prop_i(p, n, 1, -1);
  if (direction != -1 && direction != 1) return PG_OPERATOR_CANCELLED;
  if (index >= 0) gpl->act_mask = index + 1;
  if (gpl->act_mask > 0) {
    bGPDlayer_Mask *mask = BLI_findlink(&gpl->mask_layers, gpl->act_mask - 1);
    if (mask != NULL && BLI_listbase_link_move(&gpl->mask_layers, mask, direction)) {
      gpl->act_mask += direction;
      BKE_gpencil_batch_cache_dirty_tag(C->gpd);
    }
  }
  return PG_OPERATOR_FINISHED;
}

/* ---- primitives (gpencil_primitive.c) -------------------------------------------------- */
/* GPENCIL_OT_primitive_line / polyline / box / circle / curve, their confirm step: the stroke is
 * generated from the control points (gpencil_primitive_update_strokes: line, box, circle, arc,
 * bezier with `edges` subdivisions), added to the active layer's frame at cfra
 * (BKE_gpencil_layer_frame_get ADD_NEW), closed for box / circle, and left selected alone.
 * Props: type (PG_PRIMITIVE_*; arc and curve share primitive_curve), edges, material slot
 * (0-based), thickness, then the control points x y.
 * shortcut: brush material / thickness come as props (Blender reads the active brush); pressure and
 * strength are 1 as the primitive's default curve gives them. */
static int primitive_exec(PGOpContext *C, const float *p, int n, int t0, int t1)
{
  const int type = prop_i(p, n, 0, t0);
  if (type != t0 && type != t1) return PG_OPERATOR_CANCELLED;
  int edges = prop_i(p, n, 1, 0);
  if (edges < 1) edges = project_grease_blender_primitive_default_edges(type);
  const int mat = prop_i(p, n, 2, 0);
  const float thickness = prop_f(p, n, 3, 10.0f);
  const int anchors = (n - 4) / 2;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  if (anchors < 2 || edges < 1 || mat < 0 || thickness <= 0.0f || gpl == NULL || !BKE_gpencil_layer_is_editable(gpl))
    return PG_OPERATOR_CANCELLED;
  const int count = project_grease_blender_primitive_point_count(type, anchors, edges);
  if (count < 2) return PG_OPERATOR_CANCELLED;
  float *xy = MEM_mallocN(sizeof(float[2]) * (size_t)count, __func__);
  if (project_grease_blender_primitive_generate(type, p + 4, anchors, edges, 0, xy, count) != count) {
    MEM_freeN(xy);
    return PG_OPERATOR_CANCELLED;
  }
  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, C->cfra, GP_GETFRAME_ADD_NEW);
  if (gpf == NULL) {
    MEM_freeN(xy);
    return PG_OPERATOR_CANCELLED;
  }
  gpl->actframe = gpf;
  pg_gp_select_all(C->gpd, 3 /* deselect */, NULL);
  const short th = (short)fminf(fmaxf(thickness, 1.0f), 32767.0f);
  bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, mat, count, th, false);
  for (int i = 0; i < count; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->x = xy[i * 2]; pt->y = xy[i * 2 + 1]; pt->z = 0.0f;
    pt->pressure = 1.0f; pt->strength = 1.0f;
    pt->flag |= GP_SPOINT_SELECT;
  }
  MEM_freeN(xy);
  if (project_grease_blender_primitive_is_cyclic(type)) gps->flag |= GP_STROKE_CYCLIC;
  gps->flag |= GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_set(C->gpd, gps);
  BKE_gpencil_stroke_geometry_update(C->gpd, gps);
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}
static int exec_prim_line(PGOpContext *C, const float *p, int n) { return primitive_exec(C, p, n, PG_PRIMITIVE_LINE, PG_PRIMITIVE_LINE); }
static int exec_prim_polyline(PGOpContext *C, const float *p, int n) { return primitive_exec(C, p, n, PG_PRIMITIVE_POLYLINE, PG_PRIMITIVE_POLYLINE); }
static int exec_prim_box(PGOpContext *C, const float *p, int n) { return primitive_exec(C, p, n, PG_PRIMITIVE_BOX, PG_PRIMITIVE_BOX); }
static int exec_prim_circle(PGOpContext *C, const float *p, int n) { return primitive_exec(C, p, n, PG_PRIMITIVE_CIRCLE, PG_PRIMITIVE_CIRCLE); }
static int exec_prim_curve(PGOpContext *C, const float *p, int n) { return primitive_exec(C, p, n, PG_PRIMITIVE_CURVE, PG_PRIMITIVE_ARC); }

/* ---- GPENCIL_OT_stroke_join (gpencil_edit.c, both types) --------------------------------- */
/* BEGIN VERBATIM gpencil_edit.c: tJoinStrokes / gpencil_get_nearest_stroke_index */
typedef struct tJoinStrokes {
  bGPDframe *gpf;
  bGPDstroke *gps;
  bool used;
} tJoinStrokes;

static int gpencil_get_nearest_stroke_index(tJoinStrokes *strokes_list,
                                            const bGPDstroke *gps,
                                            const int totstrokes)
{
  int index = -1;
  float min_dist = FLT_MAX;
  float dist, start_a[3], end_a[3], start_b[3], end_b[3];

  bGPDspoint *pt = &gps->points[0];
  copy_v3_v3(start_a, &pt->x);

  pt = &gps->points[gps->totpoints - 1];
  copy_v3_v3(end_a, &pt->x);

  for (int i = 0; i < totstrokes; i++) {
    tJoinStrokes *elem = &strokes_list[i];
    if (elem->used) {
      continue;
    }
    pt = &elem->gps->points[0];
    copy_v3_v3(start_b, &pt->x);

    pt = &elem->gps->points[elem->gps->totpoints - 1];
    copy_v3_v3(end_b, &pt->x);

    dist = len_squared_v3v3(start_a, start_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
    dist = len_squared_v3v3(start_a, end_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
    dist = len_squared_v3v3(end_a, start_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
    dist = len_squared_v3v3(end_a, end_b);
    if (dist < min_dist) {
      min_dist = dist;
      index = i;
    }
  }

  return index;
}
/* END VERBATIM */

/* gpencil_stroke_join_exec(): type JOIN 0 / JOINCOPY 1, leave_gaps (false). The selected strokes of
 * the active frames (up to 128) are chained nearest-end first into a new stroke placed before the
 * first one; JOIN deletes the originals, JOINCOPY keeps them. */
static int exec_stroke_join(PGOpContext *C, const float *p, int n)
{
  const int max_join_strokes = 128;
  const int type = prop_i(p, n, 0, 0);
  const bool leave_gaps = prop_i(p, n, 1, 0) != 0;
  bGPDlayer *activegpl = BKE_gpencil_layer_active_get(C->gpd);
  if (type != 0 && type != 1) return PG_OPERATOR_CANCELLED;
  if (activegpl == NULL || (activegpl->flag & GP_LAYER_LOCKED)) return PG_OPERATOR_CANCELLED;
  int tot_strokes = 0;
  tJoinStrokes *strokes_list = MEM_malloc_arrayN(max_join_strokes, sizeof(tJoinStrokes), __func__);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    bGPDframe *gpf = gpl->actframe;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if (!(gps->flag & GP_STROKE_SELECT) || gps->totpoints < 1) continue;
      const MaterialGPencilStyle *st = slot_style(C->gpd, gps->mat_nr + 1);
      if (st && ((st->flag & PG_MATERIAL_UNEDITABLE) || (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (st->flag & GP_MATERIAL_LOCKED))))
        continue;
      strokes_list[tot_strokes].gpf = gpf;
      strokes_list[tot_strokes].gps = gps;
      strokes_list[tot_strokes].used = false;
      if (++tot_strokes == max_join_strokes) break; /* "Too many strokes selected, only joined first %d strokes" */
    }
    if (tot_strokes == max_join_strokes) break;
  }
  if (tot_strokes < 2) {
    MEM_SAFE_FREE(strokes_list);
    return PG_OPERATOR_CANCELLED;
  }
  tJoinStrokes *elem = &strokes_list[0];
  elem->used = true;
  bGPDstroke *gps_new = BKE_gpencil_stroke_duplicate(elem->gps, true, true);
  gps_new->flag &= ~GP_STROKE_CYCLIC;
  BLI_insertlinkbefore(&elem->gpf->strokes, elem->gps, gps_new);
  while (true) {
    const int i = gpencil_get_nearest_stroke_index(strokes_list, gps_new, tot_strokes);
    if (i < 0) break;
    elem = &strokes_list[i];
    BKE_gpencil_stroke_join(gps_new, elem->gps, leave_gaps, true, false, true);
    elem->used = true;
  }
  BKE_gpencil_stroke_geometry_update(C->gpd, gps_new);
  if (type == 0) {
    for (int i = 0; i < tot_strokes; i++) {
      elem = &strokes_list[i];
      BLI_remlink(&elem->gpf->strokes, elem->gps);
      BKE_gpencil_free_stroke(elem->gps);
    }
  }
  MEM_SAFE_FREE(strokes_list);
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* ---- batch 18: stroke merge, merge similar materials, materials to vertex colour, palette ---- */
#define GP_MERGE_STROKE -1
#define GP_MERGE_POINT 1
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_merge.c */
typedef struct tGPencilPointCache {
  float factor; /* value to sort */
  bGPDstroke *gps;
  float x, y, z;
  float pressure;
  float strength;
  float vert_color[4];
} tGPencilPointCache;

/* helper function to sort points */
static int gpencil_sort_points(const void *a1, const void *a2)
{
  const tGPencilPointCache *ps1 = a1, *ps2 = a2;

  if (ps1->factor < ps2->factor) {
    return -1;
  }

  if (ps1->factor > ps2->factor) {
    return 1;
  }

  return 0;
}

static void gpencil_insert_points_to_stroke(bGPDstroke *gps,
                                            tGPencilPointCache *points_array,
                                            int totpoints)
{
  tGPencilPointCache *point_elem = NULL;

  for (int i = 0; i < totpoints; i++) {
    point_elem = &points_array[i];
    bGPDspoint *pt_dst = &gps->points[i];

    copy_v3_v3(&pt_dst->x, &point_elem->x);
    pt_dst->pressure = point_elem->pressure;
    pt_dst->strength = point_elem->strength;
    pt_dst->uv_fac = 1.0f;
    pt_dst->uv_rot = 0;
    pt_dst->flag |= GP_SPOINT_SELECT;
    copy_v4_v4(pt_dst->vert_color, point_elem->vert_color);
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_merge.c */
/* insert a group of points in destination array */
static int gpencil_insert_to_array(tGPencilPointCache *src_array,
                                   tGPencilPointCache *dst_array,
                                   int totpoints,
                                   bGPDstroke *gps_filter,
                                   bool reverse,
                                   int last)
{
  tGPencilPointCache *src_elem = NULL;
  tGPencilPointCache *dst_elem = NULL;
  int idx = 0;

  for (int i = 0; i < totpoints; i++) {
    if (!reverse) {
      idx = i;
    }
    else {
      idx = totpoints - i - 1;
    }
    src_elem = &src_array[idx];
    /* check if all points or only a stroke */
    if (!ELEM(gps_filter, NULL, src_elem->gps)) {
      continue;
    }

    dst_elem = &dst_array[last];
    last++;

    copy_v3_v3(&dst_elem->x, &src_elem->x);
    dst_elem->gps = src_elem->gps;
    dst_elem->pressure = src_elem->pressure;
    dst_elem->strength = src_elem->strength;
    dst_elem->factor = src_elem->factor;
    copy_v4_v4(dst_elem->vert_color, src_elem->vert_color);
  }

  return last;
}

/* get first and last point location */
static void gpencil_get_extremes(
    tGPencilPointCache *src_array, int totpoints, bGPDstroke *gps_filter, float *start, float *end)
{
  tGPencilPointCache *array_pt = NULL;

  /* find first point */
  for (int i = 0; i < totpoints; i++) {
    array_pt = &src_array[i];
    if (gps_filter == array_pt->gps) {
      copy_v3_v3(start, &array_pt->x);
      break;
    }
  }
  /* find last point */
  for (int i = totpoints - 1; i >= 0; i--) {
    array_pt = &src_array[i];
    if (gps_filter == array_pt->gps) {
      copy_v3_v3(end, &array_pt->x);
      break;
    }
  }
}

static int gpencil_analyze_strokes(tGPencilPointCache *src_array,
                                   int totstrokes,
                                   int totpoints,
                                   tGPencilPointCache *dst_array)
{
  int i;
  int last = 0;
  GHash *all_strokes = BLI_ghash_ptr_new(__func__);
  /* add first stroke to array */
  tGPencilPointCache *sort_pt = &src_array[0];
  bGPDstroke *gps = sort_pt->gps;
  last = gpencil_insert_to_array(src_array, dst_array, totpoints, gps, false, last);
  float start[3];
  float end[3];
  float end_prv[3];
  gpencil_get_extremes(src_array, totpoints, gps, start, end);
  copy_v3_v3(end_prv, end);
  BLI_ghash_insert(all_strokes, sort_pt->gps, sort_pt->gps);

  /* look for near stroke */
  bool loop = (bool)(totstrokes > 1);
  while (loop) {
    bGPDstroke *gps_next = NULL;
    GHash *strokes = BLI_ghash_ptr_new(__func__);
    float dist_start = 0.0f;
    float dist_end = 0.0f;
    float dist = FLT_MAX;
    bool reverse = false;

    for (i = 0; i < totpoints; i++) {
      sort_pt = &src_array[i];
      /* avoid dups */
      if (BLI_ghash_haskey(all_strokes, sort_pt->gps)) {
        continue;
      }
      if (!BLI_ghash_haskey(strokes, sort_pt->gps)) {
        gpencil_get_extremes(src_array, totpoints, sort_pt->gps, start, end);
        /* distances to previous end */
        dist_start = len_v3v3(end_prv, start);
        dist_end = len_v3v3(end_prv, end);

        if (dist > dist_start) {
          gps_next = sort_pt->gps;
          dist = dist_start;
          reverse = false;
        }
        if (dist > dist_end) {
          gps_next = sort_pt->gps;
          dist = dist_end;
          reverse = true;
        }
        BLI_ghash_insert(strokes, sort_pt->gps, sort_pt->gps);
      }
    }
    BLI_ghash_free(strokes, NULL, NULL);

    /* add the stroke to array */
    if (gps_next != NULL) {
      BLI_ghash_insert(all_strokes, gps_next, gps_next);
      last = gpencil_insert_to_array(src_array, dst_array, totpoints, gps_next, reverse, last);
      /* replace last end */
      sort_pt = &dst_array[last - 1];
      copy_v3_v3(end_prv, &sort_pt->x);
    }

    /* loop exit */
    if (last >= totpoints) {
      loop = false;
    }
  }

  BLI_ghash_free(all_strokes, NULL, NULL);
  return last;
}
/* END VERBATIM */

/* gpencil_calc_points_factor() over the active frames of the editable layers (CTX
 * editable_gpencil_layers); the angle / distance factor is Blender's, line for line. */
static void pgw_merge_points_factor(bGPdata *gpd, const int mode, int totpoints, const bool clear_point,
                                    const bool clear_stroke, tGPencilPointCache *src_array)
{
  bGPDspoint *pt;
  int idx = 0;
  bGPDstroke **gps_array = MEM_callocN(sizeof(bGPDstroke *) * totpoints, __func__);
  bGPDspoint *pt_array = MEM_callocN(sizeof(bGPDspoint) * totpoints, __func__);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      if (!(gps->flag & GP_STROKE_SELECT)) continue;
      int i;
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        if (clear_stroke) pt->flag |= GP_SPOINT_TAG;
        else pt->flag &= ~GP_SPOINT_TAG;
        if (pt->flag & GP_SPOINT_SELECT) {
          bGPDspoint *pt2 = &pt_array[idx];
          copy_v3_v3(&pt2->x, &pt->x);
          pt2->pressure = pt->pressure;
          pt2->strength = pt->strength;
          copy_v4_v4(pt2->vert_color, pt->vert_color);
          pt->flag &= ~GP_SPOINT_SELECT;
          if (clear_point) pt->flag |= GP_SPOINT_TAG;
          gps_array[idx] = gps;
          idx++;
        }
      }
      gps->flag &= ~GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_reset(gps);
    }
  }
  int direction = 0;
  float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * totpoints, "GP Stroke temp 2d points");
  BKE_gpencil_stroke_2d_flat(pt_array, totpoints, points2d, &direction);
  float center[2] = {0.0f, 0.0f};
  for (int i = 0; i < totpoints; i++) {
    center[0] += points2d[i][0];
    center[1] += points2d[i][1];
  }
  mul_v2_fl(center, 1.0f / totpoints);
  const float axis[2] = {1.0f, 0.0f};
  float v1[3];
  for (int i = 0; i < totpoints; i++) {
    float ln = len_v2v2(center, points2d[i]);
    sub_v2_v2v2(v1, points2d[i], center);
    float angle = angle_signed_v2v2(axis, v1);
    if (angle < 0.0f) angle = fabsf(angle);
    else angle = (M_PI * 2.0) - angle;
    tGPencilPointCache *sort_pt = &src_array[i];
    bGPDspoint *pt2 = &pt_array[i];
    copy_v3_v3(&sort_pt->x, &pt2->x);
    sort_pt->pressure = pt2->pressure;
    sort_pt->strength = pt2->strength;
    copy_v4_v4(sort_pt->vert_color, pt2->vert_color);
    sort_pt->gps = gps_array[i];
    sort_pt->factor = (mode == GP_MERGE_STROKE) ? angle : (angle * 100000.0f) + ln;
  }
  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(gps_array);
  MEM_SAFE_FREE(pt_array);
}

/* gpencil_strokes_merge_poll(): active material usable, active layer unlocked and shown */
static bool poll_stroke_merge(const PGOpContext *C)
{
  const MaterialGPencilStyle *st = C->gpd ? slot_style(C->gpd, C->actcol) : NULL;
  if (st == NULL || (st->flag & (GP_MATERIAL_HIDE | GP_MATERIAL_LOCKED))) return false;
  const bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  return gpl != NULL && !(gpl->flag & (GP_LAYER_LOCKED | GP_LAYER_HIDE));
}

/* GPENCIL_OT_stroke_merge: mode (STROKE -1 / POINT 1), back, additive, cyclic, clear_point,
 * clear_stroke. The new stroke takes the active material and the default brush size
 * (gpencil_prepare_stroke uses brush->size; the app's brush lives in the UI). */
static int exec_stroke_merge(PGOpContext *C, const float *p, int n)
{
  const int mode = prop_i(p, n, 0, GP_MERGE_STROKE) == GP_MERGE_POINT ? GP_MERGE_POINT : GP_MERGE_STROKE;
  const bool back = prop_i(p, n, 1, 0) != 0;
  const bool additive = prop_i(p, n, 2, 0) != 0;
  const bool cyclic = prop_i(p, n, 3, 0) != 0;
  const bool clear_point = prop_i(p, n, 4, 0) != 0;
  const bool clear_stroke = prop_i(p, n, 5, 0) != 0;
  const int brush_size = prop_i(p, n, 6, 3);
  bGPdata *gpd = C->gpd;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
  if (gpl == NULL) return PG_OPERATOR_CANCELLED;
  int totstrokes = 0, totpoints = 0;
  LISTBASE_FOREACH (bGPDlayer *, l, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(l) || l->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &l->actframe->strokes) {
      if (!(gps->flag & GP_STROKE_SELECT)) continue;
      totstrokes++;
      for (int i = 0; i < gps->totpoints; i++)
        if (gps->points[i].flag & GP_SPOINT_SELECT) totpoints++;
    }
  }
  if (totpoints == 0) return PG_OPERATOR_CANCELLED;

  tGPencilPointCache *sorted_array = NULL;
  tGPencilPointCache *original_array = MEM_callocN(sizeof(tGPencilPointCache) * totpoints, __func__);
  pgw_merge_points_factor(gpd, mode, totpoints, clear_point, clear_stroke, original_array);
  if (mode == GP_MERGE_STROKE) {
    sorted_array = MEM_callocN(sizeof(tGPencilPointCache) * totpoints, __func__);
    totpoints = gpencil_analyze_strokes(original_array, totstrokes, totpoints, sorted_array);
  }
  else {
    sorted_array = MEM_dupallocN(original_array);
    qsort(sorted_array, totpoints, sizeof(tGPencilPointCache), gpencil_sort_points);
  }

  /* gpencil_prepare_stroke() */
  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, C->cfra, additive ? GP_GETFRAME_ADD_COPY : GP_GETFRAME_ADD_NEW);
  if (gpf == NULL) {
    MEM_SAFE_FREE(original_array);
    MEM_SAFE_FREE(sorted_array);
    return PG_OPERATOR_CANCELLED;
  }
  bGPDstroke *gps = BKE_gpencil_stroke_new(MAX2(C->actcol - 1, 0), totpoints, (short)brush_size);
  gps->flag |= GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_set(gpd, gps);
  if (cyclic) gps->flag |= GP_STROKE_CYCLIC;
  if (back) BLI_addhead(&gpf->strokes, gps);
  else BLI_addtail(&gpf->strokes, gps);

  gpencil_insert_points_to_stroke(gps, sorted_array, totpoints);
  /* gpencil_dissolve_points() */
  if (clear_point || clear_stroke) {
    LISTBASE_FOREACH (bGPDlayer *, l, &gpd->layers) {
      if (!BKE_gpencil_layer_is_editable(l) || l->actframe == NULL) continue;
      LISTBASE_FOREACH_MUTABLE (bGPDstroke *, s, &l->actframe->strokes)
        BKE_gpencil_stroke_delete_tagged_points(gpd, l->actframe, s, s->next, GP_SPOINT_TAG, false, false, 0);
    }
  }
  BKE_gpencil_stroke_geometry_update(gpd, gps);
  MEM_SAFE_FREE(original_array);
  MEM_SAFE_FREE(sorted_array);
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

/* BKE_gpencil_merge_materials_table_get() on the document's slots: r_table[secondary] = primary
 * (-1 = kept), the same tests in the same order. */
static bool pgw_merge_materials_table(const bGPdata *gpd, float hue_threshold, float sat_threshold,
                                      float val_threshold, int *r_table)
{
  bool changed = false;
  const int totcol = gpd->totcol;
  bool *mat_used = MEM_callocN(sizeof(bool) * MAX2(totcol, 1), __func__);
  for (int i = 0; i < totcol; i++) r_table[i] = -1;
  for (int idx_primary = 0; idx_primary < totcol; idx_primary++) {
    const MaterialGPencilStyle *a = slot_style(gpd, idx_primary + 1);
    if (a == NULL) continue;
    for (int idx_secondary = 0; idx_secondary < totcol; idx_secondary++) {
      if (idx_secondary == idx_primary || r_table[idx_secondary] >= 0 || mat_used[idx_secondary]) continue;
      const MaterialGPencilStyle *b = slot_style(gpd, idx_secondary + 1);
      if (b == NULL || (b->flag & GP_MATERIAL_LOCKED)) continue;
      if (a->mode != b->mode) continue;
      if ((a->flag & GP_MATERIAL_STROKE_SHOW) != (b->flag & GP_MATERIAL_STROKE_SHOW)) continue;
      if ((a->flag & GP_MATERIAL_FILL_SHOW) != (b->flag & GP_MATERIAL_FILL_SHOW)) continue;
      if (a->stroke_style != b->stroke_style || a->fill_style != b->fill_style) continue;
      float s_hsv_a[3], s_hsv_b[3], f_hsv_a[3], f_hsv_b[3], col[3];
      zero_v3(s_hsv_a); zero_v3(s_hsv_b); zero_v3(f_hsv_a); zero_v3(f_hsv_b);
      copy_v3_v3(col, a->stroke_rgba); rgb_to_hsv_compat_v(col, s_hsv_a);
      copy_v3_v3(col, b->stroke_rgba); rgb_to_hsv_compat_v(col, s_hsv_b);
      copy_v3_v3(col, a->fill_rgba); rgb_to_hsv_compat_v(col, f_hsv_a);
      copy_v3_v3(col, b->fill_rgba); rgb_to_hsv_compat_v(col, f_hsv_b);
      if (!compare_ff(s_hsv_a[0], s_hsv_b[0], hue_threshold) || !compare_ff(s_hsv_a[1], s_hsv_b[1], sat_threshold) ||
          !compare_ff(s_hsv_a[2], s_hsv_b[2], val_threshold) || !compare_ff(f_hsv_a[0], f_hsv_b[0], hue_threshold) ||
          !compare_ff(f_hsv_a[1], f_hsv_b[1], sat_threshold) || !compare_ff(f_hsv_a[2], f_hsv_b[2], val_threshold) ||
          !compare_ff(a->stroke_rgba[3], b->stroke_rgba[3], val_threshold) ||
          !compare_ff(a->fill_rgba[3], b->fill_rgba[3], val_threshold))
        continue;
      r_table[idx_secondary] = idx_primary;
      changed = true;
      mat_used[idx_primary] = true;
    }
  }
  MEM_freeN(mat_used);
  return changed;
}

/* GPENCIL_OT_stroke_merge_material: hue, sat, val thresholds (0.001). BKE_gpencil_merge_materials:
 * strokes of visible layers move to the primary slot; the merged slots stay (Blender keeps them). */
static int exec_merge_material(PGOpContext *C, const float *p, int n)
{
  bGPdata *gpd = C->gpd;
  if (gpd->totcol == 0) return PG_OPERATOR_CANCELLED;
  int *table = MEM_malloc_arrayN(gpd->totcol, sizeof(int), __func__);
  const bool changed = pgw_merge_materials_table(gpd, prop_f(p, n, 0, 0.001f), prop_f(p, n, 1, 0.001f),
                                                 prop_f(p, n, 2, 0.001f), table);
  if (changed) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl->flag & GP_LAYER_HIDE) continue;
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          const MaterialGPencilStyle *st = slot_style(gpd, gps->mat_nr + 1);
          if (st && ((st->flag & PG_MATERIAL_UNEDITABLE) ||
                     (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (st->flag & GP_MATERIAL_LOCKED))))
            continue;
          if (gps->mat_nr >= 0 && gps->mat_nr < gpd->totcol && table[gps->mat_nr] >= 0) gps->mat_nr = table[gps->mat_nr];
        }
      }
    }
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  MEM_freeN(table);
  return PG_OPERATOR_FINISHED; /* "Nothing to merge" also finishes */
}

/* The last palette made by extract_palette_vertex / material_to_vertex_color, sRGB 0xRRGGBB sorted
 * like BKE_palette_sort_hsv; the app keeps palettes in the UI, which reads it after the call. */
static unsigned int *pgw_palette = NULL;
static int pgw_palette_len = 0;
int pg_wm_last_palette(const unsigned int **r_colors)
{
  if (r_colors) *r_colors = pgw_palette;
  return pgw_palette_len;
}
typedef struct { unsigned int rgb; float h, s, v; } PGWPaletteHSV;
static int pgw_palette_cmp(const void *a1, const void *a2)
{
  const PGWPaletteHSV *ps1 = a1, *ps2 = a2; /* palettecolor_compare_hsv */
  if (ps1->h > ps2->h) return 1;
  if (ps1->h < ps2->h) return -1;
  if (ps1->s > ps2->s) return 1;
  if (ps1->s < ps2->s) return -1;
  if (1.0f - ps1->v > 1.0f - ps2->v) return 1;
  if (1.0f - ps1->v < 1.0f - ps2->v) return -1;
  return 0;
}
static void pgw_palette_key(GHash *table, const float c[3], float range)
{
  float col[3];
  for (int k = 0; k < 3; k++) col[k] = truncf(c[k] * range) / range;
  const unsigned int key = rgb_to_cpack(col[0], col[1], col[2]);
  if (!BLI_ghash_haskey(table, POINTER_FROM_INT(key))) BLI_ghash_insert(table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
}
/* gpencil_extract_palette_from_vertex(). Colours stay sRGB: the app stores vertex colours in sRGB,
 * so Blender's linear-to-sRGB step (BKE_palette_from_hash linear=true) is not needed. */
static bool pgw_extract_palette(bGPdata *gpd, bool selected, int threshold)
{
  const float range = powf(10.0f, (float)threshold);
  GHash *color_table = BLI_ghash_int_new(__func__);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl)) continue;
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        const MaterialGPencilStyle *gp_style = slot_style(gpd, gps->mat_nr + 1);
        if (gp_style == NULL || (gp_style->flag & PG_MATERIAL_UNEDITABLE) ||
            (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (gp_style->flag & GP_MATERIAL_LOCKED)))
          continue;
        if (selected && !(gps->flag & GP_STROKE_SELECT)) continue;
        const bool use_stroke = gp_style->flag & GP_MATERIAL_STROKE_SHOW;
        const bool use_fill = gp_style->flag & GP_MATERIAL_FILL_SHOW;
        if (!use_fill && !use_stroke) continue;
        if (use_stroke && gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE &&
            !(gp_style->flag & GP_MATERIAL_STROKE_PATTERN))
          continue;
        if (use_fill && gp_style->fill_style != GP_MATERIAL_FILL_STYLE_SOLID) continue;
        if (gps->vert_color_fill[3] > 0.0f) pgw_palette_key(color_table, gps->vert_color_fill, range);
        for (int i = 0; i < gps->totpoints; i++) pgw_palette_key(color_table, gps->points[i].vert_color, range);
      }
    }
  }
  const int totpal = (int)BLI_ghash_len(color_table);
  if (totpal > 0) {
    PGWPaletteHSV *arr = MEM_calloc_arrayN(totpal, sizeof(PGWPaletteHSV), __func__);
    GHashIterator gh_iter;
    int t = 0;
    GHASH_ITER (gh_iter, color_table) {
      const unsigned int col = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));
      float r, g, b;
      cpack_to_rgb(col, &r, &g, &b);
      rgb_to_hsv(r, g, b, &arr[t].h, &arr[t].s, &arr[t].v);
      arr[t].rgb = ((unsigned)(r * 255.0f + 0.5f) << 16) | ((unsigned)(g * 255.0f + 0.5f) << 8) | (unsigned)(b * 255.0f + 0.5f);
      t++;
    }
    qsort(arr, totpal, sizeof(PGWPaletteHSV), pgw_palette_cmp);
    MEM_SAFE_FREE(pgw_palette);
    pgw_palette = MEM_malloc_arrayN(totpal, sizeof(unsigned int), __func__);
    for (int i = 0; i < totpal; i++) pgw_palette[i] = arr[i].rgb;
    pgw_palette_len = totpal;
    MEM_freeN(arr);
  }
  BLI_ghash_free(color_table, NULL, NULL);
  return totpal > 0;
}
/* GPENCIL_OT_extract_palette_vertex: selected (false), threshold (1, 1..4) */
static int exec_extract_palette(PGOpContext *C, const float *p, int n)
{
  const int threshold = prop_i(p, n, 1, 1);
  if (threshold < 1 || threshold > 4) return PG_OPERATOR_CANCELLED;
  pgw_palette_len = 0;
  pgw_extract_palette(C->gpd, prop_i(p, n, 0, 0) != 0, threshold);
  return PG_OPERATOR_FINISHED; /* "Unable to find Vertex Information" also finishes; palette length 0 */
}

/* The backend's material slot maker (project_grease_gp_backend.cpp); weak for the host tests. */
bool pg_gp_material_ensure_slot(bGPdata *gpd, int index) __attribute__((weak));

/* get_material_type(): key TSSSSFFFF and the new material's name */
static unsigned int pgw_material_type(const MaterialGPencilStyle *gp_style, bool use_stroke, bool use_fill, char *name)
{
  unsigned int r_i = 0;
  static const char *const names[] = {"", "Line Stroke-Fill", "Dots Stroke-Fill", "Squares Stroke-Fill",
                                      "Line Stroke", "Dots Stroke", "Squares Stroke", "Solid Fill"};
  if (use_stroke) {
    const int base = use_fill ? 1 : 4;
    switch (gp_style->mode) {
      case GP_MATERIAL_MODE_LINE: r_i = base; break;
      case GP_MATERIAL_MODE_DOT: r_i = base + 1; break;
      case GP_MATERIAL_MODE_SQUARE: r_i = base + 2; break;
      default: break;
    }
  }
  else r_i = 7;
  strcpy(name, names[r_i]);
  r_i *= 1e8;
  if (use_stroke) r_i += gp_style->stroke_rgba[3] * 1e7;
  if (use_fill) r_i += gp_style->fill_rgba[3] * 1e3;
  return r_i;
}

/* OBJECT_OT_material_slot_remove_unused: drop slots no stroke uses (one is always kept), remap
 * mat_nr, keep the active slot on the same material when it survives. */
static void pgw_remove_unused_slots(PGOpContext *C)
{
  bGPdata *gpd = C->gpd;
  if (gpd->totcol <= 1) return;
  int *remap = MEM_calloc_arrayN(gpd->totcol, sizeof(int), __func__);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers)
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames)
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes)
        if (gps->mat_nr >= 0 && gps->mat_nr < gpd->totcol) remap[gps->mat_nr] = 1;
  int kept = 0;
  for (int i = 0; i < gpd->totcol; i++) kept += remap[i];
  if (kept == 0) remap[MAX2(C->actcol - 1, 0)] = 1;
  int j = 0;
  for (int i = 0; i < gpd->totcol; i++) {
    if (remap[i]) {
      gpd->mat[j] = gpd->mat[i];
      remap[i] = j++;
    }
    else {
      if (gpd->mat[i]) {
        MEM_SAFE_FREE(gpd->mat[i]->gp_style);
        MEM_freeN(gpd->mat[i]);
      }
      remap[i] = -1;
    }
  }
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers)
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames)
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes)
        if (gps->mat_nr >= 0 && gps->mat_nr < gpd->totcol) gps->mat_nr = remap[gps->mat_nr];
  int act = C->actcol - 1;
  if (act >= 0 && act < gpd->totcol) act = remap[act];
  for (int i = j; i < gpd->totcol; i++) gpd->mat[i] = NULL;
  gpd->totcol = j;
  C->actcol = (act >= 0 ? act : 0) + 1;
  MEM_freeN(remap);
}

/* GPENCIL_OT_material_to_vertex_color: remove (true), palette (true), selected (false),
 * threshold (unused by Blender's exec). Each stroke's material colours move into its vertex colours and the stroke takes a
 * plain material of the same type and alpha (made once per type). */
static int exec_material_to_vertex(PGOpContext *C, const float *p, int n)
{
  bGPdata *gpd = C->gpd;
  const bool remove = prop_i(p, n, 0, 1) != 0;
  const bool palette = prop_i(p, n, 1, 1) != 0;
  const bool selected = prop_i(p, n, 2, 0) != 0;
  if (gpd->totcol == 0 || pg_gp_material_ensure_slot == NULL) return PG_OPERATOR_CANCELLED;
  const int totmat = gpd->totcol;
  struct { unsigned int key; int index; bool used; } *mat_table = MEM_calloc_arrayN(totmat, sizeof(*mat_table), __func__);
  char name[32] = "";
  bool changed = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl)) continue;
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        const MaterialGPencilStyle *src = slot_style(gpd, gps->mat_nr + 1);
        if (src == NULL || (src->flag & PG_MATERIAL_UNEDITABLE) ||
            (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (src->flag & GP_MATERIAL_LOCKED)))
          continue;
        if (selected && !(gps->flag & GP_STROKE_SELECT)) continue;
        /* copy: a new slot may move gpd->mat */
        MaterialGPencilStyle gp_style = *src;
        const bool use_stroke = (gp_style.flag & GP_MATERIAL_STROKE_SHOW) && gp_style.stroke_rgba[3] > 0.0f;
        const bool use_fill = (gp_style.flag & GP_MATERIAL_FILL_SHOW) && gp_style.fill_rgba[3] > 0.0f;
        const bool is_stencil = gp_style.stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE &&
                                (gp_style.flag & GP_MATERIAL_STROKE_PATTERN);
        if (!use_fill && !use_stroke) continue;
        if (use_stroke && gp_style.stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE &&
            !(gp_style.flag & GP_MATERIAL_STROKE_PATTERN))
          continue;
        if (use_fill && gp_style.fill_style != GP_MATERIAL_FILL_STYLE_SOLID) continue;
        if (!is_stencil) {
          const unsigned int key = pgw_material_type(&gp_style, use_stroke, use_fill, name);
          int i;
          bool found = false;
          for (i = 0; i < totmat; i++) {
            if (!mat_table[i].used) break;
            if (mat_table[i].key == key) { found = true; break; }
          }
          if (!found) {
            if (i >= totmat || !pg_gp_material_ensure_slot(gpd, gpd->totcol)) continue;
            Material *ma = gpd->mat[gpd->totcol - 1];
            snprintf(ma->id.name + 2, sizeof(ma->id.name) - 2, "%s", name);
            ma->id.name[0] = 'M';
            ma->id.name[1] = 'A';
            MaterialGPencilStyle *st = ma->gp_style;
            st->mode = gp_style.mode;
            SET_FLAG_FROM_TEST(st->flag, use_stroke, GP_MATERIAL_STROKE_SHOW);
            SET_FLAG_FROM_TEST(st->flag, use_fill, GP_MATERIAL_FILL_SHOW);
            /* BKE_gpencil_material_add: white stroke and fill under the vertex colours */
            copy_v3_fl(st->stroke_rgba, 1.0f);
            copy_v3_fl(st->fill_rgba, 1.0f);
            st->stroke_rgba[3] = gp_style.stroke_rgba[3];
            st->fill_rgba[3] = gp_style.fill_rgba[3];
            mat_table[i].key = key;
            mat_table[i].index = gpd->totcol - 1;
            mat_table[i].used = true;
          }
          gps->mat_nr = mat_table[i].index;
        }
        changed = true;
        copy_v3_v3(gps->vert_color_fill, gp_style.fill_rgba);
        gps->vert_color_fill[3] = 1.0f;
        for (int k = 0; k < gps->totpoints; k++) {
          copy_v3_v3(gps->points[k].vert_color, gp_style.stroke_rgba);
          gps->points[k].vert_color[3] = 1.0f;
        }
      }
    }
  }
  MEM_freeN(mat_table);
  pgw_palette_len = 0;
  if (palette) pgw_extract_palette(gpd, selected, 1); /* Blender passes threshold 1 here */
  if (remove) pgw_remove_unused_slots(C);
  if (changed || remove) BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

/* ---- batch 19 ------------------------------------------------------------------------------ */
/* GPENCIL_OT_lock_layer: lock and hide every material, then unlock / show those the strokes of the
 * active layer's current frame use. */
static int exec_lock_layer(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  bGPdata *gpd = C->gpd;
  if (gpd->totcol == 0) return PG_OPERATOR_CANCELLED;
  for (int i = 1; i <= gpd->totcol; i++) {
    MaterialGPencilStyle *st = slot_style(gpd, i);
    if (st) st->flag |= GP_MATERIAL_LOCKED | GP_MATERIAL_HIDE;
  }
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (BKE_gpencil_layer_is_editable(gpl) && gpl->actframe != NULL && (gpl->flag & GP_LAYER_ACTIVE)) {
      for (bGPDstroke *gps = gpl->actframe->strokes.last; gps; gps = gps->prev) {
        MaterialGPencilStyle *st = slot_style(gpd, gps->mat_nr + 1);
        if (st != NULL) st->flag &= ~(GP_MATERIAL_LOCKED | GP_MATERIAL_HIDE);
      }
    }
  }
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_recalc_geometry: every stroke's triangles and lengths */
static int exec_recalc_geometry(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers)
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames)
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) BKE_gpencil_stroke_geometry_update(C->gpd, gps);
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_stroke_apply_thickness: the active layer's thickness change goes into its strokes */
static int exec_apply_thickness(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  if (gpl == NULL || gpl->frames.first == NULL) return PG_OPERATOR_CANCELLED;
  LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      if ((gps->thickness == 0) && (gpl->line_change == 0)) gps->thickness = gpl->thickness;
      else gps->thickness = gps->thickness + gpl->line_change;
    }
  }
  gpl->thickness = 0.0f;
  gpl->line_change = 0;
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_reset_transform_fill: mode ROTATE 0 / TRANSLATE 1 / SCALE 2 / ALL 3 (default ALL) */
static int exec_reset_transform_fill(PGOpContext *C, const float *p, int n)
{
  const int mode = prop_i(p, n, 0, 3);
  if (mode < 0 || mode > 3) return PG_OPERATOR_CANCELLED;
  bool changed = false;
  PGW_EDITABLE_STROKES_BEGIN (C->gpd, gpl, gps) {
    if (gps->flag & GP_STROKE_SELECT) {
      if (mode == 1 || mode == 3) zero_v2(gps->uv_translation);
      if (mode == 0 || mode == 3) gps->uv_rotation = 0.0f;
      if (mode == 2 || mode == 3) gps->uv_scale = 1.0f;
      BKE_gpencil_stroke_geometry_update(C->gpd, gps);
      changed = true;
    }
  }
  PGW_EDITABLE_STROKES_END
  if (changed) BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_vertex_group_smooth: group (the app's active group), factor (0.5), repeat (1) */
static int exec_vg_smooth(PGOpContext *C, const float *p, int n)
{
  const int def_nr = vg_index(C, p, n);
  const float fac = prop_f(p, n, 1, 0.5f);
  const int repeat = prop_i(p, n, 2, 1);
  if (def_nr < 0 || repeat < 1 || repeat > 10000 || fac < 0.0f || fac > 1.0f) return PG_OPERATOR_CANCELLED;
  bDeformGroup *defgroup = BLI_findlink(&C->gpd->vertex_group_names, def_nr);
  if (defgroup == NULL || (defgroup->flag & DG_LOCK_WEIGHT)) return PG_OPERATOR_CANCELLED; /* "locked" */
  bGPDspoint *pta, *ptb, *ptc;
  MDeformVert *dverta, *dvertb;
  PGW_EDITABLE_STROKES_BEGIN (C->gpd, gpl, gps) {
    if ((gps->totpoints == 0) || (gps->dvert == NULL)) continue;
    for (int s = 0; s < repeat; s++) {
      for (int i = 0; i < gps->totpoints; i++) {
        pta = &gps->points[i > 0 ? i - 1 : i];
        dverta = &gps->dvert[i > 0 ? i - 1 : i];
        ptb = &gps->points[i];
        dvertb = &gps->dvert[i];
        ptc = &gps->points[i + 1 < gps->totpoints ? i + 1 : i];
        float wa = BKE_defvert_find_weight(dverta, def_nr);
        float wb = BKE_defvert_find_weight(dvertb, def_nr);
        const float opfac = line_point_factor_v3(&ptb->x, &pta->x, &ptc->x);
        const float optimal = interpf(wa, wb, opfac);
        MDeformWeight *dw = BKE_defvert_ensure_index(dvertb, def_nr);
        if (dw) {
          dw->weight = interpf(wb, optimal, fac);
          CLAMP(dw->weight, 0.0, 1.0f);
        }
      }
    }
  }
  PGW_EDITABLE_STROKES_END
  BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_vertex_group_normalize_all: lock_active (true), active group (prop 1): each point's
 * weights over all groups sum to 1, locked groups (and the active one with lock_active) kept. */
static int exec_vg_normalize_all(PGOpContext *C, const float *p, int n)
{
  const bool lock_active = prop_i(p, n, 0, 1) != 0;
  const int def_nr = prop_i(p, n, 1, -1);
  bGPdata *gpd = C->gpd;
  const int defbase_tot = BLI_listbase_count(&gpd->vertex_group_names);
  if (defbase_tot == 0) return PG_OPERATOR_CANCELLED;
  PGW_EDITABLE_STROKES_BEGIN (gpd, gpl, gps) {
    if ((gps->totpoints == 0) || (gps->dvert == NULL)) continue;
    for (int i = 0; i < gps->totpoints; i++) {
      float sum = 0.0f, sum_lock = 0.0f, sum_unlock = 0.0f;
      MDeformVert *dvert = &gps->dvert[i];
      int v = 0;
      LISTBASE_FOREACH_INDEX (bDeformGroup *, defgroup, &gpd->vertex_group_names, v) {
        MDeformWeight *dw = BKE_defvert_find_index(dvert, v);
        if (dw == NULL) continue;
        sum += dw->weight;
        if ((defgroup->flag & DG_LOCK_WEIGHT) || (lock_active && v == def_nr)) sum_lock += dw->weight;
        else sum_unlock += dw->weight;
      }
      if ((sum == 1.0f) || (sum_unlock == 0.0f)) continue;
      float fac = MAX2(0, (1.0f - sum_lock) / sum_unlock);
      LISTBASE_FOREACH_INDEX (bDeformGroup *, defgroup, &gpd->vertex_group_names, v) {
        MDeformWeight *dw = BKE_defvert_find_index(dvert, v);
        if (dw == NULL) continue;
        if (!((defgroup->flag & DG_LOCK_WEIGHT) || (lock_active && v == def_nr))) {
          dw->weight *= fac;
          CLAMP(dw->weight, 0.0f, 1.0f);
        }
      }
    }
  }
  PGW_EDITABLE_STROKES_END
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_interpolate_reverse: delete the run of breakdown frames around the current one on
 * each editable layer, when there is a key before and after it. */
static int exec_interpolate_reverse(PGOpContext *C, const float *p, int n)
{
  (void)p; (void)n;
  bGPdata *gpd = C->gpd;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || (gpl->actframe == NULL)) {
      continue;
    }
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_interpolate.c */
    bGPDframe *start_key = NULL;
    bGPDframe *end_key = NULL;
    bGPDframe *gpf, *gpfn;

    /* Only continue if we're currently on a breakdown keyframe */
    if ((gpl->actframe == NULL) || (gpl->actframe->key_type != BEZT_KEYTYPE_BREAKDOWN)) {
      continue;
    }

    /* Search left for "start_key" (i.e. the first breakdown to remove) */
    gpf = gpl->actframe;
    while (gpf) {
      if (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN) {
        /* A breakdown... keep going left */
        start_key = gpf;
        gpf = gpf->prev;
      }
      else {
        /* Not a breakdown (may be a key, or an extreme,
         * or something else that wasn't generated)... stop */
        break;
      }
    }

    /* Search right for "end_key" (i.e. the last breakdown to remove) */
    gpf = gpl->actframe;
    while (gpf) {
      if (gpf->key_type == BEZT_KEYTYPE_BREAKDOWN) {
        /* A breakdown... keep going right */
        end_key = gpf;
        gpf = gpf->next;
      }
      else {
        /* Not a breakdown... stop */
        break;
      }
    }

    /* Did we find anything? */
    /* NOTE: We should only proceed if there's something before/after these extents...
     * Otherwise, there's just an extent of breakdowns with no keys to interpolate between
     */
    if ((start_key && end_key) && ELEM(NULL, start_key->prev, end_key->next) == false) {
      /* Set actframe to the key before start_key, since the keys have been removed now */
      gpl->actframe = start_key->prev;

      /* Free each frame we're removing (except the last one) */
      for (gpf = start_key; gpf && gpf != end_key; gpf = gpfn) {
        gpfn = gpf->next;

        /* free strokes and their associated memory */
        BKE_gpencil_free_strokes(gpf);
        BLI_freelinkN(&gpl->frames, gpf);
      }

      /* Now free the last one... */
      BKE_gpencil_free_strokes(end_key);
      BLI_freelinkN(&gpl->frames, end_key);
    }
/* END VERBATIM */
  }
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_weight_sample: x, y (canvas), radius (canvas units, the brush), group. The weight of
 * the active group at the two nearest weighted points of the strokes under the brush, blended by
 * distance as gpencil_weight_sample_invoke does; read with pg_wm_last_weight_sample. */
static float pgw_sampled_weight = 0.0f;
float pg_wm_last_weight_sample(void) { return pgw_sampled_weight; }
static int exec_weight_sample(PGOpContext *C, const float *p, int n)
{
  if (n < 4) return PG_OPERATOR_CANCELLED;
  const float mouse[2] = {p[0], p[1]};
  const float radius = p[2];
  const int vgroup = prop_i(p, n, 3, -1);
  if (!(radius > 0.0f) || BLI_findlink(&C->gpd->vertex_group_names, vgroup) == NULL) return PG_OPERATOR_CANCELLED;
  float closest_dist[2] = {FLT_MAX, FLT_MAX};
  float closest_weight[2] = {0.0f, 0.0f};
  int closest_count = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (gpl->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      /* ED_gpencil_stroke_check_collision: the brush square against the stroke's bounds */
      if (mouse[0] + radius < gps->boundbox_min[0] || mouse[0] - radius > gps->boundbox_max[0] ||
          mouse[1] + radius < gps->boundbox_min[1] || mouse[1] - radius > gps->boundbox_max[1])
        continue;
      if (gps->dvert == NULL) continue;
      for (int i = 0; i < gps->totpoints; i++) {
        const float dist = len_v2v2(&gps->points[i].x, mouse);
        if ((dist < closest_dist[0]) || (dist < closest_dist[1])) {
          MDeformWeight *dw = BKE_defvert_find_index(&gps->dvert[i], vgroup);
          if (dw == NULL) continue;
          if (dist < closest_dist[0]) {
            closest_dist[1] = closest_dist[0];
            closest_weight[1] = closest_weight[0];
            closest_dist[0] = dist;
            closest_weight[0] = dw->weight;
            closest_count++;
          }
          else if (dist < closest_dist[1]) {
            closest_dist[1] = dist;
            closest_weight[1] = dw->weight;
            closest_count++;
          }
        }
      }
    }
  }
  if (closest_count == 0) return PG_OPERATOR_CANCELLED;
  if (closest_count == 1) pgw_sampled_weight = closest_weight[0];
  else {
    CLAMP_MIN(closest_dist[1], 1e-6f);
    const float dist_sum = closest_dist[0] + closest_dist[1];
    pgw_sampled_weight = (1.0f - closest_dist[0] / dist_sum) * closest_weight[0] +
                         (1.0f - closest_dist[1] / dist_sum) * closest_weight[1];
  }
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_snap_to_cursor: cursor x, y (the app's 2D cursor), use_offset (true): selected strokes
 * move so their first point lands on the cursor, else each selected point goes to it.
 * shortcut: points are taken as canvas positions (no layer transform), as the app draws layers. */
static int exec_snap_to_cursor(PGOpContext *C, const float *p, int n)
{
  if (n < 2) return PG_OPERATOR_CANCELLED;
  const float cursor[3] = {p[0], p[1], 0.0f};
  const bool use_offset = prop_i(p, n, 2, 1) != 0;
  bool changed = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      const MaterialGPencilStyle *st = slot_style(C->gpd, gps->mat_nr + 1);
      if (st && ((st->flag & PG_MATERIAL_UNEDITABLE) || (!(gpl->flag & GP_LAYER_UNLOCK_COLOR) && (st->flag & GP_MATERIAL_LOCKED))))
        continue;
      if ((gps->flag & GP_STROKE_SELECT) == 0 || gps->totpoints == 0) continue;
      if (use_offset) {
        float offset[3];
        sub_v3_v3v3(offset, cursor, &gps->points->x);
        offset[2] = 0.0f;
        for (int i = 0; i < gps->totpoints; i++) add_v3_v3(&gps->points[i].x, offset);
        changed = true;
      }
      else {
        for (int i = 0; i < gps->totpoints; i++) {
          if (gps->points[i].flag & GP_SPOINT_SELECT) {
            gps->points[i].x = cursor[0];
            gps->points[i].y = cursor[1];
            changed = true;
          }
        }
      }
      BKE_gpencil_stroke_geometry_update(C->gpd, gps);
    }
  }
  if (changed) BKE_gpencil_batch_cache_dirty_tag(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* ---- curve edit mode (gpencil_edit_curve.c, rna_GPencil_curve_edit_mode_toggle) ------------------ */
/* The app has no object modes: curve editing also sets the edit-mode flag, which
 * GPENCIL_CURVE_EDIT_SESSIONS_ON needs, and clears it again when it ends. */
static bool pgw_curve_set_editmode = false;

/* GPENCIL_OT_stroke_enter_editcurve_mode: error_threshold (0.1 BU; canvas units here) */
static int exec_enter_editcurve(PGOpContext *C, const float *p, int n)
{
  bGPdata *gpd = C->gpd;
  const float error_threshold = prop_f(p, n, 0, 0.1f);
  if (!(error_threshold >= FLT_MIN) || error_threshold > 100.0f) return PG_OPERATOR_CANCELLED;
  gpd->curve_edit_threshold = error_threshold;
  /* documents made or loaded without BKE_gpencil_data_addnew's defaults have resolution 0: the stroke
   * was rebuilt with one point per curve point (straight segments) */
  if (gpd->curve_edit_resolution < 1) gpd->curve_edit_resolution = GP_DEFAULT_CURVE_RESOLUTION;
  if (!(gpd->curve_edit_corner_angle > 0.0f)) gpd->curve_edit_corner_angle = GP_DEFAULT_CURVE_EDIT_CORNER_ANGLE;
  /* ADAPTED: adaptive resolution samples arc length (Blender units) x resolution; canvas units are
   * pixels (PG_MOD_PIXELS_PER_UNIT per unit), so it made ~1000x the points: fixed points per segment */
  gpd->flag &= ~GP_DATA_CURVE_ADAPTIVE_RESOLUTION;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (gpf == gpl->actframe) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* only allow selected and non-converted strokes to be transformed */
          if ((gps->flag & GP_STROKE_SELECT && gps->editcurve == NULL) ||
              (gps->editcurve != NULL && gps->editcurve->flag & GP_CURVE_NEEDS_STROKE_UPDATE))
          {
            BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps);
            /* Update the selection from the stroke to the curve. */
            BKE_gpencil_editcurve_stroke_sync_selection(gpd, gps, gps->editcurve);
            gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
            BKE_gpencil_stroke_geometry_update(gpd, gps);
          }
        }
      }
    }
  }
  if (!(gpd->flag & GP_DATA_STROKE_EDITMODE)) { gpd->flag |= GP_DATA_STROKE_EDITMODE; pgw_curve_set_editmode = true; }
  gpd->flag |= GP_DATA_CURVE_EDIT_MODE;
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_stroke_editcurve_set_handle_type: type HD_FREE 0 / HD_AUTO 1 / HD_VECT 2 / HD_ALIGN 3 */
static bool poll_curve_edit(const PGOpContext *C)
{
  return C->gpd != NULL && GPENCIL_CURVE_EDIT_SESSIONS_ON(C->gpd) && BKE_gpencil_layer_active_get(C->gpd) != NULL;
}
static int exec_set_handle_type(PGOpContext *C, const float *p, int n)
{
  const int handle_type = prop_i(p, n, 0, HD_AUTO);
  if (!ELEM(handle_type, HD_FREE, HD_AUTO, HD_VECT, HD_ALIGN)) return PG_OPERATOR_CANCELLED;
  bGPdata *gpd = C->gpd;
  /* GP_EDITABLE_CURVES_BEGIN: editable layers, the active frame, strokes with an editcurve */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      bGPDcurve *gpc = gps->editcurve;
      if (gpc == NULL) continue;
      for (int i = 0; i < gpc->tot_curve_points; i++) {
        bGPDcurve_point *gpc_pt = &gpc->curve_points[i];

        if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
          BezTriple *bezt = &gpc_pt->bezt;

          if (bezt->f2 & SELECT) {
            bezt->h1 = handle_type;
            bezt->h2 = handle_type;
          }
          else {
            if (bezt->f1 & SELECT) {
              bezt->h1 = handle_type;
            }
            if (bezt->f3 & SELECT) {
              bezt->h2 = handle_type;
            }
          }
        }
      }

      BKE_gpencil_editcurve_recalculate_handles(gps);
      gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
      BKE_gpencil_stroke_geometry_update(gpd, gps);
    }
  }
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

/* use_curve_edit on / off (rna_GPencil_curve_edit_mode_toggle): on makes the selected strokes'
 * edit curves, off copies the curve selection back to the stroke points. */
int pg_curve_edit_set(bGPdata *gpd, bool on)
{
  if (gpd == NULL) return 0;
  if (on) {
    if (!(gpd->flag & GP_DATA_STROKE_EDITMODE)) { gpd->flag |= GP_DATA_STROKE_EDITMODE; pgw_curve_set_editmode = true; }
    gpd->flag |= GP_DATA_CURVE_EDIT_MODE;
    BKE_gpencil_strokes_selected_update_editcurve(gpd);
  }
  else {
    gpd->flag &= ~GP_DATA_CURVE_EDIT_MODE;
    BKE_gpencil_strokes_selected_sync_selection_editcurve(gpd);
    if (pgw_curve_set_editmode) { gpd->flag &= ~GP_DATA_STROKE_EDITMODE; pgw_curve_set_editmode = false; }
  }
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return 1;
}

/* The edit curves of the editable layers' current frames: 13 floats per curve point (layer,
 * stroke, point, handle1 x / y, point x / y, handle2 x / y, selection f1 | f2 << 1 | f3 << 2, h1, h2,
 * resolution samples of the curve are the stroke itself). Returns the count. */
int pg_curve_points(const bGPdata *gpd, float *out, int capacity)
{
  if (gpd == NULL || !GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) return 0;
  int n = 0, li = 0;
  LISTBASE_FOREACH_INDEX (bGPDlayer *, gpl, &gpd->layers, li) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    int si = 0;
    LISTBASE_FOREACH_INDEX (bGPDstroke *, gps, &gpl->actframe->strokes, si) {
      const bGPDcurve *gpc = gps->editcurve;
      if (gpc == NULL) continue;
      for (int i = 0; i < gpc->tot_curve_points; i++, n++) {
        if (n >= capacity || out == NULL) continue;
        const BezTriple *b = &gpc->curve_points[i].bezt;
        float *o = out + n * 13;
        o[0] = li; o[1] = si; o[2] = i;
        o[3] = b->vec[0][0]; o[4] = b->vec[0][1]; o[5] = b->vec[1][0]; o[6] = b->vec[1][1]; o[7] = b->vec[2][0]; o[8] = b->vec[2][1];
        o[9] = ((b->f1 & SELECT) ? 1 : 0) | ((b->f2 & SELECT) ? 2 : 0) | ((b->f3 & SELECT) ? 4 : 0);
        o[10] = b->h1; o[11] = b->h2; o[12] = 0;
      }
    }
  }
  return n;
}

static bGPDstroke *pgw_curve_stroke(bGPdata *gpd, int layer, int stroke)
{
  bGPDlayer *gpl = BLI_findlink(&gpd->layers, layer);
  if (gpl == NULL || gpl->actframe == NULL) return NULL;
  bGPDstroke *gps = BLI_findlink(&gpl->actframe->strokes, stroke);
  return (gps && gps->editcurve) ? gps : NULL;
}

/* Clicking a curve point (part 0 handle 1, 1 the point, 2 handle 2) as Blender's select operator
 * does in curve edit mode: without extend the rest is deselected; layer -1 deselects all. */
int pg_curve_select(bGPdata *gpd, int layer, int stroke, int point, int part, bool extend)
{
  if (gpd == NULL || !GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd)) return 0;
  if (!extend || layer < 0) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
      if (gpl->actframe == NULL) continue;
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
        bGPDcurve *gpc = gps->editcurve;
        if (gpc == NULL) continue;
        for (int i = 0; i < gpc->tot_curve_points; i++) {
          gpc->curve_points[i].flag &= ~GP_CURVE_POINT_SELECT;
          BEZT_DESEL_ALL(&gpc->curve_points[i].bezt);
        }
        gpc->flag &= ~GP_CURVE_SELECT;
        BKE_gpencil_stroke_editcurve_sync_selection(gpd, gps, gpc);
      }
    }
  }
  if (layer < 0) return 1;
  bGPDstroke *gps = pgw_curve_stroke(gpd, layer, stroke);
  if (gps == NULL || point < 0 || point >= gps->editcurve->tot_curve_points || part < 0 || part > 2) return 0;
  bGPDcurve_point *cp = &gps->editcurve->curve_points[point];
  BezTriple *bezt = &cp->bezt;
  if (part == 1) {
    BEZT_SEL_ALL(bezt);
  }
  else if (part == 0) {
    bezt->f1 |= SELECT;
  }
  else {
    bezt->f3 |= SELECT;
  }
  cp->flag |= GP_CURVE_POINT_SELECT;
  gps->editcurve->flag |= GP_CURVE_SELECT;
  BKE_gpencil_stroke_editcurve_sync_selection(gpd, gps, gps->editcurve);
  return 1;
}

/* Moving the selection in curve edit mode (createTransGPencil_curves with handles visible): a
 * selected point moves with its handles, a lone handle by itself; AUTO / ALIGN points move whole;
 * the handle types are tested (BKE_nurb_bezt_handle_test) and recalculated, and the stroke is
 * rebuilt from its curve. */
int pg_curve_translate(bGPdata *gpd, float dx, float dy)
{
  if (gpd == NULL || !GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd) || !isfinite(dx) || !isfinite(dy)) return 0;
  int moved = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpl->actframe->strokes) {
      bGPDcurve *gpc = gps->editcurve;
      if (gpc == NULL) continue;
      bool need_handle_recalc = false;
      for (int i = 0; i < gpc->tot_curve_points; i++) {
        BezTriple *bezt = &gpc->curve_points[i].bezt;
        if (bezt->hide) continue;
        /* get_bezt_sel_triple_flag() with handles visible */
        short flag = ((bezt->f1 & SELECT) ? 1 : 0) | ((bezt->f2 & SELECT) ? 2 : 0) | ((bezt->f3 & SELECT) ? 4 : 0);
        if ((flag != 7) && (flag & 2) && ELEM(bezt->h1, HD_AUTO, HD_ALIGN) && ELEM(bezt->h2, HD_AUTO, HD_ALIGN)) flag = 7;
        if (flag == 0) continue;
        for (int j = 0; j < 3; j++) {
          if (flag & (1 << j)) { bezt->vec[j][0] += dx; bezt->vec[j][1] += dy; }
        }
        BKE_nurb_bezt_handle_test(bezt, SELECT, true, false);
        need_handle_recalc = true;
      }
      if (need_handle_recalc) {
        BKE_gpencil_editcurve_recalculate_handles(gps);
        /* recalcData_gpencil_strokes */
        gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
        BKE_gpencil_stroke_geometry_update(gpd, gps);
        moved++;
      }
    }
  }
  if (moved) BKE_gpencil_batch_cache_dirty_tag(gpd);
  return moved;
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
    {"GPENCIL_OT_layer_mask_add", RU, poll_active_layer, exec_layer_mask_add},
    {"GPENCIL_OT_layer_mask_remove", RU, poll_active_layer, exec_layer_mask_remove},
    {"GPENCIL_OT_layer_mask_move", RU, poll_active_layer, exec_layer_mask_move},
    {"GPENCIL_OT_stroke_join", RU, poll_active_layer, exec_stroke_join},
    {"GPENCIL_OT_primitive_line", RU, poll_active_layer, exec_prim_line},
    {"GPENCIL_OT_primitive_polyline", RU, poll_active_layer, exec_prim_polyline},
    {"GPENCIL_OT_primitive_box", RU, poll_active_layer, exec_prim_box},
    {"GPENCIL_OT_primitive_circle", RU, poll_active_layer, exec_prim_circle},
    {"GPENCIL_OT_primitive_curve", RU, poll_active_layer, exec_prim_curve},
    {"GPENCIL_OT_stroke_merge", RU, poll_stroke_merge, exec_stroke_merge},
    {"GPENCIL_OT_stroke_merge_material", RU, poll_gpd, exec_merge_material},
    {"GPENCIL_OT_extract_palette_vertex", RU, poll_gpd, exec_extract_palette},
    {"GPENCIL_OT_material_to_vertex_color", RU, poll_gpd, exec_material_to_vertex},
    {"GPENCIL_OT_lock_layer", RU, poll_gpd, exec_lock_layer},
    {"GPENCIL_OT_recalc_geometry", RU, poll_gpd, exec_recalc_geometry},
    {"GPENCIL_OT_stroke_apply_thickness", RU, poll_active_layer, exec_apply_thickness},
    {"GPENCIL_OT_reset_transform_fill", RU, poll_gpd, exec_reset_transform_fill},
    {"GPENCIL_OT_vertex_group_smooth", RU, poll_gpd, exec_vg_smooth},
    {"GPENCIL_OT_vertex_group_normalize_all", RU, poll_gpd, exec_vg_normalize_all},
    {"GPENCIL_OT_interpolate_reverse", RU, poll_gpd, exec_interpolate_reverse},
    {"GPENCIL_OT_snap_to_cursor", RU, poll_gpd, exec_snap_to_cursor},
    {"GPENCIL_OT_stroke_enter_editcurve_mode", RU, poll_active_layer, exec_enter_editcurve},
    {"GPENCIL_OT_stroke_editcurve_set_handle_type", RU, poll_curve_edit, exec_set_handle_type},
    {"GPENCIL_OT_weight_sample", PG_OPTYPE_REGISTER, poll_gpd, exec_weight_sample}, /* brush setting: no undo */
};

const PGOperatorType *pg_wm_stroke_operator_find(const char *idname)
{
  if (idname == NULL) return NULL;
  for (size_t i = 0; i < sizeof(pg_stroke_operators) / sizeof(pg_stroke_operators[0]); i++)
    if (strcmp(pg_stroke_operators[i].idname, idname) == 0) return &pg_stroke_operators[i];
  return NULL;
}
