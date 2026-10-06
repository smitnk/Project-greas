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
  st.use_multiple_levels = 1;
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
  /* A plane between the camera and the left half of a cube (camera on -Y looking at it): the
   * cube's front edges that cross x = 0 are split into a hidden part (behind the plane) and a
   * visible part. OBJ (x, y, z) is Blender (x, -z, y): the plane is at y = -4, x in [-2, 0], z in [-1.5, 1.5], inside the frame. */
  const char *obj =
      "o Cube\n"
      "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
      "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n"
      "o Plane\n"
      "v -2 -1.5 4\nv 0 -1.5 4\nv 0 1.5 4\nv -2 1.5 4\n"
      "f 9 10 11 12\n";
  PGSceneLite *s = scene_with(obj);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.level_end = 1;
  st.use_multiple_levels = 1;
  const float target[3] = {0, 0, 0};
  pg_lite_camera_orbit(&s->camera, target, 0.0f, 0.0f, 12.0f);
  PGLineartSegment *seg = NULL;
  const int n = pg_lineart_compute(s, &st, &seg);
  int cube_visible = 0, cube_hidden = 0, cut_edges = 0, plane_lines = 0;
  for (int i = 0; i < n; i++) {
    if (!seg[i].edge_type) { CHECK(0, "every reported segment has an edge type"); break; }
    if (seg[i].object_index == 1) plane_lines++;
    if (seg[i].object_index != 0) continue;
    if (seg[i].occlusion == 0) cube_visible++;
    else cube_hidden++;
  }
  /* an edge cut by the plane: a visible and a hidden segment of the same edge */
  for (int i = 0; i < n; i++) {
    if (seg[i].object_index != 0) continue;
    for (int j = i + 1; j < n; j++) {
      if (seg[j].edge_index == seg[i].edge_index && (seg[i].occlusion == 0) != (seg[j].occlusion == 0)) {
        cut_edges++;
        break;
      }
    }
  }
  CHECK(plane_lines == 4, "the plane's outline");
  CHECK(cube_visible > 0 && cube_hidden > 0, "cube partly hidden by the plane");
  CHECK(cut_edges == 2, "the cube's top and bottom front edges are split into a visible and a hidden part");
  printf("  occluder: %d segments (cube: %d visible, %d hidden, %d edges cut)\n", n, cube_visible, cube_hidden, cut_edges);
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

static void test_strokes(void)
{
  PGSceneLite *s = scene_with(CUBE);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.stroke_depth_offset = 0.0f;
  PGLineartStrokes out;
  const int n = pg_lineart_compute_strokes(s, &st, &out);
  CHECK(n > 0 && n == out.stroke_count, "cube: chained strokes");
  int points = 0, on_edges = 1;
  for (int i = 0; i < out.stroke_count; i++) {
    CHECK(out.strokes[i].point_count >= 2 && out.strokes[i].level == 0 && out.strokes[i].object_index == 0, "stroke shape");
    points += out.strokes[i].point_count;
  }
  /* every point is on a cube edge: two of its coordinates are +-1 */
  for (int p = 0; p < out.point_count; p++) {
    int ones = 0;
    for (int k = 0; k < 3; k++) ones += fabs(fabs(out.world[p * 3 + k]) - 1.0f) < 1e-4f;
    if (ones < 2) on_edges = 0;
    if (fabs(out.image[p * 2]) > 1.0f || fabs(out.image[p * 2 + 1]) > 1.0f) on_edges = 0;
  }
  CHECK(points == out.point_count && on_edges, "stroke points lie on cube edges, inside the frame");
  /* the 9 visible edges are chained into fewer, longer strokes */
  CHECK(out.stroke_count < 9, "edges are chained");
  printf("  strokes: cube -> %d strokes, %d points\n", out.stroke_count, out.point_count);
  pg_lineart_free_strokes(&out);

  /* level 0..1: hidden edges become strokes too */
  st.level_end = 1;
  st.use_multiple_levels = 1;
  PGLineartStrokes all;
  pg_lineart_compute_strokes(s, &st, &all);
  int hidden = 0;
  for (int i = 0; i < all.stroke_count; i++) hidden += all.strokes[i].level == 1;
  CHECK(hidden > 0, "hidden-line strokes at level 1");
  pg_lineart_free_strokes(&all);

  /* stroke type filter: crease only */
  st.level_end = 0;
  st.use_multiple_levels = 1;
  st.stroke_types = CREASE;
  PGLineartStrokes creases;
  pg_lineart_compute_strokes(s, &st, &creases);
  for (int i = 0; i < creases.stroke_count; i++) CHECK(creases.strokes[i].edge_type & CREASE, "only crease strokes");
  pg_lineart_free_strokes(&creases);

  /* depth offset moves points towards the camera */
  pg_lineart_settings_default(&st);
  PGLineartStrokes off;
  pg_lineart_compute_strokes(s, &st, &off);
  const float *cam = s->camera.matrix_world[3];
  float d_off = 0, d_none = 0;
  st.stroke_depth_offset = 0.0f;
  PGLineartStrokes none;
  pg_lineart_compute_strokes(s, &st, &none);
  if (off.point_count > 0 && none.point_count > 0) {
    for (int k = 0; k < 3; k++) {
      d_off += (off.world[k] - cam[k]) * (off.world[k] - cam[k]);
      d_none += (none.world[k] - cam[k]) * (none.world[k] - cam[k]);
    }
  }
  CHECK(off.point_count > 0 && sqrtf(d_off) < sqrtf(d_none), "stroke depth offset");
  pg_lineart_free_strokes(&off);
  pg_lineart_free_strokes(&none);
  pg_lite_scene_free(s);
}

/* ---- Line Art options (item 6) ------------------------------------------------------------ */
enum { LIGHT_CONTOUR = 1 << 6, PROJECTED_SHADOW = 1 << 8 };
enum { SRC_COLLECTION = 0, SRC_OBJECT = 1, SRC_SCENE = 2 };
enum { MASK_ENABLE = 1 << 0, MASK_MATCH = 1 << 1, INTERSECTION_MATCH = 1 << 2 };
enum { CHAIN_LOOSE_EDGES = 1 << 12, CHAIN_GEOMETRY_SPACE = 1 << 13, INVERT_SOURCE_VGROUP = 1 << 7 };

typedef struct StrokeStats { int strokes, points, types, min_level, max_level; int per_object[4]; } StrokeStats;

static StrokeStats strokes_of(const PGSceneLite *s, const PGLineartSettings *st)
{
  StrokeStats r = {0};
  r.min_level = 1 << 20;
  r.max_level = -1;
  PGLineartStrokes out;
  r.strokes = pg_lineart_compute_strokes(s, st, &out);
  if (r.strokes < 0) return r;
  r.points = out.point_count;
  for (int i = 0; i < out.stroke_count; i++) {
    r.types |= out.strokes[i].edge_type;
    if (out.strokes[i].level < r.min_level) r.min_level = out.strokes[i].level;
    if (out.strokes[i].level > r.max_level) r.max_level = out.strokes[i].level;
    const int o = out.strokes[i].object_index;
    if (o >= 0 && o < 4) r.per_object[o]++;
  }
  pg_lineart_free_strokes(&out);
  return r;
}

/* Intersection strokes: with other types enabled, geometric chaining absorbs intersection edges
 * into contour / crease chains (lineart_chain.c), so look at intersection lines alone. */
static int intersection_strokes(const PGSceneLite *s, const PGLineartSettings *st)
{
  PGLineartSettings only = *st;
  only.edge_types = INTERSECTION;
  /* the intersection lines of these cubes lie on the faces they cross: count hidden ones too */
  only.use_multiple_levels = 1;
  only.level_start = 0;
  only.level_end = 2;
  return strokes_of(s, &only).strokes;
}

static const char *TWO_CUBES =
    "o A\nv -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n"
    "o B\nv 0 0 0\nv 2 0 0\nv 2 2 0\nv 0 2 0\nv 0 0 2\nv 2 0 2\nv 2 2 2\nv 0 2 2\n"
    "f 9 10 11 12\nf 13 16 15 14\nf 9 13 14 10\nf 10 14 15 11\nf 11 15 16 12\nf 13 9 12 16\n";

static void place_camera(PGSceneLite *s)
{
  const float target[3] = {0, 0, 0};
  pg_lite_camera_orbit(&s->camera, target, 0.8f, 0.45f, 12.0f);
}

static void test_usage_and_collections(void)
{
  PGSceneLite *s = scene_with(TWO_CUBES);
  place_camera(s);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  const StrokeStats base = strokes_of(s, &st);
  CHECK(base.per_object[0] > 0 && base.per_object[1] > 0 && intersection_strokes(s, &st) > 0, "two cubes: both objects and intersections");

  s->objects[1].line_art_usage = PG_LITE_USAGE_EXCLUDE;
  StrokeStats r = strokes_of(s, &st);
  CHECK(r.per_object[1] == 0 && intersection_strokes(s, &st) == 0, "object usage exclude");

  s->objects[1].line_art_usage = PG_LITE_USAGE_OCCLUSION_ONLY;
  r = strokes_of(s, &st);
  CHECK(r.per_object[1] == 0 && r.per_object[0] > 0 && intersection_strokes(s, &st) == 0, "object usage occlusion only");

  s->objects[1].line_art_usage = PG_LITE_USAGE_NO_INTERSECTION;
  r = strokes_of(s, &st);
  CHECK(r.per_object[1] > 0 && intersection_strokes(s, &st) == 0, "object usage no intersection");

  s->objects[1].line_art_usage = PG_LITE_USAGE_INTERSECTION_ONLY;
  r = strokes_of(s, &st);
  CHECK(r.per_object[1] == 0 && intersection_strokes(s, &st) > 0, "object usage intersection only");

  /* the same through a collection (object usage inherit) */
  s->objects[1].line_art_usage = PG_LITE_USAGE_INHERIT;
  const int parent = pg_lite_add_collection(s, "Parent", -1);
  const int child = pg_lite_add_collection(s, "Child", parent);
  s->objects[1].collection = child;
  r = strokes_of(s, &st);
  CHECK(r.per_object[1] == base.per_object[1], "collection include = default");
  s->collections[child].lineart_usage = PG_LITE_COLLECTION_EXCLUDE;
  r = strokes_of(s, &st);
  CHECK(r.per_object[1] == 0 && intersection_strokes(s, &st) == 0, "collection usage exclude");
  s->collections[child].lineart_usage = PG_LITE_COLLECTION_INCLUDE;
  s->collections[child].flag = PG_LITE_COLLECTION_HIDE_VIEWPORT;
  r = strokes_of(s, &st);
  CHECK(r.per_object[1] == 0, "hidden collection excludes its objects");
  s->collections[child].flag = 0;
  s->collections[child].lineart_usage = PG_LITE_COLLECTION_NO_INTERSECTION;
  r = strokes_of(s, &st);
  CHECK(r.per_object[1] > 0 && intersection_strokes(s, &st) == 0, "collection usage no intersection");
  s->collections[child].lineart_usage = PG_LITE_COLLECTION_INCLUDE;

  /* source: object / collection (with invert) */
  st.source_type = SRC_OBJECT;
  st.source_index = 0;
  r = strokes_of(s, &st);
  CHECK(r.per_object[0] == base.per_object[0] && r.per_object[1] == 0 && !(r.types & INTERSECTION), "source object");
  st.source_type = SRC_COLLECTION;
  st.source_index = parent; /* recursive: B is in the child */
  r = strokes_of(s, &st);
  CHECK(r.per_object[0] == 0 && r.per_object[1] == base.per_object[1], "source collection (recursive)");
  st.modifier_flags = 1 << 6; /* LRT_GPENCIL_INVERT_COLLECTION */
  r = strokes_of(s, &st);
  CHECK(r.per_object[0] == base.per_object[0] && r.per_object[1] == 0, "source collection inverted");
  st.source_index = 99;
  CHECK(strokes_of(s, &st).strokes == -1, "missing source collection disables the modifier");
  pg_lineart_settings_default(&st);

  /* intersection mask: collection mask bits on B's collection */
  s->collections[child].lineart_flags = PG_LITE_COLLECTION_USE_INTERSECTION_MASK;
  s->collections[child].lineart_intersection_mask = 1;
  st.intersection_mask = 2;
  CHECK(intersection_strokes(s, &st) == 0, "intersection mask filters other bits");
  st.intersection_mask = 1;
  CHECK(intersection_strokes(s, &st) > 0, "intersection mask keeps matching bits");
  st.mask_switches = INTERSECTION_MATCH;
  st.intersection_mask = 3;
  CHECK(intersection_strokes(s, &st) == 0, "intersection mask exact match");
  pg_lite_scene_free(s);
}

static const char *TWO_MATERIAL_CUBE =
    "o Cube\nv -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "usemtl Red\nf 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\n"
    "usemtl Blue\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n";

static void test_materials_crease_levels_vgroups(void)
{
  PGSceneLite *s = scene_with(TWO_MATERIAL_CUBE);
  place_camera(s);
  CHECK(s->totmaterial == 2 && pg_lite_find_material(s, "Blue") == 1, "OBJ materials in the scene");
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.edge_types |= MATERIAL;
  const StrokeStats base = strokes_of(s, &st);
  CHECK(base.strokes > 0, "material cube strokes");

  /* material mask: the mask bits of the faces a line is seen through (lineart_edge_cut), so with
   * levels 0..1 only the hidden lines behind Red (bit 1) faces stay */
  s->materials[0].lineart_flags = PG_LITE_MATERIAL_MASK_ENABLED;
  s->materials[0].material_mask_bits = 1;
  st.use_multiple_levels = 1;
  st.level_end = 1;
  st.mask_switches = MASK_ENABLE;
  st.material_mask_bits = 1;
  const StrokeStats red = strokes_of(s, &st);
  CHECK(red.strokes > 0 && red.min_level == 1, "material mask keeps lines behind the masked material");
  st.material_mask_bits = 2;
  CHECK(strokes_of(s, &st).strokes == 0, "material mask without matching bits");
  st.mask_switches = MASK_ENABLE | MASK_MATCH;
  st.material_mask_bits = 1;
  const StrokeStats match = strokes_of(s, &st);
  CHECK(match.strokes > 0 && match.points <= red.points, "material mask match");
  st.mask_switches = 0;
  st.use_multiple_levels = 0;
  st.level_end = 0;

  /* mat_occlusion 0 vs non-0: material boundary becomes contour */
  s->materials[1].mat_occlusion = 0;
  st.edge_types = CONTOUR;
  const StrokeStats occl = strokes_of(s, &st);
  s->materials[1].mat_occlusion = 1;
  const StrokeStats occl1 = strokes_of(s, &st);
  CHECK(occl.points != occl1.points, "material occlusion 0 changes contours");

  /* crease threshold: a small threshold drops the cube creases; per-object own crease wins */
  pg_lineart_settings_default(&st);
  st.edge_types = CREASE;
  CHECK(strokes_of(s, &st).strokes > 0, "cube creases at 140 degrees");
  st.crease_threshold = 0.2f;
  CHECK(strokes_of(s, &st).strokes == 0, "no creases at a small threshold");
  s->objects[0].line_art_flags = PG_LITE_OBJECT_OWN_CREASE;
  s->objects[0].line_art_crease_threshold = 2.4434609f;
  CHECK(strokes_of(s, &st).strokes > 0, "object own crease");
  s->objects[0].line_art_flags = 0;

  /* occlusion levels: single level vs range */
  pg_lineart_settings_default(&st);
  st.level_start = 1;
  st.level_end = 0;
  StrokeStats lv = strokes_of(s, &st);
  CHECK(lv.strokes > 0 && lv.min_level == 1 && lv.max_level == 1, "single level 1");
  st.use_multiple_levels = 1;
  st.level_start = 0;
  st.level_end = 1;
  lv = strokes_of(s, &st);
  CHECK(lv.min_level == 0 && lv.max_level == 1, "level range 0..1");

  /* vertex groups: prefix match, max weight, invert */
  pg_lineart_settings_default(&st);
  PGObjectLite *ob = &s->objects[0];
  const int g0 = pg_lite_object_add_vertex_group(ob, "other");
  const int g1 = pg_lite_object_add_vertex_group(ob, "line_a");
  CHECK(g0 == 0 && g1 == 1, "vertex groups added");
  for (int v = 0; v < ob->mesh.totvert; v++) ob->vgroups.weights[ob->mesh.totvert + v] = 0.75f;
  snprintf(st.source_vertex_group, sizeof(st.source_vertex_group), "line");
  PGLineartStrokes out;
  pg_lineart_compute_strokes(s, &st, &out);
  int all = out.point_count > 0;
  for (int p = 0; p < out.point_count; p++) all &= fabsf(out.weights[p] - 0.75f) < 1e-6f;
  CHECK(all, "vertex group weights transferred");
  pg_lineart_free_strokes(&out);
  st.calculation_flags |= INVERT_SOURCE_VGROUP;
  pg_lineart_compute_strokes(s, &st, &out);
  all = out.point_count > 0;
  for (int p = 0; p < out.point_count; p++) all &= fabsf(out.weights[p] - 0.25f) < 1e-6f;
  CHECK(all, "inverted vertex group weights");
  pg_lineart_free_strokes(&out);
  pg_lite_scene_free(s);
}

static const char *WIRE_ZIGZAG =
    "o W\nv 0 0 0\nv 1 0 0\nv 1.0005 0 0\nv 2 0 0.5\nl 1 2\nl 3 4\n";

static void test_chaining(void)
{
  PGSceneLite *s = scene_with(WIRE_ZIGZAG);
  place_camera(s);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.chaining_image_threshold = 0.0f;
  const StrokeStats apart = strokes_of(s, &st);
  st.calculation_flags |= CHAIN_LOOSE_EDGES;
  const StrokeStats apart_loose = strokes_of(s, &st);
  st.chaining_image_threshold = 0.01f;
  const StrokeStats joined = strokes_of(s, &st);
  st.calculation_flags &= ~CHAIN_LOOSE_EDGES;
  const StrokeStats not_loose = strokes_of(s, &st);
  CHECK(apart.strokes == 2 && apart_loose.strokes == 2, "ends too far apart for the threshold");
  CHECK(joined.strokes == 1, "chaining image threshold joins nearby loose ends");
  CHECK(not_loose.strokes == 2, "loose edges are not connected without loose edge chaining");
  st.calculation_flags |= CHAIN_GEOMETRY_SPACE | CHAIN_LOOSE_EDGES;
  CHECK(strokes_of(s, &st).strokes >= 1, "geometry space chaining runs");
  pg_lineart_settings_default(&st);
  st.chain_smooth_tolerance = 0.5f;
  CHECK(strokes_of(s, &st).strokes >= 1, "smooth tolerance runs");
  pg_lite_scene_free(s);
}

static const char *CUBE_ON_PLANE =
    "o Plane\nv -6 -1 -6\nv 6 -1 -6\nv 6 -1 6\nv -6 -1 6\nf 1 2 3 4\n"
    "o Cube\nv -1 0.5 -1\nv 1 0.5 -1\nv 1 2.5 -1\nv -1 2.5 -1\nv -1 0.5 1\nv 1 0.5 1\nv 1 2.5 1\nv -1 2.5 1\n"
    "f 5 6 7 8\nf 9 12 11 10\nf 5 9 10 6\nf 6 10 11 7\nf 7 11 12 8\nf 9 5 8 12\n";

static void sun_light(PGSceneLite *s, int type)
{
  /* light at (3, -2, 10) looking at the origin (-Z towards it), Z up like the camera orbit */
  PGCameraLite tmp;
  pg_lite_camera_default(&tmp);
  const float target[3] = {0, 0, 0};
  pg_lite_camera_orbit(&tmp, target, 0.4f, 1.2f, 12.0f);
  memcpy(s->light.matrix_world, tmp.matrix_world, sizeof(tmp.matrix_world));
  s->light.present = 1;
  s->light.type = type;
}

static void test_shadow_and_light_contour(void)
{
  PGSceneLite *s = scene_with(CUBE_ON_PLANE);
  place_camera(s);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.edge_types |= LIGHT_CONTOUR | PROJECTED_SHADOW;
  StrokeStats r = strokes_of(s, &st);
  CHECK(r.strokes > 0 && !(r.types & (LIGHT_CONTOUR | PROJECTED_SHADOW)), "no light: no shadow lines");
  for (int type = PG_LITE_LIGHT_POINT; type <= PG_LITE_LIGHT_SUN; type++) {
    sun_light(s, type);
    r = strokes_of(s, &st);
    CHECK((r.types & PROJECTED_SHADOW) != 0, type ? "sun: projected shadow" : "point: projected shadow");
    CHECK((r.types & LIGHT_CONTOUR) != 0, type ? "sun: light contour" : "point: light contour");
  }
  /* shadow selection (lit / shaded / enclosed shapes) and silhouette run and filter */
  for (int sel = 1; sel <= 3; sel++) {
    st.shadow_selection = sel;
    const StrokeStats f = strokes_of(s, &st);
    CHECK(f.strokes >= 0 && f.points <= r.points * 4, "shadow selection runs");
  }
  st.shadow_selection = 0;
  pg_lineart_settings_default(&st);
  const StrokeStats nosil = strokes_of(s, &st);
  st.silhouette_selection = 1; /* LRT_SILHOUETTE_FILTER_GROUP */
  const StrokeStats sil = strokes_of(s, &st);
  CHECK(sil.strokes > 0 && sil.points <= nosil.points, "silhouette filter");
  printf("  shadow: %d strokes with light contour / projected shadow\n", r.strokes);
  pg_lite_scene_free(s);
}

int main(int argc, char **argv)
{
  test_strokes();
  test_cube();
  test_occluder_cuts_lines();
  test_intersections();
  test_loose_and_empty();
  test_usage_and_collections();
  test_materials_crease_levels_vgroups();
  test_chaining();
  test_shadow_and_light_contour();
  if (argc > 1) {
    failures += pg_lineart_reference_compare(argv[1]);
  }
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
