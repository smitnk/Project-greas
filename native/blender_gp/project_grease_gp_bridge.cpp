#include "project_grease_gp_bridge.h"

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
