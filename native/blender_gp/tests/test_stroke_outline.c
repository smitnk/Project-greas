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
    CHECK(t.count == 4, "90 deg flat-capped: %d triangles, expected 4 (pure miter)", t.count);
    /* Consecutive outer edges meet: the outer corner vertex is shared by both quads. */
    int shared = 0;
    for (int i = 0; i < t.count * 3; i++) if (fabsf(t.xy[i * 2] - 35) < 1e-3f && fabsf(t.xy[i * 2 + 1] + 5) < 1e-3f) shared++;
    CHECK(shared >= 2, "outer miter corner (35,-5) not shared (%d)", shared);
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
  if (failures) { fprintf(stderr, "%d stroke outline check(s) failed\n", failures); return 1; }
  printf("stroke outline tests passed\n");
  return 0;
}
