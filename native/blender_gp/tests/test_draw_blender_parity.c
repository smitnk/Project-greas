/* Draw input vs a real Blender 3.6.23 run. Reads the output of
 * tools/draw_input_golden/blender_draw_reference.py (committed as tests/draw_blender_golden.txt; CI
 * regenerates it with the Blender binary), replays every trace of every brush through
 * pg_draw_input_* (project_grease_draw_input.c over Blender's gpencil_paint.c code) and compares the
 * committed stroke points. Blender region pixels are y up: canvas (x, y) = (mx, -my), 1 px / unit.
 * usage: test_draw_blender_parity REFERENCE [REPORT] */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project_grease_draw_input.h"

#define MAXP 512
#define TOL_POS 5e-4f /* px: Blender's 3D round trip (win_to_delta, location_3d_to_region_2d) */
#define TOL_VAL 1e-5f

typedef struct Trace {
  char name[64];
  int nin, nout;
  float in[MAXP][4];
  float out[MAXP][5];
} Trace;

static int read_curve(char **save, PGCurve *c)
{
  const int n = atoi(strtok_r(NULL, " ", save));
  float xy[16];
  for (int i = 0; i < 2 * n && i < 16; i++) xy[i] = strtof(strtok_r(NULL, " ", save), NULL);
  return n >= 2 && n <= 8 && pg_curve_set(c, xy, n);
}

int main(int argc, char **argv)
{
  FILE *f = fopen(argc > 1 ? argv[1] : "draw_blender_golden.txt", "r");
  if (!f) { fprintf(stderr, "cannot open reference\n"); return 1; }
  FILE *rep = argc > 2 ? fopen(argv[2], "w") : NULL;
  static char line[4096];
  static Trace t;
  char brush[64] = "";
  PGDrawSettings s;
  int failures = 0, traces = 0, have = 0;
  float bmax[5] = {0};
  long pos = 0;
  for (;;) {
    char *l = fgets(line, sizeof line, f);
    const int is_brush = l && !strncmp(l, "brush ", 6);
    const int is_trace = l && !strncmp(l, "trace ", 6);
    if ((!l || is_brush || is_trace) && have) {
      /* replay the finished trace */
      PGDrawInput d;
      memset(&d, 0, sizeof d);
      pg_draw_input_begin(&d, &s);
      static PGDrawPoint got[MAXP * 4];
      int n = 0;
      const PGDrawPoint *o;
      for (int i = 0; i < t.nin; i++) {
        const int k = pg_draw_input_add(&d, t.in[i][0], -t.in[i][1], t.in[i][2], t.in[i][3], &o);
        for (int j = 0; j < k; j++) got[n++] = o[j];
      }
      const int k = pg_draw_input_end(&d, &o);
      for (int j = 0; j < k; j++) got[n++] = o[j];
      pg_draw_input_free(&d);
      float m[5] = {0};
      if (n != t.nout) {
        failures++;
        fprintf(stderr, "FAIL %s %s: %d points, Blender %d\n", brush, t.name, n, t.nout);
      }
      for (int i = 0; i < n && i < t.nout; i++) {
        const float e[5] = {fabsf(got[i].x - t.out[i][0]), fabsf(-got[i].y - t.out[i][1]),
                            fabsf(got[i].pressure - t.out[i][2]), fabsf(got[i].strength - t.out[i][3]),
                            fabsf(got[i].time - t.out[i][4])};
        for (int c = 0; c < 5; c++) m[c] = fmaxf(m[c], e[c]);
        if (getenv("DRAW_BLENDER_VERBOSE"))
          printf("%s %s %d: app (%g %g p%g s%g t%g) blender (%g %g p%g s%g t%g)\n", brush, t.name, i, got[i].x,
                 -got[i].y, got[i].pressure, got[i].strength, got[i].time, t.out[i][0], t.out[i][1], t.out[i][2],
                 t.out[i][3], t.out[i][4]);
      }
      if (m[0] > TOL_POS || m[1] > TOL_POS || m[2] > TOL_VAL || m[3] > TOL_VAL || m[4] > TOL_VAL) {
        failures++;
        fprintf(stderr, "FAIL %s %s: max |dx| %g |dy| %g |dpressure| %g |dstrength| %g |dtime| %g\n",
                brush, t.name, m[0], m[1], m[2], m[3], m[4]);
      }
      for (int c = 0; c < 5; c++) bmax[c] = fmaxf(bmax[c], m[c]);
      traces++;
      have = 0;
    }
    if ((!l || is_brush) && brush[0]) {
      char msg[256];
      snprintf(msg, sizeof msg, "BLENDER-RUN %s: max |dx| %.3g px, |dy| %.3g px, |dpressure| %.3g, |dstrength| %.3g, |dtime| %.3g\n",
               brush, bmax[0], bmax[1], bmax[2], bmax[3], bmax[4]);
      fputs(msg, stdout);
      if (rep) fputs(msg, rep);
      memset(bmax, 0, sizeof bmax);
    }
    if (!l) break;
    char *save;
    if (is_brush) {
      char *bar = strchr(l, '|');
      *bar = 0;
      snprintf(brush, sizeof brush, "%s", l + 6);
      pg_draw_settings_default(&s);
      s.euclidean_threshold = 2; /* U.gp_euclideandist (userdef_default.c) */
      s.synthesize_fast_points = 0; /* gpencil_draw_exec() never calls gpencil_add_fake_points() */
      s.draw_strength = strtof(strtok_r(bar + 1, " ", &save), NULL);
      s.use_pressure = atoi(strtok_r(NULL, " ", &save));
      s.use_strength_pressure = atoi(strtok_r(NULL, " ", &save));
      s.active_smooth = strtof(strtok_r(NULL, " ", &save), NULL);
      s.input_samples = atoi(strtok_r(NULL, " ", &save));
      s.jitter = strtof(strtok_r(NULL, " ", &save), NULL);
      s.draw_angle_factor = strtof(strtok_r(NULL, " ", &save), NULL);
      s.draw_angle = strtof(strtok_r(NULL, " ", &save), NULL);
      if (!read_curve(&save, &s.pressure_map) || !read_curve(&save, &s.strength_map)) {
        fprintf(stderr, "bad curve for %s\n", brush);
        return 1;
      }
    }
    else if (is_trace) {
      sscanf(l, "trace %63s %d %d", t.name, &t.nin, &t.nout);
      if (t.nin > MAXP || t.nout > MAXP) return 1;
      pos = 0;
      have = 1;
    }
    else if (!strncmp(l, "in ", 3)) {
      float *v = t.in[pos++];
      sscanf(l, "in %f %f %f %f", &v[0], &v[1], &v[2], &v[3]);
      if (pos == t.nin) pos = 0;
    }
    else if (!strncmp(l, "out ", 4)) {
      float *v = t.out[pos++];
      sscanf(l, "out %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4]);
    }
  }
  fclose(f);
  if (rep) fclose(rep);
  if (traces == 0) { fprintf(stderr, "no traces in the reference\n"); return 1; }
  if (failures) { fprintf(stderr, "%d draw trace(s) differ from Blender\n", failures); return 1; }
  printf("draw input matches Blender 3.6.23 on %d traces\n", traces);
  return 0;
}
