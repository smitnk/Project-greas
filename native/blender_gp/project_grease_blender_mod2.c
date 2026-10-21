/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <math.h>
#include <stdbool.h>
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

#include "project_grease_modifier_stack.h"

int pg_time_remap(const float *p, int scene_sfra, int scene_efra, int cfra)
{
  /* the TimeGpencilModifierData fields from the parameters (layer filters: the stack's influence block) */
  const bool custom = p[PG_P_TIME_USE_RANGE] != 0.0f;
  const int mode = (int)p[PG_P_TIME_MODE];
  const int keep_loop = p[PG_P_TIME_LOOP] != 0.0f;
  const float frame_scale = p[PG_P_TIME_SCALE];
  int segments_len = (int)p[PG_P_TIME_SEGMENTS];
  if (segments_len < 1) segments_len = 1;
  if (segments_len > PG_TIME_MAX_SEGMENTS) segments_len = PG_TIME_MAX_SEGMENTS;
  struct { int seg_start, seg_end, seg_mode, seg_repeat; } segments[PG_TIME_MAX_SEGMENTS];
  for (int i = 0; i < segments_len; i++) {
    const float *q = p + PG_P_TIME_SEG + 4 * i;
    segments[i].seg_start = (int)q[0]; segments[i].seg_end = (int)q[1];
    segments[i].seg_mode = (int)q[2]; segments[i].seg_repeat = (int)q[3] < 1 ? 1 : (int)q[3];
  }

  /* BEGIN VERBATIM source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_time.c remapTime()
   * (mmd->X -> the locals above; layer name / pass filters are the stack's filter block) */
  int sfra = custom ? (int)p[PG_P_TIME_SFRA] : scene_sfra;
  int efra = custom ? (int)p[PG_P_TIME_EFRA] : scene_efra;
  int offset = (int)p[PG_P_TIME_OFFSET];
  int nfra = 0;

  if (sfra < 0) sfra = 0;
  if (efra < 0) efra = 0;

  if (offset < 0) {
    offset = abs(efra - sfra + offset + 1);
  }
  /* Avoid inverse ranges. */
  if (efra < sfra) {
    return cfra;
  }

  /* apply frame scale */
  cfra *= frame_scale;
  if (cfra < 1) cfra = 1;

  /* if fix mode, return predefined frame number */
  if (mode == 2 /* GP_TIME_MODE_FIX */) {
    return offset;
  }

  if (mode == 0 /* GP_TIME_MODE_NORMAL */) {
    if (!keep_loop) {
      nfra = cfra + sfra + offset - 1 < efra ? cfra + sfra + offset - 1 : efra;
    }
    else {
      nfra = (offset + cfra - 1) % (efra - sfra + 1) + sfra;
    }
  }
  if (mode == 1 /* GP_TIME_MODE_REVERSE */) {
    if (!keep_loop) {
      nfra = efra - cfra - offset > sfra ? efra - cfra - offset + 1 : sfra;
    }
    else {
      nfra = (efra + 1 - (cfra + offset - 1) % (efra - sfra + 1)) - 1;
    }
  }

  if (mode == 3 /* GP_TIME_MODE_PINGPONG */) {
    if (efra == sfra) return sfra; /* Blender divides by zero here */
    if (!keep_loop) {
      if (((int)(cfra + offset - 1) / (efra - sfra)) % (2)) {
        nfra = efra - (cfra + offset - 1) % (efra - sfra);
      }
      else {
        nfra = sfra + (cfra + offset - 1) % (efra - sfra);
      }
      if (cfra > (efra - sfra) * 2) {
        nfra = sfra + offset;
      }
    }
    else {

      if (((int)(cfra + offset - 1) / (efra - sfra)) % (2)) {
        nfra = efra - (cfra + offset - 1) % (efra - sfra);
      }
      else {
        nfra = sfra + (cfra + offset - 1) % (efra - sfra);
      }
    }
  }

  if (mode == 4 /* GP_TIME_MODE_CHAIN */) {
    int sequence_length = 0;
    int frame_key = 0;
    int *segment_arr;
    int start, end;
    if (segments_len > 0) {
      for (int i = 0; i < segments_len; i++) {
        start = segments[i].seg_start;
        end = segments[i].seg_end;
        if (segments[i].seg_end < segments[i].seg_start) {
          start = segments[i].seg_end;
          end = segments[i].seg_start;
        }

        if (segments[i].seg_mode == 2 /* GP_TIME_SEG_MODE_PINGPONG */) {
          sequence_length += ((end - start) * segments[i].seg_repeat) * 2 + 1;
        }
        else {
          sequence_length += ((end - start + 1) * segments[i].seg_repeat);
        }
      }
      segment_arr = calloc((size_t)(sequence_length + 1), sizeof(int));
      if (segment_arr == NULL) return cfra;

      for (int i = 0; i < segments_len; i++) {

        if (segments[i].seg_end < segments[i].seg_start) {
          start = segments[i].seg_end;
          end = segments[i].seg_start;
        }
        else {
          start = segments[i].seg_start;
          end = segments[i].seg_end;
        }
        for (int a = 0; a < segments[i].seg_repeat; a++) {
          switch (segments[i].seg_mode) {
            case 0: /* GP_TIME_SEG_MODE_NORMAL */
              for (int b = 0; b < end - start + 1; b++) {
                segment_arr[frame_key] = start + b;
                frame_key++;
              }
              break;
            case 1: /* GP_TIME_SEG_MODE_REVERSE */
              for (int b = 0; b < end - start + 1; b++) {
                segment_arr[frame_key] = end - b;
                frame_key++;
              }
              break;
            case 2: /* GP_TIME_SEG_MODE_PINGPONG */
              for (int b = 0; b < end - start; b++) {
                segment_arr[frame_key] = start + b;
                frame_key++;
              }
              for (int b = 0; b < end - start; b++) {
                segment_arr[frame_key] = end - b;
                frame_key++;
                if (a == segments[i].seg_repeat - 1 && b == end - start - 1) {
                  segment_arr[frame_key] = start;
                  frame_key++;
                }
              }
              break;
          }
        }
      }

      /* a one-frame ping-pong segment counts a frame it never writes: Blender reads garbage there */
      if (frame_key == 0) { free(segment_arr); return cfra; }
      for (int k = frame_key; k < sequence_length; k++) segment_arr[k] = segment_arr[frame_key - 1];
      if (!keep_loop) {
        if ((cfra + offset - 1) < sequence_length) {
          nfra = segment_arr[(cfra - 1 + offset)];
        }
        else {
          nfra = segment_arr[frame_key - 1];
        }
      }
      else {
        nfra = segment_arr[(cfra - 1 + offset) % sequence_length];
      }

      free(segment_arr);
    }
  }

  return nfra;
  /* END VERBATIM */
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
