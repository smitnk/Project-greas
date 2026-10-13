/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Lasso warp: Blender 3.6.23 lattice deform on 2D canvas points (project_grease_blender_lattice.c). */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Rest position (lattice space, -1..1) of point (i, j) of an nu x nv lattice. */
int pg_lattice_rest_point(int nu, int nv, int i, int j, float r_xy[2]);
/* Deforms n points (xy interleaved, canvas units, in place) by an nu x nv lattice (2..16 each)
 * whose box is bounds (x0, y0, x1, y1) and whose points are now at ctrl (2 * nu * nv floats,
 * u fastest). interp is KEY_LINEAR / KEY_CARDINAL / KEY_BSPLINE / KEY_CATMULL_ROM. */
int pg_lattice_deform_points(float *xy, int n, const float bounds[4], int nu, int nv,
                             const float *ctrl, int interp);
#ifdef __cplusplus
}
#endif
