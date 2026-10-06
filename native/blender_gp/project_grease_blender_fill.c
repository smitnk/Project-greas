/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Fill tool: Blender 3.6.23 editors/gpencil_legacy/gpencil_fill.c (see project_grease_blender_fill.h).
 *
 * The functions between BEGIN/END VERBATIM markers are Blender's own code. They run against the
 * small shims below, which stand in for what the GPU/editor context provides in Blender:
 *  - ImBuf / tgpf->ima: the GP_fill image is a CPU float RGBA buffer (row 0 = bottom, as
 *    GPU_offscreen_read_color). BKE_image_acquire_ibuf / release just hand that buffer over. The
 *    buffer has one zeroed row of padding before and after it, so the Moore outline's unchecked
 *    neighbour reads at the bottom row (a negative index in Blender) read a non-green pixel instead
 *    of foreign memory.
 *  - GPU image pass: gpencil_render_offscreen() rasterizes the strokes on the GPU; Project Grease
 *    supplies that stroke mask from its GLES presenter (red = boundary) and the fill_factor
 *    resolution is applied by resampling the mask on the CPU (pg_fill_resample_mask), since the
 *    presenter renders at view size. draw_mouse_position() (the blue seed point) is painted on the
 *    CPU as a square point of the same size, under the strokes.
 *  - Space conversion: the canvas is the 2D drawing plane with identity layer matrices
 *    (BKE_gpencil_layer_transform_matrix_get -> unit matrix). gpencil_point_to_xy_fl /
 *    gpencil_point_xy_to_3d map world <-> view pixels linearly (the ortho 2D view). The extension
 *    code runs on a temporary world-space copy of the frame strokes (canvas centre = world origin,
 *    PG_FILL_CANVAS_UNITS_PER_BU canvas units per Blender unit) so its world-unit constants
 *    (fill_extend_fac * 0.1, the 1.1 bbox margin) keep their meaning; the document is untouched.
 *  - Materials: BKE_gpencil_material_settings(ob, act) reads gpd->mat[act - 1] (Project Grease keeps
 *    the material slots on the bGPdata). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_stack.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "ED_gpencil_legacy.h"

#include "project_grease_blender_fill.h"

#define LEAK_HORZ 0
#define LEAK_VERT 1
#define FILL_LEAK 3.0f
#define MIN_WINDOW_SIZE 128
#define FILL_DEBUG 0

/* ---- shims (see the file comment) ----------------------------------------------------------- */
typedef struct ImBuf {
  int x, y;
  float *rect_float;
} ImBuf;

typedef struct PGFillIma {
  ID id;
  ImBuf *ibuf;
} PGFillIma;

#define BKE_image_acquire_ibuf(ima, iuser, lock) ((void)(iuser), (void)(lock), (ima)->ibuf)
#define BKE_image_release_ibuf(ima, ibuf, lock) ((void)(ima), (void)(ibuf), (void)(lock))

typedef struct PGFillSpace {
  float scale; /* view px per world unit */
  float ox, oy;
} PGFillSpace;

#undef BKE_gpencil_layer_transform_matrix_get
#define BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat) \
  ((void)(depsgraph), (void)(obact), (void)(gpl), unit_m4(diff_mat))

static MaterialGPencilStyle *pg_fill_material_settings(bGPdata *gpd, short act)
{
  const int i = act - 1;
  if (gpd == NULL || gpd->mat == NULL || i < 0 || i >= gpd->totcol || gpd->mat[i] == NULL) {
    return NULL;
  }
  return gpd->mat[i]->gp_style;
}
#define BKE_gpencil_material_settings(ob, act) ((void)(ob), pg_fill_material_settings(tgpf->gpd, act))

static void gpencil_point_to_world_space(const bGPDspoint *pt,
                                         const float diff_mat[4][4],
                                         bGPDspoint *r_pt)
{
  float fpt[3];
  mul_v3_m4v3(fpt, diff_mat, &pt->x);
  copy_v3_v3(&r_pt->x, fpt);
}

static void gpencil_point_to_xy_fl(const PGFillSpace *gsc,
                                   const bGPDstroke *UNUSED(gps),
                                   const bGPDspoint *pt,
                                   float *r_x,
                                   float *r_y)
{
  *r_x = pt->x * gsc->scale + gsc->ox;
  *r_y = pt->y * gsc->scale + gsc->oy;
}

static void gpencil_point_xy_to_3d(const PGFillSpace *gsc,
                                   void *UNUSED(scene),
                                   const float screen_co[2],
                                   float r_out[3])
{
  r_out[0] = (screen_co[0] - gsc->ox) / gsc->scale;
  r_out[1] = (screen_co[1] - gsc->oy) / gsc->scale;
  r_out[2] = 0.0f;
}

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_fill.c */
/* Temporary stroke data including stroke extensions. */
typedef struct tStroke {
  /* Referenced layer. */
  bGPDlayer *gpl;
  /** Referenced frame. */
  bGPDframe *gpf;
  /** Referenced stroke. */
  bGPDstroke *gps;
  /** Array of 2D points */
  float (*points2d)[2];
  /** Extreme Stroke A. */
  bGPDstroke *gps_ext_a;
  /** Extreme Stroke B. */
  bGPDstroke *gps_ext_b;
} tStroke;
/* END VERBATIM */
/* The fields of Blender's tGPDfill used by the ported functions. */
typedef struct tGPDfill {
  void *depsgraph;
  void *scene;
  Object *ob;
  bGPdata *gpd;
  Brush *brush;
  PGFillSpace gsc;
  int active_cfra;
  int flag;
  int fill_extend_mode;
  float fill_extend_fac;
  bool is_render;
  int stroke_array_num;
  tStroke **stroke_array;
  PGFillIma *ima;
  int fill_leak;
  BLI_Stack *stack;
  short sbuffer_used;
  void *sbuffer;
} tGPDfill;

bool skip_layer_check(short fill_layer_mode, int gpl_active_index, int gpl_index);

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_fill.c */
/* Free temp stroke array. */
static void stroke_array_free(tGPDfill *tgpf)
{
  if (tgpf->stroke_array) {
    for (int i = 0; i < tgpf->stroke_array_num; i++) {
      tStroke *stroke = tgpf->stroke_array[i];
      MEM_SAFE_FREE(stroke->points2d);
      MEM_freeN(stroke);
    }
    MEM_SAFE_FREE(tgpf->stroke_array);
  }
  tgpf->stroke_array_num = 0;
}

/* Delete any temporary stroke. */
static void gpencil_delete_temp_stroke_extension(tGPDfill *tgpf, const bool all_frames)
{
  LISTBASE_FOREACH (bGPDlayer *, gpl, &tgpf->gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    bGPDframe *init_gpf = (all_frames) ? gpl->frames.first :
                                         BKE_gpencil_layer_frame_get(
                                             gpl, tgpf->active_cfra, GP_GETFRAME_USE_PREV);
    if (init_gpf == NULL) {
      continue;
    }
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      LISTBASE_FOREACH_MUTABLE (bGPDstroke *, gps, &gpf->strokes) {
        /* free stroke */
        if ((gps->flag & GP_STROKE_NOFILL) &&
            (gps->flag & GP_STROKE_TAG || gps->flag & GP_STROKE_HELP)) {
          BLI_remlink(&gpf->strokes, gps);
          BKE_gpencil_free_stroke(gps);
        }
      }
      if (!all_frames) {
        break;
      }
    }
  }
}

static bool extended_bbox_overlap(
    float min1[3], float max1[3], float min2[3], float max2[3], float extend)
{
  for (int axis = 0; axis < 3; axis++) {
    float intersection_min = max_ff(min1[axis], min2[axis]) - extend;
    float intersection_max = min_ff(max1[axis], max2[axis]) + extend;
    if (intersection_min > intersection_max) {
      return false;
    }
  }
  return true;
}

static void add_stroke_extension(bGPDframe *gpf, bGPDstroke *gps, float p1[3], float p2[3])
{
  bGPDstroke *gps_new = BKE_gpencil_stroke_new(gps->mat_nr, 2, gps->thickness);
  gps_new->flag |= GP_STROKE_NOFILL | GP_STROKE_TAG;
  BLI_addtail(&gpf->strokes, gps_new);

  bGPDspoint *pt = &gps_new->points[0];
  copy_v3_v3(&pt->x, p1);
  pt->strength = 1.0f;
  pt->pressure = 1.0f;

  pt = &gps_new->points[1];
  copy_v3_v3(&pt->x, p2);
  pt->strength = 1.0f;
  pt->pressure = 1.0f;
}

static void add_endpoint_radius_help(bGPDframe *gpf,
                                     bGPDstroke *gps,
                                     const float endpoint[3],
                                     const float radius,
                                     const bool focused)
{
  float circumference = 2.0f * M_PI * radius;
  float vertex_spacing = 0.005f;
  int num_vertices = min_ii(max_ii((int)ceilf(circumference / vertex_spacing), 3), 40);

  bGPDstroke *gps_new = BKE_gpencil_stroke_new(gps->mat_nr, num_vertices, gps->thickness);
  gps_new->flag |= GP_STROKE_NOFILL | GP_STROKE_CYCLIC | GP_STROKE_HELP;
  if (focused) {
    gps_new->flag |= GP_STROKE_TAG;
  }
  BLI_addtail(&gpf->strokes, gps_new);

  for (int i = 0; i < num_vertices; i++) {
    float angle = ((float)i / (float)num_vertices) * 2.0f * M_PI;
    bGPDspoint *pt = &gps_new->points[i];
    pt->x = endpoint[0] + radius * cosf(angle);
    pt->y = endpoint[1];
    pt->z = endpoint[2] + radius * sinf(angle);
    pt->strength = 1.0f;
    pt->pressure = 1.0f;
  }
}

static void extrapolate_points_by_length(bGPDspoint *a,
                                         bGPDspoint *b,
                                         float length,
                                         float r_point[3])
{
  float ab[3];
  sub_v3_v3v3(ab, &b->x, &a->x);
  normalize_v3(ab);
  mul_v3_fl(ab, length);
  add_v3_v3v3(r_point, &b->x, ab);
}

/* Calculate the size of the array for strokes. */
static void gpencil_strokes_array_size(tGPDfill *tgpf)
{
  bGPdata *gpd = tgpf->gpd;
  Brush *brush = tgpf->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;

  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  BLI_assert(gpl_active != NULL);

  const int gpl_active_index = BLI_findindex(&gpd->layers, gpl_active);
  BLI_assert(gpl_active_index >= 0);

  tgpf->stroke_array_num = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    /* Decide if the strokes of layers are included or not depending on the layer mode. */
    const int gpl_index = BLI_findindex(&gpd->layers, gpl);
    bool skip = skip_layer_check(brush_settings->fill_layer_mode, gpl_active_index, gpl_index);
    if (skip) {
      continue;
    }

    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, tgpf->active_cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) {
      continue;
    }
    tgpf->stroke_array_num += BLI_listbase_count(&gpf->strokes);
  }
}

/** Load all strokes to be processed by extend lines. */
static void gpencil_load_array_strokes(tGPDfill *tgpf)
{
  Object *ob = tgpf->ob;
  bGPdata *gpd = tgpf->gpd;
  Brush *brush = tgpf->brush;
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;

  bGPDlayer *gpl_active = BKE_gpencil_layer_active_get(gpd);
  BLI_assert(gpl_active != NULL);

  const int gpl_active_index = BLI_findindex(&gpd->layers, gpl_active);
  BLI_assert(gpl_active_index >= 0);

  /* Create array of strokes. */
  gpencil_strokes_array_size(tgpf);
  if (tgpf->stroke_array_num == 0) {
    return;
  }

  tgpf->stroke_array = MEM_callocN(sizeof(tStroke *) * tgpf->stroke_array_num, __func__);
  int idx = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }

    /* Decide if the strokes of layers are included or not depending on the layer mode. */
    const int gpl_index = BLI_findindex(&gpd->layers, gpl);
    bool skip = skip_layer_check(brush_settings->fill_layer_mode, gpl_active_index, gpl_index);
    if (skip) {
      continue;
    }

    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, tgpf->active_cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) {
      continue;
    }

    float diff_mat[4][4];
    BKE_gpencil_layer_transform_matrix_get(tgpf->depsgraph, tgpf->ob, gpl, diff_mat);

    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      /* Check if stroke can be drawn. */
      if ((gps->points == NULL) || (gps->totpoints < 2)) {
        continue;
      }
      /* Check if the color is visible. */
      MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
      if ((gp_style == NULL) || (gp_style->flag & GP_MATERIAL_HIDE)) {
        continue;
      }
      /* Don't include temp strokes. */
      if ((gps->flag & GP_STROKE_NOFILL) && (gps->flag & GP_STROKE_TAG)) {
        continue;
      }

      tStroke *stroke = MEM_callocN(sizeof(tStroke), __func__);
      stroke->gpl = gpl;
      stroke->gpf = gpf;
      stroke->gps = gps;

      /* Create the extension strokes only for Lines. */
      if (tgpf->fill_extend_mode == GP_FILL_EMODE_EXTEND) {
        /* Convert all points to 2D to speed up collision checks and avoid convert in each
         * iteration. */
        stroke->points2d = (float(*)[2])MEM_mallocN(sizeof(*stroke->points2d) * gps->totpoints,
                                                    "GP Stroke temp 2d points");

        for (int i = 0; i < gps->totpoints; i++) {
          bGPDspoint *pt = &gps->points[i];
          bGPDspoint pt2;
          gpencil_point_to_world_space(pt, diff_mat, &pt2);
          gpencil_point_to_xy_fl(
              &tgpf->gsc, gps, &pt2, &stroke->points2d[i][0], &stroke->points2d[i][1]);
        }

        /* Extend start. */
        bGPDspoint *pt1 = &gps->points[0];
        stroke->gps_ext_a = BKE_gpencil_stroke_new(gps->mat_nr, 2, gps->thickness);
        stroke->gps_ext_a->flag |= GP_STROKE_NOFILL | GP_STROKE_TAG;
        stroke->gps_ext_a->fill_opacity_fac = FLT_MAX;
        BLI_addtail(&gpf->strokes, stroke->gps_ext_a);

        bGPDspoint *pt = &stroke->gps_ext_a->points[0];
        copy_v3_v3(&pt->x, &pt1->x);
        pt->strength = 1.0f;
        pt->pressure = 1.0f;

        pt = &stroke->gps_ext_a->points[1];
        pt->strength = 1.0f;
        pt->pressure = 1.0f;

        /* Extend end. */
        pt1 = &gps->points[gps->totpoints - 1];
        stroke->gps_ext_b = BKE_gpencil_stroke_new(gps->mat_nr, 2, gps->thickness);
        stroke->gps_ext_b->flag |= GP_STROKE_NOFILL | GP_STROKE_TAG;
        stroke->gps_ext_b->fill_opacity_fac = FLT_MAX;
        BLI_addtail(&gpf->strokes, stroke->gps_ext_b);

        pt = &stroke->gps_ext_b->points[0];
        copy_v3_v3(&pt->x, &pt1->x);
        pt->strength = 1.0f;
        pt->pressure = 1.0f;

        pt = &stroke->gps_ext_b->points[1];
        pt->strength = 1.0f;
        pt->pressure = 1.0f;
      }
      else {
        stroke->gps_ext_a = NULL;
        stroke->gps_ext_b = NULL;
      }

      tgpf->stroke_array[idx] = stroke;

      idx++;
    }
  }
  tgpf->stroke_array_num = idx;
}

static void set_stroke_collide(bGPDstroke *gps_a, bGPDstroke *gps_b, const float connection_dist)
{
  gps_a->flag |= GP_STROKE_COLLIDE;
  gps_b->flag |= GP_STROKE_COLLIDE;

  /* It uses `fill_opacity_fac` to store distance because this variable is never
   * used by this type of strokes and can be used for these
   * temp strokes without adding new variables to the bGPStroke struct. */
  gps_a->fill_opacity_fac = connection_dist;
  gps_b->fill_opacity_fac = connection_dist;
  BKE_gpencil_stroke_boundingbox_calc(gps_a);
  BKE_gpencil_stroke_boundingbox_calc(gps_b);
}

static void gpencil_stroke_collision(
    tGPDfill *tgpf, bGPDlayer *gpl, bGPDstroke *gps_a, float a1xy[2], float a2xy[2])
{
  const float connection_dist = tgpf->fill_extend_fac * 0.1f;
  float diff_mat[4][4], inv_mat[4][4];

  /* Transform matrix for original stroke. */
  BKE_gpencil_layer_transform_matrix_get(tgpf->depsgraph, tgpf->ob, gpl, diff_mat);
  invert_m4_m4(inv_mat, diff_mat);

  for (int idx = 0; idx < tgpf->stroke_array_num; idx++) {
    tStroke *stroke = tgpf->stroke_array[idx];
    bGPDstroke *gps_b = stroke->gps;

    if (!extended_bbox_overlap(gps_a->boundbox_min,
                               gps_a->boundbox_max,
                               gps_b->boundbox_min,
                               gps_b->boundbox_max,
                               1.1f))
    {
      continue;
    }

    /* Loop all segments of the stroke. */
    for (int i = 0; i < gps_b->totpoints - 1; i++) {
      /* Skip segments over same pixel. */
      if (((int)a1xy[0] == (int)stroke->points2d[i + 1][0]) &&
          ((int)a1xy[1] == (int)stroke->points2d[i + 1][1]))
      {
        continue;
      }

      /* Check if extensions cross. */
      if (isect_seg_seg_v2_simple(a1xy, a2xy, stroke->points2d[i], stroke->points2d[i + 1])) {
        bGPDspoint *extreme_a = &gps_a->points[1];
        float intersection2D[2];
        isect_line_line_v2_point(
            a1xy, a2xy, stroke->points2d[i], stroke->points2d[i + 1], intersection2D);

        gpencil_point_xy_to_3d(&tgpf->gsc, tgpf->scene, intersection2D, &extreme_a->x);
        mul_m4_v3(inv_mat, &extreme_a->x);
        BKE_gpencil_stroke_boundingbox_calc(gps_a);

        gps_a->flag |= GP_STROKE_COLLIDE;
        gps_a->fill_opacity_fac = connection_dist;
        return;
      }
    }
  }
}

/* Cut the extended lines if collide. */
static void gpencil_cut_extensions(tGPDfill *tgpf)
{
  const float connection_dist = tgpf->fill_extend_fac * 0.1f;
  const bool use_stroke_collide = (tgpf->flag & GP_BRUSH_FILL_STROKE_COLLIDE) != 0;

  bGPDlayer *gpl_prev = NULL;
  bGPDframe *gpf_prev = NULL;
  float diff_mat[4][4], inv_mat[4][4];

  /* Allocate memory for all extend strokes. */
  bGPDstroke **gps_array = MEM_callocN(sizeof(bGPDstroke *) * tgpf->stroke_array_num * 2,
                                       __func__);

  for (int idx = 0; idx < tgpf->stroke_array_num; idx++) {
    tStroke *stroke = tgpf->stroke_array[idx];
    bGPDframe *gpf = stroke->gpf;
    if (stroke->gpl != gpl_prev) {
      BKE_gpencil_layer_transform_matrix_get(tgpf->depsgraph, tgpf->ob, stroke->gpl, diff_mat);
      invert_m4_m4(inv_mat, diff_mat);
      gpl_prev = stroke->gpl;
    }

    if (gpf == gpf_prev) {
      continue;
    }
    gpf_prev = gpf;

    /* Store all frame extend strokes in an array. */
    int tot_idx = 0;
    for (int i = 0; i < tgpf->stroke_array_num; i++) {
      tStroke *s = tgpf->stroke_array[i];
      if (s->gpf != gpf) {
        continue;
      }
      if ((s->gps_ext_a) && ((s->gps_ext_a->flag & GP_STROKE_COLLIDE) == 0)) {
        gps_array[tot_idx] = s->gps_ext_a;
        tot_idx++;
      }
      if ((s->gps_ext_b) && ((s->gps_ext_b->flag & GP_STROKE_COLLIDE) == 0)) {
        gps_array[tot_idx] = s->gps_ext_b;
        tot_idx++;
      }
    }

    /* Compare all strokes. */
    for (int i = 0; i < tot_idx; i++) {
      bGPDstroke *gps_a = gps_array[i];

      bGPDspoint pt2;
      float a1xy[2], a2xy[2];
      float b1xy[2], b2xy[2];

      /* First stroke. */
      bGPDspoint *pt = &gps_a->points[0];
      gpencil_point_to_world_space(pt, diff_mat, &pt2);
      gpencil_point_to_xy_fl(&tgpf->gsc, gps_a, &pt2, &a1xy[0], &a1xy[1]);

      pt = &gps_a->points[1];
      gpencil_point_to_world_space(pt, diff_mat, &pt2);
      gpencil_point_to_xy_fl(&tgpf->gsc, gps_a, &pt2, &a2xy[0], &a2xy[1]);
      bGPDspoint *extreme_a = &gps_a->points[1];

      /* Loop all other strokes and check the intersections. */
      for (int z = 0; z < tot_idx; z++) {
        bGPDstroke *gps_b = gps_array[z];
        /* Don't check stroke with itself. */
        if (i == z) {
          continue;
        }

        /* Don't check strokes unless the bounding boxes of the strokes
         * are close enough together that they can plausibly be connected. */
        if (!extended_bbox_overlap(gps_a->boundbox_min,
                                   gps_a->boundbox_max,
                                   gps_b->boundbox_min,
                                   gps_b->boundbox_max,
                                   1.1f))
        {
          continue;
        }

        pt = &gps_b->points[0];
        gpencil_point_to_world_space(pt, diff_mat, &pt2);
        gpencil_point_to_xy_fl(&tgpf->gsc, gps_b, &pt2, &b1xy[0], &b1xy[1]);

        pt = &gps_b->points[1];
        gpencil_point_to_world_space(pt, diff_mat, &pt2);
        gpencil_point_to_xy_fl(&tgpf->gsc, gps_b, &pt2, &b2xy[0], &b2xy[1]);
        bGPDspoint *extreme_b = &gps_b->points[1];

        /* Check if extreme points are near. This case is when the
         * extended lines are co-linear or parallel and close together. */
        const float gap_pixsize_sq = 25.0f;
        float intersection3D[3];
        if (len_squared_v2v2(a2xy, b2xy) <= gap_pixsize_sq) {
          gpencil_point_xy_to_3d(&tgpf->gsc, tgpf->scene, b2xy, intersection3D);
          mul_m4_v3(inv_mat, intersection3D);
          copy_v3_v3(&extreme_a->x, intersection3D);
          copy_v3_v3(&extreme_b->x, intersection3D);
          set_stroke_collide(gps_a, gps_b, connection_dist);
          break;
        }
        /* Check if extensions cross. */
        if (isect_seg_seg_v2_simple(a1xy, a2xy, b1xy, b2xy)) {
          float intersection2D[2];
          isect_line_line_v2_point(a1xy, a2xy, b1xy, b2xy, intersection2D);

          gpencil_point_xy_to_3d(&tgpf->gsc, tgpf->scene, intersection2D, intersection3D);
          mul_m4_v3(inv_mat, intersection3D);
          copy_v3_v3(&extreme_a->x, intersection3D);
          copy_v3_v3(&extreme_b->x, intersection3D);
          set_stroke_collide(gps_a, gps_b, connection_dist);
          break;
        }
        /* Check if extension extreme is near of the origin of any other extension. */
        if (len_squared_v2v2(a2xy, b1xy) <= gap_pixsize_sq) {
          gpencil_point_xy_to_3d(&tgpf->gsc, tgpf->scene, b1xy, &extreme_a->x);
          mul_m4_v3(inv_mat, &extreme_a->x);
          set_stroke_collide(gps_a, gps_b, connection_dist);
          break;
        }
        if (len_squared_v2v2(a1xy, b2xy) <= gap_pixsize_sq) {
          gpencil_point_xy_to_3d(&tgpf->gsc, tgpf->scene, a1xy, &extreme_b->x);
          mul_m4_v3(inv_mat, &extreme_b->x);
          set_stroke_collide(gps_a, gps_b, connection_dist);
          break;
        }
      }

      /* Check if collide with normal strokes. */
      if (use_stroke_collide && (gps_a->flag & GP_STROKE_COLLIDE) == 0) {
        gpencil_stroke_collision(tgpf, stroke->gpl, gps_a, a1xy, a2xy);
      }
    }
  }
  MEM_SAFE_FREE(gps_array);
}

/* Loop all strokes and update stroke line extensions. */
static void gpencil_update_extensions_line(tGPDfill *tgpf)
{
  float connection_dist = tgpf->fill_extend_fac * 0.1f;

  for (int idx = 0; idx < tgpf->stroke_array_num; idx++) {
    tStroke *stroke = tgpf->stroke_array[idx];
    bGPDstroke *gps = stroke->gps;
    bGPDstroke *gps_a = stroke->gps_ext_a;
    bGPDstroke *gps_b = stroke->gps_ext_b;

    /* Extend start. */
    if (((gps_a->flag & GP_STROKE_COLLIDE) == 0) || (gps_a->fill_opacity_fac > connection_dist)) {
      bGPDspoint *pt0 = &gps->points[1];
      bGPDspoint *pt1 = &gps->points[0];
      bGPDspoint *pt = &gps_a->points[1];
      extrapolate_points_by_length(pt0, pt1, connection_dist, &pt->x);
      gps_a->flag &= ~GP_STROKE_COLLIDE;
    }

    /* Extend end. */
    if (((gps_b->flag & GP_STROKE_COLLIDE) == 0) || (gps_b->fill_opacity_fac > connection_dist)) {
      bGPDspoint *pt0 = &gps->points[gps->totpoints - 2];
      bGPDspoint *pt1 = &gps->points[gps->totpoints - 1];
      bGPDspoint *pt = &gps_b->points[1];
      extrapolate_points_by_length(pt0, pt1, connection_dist, &pt->x);
      gps_b->flag &= ~GP_STROKE_COLLIDE;
    }
  }

  /* Cut over-length strokes. */
  gpencil_cut_extensions(tgpf);
}
/* END VERBATIM */
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_fill.c */
static bool gpencil_stroke_is_drawable(tGPDfill *tgpf, bGPDstroke *gps)
{
  const bool is_line_mode = (tgpf->fill_extend_mode == GP_FILL_EMODE_EXTEND);
  const bool show_help = (tgpf->flag & GP_BRUSH_FILL_SHOW_HELPLINES) != 0;
  const bool show_extend = (tgpf->flag & GP_BRUSH_FILL_SHOW_EXTENDLINES) != 0;
  const bool use_stroke_collide = (tgpf->flag & GP_BRUSH_FILL_STROKE_COLLIDE) != 0;
  const bool is_extend_stroke = (gps->flag & GP_STROKE_NOFILL) && (gps->flag & GP_STROKE_TAG);
  const bool is_help_stroke = (gps->flag & GP_STROKE_NOFILL) && (gps->flag & GP_STROKE_HELP);
  const bool stroke_collide = (gps->flag & GP_STROKE_COLLIDE) != 0;

  if (is_line_mode && is_extend_stroke && tgpf->is_render && use_stroke_collide && !stroke_collide)
  {
    return false;
  }

  if (tgpf->is_render) {
    return true;
  }

  if ((!show_help) && (show_extend)) {
    if (!is_extend_stroke && !is_help_stroke) {
      return false;
    }
  }

  if ((show_help) && (!show_extend)) {
    if (is_extend_stroke || is_help_stroke) {
      return false;
    }
  }

  return true;
}
/* END VERBATIM */
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_fill.c */
/* Helper: Check if must skip the layer */
bool skip_layer_check(short fill_layer_mode, int gpl_active_index, int gpl_index)
{
  bool skip = false;

  switch (fill_layer_mode) {
    case GP_FILL_GPLMODE_ACTIVE: {
      if (gpl_index != gpl_active_index) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_ABOVE: {
      if (gpl_index != gpl_active_index + 1) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_BELOW: {
      if (gpl_index != gpl_active_index - 1) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_ALL_ABOVE: {
      if (gpl_index <= gpl_active_index) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_ALL_BELOW: {
      if (gpl_index >= gpl_active_index) {
        skip = true;
      }
      break;
    }
    case GP_FILL_GPLMODE_VISIBLE:
    default:
      break;
  }

  return skip;
}
/* END VERBATIM */
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_fill.c */
/* Return pixel data (RGBA) at index. */
static void get_pixel(const ImBuf *ibuf, const int idx, float r_col[4])
{
  BLI_assert(ibuf->rect_float != NULL);
  memcpy(r_col, &ibuf->rect_float[idx * 4], sizeof(float[4]));
}

/* Set pixel data (RGBA) at index. */
static void set_pixel(ImBuf *ibuf, int idx, const float col[4])
{
  BLI_assert(ibuf->rect_float != NULL);
  float *rrectf = &ibuf->rect_float[idx * 4];
  copy_v4_v4(rrectf, col);
}

/* Helper: Check if one image row is empty. */
static bool is_row_filled(const ImBuf *ibuf, const int row_index)
{
  float *row = &ibuf->rect_float[ibuf->x * 4 * row_index];
  return (row[0] == 0.0f && memcmp(row, row + 1, ((ibuf->x * 4) - 1) * sizeof(float)) != 0);
}

/**
 * Check if the size of the leak is narrow to determine if the stroke is closed
 * this is used for strokes with small gaps between them to get a full fill
 * and do not get a full screen fill.
 *
 * This function assumes that if the furthest pixel is occupied,
 * the other pixels are occupied.
 *
 * \param ibuf: Image pixel data.
 * \param maxpixel: Maximum index.
 * \param limit: Limit of pixels to analyze.
 * \param index: Index of current pixel.
 * \param type: 0-Horizontal 1-Vertical.
 */
static bool is_leak_narrow(ImBuf *ibuf, const int maxpixel, int limit, int index, int type)
{
  float rgba[4];
  int pt;
  bool t_a = false;
  bool t_b = false;
  const int extreme = limit - 1;

  /* Horizontal leak (check vertical pixels)
   * X
   * X
   * xB7
   * X
   * X
   */
  if (type == LEAK_HORZ) {
    /* pixels on top */
    pt = index + (ibuf->x * extreme);
    if (pt <= maxpixel) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_a = true;
      }
    }
    else {
      /* Edge of image. */
      t_a = true;
    }
    /* pixels on bottom */
    pt = index - (ibuf->x * extreme);
    if (pt >= 0) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_b = true;
      }
    }
    else {
      /* Edge of image. */
      t_b = true;
    }
  }

  /* Vertical leak (check horizontal pixels)
   *
   * XXXxB7XX
   */
  if (type == LEAK_VERT) {
    /* get pixel range of the row */
    int row = index / ibuf->x;
    int lowpix = row * ibuf->x;
    int higpix = lowpix + ibuf->x - 1;

    /* pixels to right */
    pt = index - extreme;
    if (pt >= lowpix) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_a = true;
      }
    }
    else {
      t_a = true; /* Edge of image. */
    }
    /* pixels to left */
    pt = index + extreme;
    if (pt <= higpix) {
      get_pixel(ibuf, pt, rgba);
      if (rgba[0] == 1.0f) {
        t_b = true;
      }
    }
    else {
      t_b = true; /* edge of image */
    }
  }
  return (bool)(t_a && t_b);
}

/**
 * Boundary fill inside strokes
 * Fills the space created by a set of strokes using the stroke color as the boundary
 * of the shape to fill.
 *
 * \param tgpf: Temporary fill data.
 */
static bool gpencil_boundaryfill_area(tGPDfill *tgpf)
{
  ImBuf *ibuf;
  float rgba[4];
  void *lock;
  const float fill_col[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  const int maxpixel = (ibuf->x * ibuf->y) - 1;
  bool border_contact = false;

  BLI_Stack *stack = BLI_stack_new(sizeof(int), __func__);

  /* Calculate index of the seed point using the position of the mouse looking
   * for a blue pixel. */
  int index = -1;
  for (int i = 0; i < maxpixel; i++) {
    get_pixel(ibuf, i, rgba);
    if (rgba[2] == 1.0f) {
      index = i;
      break;
    }
  }

  if ((index >= 0) && (index <= maxpixel)) {
    if (!FILL_DEBUG) {
      BLI_stack_push(stack, &index);
    }
  }

  /**
   * The fill use a stack to save the pixel list instead of the common recursive
   * 4-contact point method.
   * The problem with recursive calls is that for big fill areas, we can get max limit
   * of recursive calls and STACK_OVERFLOW error.
   *
   * The 4-contact point analyze the pixels to the left, right, bottom and top
   * <pre>
   * -----------
   * |    X    |
   * |   XoX   |
   * |    X    |
   * -----------
   * </pre>
   */
  while (!BLI_stack_is_empty(stack)) {
    int v;

    BLI_stack_pop(stack, &v);

    get_pixel(ibuf, v, rgba);

    /* Determine if the flood contacts with external borders. */
    if (rgba[3] == 0.5f) {
      border_contact = true;
    }

    /* check if no border(red) or already filled color(green) */
    if ((rgba[0] != 1.0f) && (rgba[1] != 1.0f)) {
      /* fill current pixel with green */
      set_pixel(ibuf, v, fill_col);

      /* add contact pixels */
      /* pixel left */
      if (v - 1 >= 0) {
        index = v - 1;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_HORZ)) {
          BLI_stack_push(stack, &index);
        }
      }
      /* pixel right */
      if (v + 1 <= maxpixel) {
        index = v + 1;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_HORZ)) {
          BLI_stack_push(stack, &index);
        }
      }
      /* pixel top */
      if (v + ibuf->x <= maxpixel) {
        index = v + ibuf->x;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_VERT)) {
          BLI_stack_push(stack, &index);
        }
      }
      /* pixel bottom */
      if (v - ibuf->x >= 0) {
        index = v - ibuf->x;
        if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_VERT)) {
          BLI_stack_push(stack, &index);
        }
      }
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
  /* free temp stack data */
  BLI_stack_free(stack);

  return border_contact;
}

/* Set a border to create image limits. */
static void gpencil_set_borders(tGPDfill *tgpf, const bool transparent)
{
  ImBuf *ibuf;
  void *lock;
  const float fill_col[2][4] = {{1.0f, 0.0f, 0.0f, 0.5f}, {0.0f, 0.0f, 0.0f, 0.0f}};
  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  int idx;
  int pixel = 0;
  const int coloridx = transparent ? 0 : 1;

  /* horizontal lines */
  for (idx = 0; idx < ibuf->x; idx++) {
    /* bottom line */
    set_pixel(ibuf, idx, fill_col[coloridx]);
    /* top line */
    pixel = idx + (ibuf->x * (ibuf->y - 1));
    set_pixel(ibuf, pixel, fill_col[coloridx]);
  }
  /* vertical lines */
  for (idx = 0; idx < ibuf->y; idx++) {
    /* left line */
    set_pixel(ibuf, ibuf->x * idx, fill_col[coloridx]);
    /* right line */
    pixel = ibuf->x * idx + (ibuf->x - 1);
    set_pixel(ibuf, pixel, fill_col[coloridx]);
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
}

/* Invert image to paint inverse area. */
static void gpencil_invert_image(tGPDfill *tgpf)
{
  ImBuf *ibuf;
  void *lock;
  const float fill_col[3][4] = {
      {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);

  const int maxpixel = (ibuf->x * ibuf->y) - 1;

  for (int v = maxpixel; v != 0; v--) {
    float color[4];
    get_pixel(ibuf, v, color);
    /* Green->Red. */
    if (color[1] == 1.0f) {
      set_pixel(ibuf, v, fill_col[0]);
    }
    /* Red->Green */
    else if (color[0] == 1.0f) {
      set_pixel(ibuf, v, fill_col[1]);
    }
    else {
      /* Set to Transparent. */
      set_pixel(ibuf, v, fill_col[2]);
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
}

/* Mark and clear processed areas. */
static void gpencil_erase_processed_area(tGPDfill *tgpf)
{
  ImBuf *ibuf;
  void *lock;
  const float blue_col[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  const float clear_col[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  tGPspoint *point2D;

  if (tgpf->sbuffer_used == 0) {
    return;
  }

  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  point2D = (tGPspoint *)tgpf->sbuffer;

  /* First set in blue the perimeter. */
  for (int i = 0; i < tgpf->sbuffer_used && point2D; i++, point2D++) {
    int image_idx = ibuf->x * (int)point2D->m_xy[1] + (int)point2D->m_xy[0];
    set_pixel(ibuf, image_idx, blue_col);
  }

  /* Second, clean by lines any pixel between blue pixels. */
  float rgba[4];

  for (int idy = 0; idy < ibuf->y; idy++) {
    int init = -1;
    int end = -1;
    for (int idx = 0; idx < ibuf->x; idx++) {
      int image_idx = ibuf->x * idy + idx;
      get_pixel(ibuf, image_idx, rgba);
      /* Blue. */
      if (rgba[2] == 1.0f) {
        if (init < 0) {
          init = image_idx;
        }
        else {
          end = image_idx;
        }
      }
      /* Red. */
      else if (rgba[0] == 1.0f) {
        if (init > -1) {
          for (int i = init; i <= max_ii(init, end); i++) {
            set_pixel(ibuf, i, clear_col);
          }
          init = -1;
          end = -1;
        }
      }
    }
    /* Check last segment. */
    if (init > -1) {
      for (int i = init; i <= max_ii(init, end); i++) {
        set_pixel(ibuf, i, clear_col);
      }
      set_pixel(ibuf, init, clear_col);
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);

  tgpf->ima->id.tag |= LIB_TAG_DOIT;
}

/**
 * Naive dilate
 *
 * Expand green areas into enclosing red or transparent areas.
 * Using stack prevents creep when replacing colors directly.
 * <pre>
 * -----------
 *  XXXXXXX
 *  XoooooX
 *  XXooXXX
 *   XXXX
 * -----------
 * </pre>
 */
static bool dilate_shape(ImBuf *ibuf)
{
#define IS_GREEN (color[1] == 1.0f)
#define IS_NOT_GREEN (color[1] != 1.0f)

  bool done = false;

  BLI_Stack *stack = BLI_stack_new(sizeof(int), __func__);
  const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  const int max_size = (ibuf->x * ibuf->y) - 1;
  /* detect pixels and expand into red areas */
  for (int row = 0; row < ibuf->y; row++) {
    if (!is_row_filled(ibuf, row)) {
      continue;
    }
    int maxpixel = (ibuf->x * (row + 1)) - 1;
    int minpixel = ibuf->x * row;

    for (int v = maxpixel; v != minpixel; v--) {
      float color[4];
      int index;
      get_pixel(ibuf, v, color);
      if (IS_GREEN) {
        int tp = 0;
        int bm = 0;
        int lt = 0;
        int rt = 0;

        /* pixel left */
        if (v - 1 >= 0) {
          index = v - 1;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
            lt = index;
          }
        }
        /* pixel right */
        if (v + 1 <= maxpixel) {
          index = v + 1;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
            rt = index;
          }
        }
        /* pixel top */
        if (v + ibuf->x <= max_size) {
          index = v + ibuf->x;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
            tp = index;
          }
        }
        /* pixel bottom */
        if (v - ibuf->x >= 0) {
          index = v - ibuf->x;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
            bm = index;
          }
        }
        /* pixel top-left */
        if (tp && lt) {
          index = tp - 1;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
          }
        }
        /* pixel top-right */
        if (tp && rt) {
          index = tp + 1;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
          }
        }
        /* pixel bottom-left */
        if (bm && lt) {
          index = bm - 1;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
          }
        }
        /* pixel bottom-right */
        if (bm && rt) {
          index = bm + 1;
          get_pixel(ibuf, index, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &index);
          }
        }
      }
    }
  }
  /* set dilated pixels */
  while (!BLI_stack_is_empty(stack)) {
    int v;
    BLI_stack_pop(stack, &v);
    set_pixel(ibuf, v, green);
    done = true;
  }
  BLI_stack_free(stack);

  return done;

#undef IS_GREEN
#undef IS_NOT_GREEN
}

/**
 * Contract
 *
 * Contract green areas to scale down the size.
 * Using stack prevents creep when replacing colors directly.
 */
static bool contract_shape(ImBuf *ibuf)
{
#define IS_GREEN (color[1] == 1.0f)
#define IS_NOT_GREEN (color[1] != 1.0f)

  bool done = false;

  BLI_Stack *stack = BLI_stack_new(sizeof(int), __func__);
  const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const int max_size = (ibuf->x * ibuf->y) - 1;

  /* Detect if pixel is near of no green pixels and mark green pixel to be cleared. */
  for (int row = 0; row < ibuf->y; row++) {
    if (!is_row_filled(ibuf, row)) {
      continue;
    }
    int maxpixel = (ibuf->x * (row + 1)) - 1;
    int minpixel = ibuf->x * row;

    for (int v = maxpixel; v != minpixel; v--) {
      float color[4];
      get_pixel(ibuf, v, color);
      if (IS_GREEN) {
        /* pixel left */
        if (v - 1 >= 0) {
          get_pixel(ibuf, v - 1, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &v);
            continue;
          }
        }
        /* pixel right */
        if (v + 1 <= maxpixel) {
          get_pixel(ibuf, v + 1, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &v);
            continue;
          }
        }
        /* pixel top */
        if (v + ibuf->x <= max_size) {
          get_pixel(ibuf, v + ibuf->x, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &v);
            continue;
          }
        }
        /* pixel bottom */
        if (v - ibuf->x >= 0) {
          get_pixel(ibuf, v - ibuf->x, color);
          if (IS_NOT_GREEN) {
            BLI_stack_push(stack, &v);
            continue;
          }
        }
      }
    }
  }
  /* Clear pixels. */
  while (!BLI_stack_is_empty(stack)) {
    int v;
    BLI_stack_pop(stack, &v);
    set_pixel(ibuf, v, clear);
    done = true;
  }
  BLI_stack_free(stack);

  return done;

#undef IS_GREEN
#undef IS_NOT_GREEN
}

/* Get the outline points of a shape using Moore Neighborhood algorithm
 *
 * This is a Blender customized version of the general algorithm described
 * in https://en.wikipedia.org/wiki/Moore_neighborhood
 */
static void gpencil_get_outline_points(tGPDfill *tgpf, const bool dilate)
{
  ImBuf *ibuf;
  Brush *brush = tgpf->brush;
  float rgba[4];
  void *lock;
  int v[2];
  int boundary_co[2];
  int start_co[2];
  int first_co[2] = {-1, -1};
  int backtracked_co[2];
  int current_check_co[2];
  int prev_check_co[2];
  int backtracked_offset[1][2] = {{0, 0}};
  bool first_pixel = false;
  bool start_found = false;
  const int NEIGHBOR_COUNT = 8;

  const int offset[8][2] = {
      {-1, -1},
      {0, -1},
      {1, -1},
      {1, 0},
      {1, 1},
      {0, 1},
      {-1, 1},
      {-1, 0},
  };

  tgpf->stack = BLI_stack_new(sizeof(int[2]), __func__);

  ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
  int imagesize = ibuf->x * ibuf->y;

  /* Dilate or contract. */
  if (dilate) {
    for (int i = 0; i < abs(brush->gpencil_settings->dilate_pixels); i++) {
      if (brush->gpencil_settings->dilate_pixels > 0) {
        dilate_shape(ibuf);
      }
      else {
        contract_shape(ibuf);
      }
    }
  }

  for (int idx = imagesize - 1; idx != 0; idx--) {
    get_pixel(ibuf, idx, rgba);
    if (rgba[1] == 1.0f) {
      boundary_co[0] = idx % ibuf->x;
      boundary_co[1] = idx / ibuf->x;
      copy_v2_v2_int(start_co, boundary_co);
      backtracked_co[0] = (idx - 1) % ibuf->x;
      backtracked_co[1] = (idx - 1) / ibuf->x;
      backtracked_offset[0][0] = backtracked_co[0] - boundary_co[0];
      backtracked_offset[0][1] = backtracked_co[1] - boundary_co[1];
      copy_v2_v2_int(prev_check_co, start_co);

      BLI_stack_push(tgpf->stack, &boundary_co);
      start_found = true;
      break;
    }
  }

  while (start_found) {
    int cur_back_offset = -1;
    for (int i = 0; i < NEIGHBOR_COUNT; i++) {
      if (backtracked_offset[0][0] == offset[i][0] && backtracked_offset[0][1] == offset[i][1]) {
        /* Finding the back-tracked pixel offset index */
        cur_back_offset = i;
        break;
      }
    }

    int loop = 0;
    while (loop < (NEIGHBOR_COUNT - 1) && cur_back_offset != -1) {
      int offset_idx = (cur_back_offset + 1) % NEIGHBOR_COUNT;
      current_check_co[0] = boundary_co[0] + offset[offset_idx][0];
      current_check_co[1] = boundary_co[1] + offset[offset_idx][1];

      int image_idx = ibuf->x * current_check_co[1] + current_check_co[0];
      /* Check if the index is inside the image. If the index is outside is
       * because the algorithm is unable to find the outline of the figure. This is
       * possible for negative filling when click inside a figure instead of
       * clicking outside.
       * If the index is out of range, finish the filling. */
      if (image_idx > imagesize - 1) {
        start_found = false;
        break;
      }
      get_pixel(ibuf, image_idx, rgba);

      /* find next boundary pixel */
      if (rgba[1] == 1.0f) {
        copy_v2_v2_int(boundary_co, current_check_co);
        copy_v2_v2_int(backtracked_co, prev_check_co);
        backtracked_offset[0][0] = backtracked_co[0] - boundary_co[0];
        backtracked_offset[0][1] = backtracked_co[1] - boundary_co[1];

        BLI_stack_push(tgpf->stack, &boundary_co);

        break;
      }
      copy_v2_v2_int(prev_check_co, current_check_co);
      cur_back_offset++;
      loop++;
    }
    /* Current pixel is equal to starting or first pixel. */
    if ((boundary_co[0] == start_co[0] && boundary_co[1] == start_co[1]) ||
        (boundary_co[0] == first_co[0] && boundary_co[1] == first_co[1]))
    {
      BLI_stack_pop(tgpf->stack, &v);
      break;
    }

    if (!first_pixel) {
      first_pixel = true;
      copy_v2_v2_int(first_co, boundary_co);
    }
  }

  /* release ibuf */
  BKE_image_release_ibuf(tgpf->ima, ibuf, lock);
}
/* END VERBATIM */
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_fill.c */
/* create array of points using stack as source */
static int gpencil_points_from_stack(tGPDfill *tgpf)
{
  tGPspoint *point2D;
  int totpoints = BLI_stack_count(tgpf->stack);
  if (totpoints == 0) {
    return 0;
  }

  tgpf->sbuffer_used = (short)totpoints;
  tgpf->sbuffer = MEM_callocN(sizeof(tGPspoint) * totpoints, __func__);

  point2D = tgpf->sbuffer;
  while (!BLI_stack_is_empty(tgpf->stack)) {
    int v[2];
    BLI_stack_pop(tgpf->stack, &v);
    copy_v2fl_v2i(point2D->m_xy, v);
    /* shift points to center of pixel */
    add_v2_fl(point2D->m_xy, 0.5f);
    point2D->pressure = 1.0f;
    point2D->strength = 1.0f;
    point2D->time = 0.0f;
    point2D++;
  }

  return totpoints;
}
/* END VERBATIM */
/* ---- Project Grease drivers ------------------------------------------------------------------ */
float pg_fill_factor_clamp(float fill_factor)
{
  if (!isfinite(fill_factor)) {
    fill_factor = 1.0f;
  }
  return max_ff(PG_FILL_MIN_FAC, min_ff(fill_factor, PG_FILL_MAX_FAC));
}

int pg_fill_leak_from_factor(float fill_factor)
{
  return (int)ceil(FILL_LEAK * pg_fill_factor_clamp(fill_factor));
}

int pg_fill_render_size(int win, float fill_factor)
{
  return max_ii((int)win * pg_fill_factor_clamp(fill_factor), MIN_WINDOW_SIZE);
}

void pg_fill_resample_mask(const float *src, int sw, int sh, float *dst, int dw, int dh)
{
  for (int y = 0; y < dh; y++) {
    const int y0 = (int)((long long)y * sh / dh);
    int y1 = (int)((long long)(y + 1) * sh / dh);
    if (y1 <= y0) {
      y1 = y0 + 1;
    }
    for (int x = 0; x < dw; x++) {
      const int x0 = (int)((long long)x * sw / dw);
      int x1 = (int)((long long)(x + 1) * sw / dw);
      if (x1 <= x0) {
        x1 = x0 + 1;
      }
      const float *best = &src[((size_t)y0 * sw + x0) * 4];
      for (int sy = y0; sy < y1 && sy < sh; sy++) {
        for (int sx = x0; sx < x1 && sx < sw; sx++) {
          const float *p = &src[((size_t)sy * sw + sx) * 4];
          if (p[3] > best[3]) {
            best = p;
          }
        }
      }
      copy_v4_v4(&dst[((size_t)y * dw + x) * 4], best);
    }
  }
}

int pg_fill_raster(float *rgba, int w, int h, int seed_x, int seed_y, float point_size, int fill_leak,
                   int dilate_pixels, float **r_xy, int *r_border_contact)
{
  if (r_xy) {
    *r_xy = NULL;
  }
  if (r_border_contact) {
    *r_border_contact = 0;
  }
  if (rgba == NULL || r_xy == NULL || w < 3 || h < 3 || seed_x < 0 || seed_x >= w || seed_y < 0 ||
      seed_y >= h)
  {
    return 0;
  }
  const size_t pixels = (size_t)w * (size_t)h;
  float *padded = MEM_callocN(sizeof(float[4]) * (pixels + 2 * (size_t)w), __func__);
  ImBuf ibuf = {w, h, padded + (size_t)w * 4};
  memcpy(ibuf.rect_float, rgba, sizeof(float[4]) * pixels);

  /* draw_mouse_position(): blue point (size point_size) drawn before the strokes. */
  const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  const float size = max_ff(point_size, 1.0f);
  const int px0 = (int)floorf((float)seed_x + 0.5f - size * 0.5f);
  const int py0 = (int)floorf((float)seed_y + 0.5f - size * 0.5f);
  const int pn = max_ii(1, (int)roundf(size));
  for (int y = max_ii(py0, 0); y < min_ii(py0 + pn, h); y++) {
    for (int x = max_ii(px0, 0); x < min_ii(px0 + pn, w); x++) {
      float *p = &ibuf.rect_float[((size_t)y * w + x) * 4];
      if (p[0] != 1.0f) {
        copy_v4_v4(p, blue);
      }
    }
  }

  BrushGpencilSettings settings = {0};
  settings.dilate_pixels = dilate_pixels;
  Brush brush = {{0}};
  brush.gpencil_settings = &settings;
  PGFillIma ima = {{0}};
  ima.ibuf = &ibuf;
  tGPDfill tgpf = {0};
  tgpf.ima = &ima;
  tgpf.brush = &brush;
  tgpf.fill_leak = max_ii(1, fill_leak);

  /* gpencil_do_frame_fill() without inversion. */
  int totpoints = 0;
  gpencil_set_borders(&tgpf, true);
  const bool border_contact = gpencil_boundaryfill_area(&tgpf);
  if (!border_contact) {
    gpencil_set_borders(&tgpf, false);
    gpencil_get_outline_points(&tgpf, true);
    totpoints = gpencil_points_from_stack(&tgpf);
    if (tgpf.stack) {
      BLI_stack_free(tgpf.stack);
    }
    if (totpoints > 0) {
      totpoints = tgpf.sbuffer_used;
      float *xy = malloc(sizeof(float[2]) * (size_t)totpoints);
      const tGPspoint *pt = tgpf.sbuffer;
      for (int i = 0; i < totpoints; i++) {
        xy[i * 2] = pt[i].m_xy[0];
        xy[i * 2 + 1] = pt[i].m_xy[1];
      }
      *r_xy = xy;
    }
    MEM_SAFE_FREE(tgpf.sbuffer);
  }
  else if (r_border_contact) {
    *r_border_contact = 1;
  }
  MEM_freeN(padded);
  return totpoints;
}

int pg_fill_extend_lines(bGPdata *gpd, int cfra, float extend_fac, int use_stroke_collide,
                         float px_scale, float px_ox, float px_oy, float canvas_cx, float canvas_cy,
                         float *out, int max_segments)
{
  if (gpd == NULL || out == NULL || max_segments <= 0 || !(extend_fac > 0.0f) ||
      !(px_scale > 0.0f))
  {
    return 0;
  }
  const float upb = PG_FILL_CANVAS_UNITS_PER_BU;

  /* World-space copy of the visible frames (canvas centre at the origin). */
  bGPdata *tmp = MEM_callocN(sizeof(bGPdata), __func__);
  tmp->mat = gpd->mat;
  tmp->totcol = gpd->totcol;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->flag & GP_LAYER_HIDE) {
      continue;
    }
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_USE_PREV);
    if (gpf == NULL) {
      continue;
    }
    bGPDlayer *tl = BKE_gpencil_layer_addnew(tmp, gpl->info, tmp->layers.first == NULL, false);
    bGPDframe *tf = BKE_gpencil_frame_addnew(tl, cfra);
    LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
      bGPDstroke *c = BKE_gpencil_stroke_duplicate(gps, true, false);
      for (int i = 0; i < c->totpoints; i++) {
        c->points[i].x = (c->points[i].x - canvas_cx) / upb;
        c->points[i].y = (c->points[i].y - canvas_cy) / upb;
        c->points[i].z = 0.0f;
      }
      BKE_gpencil_stroke_boundingbox_calc(c);
      BLI_addtail(&tf->strokes, c);
    }
  }
  if (tmp->layers.first == NULL) {
    MEM_freeN(tmp);
    return 0;
  }

  BrushGpencilSettings settings = {0};
  settings.fill_layer_mode = GP_FILL_GPLMODE_VISIBLE;
  Brush brush = {{0}};
  brush.gpencil_settings = &settings;
  tGPDfill tgpf = {0};
  tgpf.gpd = tmp;
  tgpf.brush = &brush;
  tgpf.active_cfra = cfra;
  tgpf.fill_extend_mode = GP_FILL_EMODE_EXTEND;
  tgpf.fill_extend_fac = extend_fac;
  tgpf.flag = use_stroke_collide ? GP_BRUSH_FILL_STROKE_COLLIDE : 0;
  tgpf.is_render = true;
  /* world -> view px: (w * upb + c) * px_scale + o */
  tgpf.gsc.scale = upb * px_scale;
  tgpf.gsc.ox = canvas_cx * px_scale + px_ox;
  tgpf.gsc.oy = canvas_cy * px_scale + px_oy;

  /* gpencil_update_extend() for GP_FILL_EMODE_EXTEND. */
  gpencil_load_array_strokes(&tgpf);
  int n = 0;
  if (tgpf.stroke_array != NULL) {
    gpencil_update_extensions_line(&tgpf);
    for (int i = 0; i < tgpf.stroke_array_num; i++) {
      bGPDstroke *ext[2] = {tgpf.stroke_array[i]->gps_ext_a, tgpf.stroke_array[i]->gps_ext_b};
      for (int e = 0; e < 2; e++) {
        if (ext[e] == NULL || n >= max_segments || !gpencil_stroke_is_drawable(&tgpf, ext[e])) {
          continue;
        }
        for (int p = 0; p < 2; p++) {
          out[n * 4 + p * 2] = ext[e]->points[p].x * upb + canvas_cx;
          out[n * 4 + p * 2 + 1] = ext[e]->points[p].y * upb + canvas_cy;
        }
        n++;
      }
    }
    stroke_array_free(&tgpf);
  }
  gpencil_delete_temp_stroke_extension(&tgpf, true);
  BKE_gpencil_free_layers(&tmp->layers);
  MEM_freeN(tmp);
  return n;
}
