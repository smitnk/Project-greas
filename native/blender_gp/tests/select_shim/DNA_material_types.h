/* Test-only stand-in: only the Grease Pencil material style fields the selection code reads. */
#pragma once
#define GP_MATERIAL_HIDE (1 << 1)
#define GP_MATERIAL_LOCKED (1 << 3)
#define GP_MATERIAL_FILL_SHOW (1 << 4)
#define GP_MATERIAL_FLIP_FILL (1 << 6)
#define GP_MATERIAL_DISABLE_STENCIL (1 << 12)
#define GP_MATERIAL_IS_STROKE_HOLDOUT (1 << 13)
#define GP_MATERIAL_IS_FILL_HOLDOUT (1 << 14)
typedef struct MaterialGPencilStyle {
  int flag; float stroke_rgba[4]; float fill_rgba[4];
  /* texture settings (real DNA names) */
  short stroke_style, fill_style;
  float mix_factor, texture_angle, texture_scale[2], texture_offset[2], texture_pixsize, mix_stroke_factor;
  float mix_rgba[4]; int gradient_type;
} MaterialGPencilStyle;
enum { GP_MATERIAL_STROKE_STYLE_SOLID = 0, GP_MATERIAL_STROKE_STYLE_TEXTURE = 1 };
enum { GP_MATERIAL_FILL_STYLE_SOLID = 0, GP_MATERIAL_FILL_STYLE_GRADIENT = 1, GP_MATERIAL_FILL_STYLE_TEXTURE = 3 };
enum { GP_MATERIAL_GRADIENT_LINEAR = 0, GP_MATERIAL_GRADIENT_RADIAL = 1 };
typedef struct Material { MaterialGPencilStyle *gp_style; } Material;
