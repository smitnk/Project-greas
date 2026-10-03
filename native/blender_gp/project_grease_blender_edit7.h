/* SPDX-License-Identifier: GPL-2.0-or-later
 * Seventh batch (edit6 one-go): vertex-group operators on the selection and layer operators. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDlayer;
enum {
  PG_EDIT7_CMD_VG_ASSIGN = 93,    /* args: def_nr, weight */
  PG_EDIT7_CMD_VG_REMOVE = 94,    /* args: def_nr */
  PG_EDIT7_CMD_VG_SELECT = 95,    /* args: def_nr */
  PG_EDIT7_CMD_VG_DESELECT = 96,  /* args: def_nr */
  PG_EDIT7_CMD_VG_INVERT = 97,    /* args: def_nr */
  PG_EDIT7_CMD_VG_NORMALIZE = 98, /* args: def_nr */
  PG_EDIT7_CMD_LAYER_MERGE = 99,  /* args: - (active layer into the one below) */
  PG_EDIT7_CMD_LAYER_ISOLATE = 100,/* args: - (toggle) */
  PG_EDIT7_CMD_LOCK_ALL = 101,     /* args: - */
  PG_EDIT7_CMD_UNLOCK_ALL = 102,   /* args: - */
};
int pg_gp_vgroup_assign(struct bGPdata *gpd, const struct bGPDlayer *only, int def_nr, float weight);
int pg_gp_vgroup_remove(struct bGPdata *gpd, const struct bGPDlayer *only, int def_nr);
int pg_gp_vgroup_select(struct bGPdata *gpd, const struct bGPDlayer *only, int def_nr, int select);
int pg_gp_vgroup_invert(struct bGPdata *gpd, const struct bGPDlayer *only, int def_nr);
int pg_gp_vgroup_normalize(struct bGPdata *gpd, const struct bGPDlayer *only, int def_nr);
/* returns the layer that is active afterwards through *r_active (the lower layer on success) */
int pg_gp_layer_merge_down(struct bGPdata *gpd, struct bGPDlayer *active, struct bGPDlayer **r_active);
int pg_gp_layer_isolate(struct bGPdata *gpd, struct bGPDlayer *active);
int pg_gp_layers_lock_all(struct bGPdata *gpd, int lock);
int pg_gp_edit7_dispatch(struct bGPdata *gpd, struct bGPDlayer *active_layer, int command,
                         const float *args, int arg_count);
#ifdef __cplusplus
}
#endif
