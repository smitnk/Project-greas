#include "project_grease_gp_bridge.h"

#include <cstdio>

int main()
{
  ProjectGreaseGPHandle *handle = project_grease_gp_create();
  if (!handle) {
    std::fprintf(stderr, "bridge create failed\n");
    return 1;
  }

  if (!project_grease_gp_begin_stroke(handle, 0, 3.0f) ||
      !project_grease_gp_add_point(handle, {-1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !project_grease_gp_add_point(handle, {0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.1f}) ||
      !project_grease_gp_add_point(handle, {1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.2f}) ||
      !project_grease_gp_end_stroke(handle)) {
    std::fprintf(stderr, "bridge stroke failed: %s\n",
                 project_grease_gp_last_error(handle));
    project_grease_gp_destroy(handle);
    return 2;
  }

  // Advanced edit regression: these operations must stay direct Blender 3.6.23 BKE calls.
  if (!project_grease_gp_duplicate_stroke(handle, 0) ||
      !project_grease_gp_apply_legacy_geometry_batch(
          handle, 1,
          (ProjectGreaseGPLegacyGeometryOp[]){{14, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0}},
          1) ||
      !project_grease_gp_apply_legacy_geometry_batch(
          handle, 1,
          (ProjectGreaseGPLegacyGeometryOp[]){{15, 0.0f, 0.0f, 0.0f, 8, 0, 1, 0}},
          1) ||
      !project_grease_gp_apply_legacy_geometry_batch(
          handle, 1,
          (ProjectGreaseGPLegacyGeometryOp[]){{16, 5.0f, 0.0f, 0.0f, 1, 0, 0, 0}},
          1) ||
      !project_grease_gp_apply_legacy_geometry_batch(
          handle, 1,
          (ProjectGreaseGPLegacyGeometryOp[]){{17, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0}},
          1) ||
      !project_grease_gp_delete_stroke(handle, 1)) {
    std::fprintf(stderr, "advanced Legacy GP geometry regression failed: %s\n",
                 project_grease_gp_last_error(handle));
    project_grease_gp_destroy(handle);
    return 10;
  }

  if (project_grease_gp_stroke_count(handle) != 1 ||
      project_grease_gp_point_count(handle) != 3) {
    std::fprintf(stderr, "bridge state mismatch: strokes=%d points=%d\n",
                 project_grease_gp_stroke_count(handle),
                 project_grease_gp_point_count(handle));
    project_grease_gp_destroy(handle);
    return 3;
  }

  // Sculpt foundation must operate on the real Legacy GP stroke without changing
  // stroke topology. Smooth is Blender's BKE GP algorithm.
  if (!project_grease_gp_sculpt_at(handle, 0, 0.0f, 0.0f, 64.0f, 0.25f)) {
    std::fprintf(stderr, "legacy GP sculpt smooth failed: %s\n",
                 project_grease_gp_last_error(handle));
    project_grease_gp_destroy(handle);
    return 4;
  }

  if (!project_grease_gp_soft_erase_at(handle, 0.0f, 0.0f, 64.0f, 0.25f)) {
    std::fprintf(stderr, "legacy GP soft eraser failed: %s\n",
                 project_grease_gp_last_error(handle));
    project_grease_gp_destroy(handle);
    return 5;
  }
  ProjectGreaseGPPoint eraser_point{};
  if (!project_grease_gp_get_point(handle, 0, 1, &eraser_point) ||
      eraser_point.strength >= 1.0f) {
    std::fprintf(stderr, "legacy GP soft eraser strength regression failed\n");
    project_grease_gp_destroy(handle);
    return 6;
  }

  if (!project_grease_gp_render(handle)) {
    std::fprintf(stderr, "bridge render failed: %s\n",
                 project_grease_gp_last_error(handle));
    project_grease_gp_destroy(handle);
    return 7;
  }

  if (!project_grease_gp_create_frame(handle, 2) ||
      project_grease_gp_frame_count(handle) != 2 ||
      !project_grease_gp_delete_frame(handle, 2) ||
      project_grease_gp_frame_count(handle) != 1) {
    std::fprintf(stderr, "frame create/delete regression failed: %s\n",
                 project_grease_gp_last_error(handle));
    project_grease_gp_destroy(handle);
    return 8;
  }

  // Cancellation must discard an open Android stroke instead of committing it.
  if (!project_grease_gp_begin_stroke(handle, 0, 3.0f) ||
      !project_grease_gp_add_point(handle, {5.0f, 5.0f, 0.0f, 0.7f, 1.0f, 1.0f}) ||
      !project_grease_gp_cancel_stroke(handle) ||
      project_grease_gp_stroke_count(handle) != 1 ||
      project_grease_gp_point_count(handle) != 3) {
    std::fprintf(stderr, "bridge stroke cancellation failed: %s\n",
                 project_grease_gp_last_error(handle));
    project_grease_gp_destroy(handle);
    return 9;
  }

  std::puts("Project Grease GP native bridge simulation passed");
  project_grease_gp_destroy(handle);
  return 0;
}
