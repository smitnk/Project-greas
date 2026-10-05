/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_stroke_trim.h. BKE_gpencil_stroke_trim() (gpencil_geom_legacy.cc) accepts a
 * crossing only strictly inside both segments (0 < lambda < 1). A loop that crosses back exactly
 * through a sample point (lambda 1 on one segment, 0 on the next) is then never found. Resampled
 * strokes hit their own samples like that depending on the screen-to-canvas scale: the bug hunt's
 * opTrim failed on the API 26 and API 34 emulators. When Blender's search finds nothing, the same
 * search runs with the segment ends included, then Blender's own cut: keep points start..end and
 * move both ends onto the crossing. */
#include <stdbool.h>

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "BKE_gpencil_geom_legacy.h"

#include "project_grease_stroke_trim.h"

static bool pg_trim_at_vertex_crossing(bGPdata *gpd, bGPDstroke *gps)
{
  const float eps = 1e-5f;
  if (gps->totpoints < 4 || gps->points == NULL) {
    return false;
  }
  for (int i = 0; i < gps->totpoints - 2; i++) {
    const float *a = &gps->points[i].x;
    const float *b = &gps->points[i + 1].x;
    for (int j = i + 2; j < gps->totpoints - 1; j++) {
      const float *c = &gps->points[j].x;
      const float *d = &gps->points[j + 1].x;
      float point[3], pointb[3], closest[3];
      if (!isect_line_line_v3(a, b, c, d, point, pointb)) {
        continue;
      }
      const float la = closest_to_line_v3(closest, point, a, b);
      const float lc = closest_to_line_v3(closest, point, c, d);
      if (la < -eps || la > 1.0f + eps || lc < -eps || lc > 1.0f + eps) {
        continue;
      }
      /* A last segment ending on the first point closes the stroke; it does not cross it. */
      if (i == 0 && j + 1 == gps->totpoints - 1 && la <= eps && lc >= 1.0f - eps) {
        continue;
      }
      if (!BKE_gpencil_stroke_trim_points(gps, i, j + 1, false)) {
        return false;
      }
      copy_v3_v3(&gps->points[0].x, point);
      copy_v3_v3(&gps->points[gps->totpoints - 1].x, point);
      BKE_gpencil_stroke_geometry_update(gpd, gps);
      return true;
    }
  }
  return false;
}

bool pg_gpencil_stroke_trim(bGPdata *gpd, bGPDstroke *gps)
{
  if (BKE_gpencil_stroke_trim(gpd, gps)) {
    return true;
  }
  return pg_trim_at_vertex_crossing(gpd, gps);
}
