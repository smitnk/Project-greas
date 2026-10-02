// Blender 3.6.23 Legacy GP eraser, Android adapter.
//
// The per-stroke evaluation (gpencil_stroke_eraser_dostroke) lives in
// project_grease_blender_eraser.c with its own conformance tests. This file only performs the
// two operations that need Blender's data layer, using Blender's own functions:
//   - BKE_gpencil_stroke_delete_tagged_points()  (cull tagged points, splitting the stroke)
//   - BLI_remlink() + BKE_gpencil_free_stroke()  (gpencil_free_stroke())

#include "project_grease_legacy_eraser.h"

#include <cmath>

#include "BLI_listbase.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "DNA_gpencil_legacy_types.h"

#include "project_grease_blender_eraser.h"

namespace project_grease::legacy_gp_eraser {

bool process_stroke(bGPdata *gpd, bGPDframe *frame, bGPDstroke *stroke,
                    float x, float y, int radius, const Settings &settings)
{
  if (!gpd || !frame || !stroke || radius <= 0 || !std::isfinite(x) || !std::isfinite(y)) {
    return false;
  }

  PGEraserParams params{};
  params.mval[0] = x;
  params.mval[1] = y;
  params.radius = radius;
  params.mode = settings.stroke_eraser ? PG_ERASER_STROKE :
                (settings.soft ? PG_ERASER_SOFT : PG_ERASER_HARD);
  params.draw_strength = settings.draw_strength;
  params.use_pressure = 1;
  params.pressure = settings.pointer_pressure;
  params.soft_strength = settings.soft_strength;
  params.soft_thickness = settings.soft_thickness;
  params.hard_flag = 0;

  const int result = pg_eraser_dostroke(stroke, &params);

  if (result & PG_ERASER_FREE) {
    // gpencil_free_stroke(): the caller keeps its own `next` pointer before calling us.
    BLI_remlink(&frame->strokes, stroke);
    BKE_gpencil_free_stroke(stroke);
    return true;
  }
  if (result & PG_ERASER_CULL) {
    BKE_gpencil_stroke_delete_tagged_points(
        gpd, frame, stroke, stroke->next, GP_SPOINT_TAG, false, false, 0);
  }
  return (result & (PG_ERASER_MODIFIED | PG_ERASER_CULL)) != 0;
}

} // namespace project_grease::legacy_gp_eraser
