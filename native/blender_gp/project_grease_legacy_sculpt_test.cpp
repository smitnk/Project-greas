#include <cassert>
#include <cmath>

static float influence(float alpha, float pressure, float distance, float radius)
{
  if (radius <= 0.0f || distance > radius) return 0.0f;
  return alpha * pressure * (1.0f - distance / radius);
}

int main()
{
  assert(std::fabs(influence(1.0f, 1.0f, 0.0f, 20.0f) - 1.0f) < 1e-6f);
  assert(std::fabs(influence(0.8f, 0.5f, 10.0f, 20.0f) - 0.2f) < 1e-6f);
  assert(influence(1.0f, 1.0f, 21.0f, 20.0f) == 0.0f);
  return 0;
}
