#include "project_grease_legacy_eraser.h"

#include <algorithm>
#include <cmath>

#include "BLI_math_geom.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "DNA_gpencil_legacy_types.h"

namespace project_grease::legacy_gp_eraser {

namespace {

static float influence(float x,
                       float y,
                       int radius,
                       float point_x,
                       float point_y,
                       float draw_strength,
                       float pointer_pressure)
{
  if (radius <= 0) return 0.0f;
  const int mx = static_cast<int>(std::lround(x));
  const int my = static_cast<int>(std::lround(y));
  const int px = static_cast<int>(std::lround(point_x));
  const int py = static_cast<int>(std::lround(point_y));
  float distance = std::hypot(float(mx - px), float(my - py));
  distance = std::clamp(distance, 0.0f, float(radius));
  float factor = 1.0f - distance / float(radius);
  factor *= std::clamp(draw_strength, 0.0f, 1.0f);
  factor *= std::clamp(pointer_pressure, 0.01f, 1.0f);
  return factor;
}

static void soft_refine(bGPDstroke *stroke)
{
  if (!stroke || !stroke->points || stroke->totpoints < 3) return;
  for (int i = 1; i < stroke->totpoints - 1; ++i) {
    if ((stroke->points[i].flag & GP_SPOINT_TAG) &&
        !(stroke->points[i + 1].flag & GP_SPOINT_TAG)) {
      stroke->points[i].flag &= ~GP_SPOINT_TAG;
    }
  }
  for (int i = stroke->totpoints - 1; i > 0; --i) {
    if ((stroke->points[i].flag & GP_SPOINT_TAG) &&
        !(stroke->points[i - 1].flag & GP_SPOINT_TAG)) {
      stroke->points[i].flag &= ~GP_SPOINT_TAG;
    }
  }
}

} // namespace

bool process_stroke(bGPdata *gpd, bGPDframe *frame, bGPDstroke *stroke,
                    float x, float y, int radius, const Settings &settings)
{
  if (!gpd || !frame || !stroke || radius <= 0 || !stroke->points || stroke->totpoints <= 0) {
    return false;
  }

  if (settings.stroke_eraser) {
    for (int i = 0; i < stroke->totpoints; ++i) {
      if (influence(x, y, radius, stroke->points[i].x, stroke->points[i].y,
                    settings.draw_strength, settings.pointer_pressure) > 0.0f) {
        BKE_gpencil_stroke_delete_tagged_points(gpd, frame, stroke, stroke->next,
                                                 GP_SPOINT_TAG, false, false, 0);
        return true;
      }
    }
    return false;
  }

  bool changed = false;
  bool any_tagged = false;
  const float center[2] = {x, y};

  for (int i = 0; i + 1 < stroke->totpoints; ++i) {
    bGPDspoint *p0 = i > 0 ? &stroke->points[i - 1] : nullptr;
    bGPDspoint *p1 = &stroke->points[i];
    bGPDspoint *p2 = &stroke->points[i + 1];
    const float a[2] = {p1->x, p1->y};
    const float b[2] = {p2->x, p2->y};

    if (dist_squared_to_line_segment_v2(center, a, b) >= float(radius * radius)) {
      continue;
    }

    const float inf1 = influence(x, y, radius, p1->x, p1->y,
                                 settings.draw_strength, settings.pointer_pressure);
    const float inf2 = influence(x, y, radius, p2->x, p2->y,
                                 settings.draw_strength, settings.pointer_pressure);

    if (settings.soft) {
      constexpr float decrement = 0.1f;
      if (p0) {
        const float inf0 = influence(x, y, radius, p0->x, p0->y,
                                     settings.draw_strength, settings.pointer_pressure);
        p0->strength = std::max(0.0f, p0->strength - inf0 * decrement *
                                      std::clamp(settings.soft_strength, 0.0f, 1.0f) * 0.5f);
        p0->pressure = std::max(0.0f, p0->pressure - inf0 * decrement *
                                      std::clamp(settings.soft_thickness, 0.0f, 1.0f) * 0.5f);
      }
      p1->strength = std::max(0.0f, p1->strength - inf1 * decrement *
                                    std::clamp(settings.soft_strength, 0.0f, 1.0f));
      p1->pressure = std::max(0.0f, p1->pressure - inf1 * decrement *
                                    std::clamp(settings.soft_thickness, 0.0f, 1.0f));
      p2->strength = std::max(0.0f, p2->strength - inf2 * decrement *
                                    std::clamp(settings.soft_strength, 0.0f, 1.0f) * 0.5f);
      p2->pressure = std::max(0.0f, p2->pressure - inf2 * decrement *
                                    std::clamp(settings.soft_thickness, 0.0f, 1.0f) * 0.5f);

      if (p1->strength <= 0.01f || p1->pressure < 0.005f) {
        p1->flag |= GP_SPOINT_TAG; any_tagged = true;
      }
      if (p2->strength <= 0.01f || p2->pressure < 0.005f) {
        p2->flag |= GP_SPOINT_TAG; any_tagged = true;
      }
      changed = true;
    }
    else {
      if (inf1 > 0.0f) {
        p1->pressure = 0.0f;
        p1->flag |= GP_SPOINT_TAG;
        any_tagged = true;
      }
      if (inf2 > 0.0f) {
        p2->pressure = 0.0f;
        p2->flag |= GP_SPOINT_TAG;
        any_tagged = true;
      }
      changed = true;
    }
  }

  if (any_tagged) {
    if (settings.soft) soft_refine(stroke);
    BKE_gpencil_stroke_delete_tagged_points(
        gpd, frame, stroke, stroke->next, GP_SPOINT_TAG, false, false, 0);
  }
  return changed;
}

} // namespace project_grease::legacy_gp_eraser
