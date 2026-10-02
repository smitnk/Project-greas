/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Line Art on Scene-lite (SPEC_LINE_ART_ARCHITECTURE batch 2): Blender 3.6.23's Line Art
 * feature-line detection, near/far clipping, triangle intersections, bounding-area acceleration
 * and occlusion, run on the meshes and camera of a PGSceneLite. The result is every feature edge
 * cut into segments with their occlusion level, in Line Art frame-buffer coordinates (-1..1).
 * Chaining and GP stroke output are batch 3. Shadow / light contour features are not supported.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct PGSceneLite;

/* LineartGpencilModifierData settings that reach LineartData::conf (defaults = a new Line Art
 * modifier: _DNA_DEFAULT_LineartGpencilModifierData). */
typedef struct PGLineartSettings {
  int edge_types;          /* LRT_EDGE_FLAG_* (default LRT_EDGE_FLAG_INIT_TYPE) */
  int calculation_flags;   /* LRT_* calculation flags */
  float crease_threshold;  /* radians, default DEG2RAD(140) */
  float overscan;          /* default 0.1 */
  int level_end;           /* highest occlusion level computed (default 0) */
} PGLineartSettings;

void pg_lineart_settings_default(PGLineartSettings *settings);

/* One segment of a feature edge: from (x0, y0) to (x1, y1) in frame-buffer coordinates, with the
 * occlusion level of that piece and the edge type (one LRT_EDGE_FLAG_* bit). */
typedef struct PGLineartSegment {
  float x0, y0, x1, y1;
  int occlusion;
  int edge_type;
  int object_index; /* index in PGSceneLite::objects, -1 for intersection lines */
} PGLineartSegment;

/* Runs Line Art. Returns the number of segments (0 is valid: nothing visible) and stores a
 * malloc'ed array in *r_segments (free with pg_lineart_free_segments); -1 on failure. */
int pg_lineart_compute(const struct PGSceneLite *scene, const PGLineartSettings *settings,
                       PGLineartSegment **r_segments);
void pg_lineart_free_segments(PGLineartSegment *segments);

#ifdef __cplusplus
}
#endif
