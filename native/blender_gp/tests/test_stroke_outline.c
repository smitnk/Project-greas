/* Host tests for the GLES stroke outline (project_grease_stroke_outline.h): no wedge gaps at bends,
 * straight strokes are a plain quad strip, sharp turns stay within Blender's miter limit. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "project_grease_stroke_outline.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

typedef struct Tris { float *xy; int count; } Tris;

static Tris build(const PGOutlinePoint *p, int n, int flags)
{
  Tris t;
  const int max = pg_stroke_outline_max_triangles(n, 8);
  t.xy = malloc(sizeof(float) * 6 * (size_t)max);
  t.count = pg_stroke_outline(p, n, flags, 8, t.xy, max);
  CHECK(t.count < max, "triangle bound reached (%d of %d)", t.count, max);
  return t;
}

static int inside(const float *t, float x, float y)
{
  const float d1 = (x - t[2]) * (t[1] - t[3]) - (t[0] - t[2]) * (y - t[3]);
  const float d2 = (x - t[4]) * (t[3] - t[5]) - (t[2] - t[4]) * (y - t[5]);
  const float d3 = (x - t[0]) * (t[5] - t[1]) - (t[4] - t[0]) * (y - t[1]);
  const int neg = (d1 < -1e-4f) || (d2 < -1e-4f) || (d3 < -1e-4f);
  const int pos = (d1 > 1e-4f) || (d2 > 1e-4f) || (d3 > 1e-4f);
  return !(neg && pos);
}
static int covered(const Tris *t, float x, float y)
{
  for (int i = 0; i < t->count; i++) {
    if (inside(t->xy + i * 6, x, y)) return 1;
  }
  return 0;
}
static float seg_dist(float px, float py, float ax, float ay, float bx, float by)
{
  const float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
  float t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  return hypotf(px - ax - t * dx, py - ay - t * dy);
}
/* Every sample within 0.97 r of the polyline must be covered: no gaps anywhere, bends included. */
static int uncovered_samples(const PGOutlinePoint *p, int n, const Tris *t, float r)
{
  float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
  for (int i = 0; i < n; i++) {
    minx = fminf(minx, p[i].x); maxx = fmaxf(maxx, p[i].x);
    miny = fminf(miny, p[i].y); maxy = fmaxf(maxy, p[i].y);
  }
  int bad = 0;
  for (float y = miny - r; y <= maxy + r; y += r / 16) {
    for (float x = minx - r; x <= maxx + r; x += r / 16) {
      float d = 1e9f;
      for (int i = 0; i + 1 < n; i++) d = fminf(d, seg_dist(x, y, p[i].x, p[i].y, p[i + 1].x, p[i + 1].y));
      if (d < 0.97f * r && !covered(t, x, y)) bad++;
    }
  }
  return bad;
}
static float max_vertex_reach(const PGOutlinePoint *p, int n, const Tris *t)
{
  float worst = 0;
  for (int i = 0; i < t->count * 3; i++) {
    float best = 1e9f;
    for (int k = 0; k < n; k++) best = fminf(best, hypotf(t->xy[i * 2] - p[k].x, t->xy[i * 2 + 1] - p[k].y));
    worst = fmaxf(worst, best);
  }
  return worst;
}
static void turn_case(float degrees)
{
  const float r = 50.0f, a = degrees * (float)M_PI / 180.0f;
  PGOutlinePoint p[3] = {{0, 0, r}, {300, 0, r}, {300 + 300 * cosf(a), 300 * sinf(a), r}};
  Tris t = build(p, 3, 0);
  const int bad = uncovered_samples(p, 3, &t, r);
  CHECK(bad == 0, "%.0f deg turn leaves %d uncovered samples (gap)", degrees, bad);
  /* Within Blender's miter limit: no vertex further than r / miter_limit from its point. */
  const float reach = max_vertex_reach(p, 3, &t);
  CHECK(reach <= r / PG_OUTLINE_MITER_LIMIT + 1e-3f, "%.0f deg turn reaches %.2f > %.2f", degrees, reach, r / PG_OUTLINE_MITER_LIMIT);
  free(t.xy);
}

int main(void)
{
  /* Straight line, flat caps: a plain quad strip (2 triangles per segment), width 2r. */
  {
    PGOutlinePoint p[4] = {{0, 0, 5}, {10, 0, 5}, {20, 0, 5}, {30, 0, 5}};
    Tris t = build(p, 4, PG_OUTLINE_FLAT_START | PG_OUTLINE_FLAT_END);
    CHECK(t.count == 6, "straight strip: %d triangles, expected 6", t.count);
    for (int i = 0; i < t.count * 3; i++) CHECK(fabsf(fabsf(t.xy[i * 2 + 1]) - 5) < 1e-4f, "strip vertex off the band edge");
    for (float x = 0.5f; x < 30; x += 1) CHECK(covered(&t, x, 4.8f) && covered(&t, x, -4.8f), "straight strip gap at x=%.1f", x);
    free(t.xy);
  }
  /* Straight line, round caps (default): strip + 2 half-disc fans. */
  {
    PGOutlinePoint p[2] = {{0, 0, 5}, {30, 0, 5}};
    Tris t = build(p, 2, 0);
    CHECK(t.count == 2 + 8 + 8, "round caps: %d triangles", t.count);
    CHECK(covered(&t, -4.5f, 0) && covered(&t, 34.5f, 0), "round caps missing");
    CHECK(!covered(&t, -5.5f, 0), "cap too large");
    free(t.xy);
  }
  turn_case(90);   /* miter join */
  turn_case(150);  /* broken miter: bevel + round join */
  turn_case(45);
  turn_case(179);  /* near hairpin */
  turn_case(-120);
  /* Miter vs break threshold: 90 deg is within the limit (no fan at the corner), 150 deg breaks. */
  {
    PGOutlinePoint p[3] = {{0, 0, 5}, {30, 0, 5}, {30, 30, 5}};
    Tris t = build(p, 3, PG_OUTLINE_FLAT_START | PG_OUTLINE_FLAT_END);
    CHECK(t.count == 5, "90 deg flat-capped: %d triangles, expected 5 (miter + outer wedge)", t.count);
    /* Consecutive outer edges meet: the outer corner vertex is shared by both quads. */
    int shared = 0;
    for (int i = 0; i < t.count * 3; i++) if (fabsf(t.xy[i * 2] - 35) < 1e-3f && fabsf(t.xy[i * 2 + 1] + 5) < 1e-3f) shared++;
    CHECK(shared >= 2, "outer miter corner (35,-5) not shared (%d)", shared);
    /* The missing outer wedge is bounded by the original segment endpoints
     * (30,-5), (35,0), and the miter intersection (35,-5). */
    int has_outer_prev = 0, has_outer_next = 0;
    for (int i = 0; i < t.count * 3; i++) {
      const float x = t.xy[i * 2], y = t.xy[i * 2 + 1];
      if (fabsf(x - 30) < 1e-3f && fabsf(y + 5) < 1e-3f) has_outer_prev = 1;
      if (fabsf(x - 35) < 1e-3f && fabsf(y) < 1e-3f) has_outer_next = 1;
    }
    CHECK(has_outer_prev && has_outer_next, "outer wedge endpoints are missing");
    free(t.xy);
  }
  /* Variable pressure, many points (freehand curve like the device report). */
  {
    PGOutlinePoint p[64];
    for (int i = 0; i < 64; i++) {
      const float a = (float)i / 63.0f * 3.0f;
      p[i].x = 400 + 300 * cosf(a); p[i].y = 400 + 300 * sinf(a); p[i].radius = 50;
    }
    Tris t = build(p, 64, 0);
    CHECK(uncovered_samples(p, 64, &t, 50) == 0, "freehand arc has gaps");
    free(t.xy);
  }
  /* Single point: a full disc. Cyclic square: closed, no caps, no gaps. */
  {
    PGOutlinePoint p[1] = {{0, 0, 3}};
    Tris t = build(p, 1, 0);
    CHECK(covered(&t, 2.8f, 0) && covered(&t, -2.8f, 0) && covered(&t, 0, 2.8f), "dot not a disc");
    free(t.xy);
    PGOutlinePoint q[4] = {{0, 0, 4}, {40, 0, 4}, {40, 40, 4}, {0, 40, 4}};
    Tris c = build(q, 4, PG_OUTLINE_CYCLIC);
    CHECK(covered(&c, 20, 3.5f) && covered(&c, 3.5f, 20) && covered(&c, -3.5f, -3.5f), "cyclic square not closed");
    free(c.xy);
  }
  /* Device report: a thick Box primitive drew as offset blocks with misaligned corners. The Box
   * primitive's points (4 per edge, project_grease_blender_primitive.c) closed cyclic: every corner is
   * a pure miter shared by both edges (no fans, no caps), the band has the exact width, and the
   * inside stays empty. */
  {
    PGOutlinePoint q[16];
    const float corners[5][2] = {{50, 30}, {150, 30}, {150, 90}, {50, 90}, {50, 30}};
    int k = 0;
    for (int e = 0; e < 4; e++) {
      for (int j = 0; j < 4; j++, k++) {
        q[k].x = corners[e][0] + (corners[e + 1][0] - corners[e][0]) * (float)j / 4.0f;
        q[k].y = corners[e][1] + (corners[e + 1][1] - corners[e][1]) * (float)j / 4.0f;
        q[k].radius = 8;
      }
    }
    Tris c = build(q, 16, PG_OUTLINE_CYCLIC);
    CHECK(c.count == 36, "box: %d triangles, expected 36 (16 segments + 4 outer wedges)", c.count);
    const float outer[4][2] = {{42, 22}, {158, 22}, {158, 98}, {42, 98}};
    const float inner[4][2] = {{58, 38}, {142, 38}, {142, 82}, {58, 82}};
    for (int i = 0; i < 4; i++) {
      int shared_o = 0, shared_i = 0;
      for (int v = 0; v < c.count * 3; v++) {
        if (fabsf(c.xy[v * 2] - outer[i][0]) < 1e-3f && fabsf(c.xy[v * 2 + 1] - outer[i][1]) < 1e-3f) shared_o++;
        if (fabsf(c.xy[v * 2] - inner[i][0]) < 1e-3f && fabsf(c.xy[v * 2 + 1] - inner[i][1]) < 1e-3f) shared_i++;
      }
      CHECK(shared_o >= 2 && shared_i >= 2, "box corner %d not a shared miter (outer %d, inner %d)", i, shared_o, shared_i);
      CHECK(covered(&c, outer[i][0] + (i == 0 || i == 3 ? 0.5f : -0.5f), outer[i][1] + (i < 2 ? 0.5f : -0.5f)),
            "box outer corner %d not filled", i);
    }
    CHECK(!covered(&c, 100, 60) && !covered(&c, 60, 50), "box inside filled");
    CHECK(!covered(&c, 41, 60) && !covered(&c, 100, 99), "box wider than its thickness");
    CHECK(uncovered_samples(q, 16, &c, 8) == 0, "box band has gaps");
    free(c.xy);
  }
  /* 90 deg corner at a primitive's sample spacing: the outer corner is one sharp miter vertex. */
  {
    PGOutlinePoint p[5] = {{0, 0, 10}, {25, 0, 10}, {50, 0, 10}, {50, 25, 10}, {50, 50, 10}};
    Tris t = build(p, 5, PG_OUTLINE_FLAT_START | PG_OUTLINE_FLAT_END);
    CHECK(t.count == 9, "90 deg primitive corner: %d triangles, expected 9 (outer wedge)", t.count);
    CHECK(covered(&t, 59.5f, -9.5f), "90 deg outer corner not filled");
    CHECK(!covered(&t, 60.5f, -10.5f), "90 deg corner overshoots the miter");
    free(t.xy);
  }
  if (failures) { fprintf(stderr, "%d stroke outline check(s) failed\n", failures); return 1; }
  printf("stroke outline tests passed\n");
  return 0;
}
