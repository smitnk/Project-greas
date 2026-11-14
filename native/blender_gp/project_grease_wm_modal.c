/* SPDX-License-Identifier: GPL-2.0-or-later
 * Modal paint operators on the operator layer (project_grease_wm.h): GPENCIL_OT_draw
 * (gpencil_paint.c), GPENCIL_OT_sculpt_paint, GPENCIL_OT_vertex_paint, GPENCIL_OT_weight_paint.
 * Their invoke / modal / cancel bodies are the native tool session (project_grease_tool_session.c,
 * the ported gpencil_*_paint.c brush code); this file adds the window manager part: which operator
 * runs, one at a time, and its status. The undo push on FINISHED stays with the caller, as
 * wm_operator_finished does it for OPTYPE_UNDO. */
#include <stddef.h>

#include "project_grease_tool_session.h"
#include "project_grease_wm.h"

static const char *const pg_modal_idnames[] = {
    [PG_TOOL_DRAW] = "GPENCIL_OT_draw",
    [PG_TOOL_SCULPT] = "GPENCIL_OT_sculpt_paint",
    [PG_TOOL_VERTEX_PAINT] = "GPENCIL_OT_vertex_paint",
    [PG_TOOL_WEIGHT_PAINT] = "GPENCIL_OT_weight_paint",
};
/* The running modal operator is state of the document handle (its PGToolSession), not of the
 * process: two handles (or a handle that died mid-gesture) cannot see each other's operator. */
#define MODAL_RUNNING(C) pg_tool_session_modal_running((C)->tool_session)
#define MODAL_ERASING(C) (pg_tool_session_modal_erasing((C)->tool_session) != 0)
#define MODAL_SET(C, run, erase) pg_tool_session_set_modal((C)->tool_session, (run), (erase))

static const char *modal_name(const PGToolSession *s)
{
  if (!s) return NULL;
  if (pg_tool_session_modal_erasing(s)) return "GPENCIL_OT_draw";
  const int r = pg_tool_session_modal_running(s);
  return r >= 0 ? pg_modal_idnames[r] : NULL;
}

/* idname of the most recently used handle's running modal operator */
const char *pg_wm_modal_running(void) { return modal_name(pg_tool_session_last_modal()); }

/* The session was ended from outside the phase protocol: forget the running operator. */
void pg_wm_modal_reset_session(PGToolSession *s) { pg_tool_session_set_modal(s, -1, 0); }
void pg_wm_modal_reset(void)
{
  PGToolSession *s = pg_tool_session_last_modal();
  if (s) pg_tool_session_set_modal(s, -1, 0);
}

int pg_wm_modal_erase(PGOpContext *C, int phase, bool changed)
{
  if (C == NULL || C->tool_session == NULL) return PG_OPERATOR_CANCELLED;
  if (phase == PG_TOOL_PHASE_BEGIN) {
    /* invoke cancels a running paint operator, as for the other modal tools */
    if (MODAL_RUNNING(C) >= 0) {
      if (pg_tool_session_samples(C->tool_session, NULL, MODAL_RUNNING(C), NULL, 0, PG_TOOL_PHASE_CANCEL, NULL, 0, NULL) &
          PG_TOOL_RESULT_ENDED) {
        pg_tool_session_note_unrecorded(C->tool_session); /* cut-off brush edits still need an undo step */
      }
    }
    MODAL_SET(C, -1, 0);
    if (C->gpd == NULL) return PG_OPERATOR_CANCELLED; /* gpencil_draw_poll */
    MODAL_SET(C, -1, 1);
    return PG_OPERATOR_RUNNING_MODAL;
  }
  if (!MODAL_ERASING(C)) return PG_OPERATOR_CANCELLED;
  if (phase == PG_TOOL_PHASE_MOVE) return PG_OPERATOR_RUNNING_MODAL;
  MODAL_SET(C, -1, 0);
  return (phase == PG_TOOL_PHASE_END && changed) ? PG_OPERATOR_FINISHED : PG_OPERATOR_CANCELLED;
}

int pg_wm_modal_tool(PGOpContext *C, int tool, int phase, const float *samples, int count,
                     const float *params, int n_params, int *r_status)
{
  int status = PG_OPERATOR_CANCELLED;
  int result = 0;
  if (C == NULL || C->tool_session == NULL || tool < PG_TOOL_DRAW || tool > PG_TOOL_WEIGHT_PAINT) {
    if (r_status) *r_status = status;
    return 0;
  }
  if (phase == PG_TOOL_PHASE_BEGIN) {
    /* invoke: a running modal operator is cancelled first (its gesture was cut off) */
    MODAL_SET(C, MODAL_RUNNING(C), 0);
    if (MODAL_RUNNING(C) >= 0) {
      if (pg_tool_session_samples(C->tool_session, NULL, MODAL_RUNNING(C), NULL, 0, PG_TOOL_PHASE_CANCEL, NULL, 0, NULL) &
          PG_TOOL_RESULT_ENDED) {
        pg_tool_session_note_unrecorded(C->tool_session);
      }
      MODAL_SET(C, -1, 0);
    }
    /* poll: the brush tools need GP data; Draw creates its stroke through the sink */
    if (tool != PG_TOOL_DRAW && C->gpd == NULL) {
      if (r_status) *r_status = status;
      return 0;
    }
    result = pg_tool_session_samples(C->tool_session, C->gpd, tool, samples, count, phase, params, n_params, C->sink);
    if (result != 0) {
      MODAL_SET(C, tool, 0);
      status = PG_OPERATOR_RUNNING_MODAL;
    }
  }
  else if (MODAL_RUNNING(C) == tool) {
    result = pg_tool_session_samples(C->tool_session, C->gpd, tool, samples, count, phase, params, n_params, C->sink);
    if (phase == PG_TOOL_PHASE_MOVE) {
      status = PG_OPERATOR_RUNNING_MODAL;
    }
    else {
      MODAL_SET(C, -1, 0);
      status = (phase == PG_TOOL_PHASE_END && (result & PG_TOOL_RESULT_ENDED)) ? PG_OPERATOR_FINISHED : PG_OPERATOR_CANCELLED;
    }
  }
  else if (phase == PG_TOOL_PHASE_CANCEL) {
    /* a cancel with nothing running (document replaced): drop any half gesture */
    result = pg_tool_session_samples(C->tool_session, NULL, tool, NULL, 0, PG_TOOL_PHASE_CANCEL, NULL, 0, NULL);
  }
  if (r_status) *r_status = status;
  return result;
}
