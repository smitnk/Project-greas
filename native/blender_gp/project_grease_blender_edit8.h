/* SPDX-License-Identifier: GPL-2.0-or-later
 * Batch 21 document-state commands (dispatched after edit7) and the document query used by the
 * editor to read back state that has no other accessor. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;
enum {
  PG_EDIT8_CMD_FRAME_KEYTYPE = 103,  /* args: frame number, key type (BEZT_KEYTYPE_*), all layers */
  PG_EDIT8_CMD_FRAME_SELECT = 104,   /* args: frame number, mode (0 set, 1 toggle, 2 add), all layers */
  PG_EDIT8_CMD_FRAME_DESELECT = 105, /* args: all layers */
  PG_EDIT8_CMD_LAYER_BLEND = 106,    /* args: layer, blend mode (eGplBlendMode_*) */
  PG_EDIT8_CMD_LAYER_TINT = 107,     /* args: layer, r, g, b, factor */
  PG_EDIT8_CMD_LAYER_LINE = 108,     /* args: layer, line_change (px) */
  PG_EDIT8_CMD_MATERIAL_MOVE = 109,  /* args: from, to (slots reordered, strokes remapped) */
  PG_EDIT8_CMD_MATERIAL_FLAGS = 110, /* args: slot, locked, hidden */
  PG_EDIT8_CMD_MATERIAL_SOLO = 111,  /* args: slot (GPENCIL_OT_material_isolate, visibility) */
  PG_EDIT8_CMD_MATERIAL_MODE = 112,  /* args: slot, mode (line/dots/squares), alignment, rotation */
  PG_EDIT8_CMD_ONION_FILTER = 113,   /* args: onion_keytype (-1 all), loop */
  PG_EDIT8_CMD_LAYER_PASS = 114,     /* args: layer, pass index */
  PG_EDIT8_CMD_MATERIAL_PASS = 115,  /* args: slot, pass index */
  PG_EDIT8_CMD_EASING_PARAMS = 116,  /* args: elastic amplitude, period (no document change) */
};
enum {
  PG_DOC_Q_FRAMES = 0,   /* args: layer (-1 active) -> [framenum, key_type, selected] per frame */
  PG_DOC_Q_LAYER = 1,    /* args: layer -> [blend, tint r, g, b, factor, line_change, pass_index] */
  PG_DOC_Q_MATERIAL = 2, /* args: slot -> [mode, alignment, rotation, locked, hidden, pass_index] */
  PG_DOC_Q_ONION = 3,    /* -> [onion_keytype, loop, multiedit] */
  PG_DOC_Q_SELECTED_STROKES = 4, /* -> indices of the active frame's strokes with GP_STROKE_SELECT */
};
int pg_gp_frame_set_keytype(struct bGPdata *gpd, struct bGPDlayer *active, int framenum, int key_type, int all_layers);
int pg_gp_frame_select(struct bGPdata *gpd, struct bGPDlayer *active, int framenum, int mode, int all_layers);
int pg_gp_material_move(struct bGPdata *gpd, int from, int to);
int pg_gp_material_isolate(struct bGPdata *gpd, int slot);
int pg_gp_edit8_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);
/* Writes up to `capacity` floats; returns the count, or -1 for a bad query. */
int pg_gp_doc_query(const struct bGPdata *gpd, const struct bGPDlayer *active_layer, int what,
                    const float *args, int arg_count, float *out, int capacity);
#ifdef __cplusplus
}
#endif
