#pragma once

struct bGPdata;
struct bGPDframe;
struct bGPDstroke;

namespace project_grease::legacy_gp_sculpt {

enum Tool {
  Smooth = 0,
  Thickness = 1,
  Strength = 2,
  Grab = 3,
  Push = 4,
  Pinch = 5,
  Twist = 6,
  Randomize = 7,
};

struct Settings {
  float brush_alpha = 1.0f;
  float pressure = 1.0f;
  float radius = 24.0f;
  float multiframe_falloff = 1.0f;
  bool invert = false;
  bool apply_position = true;
  bool apply_strength = false;
  bool apply_thickness = false;
  bool apply_uv = false;
};

struct Context {
  float mouse_x = 0.0f;
  float mouse_y = 0.0f;
  float prev_x = 0.0f;
  float prev_y = 0.0f;
  float delta_x = 0.0f;
  float delta_y = 0.0f;
};

bool apply(bGPdata *gpd,
           bGPDframe *frame,
           bGPDstroke *stroke,
           Tool tool,
           const Context &context,
           const Settings &settings,
           int iterations);

}  // namespace project_grease::legacy_gp_sculpt
