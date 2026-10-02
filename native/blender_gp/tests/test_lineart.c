/* Host test for Line Art batch 2: Blender 3.6.23's Line Art core (generated verbatim regions) on
 * Scene-lite. Analytic checks of feature lines and occlusion; with a reference directory argument it
 * also compares against Blender's own Line Art output (tools/lineart_reference). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project_grease_lineart_lite.h"
#include "project_grease_scene_lite.h"

int pg_lineart_reference_compare(const char *dir); /* test_lineart_reference.c */

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

/* LRT_EDGE_FLAG_* (DNA_lineart_types.h) */
enum { CONTOUR = 1 << 1, CREASE = 1 << 2, MATERIAL = 1 << 3, INTERSECTION = 1 << 4, LOOSE = 1 << 5 };

static const char *CUBE =
    "o Cube\n"
    "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n";

static PGSceneLite *scene_with(const char *obj)
{
  PGSceneLite *s = pg_lite_scene_create();
  if (pg_lite_load_obj(s, obj, (int)strlen(obj)) <= 0) {
    printf("FAIL: OBJ did not load\n");
    failures++;
  }
  return s;
}

typedef struct Summary { int segments, visible, occluded, types; double visible_length; } Summary;

static Summary run(PGSceneLite *s, const PGLineartSettings *st, PGLineartSegment **keep)
{
  Summary r = {0};
  PGLineartSegment *seg = NULL;
  const int n = pg_lineart_compute(s, st, &seg);
  r.segments = n;
  for (int i = 0; i < n; i++) {
    r.types |= seg[i].edge_type;
    if (seg[i].occlusion == 0) {
      r.visible++;
      r.visible_length += hypot(seg[i].x1 - seg[i].x0, seg[i].y1 - seg[i].y0);
    }
    else {
      r.occluded++;
    }
  }
  if (keep) *keep = seg;
  else pg_lineart_free_segments(seg);
  return r;
}

static void test_cube(void)
{
  PGSceneLite *s = scene_with(CUBE);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  /* Default camera: three faces seen from a corner -> 9 visible edges, 3 hidden. With flat faces
   * every cube edge is a crease (90 degrees > 140 degree threshold); the outline is contour. */
  Summary r = run(s, &st, NULL);
  CHECK(r.segments == 12 && r.visible == 9 && r.occluded == 3, "cube from a corner: 9 visible, 3 hidden edges");
  CHECK((r.types & CONTOUR) && (r.types & CREASE), "contour and crease types");

  /* level_end 1 computes occlusion levels up to 1: hidden edges are behind exactly one face. */
  st.level_end = 1;
  PGLineartSegment *seg = NULL;
  const int n = pg_lineart_compute(s, &st, &seg);
  int level1 = 0;
  for (int i = 0; i < n; i++) level1 += seg[i].occlusion == 1;
  CHECK(level1 == 3, "hidden edges have occlusion level 1");
  pg_lineart_free_segments(seg);

  /* Looking straight at a face (orthographic): 4 visible outline edges. */
  pg_lineart_settings_default(&st);
  s->camera.type = 1;
  const float target[3] = {0, 0, 0};
  pg_lite_camera_orbit(&s->camera, target, 0.0f, 0.0f, 10.0f);
  r = run(s, &st, NULL);
  CHECK(r.visible == 4, "front orthographic view: 4 visible edges");
  printf("  ortho front: %d segments, %d visible, visible length %.4f\n", r.segments, r.visible, r.visible_length);
  pg_lite_scene_free(s);
}

static void test_occluder_cuts_lines(void)
{
  /* A cube behind a plane that covers its left half: the cube's edges are split into a hidden part
   * (behind the plane) and a visible part. */
  const char *obj =
      "o Cube\n"
      "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
      "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n"
      "o Plane\n"
      "v -3 -3 4\nv 0 -3 4\nv 0 3 4\nv -3 3 4\n"
      "f 9 10 11 12\n";
  PGSceneLite *s = scene_with(obj);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.level_end = 1;
  const float target[3] = {0, 0, 0};
  pg_lite_camera_orbit(&s->camera, target, 0.3f, 0.35f, 12.0f);
  PGLineartSegment *seg = NULL;
  const int n = pg_lineart_compute(s, &st, &seg);
  int cube_cut = 0, cube_hidden = 0, cube_visible = 0;
  for (int i = 0; i < n; i++) {
    if (seg[i].object_index != 0) continue;
    if (seg[i].occlusion == 0) cube_visible++;
    else cube_hidden++;
  }
  /* edges with more than one segment */
  for (int i = 0; i + 1 < n; i++) {
    if (seg[i].object_index == 0 && seg[i + 1].object_index == 0 && seg[i].x1 == seg[i + 1].x0 &&
        seg[i].y1 == seg[i + 1].y0 && seg[i].occlusion != seg[i + 1].occlusion)
    {
      cube_cut++;
    }
  }
  CHECK(cube_visible > 0 && cube_hidden > 0, "cube partly hidden by the plane");
  CHECK(cube_cut > 0, "edges cut where the plane's border crosses them");
  printf("  occluder: %d segments (cube: %d visible, %d hidden, %d cuts)\n", n, cube_visible, cube_hidden, cube_cut);
  pg_lineart_free_segments(seg);
  pg_lite_scene_free(s);
}

static void test_intersections(void)
{
  /* Two cubes overlapping: Line Art adds intersection lines where their faces cross. */
  const char *obj =
      "o A\n"
      "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
      "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n"
      "o B\n"
      "v 0 0 0\nv 2 0 0\nv 2 2 0\nv 0 2 0\nv 0 0 2\nv 2 0 2\nv 2 2 2\nv 0 2 2\n"
      "f 9 10 11 12\nf 13 16 15 14\nf 9 13 14 10\nf 10 14 15 11\nf 11 15 16 12\nf 13 9 12 16\n";
  PGSceneLite *s = scene_with(obj);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  Summary r = run(s, &st, NULL);
  CHECK(r.types & INTERSECTION, "intersection lines between overlapping cubes");
  st.edge_types &= ~INTERSECTION;
  Summary r2 = run(s, &st, NULL);
  CHECK(!(r2.types & INTERSECTION) && r2.segments < r.segments, "intersection type can be turned off");
  pg_lite_scene_free(s);
}

static void test_loose_and_empty(void)
{
  PGSceneLite *s = scene_with("o Wire\nv -1 0 0\nv 1 0 0\nv 1 0 1\nl 1 2 3\n");
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  Summary r = run(s, &st, NULL);
  CHECK(r.segments == 2 && r.visible == 2 && r.types == LOOSE, "loose edges");
  pg_lite_scene_clear(s);
  PGLineartSegment *seg = NULL;
  CHECK(pg_lineart_compute(s, &st, &seg) == 0, "empty scene: no segments");
  pg_lineart_free_segments(seg);
  /* object behind the camera: discarded by the bound box check */
  pg_lite_load_obj(s, CUBE, (int)strlen(CUBE));
  for (int i = 0; i < 3; i++) s->objects[0].matrix_world[3][i] = s->camera.matrix_world[3][i] + s->camera.matrix_world[2][i] * 5.0f;
  CHECK(pg_lineart_compute(s, &st, &seg) == 0, "object behind the camera: no segments");
  pg_lineart_free_segments(seg);
  pg_lite_scene_free(s);
}

int main(int argc, char **argv)
{
  test_cube();
  test_occluder_cuts_lines();
  test_intersections();
  test_loose_and_empty();
  if (argc > 1) {
    failures += pg_lineart_reference_compare(argv[1]);
  }
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
