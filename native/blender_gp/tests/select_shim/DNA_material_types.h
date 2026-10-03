/* Test-only stand-in: only the Grease Pencil material style fields the selection code reads. */
#pragma once
#define GP_MATERIAL_HIDE (1 << 1)
#define GP_MATERIAL_LOCKED (1 << 3)
#define GP_MATERIAL_FILL_SHOW (1 << 4)
typedef struct MaterialGPencilStyle {
  int flag; float stroke_rgba[4]; float fill_rgba[4];
  /* texture settings (real DNA names) */
  short stroke_style, fill_style;
  float mix_factor, texture_angle, texture_scale[2], texture_offset[2], texture_pixsize, mix_stroke_factor;
} MaterialGPencilStyle;
enum { GP_MATERIAL_STROKE_STYLE_SOLID = 0, GP_MATERIAL_STROKE_STYLE_TEXTURE = 1 };
enum { GP_MATERIAL_FILL_STYLE_SOLID = 0, GP_MATERIAL_FILL_STYLE_TEXTURE = 3 };
typedef struct Material { MaterialGPencilStyle *gp_style; } Material;
