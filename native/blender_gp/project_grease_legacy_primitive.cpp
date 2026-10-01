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
  for (int i = 0; i < count; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(count - 1);
    points.push_back(lerp(start, end, t));
  }
  return points;
}

std::vector<Point> rectangle(const Point& start, const Point& end, int edges)
{
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
    for (int k = 0; k < side_edges; ++k) {
      const float t = static_cast<float>(k) * step;
      points.push_back(lerp(corners[side], corners[side + 1], t));
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
  for (int i = 0; i < count; ++i) {
    const float angle = static_cast<float>(i) * step;
    points.push_back({center.x + std::cos(angle) * rx,
                      center.y + std::sin(angle) * ry});
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

  const float rotation = flip ? kHalfPi : -kHalfPi;
  const Point cp1 = rotate_around(flip ? start : end, midpoint, rotation);
  const Point corner{
      midpoint.x - (cp1.x - midpoint.x),
      midpoint.y - (cp1.y - midpoint.y),
  };

  std::vector<Point> points;
  points.reserve(count);
  for (int i = 0; i < count; ++i) {
    const float angle = static_cast<float>(i) * step;
    const float s = std::sin(angle);
    const float c = std::cos(angle);
    points.push_back({
        corner.x + (end.x - corner.x) * s + (start.x - corner.x) * c,
        corner.y + (end.y - corner.y) * s + (start.y - corner.y) * c,
    });
  }
  return points;
}

std::vector<Point> bezier(const Point& start,
                          const Point& control1,
                          const Point& control2,
                          const Point& end,
                          int edges)
{
  const int count = std::max(2, edges);
  std::vector<Point> points;
  points.reserve(count);
  for (int i = 0; i < count; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(count - 1);
    const float u = 1.0f - t;
    points.push_back({
        u * u * u * start.x + 3.0f * u * u * t * control1.x +
            3.0f * u * t * t * control2.x + t * t * t * end.x,
        u * u * u * start.y + 3.0f * u * u * t * control1.y +
            3.0f * u * t * t * control2.y + t * t * t * end.y,
    });
  }
  return points;
}

std::vector<Point> polyline(const std::vector<Point>& control_points, int edges_per_segment)
{
  if (control_points.empty()) {
    return {};
  }
  if (control_points.size() == 1) {
    return control_points;
  }

  const int edges = std::max(2, edges_per_segment);
  std::vector<Point> points;
  points.reserve((control_points.size() - 1) * edges);

  for (size_t segment = 0; segment + 1 < control_points.size(); ++segment) {
    const Point& a = control_points[segment];
    const Point& b = control_points[segment + 1];
    for (int i = 0; i < edges; ++i) {
      if (segment > 0 && i == 0) {
        continue;
      }
      const float t = static_cast<float>(i) / static_cast<float>(edges - 1);
      points.push_back(lerp(a, b, t));
    }
  }
  return points;
}

std::vector<Point> generate(int type,
                             const Point& start,
                             const Point& end,
                             float start_angle,
                             float end_angle,
                             int segments,
                             const Point& control1,
                             const Point& control2,
                             const std::vector<Point>& polyline_points)
{
  switch (type) {
    // Match Blender 3.6.23 gpencil_primitive_type[] exactly:
    // BOX=0, LINE=1, POLYLINE=2, CIRCLE=3, ARC=4, CURVE=5.
    case 0:
      return rectangle(start, end, segments == 64 ? 1 : segments);
    case 1:
      return line(start, end, segments);
    case 2:
      return polyline(polyline_points.empty() ? std::vector<Point>{start, end} : polyline_points,
                      segments);
    case 3:
      return circle(start, end, segments);
    case 4:
      return arc(start, end, segments, end_angle < start_angle);
    case 5:
      return bezier(start, control1, control2, end, segments);
    default:
      return {};
  }
}

}  // namespace project_grease::legacy_gp_primitive
