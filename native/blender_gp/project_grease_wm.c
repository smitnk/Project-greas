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

static const PGOperatorType pg_operators[] = {
    {"GPENCIL_OT_copy", 0, poll_stroke_edit, exec_copy},
    {"GPENCIL_OT_paste", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_paste, exec_paste},
    {"GPENCIL_OT_delete", PG_OPTYPE_REGISTER | PG_OPTYPE_UNDO, poll_stroke_edit, exec_delete},
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
