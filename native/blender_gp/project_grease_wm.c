/* SPDX-License-Identifier: GPL-2.0-or-later
 * Operator layer, see project_grease_wm.h. Poll and exec bodies follow gpencil_edit.c
 * (Blender 3.6.23); exec bodies live with the ported code they call. */
#include <math.h>
#include <string.h>

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit4.h"
#include "project_grease_blender_select.h"
#include "project_grease_blender_edit2.h"
#include "project_grease_blender_edit3.h"
#include "project_grease_wm.h"

typedef struct PGOperatorType {
  const char *idname;
  int flag;
  bool (*poll)(const PGOpContext *C);
  int (*exec)(PGOpContext *C, const float *props, int n);
} PGOperatorType;

static int prop_i(const float *props, int n, int i, int def)
{
  return (i < n && isfinite(props[i])) ? (int)lroundf(props[i]) : def;
}

/* gpencil_stroke_edit_poll: CTX_DATA_COUNT(C, editable_gpencil_strokes) != 0 */
static bool poll_stroke_edit(const PGOpContext *C)
{
  if (C->gpd == NULL) return false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers) {
    if (BKE_gpencil_layer_is_editable(gpl) && gpl->actframe && !BLI_listbase_is_empty(&gpl->actframe->strokes))
      return true;
  }
  return false;
}

/* GPENCIL_OT_copy (no flags: copying is not an undo step) */
static int exec_copy(PGOpContext *C, const float *props, int n)
{
  (void)props; (void)n;
  pg_gp_copy(C->gpd, NULL);
  return PG_OPERATOR_FINISHED;
}

/* gpencil_strokes_paste_poll: GP data and a non-empty copy buffer */
static bool poll_paste(const PGOpContext *C)
{
  const ListBase *buf = pg_gp_clipboard_strokes();
  return C->gpd != NULL && buf && !BLI_listbase_is_empty(buf);
}

/* GPENCIL_OT_paste: props type (1 ACTIVE, -1 LAYER; default ACTIVE), paste_back (0) */
static int exec_paste(PGOpContext *C, const float *props, int n)
{
  const int type = prop_i(props, n, 0, 1) == -1 ? -1 : 1;
  const bool on_back = prop_i(props, n, 1, 0) != 0;
  return pg_gp_paste_ex(C->gpd, C->active_layer, C->cfra, type, on_back, C->autokey) ?
             PG_OPERATOR_FINISHED :
             PG_OPERATOR_CANCELLED;
}

/* gpencil_actframe_delete_exec: the active layer's frame shown at cfra */
static int delete_active_frame(PGOpContext *C)
{
  bGPDlayer *gpl = C->active_layer;
  bGPDframe *gpf = gpl ? BKE_gpencil_layer_frame_get(gpl, C->cfra, GP_GETFRAME_USE_PREV) : NULL;
  if (gpl == NULL || gpf == NULL) return PG_OPERATOR_CANCELLED;
  BKE_gpencil_layer_frame_delete(gpl, gpf);
  return PG_OPERATOR_FINISHED;
}

/* GPENCIL_OT_delete: prop type GP_DELETEOP_POINTS (0, default), STROKES (1), FRAME (2) */
static int exec_delete(PGOpContext *C, const float *props, int n)
{
  switch (prop_i(props, n, 0, 0)) {
    case 1: return pg_gp_edit_delete_strokes(C->gpd, NULL) ? PG_OPERATOR_FINISHED : PG_OPERATOR_CANCELLED;
    case 2: return delete_active_frame(C);
    default: return pg_gp_edit_delete_points(C->gpd, NULL) ? PG_OPERATOR_FINISHED : PG_OPERATOR_CANCELLED;
  }
}

#define FINISHED_IF(x) ((x) ? PG_OPERATOR_FINISHED : PG_OPERATOR_CANCELLED)

/* GPENCIL_OT_duplicate */
static int exec_duplicate(PGOpContext *C, const float *props, int n)
{
  (void)props; (void)n;
  return FINISHED_IF(pg_gp_duplicate(C->gpd, NULL));
}
/* GPENCIL_OT_dissolve: prop type POINTS (0, default), BETWEEN (1), UNSELECT (2) */
static int exec_dissolve(PGOpContext *C, const float *props, int n)
{
  const int type = prop_i(props, n, 0, PG_DISSOLVE_POINTS);
  if (type < PG_DISSOLVE_POINTS || type > PG_DISSOLVE_UNSELECT) return PG_OPERATOR_CANCELLED;
  return FINISHED_IF(pg_gp_dissolve(C->gpd, NULL, type));
}
/* GPENCIL_OT_stroke_join: props type (JOIN 0; JOINCOPY 1 is not ported), leave_gaps */
static int exec_join(PGOpContext *C, const float *props, int n)
{
  if (prop_i(props, n, 0, 0) != 0) return PG_OPERATOR_CANCELLED; /* shortcut: JOINCOPY not ported; port it when the UI offers it */
  return FINISHED_IF(pg_gp_join(C->gpd, NULL, prop_i(props, n, 1, 0)));
}
/* GPENCIL_OT_move_to_layer: prop layer (target index) */
static int exec_move_to_layer(PGOpContext *C, const float *props, int n)
{
  const int target = prop_i(props, n, 0, -1);
  if (target < 0 || C->active_layer == NULL) return PG_OPERATOR_CANCELLED;
  return FINISHED_IF(pg_gp_move_to_layer(C->gpd, C->active_layer, target));
}
/* gpencil_select_poll: GP data present */
static bool poll_select(const PGOpContext *C) { return C->gpd != NULL; }
/* GPENCIL_OT_select_all: prop action TOGGLE (0, default), SELECT 1, DESELECT 2, INVERT 3 */
static int exec_select_all(PGOpContext *C, const float *props, int n)
{
  const int action = prop_i(props, n, 0, PG_SEL_TOGGLE);
  if (action < PG_SEL_TOGGLE || action > PG_SEL_INVERT) return PG_OPERATOR_CANCELLED;
  pg_gp_select_all(C->gpd, action, NULL);
  return PG_OPERATOR_FINISHED; /* gpencil_select_all_exec finishes even when nothing changed */
}

/* GPENCIL_OT_stroke_arrange: prop direction in Blender's values UP -1, DOWN 1, TOP 2, BOTTOM 3 */
static int exec_arrange(PGOpContext *C, const float *props, int n)
{
  int dir;
  switch (prop_i(props, n, 0, -1)) {
    case -1: dir = PG_ARRANGE_UP; break;
    case 1: dir = PG_ARRANGE_DOWN; break;
    case 2: dir = PG_ARRANGE_TOP; break;
    case 3: dir = PG_ARRANGE_BOTTOM; break;
    default: return PG_OPERATOR_CANCELLED;
  }
  return FINISHED_IF(pg_gp_stroke_arrange(C->gpd, NULL, dir));
}
/* GPENCIL_OT_stroke_split */
static int exec_split(PGOpContext *C, const float *props, int n)
{
  (void)props; (void)n;
  return FINISHED_IF(pg_gp_split(C->gpd, NULL));
}
/* GPENCIL_OT_extrude */
static int exec_extrude(PGOpContext *C, const float *props, int n)
{
  (void)props; (void)n;
  return FINISHED_IF(pg_gp_extrude(C->gpd, NULL));
}
/* GPENCIL_OT_stroke_separate: prop mode POINT 0, STROKE 1, LAYER 2. Blender moves the result to a
 * new object; the app has one drawing, so it goes to a new layer of it. */
static int exec_separate(PGOpContext *C, const float *props, int n)
{
  const int mode = prop_i(props, n, 0, 0);
  if (mode == 2) return FINISHED_IF(C->active_layer && pg_gp_separate_to_layer(C->gpd, C->active_layer));
  if (mode < 0 || mode > 1) return PG_OPERATOR_CANCELLED;
  return FINISHED_IF(pg_gp_stroke_separate(C->gpd, NULL, mode));
}
/* gpencil_add_poll / active layer: GPENCIL_OT_blank_frame_add, prop all_layers */
static bool poll_active_layer(const PGOpContext *C) { return C->gpd && C->active_layer; }
static int exec_blank_frame(PGOpContext *C, const float *props, int n)
{
  bool any = false;
  if (prop_i(props, n, 0, 0)) {
    LISTBASE_FOREACH (bGPDlayer *, gpl, &C->gpd->layers)
      if (BKE_gpencil_layer_is_editable(gpl)) any |= pg_gp_blank_frame_add(C->gpd, gpl, C->cfra) != 0;
  }
  else {
    any = pg_gp_blank_frame_add(C->gpd, C->active_layer, C->cfra) != 0;
  }
  return FINISHED_IF(any);
}

/* ---- Vertex Paint menu (gpencil_vertex_ops.c): first prop mode STROKE 0 / FILL 1 / BOTH 2. As
 * in Blender, nothing selected means every editable stroke, and each exec finishes. */
static bool poll_vertex(const PGOpContext *C) { return C->gpd != NULL; }
static bool vmode(const float *props, int n, int *r_mode)
{
  *r_mode = prop_i(props, n, 0, 2);
  return *r_mode >= 0 && *r_mode <= 2;
}
static float prop_f(const float *props, int n, int i, float def)
{
  return (i < n && isfinite(props[i])) ? props[i] : def;
}
/* GPENCIL_OT_vertex_color_set: mode, factor, then the brush colour r g b (scene linear) */
static int exec_vcolor_set(PGOpContext *C, const float *props, int n)
{
  int m;
  if (!vmode(props, n, &m) || n < 5) return PG_OPERATOR_CANCELLED;
  const float rgb[3] = {prop_f(props, n, 2, 0), prop_f(props, n, 3, 0), prop_f(props, n, 4, 0)};
  pg_gp_vcolor_set(C->gpd, NULL, m, rgb, prop_f(props, n, 1, 1.0f));
  return PG_OPERATOR_FINISHED;
}
static int exec_vcolor_invert(PGOpContext *C, const float *props, int n)
{
  int m;
  if (!vmode(props, n, &m)) return PG_OPERATOR_CANCELLED;
  pg_gp_vcolor_invert(C->gpd, NULL, m);
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_vertex_color_levels: mode, offset (0), gain (1) */
static int exec_vcolor_levels(PGOpContext *C, const float *props, int n)
{
  int m;
  if (!vmode(props, n, &m)) return PG_OPERATOR_CANCELLED;
  pg_gp_vcolor_levels(C->gpd, NULL, m, prop_f(props, n, 1, 0.0f), prop_f(props, n, 2, 1.0f));
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_vertex_color_hsv: mode, h (0.5), s (1), v (1) */
static int exec_vcolor_hsv(PGOpContext *C, const float *props, int n)
{
  int m;
  if (!vmode(props, n, &m)) return PG_OPERATOR_CANCELLED;
  pg_gp_vcolor_hsv(C->gpd, NULL, m, prop_f(props, n, 1, 0.5f), prop_f(props, n, 2, 1.0f), prop_f(props, n, 3, 1.0f));
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_vertex_color_brightness_contrast: mode, brightness (0), contrast (0) */
static int exec_vcolor_bc(PGOpContext *C, const float *props, int n)
{
  int m;
  if (!vmode(props, n, &m)) return PG_OPERATOR_CANCELLED;
  pg_gp_vcolor_brightness_contrast(C->gpd, NULL, m, prop_f(props, n, 1, 0.0f), prop_f(props, n, 2, 0.0f));
  return PG_OPERATOR_FINISHED;
}
/* GPENCIL_OT_stroke_reset_vertex_color: mode */
static int exec_vcolor_reset(PGOpContext *C, const float *props, int n)
{
  int m;
  if (!vmode(props, n, &m)) return PG_OPERATOR_CANCELLED;
  pg_gp_stroke_reset_vertex_color(C->gpd, NULL, m);
  return PG_OPERATOR_FINISHED;
}

static const PGOperatorType pg_operators[] = {
    {"GPENCIL_OT_copy", 0, poll_stroke_edit, exec_copy},
    {"GPENCIL_OT_paste", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_paste, exec_paste},
    {"GPENCIL_OT_delete", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_delete},
    {"GPENCIL_OT_duplicate", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_duplicate},
    {"GPENCIL_OT_dissolve", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_dissolve},
    {"GPENCIL_OT_stroke_join", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_join},
    {"GPENCIL_OT_move_to_layer", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_move_to_layer},
    {"GPENCIL_OT_select_all", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_select, exec_select_all},
    {"GPENCIL_OT_stroke_arrange", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_arrange},
    {"GPENCIL_OT_stroke_split", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_split},
    {"GPENCIL_OT_extrude", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_extrude},
    {"GPENCIL_OT_stroke_separate", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_separate},
    {"GPENCIL_OT_blank_frame_add", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_active_layer, exec_blank_frame},
    {"GPENCIL_OT_vertex_color_set", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_vertex, exec_vcolor_set},
    {"GPENCIL_OT_vertex_color_invert", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_vertex, exec_vcolor_invert},
    {"GPENCIL_OT_vertex_color_levels", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_vertex, exec_vcolor_levels},
    {"GPENCIL_OT_vertex_color_hsv", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_vertex, exec_vcolor_hsv},
    {"GPENCIL_OT_vertex_color_brightness_contrast", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_vertex, exec_vcolor_bc},
    {"GPENCIL_OT_stroke_reset_vertex_color", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_vertex, exec_vcolor_reset},
};

static const PGOperatorType *find_type(const char *idname)
{
  if (idname == NULL) return NULL;
  for (size_t i = 0; i < sizeof(pg_operators) / sizeof(pg_operators[0]); i++)
    if (strcmp(pg_operators[i].idname, idname) == 0) return &pg_operators[i];
  return NULL;
}

bool pg_wm_operator_poll(const PGOpContext *C, const char *idname)
{
  const PGOperatorType *ot = find_type(idname);
  return C && ot && (ot->poll == NULL || ot->poll(C));
}

int pg_wm_operator_call(PGOpContext *C, const char *idname, const float *props, int prop_count, int *r_flag)
{
  const PGOperatorType *ot = find_type(idname);
  if (r_flag) *r_flag = ot ? ot->flag : 0;
  if (C == NULL || ot == NULL || prop_count < 0 || (prop_count > 0 && props == NULL)) return PG_OPERATOR_CANCELLED;
  if (ot->poll && !ot->poll(C)) return PG_OPERATOR_CANCELLED;
  return ot->exec(C, props, prop_count);
}
