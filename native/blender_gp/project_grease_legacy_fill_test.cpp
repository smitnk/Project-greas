#include "project_grease_legacy_fill.h"

#include <cassert>

using project_grease::legacy_gp_fill::Image;
using project_grease::legacy_gp_fill::Result;

int main()
{
  Image image(64, 64);

  // Synthetic raster equivalent of a closed Legacy GP boundary.
  for (int y = 12; y < 52; ++y) {
    for (int x = 12; x < 52; ++x) {
      const bool border = x == 12 || x == 51 || y == 12 || y == 51;
      if (border) {
        float *p = image.pixel(y * image.width() + x);
        p[0] = 1.0f;
        p[3] = 1.0f;
      }
    }
  }

  project_grease::legacy_gp_fill::normalize_to_legacy_mask(image, 0.5f);
  Result result = project_grease::legacy_gp_fill::run(image, 32, 32, 3, 0);

  assert(result.valid);
  assert(!result.border_contact);
  assert(result.outline.size() >= 3);
  return 0;
}
