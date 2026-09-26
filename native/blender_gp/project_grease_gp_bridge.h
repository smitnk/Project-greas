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

const char *project_grease_gp_last_error(
    const ProjectGreaseGPHandle *handle);

#ifdef __cplusplus
}
#endif
