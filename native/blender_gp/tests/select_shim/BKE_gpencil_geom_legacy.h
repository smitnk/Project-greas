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
void BKE_gpencil_stroke_flip(bGPDstroke *gps);
bGPDstroke *BKE_gpencil_stroke_duplicate(bGPDstroke *gps_src, bool dup_points, bool dup_curve);
void BKE_gpencil_stroke_join(bGPDstroke *gps_a, bGPDstroke *gps_b, bool leave_gaps, bool fit_thickness, bool smooth, bool auto_flip);
void BKE_gpencil_stroke_simplify_fixed(bGPdata *gpd, bGPDstroke *gps);
bool BKE_gpencil_stroke_sample(bGPdata *gpd, bGPDstroke *gps, float dist, bool select, float sharp_threshold);
bGPDframe *BKE_gpencil_frame_addnew(bGPDlayer *gpl, int cframe);
bool BKE_gpencil_layer_frame_delete(bGPDlayer *gpl, bGPDframe *gpf);
void BKE_gpencil_stroke_merge_distance(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, float threshold, bool use_unselected);
bGPDlayer *BKE_gpencil_layer_addnew(bGPdata *gpd, const char *name, bool setactive, bool add_to_header);
#include <stdint.h>
void BKE_gpencil_stroke_uniform_subdivide(bGPdata *gpd, bGPDstroke *gps, uint32_t target_number, bool select);
