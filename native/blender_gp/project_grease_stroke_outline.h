/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stroke outline for the GLES presentation: a stroke becomes one continuous band of triangles with
 * per-point joins, following Blender 3.6.23 common_gpencil_lib.glsl gpencil_vertex():
 * - the offset at an interior point is along the miter (bisector of the two segment directions),
 *   scaled by 1 / dot(miter_tan, line_adj), so consecutive segments share their corner vertices
 *   and leave no wedge gap on the outer side of a bend;
 * - when dot(miter_tan, line_adj) < 0.5 (Blender's miter_limit, cos 60 deg) the miter breaks: the
 *   segments keep their own normals and the outer corner is closed with a bevel plus a round join
 *   (Blender draws round caps at broken corners);
 * - round caps at both ends (GP_STROKE_CAP_ROUND, the default) unless flat caps are asked for.
 * Output is a GL_TRIANGLES list of x, y pairs in the input space.
 */
#ifndef PROJECT_GREASE_STROKE_OUTLINE_H
#define PROJECT_GREASE_STROKE_OUTLINE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PGOutlinePoint {
  float x, y;
  float radius; /* half the thickness at this point (thickness x pressure / 2) */
} PGOutlinePoint;

/* Blender's miter limit: cos(60 deg). */
#define PG_OUTLINE_MITER_LIMIT 0.5f

enum {
  PG_OUTLINE_CYCLIC = 1 << 0,
  PG_OUTLINE_FLAT_START = 1 << 1, /* GP_STROKE_CAP_FLAT at the start (caps[0]) */
  PG_OUTLINE_FLAT_END = 1 << 2,   /* GP_STROKE_CAP_FLAT at the end (caps[1]) */
};

/* Upper bound of the triangles pg_stroke_outline() writes for `count` points. */
int pg_stroke_outline_max_triangles(int count, int arc_steps);

/* Writes triangles (6 floats each) to out_xy, at most max_triangles; returns how many were
 * written. arc_steps is the number of fan triangles per half turn of a round cap/join (>= 2). */
int pg_stroke_outline(const PGOutlinePoint *points, int count, int flags, int arc_steps,
                      float *out_xy, int max_triangles);

#ifdef __cplusplus
}
#endif

#endif
