#include <cassert>
#include <cmath>

static float legacy_influence(float distance, float radius, float strength, float pressure)
{
  if (radius <= 0.0f) return 0.0f;
  const float d = std::fmax(0.0f, std::fmin(distance, radius));
  float value = 1.0f - d / radius;
  value *= std::fmax(0.0f, std::fmin(strength, 1.0f));
  value *= std::fmax(0.01f, std::fmin(pressure, 1.0f));
  return value;
}

int main()
{
  assert(std::fabs(legacy_influence(0.0f, 10.0f, 1.0f, 1.0f) - 1.0f) < 1e-6f);
  assert(std::fabs(legacy_influence(5.0f, 10.0f, 1.0f, 1.0f) - 0.5f) < 1e-6f);
  assert(std::fabs(legacy_influence(20.0f, 10.0f, 1.0f, 1.0f)) < 1e-6f);
  assert(std::fabs(legacy_influence(0.0f, 10.0f, 0.5f, 1.0f) - 0.5f) < 1e-6f);
  assert(std::fabs(legacy_influence(0.0f, 10.0f, 1.0f, 0.5f) - 0.5f) < 1e-6f);
  return 0;
}
