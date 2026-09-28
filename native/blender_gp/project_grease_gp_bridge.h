#pragma once

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

int project_grease_gp_begin_stroke(
    ProjectGreaseGPHandle *handle,
    int material_index,
    float thickness);

int project_grease_gp_add_point(
    ProjectGreaseGPHandle *handle,
    ProjectGreaseGPPoint point);

int project_grease_gp_end_stroke(ProjectGreaseGPHandle *handle);
int project_grease_gp_initialize_external_gpu(ProjectGreaseGPHandle *handle);
int project_grease_gp_render_external_context(ProjectGreaseGPHandle *handle);
int project_grease_gp_render(ProjectGreaseGPHandle *handle);

int project_grease_gp_stroke_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_point_count(const ProjectGreaseGPHandle *handle);

int project_grease_gp_create_layer(ProjectGreaseGPHandle *handle, const char *name);
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
int project_grease_gp_frame_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_duplicate_frame(ProjectGreaseGPHandle *handle, int source_frame, int target_frame);
int project_grease_gp_delete_frame(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_select_stroke(ProjectGreaseGPHandle *handle, int index);
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
int project_grease_gp_scale_stroke(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y);
int project_grease_gp_mirror_stroke(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y);
int project_grease_gp_subdivide_stroke(ProjectGreaseGPHandle *handle, int index, int level);
int project_grease_gp_close_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_trim_stroke(ProjectGreaseGPHandle *handle, int index, int from, int to, int keep_single_point);
int project_grease_gp_split_stroke(ProjectGreaseGPHandle *handle, int index, int before_index);
int project_grease_gp_create_primitive(ProjectGreaseGPHandle *handle, int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, int material_index, float thickness);
int project_grease_gp_create_polyline(ProjectGreaseGPHandle *handle, const ProjectGreaseGPPoint *points, int count, int material_index, float thickness, int cyclic);
int project_grease_gp_erase_at(ProjectGreaseGPHandle *handle, float x, float y, float radius);
void project_grease_gp_clear_selection(ProjectGreaseGPHandle *handle);
int project_grease_gp_lasso_select(ProjectGreaseGPHandle *handle, const float *xy, int count, int additive);
int project_grease_gp_get_point(const ProjectGreaseGPHandle *handle, int stroke_index, int point_index, ProjectGreaseGPPoint *out);

const char *project_grease_gp_last_error(
    const ProjectGreaseGPHandle *handle);

#ifdef __cplusplus
}
#endif
