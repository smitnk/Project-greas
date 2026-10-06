/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_blender_edit8.h. Keyframe types and frame selection follow the action editor
 * operators on Grease Pencil frames (ACTION_OT_keyframe_type / ACTION_OT_clickselect for GP data:
 * bGPDframe.key_type, GP_FRAME_SELECT); the material operators follow GPENCIL_OT_material_* (slot
 * move remaps bGPDstroke.mat_nr like BKE_object_material_remap; isolate hides every other slot,
 * a second isolate shows them again). */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit8.h"
#include "project_grease_blender_edit.h"

static bGPDlayer *pe8_layer(bGPdata *gpd, bGPDlayer *active, int index)
{
  if (index < 0) return active;
  return BLI_findlink(&gpd->layers, index);
}

static MaterialGPencilStyle *pe8_style(bGPdata *gpd, int slot)
{
  if (gpd->mat == NULL || slot < 0 || slot >= gpd->totcol || gpd->mat[slot] == NULL) return NULL;
  return gpd->mat[slot]->gp_style;
}

int pg_gp_frame_set_keytype(bGPdata *gpd, bGPDlayer *active, int framenum, int key_type, int all_layers)
{
  if (gpd == NULL || key_type < BEZT_KEYTYPE_KEYFRAME || key_type > BEZT_KEYTYPE_MOVEHOLD) return 0;
  int changed = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!all_layers && gpl != active) continue;
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (gpf->framenum == framenum && gpf->key_type != key_type) {
        gpf->key_type = (short)key_type;
        changed = 1;
      }
    }
  }
  return changed;
}

int pg_gp_frame_select(bGPdata *gpd, bGPDlayer *active, int framenum, int mode, int all_layers)
{
  if (gpd == NULL) return 0;
  int changed = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!all_layers && gpl != active) continue;
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      const int was = gpf->flag;
      if (gpf->framenum == framenum) {
        if (mode == 1) gpf->flag ^= GP_FRAME_SELECT;
        else gpf->flag |= GP_FRAME_SELECT;
      }
      else if (mode == 0) {
        gpf->flag &= ~GP_FRAME_SELECT;
      }
      if (gpf->flag != was) changed = 1;
    }
  }
  return changed;
}

static int pe8_deselect_frames(bGPdata *gpd, bGPDlayer *active, int all_layers)
{
  int changed = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (!all_layers && gpl != active) continue;
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (gpf->flag & GP_FRAME_SELECT) { gpf->flag &= ~GP_FRAME_SELECT; changed = 1; }
    }
  }
  return changed;
}

int pg_gp_material_move(bGPdata *gpd, int from, int to)
{
  if (gpd == NULL || gpd->mat == NULL || from < 0 || to < 0 || from >= gpd->totcol || to >= gpd->totcol || from == to)
    return 0;
  /* new order: slot `from` moves to `to`, the slots in between shift by one */
  Material *moved = gpd->mat[from];
  if (from < to) memmove(&gpd->mat[from], &gpd->mat[from + 1], sizeof(Material *) * (size_t)(to - from));
  else memmove(&gpd->mat[to + 1], &gpd->mat[to], sizeof(Material *) * (size_t)(from - to));
  gpd->mat[to] = moved;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        const int m = gps->mat_nr;
        if (m == from) gps->mat_nr = (short)to;
        else if (from < to && m > from && m <= to) gps->mat_nr = (short)(m - 1);
        else if (from > to && m >= to && m < from) gps->mat_nr = (short)(m + 1);
      }
    }
  }
  return 1;
}

int pg_gp_material_isolate(bGPdata *gpd, int slot)
{
  MaterialGPencilStyle *active = pe8_style(gpd, slot);
  if (active == NULL) return 0;
  /* gpencil_material_isolate_exec(): if another slot is visible hide them all, else show all */
  bool others_visible = false;
  for (int i = 0; i < gpd->totcol; i++) {
    MaterialGPencilStyle *st = pe8_style(gpd, i);
    if (st != NULL && i != slot && !(st->flag & GP_MATERIAL_HIDE)) others_visible = true;
  }
  int changed = 0;
  for (int i = 0; i < gpd->totcol; i++) {
    MaterialGPencilStyle *st = pe8_style(gpd, i);
    if (st == NULL) continue;
    const int was = st->flag;
    if (i == slot) st->flag &= ~GP_MATERIAL_HIDE;
    else if (others_visible) st->flag |= GP_MATERIAL_HIDE;
    else st->flag &= ~GP_MATERIAL_HIDE;
    if (st->flag != was) changed = 1;
  }
  return changed;
}

int pg_gp_edit8_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args, int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) if (!isfinite(args[i])) return 0;
#define NEED(n) if (arg_count < (n)) return 0
#define I(k) (int)lroundf(args[k])
  int changed = 0;
  switch (command) {
    case PG_EDIT8_CMD_FRAME_KEYTYPE:
      NEED(2);
      changed = pg_gp_frame_set_keytype(gpd, active_layer, I(0), I(1), arg_count > 2 && args[2] != 0.0f);
      break;
    case PG_EDIT8_CMD_FRAME_SELECT:
      NEED(1);
      changed = pg_gp_frame_select(gpd, active_layer, I(0), arg_count > 1 ? I(1) : 0, arg_count > 2 && args[2] != 0.0f);
      break;
    case PG_EDIT8_CMD_FRAME_DESELECT:
      changed = pe8_deselect_frames(gpd, active_layer, arg_count > 0 && args[0] != 0.0f);
      break;
    case PG_EDIT8_CMD_LAYER_BLEND: {
      NEED(2);
      bGPDlayer *gpl = pe8_layer(gpd, active_layer, I(0));
      const int mode = I(1);
      if (gpl == NULL || mode < eGplBlendMode_Regular || mode > eGplBlendMode_Divide) return 0;
      changed = gpl->blend_mode != mode;
      gpl->blend_mode = mode;
      break;
    }
    case PG_EDIT8_CMD_LAYER_TINT: {
      NEED(5);
      bGPDlayer *gpl = pe8_layer(gpd, active_layer, I(0));
      if (gpl == NULL) return 0;
      for (int c = 0; c < 4; c++) {
        const float v = args[1 + c] < 0.0f ? 0.0f : (args[1 + c] > 1.0f ? 1.0f : args[1 + c]);
        if (gpl->tintcolor[c] != v) changed = 1;
        gpl->tintcolor[c] = v;
      }
      break;
    }
    case PG_EDIT8_CMD_LAYER_LINE: {
      NEED(2);
      bGPDlayer *gpl = pe8_layer(gpd, active_layer, I(0));
      if (gpl == NULL) return 0;
      int v = I(1);
      v = v < -300 ? -300 : (v > 300 ? 300 : v); /* RNA "thickness" (line_change) soft range */
      changed = gpl->line_change != v;
      gpl->line_change = (short)v;
      break;
    }
    case PG_EDIT8_CMD_LAYER_PASS: {
      NEED(2);
      bGPDlayer *gpl = pe8_layer(gpd, active_layer, I(0));
      if (gpl == NULL) return 0;
      const int v = I(1) < 0 ? 0 : (I(1) > 255 ? 255 : I(1));
      changed = gpl->pass_index != v;
      gpl->pass_index = v;
      break;
    }
    case PG_EDIT8_CMD_MATERIAL_MOVE:
      NEED(2);
      changed = pg_gp_material_move(gpd, I(0), I(1));
      break;
    case PG_EDIT8_CMD_MATERIAL_FLAGS: {
      NEED(3);
      MaterialGPencilStyle *st = pe8_style(gpd, I(0));
      if (st == NULL) return 0;
      const int was = st->flag;
      if (args[1] != 0.0f) st->flag |= GP_MATERIAL_LOCKED; else st->flag &= ~GP_MATERIAL_LOCKED;
      if (args[2] != 0.0f) st->flag |= GP_MATERIAL_HIDE; else st->flag &= ~GP_MATERIAL_HIDE;
      changed = st->flag != was;
      break;
    }
    case PG_EDIT8_CMD_MATERIAL_SOLO:
      NEED(1);
      changed = pg_gp_material_isolate(gpd, I(0));
      break;
    case PG_EDIT8_CMD_MATERIAL_MODE: {
      NEED(2);
      MaterialGPencilStyle *st = pe8_style(gpd, I(0));
      const int mode = I(1);
      if (st == NULL || mode < GP_MATERIAL_MODE_LINE || mode > GP_MATERIAL_MODE_SQUARE) return 0;
      changed = st->mode != mode;
      st->mode = mode;
      if (arg_count > 2) {
        const int align = I(2) < GP_MATERIAL_FOLLOW_PATH ? GP_MATERIAL_FOLLOW_PATH : (I(2) > GP_MATERIAL_FOLLOW_FIXED ? GP_MATERIAL_FOLLOW_FIXED : I(2));
        changed |= st->alignment_mode != align;
        st->alignment_mode = align;
      }
      if (arg_count > 3) {
        changed |= st->alignment_rotation != args[3];
        st->alignment_rotation = args[3];
      }
      break;
    }
    case PG_EDIT8_CMD_MATERIAL_PASS: {
      NEED(2);
      MaterialGPencilStyle *st = pe8_style(gpd, I(0));
      if (st == NULL) return 0;
      const int v = I(1) < 0 ? 0 : (I(1) > 255 ? 255 : I(1));
      changed = st->index != v;
      st->index = v;
      break;
    }
    case PG_EDIT8_CMD_ONION_FILTER: {
      NEED(1);
      int kt = I(0);
      if (kt < -1 || kt > BEZT_KEYTYPE_MOVEHOLD) return 0;
      const int was_flag = gpd->onion_flag;
      changed = gpd->onion_keytype != kt;
      gpd->onion_keytype = (short)kt;
      if (arg_count > 1) {
        if (args[1] != 0.0f) gpd->onion_flag |= GP_ONION_LOOP; else gpd->onion_flag &= ~GP_ONION_LOOP;
      }
      changed |= gpd->onion_flag != was_flag;
      break;
    }
    case PG_EDIT8_CMD_EASING_PARAMS:
      NEED(2);
      pg_gp_set_elastic(args[0], args[1]);
      return 0;
    default:
      return 0;
  }
#undef I
#undef NEED
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed;
}

int pg_gp_doc_query(const bGPdata *gpd, const bGPDlayer *active, int what, const float *args, int arg_count,
                    float *out, int capacity)
{
  if (gpd == NULL || capacity < 0 || (out == NULL && capacity > 0)) return -1; /* capacity 0: count only */
  int n = 0;
#define PUT(v) do { if (n < capacity) out[n] = (float)(v); n++; } while (0)
  switch (what) {
    case PG_DOC_Q_FRAMES: {
      const bGPDlayer *gpl = (arg_count > 0 && args[0] >= 0) ? BLI_findlink(&gpd->layers, (int)args[0]) : active;
      if (gpl == NULL) return -1;
      LISTBASE_FOREACH (const bGPDframe *, gpf, &gpl->frames) {
        PUT(gpf->framenum);
        PUT(gpf->key_type);
        PUT((gpf->flag & GP_FRAME_SELECT) ? 1 : 0);
      }
      break;
    }
    case PG_DOC_Q_LAYER: {
      const bGPDlayer *gpl = (arg_count > 0 && args[0] >= 0) ? BLI_findlink(&gpd->layers, (int)args[0]) : active;
      if (gpl == NULL) return -1;
      PUT(gpl->blend_mode);
      for (int c = 0; c < 4; c++) PUT(gpl->tintcolor[c]);
      PUT(gpl->line_change);
      PUT(gpl->pass_index);
      break;
    }
    case PG_DOC_Q_MATERIAL: {
      const int slot = arg_count > 0 ? (int)args[0] : -1;
      if (gpd->mat == NULL || slot < 0 || slot >= gpd->totcol || gpd->mat[slot] == NULL || gpd->mat[slot]->gp_style == NULL)
        return -1;
      const MaterialGPencilStyle *st = gpd->mat[slot]->gp_style;
      PUT(st->mode);
      PUT(st->alignment_mode);
      PUT(st->alignment_rotation);
      PUT((st->flag & GP_MATERIAL_LOCKED) ? 1 : 0);
      PUT((st->flag & GP_MATERIAL_HIDE) ? 1 : 0);
      PUT(st->index);
      /* 6..21: gradient fill and material options (pg_gp_material_gradient_set / options_set) */
      PUT(st->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT ? 1 : 0);
      PUT(st->gradient_type);
      for (int c = 0; c < 4; c++) PUT(st->mix_rgba[c]);
      PUT(st->mix_factor);
      PUT(st->texture_angle);
      PUT(st->texture_scale[0]); PUT(st->texture_scale[1]);
      PUT(st->texture_offset[0]); PUT(st->texture_offset[1]);
      PUT((st->flag & GP_MATERIAL_FLIP_FILL) ? 1 : 0);
      PUT((st->flag & GP_MATERIAL_IS_STROKE_HOLDOUT) ? 1 : 0);
      PUT((st->flag & GP_MATERIAL_IS_FILL_HOLDOUT) ? 1 : 0);
      PUT((st->flag & GP_MATERIAL_DISABLE_STENCIL) ? 1 : 0);
      break;
    }
    case PG_DOC_Q_SELECTED_STROKES: {
      const bGPDframe *gpf = active ? active->actframe : NULL;
      if (gpf == NULL) break;
      int index = 0;
      LISTBASE_FOREACH (const bGPDstroke *, gps, &gpf->strokes) {
        if (gps->flag & GP_STROKE_SELECT) PUT(index);
        index++;
      }
      break;
    }
    case PG_DOC_Q_ONION:
      PUT(gpd->onion_keytype);
      PUT((gpd->onion_flag & GP_ONION_LOOP) ? 1 : 0);
      PUT((gpd->flag & GP_DATA_STROKE_MULTIEDIT) ? 1 : 0);
      break;
    default:
      return -1;
  }
#undef PUT
  return n;
}
