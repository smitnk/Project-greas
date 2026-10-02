/* Test-only stand-in: only the Grease Pencil material style fields the selection code reads. */
#pragma once
#define GP_MATERIAL_HIDE (1 << 1)
#define GP_MATERIAL_LOCKED (1 << 3)
#define GP_MATERIAL_FILL_SHOW (1 << 4)
typedef struct MaterialGPencilStyle {
  float stroke_rgba[4];
  float fill_rgba[4];
  int flag;
} MaterialGPencilStyle;
typedef struct Material { MaterialGPencilStyle *gp_style; } Material;
