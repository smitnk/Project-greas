/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Line Art on Scene-lite (SPEC_LINE_ART_ARCHITECTURE batch 2): Blender 3.6.23's Line Art
 * feature-line detection, near/far clipping, triangle intersections, bounding-area acceleration
 * and occlusion, run on the meshes and camera of a PGSceneLite. The result is every feature edge
 * cut into segments with their occlusion level, in Line Art frame-buffer coordinates (-1..1).
 * pg_lineart_compute_strokes() continues through Blender's chaining (lineart_chain.c) and the
 * filtering of lineart_gpencil_generate() to strokes: source (scene / object / collection),
 * occlusion level range, material and intersection masks, shadow / light contour (with the
 * scene's PGLightLite as light_contour_object, lineart_shadow.c), silhouette and vertex groups.
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
  int level_start;         /* lowest occlusion level turned into strokes (default 0) */
  float chaining_image_threshold;  /* default 0.001 */
  float chain_smooth_tolerance;    /* default 0 */
  float angle_splitting_threshold; /* radians, default 0 (no split) */
  float stroke_depth_offset;       /* default 0.05 (towards the camera) */
  int stroke_types;        /* edge types turned into strokes (default: all enabled types) */

  /* Occlusion range: use_multiple_levels ? level_start..level_end : level_start only (as
   * generate_strokes_actual / BKE_gpencil_get_lineart_modifier_limits). Default 0. */
  int use_multiple_levels;
  /* Source (eLineartGpencilModifierSource): LRT_SOURCE_SCENE (default) / OBJECT / COLLECTION, with
   * source_index the object or collection index in the PGSceneLite. */
  int source_type;
  int source_index;
  int modifier_flags;      /* LRT_GPENCIL_INVERT_COLLECTION / LRT_GPENCIL_INVERT_SILHOUETTE_FILTER */
  int mask_switches;       /* LRT_GPENCIL_MATERIAL_MASK_ENABLE / _MATCH, LRT_GPENCIL_INTERSECTION_MATCH */
  int material_mask_bits;  /* 8 bits */
  int intersection_mask;   /* 8 bits */
  int shadow_selection;    /* LRT_SHADOW_FILTER_* */
  int silhouette_selection; /* LRT_SILHOUETTE_FILTER_* */
  /* Light "camera" of the shadow stage (defaults 0.1 / 200 / 200). */
  float shadow_camera_near, shadow_camera_far, shadow_camera_size;
  /* Vertex groups: groups of the source mesh whose names start with source_vertex_group are
   * transferred (max of the weights, 1 - w with LRT_GPENCIL_INVERT_SOURCE_VGROUP) into one output
   * weight per stroke point (PGLineartStrokes::weights). Empty = no transfer. */
  char source_vertex_group[64];
} PGLineartSettings;

void pg_lineart_settings_default(PGLineartSettings *settings);

/* One segment of a feature edge: from (x0, y0) to (x1, y1) in frame-buffer coordinates, with the
 * occlusion level of that piece and the edge type (one LRT_EDGE_FLAG_* bit). */
typedef struct PGLineartSegment {
  float x0, y0, x1, y1;
  int occlusion;
  int edge_type;
  int object_index; /* index in PGSceneLite::objects, -1 for intersection lines */
  int edge_index;   /* the feature edge this segment belongs to (segments of one edge share it) */
} PGLineartSegment;

/* Runs Line Art. Returns the number of segments (0 is valid: nothing visible) and stores a
 * malloc'ed array in *r_segments (free with pg_lineart_free_segments); -1 on failure. */
int pg_lineart_compute(const struct PGSceneLite *scene, const PGLineartSettings *settings,
                       PGLineartSegment **r_segments);
void pg_lineart_free_segments(PGLineartSegment *segments);

/* Line Art strokes: the chains that lineart_gpencil_generate() would write as GP strokes, with
 * every point in world space (eci->gpos, after the depth offset) and in frame-buffer coordinates
 * (eci->pos). */
typedef struct PGLineartStroke {
  int first;        /* index of the first point in PGLineartStrokes::world / ::image */
  int point_count;
  int edge_type;    /* LRT_EDGE_FLAG_* of the chain */
  int level;        /* occlusion level */
  int object_index; /* index in PGSceneLite::objects, -1 when none (intersections) */
} PGLineartStroke;

typedef struct PGLineartStrokes {
  PGLineartStroke *strokes;
  int stroke_count;
  float *world; /* x, y, z per point */
  float *image; /* x, y per point (-1..1) */
  float *weights; /* per point vertex group weight (0 when no source_vertex_group) */
  int point_count;
} PGLineartStrokes;

/* Returns the stroke count (0 is valid), -1 on failure. Free with pg_lineart_free_strokes(). */
int pg_lineart_compute_strokes(const struct PGSceneLite *scene, const PGLineartSettings *settings,
                               PGLineartStrokes *r_strokes);
void pg_lineart_free_strokes(PGLineartStrokes *strokes);

#ifdef __cplusplus
}
#endif
