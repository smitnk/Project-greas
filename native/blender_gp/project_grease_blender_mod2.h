/* SPDX-License-Identifier: GPL-2.0-or-later
 * Batch 21: modifier and tool math (Project Grease 2D adaptations of Blender 3.6.23 behaviour).
 * Pure functions on stroke data; the live modifier stack / tools call them. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct bGPDstroke;

/* Build (MOD_gpencil_legacy_build.c): visible points per stroke at frame cfra.
 * mode 0 sequential, 1 concurrent; transition 0 grow, 1 shrink. Returns total visible. */
int pg_build_visible(const int *totpoints, int nstrokes, int mode, int transition, float cfra,
                     float start_frame, float length, int *r_visible);

/* Time Offset: remapTime() of MOD_gpencil_legacy_time.c. p = the modifier's PG_P_TIME_* params
 * (segments for Chain mode included); scene_sfra/efra = the scene range used without a custom one. */
int pg_time_remap(const float *p, int scene_sfra, int scene_efra, int cfra);

/* Envelope, segments mode (MOD_gpencil_legacy_envelope.c): number of segment strokes
 * point i -> i+spread (skip = points skipped between segments). Fills pairs[2*k] (indices). */
int pg_envelope_segments(int totpoints, int cyclic, int spread, int skip, int *pairs, int max_pairs);

/* Weight Angle: weight from segment angle. */
float pg_weight_angle(float ax, float ay, float bx, float by, float angle, int invert);

/* Drawing guides (GP_GUIDE_*): project an input point onto the guide constraint.
 * type 0 circular, 1 radial, 2 parallel, 3 grid, 4 isometric; (sx,sy) = stroke start. */
void pg_guide_snap(int type, float cx, float cy, float angle, float spacing, float sx, float sy,
                   float x, float y, float r_xy[2]);

/* Onion keyframe-type filter: key_types[i] (BEZT_KEYTYPE_*) kept when == filter, or all if filter < 0. */
int pg_onion_keytype_filter(const int *key_types, int nkeys, int filter, unsigned char *r_keep);

#ifdef __cplusplus
}
#endif
