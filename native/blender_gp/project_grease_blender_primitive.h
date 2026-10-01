/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Project Grease entry point for Blender 3.6.23 Legacy GP primitive geometry.
 * The geometry itself is produced by the verbatim gpencil_primitive.c
 * functions in project_grease_blender_primitive_verbatim.inc.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Project Grease primitive ids used across JNI. These are NOT Blender's
 * GP_STROKE_* values (BOX=-1, LINE=1, CIRCLE=2, ARC=3, CURVE=4, POLYLINE=5);
 * the mapping lives in project_grease_blender_primitive.c. */
enum {
  PG_PRIMITIVE_BOX = 0,
  PG_PRIMITIVE_LINE = 1,
  PG_PRIMITIVE_POLYLINE = 2,
  PG_PRIMITIVE_CIRCLE = 3,
  PG_PRIMITIVE_ARC = 4,
  PG_PRIMITIVE_CURVE = 5,
};

/* Blender's default "edges" for the primitive operator (0 if type is invalid). */
int project_grease_blender_primitive_default_edges(int pg_type);

/* 1 when Blender creates the primitive as a cyclic stroke (box, circle). */
int project_grease_blender_primitive_is_cyclic(int pg_type);

/* Number of points generate() will write, or 0 when the input is invalid.
 * edges <= 0 selects Blender's default. */
int project_grease_blender_primitive_point_count(int pg_type, int anchor_count, int edges);

/* Generate primitive points in Project Grease canvas space (y-down).
 * anchors_xy: shapes take start,end and optionally cp1,cp2 (2 or 4 anchors);
 *             polyline takes its vertices (>= 2 anchors).
 * Returns the number of points written to out_xy (x,y pairs), 0 on failure. */
int project_grease_blender_primitive_generate(int pg_type,
                                              const float *anchors_xy,
                                              int anchor_count,
                                              int edges,
                                              int flip,
                                              float *out_xy,
                                              int out_capacity);

#ifdef __cplusplus
}
#endif
