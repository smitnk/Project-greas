#include "project_grease_legacy_sculpt.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

#include "BLI_math.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_brush.h"
#include "DNA_gpencil_legacy_types.h"

namespace project_grease::legacy_gp_sculpt {

namespace {

static float influence(const Context &ctx,
                       const Settings &settings,
                       float px,
                       float py)
{
  const int radius = std::max(1, static_cast<int>(std::lround(settings.radius)));
  const int mval_x = static_cast<int>(std::lround(ctx.mouse_x));
  const int mval_y = static_cast<int>(std::lround(ctx.mouse_y));
  const int point_x = static_cast<int>(std::lround(px));
  const int point_y = static_cast<int>(std::lround(py));
  const float distance = static_cast<float>(
      std::hypot(static_cast<float>(mval_x - point_x),
                 static_cast<float>(mval_y - point_y)));

  float value = std::max(0.0f, std::min(settings.brush_alpha, 1.0f));
  if (settings.pressure > 0.0f) {
    value *= std::max(0.0f, std::min(settings.pressure, 1.0f));
  }
  value *= std::max(0.0f, std::min(1.0f - distance / static_cast<float>(radius), 1.0f));
  value *= std::max(0.0f, settings.multiframe_falloff);
  return value;
}

static void apply_position_smooth(bGPDstroke *stroke, int point_index, float inf, int iterations)
{
  BKE_gpencil_stroke_smooth_point(
      stroke, point_index, inf, std::max(1, iterations), false, false, stroke);
}

static void apply_strength_smooth(bGPDstroke *stroke, int point_index, float inf, int iterations)
{
  BKE_gpencil_stroke_smooth_strength(
      stroke, point_index, inf, std::max(1, iterations), stroke);
}

static void apply_thickness_smooth(bGPDstroke *stroke, int point_index, float inf, int iterations)
{
  BKE_gpencil_stroke_smooth_thickness(
      stroke, point_index, inf, std::max(1, iterations), stroke);
}

}  // namespace

bool apply(bGPdata *gpd,
           bGPDframe *frame,
           bGPDstroke *stroke,
           Tool tool,
           const Context &context,
           const Settings &settings,
           int iterations)
{
  if (!gpd || !frame || !stroke || !stroke->points || stroke->totpoints <= 0 ||
      settings.radius <= 0.0f || !std::isfinite(settings.radius)) {
    return false;
  }

  bool changed = false;
  const float dir_x = context.mouse_x - context.prev_x;
  const float dir_y = context.mouse_y - context.prev_y;
  const float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
  const float nx = dir_len > 1e-5f ? -dir_y / dir_len : 0.0f;
  const float ny = dir_len > 1e-5f ? dir_x / dir_len : 0.0f;

  std::mt19937 rng(static_cast<uint32_t>(
      std::llround(context.mouse_x * 73856093.0f +
                   context.mouse_y * 19349663.0f +
                   context.prev_x * 83492791.0f)));
  std::uniform_real_distribution<float> random_unit(-1.0f, 1.0f);

  for (int i = 0; i < stroke->totpoints; ++i) {
    bGPDspoint &point = stroke->points[i];
    const float inf = influence(context, settings, point.x, point.y);
    if (inf <= 0.0f) {
      continue;
    }

    float amount = inf;
    if (settings.invert) {
      amount = -amount;
    }

    switch (tool) {
      case Smooth:
        if (settings.apply_position) {
          apply_position_smooth(stroke, i, inf, iterations);
        }
        if (settings.apply_strength) {
          apply_strength_smooth(stroke, i, inf, iterations);
        }
        if (settings.apply_thickness) {
          apply_thickness_smooth(stroke, i, inf, iterations);
        }
        if (settings.apply_uv) {
          BKE_gpencil_stroke_smooth_uv(stroke, i, inf, std::max(1, iterations), stroke);
        }
        changed = true;
        break;

      case Thickness:
        point.pressure = std::max(0.0f, point.pressure + amount / 10.0f);
        changed = true;
        break;

      case Strength:
        point.strength = std::max(0.0f, std::min(1.0f, point.strength + amount * 0.125f));
        changed = true;
        break;

      case Grab:
        point.x += context.delta_x * inf;
        point.y += context.delta_y * inf;
        changed = true;
        break;

      case Push: {
        const float dx = point.x - context.mouse_x;
        const float dy = point.y - context.mouse_y;
        const float length = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
        point.x += (dx / length) * context.delta_x * inf;
        point.y += (dy / length) * context.delta_y * inf;
        changed = true;
        break;
      }

      case Pinch: {
        const float fac = std::max(0.0f, 1.0f - amount * amount);
        point.x = context.mouse_x + (point.x - context.mouse_x) * fac;
        point.y = context.mouse_y + (point.y - context.mouse_y) * fac;
        changed = true;
        break;
      }

      case Twist: {
        const float angle = (settings.invert ? -1.0f : 1.0f) *
                            amount * float(M_PI / 180.0);
        const float dx = point.x - context.mouse_x;
        const float dy = point.y - context.mouse_y;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        point.x = context.mouse_x + dx * c - dy * s;
        point.y = context.mouse_y + dx * s + dy * c;
        changed = true;
        break;
      }

      case Randomize: {
        if (settings.apply_position) {
          const float displacement = random_unit(rng) * amount;
          point.x += nx * displacement;
          point.y += ny * displacement;
        }
        if (settings.apply_strength) {
          point.strength = std::max(
              0.0f, std::min(1.0f, point.strength + random_unit(rng) * amount));
        }
        if (settings.apply_thickness) {
          point.pressure = std::max(0.0f, point.pressure + random_unit(rng) * amount);
        }
        if (settings.apply_uv) {
          point.uv_rot = std::max(-float(M_PI_2),
                                  std::min(float(M_PI_2),
                                           point.uv_rot + random_unit(rng) * amount));
        }
        changed = true;
        break;
      }
    }
  }

  if (changed) {
    stroke->flag |= GP_STROKE_TAG;
    BKE_gpencil_stroke_geometry_update(gpd, stroke);
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  }

  return changed;
}

}  // namespace project_grease::legacy_gp_sculpt
