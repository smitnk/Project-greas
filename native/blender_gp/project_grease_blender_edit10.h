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
  PG_EDIT10_CMD_LAST = 139,
};

int pg_gp_edit10_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                          const float *args, int arg_count);
#ifdef __cplusplus
}
#endif
