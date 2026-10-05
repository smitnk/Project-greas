/* Host test for pg_gpencil_stroke_trim (project_grease_stroke_trim.c) on the real pinned
 * BKE_gpencil_stroke_trim. Regression for the bug hunt's opTrim failure: a loop crossing back
 * exactly through one of the stroke's own sample points. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "DNA_gpencil_legacy_types.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "project_grease_stroke_trim.h"

static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } } while (0)

static bGPDstroke *make_stroke(const float (*xy)[2], int n)
{
  bGPDstroke *gps = BKE_gpencil_stroke_new(0, n, 10);
  for (int i = 0; i < n; i++) {
    gps->points[i].x = xy[i][0];
    gps->points[i].y = xy[i][1];
    gps->points[i].z = 0.0f;
    gps->points[i].pressure = 1.0f;
    gps->points[i].strength = 1.0f;
  }
  return gps;
}

static int near(float a, float b) { return fabsf(a - b) < 1e-5f; }

int main(void)
{
  bGPdata *gpd = MEM_callocN(sizeof(bGPdata), "gpd");

  /* 1. Crossing through sample point (1,0), shared end of segments 0 and 1: Blender alone misses it. */
  {
    const float xy[7][2] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {1, 1}, {1, -1}, {1, -2}};
    bGPDstroke *a = make_stroke(xy, 7);
    CHECK(!BKE_gpencil_stroke_trim(gpd, a), "precondition: BKE trim misses a vertex crossing");
    BKE_gpencil_free_stroke(a);

    bGPDstroke *gps = make_stroke(xy, 7);
    CHECK(pg_gpencil_stroke_trim(gpd, gps), "vertex crossing trimmed");
    CHECK(gps->totpoints == 6, "points 0..5 kept");
    CHECK(near(gps->points[0].x, 1) && near(gps->points[0].y, 0), "start moved onto the crossing");
    CHECK(near(gps->points[gps->totpoints - 1].x, 1) && near(gps->points[gps->totpoints - 1].y, 0),
          "end moved onto the crossing");
    BKE_gpencil_free_stroke(gps);
  }

  /* 2. A crossing inside both segments still goes through Blender's own trim (same result). */
  {
    const float xy[6][2] = {{6, 0}, {8, 2}, {6, 4}, {8, 0}, {9, 1}, {9, 2}};
    bGPDstroke *b = make_stroke(xy, 6);
    bGPDstroke *p = make_stroke(xy, 6);
    CHECK(BKE_gpencil_stroke_trim(gpd, b), "BKE trims an interior crossing");
    CHECK(pg_gpencil_stroke_trim(gpd, p), "pg trims an interior crossing");
    CHECK(b->totpoints == p->totpoints, "same point count as Blender");
    for (int i = 0; i < b->totpoints && i < p->totpoints; i++) {
      CHECK(near(b->points[i].x, p->points[i].x) && near(b->points[i].y, p->points[i].y), "same points as Blender");
    }
    BKE_gpencil_free_stroke(b);
    BKE_gpencil_free_stroke(p);
  }

  /* 3. No crossing, and a stroke whose last point returns to its first (closed, not crossing). */
  {
    const float open[5][2] = {{0, 0}, {1, 0}, {2, 0}, {3, 1}, {4, 3}};
    bGPDstroke *gps = make_stroke(open, 5);
    CHECK(!pg_gpencil_stroke_trim(gpd, gps) && gps->totpoints == 5, "no crossing: unchanged");
    BKE_gpencil_free_stroke(gps);
    const float closed[5][2] = {{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}};
    gps = make_stroke(closed, 5);
    CHECK(!pg_gpencil_stroke_trim(gpd, gps) && gps->totpoints == 5, "closed loop: unchanged");
    BKE_gpencil_free_stroke(gps);
  }

  MEM_freeN(gpd);
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("stroke trim tests passed\n");
  return 0;
}
