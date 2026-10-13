/* Host test of the lasso warp (Blender lattice deform, project_grease_blender_lattice.c). */
#include <math.h>
#include <stdio.h>

#include "DNA_key_types.h"
#include "project_grease_blender_lattice.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-3f)

static void rest(int nu, int nv, const float b[4], float *ctrl)
{
  for (int j = 0; j < nv; j++) for (int i = 0; i < nu; i++) {
    float r[2]; pg_lattice_rest_point(nu, nv, i, j, r);
    ctrl[2 * (j * nu + i)] = 0.5f * (b[0] + b[2]) + r[0] * 0.5f * (b[2] - b[0]);
    ctrl[2 * (j * nu + i) + 1] = 0.5f * (b[1] + b[3]) + r[1] * 0.5f * (b[3] - b[1]);
  }
}

int main(void)
{
  const float b[4] = {100, 200, 300, 400};
  float ctrl[2 * 16];
  /* untouched lattice: points stay (linear and B-spline) */
  float p[6] = {100, 200, 150, 260, 300, 400};
  rest(2, 2, b, ctrl);
  CHECK(pg_lattice_deform_points(p, 3, b, 2, 2, ctrl, KEY_LINEAR));
  CHECK(NEAR(p[0], 100) && NEAR(p[1], 200) && NEAR(p[2], 150) && NEAR(p[3], 260) && NEAR(p[4], 300));
  rest(4, 4, b, ctrl);
  float q[2] = {170, 330};
  CHECK(pg_lattice_deform_points(q, 1, b, 4, 4, ctrl, KEY_BSPLINE));
  CHECK(NEAR(q[0], 170) && NEAR(q[1], 330));
  /* 2x2 linear = bilinear: the (x1, y1) corner moved by (+40, +20) moves that corner fully, the
   * centre by a quarter and the opposite corner not at all */
  rest(2, 2, b, ctrl);
  ctrl[6] += 40; ctrl[7] += 20;
  float c[6] = {300, 400, 200, 300, 100, 200};
  CHECK(pg_lattice_deform_points(c, 3, b, 2, 2, ctrl, KEY_LINEAR));
  CHECK(NEAR(c[0], 340) && NEAR(c[1], 420));
  CHECK(NEAR(c[2], 210) && NEAR(c[3], 305));
  CHECK(NEAR(c[4], 100) && NEAR(c[5], 200));
  /* bad input */
  CHECK(!pg_lattice_deform_points(c, 1, b, 1, 2, ctrl, KEY_LINEAR));
  CHECK(!pg_lattice_deform_points(c, 1, b, 2, 2, ctrl, 99));
  printf(failures ? "%d FAILURES\n" : "lattice tests passed\n", failures);
  return failures ? 1 : 0;
}
