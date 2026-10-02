/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Vertex-color mixing for the Android presenter (android_gp_presentation.cpp), following Blender
 * 3.6.23 draw/engines/gpencil/shaders/gpencil_vert.glsl, gpencil_color_output():
 *
 *     mixed_col.rgb = mix(mixed_col.rgb, vert_col.rgb, vert_col.a * gpVertexColorOpacity);
 *     mixed_col.a  *= vert_strength * gpLayerOpacity;
 *
 * So a vertex color REPLACES the material color in proportion to its alpha (alpha 0 = no vertex
 * color = material color), and its alpha is never multiplied into the result's alpha. Strokes use
 * the point's vert_color; fills use the stroke's vert_color_fill with the material fill color.
 *
 * Limitation: the presenter issues one uniform color per draw call, so a stroke gets one color:
 * the mean of the per-point mixes (pg_gp_stroke_mean_mix), not a per-vertex gradient.
 *
 * gpVertexColorOpacity is pd->vertex_paint_opacity in the viewport (default 1.0); the presenter
 * uses 1.0. Plain C so the host tests can include it.
 */
#pragma once

#include <stddef.h>

#define PG_GP_VERTEX_COLOR_OPACITY 1.0f

static inline float pg_gp_clamp01(float v)
{
  if (!(v > 0.0f)) {
    return 0.0f; /* also maps NaN to 0 */
  }
  return v > 1.0f ? 1.0f : v;
}

/* mix(base_rgb, vert_rgba.rgb, vert_rgba.a * vertex_opacity) */
static inline void pg_gp_mix_vertex_color(const float base_rgb[3],
                                          const float vert_rgba[4],
                                          float vertex_opacity,
                                          float r_rgb[3])
{
  const float f = pg_gp_clamp01(pg_gp_clamp01(vert_rgba[3]) * vertex_opacity);
  for (int c = 0; c < 3; c++) {
    r_rgb[c] = base_rgb[c] * (1.0f - f) + pg_gp_clamp01(vert_rgba[c]) * f;
  }
}

/* Mean over `count` points of the per-point mix. `first_vert_color` points at the first point's
 * vert_color[4]; `stride_bytes` is sizeof(bGPDspoint). No points: the base color. */
static inline void pg_gp_stroke_mean_mix(const float base_rgb[3],
                                         const float *first_vert_color,
                                         size_t stride_bytes,
                                         int count,
                                         float vertex_opacity,
                                         float r_rgb[3])
{
  if (first_vert_color == NULL || count <= 0) {
    r_rgb[0] = base_rgb[0];
    r_rgb[1] = base_rgb[1];
    r_rgb[2] = base_rgb[2];
    return;
  }
  float sum[3] = {0.0f, 0.0f, 0.0f};
  for (int i = 0; i < count; i++) {
    const float *vc = (const float *)((const char *)first_vert_color + (size_t)i * stride_bytes);
    float mixed[3];
    pg_gp_mix_vertex_color(base_rgb, vc, vertex_opacity, mixed);
    sum[0] += mixed[0];
    sum[1] += mixed[1];
    sum[2] += mixed[2];
  }
  const float inv = 1.0f / (float)count;
  r_rgb[0] = sum[0] * inv;
  r_rgb[1] = sum[1] * inv;
  r_rgb[2] = sum[2] * inv;
}
