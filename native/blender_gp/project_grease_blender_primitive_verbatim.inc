/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation.
 *
 * Verbatim regions of Blender 3.6.23
 * source/blender/editors/gpencil_legacy/gpencil_primitive.c.
 * tools/verify_blender_verbatim.py fails CI if any region below differs
 * from the pinned source. Do not edit inside BEGIN/END VERBATIM markers.
 */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_primitive.c */
#define MIN_EDGES 2
#define MAX_EDGES 128
#define MAX_CP 128

#define IDLE 0
#define IN_PROGRESS 1
#define IN_CURVE_EDIT 2
#define IN_MOVE 3
#define IN_BRUSH_SIZE 4
#define IN_BRUSH_STRENGTH 5
#define IN_POLYLINE 6

#define SELECT_NONE 0
#define SELECT_START 1
#define SELECT_CP1 2
#define SELECT_CP2 3
#define SELECT_END 4

#define BIG_SIZE_CTL 15
#define MID_SIZE_CTL 10
#define SMALL_SIZE_CTL 8

#define MOVE_NONE 0
#define MOVE_ENDS 1
#define MOVE_CP 2
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_primitive.c */
/* Helper to square a primitive */
static void gpencil_primitive_to_square(tGPDprimitive *tgpi, const float x, const float y)
{
  float w = fabsf(x);
  float h = fabsf(y);
  if ((x > 0 && y > 0) || (x < 0 && y < 0)) {
    if (w > h) {
      tgpi->end[1] = tgpi->origin[1] + x;
    }
    else {
      tgpi->end[0] = tgpi->origin[0] + y;
    }
  }
  else {
    if (w > h) {
      tgpi->end[1] = tgpi->origin[1] - x;
    }
    else {
      tgpi->end[0] = tgpi->origin[0] - y;
    }
  }
}

/* Helper to constrain a primitive */
static void gpencil_primitive_constrain(tGPDprimitive *tgpi, bool line_mode)
{
  float x = tgpi->end[0] - tgpi->origin[0];
  float y = tgpi->end[1] - tgpi->origin[1];

  if (line_mode) {
    float angle = fabsf(atan2f(y, x));
    if (angle < 0.4f || angle > (M_PI - 0.4f)) {
      tgpi->end[1] = tgpi->origin[1];
    }
    else if (angle > (M_PI_2 - 0.4f) && angle < (M_PI_2 + 0.4f)) {
      tgpi->end[0] = tgpi->origin[0];
    }
    else {
      gpencil_primitive_to_square(tgpi, x, y);
    }
  }
  else {
    gpencil_primitive_to_square(tgpi, x, y);
  }
}

/* Helper to rotate point around origin */
static void gpencil_rotate_v2_v2v2fl(float v[2],
                                     const float p[2],
                                     const float origin[2],
                                     const float angle)
{
  float pt[2];
  float r[2];
  sub_v2_v2v2(pt, p, origin);
  rotate_v2_v2fl(r, pt, angle);
  add_v2_v2v2(v, r, origin);
}

/* Helper to rotate line around line center. */
static void gpencil_primitive_rotate_line(
    float va[2], float vb[2], const float a[2], const float b[2], const float angle)
{
  float midpoint[2];
  mid_v2_v2v2(midpoint, a, b);
  gpencil_rotate_v2_v2v2fl(va, a, midpoint, angle);
  gpencil_rotate_v2_v2v2fl(vb, b, midpoint, angle);
}

/* Helper to update cps */
static void gpencil_primitive_update_cps(tGPDprimitive *tgpi)
{
  if (!tgpi->curve) {
    mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
    copy_v2_v2(tgpi->cp1, tgpi->midpoint);
    copy_v2_v2(tgpi->cp2, tgpi->cp1);
  }
  else if (tgpi->type == GP_STROKE_CURVE) {
    mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
    interp_v2_v2v2(tgpi->cp1, tgpi->midpoint, tgpi->start, 0.33f);
    interp_v2_v2v2(tgpi->cp2, tgpi->midpoint, tgpi->end, 0.33f);
  }
  else if (tgpi->type == GP_STROKE_ARC) {
    if (tgpi->flip) {
      gpencil_primitive_rotate_line(tgpi->cp1, tgpi->cp2, tgpi->start, tgpi->end, M_PI_2);
    }
    else {
      gpencil_primitive_rotate_line(tgpi->cp1, tgpi->cp2, tgpi->end, tgpi->start, M_PI_2);
    }
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_primitive.c */
/* Helper: set control point */
static void gpencil_primitive_set_cp(tGPDprimitive *tgpi,
                                     const float p[2],
                                     float color[4],
                                     int size)
{
  if (tgpi->flag == IN_PROGRESS) {
    return;
  }

  bGPDcontrolpoint *cp_points = tgpi->gpd->runtime.cp_points;

  if (tgpi->gpd->runtime.tot_cp_points < MAX_CP) {
    CLAMP(size, 5, 20);
    bGPDcontrolpoint *cp = &cp_points[tgpi->gpd->runtime.tot_cp_points];
    copy_v2_v2(&cp->x, p);
    copy_v4_v4(cp->color, color);
    color[3] = 0.8f;
    cp->size = size;
    tgpi->gpd->runtime.tot_cp_points += 1;
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_primitive.c */
/* create a rectangle */
static void gpencil_primitive_rectangle(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  float coords[5][2];

  coords[0][0] = tgpi->start[0];
  coords[0][1] = tgpi->start[1];
  coords[1][0] = tgpi->end[0];
  coords[1][1] = tgpi->start[1];
  coords[2][0] = tgpi->end[0];
  coords[2][1] = tgpi->end[1];
  coords[3][0] = tgpi->start[0];
  coords[3][1] = tgpi->end[1];
  coords[4][0] = tgpi->start[0];
  coords[4][1] = tgpi->start[1];

  if (tgpi->tot_edges == 1) {
    for (int j = 0; j < 4; j++) {
      tGPspoint *p2d = &points2D[j];
      copy_v2_v2(p2d->m_xy, coords[j]);
    }
  }
  else {
    const float step = 1.0f / (float)(tgpi->tot_edges);
    int i = tgpi->tot_stored_edges;
    for (int j = 0; j < 4; j++) {
      float a = 0.0f;
      for (int k = 0; k < tgpi->tot_edges; k++) {
        tGPspoint *p2d = &points2D[i];
        interp_v2_v2v2(p2d->m_xy, coords[j], coords[j + 1], a);
        a += step;
        i++;
      }
    }
  }

  mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_REDALERT, color);
  gpencil_primitive_set_cp(tgpi, tgpi->midpoint, color, SMALL_SIZE_CTL);
}

/* create a line */
static void gpencil_primitive_line(tGPDprimitive *tgpi, tGPspoint *points2D, bool editable)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = 1.0f / (float)(tgpi->tot_edges - 1);
  float a = tgpi->tot_stored_edges ? step : 0.0f;

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    interp_v2_v2v2(p2d->m_xy, tgpi->start, tgpi->end, a);
    a += step;
  }

  if (editable) {
    float color[4];
    UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
    gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
    if (tgpi->tot_stored_edges) {
      UI_GetThemeColor4fv(TH_REDALERT, color);
      gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
    }
    else {
      gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
    }
  }
  else {
    float color[4];
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
}

/* create an arc */
static void gpencil_primitive_arc(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = M_PI_2 / (float)(tgpi->tot_edges - 1);
  float start[2];
  float end[2];
  float cp1[2];
  float corner[2];
  float midpoint[2];
  float a = tgpi->tot_stored_edges ? step : 0.0f;

  mid_v2_v2v2(tgpi->midpoint, tgpi->start, tgpi->end);
  copy_v2_v2(start, tgpi->start);
  copy_v2_v2(end, tgpi->end);
  copy_v2_v2(cp1, tgpi->cp1);
  copy_v2_v2(midpoint, tgpi->midpoint);

  corner[0] = midpoint[0] - (cp1[0] - midpoint[0]);
  corner[1] = midpoint[1] - (cp1[1] - midpoint[1]);

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    p2d->m_xy[0] = corner[0] + (end[0] - corner[0]) * sinf(a) + (start[0] - corner[0]) * cosf(a);
    p2d->m_xy[1] = corner[1] + (end[1] - corner[1]) * sinf(a) + (start[1] - corner[1]) * cosf(a);
    a += step;
  }
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->cp1, color, BIG_SIZE_CTL * 0.9f);
}

/* create a bezier */
static void gpencil_primitive_bezier(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = 1.0f / (float)(tgpi->tot_edges - 1);
  float bcp1[2];
  float bcp2[2];
  float bcp3[2];
  float bcp4[2];
  float a = tgpi->tot_stored_edges ? step : 0.0f;

  copy_v2_v2(bcp1, tgpi->start);
  copy_v2_v2(bcp2, tgpi->cp1);
  copy_v2_v2(bcp3, tgpi->cp2);
  copy_v2_v2(bcp4, tgpi->end);

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    interp_v2_v2v2v2v2_cubic(p2d->m_xy, bcp1, bcp2, bcp3, bcp4, a);
    a += step;
  }
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  if (tgpi->tot_stored_edges) {
    UI_GetThemeColor4fv(TH_REDALERT, color);
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, SMALL_SIZE_CTL);
  }
  else {
    gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  }
  UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->cp1, color, BIG_SIZE_CTL * 0.9f);
  gpencil_primitive_set_cp(tgpi, tgpi->cp2, color, BIG_SIZE_CTL * 0.9f);
}

/* create a circle */
static void gpencil_primitive_circle(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  const int totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  const float step = (2.0f * M_PI) / (float)(tgpi->tot_edges);
  float center[2];
  float radius[2];
  float a = 0.0f;

  center[0] = tgpi->start[0] + ((tgpi->end[0] - tgpi->start[0]) / 2.0f);
  center[1] = tgpi->start[1] + ((tgpi->end[1] - tgpi->start[1]) / 2.0f);
  radius[0] = fabsf((tgpi->end[0] - tgpi->start[0]) / 2.0f);
  radius[1] = fabsf((tgpi->end[1] - tgpi->start[1]) / 2.0f);

  for (int i = tgpi->tot_stored_edges; i < totpoints; i++) {
    tGPspoint *p2d = &points2D[i];
    p2d->m_xy[0] = (center[0] + cosf(a) * radius[0]);
    p2d->m_xy[1] = (center[1] + sinf(a) * radius[1]);
    a += step;
  }
  float color[4];
  UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
  gpencil_primitive_set_cp(tgpi, tgpi->end, color, BIG_SIZE_CTL);
  gpencil_primitive_set_cp(tgpi, tgpi->start, color, BIG_SIZE_CTL);
  UI_GetThemeColor4fv(TH_REDALERT, color);
  gpencil_primitive_set_cp(tgpi, center, color, SMALL_SIZE_CTL);
}
/* END VERBATIM */
