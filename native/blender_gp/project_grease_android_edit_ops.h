#pragma once

struct bGPdata;
struct bGPDframe;
struct bGPDstroke;

#ifdef __cplusplus
extern "C" {
#endif

bool project_grease_android_stroke_flip(bGPDstroke *stroke);
bool project_grease_android_stroke_subdivide(bGPdata *gpd, bGPDstroke *stroke, int level);
bool project_grease_android_stroke_close(bGPDstroke *stroke);
bool project_grease_android_stroke_trim_points(
    bGPDstroke *stroke, int index_from, int index_to, bool keep_single_point);
bool project_grease_android_stroke_split(
    bGPdata *gpd, bGPDframe *frame, bGPDstroke *stroke, int before_index, bGPDstroke **remaining);

#ifdef __cplusplus
}
#endif
