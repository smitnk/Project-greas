/* SPDX-License-Identifier: GPL-2.0-or-later
 * Edit command ids 135-139. GPENCIL_OT_stroke_separate (modes POINT / STROKE) is ported in
 * project_grease_blender_edit4.c (pg_gp_stroke_separate).
 */
#include <math.h>
#include <stddef.h>

#include "DNA_gpencil_legacy_types.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_blender_edit4.h"
#include "project_grease_blender_edit10.h"

int pg_gp_edit10_dispatch(bGPdata *gpd, bGPDlayer *active_layer, int command, const float *args,
                          int arg_count)
{
  if (gpd == NULL || arg_count < 0 || (arg_count > 0 && args == NULL)) return 0;
  for (int i = 0; i < arg_count; i++) {
    if (!isfinite(args[i])) return 0;
  }
  int changed = 0;
  switch (command) {
    case PG_EDIT10_CMD_SEPARATE:
      if (arg_count < 1) return 0;
      changed = pg_gp_stroke_separate(gpd, active_layer, (int)lroundf(args[0]));
      break;
    default:
      return 0;
  }
  if (changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    BKE_gpencil_batch_cache_dirty_tag(gpd);
  }
  return changed ? 1 : 0;
}
