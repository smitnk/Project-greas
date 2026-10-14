/* SPDX-License-Identifier: GPL-2.0-or-later
 * Tenth batch: edit command ids 135-139 (routed by project_grease_gp_bridge.cpp).
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;

enum {
  PG_EDIT10_CMD_FIRST = 135,
  PG_EDIT10_CMD_SEPARATE = 135, /* args: mode (PG_SEPARATE_POINT / PG_SEPARATE_STROKE) */
  PG_EDIT10_CMD_CUTTER = 136,   /* args: flat_caps, x0, y0, x1, y1, ... (lasso, canvas units) */
  PG_EDIT10_CMD_LAST = 139,
};

/* GPENCIL_OT_stroke_cutter (gpencil_cutter_lasso_select + gpencil_cutter_dissolve): the points of
 * the editable layers' active frames inside the lasso are selected, each selection is widened to
 * the nearest crossings with points inserted exactly there (ED_gpencil_select_stroke_segment with
 * insert), then the selected points are dissolved. Returns 1 when something was cut. */
int pg_gp_cutter(struct bGPdata *gpd, const int (*mcoords)[2], int mcoords_len, int flat_caps);

int pg_gp_edit10_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                          const float *args, int arg_count);
#ifdef __cplusplus
}
#endif
