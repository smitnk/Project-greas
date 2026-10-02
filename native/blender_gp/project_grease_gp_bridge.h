#pragma once

#include "project_grease_document_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ProjectGreaseGPHandle ProjectGreaseGPHandle;

typedef struct ProjectGreaseGPPoint {
  float x;
  float y;
  float z;
  float pressure;
  float strength;
  float time;
} ProjectGreaseGPPoint;

typedef struct ProjectGreaseGPStrokeStyle {
  int material_index;
  float thickness;
} ProjectGreaseGPStrokeStyle;

ProjectGreaseGPHandle *project_grease_gp_create(void);
void project_grease_gp_destroy(ProjectGreaseGPHandle *handle);

typedef struct ProjectGreaseGPLegacyPaintSettings {
  int draw_smooth_level;
  float draw_smooth_factor;
  int input_samples;
  int smooth_position;
  int smooth_strength;
} ProjectGreaseGPLegacyPaintSettings;

int project_grease_gp_set_legacy_paint_settings(
    ProjectGreaseGPHandle *handle,
    ProjectGreaseGPLegacyPaintSettings settings);

int project_grease_gp_begin_stroke(
    ProjectGreaseGPHandle *handle,
    int material_index,
    float thickness);

int project_grease_gp_add_point(
    ProjectGreaseGPHandle *handle,
    ProjectGreaseGPPoint point);

int project_grease_gp_end_stroke(ProjectGreaseGPHandle *handle);
int project_grease_gp_cancel_stroke(ProjectGreaseGPHandle *handle);
int project_grease_gp_initialize_external_gpu(ProjectGreaseGPHandle *handle);
int project_grease_gp_render_external_context(ProjectGreaseGPHandle *handle);
int project_grease_gp_render_fill_mask(ProjectGreaseGPHandle *handle);
int project_grease_gp_render(ProjectGreaseGPHandle *handle);

int project_grease_gp_stroke_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_point_count(const ProjectGreaseGPHandle *handle);

int project_grease_gp_create_layer(ProjectGreaseGPHandle *handle, const char *name);
int project_grease_gp_reset_document(ProjectGreaseGPHandle *handle);
int project_grease_gp_select_layer(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_layer_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_set_layer_visibility(ProjectGreaseGPHandle *handle, int index, int visible);
int project_grease_gp_set_layer_locked(ProjectGreaseGPHandle *handle, int index, int locked);
int project_grease_gp_move_layer(ProjectGreaseGPHandle *handle, int from_index, int to_index);
int project_grease_gp_duplicate_layer(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_delete_layer(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_rename_layer(ProjectGreaseGPHandle *handle, int index, const char *name);
int project_grease_gp_create_frame(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_select_frame(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_select_frame_or_hold(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_frame_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_frame_end(const ProjectGreaseGPHandle *handle);
int project_grease_gp_frame_numbers(const ProjectGreaseGPHandle *handle, int *out_frames, int capacity);
int project_grease_gp_interpolate_frame(ProjectGreaseGPHandle *handle, int source_frame, int target_frame, int result_frame, float factor);
/* easing_type/easing_mode: PG_EASE_* of project_grease_blender_edit.h */
int project_grease_gp_interpolate_frame_eased(ProjectGreaseGPHandle *handle, int source_frame, int target_frame, int result_frame, float factor, int easing_type, int easing_mode);
int project_grease_gp_duplicate_frame(ProjectGreaseGPHandle *handle, int source_frame, int target_frame);
int project_grease_gp_delete_frame(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_select_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_stroke_center(const ProjectGreaseGPHandle *handle, int index, float *x, float *y);
int project_grease_gp_hit_test_stroke(
    const ProjectGreaseGPHandle *handle,
    float x,
    float y,
    float radius);
int project_grease_gp_delete_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_delete_last_stroke(ProjectGreaseGPHandle *handle);
int project_grease_gp_duplicate_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_translate_stroke(ProjectGreaseGPHandle *handle, int index, float dx, float dy, float dz);
int project_grease_gp_flip_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_rotate_stroke(ProjectGreaseGPHandle *handle, int index, float radians);
int project_grease_gp_rotate_stroke_about(ProjectGreaseGPHandle *handle, int index, float radians, float center_x, float center_y);
int project_grease_gp_scale_stroke(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y);
int project_grease_gp_scale_stroke_about(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y, float center_x, float center_y);
int project_grease_gp_mirror_stroke(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y);
int project_grease_gp_mirror_stroke_about(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y, float center_x, float center_y);
int project_grease_gp_subdivide_stroke(ProjectGreaseGPHandle *handle, int index, int level);
int project_grease_gp_close_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_trim_stroke(ProjectGreaseGPHandle *handle, int index, int from, int to, int keep_single_point);
int project_grease_gp_trim_stroke_to_intersection(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_split_stroke(ProjectGreaseGPHandle *handle, int index, int before_index);
int project_grease_gp_history_reset(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_record(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_undo(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_redo(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_can_undo(const ProjectGreaseGPHandle *handle);
int project_grease_gp_history_can_redo(const ProjectGreaseGPHandle *handle);
int project_grease_gp_create_primitive(ProjectGreaseGPHandle *handle, int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, int material_index, float thickness);
int project_grease_gp_generate_primitive_preview(int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, float *xy, int capacity, int *count);
int project_grease_gp_create_polyline(ProjectGreaseGPHandle *handle, const ProjectGreaseGPPoint *points, int count, int material_index, float thickness, int cyclic);
int project_grease_gp_erase_at(ProjectGreaseGPHandle *handle, float x, float y, float radius);
int project_grease_gp_soft_erase_at(ProjectGreaseGPHandle *handle, float x, float y, float radius, float strength);
void project_grease_gp_clear_selection(ProjectGreaseGPHandle *handle);
int project_grease_gp_lasso_select(ProjectGreaseGPHandle *handle, const float *xy, int count, int additive);
int project_grease_gp_apply_edit_command(ProjectGreaseGPHandle *handle,
                                         int command,
                                         const float *args,
                                         int arg_count);
int project_grease_gp_material_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_create_material(ProjectGreaseGPHandle *handle);
int project_grease_gp_set_material_colors(ProjectGreaseGPHandle *handle, int index, const float stroke_rgba[4], const float fill_rgba[4]);
int project_grease_gp_set_material_visibility(ProjectGreaseGPHandle *handle, int index, int visible);
int project_grease_gp_set_material_fill_enabled(ProjectGreaseGPHandle *handle, int index, int enabled);
int project_grease_gp_smooth_stroke(ProjectGreaseGPHandle *handle, int index, float influence, int iterations);
int project_grease_gp_sculpt_at(ProjectGreaseGPHandle *handle, int tool, float x, float y, float radius, float influence);
int project_grease_gp_apply_blender_modifier(ProjectGreaseGPHandle *handle, int index, int modifier_type, float factor, int iterations);
int project_grease_gp_apply_blender_modifier_named(ProjectGreaseGPHandle *handle, int index, const char *name, float factor, int iterations);
typedef struct ProjectGreaseGPLegacyGeometryOp {
  int type;
  float value0;
  float value1;
  float value2;
  int int0;
  int int1;
  int flag0;
  int flag1;
} ProjectGreaseGPLegacyGeometryOp;

/* Batched direct calls into Blender 3.6.23 Legacy GP BKE geometry algorithms. */
int project_grease_gp_apply_legacy_geometry_batch(
    ProjectGreaseGPHandle *handle,
    int stroke_index,
    const ProjectGreaseGPLegacyGeometryOp *operations,
    int operation_count);
int project_grease_gp_apply_blender_modifier_stack(ProjectGreaseGPHandle *handle,
                                                   int index,
                                                   const int *modifier_types,
                                                   int modifier_count,
                                                   float factor,
                                                   int iterations);
int project_grease_gp_set_onion_skin(ProjectGreaseGPHandle *handle, int enabled, int before, int after, float opacity);
int project_grease_gp_set_multiframe_editing(ProjectGreaseGPHandle *handle, int enabled);
int project_grease_gp_fill_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_fill_at_screen(ProjectGreaseGPHandle *handle,
                                     const float *rgba,
                                     int width,
                                     int height,
                                     int seed_x,
                                     int seed_y,
                                     int fill_leak,
                                     int dilate_pixels,
                                     int material_index,
                                     float thickness);
/* Save/load state (stroke style, layer state, palette); structs live in project_grease_document_state.h. */
int project_grease_gp_get_stroke_info(const ProjectGreaseGPHandle *handle, int stroke_index, PGStrokeInfo *out);
/* Vertex colors are passed separately: `colors` holds 4 floats (RGBA) per point and may be NULL
 * (no vertex color). */
int project_grease_gp_add_stroke(ProjectGreaseGPHandle *handle,
                                 const ProjectGreaseGPPoint *points,
                                 const float *colors,
                                 int count,
                                 const PGStrokeInfo *info);
int project_grease_gp_get_point_color(const ProjectGreaseGPHandle *handle,
                                      int stroke_index,
                                      int point_index,
                                      float out_rgba[4]);
int project_grease_gp_get_layer_info(const ProjectGreaseGPHandle *handle, int index, PGLayerInfo *out);
int project_grease_gp_set_layer_opacity(ProjectGreaseGPHandle *handle, int index, float opacity);
/* Layer masks (names refer to layers; flags: bit 0 hidden, bit 1 inverted). */
int project_grease_gp_layer_use_mask(const ProjectGreaseGPHandle *handle, int layer_index);
int project_grease_gp_set_layer_use_mask(ProjectGreaseGPHandle *handle, int layer_index, int enabled);
int project_grease_gp_mask_count(const ProjectGreaseGPHandle *handle, int layer_index);
int project_grease_gp_mask_add(ProjectGreaseGPHandle *handle, int layer_index, int mask_layer_index);
int project_grease_gp_mask_remove(ProjectGreaseGPHandle *handle, int layer_index, int mask_index);
int project_grease_gp_mask_get(const ProjectGreaseGPHandle *handle, int layer_index, int mask_index, char *name, int name_capacity, int *flags);
int project_grease_gp_mask_set_flags(ProjectGreaseGPHandle *handle, int layer_index, int mask_index, int flags);

/* Live per-layer modifier stack (project_grease_modifier_stack.h). */
int project_grease_gp_modifier_count(const ProjectGreaseGPHandle *handle, int layer_index);
int project_grease_gp_modifier_add(ProjectGreaseGPHandle *handle, int layer_index, int type); /* index or -1 */
int project_grease_gp_modifier_remove(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index);
int project_grease_gp_modifier_move(ProjectGreaseGPHandle *handle, int layer_index, int from_index, int to_index);
int project_grease_gp_modifier_set_enabled(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, int enabled);
int project_grease_gp_modifier_set_params(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, const float *params, int count);
/* Returns the parameter count or -1; type/enabled/params are filled when non-NULL. */
int project_grease_gp_modifier_get(const ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, int *type, int *enabled, float *params, int capacity);
int project_grease_gp_modifier_apply(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index);
int project_grease_gp_get_material_info(const ProjectGreaseGPHandle *handle, int index, PGMaterialInfo *out);
int project_grease_gp_get_point(const ProjectGreaseGPHandle *handle, int stroke_index, int point_index, ProjectGreaseGPPoint *out);
int project_grease_gp_set_point(ProjectGreaseGPHandle *handle, int stroke_index, int point_index, ProjectGreaseGPPoint point);

const char *project_grease_gp_last_error(
    const ProjectGreaseGPHandle *handle);

#ifdef __cplusplus
}
#endif
