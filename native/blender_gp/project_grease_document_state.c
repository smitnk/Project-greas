/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_document_state.h. */

#include <string.h>

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"

#include "project_grease_document_state.h"

static float clamp01(float v)
{
  if (!(v > 0.0f)) {
    return 0.0f; /* also maps NaN to 0 */
  }
  return v > 1.0f ? 1.0f : v;
}

int pg_doc_stroke_info_get(const bGPDstroke *gps, PGStrokeInfo *r_info)
{
  if (gps == NULL || r_info == NULL) {
    return 0;
  }
  r_info->material_index = gps->mat_nr;
  r_info->thickness = (float)gps->thickness;
  r_info->cyclic = (gps->flag & GP_STROKE_CYCLIC) != 0;
  r_info->fill_opacity_fac = gps->fill_opacity_fac;
  memcpy(r_info->fill_color, gps->vert_color_fill, sizeof(r_info->fill_color));
  return 1;
}

int pg_doc_stroke_info_apply(bGPDstroke *gps, const PGStrokeInfo *info)
{
  if (gps == NULL || info == NULL) {
    return 0;
  }
  float thickness = info->thickness;
  if (!(thickness >= 1.0f)) {
    thickness = 1.0f;
  }
  if (thickness > 32767.0f) {
    thickness = 32767.0f;
  }
  gps->mat_nr = info->material_index < 0 ? 0 : info->material_index;
  gps->thickness = (short)thickness;
  gps->fill_opacity_fac = clamp01(info->fill_opacity_fac);
  memcpy(gps->vert_color_fill, info->fill_color, sizeof(gps->vert_color_fill));
  if (info->cyclic) {
    gps->flag |= GP_STROKE_CYCLIC;
  }
  else {
    gps->flag &= ~GP_STROKE_CYCLIC;
  }
  return 1;
}

int pg_doc_point_color_get(const bGPDspoint *pt, float r_rgba[4])
{
  if (pt == NULL || r_rgba == NULL) {
    return 0;
  }
  memcpy(r_rgba, pt->vert_color, 4 * sizeof(float));
  return 1;
}

int pg_doc_point_color_apply(bGPDspoint *pt, const float rgba[4])
{
  if (pt == NULL || rgba == NULL) {
    return 0;
  }
  for (int c = 0; c < 4; c++) {
    pt->vert_color[c] = clamp01(rgba[c]);
  }
  return 1;
}

int pg_doc_layer_info_get(const bGPDlayer *gpl, PGLayerInfo *r_info)
{
  if (gpl == NULL || r_info == NULL) {
    return 0;
  }
  memset(r_info->name, 0, sizeof(r_info->name));
  strncpy(r_info->name, gpl->info, sizeof(r_info->name) - 1);
  r_info->visible = (gpl->flag & GP_LAYER_HIDE) == 0;
  r_info->locked = (gpl->flag & GP_LAYER_LOCKED) != 0;
  r_info->opacity = gpl->opacity;
  r_info->onion = (gpl->onion_flag & GP_LAYER_ONIONSKIN) != 0;
  return 1;
}

int pg_doc_layer_info_apply(bGPDlayer *gpl, const PGLayerInfo *info)
{
  if (gpl == NULL || info == NULL) {
    return 0;
  }
  if (info->name[0] != '\0') {
    strncpy(gpl->info, info->name, sizeof(gpl->info) - 1);
    gpl->info[sizeof(gpl->info) - 1] = '\0';
  }
  if (info->visible) {
    gpl->flag &= ~GP_LAYER_HIDE;
  }
  else {
    gpl->flag |= GP_LAYER_HIDE;
  }
  if (info->locked) {
    gpl->flag |= GP_LAYER_LOCKED;
  }
  else {
    gpl->flag &= ~GP_LAYER_LOCKED;
  }
  gpl->opacity = clamp01(info->opacity);
  return 1;
}

int pg_doc_material_info_get(const Material *ma, PGMaterialInfo *r_info)
{
  if (ma == NULL || ma->gp_style == NULL || r_info == NULL) {
    return 0;
  }
  memcpy(r_info->stroke_rgba, ma->gp_style->stroke_rgba, sizeof(r_info->stroke_rgba));
  memcpy(r_info->fill_rgba, ma->gp_style->fill_rgba, sizeof(r_info->fill_rgba));
  r_info->visible = (ma->gp_style->flag & GP_MATERIAL_HIDE) == 0;
  r_info->fill_enabled = (ma->gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0;
  return 1;
}

int pg_doc_material_info_apply(Material *ma, const PGMaterialInfo *info)
{
  if (ma == NULL || ma->gp_style == NULL || info == NULL) {
    return 0;
  }
  memcpy(ma->gp_style->stroke_rgba, info->stroke_rgba, sizeof(info->stroke_rgba));
  memcpy(ma->gp_style->fill_rgba, info->fill_rgba, sizeof(info->fill_rgba));
  if (info->visible) {
    ma->gp_style->flag &= ~GP_MATERIAL_HIDE;
  }
  else {
    ma->gp_style->flag |= GP_MATERIAL_HIDE;
  }
  if (info->fill_enabled) {
    ma->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }
  else {
    ma->gp_style->flag &= ~GP_MATERIAL_FILL_SHOW;
  }
  return 1;
}
