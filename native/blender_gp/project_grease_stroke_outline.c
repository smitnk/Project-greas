/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_stroke_outline.h. */
#include "project_grease_stroke_outline.h"

#include <math.h>
#include <stdlib.h>

typedef struct V2 {
  float x, y;
} V2;

static V2 v2(float x, float y)
{
  V2 r = {x, y};
  return r;
}
static V2 add(V2 a, V2 b) { return v2(a.x + b.x, a.y + b.y); }
static V2 sub(V2 a, V2 b) { return v2(a.x - b.x, a.y - b.y); }
static V2 mul(V2 a, float f) { return v2(a.x * f, a.y * f); }
static float dot2(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
static float cross2(V2 a, V2 b) { return a.x * b.y - a.y * b.x; }
static V2 perp(V2 a) { return v2(-a.y, a.x); } /* Blender: vec2(-line.y, line.x) */
static V2 safe_normalize(V2 a)
{
  const float l = sqrtf(dot2(a, a));
  return l > 1e-12f ? mul(a, 1.0f / l) : v2(1.0f, 0.0f);
}

typedef struct Out {
  float *xy;
  int count, max;
} Out;

static void tri(Out *o, V2 a, V2 b, V2 c)
{
  if (o->count >= o->max) {
    return;
  }
  float *t = o->xy + (size_t)o->count * 6;
  t[0] = a.x; t[1] = a.y; t[2] = b.x; t[3] = b.y; t[4] = c.x; t[5] = c.y;
  o->count++;
}

/* Fan around c from direction a0 (unit) turning by `angle` radians (signed). */
static void fan(Out *o, V2 c, float r, V2 a0, float angle, int arc_steps)
{
  int steps = (int)ceilf(fabsf(angle) / (float)M_PI * (float)arc_steps);
  if (steps < 1) {
    steps = 1;
  }
  const float base = atan2f(a0.y, a0.x);
  V2 prev = add(c, mul(a0, r));
  for (int k = 1; k <= steps; k++) {
    const float t = base + angle * (float)k / (float)steps;
    const V2 next = add(c, v2(cosf(t) * r, sinf(t) * r));
    tri(o, c, prev, next);
    prev = next;
  }
}

int pg_stroke_outline_max_triangles(int count, int arc_steps)
{
  if (count <= 0) {
    return 0;
  }
  if (arc_steps < 2) {
    arc_steps = 2;
  }
  /* 2 per segment (+1 closing), a join fan per point (< one half turn), two caps or a dot. */
  return (count + 1) * (2 + arc_steps + 1) + 2 * (arc_steps + 1) + 2 * arc_steps + 4;
}

int pg_stroke_outline(const PGOutlinePoint *points, int count, int flags, int arc_steps,
                      float *out_xy, int max_triangles)
{
  Out o = {out_xy, 0, max_triangles};
  if (!points || count <= 0 || !out_xy || max_triangles <= 0) {
    return 0;
  }
  if (arc_steps < 2) {
    arc_steps = 2;
  }
  /* Drop repeated points: they have no direction. */
  int *idx = (int *)malloc(sizeof(int) * (size_t)count);
  if (!idx) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < count; i++) {
    if (n > 0) {
      const PGOutlinePoint *q = &points[idx[n - 1]];
      if (fabsf(points[i].x - q->x) < 1e-6f && fabsf(points[i].y - q->y) < 1e-6f) {
        continue;
      }
    }
    idx[n++] = i;
  }
  int cyclic = (flags & PG_OUTLINE_CYCLIC) != 0;
  if (cyclic && n > 2) {
    const PGOutlinePoint *a = &points[idx[0]], *b = &points[idx[n - 1]];
    if (fabsf(a->x - b->x) < 1e-6f && fabsf(a->y - b->y) < 1e-6f) {
      n--;
    }
  }
  if (n < 3) {
    cyclic = 0;
  }
#define P(i) v2(points[idx[(i)]].x, points[idx[(i)]].y)
#define R(i) (points[idx[(i)]].radius > 0.0f ? points[idx[(i)]].radius : 0.0f)
  if (n == 1) {
    fan(&o, P(0), R(0), v2(1.0f, 0.0f), 2.0f * (float)M_PI, 2 * arc_steps);
    free(idx);
    return o.count;
  }
  const int segs = cyclic ? n : n - 1;
  /* Per segment: start offset and end offset (left side; the right side is the negation). */
  V2 *start_ofs = (V2 *)malloc(sizeof(V2) * (size_t)segs * 2);
  if (!start_ofs) {
    free(idx);
    return 0;
  }
  V2 *end_ofs = start_ofs + segs;
  for (int s = 0; s < segs; s++) {
    const V2 line = safe_normalize(sub(P((s + 1) % n), P(s)));
    start_ofs[s] = mul(perp(line), R(s));
    end_ofs[s] = mul(perp(line), R((s + 1) % n));
  }
  /* Joins at interior points (every point of a cyclic stroke). */
  for (int i = cyclic ? 0 : 1; i < (cyclic ? n : n - 1); i++) {
    const int s_prev = (i - 1 + segs) % segs, s_next = i % segs;
    const V2 line_adj = safe_normalize(sub(P(i), P((i - 1 + n) % n)));
    const V2 line = safe_normalize(sub(P((i + 1) % n), P(i)));
    V2 miter_tan = safe_normalize(add(line_adj, line));
    const float miter_dot = dot2(miter_tan, line_adj);
    const int miter_break = miter_dot < PG_OUTLINE_MITER_LIMIT;
    if (!miter_break) {
      const V2 miter = mul(perp(mul(miter_tan, 1.0f / miter_dot)), R(i));
      end_ofs[s_prev] = miter;
      start_ofs[s_next] = miter;
      continue;
    }
    /* Broken miter: bevel + round join on the outer side. Turning left (cross > 0) puts the outer
     * side on the right (-perp). */
    const float turn = cross2(line_adj, line);
    const V2 c = P(i);
    const V2 n_prev = perp(line_adj), n_next = perp(line);
    const V2 from = turn > 0.0f ? mul(n_prev, -1.0f) : n_prev;
    const V2 to = turn > 0.0f ? mul(n_next, -1.0f) : n_next;
    float angle = atan2f(cross2(from, to), dot2(from, to));
    fan(&o, c, R(i), from, angle, arc_steps);
  }
  for (int s = 0; s < segs; s++) {
    const V2 a = P(s), b = P((s + 1) % n);
    const V2 al = add(a, start_ofs[s]), ar = sub(a, start_ofs[s]);
    const V2 bl = add(b, end_ofs[s]), br = sub(b, end_ofs[s]);
    tri(&o, al, ar, bl);
    tri(&o, bl, ar, br);
  }
  if (!cyclic) {
    if (!(flags & PG_OUTLINE_FLAT_START)) {
      const V2 line = safe_normalize(sub(P(1), P(0)));
      fan(&o, P(0), R(0), perp(line), (float)M_PI, arc_steps); /* left, back, to right */
    }
    if (!(flags & PG_OUTLINE_FLAT_END)) {
      const V2 line = safe_normalize(sub(P(n - 1), P(n - 2)));
      fan(&o, P(n - 1), R(n - 1), mul(perp(line), -1.0f), (float)M_PI, arc_steps);
    }
  }
#undef P
#undef R
  free(start_ofs);
  free(idx);
  return o.count;
}
