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
