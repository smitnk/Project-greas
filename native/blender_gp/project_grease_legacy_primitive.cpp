#include "project_grease_legacy_primitive.h"

#include <algorithm>
#include <cmath>

namespace project_grease::legacy_gp_primitive {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kHalfPi = 0.5f * kPi;

Point lerp(const Point& a, const Point& b, float t)
{
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

Point rotate_around(const Point& point, const Point& center, float angle)
{
  const float x = point.x - center.x;
  const float y = point.y - center.y;
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  return {center.x + x * c - y * s, center.y + x * s + y * c};
}
}  // namespace

std::vector<Point> line(const Point& start, const Point& end, int edges)
{
  const int count = std::max(2, edges);
  std::vector<Point> points;
  points.reserve(count);

  // Blender 3.6.23 gpencil_primitive_line:
  // step = 1 / (tot_edges - 1), then interpolate start -> end.
  for (int i = 0; i < count; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(count - 1);
    points.push_back(lerp(start, end, t));
  }
  return points;
}

std::vector<Point> rectangle(const Point& start, const Point& end, int edges)
{
  // Blender's rectangle helper treats one edge as the four corners and,
  // for higher edge counts, subdivides each of the four sides.
  const int side_edges = std::max(1, edges);
  const Point corners[5] = {
      start,
      {end.x, start.y},
      end,
      {start.x, end.y},
      start,
  };

  std::vector<Point> points;
  points.reserve(side_edges == 1 ? 4 : side_edges * 4);

  if (side_edges == 1) {
    points.insert(points.end(), corners, corners + 4);
    return points;
  }

  const float step = 1.0f / static_cast<float>(side_edges);
  for (int side = 0; side < 4; ++side) {
    float t = 0.0f;
    for (int k = 0; k < side_edges; ++k) {
      points.push_back(lerp(corners[side], corners[side + 1], t));
      t += step;
    }
  }
  return points;
}

std::vector<Point> circle(const Point& start, const Point& end, int edges)
{
  const int count = std::max(2, edges);
  const float step = (2.0f * kPi) / static_cast<float>(count);
  const Point center{
      start.x + (end.x - start.x) * 0.5f,
      start.y + (end.y - start.y) * 0.5f,
  };
  const float rx = std::fabs(end.x - start.x) * 0.5f;
  const float ry = std::fabs(end.y - start.y) * 0.5f;

  std::vector<Point> points;
  points.reserve(count);
  float angle = 0.0f;
  for (int i = 0; i < count; ++i) {
    points.push_back({center.x + std::cos(angle) * rx,
                      center.y + std::sin(angle) * ry});
    angle += step;
  }
  return points;
}

std::vector<Point> arc(const Point& start, const Point& end, int edges, bool flip)
{
  const int count = std::max(2, edges);
  const float step = kHalfPi / static_cast<float>(count - 1);
  const Point midpoint{
      (start.x + end.x) * 0.5f,
      (start.y + end.y) * 0.5f,
  };

  // Blender 3.6.23 gpencil_primitive_update_cps initializes the Arc control
  // line by rotating end/start around their midpoint by +/- 90 degrees.
  const float rotation = flip ? kHalfPi : -kHalfPi;
  const Point cp1 = rotate_around(flip ? start : end, midpoint, rotation);
  const Point corner{
      midpoint.x - (cp1.x - midpoint.x),
      midpoint.y - (cp1.y - midpoint.y),
  };

  std::vector<Point> points;
  points.reserve(count);
  float angle = 0.0f;
  for (int i = 0; i < count; ++i) {
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    points.push_back({
        corner.x + (end.x - corner.x) * s + (start.x - corner.x) * c,
        corner.y + (end.y - corner.y) * s + (start.y - corner.y) * c,
    });
    angle += step;
  }
  return points;
}

std::vector<Point> generate(int type,
                             const Point& start,
                             const Point& end,
                             float start_angle,
                             float end_angle,
                             int segments)
{
  switch (type) {
    case 0:
      return line(start, end, segments);
    case 1:
      return rectangle(start, end, segments == 64 ? 1 : segments);
    case 2:
      return circle(start, end, segments);
    case 3:
      return arc(start, end, segments, end_angle < start_angle);
    default:
      return {};
  }
}

}  // namespace project_grease::legacy_gp_primitive
