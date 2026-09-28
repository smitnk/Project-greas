#pragma once

#include <vector>

namespace project_grease::legacy_gp_primitive {

struct Point {
  float x;
  float y;
};

std::vector<Point> line(const Point& start, const Point& end, int edges);
std::vector<Point> rectangle(const Point& start, const Point& end, int edges);
std::vector<Point> circle(const Point& start, const Point& end, int edges);
std::vector<Point> arc(const Point& start, const Point& end, int edges, bool flip);

}  // namespace project_grease::legacy_gp_primitive
