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

/* Time Offset (MOD_gpencil_legacy_time.c): mode 0 normal, 1 reverse, 2 fixed, 3 ping-pong.
 * Custom range [sfra, efra] with loop. */
int pg_time_offset_frame(int mode, int cfra, int offset, float scale, int use_range, int sfra, int efra, int loop);

/* Hook (2D): move points toward center+offset (and rotate/scale about center) with falloff.
 * falloff 0 none (constant), 1 smooth (3t^2-2t^3), 2 linear. Returns points moved. */
int pg_hook_deform(struct bGPDstroke *gps, float cx, float cy, float dx, float dy, float angle,
                   float scale, float radius, int falloff, float strength);

/* Lattice (2D): nu x nv control grid over rect (x0,y0)-(x1,y1); offsets[(v*nu+u)*2] displace
 * the grid; points move by bilinear interpolation of the cell they fall in (outside: clamped). */
int pg_lattice_deform(struct bGPDstroke *gps, float x0, float y0, float x1, float y1, int nu, int nv,
                      const float *offsets, float strength);

/* Envelope, segments mode (MOD_gpencil_legacy_envelope.c): number of segment strokes
 * point i -> i+spread (skip = points skipped between segments). Fills pairs[2*k] (indices). */
int pg_envelope_segments(int totpoints, int cyclic, int spread, int skip, int *pairs, int max_pairs);

/* Weight Proximity: weight from distance to a canvas point; Weight Angle: from segment angle. */
float pg_weight_proximity(float px, float py, float ox, float oy, float dist_start, float dist_end, int invert);
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
