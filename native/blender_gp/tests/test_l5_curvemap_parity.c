/* L5 parity test: Project Grease's CurveMapping port (project_grease_curvemap.c) against Blender
 * 3.6.23's real colortools.c (BKE_curvemapping_*), on random 2..8 point curves with auto and vector
 * handles, evaluated below, inside and above [0, 1]. Results must be bit-identical. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "BKE_colortools.h"
#include "DNA_color_types.h"
#include "MEM_guardedalloc.h"

#include "project_grease_curvemap.h"

static unsigned int rng = 12345u;
static float frand(void)
{
  rng = rng * 1664525u + 1013904223u;
  return (float)(rng >> 8) / 16777216.0f;
}

int main(void)
{
  int failures = 0, evals = 0;
  for (int trial = 0; trial < 2000; trial++) {
    const int n = 2 + (int)(frand() * (PG_CURVE_MAX_POINTS - 1));
    float xy[2 * PG_CURVE_MAX_POINTS];
    for (int i = 0; i < n; i++) {
      xy[2 * i] = (float)i / (float)(n - 1) * 0.8f + 0.2f * frand(); /* increasing x */
      xy[2 * i + 1] = frand();
    }
    PGCurve pc;
    pg_curve_init_linear(&pc);
    if (!pg_curve_set(&pc, xy, n)) {
      printf("FAIL: pg_curve_set rejected trial %d\n", trial);
      failures++;
      continue;
    }
    /* Blender: one-channel mapping, as a brush / modifier curve (BKE_curvemapping_add(1, ...)). */
    CurveMapping *cm = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    CurveMap *cuma = &cm->cm[0];
    MEM_freeN(cuma->curve);
    cuma->totpoint = n;
    cuma->curve = MEM_callocN(n * sizeof(CurveMapPoint), "pts");
    for (int i = 0; i < n; i++) {
      cuma->curve[i].x = pc.x[i];
      cuma->curve[i].y = pc.y[i];
      if (pc.vector[i]) cuma->curve[i].flag |= CUMA_HANDLE_VECTOR;
    }
    BKE_curvemapping_changed(cm, false);
    BKE_curvemapping_init(cm);
    for (int k = -4; k <= 24; k++) {
      const float v = (float)k / 20.0f;
      const float a = pg_curve_evaluate(&pc, v);
      const float b = BKE_curvemapping_evaluateF(cm, 0, v);
      evals++;
      if (memcmp(&a, &b, sizeof(float)) != 0) {
        if (failures < 20) printf("FAIL: trial %d n=%d v=%g: PG %.9g Blender %.9g\n", trial, n, v, a, b);
        failures++;
      }
    }
    BKE_curvemapping_free(cm);
  }
  printf("%d evaluations compared with Blender 3.6.23 colortools.c\n", evals);
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
