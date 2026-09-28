#include "project_grease_android_edit_ops.h"

 #include <stdint.h>

/* Blender 3.6.23 keeps the Legacy GP DNA behind its deprecation gate. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_gpencil_legacy_types.h"
#include "BKE_gpencil_geom_legacy.h"

extern "C" bool project_grease_android_stroke_flip(bGPDstroke *stroke)
{
  if (!stroke || !stroke->points || stroke->totpoints < 2) return false;
  BKE_gpencil_stroke_flip(stroke);
  return true;
}

extern "C" bool project_grease_android_stroke_subdivide(
    bGPdata *gpd, bGPDstroke *stroke, int level)
{
  if (!gpd || !stroke || !stroke->points || stroke->totpoints < 2 || level <= 0) {
    return false;
  }
  BKE_gpencil_stroke_subdivide(gpd, stroke, level, 0);
  return true;
}

extern "C" bool project_grease_android_stroke_close(bGPDstroke *stroke)
{
  if (!stroke || !stroke->points || stroke->totpoints < 3) return false;
  return BKE_gpencil_stroke_close(stroke);
}

extern "C" bool project_grease_android_stroke_trim_points(
    bGPDstroke *stroke, int index_from, int index_to, bool keep_single_point)
{
  if (!stroke || !stroke->points) return false;
  return BKE_gpencil_stroke_trim_points(stroke, index_from, index_to, keep_single_point);
}

extern "C" bool project_grease_android_stroke_split(
    bGPdata *gpd, bGPDframe *frame, bGPDstroke *stroke,
    int before_index, bGPDstroke **remaining)
{
  if (!gpd || !frame || !stroke || !remaining) return false;
  return BKE_gpencil_stroke_split(gpd, frame, stroke, before_index, remaining);
}
