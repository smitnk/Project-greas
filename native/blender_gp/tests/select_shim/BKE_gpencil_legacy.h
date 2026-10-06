/* Test-only stand-in: the BKE selection helpers (implemented in the test). */
#pragma once
#include <stdbool.h>
#include "DNA_gpencil_legacy_types.h"
void BKE_gpencil_stroke_select_index_set(bGPdata *gpd, bGPDstroke *gps);
void BKE_gpencil_stroke_select_index_reset(bGPDstroke *gps);
bool BKE_gpencil_stroke_select_check(const bGPDstroke *gps);
void BKE_gpencil_stroke_sync_selection(bGPdata *gpd, bGPDstroke *gps);
bool BKE_gpencil_layer_is_editable(const bGPDlayer *gpl);
void BKE_gpencil_batch_cache_dirty_tag(bGPdata *gpd);
void BKE_gpencil_free_stroke(bGPDstroke *gps);
struct bGPDlayer;
void BKE_gpencil_layer_copy_settings(const bGPDlayer *gpl_src, bGPDlayer *gpl_dst);
void BKE_gpencil_layer_mask_copy(const bGPDlayer *gpl_src, bGPDlayer *gpl_dst);
enum { GP_GETFRAME_USE_PREV = 0, GP_GETFRAME_ADD_NEW = 1, GP_GETFRAME_ADD_COPY = 2 };
bGPDframe *BKE_gpencil_layer_frame_get(bGPDlayer *gpl, int cframe, int addnew);
