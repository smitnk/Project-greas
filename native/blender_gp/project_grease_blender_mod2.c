/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <math.h>
#include <stdlib.h>
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_blender_mod2.h"

static float clampf01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

int pg_build_visible(const int *totpoints, int n, int mode, int transition, float cfra, float start,
                     float length, int *r_visible)
{
  if (totpoints == NULL || r_visible == NULL || n <= 0 || !(length > 0) || !isfinite(cfra)) return 0;
  float f = clampf01((cfra - start) / length);
  if (transition == 1) f = 1.0f - f; /* shrink */
  int total = 0;
  if (mode == 1) { /* concurrent: every stroke grows at the same rate */
    for (int i = 0; i < n; i++) { r_visible[i] = (int)ceilf(f * totpoints[i] - 1e-6f); total += r_visible[i]; }
    return total;
  }
  int all = 0;
  for (int i = 0; i < n; i++) all += totpoints[i];
  int budget = (int)ceilf(f * all - 1e-6f);
  for (int i = 0; i < n; i++) { /* sequential: strokes appear one after another */
    int v = budget > totpoints[i] ? totpoints[i] : (budget < 0 ? 0 : budget);
    r_visible[i] = v; budget -= v; total += v;
  }
  return total;
}

static int pmod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }

int pg_time_offset_frame(int mode, int cfra, int offset, float scale, int use_range, int sfra, int efra, int loop)
{
  if (mode == 2) return offset; /* fixed */
  int f = (int)lroundf((float)cfra * scale) + offset;
  if (!use_range || efra < sfra) return f;
  const int len = efra - sfra + 1;
  switch (mode) {
    case 0: return loop ? sfra + pmod(f - sfra, len) : (f < sfra ? sfra : (f > efra ? efra : f));
    case 1: { int k = loop ? pmod(f - sfra, len) : (f - sfra < 0 ? 0 : (f - sfra >= len ? len - 1 : f - sfra)); return efra - k; }
    case 3: { if (len == 1) return sfra; int p = 2 * len - 2, k = pmod(f - sfra, p); return sfra + (k < len ? k : p - k); }
    default: return f;
  }
}

int pg_envelope_segments(int totpoints, int cyclic, int spread, int skip, int *pairs, int max_pairs)
{
  if (totpoints < 2 || spread < 1 || skip < 0 || pairs == NULL || max_pairs <= 0) return 0;
  int n = 0;
  for (int i = 0; i < totpoints && n < max_pairs; i += 1 + skip) {
    int j = i + spread;
    if (j >= totpoints) { if (!cyclic) break; j %= totpoints; if (j == i) continue; }
    pairs[2 * n] = i; pairs[2 * n + 1] = j; n++;
  }
  return n;
}

float pg_weight_angle(float ax, float ay, float bx, float by, float angle, int invert)
{
  float l = hypotf(bx - ax, by - ay);
  if (l < 1e-8f) return invert ? 1.0f : 0.0f;
  float dx = (bx - ax) / l, dy = (by - ay) / l;
  float w = fabsf(dx * cosf(angle) + dy * sinf(angle)); /* alignment with the reference direction */
  return invert ? 1.0f - w : w;
}

void pg_guide_snap(int type, float cx, float cy, float angle, float spacing, float sx, float sy, float x, float y, float r[2])
{
  r[0] = x; r[1] = y;
  switch (type) {
    case 0: { /* circular: keep the start point's distance from the center */
      float rad = hypotf(sx - cx, sy - cy), d = hypotf(x - cx, y - cy);
      if (d > 1e-6f) { r[0] = cx + (x - cx) * rad / d; r[1] = cy + (y - cy) * rad / d; }
      break;
    }
    case 1: { /* radial: line from the center through the start point */
      float dx = sx - cx, dy = sy - cy, l = hypotf(dx, dy);
      if (l > 1e-6f) { dx /= l; dy /= l; float t = (x - cx) * dx + (y - cy) * dy; r[0] = cx + dx * t; r[1] = cy + dy * t; }
      break;
    }
    case 2: case 4: { /* parallel to `angle` (isometric: nearest of 30/90/150 deg) through the start */
      float a = angle;
      if (type == 4) {
        float best = 1e9f; const float cand[3] = {(float)M_PI / 6, (float)M_PI / 2, 5 * (float)M_PI / 6};
        for (int k = 0; k < 3; k++) {
          float ux = cosf(cand[k]), uy = sinf(cand[k]);
          float t = (x - sx) * ux + (y - sy) * uy, ex = sx + ux * t - x, ey = sy + uy * t - y;
          if (ex * ex + ey * ey < best) { best = ex * ex + ey * ey; a = cand[k]; }
        }
      }
      float ux = cosf(a), uy = sinf(a), t = (x - sx) * ux + (y - sy) * uy;
      r[0] = sx + ux * t; r[1] = sy + uy * t;
      break;
    }
    case 3: /* grid: nearest grid line in each axis */
      if (spacing > 0) { r[0] = cx + spacing * roundf((x - cx) / spacing); r[1] = cy + spacing * roundf((y - cy) / spacing); }
      break;
  }
}

int pg_onion_keytype_filter(const int *key_types, int n, int filter, unsigned char *keep)
{
  if (key_types == NULL || keep == NULL || n <= 0) return 0;
  int k = 0;
  for (int i = 0; i < n; i++) { keep[i] = (filter < 0 || key_types[i] == filter); k += keep[i]; }
  return k;
}
