#include "project_grease_gp_bridge.h"

#include <vector>

#include "project_grease_gp_backend.h"

struct ProjectGreaseGPHandle {
  project_grease::gp::Backend backend;
  bool ready = false;
};

static int ensure_ready(ProjectGreaseGPHandle *handle)
{
  if (!handle || !handle->ready) {
    return 0;
  }
  return 1;
}

ProjectGreaseGPHandle *project_grease_gp_create(void)
{
  ProjectGreaseGPHandle *handle = new ProjectGreaseGPHandle();
  if (!handle->backend.initialize() ||
      !handle->backend.create_document() ||
      !handle->backend.create_layer("Layer 1") ||
      !handle->backend.create_frame(1)) {
    delete handle;
    return nullptr;
  }

  handle->ready = true;
  return handle;
}

void project_grease_gp_destroy(ProjectGreaseGPHandle *handle)
{
  delete handle;
}

int project_grease_gp_begin_stroke(
    ProjectGreaseGPHandle *handle,
    int material_index,
    float thickness)
{
  if (!ensure_ready(handle)) {
    return 0;
  }
  return handle->backend.begin_stroke({material_index, thickness}) ? 1 : 0;
}

int project_grease_gp_add_point(
    ProjectGreaseGPHandle *handle,
    ProjectGreaseGPPoint point)
{
  if (!ensure_ready(handle)) {
    return 0;
  }

  project_grease::gp::StrokePoint native_point{
      point.x,
      point.y,
      point.z,
      point.pressure,
      point.strength,
      point.time,
  };

  return handle->backend.add_point(native_point) ? 1 : 0;
}

int project_grease_gp_end_stroke(ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(handle)) {
    return 0;
  }
  return handle->backend.end_stroke() ? 1 : 0;
}

int project_grease_gp_initialize_external_gpu(ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(handle)) {
    return 0;
  }
  return handle->backend.initialize_external_gpu_context() ? 1 : 0;
}

int project_grease_gp_render_external_context(ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(handle)) {
    return 0;
  }
  return handle->backend.render_external_context() ? 1 : 0;
}

int project_grease_gp_render(ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(handle)) {
    return 0;
  }
  return handle->backend.render() ? 1 : 0;
}

int project_grease_gp_stroke_count(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return 0;
  }
  return handle->backend.stroke_count();
}

int project_grease_gp_point_count(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return 0;
  }
  return handle->backend.point_count();
}

const char *project_grease_gp_last_error(
    const ProjectGreaseGPHandle *handle)
{
  if (!handle) {
    return "null Project Grease GP handle";
  }
  return handle->backend.last_error();
}


int project_grease_gp_create_layer(ProjectGreaseGPHandle *handle, const char *name)
{
  return ensure_ready(handle) && handle->backend.create_layer(name) ? 1 : 0;
}
int project_grease_gp_select_layer(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.select_layer(index) ? 1 : 0;
}
int project_grease_gp_layer_count(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) return 0;
  return handle->backend.layer_count();
}
int project_grease_gp_set_layer_visibility(ProjectGreaseGPHandle *handle, int index, int visible)
{ return ensure_ready(handle) && handle->backend.set_layer_visibility(index, visible != 0) ? 1 : 0; }
int project_grease_gp_set_layer_locked(ProjectGreaseGPHandle *handle, int index, int locked)
{ return ensure_ready(handle) && handle->backend.set_layer_locked(index, locked != 0) ? 1 : 0; }
int project_grease_gp_move_layer(ProjectGreaseGPHandle *handle, int from_index, int to_index)
{ return ensure_ready(handle) && handle->backend.move_layer(from_index, to_index) ? 1 : 0; }
int project_grease_gp_duplicate_layer(ProjectGreaseGPHandle *handle, int index)
{ return ensure_ready(handle) && handle->backend.duplicate_layer(index) ? 1 : 0; }
int project_grease_gp_delete_layer(ProjectGreaseGPHandle *handle, int index)
{ return ensure_ready(handle) && handle->backend.delete_layer(index) ? 1 : 0; }
int project_grease_gp_rename_layer(ProjectGreaseGPHandle *handle, int index, const char *name)
{ return ensure_ready(handle) && handle->backend.rename_layer(index, name) ? 1 : 0; }
int project_grease_gp_create_frame(ProjectGreaseGPHandle *handle, int frame_number)
{
  return ensure_ready(handle) && handle->backend.create_frame(frame_number) ? 1 : 0;
}
int project_grease_gp_select_frame(ProjectGreaseGPHandle *handle, int frame_number)
{
  return ensure_ready(handle) && handle->backend.select_frame(frame_number) ? 1 : 0;
}
int project_grease_gp_frame_count(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) return 0;
  return handle->backend.frame_count();
}
int project_grease_gp_duplicate_frame(ProjectGreaseGPHandle *handle, int source_frame, int target_frame)
{
  return ensure_ready(handle) && handle->backend.duplicate_frame(source_frame, target_frame) ? 1 : 0;
}
int project_grease_gp_delete_frame(ProjectGreaseGPHandle *handle, int frame_number)
{
  return ensure_ready(handle) && handle->backend.delete_frame(frame_number) ? 1 : 0;
}
int project_grease_gp_select_stroke(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.select_stroke(index) ? 1 : 0;
}
int project_grease_gp_hit_test_stroke(
    const ProjectGreaseGPHandle *handle,
    float x,
    float y,
    float radius)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return -1;
  }
  return handle->backend.hit_test_stroke(x, y, radius);
}
int project_grease_gp_delete_stroke(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.delete_stroke(index) ? 1 : 0;
}
int project_grease_gp_delete_last_stroke(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.delete_last_stroke() ? 1 : 0;
}
int project_grease_gp_duplicate_stroke(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.duplicate_stroke(index) ? 1 : 0;
}
int project_grease_gp_translate_stroke(ProjectGreaseGPHandle *handle, int index, float dx, float dy, float dz)
{
  return ensure_ready(handle) && handle->backend.translate_stroke(index, dx, dy, dz) ? 1 : 0;
}
int project_grease_gp_flip_stroke(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.flip_stroke(index) ? 1 : 0;
}
int project_grease_gp_rotate_stroke(ProjectGreaseGPHandle *handle, int index, float radians)
{ return ensure_ready(handle) && handle->backend.rotate_stroke(index, radians) ? 1 : 0; }
int project_grease_gp_scale_stroke(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y)
{ return ensure_ready(handle) && handle->backend.scale_stroke(index, scale_x, scale_y) ? 1 : 0; }
int project_grease_gp_mirror_stroke(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y)
{ return ensure_ready(handle) && handle->backend.mirror_stroke(index, mirror_x != 0, mirror_y != 0) ? 1 : 0; }

int project_grease_gp_subdivide_stroke(ProjectGreaseGPHandle *handle, int index, int level)
{
  return ensure_ready(handle) && handle->backend.subdivide_stroke(index, level) ? 1 : 0;
}
int project_grease_gp_close_stroke(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.close_stroke(index) ? 1 : 0;
}
int project_grease_gp_trim_stroke(ProjectGreaseGPHandle *handle, int index, int from, int to, int keep_single_point)
{
  return ensure_ready(handle) && handle->backend.trim_stroke_points(index, from, to, keep_single_point != 0) ? 1 : 0;
}
int project_grease_gp_split_stroke(ProjectGreaseGPHandle *handle, int index, int before_index)
{
  return ensure_ready(handle) && handle->backend.split_stroke(index, before_index) ? 1 : 0;
}
int project_grease_gp_create_primitive(ProjectGreaseGPHandle *handle, int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, int material_index, float thickness)
{
  if (!ensure_ready(handle)) return 0;
  return handle->backend.create_primitive(type, x0, y0, x1, y1, start_angle, end_angle, segments, {material_index, thickness}) ? 1 : 0;
}

int project_grease_gp_create_polyline(ProjectGreaseGPHandle *handle, const ProjectGreaseGPPoint *points, int count, int material_index, float thickness, int cyclic)
{
  if (!ensure_ready(handle) || !points || count < 2) return 0;
  std::vector<project_grease::gp::StrokePoint> native_points;
  native_points.reserve(count);
  for (int i = 0; i < count; ++i) {
    native_points.push_back({points[i].x, points[i].y, points[i].z, points[i].pressure, points[i].strength, points[i].time});
  }
  return handle->backend.create_polyline(native_points.data(), count, {material_index, thickness}, cyclic != 0) ? 1 : 0;
}

int project_grease_gp_erase_at(ProjectGreaseGPHandle *handle, float x, float y, float radius)
{
  return ensure_ready(handle) && handle->backend.erase_at(x, y, radius) ? 1 : 0;
}

void project_grease_gp_clear_selection(ProjectGreaseGPHandle *handle)
{
  if (ensure_ready(handle)) handle->backend.clear_selection();
}

int project_grease_gp_lasso_select(ProjectGreaseGPHandle *handle, const float *xy, int count, int additive)
{
  if (!ensure_ready(handle) || !xy) return 0;
  return handle->backend.lasso_select(xy, count, additive != 0);
}

int project_grease_gp_get_point(const ProjectGreaseGPHandle *handle,
                                  int stroke_index,
                                  int point_index,
                                  ProjectGreaseGPPoint *out)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) || !out) {
    return 0;
  }

  project_grease::gp::StrokePoint native_point{};
  if (!handle->backend.get_point(stroke_index, point_index, &native_point)) {
    return 0;
  }

  out->x = native_point.x;
  out->y = native_point.y;
  out->z = native_point.z;
  out->pressure = native_point.pressure;
  out->strength = native_point.strength;
  out->time = native_point.time;
  return 1;
}
