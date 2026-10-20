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
static int pg_modal_running = -1; /* tool of the running modal operator */

const char *pg_wm_modal_running(void)
{
  return pg_modal_running >= 0 ? pg_modal_idnames[pg_modal_running] : NULL;
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
    if (pg_modal_running >= 0) {
      pg_tool_session_samples(C->tool_session, NULL, pg_modal_running, NULL, 0, PG_TOOL_PHASE_CANCEL, NULL, 0, NULL);
      pg_modal_running = -1;
    }
    /* poll: the brush tools need GP data; Draw creates its stroke through the sink */
    if (tool != PG_TOOL_DRAW && C->gpd == NULL) {
      if (r_status) *r_status = status;
      return 0;
    }
    result = pg_tool_session_samples(C->tool_session, C->gpd, tool, samples, count, phase, params, n_params, C->sink);
    if (result != 0) {
      pg_modal_running = tool;
      status = PG_OPERATOR_RUNNING_MODAL;
    }
  }
  else if (pg_modal_running == tool) {
    result = pg_tool_session_samples(C->tool_session, C->gpd, tool, samples, count, phase, params, n_params, C->sink);
    if (phase == PG_TOOL_PHASE_MOVE) {
      status = PG_OPERATOR_RUNNING_MODAL;
    }
    else {
      pg_modal_running = -1;
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
