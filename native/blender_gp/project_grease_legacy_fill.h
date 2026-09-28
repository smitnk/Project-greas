#pragma once

#include <cstdint>
#include <vector>

namespace project_grease::legacy_gp_fill {

/*
 * Blender 3.6.23 Legacy Grease Pencil fill extraction.
 *
 * Behavioral source:
 *   source/blender/editors/gpencil_legacy/gpencil_fill.c
 *   tag v3.6.23
 *
 * This is NOT a new fill engine. It is the minimum Android-facing raster
 * adapter for Blender's Legacy GP boundary-fill algorithm. The flood-fill
 * and Moore-neighborhood outline behavior are kept equivalent to Blender
 * 3.6.23. Project Grease supplies only the pixel buffer/seed bridge.
 */

struct Point {
  float x = 0.0f;
  float y = 0.0f;
};

struct Result {
  bool valid = false;
  bool border_contact = false;
  std::vector<Point> outline;
};

class Image {
 public:
  Image(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }

  float *pixel(int index) { return &rgba_[static_cast<size_t>(index) * 4u]; }
  const float *pixel(int index) const { return &rgba_[static_cast<size_t>(index) * 4u]; }

  std::vector<float> &rgba() { return rgba_; }
  const std::vector<float> &rgba() const { return rgba_; }

 private:
  int width_;
  int height_;
  std::vector<float> rgba_;
};

/* Converts the Android/GLES RGBA readback into Blender fill-mask semantics. */
void normalize_to_legacy_mask(Image &image, float alpha_threshold);

/* Exact Legacy GP flood-fill + outline extraction path. */
Result run(Image &image, int seed_x, int seed_y, int fill_leak, int dilate_pixels);

}  // namespace project_grease::legacy_gp_fill
