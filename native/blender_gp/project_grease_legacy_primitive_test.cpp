#include "project_grease_legacy_primitive.h"

#include <cassert>
#include <cmath>

using project_grease::legacy_gp_primitive::Point;

static bool near(float a, float b, float eps = 1e-4f)
{
  return std::fabs(a - b) <= eps;
}

int main()
{
  {
    const auto p = project_grease::legacy_gp_primitive::line({10.0f, 20.0f}, {30.0f, 40.0f}, 3);
    assert(p.size() == 3);
    assert(near(p.front().x, 10.0f) && near(p.front().y, 20.0f));
    assert(near(p[1].x, 20.0f) && near(p[1].y, 30.0f));
    assert(near(p.back().x, 30.0f) && near(p.back().y, 40.0f));
  }

  {
    const auto p = project_grease::legacy_gp_primitive::rectangle({10.0f, 20.0f}, {30.0f, 40.0f}, 1);
    assert(p.size() == 4);
    assert(near(p[0].x, 10.0f) && near(p[0].y, 20.0f));
    assert(near(p[1].x, 30.0f) && near(p[1].y, 20.0f));
    assert(near(p[2].x, 30.0f) && near(p[2].y, 40.0f));
    assert(near(p[3].x, 10.0f) && near(p[3].y, 40.0f));
  }

  {
    const auto p = project_grease::legacy_gp_primitive::rectangle({0.0f, 0.0f}, {8.0f, 4.0f}, 2);
    assert(p.size() == 8);
    assert(near(p[0].x, 0.0f) && near(p[0].y, 0.0f));
    assert(near(p[1].x, 4.0f) && near(p[1].y, 0.0f));
  }

  {
    const auto p = project_grease::legacy_gp_primitive::circle({0.0f, 0.0f}, {20.0f, 10.0f}, 8);
    assert(p.size() == 8);
    assert(near(p[0].x, 20.0f) && near(p[0].y, 5.0f));
    assert(near(p[2].x, 10.0f) && near(p[2].y, 10.0f));
  }

  {
    const auto p = project_grease::legacy_gp_primitive::arc({0.0f, 0.0f}, {10.0f, 10.0f}, 3, false);
    assert(p.size() == 3);
    assert(near(p.front().x, 0.0f) && near(p.front().y, 0.0f));
    assert(near(p.back().x, 10.0f) && near(p.back().y, 10.0f));
  }

  {
    const auto p = project_grease::legacy_gp_primitive::generate(
        0, {0.0f, 0.0f}, {10.0f, 10.0f}, 0.0f, 6.2831855f, 8);
    assert(p.size() == 8);
    assert(near(p.front().x, 0.0f) && near(p.front().y, 0.0f));
    assert(near(p.back().x, 10.0f) && near(p.back().y, 10.0f));
  }

  {
    const auto p = project_grease::legacy_gp_primitive::generate(
        1, {0.0f, 0.0f}, {10.0f, 20.0f}, 0.0f, 6.2831855f, 64);
    assert(p.size() == 4);
    assert(near(p[0].x, 0.0f) && near(p[0].y, 0.0f));
    assert(near(p[2].x, 10.0f) && near(p[2].y, 20.0f));
  }

  {
    const auto p = project_grease::legacy_gp_primitive::generate(
        3, {0.0f, 0.0f}, {10.0f, 10.0f}, 0.0f, 6.2831855f, 9);
    assert(p.size() == 9);
    assert(near(p.front().x, 0.0f) && near(p.front().y, 0.0f));
    assert(near(p.back().x, 10.0f) && near(p.back().y, 10.0f));
  }

  return 0;
}
