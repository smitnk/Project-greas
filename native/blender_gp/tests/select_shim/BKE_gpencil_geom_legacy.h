/* Test-only stand-in: geometry helpers used by the edit module (implemented in the tests). */
#pragma once
#include <stdbool.h>
#include "DNA_gpencil_legacy_types.h"
void BKE_gpencil_stroke_geometry_update(bGPdata *gpd, bGPDstroke *gps);
bGPDstroke *BKE_gpencil_stroke_delete_tagged_points(bGPdata *gpd,
                                                    bGPDframe *gpf,
                                                    bGPDstroke *gps,
                                                    bGPDstroke *next_stroke,
                                                    int tag_flags,
                                                    bool select,
                                                    bool flat_cap,
                                                    int limit);
bool BKE_gpencil_stroke_stretch(bGPDstroke *gps, float dist, float overshoot_fac, short mode,
                                bool follow_curvature, int extra_point_count, float segment_influence,
                                float max_angle, bool invert_curvature);
bool BKE_gpencil_stroke_shrink(bGPDstroke *gps, float dist, short mode);
float BKE_gpencil_stroke_length(const bGPDstroke *gps, bool use_3d);
