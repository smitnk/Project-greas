/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Document state that save/load must round-trip but that the point API (get_point) does not
 * carry: per-stroke style, layer state and material palette entries. Plain field copies on
 * the Blender 3.6.23 Legacy GP DNA (bGPDstroke, bGPDlayer, MaterialGPencilStyle); no Blender
 * function is called, so the module is compiled against shim headers in host tests.
 *
 * All functions return 1 on success, 0 on a NULL argument or missing gp_style.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct bGPDspoint;
struct bGPDstroke;
struct bGPDlayer;
struct Material;

typedef struct PGStrokeInfo {
  int material_index;      /* bGPDstroke.mat_nr */
  float thickness;         /* bGPDstroke.thickness */
  int cyclic;              /* GP_STROKE_CYCLIC */
  float fill_opacity_fac;  /* bGPDstroke.fill_opacity_fac */
  float fill_color[4];     /* bGPDstroke.vert_color_fill */
} PGStrokeInfo;

typedef struct PGLayerInfo {
  char name[128];          /* bGPDlayer.info */
  int visible;             /* !GP_LAYER_HIDE */
  int locked;              /* GP_LAYER_LOCKED */
  float opacity;           /* bGPDlayer.opacity */
  int onion;               /* GP_LAYER_ONIONSKIN (read only: pg_doc_layer_info_apply ignores it) */
} PGLayerInfo;

typedef struct PGMaterialInfo {
  float stroke_rgba[4];    /* MaterialGPencilStyle.stroke_rgba */
  float fill_rgba[4];      /* MaterialGPencilStyle.fill_rgba */
  int visible;             /* !GP_MATERIAL_HIDE */
  int fill_enabled;        /* GP_MATERIAL_FILL_SHOW */
} PGMaterialInfo;

int pg_doc_stroke_info_get(const struct bGPDstroke *gps, PGStrokeInfo *r_info);
/* Sets mat_nr, thickness (clamped to 1..32767), fill and the cyclic flag. The caller refreshes
 * the stroke triangulation when `cyclic` is set. */
int pg_doc_stroke_info_apply(struct bGPDstroke *gps, const PGStrokeInfo *info);

/* bGPDspoint.vert_color[4]. Zero alpha = no vertex color (Blender's default; the renderer then
 * shows the material color). Apply clamps to 0..1 and maps NaN to 0. */
int pg_doc_point_color_get(const struct bGPDspoint *pt, float r_rgba[4]);
int pg_doc_point_color_apply(struct bGPDspoint *pt, const float rgba[4]);

int pg_doc_layer_info_get(const struct bGPDlayer *gpl, PGLayerInfo *r_info);
/* Sets name (empty keeps the current name), visibility, lock and opacity (clamped to 0..1). */
int pg_doc_layer_info_apply(struct bGPDlayer *gpl, const PGLayerInfo *info);

int pg_doc_material_info_get(const struct Material *ma, PGMaterialInfo *r_info);
int pg_doc_material_info_apply(struct Material *ma, const PGMaterialInfo *info);

#ifdef __cplusplus
}
#endif
