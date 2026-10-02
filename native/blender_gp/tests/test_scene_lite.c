/* Host test for Scene-lite (native/blender_gp/project_grease_scene_lite.c): OBJ loading, edge
 * building and the Line Art camera projection. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "project_grease_scene_lite.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)
#define NEAR(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))

static const char *CUBE =
    "# cube with two materials, a loose polyline and a second object\n"
    "o Cube\n"
    "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "vn 0 0 1\n"
    "usemtl Red\n"
    "f 1//1 2//1 3//1 4//1\nf 5 8 7 6\nf 1 5 6 2\n"
    "usemtl Blue\n"
    "f 2/1/1 6/1/1 7/1/1 3/1/1\nf 3 7 8 4\nf -4 -8 -5 -1\n"
    "o Wire\r\n"
    "v 3 0 0\nv 4 0 0\nv 5 0 0\n"
    "l 9 10 11\n";

static int count_flag(const PGMeshLite *me, int flag)
{
  int n = 0;
  for (int i = 0; i < me->totedge; i++) n += (me->edges[i].flag & flag) != 0;
  return n;
}

static void test_obj(void)
{
  PGSceneLite *s = pg_lite_scene_create();
  CHECK(pg_lite_load_obj(s, CUBE, (int)strlen(CUBE)) == 2, "two objects");
  CHECK(strcmp(s->objects[0].name, "Cube") == 0 && strcmp(s->objects[1].name, "Wire") == 0, "names");
  const PGMeshLite *cube = &s->objects[0].mesh;
  CHECK(cube->totvert == 8 && cube->tottri == 12 && cube->totpoly == 6, "cube counts");
  CHECK(cube->totedge == 18, "12 mesh edges + 6 quad diagonals");
  CHECK(count_flag(cube, PG_LITE_EDGE_POLY_INTERNAL) == 6, "diagonals flagged");
  CHECK(count_flag(cube, PG_LITE_EDGE_LOOSE) == 0 && count_flag(cube, PG_LITE_EDGE_NON_MANIFOLD) == 0, "closed manifold");
  int boundary = count_flag(cube, PG_LITE_EDGE_MATERIAL_BOUNDARY);
  CHECK(boundary == 8, "Red {z-, z+, y-} vs Blue {x+, y+, x-}: 3 + 3 + 2 boundary edges");
  for (int e = 0; e < cube->totedge; e++) {
    if (cube->edges[e].tri[0] < 0 || cube->edges[e].tri[1] < 0) { CHECK(0, "every cube edge has two triangles"); break; }
  }
  CHECK(cube->tri_poly[0] == 0 && cube->tri_poly[1] == 0 && cube->tri_poly[11] == 5, "triangles keep their polygon");
  CHECK(cube->tri_material[0] == 0 && cube->tri_material[11] == 1, "material per usemtl");
  /* Y-up -> Z-up: OBJ (-1,-1,-1) -> (-1, 1, -1) */
  CHECK(cube->verts[0][0] == -1.0f && cube->verts[0][1] == 1.0f && cube->verts[0][2] == -1.0f, "axis conversion");
  const PGMeshLite *wire = &s->objects[1].mesh;
  CHECK(wire->totvert == 3 && wire->tottri == 0 && wire->totedge == 2 && count_flag(wire, PG_LITE_EDGE_LOOSE) == 2, "loose polyline");
  int stats[5];
  pg_lite_stats(s, stats);
  CHECK(stats[0] == 2 && stats[1] == 11 && stats[2] == 12 && stats[3] == 20 && stats[4] == 2, "stats");

  CHECK(pg_lite_load_obj(s, "v 0 0 0\nf 1 2 3\n", (int)strlen("v 0 0 0\nf 1 2 3\n")) == 0 && s->totobject == 2, "bad index: nothing added");
  CHECK(pg_lite_load_obj(s, "v 0 0\n", (int)strlen("v 0 0\n")) == 0, "short vertex rejected");
  CHECK(pg_lite_load_obj(s, "# nothing\n", (int)strlen("# nothing\n")) == 0, "no geometry");
  pg_lite_scene_clear(s);
  CHECK(s->totobject == 0, "clear");
  pg_lite_scene_free(s);
}

static void test_camera(void)
{
  PGCameraLite cam;
  pg_lite_camera_default(&cam);
  const float target[3] = {1.0f, 2.0f, 0.5f};
  pg_lite_camera_orbit(&cam, target, 0.3f, 0.2f, 10.0f);
  double vp[4][4], sx, sy, fb[2];
  pg_lite_view_projection(&cam, 1920, 1080, 0.0f, vp);
  pg_lite_camera_shift(&cam, 1920, 1080, &sx, &sy);
  CHECK(pg_lite_project(vp, sx, sy, target, fb) && NEAR(fb[0], 0, 1e-5) && NEAR(fb[1], 0, 1e-5), "target at the center");

  /* Perspective, 16:9, auto fit -> horizontal, sensor 36, lens 50: the frame edge at distance d is
   * d * tan(fov / 2) = d * 18 / 50 along the camera X axis. */
  const float d = 10.0f, half = d * 18.0f / 50.0f;
  float edge[3];
  for (int i = 0; i < 3; i++) edge[i] = target[i] + cam.matrix_world[0][i] * half;
  CHECK(pg_lite_project(vp, sx, sy, edge, fb) && NEAR(fb[0], 1, 1e-4) && NEAR(fb[1], 0, 1e-4), "right frame edge -> fb x = 1");
  for (int i = 0; i < 3; i++) edge[i] = target[i] + cam.matrix_world[1][i] * half * 1080.0f / 1920.0f;
  CHECK(pg_lite_project(vp, sx, sy, edge, fb) && NEAR(fb[1], 1, 1e-4), "top frame edge -> fb y = 1");
  float behind[3];
  for (int i = 0; i < 3; i++) behind[i] = cam.matrix_world[3][i] + cam.matrix_world[2][i];
  CHECK(pg_lite_project(vp, sx, sy, behind, fb) == 0, "behind the camera");

  /* Portrait with auto fit -> vertical: sensor_x is the fitted size along Y. */
  pg_lite_view_projection(&cam, 1080, 1920, 0.0f, vp);
  for (int i = 0; i < 3; i++) edge[i] = target[i] + cam.matrix_world[1][i] * half;
  CHECK(pg_lite_project(vp, 0, 0, edge, fb) && NEAR(fb[1], 1, 1e-4), "portrait: top edge at d*18/50");

  /* Orthographic: ortho_scale 6 spans x -3..3 */
  cam.type = 1;
  pg_lite_view_projection(&cam, 1920, 1080, 0.0f, vp);
  for (int i = 0; i < 3; i++) edge[i] = target[i] + cam.matrix_world[0][i] * 3.0f + cam.matrix_world[1][i] * (3.0f * 1080.0f / 1920.0f);
  CHECK(pg_lite_project(vp, 0, 0, edge, fb) && NEAR(fb[0], 1, 1e-5) && NEAR(fb[1], 1, 1e-5), "ortho corner");

  /* Shift: horizontal fit keeps shift_x, scales shift_y by the aspect; fb moves by -2 * shift */
  cam.type = 0;
  cam.shift_x = 0.1f;
  cam.shift_y = 0.05f;
  pg_lite_view_projection(&cam, 1920, 1080, 0.0f, vp);
  pg_lite_camera_shift(&cam, 1920, 1080, &sx, &sy);
  CHECK(NEAR(sx, 0.1, 1e-7) && NEAR(sy, 0.05 * 1920.0 / 1080.0, 1e-6), "shift with horizontal fit");
  CHECK(pg_lite_project(vp, sx, sy, target, fb) && NEAR(fb[0], -0.2, 1e-5) && NEAR(fb[1], -2 * sy, 1e-5), "shift moves the frame");

  /* Camera scale is ignored (axes are normalized), like Line Art */
  PGCameraLite scaled = cam;
  for (int c = 0; c < 3; c++) for (int i = 0; i < 3; i++) scaled.matrix_world[c][i] *= 3.0f;
  double vp2[4][4];
  pg_lite_view_projection(&scaled, 1920, 1080, 0.0f, vp2);
  int same = 1;
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) same &= NEAR(vp[i][j], vp2[i][j], 1e-5);
  CHECK(same, "scaled camera projects the same");
}

int main(void)
{
  test_obj();
  test_camera();
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
