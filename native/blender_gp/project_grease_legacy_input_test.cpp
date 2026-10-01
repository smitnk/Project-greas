#include <cassert>
#include <cmath>

static float legacy_pressure_curve(float input, float curve)
{
  input = std::fmax(0.0f, std::fmin(input, 1.0f));
  return std::pow(input, curve);
}

static bool legacy_spacing_accept(float x, float y, float last_x, float last_y, float spacing)
{
  if (spacing <= 0.0f) {
    return true;
  }
  const float dx = x - last_x;
  const float dy = y - last_y;
  return (dx * dx + dy * dy) >= spacing * spacing;
}

int main()
{
  // Contract copied from Blender 3.6.23 gpencil_paint.c pressure evaluation:
  // curve_sensitivity is evaluated on normalized pointer pressure.
  assert(std::fabs(legacy_pressure_curve(0.25f, 1.0f) - 0.25f) < 1e-6f);
  assert(std::fabs(legacy_pressure_curve(0.25f, 2.0f) - 0.0625f) < 1e-6f);

  // Contract for the distance gate used by Legacy GP input filtering:
  // sub-threshold samples are rejected, threshold-or-greater samples pass.
  assert(!legacy_spacing_accept(3.0f, 4.0f, 0.0f, 0.0f, 6.0f));
  assert(legacy_spacing_accept(3.0f, 4.0f, 0.0f, 0.0f, 5.0f));

  return 0;
}
