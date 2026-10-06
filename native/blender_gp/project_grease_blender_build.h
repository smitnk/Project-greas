/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Build modifier, ported from Blender 3.6.23 MOD_gpencil_legacy_build.c (generate_geometry,
 * build_sequential, build_concurrent, reduce_stroke_points, fade_stroke_points).
 * Frames and Percentage time modes; Draw Speed and the control object are not supported. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPdata;
struct bGPDframe;

typedef struct PGBuildParams {
  int mode;           /* GP_BUILD_MODE_SEQUENTIAL 0 / CONCURRENT 1 / ADDITIVE 2 */
  int transition;     /* GP_BUILD_TRANSITION_GROW 0 / SHRINK 1 / VANISH 2 */
  int time_alignment; /* GP_BUILD_TIMEALIGN_START 0 / END 1 */
  int percentage;     /* GP_BUILD_TIMEMODE_PERCENTAGE */
  float percentage_fac;
  float start_delay, length;
  int use_fading;     /* GP_BUILD_USE_FADING */
  float fade_fac, fade_thickness_strength, fade_opacity_strength;
  int restrict_time;  /* GP_BUILD_RESTRICT_TIME */
  float start_frame, end_frame;
} PGBuildParams;

/* Runs the build on gpf (an evaluated copy of a layer frame).
 * framenum: the keyframe's number; prev_strokes: stroke count of the previous keyframe (or -1);
 * next_framenum: the next keyframe's number (or -1). Returns 1 when gpf changed. */
int pg_build_generate(struct bGPdata *gpd, struct bGPDframe *gpf, int framenum, int prev_strokes,
                      int next_framenum, float ctime, const PGBuildParams *p);
#ifdef __cplusplus
}
#endif
