/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Runs Blender 3.6.23 Legacy GP primitive geometry for Project Grease.
 *
 * The shape generators (rectangle/line/arc/bezier/circle), control-point
 * setup and constants are verbatim copies of gpencil_primitive.c, kept in
 * project_grease_blender_primitive_verbatim.inc and checked against the
 * pinned source by tools/verify_blender_verbatim.py.
 *
 * This file only supplies what the desktop operator gets from its context:
 * - tGPDprimitive: the fields the verbatim functions read/write (the full
 *   struct in gpencil_intern.h also holds bContext/RNA/view pointers).
 * - UI_GetThemeColor4fv: control-point colors come from the desktop theme.
 * - The driver below, which mirrors gpencil_primitive_update_strokes() and
 *   gpencil_primitive_add_segment() bookkeeping without the window manager.
 *
 * Blender works in region space (y-up). Project Grease canvas space is
 * y-down, so coordinates are mirrored on the way in and out. This keeps arc
 * bulge and circle winding visually identical to Blender.
 */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"
#include "DNA_gpencil_legacy_types.h"
#include "ED_gpencil_legacy.h"

#include "project_grease_blender_primitive.h"

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_intern.h */
enum {
  GP_STROKE_BOX = -1,
  GP_STROKE_LINE = 1,
  GP_STROKE_CIRCLE = 2,
  GP_STROKE_ARC = 3,
  GP_STROKE_CURVE = 4,
  GP_STROKE_POLYLINE = 5,
};
/* END VERBATIM */

/* Subset of gpencil_intern.h tGPDprimitive used by the verbatim functions. */
typedef struct tGPDprimitive {
  struct bGPdata *gpd;
  int type;
  int orign_type;
  bool curve;
  short flip;
  int tot_stored_edges;
  int tot_edges;
  float origin[2];
  float start[2];
  float end[2];
  float midpoint[2];
  float cp1[2];
  float cp2[2];
  int flag;
} tGPDprimitive;

/* Theme ids used by gpencil_primitive.c (UI_resources.h). */
enum {
  TH_GIZMO_PRIMARY = 1,
  TH_GIZMO_SECONDARY,
  TH_REDALERT,
};

/* No Blender theme on Android: neutral colors for control points. */
static void UI_GetThemeColor4fv(int colorid, float col[4])
{
  col[0] = colorid == TH_REDALERT ? 1.0f : 0.25f;
  col[1] = colorid == TH_GIZMO_SECONDARY ? 0.8f : 0.5f;
  col[2] = colorid == TH_REDALERT ? 0.25f : 1.0f;
  col[3] = 1.0f;
}

#include "project_grease_blender_primitive_verbatim.inc"

static int pg_blender_type(int pg_type)
{
  switch (pg_type) {
    case PG_PRIMITIVE_BOX:
      return GP_STROKE_BOX;
    case PG_PRIMITIVE_LINE:
      return GP_STROKE_LINE;
    case PG_PRIMITIVE_POLYLINE:
      return GP_STROKE_POLYLINE;
    case PG_PRIMITIVE_CIRCLE:
      return GP_STROKE_CIRCLE;
    case PG_PRIMITIVE_ARC:
      return GP_STROKE_ARC;
    case PG_PRIMITIVE_CURVE:
      return GP_STROKE_CURVE;
    default:
      return 0;
  }
}

int project_grease_blender_primitive_default_edges(int pg_type)
{
  /* "subdivision" defaults of GPENCIL_OT_primitive_box/line/polyline/circle/curve
   * (arc is the curve operator with type ARC), converted to "edges" exactly
   * like gpencil_primitive_init(): box -> subdiv + 1, others -> subdiv + 2. */
  int subdiv;
  const int type = pg_blender_type(pg_type);
  switch (type) {
    case GP_STROKE_BOX:
      subdiv = 3;
      break;
    case GP_STROKE_LINE:
    case GP_STROKE_POLYLINE:
      subdiv = 6;
      break;
    case GP_STROKE_CIRCLE:
      subdiv = 94;
      break;
    case GP_STROKE_ARC:
    case GP_STROKE_CURVE:
      subdiv = 62;
      break;
    default:
      return 0;
  }
  return type == GP_STROKE_BOX ? subdiv + 1 : subdiv + 2;
}

int project_grease_blender_primitive_is_cyclic(int pg_type)
{
  /* gpencil_primitive_set_initdata(): box and circle strokes are cyclic. */
  const int type = pg_blender_type(pg_type);
  return (type == GP_STROKE_BOX || type == GP_STROKE_CIRCLE) ? 1 : 0;
}

static int pg_resolve_edges(int type, int pg_type, int edges)
{
  if (edges <= 0) {
    edges = project_grease_blender_primitive_default_edges(pg_type);
  }
  /* Same clamp as the modal operator's subdivision keys. */
  CLAMP(edges, type == GP_STROKE_BOX ? 1 : MIN_EDGES, MAX_EDGES);
  return edges;
}

/* gpencil_primitive_update_strokes(): gps->totpoints. */
static int pg_total_points(const tGPDprimitive *tgpi)
{
  int totpoints;
  if (tgpi->type == GP_STROKE_BOX) {
    totpoints = (tgpi->tot_edges * 4 + tgpi->tot_stored_edges);
  }
  else {
    totpoints = (tgpi->tot_edges + tgpi->tot_stored_edges);
  }
  if (tgpi->tot_stored_edges) {
    totpoints--;
  }
  return totpoints;
}

/* gpencil_primitive_update_strokes(): shape dispatch. */
static void pg_generate(tGPDprimitive *tgpi, tGPspoint *points2D)
{
  if (tgpi->tot_edges > 0) {
    switch (tgpi->type) {
      case GP_STROKE_BOX:
        gpencil_primitive_rectangle(tgpi, points2D);
        break;
      case GP_STROKE_LINE:
        gpencil_primitive_line(tgpi, points2D, true);
        break;
      case GP_STROKE_POLYLINE:
        gpencil_primitive_line(tgpi, points2D, false);
        break;
      case GP_STROKE_CIRCLE:
        gpencil_primitive_circle(tgpi, points2D);
        break;
      case GP_STROKE_ARC:
        gpencil_primitive_arc(tgpi, points2D);
        break;
      case GP_STROKE_CURVE:
        gpencil_primitive_bezier(tgpi, points2D);
        break;
      default:
        break;
    }
  }
}

static void pg_to_region(float r[2], const float *xy)
{
  r[0] = xy[0];
  r[1] = -xy[1];
}

static bool pg_anchors_valid(const float *anchors_xy, int anchor_count)
{
  if (!anchors_xy) {
    return false;
  }
  for (int i = 0; i < anchor_count * 2; i++) {
    if (!isfinite(anchors_xy[i])) {
      return false;
    }
  }
  return true;
}

int project_grease_blender_primitive_point_count(int pg_type, int anchor_count, int edges)
{
  const int type = pg_blender_type(pg_type);
  if (type == 0) {
    return 0;
  }
  edges = pg_resolve_edges(type, pg_type, edges);
  if (type == GP_STROKE_POLYLINE) {
    if (anchor_count < 2) {
      return 0;
    }
    /* First segment stores all edges, later ones share their first point. */
    return edges + (anchor_count - 2) * (edges - 1);
  }
  if (anchor_count != 2 && anchor_count != 4) {
    return 0;
  }
  return type == GP_STROKE_BOX ? edges * 4 : edges;
}

int project_grease_blender_primitive_generate(int pg_type,
                                              const float *anchors_xy,
                                              int anchor_count,
                                              int edges,
                                              int flip,
                                              float *out_xy,
                                              int out_capacity)
{
  const int type = pg_blender_type(pg_type);
  const int expected = project_grease_blender_primitive_point_count(pg_type, anchor_count, edges);
  if (expected <= 0 || !out_xy || out_capacity < expected ||
      !pg_anchors_valid(anchors_xy, anchor_count))
  {
    return 0;
  }

  tGPDprimitive tgpi;
  memset(&tgpi, 0, sizeof(tgpi));
  tgpi.type = type;
  tgpi.orign_type = type;
  /* gpencil_primitive_init() */
  tgpi.curve = (type == GP_STROKE_ARC || type == GP_STROKE_CURVE);
  tgpi.flip = flip ? 1 : 0;
  tgpi.tot_edges = pg_resolve_edges(type, pg_type, edges);
  tgpi.tot_stored_edges = 0;
  /* IN_PROGRESS: control points are not collected (see gpencil_primitive_set_cp). */
  tgpi.flag = IN_PROGRESS;

  /* Generators may write one point past the kept range (stored segments). */
  const int buffer_len = (type == GP_STROKE_BOX ? tgpi.tot_edges * 4 :
                                                  anchor_count * tgpi.tot_edges) + 8;
  tGPspoint *points2D = calloc((size_t)buffer_len, sizeof(tGPspoint));
  if (!points2D) {
    return 0;
  }

  int count;
  if (type == GP_STROKE_POLYLINE) {
    for (int k = 0; k + 1 < anchor_count; k++) {
      pg_to_region(tgpi.start, &anchors_xy[k * 2]);
      pg_to_region(tgpi.end, &anchors_xy[(k + 1) * 2]);
      copy_v2_v2(tgpi.origin, tgpi.start);
      gpencil_primitive_update_cps(&tgpi);
      pg_generate(&tgpi, points2D);
      /* gpencil_primitive_add_segment() */
      if (tgpi.tot_stored_edges > 0) {
        tgpi.tot_stored_edges += (tgpi.tot_edges - 1);
      }
      else {
        tgpi.tot_stored_edges += tgpi.tot_edges;
      }
    }
    /* Polyline confirm in gpencil_primitive_modal(). */
    tgpi.tot_edges = tgpi.tot_stored_edges ? 1 : 0;
    count = pg_total_points(&tgpi);
  }
  else {
    pg_to_region(tgpi.start, &anchors_xy[0]);
    pg_to_region(tgpi.end, &anchors_xy[2]);
    copy_v2_v2(tgpi.origin, tgpi.start);
    if (anchor_count == 4) {
      pg_to_region(tgpi.cp1, &anchors_xy[4]);
      pg_to_region(tgpi.cp2, &anchors_xy[6]);
    }
    else {
      gpencil_primitive_update_cps(&tgpi);
    }
    pg_generate(&tgpi, points2D);
    count = pg_total_points(&tgpi);
  }

  if (count != expected) {
    free(points2D);
    return 0;
  }
  for (int i = 0; i < count; i++) {
    out_xy[i * 2] = points2D[i].m_xy[0];
    out_xy[i * 2 + 1] = -points2D[i].m_xy[1];
  }
  free(points2D);
  return count;
}
