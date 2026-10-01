#pragma once

struct bGPdata;
struct bGPDframe;
struct bGPDstroke;

namespace project_grease::legacy_gp_eraser {

struct Settings {
  float draw_strength = 1.0f;
  float pointer_pressure = 1.0f;
  bool soft = false;
  float soft_strength = 1.0f;
  float soft_thickness = 1.0f;
  bool stroke_eraser = false;
};

bool process_stroke(bGPdata *gpd,
                    bGPDframe *frame,
                    bGPDstroke *stroke,
                    float x,
                    float y,
                    int radius,
                    const Settings &settings);

}  // namespace project_grease::legacy_gp_eraser
