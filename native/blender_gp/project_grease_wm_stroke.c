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

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit2.h"
#include "project_grease_blender_edit3.h"
#include "project_grease_blender_edit4.h"
#include "project_grease_blender_edit6.h"
#include "project_grease_blender_select.h"
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
};

const PGOperatorType *pg_wm_stroke_operator_find(const char *idname)
{
  if (idname == NULL) return NULL;
  for (size_t i = 0; i < sizeof(pg_stroke_operators) / sizeof(pg_stroke_operators[0]); i++)
    if (strcmp(pg_stroke_operators[i].idname, idname) == 0) return &pg_stroke_operators[i];
  return NULL;
}
