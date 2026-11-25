/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Project Grease focused extraction from Blender 3.6.23 Legacy GP editor.
 *
 * This is the exact ED_gpencil_sbuffer_ensure algorithm from the pinned
 * Blender 3.6.23 source, isolated because the complete desktop gpencil_utils.c
 * pulls RNA/window-manager/editor dependencies that Project Grease deliberately
 * does not port.
 */

#include <stdbool.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "DNA_gpencil_legacy_types.h"
#include "ED_gpencil_legacy.h"
#include "DNA_meshdata_types.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

/* Exact constant from Blender 3.6.23 gpencil_intern.h.
 * Keep this local so the focused extraction does not pull the desktop
 * gpencil_intern.h -> ED_numinput.h dependency closure. */
#define GP_STROKE_BUFFER_CHUNK 2048

struct tGPspoint *ED_gpencil_sbuffer_ensure(struct tGPspoint *buffer_array,
                                            int *buffer_size,
                                            int *buffer_used,
                                            bool clear)
{
  struct tGPspoint *p = NULL;

  if (*buffer_used + 1 > *buffer_size) {
    if ((*buffer_size == 0) || (buffer_array == NULL)) {
      p = MEM_callocN(sizeof(struct tGPspoint) * GP_STROKE_BUFFER_CHUNK,
                      "GPencil Sbuffer");
      *buffer_size = GP_STROKE_BUFFER_CHUNK;
    }
    else {
      *buffer_size += GP_STROKE_BUFFER_CHUNK;
      p = MEM_recallocN(buffer_array,
                        sizeof(struct tGPspoint) * *buffer_size);
    }

    if (p == NULL) {
      *buffer_size = *buffer_used = 0;
    }

    buffer_array = p;
  }

  if (clear) {
    *buffer_used = 0;
    if (buffer_array != NULL) {
      memset(buffer_array, 0, sizeof(struct tGPspoint) * *buffer_size);
    }
  }

  return buffer_array;
}

/* The Draw commit's Subdivisions post-process (gpencil_stroke_newfrombuffer, gpencil_paint.c:1261),
 * declared in gpencil_intern.h. */
void gpencil_subdivide_stroke(bGPdata *gpd, bGPDstroke *gps, int subdivide);

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
void gpencil_subdivide_stroke(bGPdata *gpd, bGPDstroke *gps, const int subdivide)
{
  bGPDspoint *temp_points;
  int totnewpoints, oldtotpoints;
  int i2;

  /* loop as many times as levels */
  for (int s = 0; s < subdivide; s++) {
    totnewpoints = gps->totpoints - 1;
    /* duplicate points in a temp area */
    temp_points = MEM_dupallocN(gps->points);
    oldtotpoints = gps->totpoints;

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
    if (gps->dvert != NULL) {
      gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
    }

    /* move points from last to first to new place */
    i2 = gps->totpoints - 1;
    for (int i = oldtotpoints - 1; i > 0; i--) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[i2];

      copy_v3_v3(&pt_final->x, &pt->x);
      pt_final->pressure = pt->pressure;
      pt_final->strength = pt->strength;
      pt_final->time = pt->time;
      pt_final->flag = pt->flag;
      pt_final->uv_fac = pt->uv_fac;
      pt_final->uv_rot = pt->uv_rot;
      copy_v4_v4(pt_final->vert_color, pt->vert_color);

      if (gps->dvert != NULL) {
        MDeformVert *dvert = &gps->dvert[i];
        MDeformVert *dvert_final = &gps->dvert[i2];

        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = dvert->dw;
      }

      i2 -= 2;
    }
    /* interpolate mid points */
    i2 = 1;
    for (int i = 0; i < oldtotpoints - 1; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *next = &temp_points[i + 1];
      bGPDspoint *pt_final = &gps->points[i2];

      /* add a half way point */
      interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
      pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
      pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
      CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt_final->time = interpf(pt->time, next->time, 0.5f);
      pt_final->uv_fac = interpf(pt->uv_fac, next->uv_fac, 0.5f);
      pt_final->uv_rot = interpf(pt->uv_rot, next->uv_rot, 0.5f);
      interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);

      if (gps->dvert != NULL) {
        MDeformVert *dvert_final = &gps->dvert[i2];
        dvert_final->totweight = 0;
        dvert_final->dw = NULL;
      }

      i2 += 2;
    }

    MEM_SAFE_FREE(temp_points);

    /* move points to smooth stroke */
    /* duplicate points in a temp area with the new subdivide data */
    temp_points = MEM_dupallocN(gps->points);

    /* extreme points are not changed */
    for (int i = 0; i < gps->totpoints - 2; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *next = &temp_points[i + 1];
      bGPDspoint *pt_final = &gps->points[i + 1];

      /* move point */
      interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
    }
    /* free temp memory */
    MEM_SAFE_FREE(temp_points);
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}
/* END VERBATIM */
