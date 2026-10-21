/* SPDX-License-Identifier: GPL-2.0-or-later
 * Layer operators of gpencil_data.c (Blender 3.6.23) on the operator layer (project_grease_wm.h).
 * Exec bodies follow Blender's; the context's active layer is BKE_gpencil_layer_active_get(). */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "DNA_gpencil_legacy_types.h"
#include "BKE_gpencil_legacy.h"

#include "BKE_gpencil_geom_legacy.h"
#include "DNA_material_types.h"
#include "project_grease_blender_edit7.h"
#include "project_grease_wm.h"

static int prop_i(const float *props, int n, int i, int def)
{
  return (i < n && isfinite(props[i])) ? (int)lroundf(props[i]) : def;
}
static bool poll_gpd(const PGOpContext *C) { return C->gpd != NULL; }
/* gpencil_active_layer_poll */
static bool poll_active_layer(const PGOpContext *C)
{
  return C->gpd != NULL && BKE_gpencil_layer_active_get(C->gpd) != NULL;
}

/* GPENCIL_OT_layer_add: new "GP_Layer" after the active one, made active, with a key at cfra.
 * shortcut: new_layer_name is not a property here (props are numbers); rename after adding. */
static int exec_layer_add(PGOpContext *C, const float *props, int n)
{
  (void)props; (void)n;
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(C->gpd, "GP_Layer", true, false);
  if (gpl != NULL) gpl->actframe = BKE_gpencil_layer_frame_get(gpl, C->cfra, GP_GETFRAME_ADD_NEW);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_layer_remove: locked layers stay; the one below (else above) becomes active. */
static int exec_layer_remove(PGOpContext *C, const float *props, int n)
{
  (void)props; (void)n;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  if (gpl->flag & GP_LAYER_LOCKED) return PG_OPERATOR_CANCELLED; /* "Cannot delete locked layers" */
  /* shortcut: the app always keeps one layer (Blender allows none); allow it with an empty-document state */
  if (gpl->prev == NULL && gpl->next == NULL) return PG_OPERATOR_CANCELLED;
  BKE_gpencil_layer_active_set(C->gpd, gpl->prev ? gpl->prev : gpl->next);
  BKE_gpencil_layer_delete(C->gpd, gpl);
  BKE_gpencil_layer_mask_sort_all(C->gpd);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_layer_move: type UP (-1) / DOWN (1); up is towards the top of the stack (list end). */
static int exec_layer_move(PGOpContext *C, const float *props, int n)
{
  const int type = prop_i(props, n, 0, -1);
  if (type != -1 && type != 1) return PG_OPERATOR_CANCELLED;
  if (BLI_listbase_link_move(&C->gpd->layers, BKE_gpencil_layer_active_get(C->gpd), type * -1)) {
    BKE_gpencil_layer_mask_sort_all(C->gpd);
  }
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_layer_duplicate: mode ALL (0, above, with strokes) / EMPTY (1, below, keys only) */
static int exec_layer_duplicate(PGOpContext *C, const float *props, int n)
{
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(C->gpd);
  const bool dup_strokes = prop_i(props, n, 0, 0) == 0;
  bGPDlayer *new_layer = BKE_gpencil_layer_duplicate(gpl, true, dup_strokes);
  if (dup_strokes) BLI_insertlinkafter(&C->gpd->layers, gpl, new_layer);
  else BLI_insertlinkbefore(&C->gpd->layers, gpl, new_layer);
  BLI_uniquename(&C->gpd->layers, new_layer, "GP_Layer", '.', offsetof(bGPDlayer, info), sizeof(new_layer->info));
  BKE_gpencil_layer_active_set(C->gpd, new_layer);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_layer_merge: mode ACTIVE (0) merges the active layer into the one below.
 * shortcut: mode ALL (flatten) is not ported yet -> CANCELLED; port gpencil_merge_layer_exec's ALL branch. */
static int exec_layer_merge(PGOpContext *C, const float *props, int n)
{
  const int mode = prop_i(props, n, 0, 0);
  if (mode == 1) {
    /* ALL: every layer combined into the active one, layers above drawn over it, layers below
     * under it, then the layer settings reset (gpencil_merge_layer_exec, GP_LAYER_MERGE_ALL).
     * shortcut: the layers below are merged by moving the active layer down into them, so the
     * result keeps the bottom layer's modifiers / effects (Blender keeps the active layer's
     * settings, reset to defaults as here); port ED_gpencil_layer_merge(reverse) to match exactly. */
    bGPDlayer *active = BKE_gpencil_layer_active_get(C->gpd), *below = NULL;
    if (active == NULL || (active->prev == NULL && active->next == NULL)) return PG_OPERATOR_CANCELLED;
    char name[sizeof(active->info)];
    STRNCPY(name, active->info);
    while (C->gpd->layers.last != active) {
      if (!pg_gp_layer_merge_down(C->gpd, C->gpd->layers.last, &below)) return PG_OPERATOR_CANCELLED;
    }
    while (active->prev != NULL) {
      if (!pg_gp_layer_merge_down(C->gpd, active, &below) || below == NULL) return PG_OPERATOR_CANCELLED;
      active = below;
    }
    STRNCPY(active->info, name);
    active->blend_mode = eGplBlendMode_Regular;
    active->flag &= ~(GP_LAYER_LOCKED | GP_LAYER_HIDE);
    active->flag |= GP_LAYER_USE_LIGHTS;
    active->onion_flag |= GP_LAYER_ONIONSKIN;
    BKE_gpencil_layer_active_set(C->gpd, active);
    LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) BKE_gpencil_layer_mask_cleanup(C->gpd, gpl);
    return PG_OPERATOR_FINISHED;
  }
  if (mode != 0) return PG_OPERATOR_CANCELLED;
  bGPDlayer *active = BKE_gpencil_layer_active_get(C->gpd), *below = NULL;
  if (active->prev == NULL) return PG_OPERATOR_CANCELLED; /* "No layers to merge" */
  if (!pg_gp_layer_merge_down(C->gpd, active, &below) || below == NULL) return PG_OPERATOR_CANCELLED;
  BKE_gpencil_layer_active_set(C->gpd, below);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) BKE_gpencil_layer_mask_cleanup(C->gpd, gpl);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_layer_active: layer index (bottom = 0). */
static int exec_layer_active(PGOpContext *C, const float *props, int n)
{
  bGPDlayer *gpl = BLI_findlink(&C->gpd->layers, prop_i(props, n, 0, -1));
  if (gpl == NULL) return PG_OPERATOR_CANCELLED;
  BKE_gpencil_layer_active_set(C->gpd, gpl);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_layer_change: layer index, -1 = a new "GP_Layer" (made active) */
static int exec_layer_change(PGOpContext *C, const float *props, int n)
{
  const int layer_num = prop_i(props, n, 0, -1);
  bGPDlayer *gpl;
  if (layer_num == -1) {
    gpl = BKE_gpencil_layer_addnew(C->gpd, "GP_Layer", true, false);
    if (gpl != NULL) gpl->actframe = BKE_gpencil_layer_frame_get(gpl, C->cfra, GP_GETFRAME_ADD_NEW);
  }
  else {
    gpl = BLI_findlink(&C->gpd->layers, layer_num);
    if (gpl == NULL) return PG_OPERATOR_CANCELLED; /* "Cannot change to non-existent layer" */
  }
  if (gpl == NULL) return PG_OPERATOR_CANCELLED;
  BKE_gpencil_layer_active_set(C->gpd, gpl);
  return PG_OPERATOR_FINISHED;
}

/* The backend's material slot maker (project_grease_gp_backend.cpp); weak for the host tests. */
bool pg_gp_material_ensure_slot(bGPdata *gpd, int index) __attribute__((weak));

/* GPENCIL_OT_image_to_grease_pencil: width, height, size (canvas units per pixel), x, y (top-left
 * on the canvas), mask, then the pixels as r, g, b, a (0..1, top row first). Blender's
 * image_to_gpencil_exec / BKE_gpencil_from_image into an "Image Layer" of this drawing (Blender makes
 * a new object): a Square-dots material, one stroke per pixel row with the pixel colours, and the
 * near-transparent points deleted. */
static int exec_image_to_gp(PGOpContext *C, const float *props, int n)
{
  const int img_x = prop_i(props, n, 0, 0), img_y = prop_i(props, n, 1, 0);
  const float size = n > 2 ? props[2] : 0.0f;
  const float ox = n > 3 ? props[3] : 0.0f, oy = n > 4 ? props[4] : 0.0f;
  const bool mask = prop_i(props, n, 5, 0) != 0;
  if (img_x < 1 || img_y < 1 || !(size > 0.0f) || n < 6 + img_x * img_y * 4 || pg_gp_material_ensure_slot == NULL)
    return PG_OPERATOR_CANCELLED;
  const float *px = props + 6;
  bGPdata *gpd = C->gpd;
  /* BKE_gpencil_object_material_new(..., "Image Material"): a Square material */
  const int slot = gpd->totcol;
  if (!pg_gp_material_ensure_slot(gpd, slot)) return PG_OPERATOR_CANCELLED;
  Material *ma = gpd->mat[slot];
  BLI_strncpy(ma->id.name + 2, "Image Material", sizeof(ma->id.name) - 2);
  ma->id.name[0] = 'M'; ma->id.name[1] = 'A';
  ma->gp_style->mode = GP_MATERIAL_MODE_SQUARE;
  ma->gp_style->stroke_rgba[0] = ma->gp_style->stroke_rgba[1] = ma->gp_style->stroke_rgba[2] = 1.0f;
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "Image Layer", true, false);
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, C->cfra);
  if (gpl == NULL || gpf == NULL) return PG_OPERATOR_CANCELLED;
  gpl->actframe = gpf;
  for (int row = 0; row < img_y; row++) {
    /* Blender's size * 1000 is its pixel factor; the app's thickness is canvas units: one pixel wide */
    bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, slot, img_x, (short)fminf(fmaxf(1.0f, size), 32000.0f), false);
    for (int col = 0; col < img_x; col++) {
      const float *color = px + (row * img_x + col) * 4;
      bGPDspoint *pt = &gps->points[col];
      pt->pressure = 1.0f;
      pt->x = ox + col * size;
      pt->y = oy + row * size; /* Blender puts rows on z (front view); the canvas is x / y */
      if (!mask) {
        copy_v3_v3(pt->vert_color, color);
        pt->vert_color[3] = 1.0f;
        pt->strength = color[3];
      }
      else {
        zero_v3(pt->vert_color);
        pt->vert_color[3] = 1.0f;
        pt->strength = 1.0f - color[3];
      }
      if (pt->strength < 0.03f) {
        gps->flag |= GP_STROKE_SELECT;
        pt->flag |= GP_SPOINT_SELECT;
      }
    }
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }
  LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
    BKE_gpencil_stroke_delete_tagged_points(gpd, gpf, gps, gps->next, GP_SPOINT_SELECT, false, false, 0);
  }
  BKE_gpencil_batch_cache_dirty_tag(gpd);
  return PG_OPERATOR_FINISHED;
}

static const PGOperatorType pg_layer_operators[] = {
    {"GPENCIL_OT_image_to_grease_pencil", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_gpd, exec_image_to_gp},
    {"GPENCIL_OT_layer_change", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_gpd, exec_layer_change},
    {"GPENCIL_OT_layer_add", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_gpd, exec_layer_add},
    {"GPENCIL_OT_layer_remove", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_active_layer, exec_layer_remove},
    {"GPENCIL_OT_layer_move", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_active_layer, exec_layer_move},
    {"GPENCIL_OT_layer_duplicate", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_active_layer, exec_layer_duplicate},
    {"GPENCIL_OT_layer_merge", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_active_layer, exec_layer_merge},
    {"GPENCIL_OT_layer_active", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_active_layer, exec_layer_active},
};

const PGOperatorType *pg_wm_layer_operator_find(const char *idname)
{
  if (idname == NULL) return NULL;
  for (size_t i = 0; i < sizeof(pg_layer_operators) / sizeof(pg_layer_operators[0]); i++)
    if (strcmp(pg_layer_operators[i].idname, idname) == 0) return &pg_layer_operators[i];
  return NULL;
}
