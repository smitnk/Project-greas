#include "project_grease_legacy_fill.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>

namespace project_grease::legacy_gp_fill {
namespace {

constexpr int LEAK_HORZ = 0;
constexpr int LEAK_VERT = 1;

inline float *pixel(Image &image, int index) {
  return image.pixel(index);
}
inline const float *pixel(const Image &image, int index) {
  return image.pixel(index);
}

inline void set_pixel(Image &image, int index, float r, float g, float b, float a) {
  float *p = pixel(image, index);
  p[0] = r; p[1] = g; p[2] = b; p[3] = a;
}

bool is_leak_narrow(const Image &image, int maxpixel, int limit, int index, int type)
{
  float rgba[4];
  bool t_a = false;
  bool t_b = false;
  const int extreme = limit - 1;

  if (type == LEAK_HORZ) {
    const int pt_a = index + image.width() * extreme;
    if (pt_a <= maxpixel) {
      std::memcpy(rgba, pixel(image, pt_a), sizeof(rgba));
      t_a = rgba[0] == 1.0f;
    }
    else {
      t_a = true;
    }

    const int pt_b = index - image.width() * extreme;
    if (pt_b >= 0) {
      std::memcpy(rgba, pixel(image, pt_b), sizeof(rgba));
      t_b = rgba[0] == 1.0f;
    }
    else {
      t_b = true;
    }
  }
  else {
    const int row = index / image.width();
    const int lowpix = row * image.width();
    const int higpix = lowpix + image.width() - 1;

    const int pt_a = index - extreme;
    if (pt_a >= lowpix) {
      std::memcpy(rgba, pixel(image, pt_a), sizeof(rgba));
      t_a = rgba[0] == 1.0f;
    }
    else {
      t_a = true;
    }

    const int pt_b = index + extreme;
    if (pt_b <= higpix) {
      std::memcpy(rgba, pixel(image, pt_b), sizeof(rgba));
      t_b = rgba[0] == 1.0f;
    }
    else {
      t_b = true;
    }
  }

  return t_a && t_b;
}

bool boundary_fill(Image &image, int seed_index, int fill_leak, bool &border_contact)
{
  const int maxpixel = image.width() * image.height() - 1;
  if (seed_index < 0 || seed_index > maxpixel) {
    return false;
  }

  std::deque<int> stack;
  stack.push_back(seed_index);
  const float fill_col[4] = {0.0f, 1.0f, 0.0f, 1.0f};

  while (!stack.empty()) {
    const int v = stack.back();
    stack.pop_back();

    const float *rgba = pixel(image, v);

    if (rgba[3] == 0.5f) {
      border_contact = true;
    }

    if (rgba[0] != 1.0f && rgba[1] != 1.0f) {
      set_pixel(image, v, fill_col[0], fill_col[1], fill_col[2], fill_col[3]);

      if (v - 1 >= 0 &&
          !is_leak_narrow(image, maxpixel, fill_leak, v, LEAK_HORZ)) {
        stack.push_back(v - 1);
      }
      if (v + 1 <= maxpixel &&
          !is_leak_narrow(image, maxpixel, fill_leak, v, LEAK_HORZ)) {
        stack.push_back(v + 1);
      }
      if (v + image.width() <= maxpixel &&
          !is_leak_narrow(image, maxpixel, fill_leak, v, LEAK_VERT)) {
        stack.push_back(v + image.width());
      }
      if (v - image.width() >= 0 &&
          !is_leak_narrow(image, maxpixel, fill_leak, v, LEAK_VERT)) {
        stack.push_back(v - image.width());
      }
    }
  }

  return true;
}

void set_borders(Image &image, bool transparent)
{
  const float red[4] = {1.0f, 0.0f, 0.0f, 0.5f};
  const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float *color = transparent ? red : clear;

  for (int x = 0; x < image.width(); ++x) {
    set_pixel(image, x, color[0], color[1], color[2], color[3]);
    set_pixel(image,
              x + image.width() * (image.height() - 1),
              color[0], color[1], color[2], color[3]);
  }

  for (int y = 0; y < image.height(); ++y) {
    set_pixel(image, image.width() * y, color[0], color[1], color[2], color[3]);
    set_pixel(image,
              image.width() * y + image.width() - 1,
              color[0], color[1], color[2], color[3]);
  }
}

bool dilate_shape(Image &image)
{
  // Source-aligned extraction of Blender 3.6.23 gpencil_fill.c::dilate_shape.
  // The upstream implementation stages all candidate pixels before applying
  // them, including diagonal neighbors, to prevent same-pass creep.
  std::vector<int> additions;
  const int max_size = image.width() * image.height() - 1;

  for (int row = 0; row < image.height(); ++row) {
    int minpixel = image.width() * row;
    int maxpixel = image.width() * (row + 1) - 1;

    for (int v = maxpixel; v != minpixel; --v) {
      const float *color = pixel(image, v);
      if (color[1] != 1.0f) {
        continue;
      }

      int top = 0;
      int bottom = 0;
      int left = 0;
      int right = 0;

      auto stage = [&](int index, int &slot) {
        if (index < 0 || index > max_size) {
          return;
        }
        if (pixel(image, index)[1] != 1.0f) {
          additions.push_back(index);
          slot = index;
        }
      };

      stage(v - 1, left);
      stage(v + 1, right);
      stage(v + image.width(), top);
      stage(v - image.width(), bottom);

      if (top && left) stage(top - 1, left);
      if (top && right) stage(top + 1, right);
      if (bottom && left) stage(bottom - 1, left);
      if (bottom && right) stage(bottom + 1, right);
    }
  }

  for (int index : additions) {
    set_pixel(image, index, 0.0f, 1.0f, 0.0f, 1.0f);
  }
  return !additions.empty();
}

bool contract_shape(Image &image)
{
  // Source-aligned extraction of Blender 3.6.23 gpencil_fill.c::contract_shape.
  std::vector<int> removals;
  const int max_size = image.width() * image.height() - 1;

  for (int row = 0; row < image.height(); ++row) {
    int minpixel = image.width() * row;
    int maxpixel = image.width() * (row + 1) - 1;

    for (int v = maxpixel; v != minpixel; --v) {
      const float *color = pixel(image, v);
      if (color[1] != 1.0f) {
        continue;
      }

      const int neighbors[4] = {v - 1, v + 1, v + image.width(), v - image.width()};
      bool clear = false;
      for (int index : neighbors) {
        if (index < 0 || index > max_size || pixel(image, index)[1] != 1.0f) {
          clear = true;
          break;
        }
      }
      if (clear) {
        removals.push_back(v);
      }
    }
  }

  for (int index : removals) {
    set_pixel(image, index, 0.0f, 0.0f, 0.0f, 0.0f);
  }
  return !removals.empty();
}

std::vector<Point> outline_points(Image &image, int dilate_pixels)
{
  for (int i = 0; i < std::abs(dilate_pixels); ++i) {
    if (dilate_pixels > 0) {
      dilate_shape(image);
    }
    else {
      contract_shape(image);
    }
  }

  const int offsets[8][2] = {
      {-1, -1}, {0, -1}, {1, -1}, {1, 0},
      {1, 1}, {0, 1}, {-1, 1}, {-1, 0}};

  std::vector<Point> stack;
  const int imagesize = image.width() * image.height();

  int start_x = -1;
  int start_y = -1;
  for (int idx = imagesize - 1; idx != 0; --idx) {
    if (pixel(image, idx)[1] == 1.0f) {
      start_x = idx % image.width();
      start_y = idx / image.width();
      break;
    }
  }

  if (start_x < 0) {
    return {};
  }

  int boundary_x = start_x;
  int boundary_y = start_y;
  int previous_x = start_x - 1;
  int previous_y = start_y;
  int first_x = -1;
  int first_y = -1;
  bool first_pixel = false;

  stack.push_back({boundary_x + 0.5f, boundary_y + 0.5f});

  while (true) {
    const int back_dx = previous_x - boundary_x;
    const int back_dy = previous_y - boundary_y;

    int back_index = -1;
    for (int i = 0; i < 8; ++i) {
      if (offsets[i][0] == back_dx && offsets[i][1] == back_dy) {
        back_index = i;
        break;
      }
    }

    if (back_index < 0) {
      break;
    }

    bool found = false;
    int next_x = boundary_x;
    int next_y = boundary_y;
    int next_prev_x = previous_x;
    int next_prev_y = previous_y;

    for (int loop = 0; loop < 7; ++loop) {
      const int oi = (back_index + 1 + loop) % 8;
      const int cx = boundary_x + offsets[oi][0];
      const int cy = boundary_y + offsets[oi][1];

      if (cx < 0 || cx >= image.width() || cy < 0 || cy >= image.height()) {
        return {};
      }

      const int image_idx = image.width() * cy + cx;
      if (pixel(image, image_idx)[1] == 1.0f) {
        next_x = cx;
        next_y = cy;
        next_prev_x = boundary_x + offsets[(oi + 7) % 8][0];
        next_prev_y = boundary_y + offsets[(oi + 7) % 8][1];
        found = true;
        break;
      }

      previous_x = cx;
      previous_y = cy;
    }

    if (!found) {
      break;
    }

    boundary_x = next_x;
    boundary_y = next_y;
    previous_x = next_prev_x;
    previous_y = next_prev_y;

    stack.push_back({boundary_x + 0.5f, boundary_y + 0.5f});

    if ((boundary_x == start_x && boundary_y == start_y) ||
        (boundary_x == first_x && boundary_y == first_y)) {
      if (!stack.empty()) {
        stack.pop_back();
      }
      break;
    }

    if (!first_pixel) {
      first_pixel = true;
      first_x = boundary_x;
      first_y = boundary_y;
    }
  }

  return stack;
}

}  // namespace

Image::Image(int width, int height)
    : width_(std::max(0, width)),
      height_(std::max(0, height)),
      rgba_(static_cast<size_t>(std::max(0, width)) *
            static_cast<size_t>(std::max(0, height)) * 4u,
            0.0f)
{
}

void normalize_to_legacy_mask(Image &image, float alpha_threshold)
{
  for (int i = 0; i < image.width() * image.height(); ++i) {
    const float alpha = pixel(image, i)[3];
    if (alpha > alpha_threshold) {
      set_pixel(image, i, 1.0f, 0.0f, 0.0f, 1.0f);
    }
    else {
      set_pixel(image, i, 0.0f, 0.0f, 0.0f, 0.0f);
    }
  }
}

Result run(Image &image, int seed_x, int seed_y, int fill_leak, int dilate_pixels)
{
  Result result;
  if (image.width() < 3 || image.height() < 3 ||
      seed_x < 0 || seed_x >= image.width() ||
      seed_y < 0 || seed_y >= image.height()) {
    return result;
  }

  set_borders(image, true);
  set_pixel(image, seed_y * image.width() + seed_x, 0.0f, 0.0f, 1.0f, 1.0f);

  if (!boundary_fill(image,
                     seed_y * image.width() + seed_x,
                     std::max(1, fill_leak),
                     result.border_contact)) {
    return result;
  }

  if (result.border_contact) {
    return result;
  }

  set_borders(image, false);
  result.outline = outline_points(image, dilate_pixels);
  result.valid = result.outline.size() >= 3;
  return result;
}

}  // namespace project_grease::legacy_gp_fill
