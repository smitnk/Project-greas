#include <algorithm>
#if defined(__has_include)
#if __has_include(<android/log.h>)
#include <android/log.h>
#endif
#endif
#include "project_grease_gp_bridge.h"

#include <string>
#include <vector>

#include "project_grease_gp_backend.h"
#include "DNA_gpencil_modifier_types.h"
#include "project_grease_legacy_primitive.h"
#include "project_grease_blender_select.h"
#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit3.h"
#include "project_grease_blender_edit5.h"
#include "project_grease_blender_edit8.h"
#include "project_grease_blender_edit9.h"
#include "project_grease_blender_edit7.h"
#include "project_grease_tool_session.h"
#include "project_grease_annotations.h"
#include <cstdio>
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"

struct ProjectGreaseGPHandle {
  project_grease::gp::Backend backend;
  bool ready = false;
  // Native tool session (project_grease_tool_session.h) and the document its gesture runs on.
  PGToolSession *tool_session = nullptr;
  bGPdata *tool_gpd = nullptr;
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
  if (handle) {
    pg_tool_session_free(handle->tool_session);
  }
  delete handle;
}


int project_grease_gp_set_legacy_paint_settings(
    ProjectGreaseGPHandle *handle,
    ProjectGreaseGPLegacyPaintSettings settings)
{
  if (!ensure_ready(handle)) {
    return 0;
  }

  project_grease::gp::Backend::LegacyPaintSettings native_settings{};
  native_settings.draw_smooth_level = settings.draw_smooth_level;
  native_settings.draw_smooth_factor = settings.draw_smooth_factor;
  native_settings.input_samples = settings.input_samples;
  native_settings.smooth_position = settings.smooth_position != 0;
  native_settings.smooth_strength = settings.smooth_strength != 0;
  return handle->backend.set_legacy_paint_settings(native_settings) ? 1 : 0;
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

int project_grease_gp_cancel_stroke(ProjectGreaseGPHandle *handle)
{
  return handle && handle->backend.cancel_stroke() ? 1 : 0;
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

int project_grease_gp_render_fill_mask(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.render_fill_mask() ? 1 : 0;
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

int project_grease_gp_selected_point_count(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return 0;
  }
  return handle->backend.frame_selected_point_count();
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

int project_grease_gp_doc_query(const ProjectGreaseGPHandle *handle, int what, const float *args, int arg_count,
                                float *out, int capacity)
{
  if (!handle || !handle->backend.document_data()) return -1;
  return pg_gp_doc_query(handle->backend.document_data(), handle->backend.active_layer_data(), what, args,
                         arg_count, out, capacity);
}

static Material *bridge_material(const ProjectGreaseGPHandle *handle, int slot)
{
  bGPdata *gpd = handle ? handle->backend.document_data() : nullptr;
  if (!gpd || !gpd->mat || slot < 0 || slot >= gpd->totcol) return nullptr;
  return gpd->mat[slot];
}

int project_grease_gp_material_name(const ProjectGreaseGPHandle *handle, int slot, char *out, int capacity)
{
  Material *ma = bridge_material(handle, slot);
  if (!ma || !out || capacity <= 0) return 0;
  std::snprintf(out, static_cast<size_t>(capacity), "%s", ma->id.name + 2);
  return 1;
}

int project_grease_gp_set_material_name(ProjectGreaseGPHandle *handle, int slot, const char *name)
{
  Material *ma = bridge_material(handle, slot);
  if (!ma || !name) return 0;
  std::snprintf(ma->id.name + 2, sizeof(ma->id.name) - 2, "%s", name);
  ma->id.name[0] = 'M';
  ma->id.name[1] = 'A';
  return 1;
}
int project_grease_gp_reset_document(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.reset_document() ? 1 : 0;
}

int project_grease_gp_create_frame(ProjectGreaseGPHandle *handle, int frame_number)
{
  return ensure_ready(handle) && handle->backend.create_frame(frame_number) ? 1 : 0;
}
int project_grease_gp_select_frame(ProjectGreaseGPHandle *handle, int frame_number)
{
  return ensure_ready(handle) && handle->backend.select_frame(frame_number) ? 1 : 0;
}
int project_grease_gp_select_frame_or_hold(ProjectGreaseGPHandle *handle, int frame_number)
{
  return ensure_ready(handle) && handle->backend.select_frame_or_hold(frame_number) ? 1 : 0;
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

int project_grease_gp_stroke_center(const ProjectGreaseGPHandle *handle, int index, float *x, float *y)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) || !x || !y) {
    return 0;
  }
  return handle->backend.stroke_center(index, x, y) ? 1 : 0;
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
int project_grease_gp_rotate_stroke_about(ProjectGreaseGPHandle *handle, int index, float radians, float center_x, float center_y)
{ return ensure_ready(handle) && handle->backend.rotate_stroke_about(index, radians, center_x, center_y) ? 1 : 0; }
int project_grease_gp_scale_stroke(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y)
{ return ensure_ready(handle) && handle->backend.scale_stroke(index, scale_x, scale_y) ? 1 : 0; }
int project_grease_gp_scale_stroke_about(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y, float center_x, float center_y)
{ return ensure_ready(handle) && handle->backend.scale_stroke_about(index, scale_x, scale_y, center_x, center_y) ? 1 : 0; }
int project_grease_gp_mirror_stroke(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y)
{ return ensure_ready(handle) && handle->backend.mirror_stroke(index, mirror_x != 0, mirror_y != 0) ? 1 : 0; }
int project_grease_gp_mirror_stroke_about(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y, float center_x, float center_y)
{ return ensure_ready(handle) && handle->backend.mirror_stroke_about(index, mirror_x != 0, mirror_y != 0, center_x, center_y) ? 1 : 0; }

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
int project_grease_gp_trim_stroke_to_intersection(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.trim_stroke(index) ? 1 : 0;
}
int project_grease_gp_split_stroke(ProjectGreaseGPHandle *handle, int index, int before_index)
{
  return ensure_ready(handle) && handle->backend.split_stroke(index, before_index) ? 1 : 0;
}

int project_grease_gp_history_reset(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.history_reset() ? 1 : 0;
}
int project_grease_gp_history_record(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.history_record() ? 1 : 0;
}
int project_grease_gp_history_undo(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.history_undo() ? 1 : 0;
}
int project_grease_gp_history_redo(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.history_redo() ? 1 : 0;
}
int project_grease_gp_history_can_undo(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) return 0;
  return handle->backend.history_can_undo() ? 1 : 0;
}
int project_grease_gp_history_can_redo(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) return 0;
  return handle->backend.history_can_redo() ? 1 : 0;
}
int project_grease_gp_create_primitive(ProjectGreaseGPHandle *handle, int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, int material_index, float thickness)
{
  if (!ensure_ready(handle)) return 0;
  return handle->backend.create_primitive(type, x0, y0, x1, y1, start_angle, end_angle, segments, {material_index, thickness}) ? 1 : 0;
}

int project_grease_gp_generate_primitive_preview(int type,
                                                float x0,
                                                float y0,
                                                float x1,
                                                float y1,
                                                float start_angle,
                                                float end_angle,
                                                int segments,
                                                float *xy,
                                                int capacity,
                                                int *count)
{
  if (!xy || !count || capacity < 1) return 0;
  const auto geometry = project_grease::legacy_gp_primitive::generate(
      type, {x0, y0}, {x1, y1}, start_angle, end_angle, segments);
  if (geometry.empty() || static_cast<int>(geometry.size()) > capacity) return 0;
  for (size_t i = 0; i < geometry.size(); ++i) {
    xy[i * 2] = geometry[i].x;
    xy[i * 2 + 1] = geometry[i].y;
  }
  *count = static_cast<int>(geometry.size());
  return 1;
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

int project_grease_gp_soft_erase_at(ProjectGreaseGPHandle *handle, float x, float y, float radius, float strength)
{
  return ensure_ready(handle) &&
                 handle->backend.soft_erase_at(x, y, radius, strength) ? 1 : 0;
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


int project_grease_gp_apply_edit_command(ProjectGreaseGPHandle *handle,
                                         int command,
                                         const float *args,
                                         int arg_count)
{
  if (!ensure_ready(handle)) return 0;

  switch (command) {
    case 1: // select all: args[0] = mode
      return handle->backend.select_all(
                 arg_count > 0 ? static_cast<int>(args[0]) : 0)
             ? 1
             : 0;
    case 2: // circle select: x, y, radius, mode
      if (!args || arg_count < 4) return 0;
      return handle->backend.select_circle(args[0], args[1], args[2],
                                           static_cast<int>(args[3]));
    case 3: // reverse selected strokes
      return handle->backend.reverse_selected_strokes() ? 1 : 0;
    case 4: // dissolve selected points
      return handle->backend.dissolve_selected_points() ? 1 : 0;
    case 5: // merge selected points: threshold
      if (!args || arg_count < 1) return 0;
      return handle->backend.merge_selected_points(args[0]) ? 1 : 0;
    case 6: // reorder selected strokes: direction
      return handle->backend.reorder_selected_strokes(
                 arg_count > 0 ? static_cast<int>(args[0]) : 1)
             ? 1
             : 0;
    case 7: // join selected strokes
      return handle->backend.join_selected_strokes() ? 1 : 0;
    case 8: // select first points: only_selected_strokes, extend
      return handle->backend.select_first_points(
                 arg_count > 0 && args[0] != 0.0f,
                 arg_count > 1 && args[1] != 0.0f)
             ? 1
             : 0;
    case 9: // select grouped: 0=layer, 1=material
      return handle->backend.select_grouped(
                 arg_count > 0 ? static_cast<int>(args[0]) : 0)
             ? 1
             : 0;
    default:
      // Selection-aware editing (ids 31..37), then the selection operators (20..30).
      if (command == PG_EDIT7_CMD_LAYER_MERGE) {
        return handle->backend.merge_layer_down() ? 1 : 0;
      }
      if (command >= PG_EDIT9_CMD_FIRST && command <= PG_EDIT9_CMD_LAST) {
        const int changed = pg_gp_edit9_dispatch(handle->backend.document_data(),
                                                 handle->backend.active_layer_data(),
                                                 command,
                                                 args,
                                                 arg_count);
        /* moving, scaling and pasting frames can renumber or free the layer's actframe */
        if (changed && command >= PG_EDIT9_CMD_FRAMES_MOVE && command <= PG_EDIT9_CMD_FRAMES_PASTE) {
          handle->backend.sync_active_frame();
        }
        return changed;
      }
      if (command >= PG_EDIT5_CMD_FIRST && command <= PG_EDIT5_CMD_LAST) {
        return pg_gp_edit5_dispatch(handle->backend.document_data(),
                                    handle->backend.active_layer_data(),
                                    command,
                                    args,
                                    arg_count);
      }
      if (command >= PG_EDIT_CMD_FIRST && command <= PG_EDIT_CMD_LAST) {
        const int changed = pg_gp_edit_dispatch(handle->backend.document_data(),
                                                handle->backend.active_layer_data(),
                                                command,
                                                args,
                                                arg_count);
        if (changed && (command == PG_EDIT3_CMD_BLANK_FRAME || command == PG_EDIT3_CMD_CLEAN_DUP_FRAMES)) {
          /* these move the layer's actframe (and clean may free the cached one) */
          handle->backend.sync_active_frame();
        }
        return changed;
      }
      return pg_gp_select_dispatch(handle->backend.document_data(),
                                   handle->backend.active_layer_data(),
                                   command,
                                   args,
                                   arg_count);
  }
}


int project_grease_gp_annotation_command(ProjectGreaseGPHandle *handle, int command, const float *args, int arg_count)
{
  if (!ensure_ready(handle)) return 0;
  bGPdata *annot = handle->backend.annotation_data();
  if (!annot) return 0;
  const int frame = handle->backend.current_frame_number();
#if defined(__ANDROID__) && defined(__has_include)
#if __has_include(<android/log.h>)
  // Sweep evidence (annotations test): the commands the touch path sends.
  if (command != PG_ANNOT_CMD_ADD_POINT && command != PG_ANNOT_CMD_COUNT) {
    __android_log_print(ANDROID_LOG_INFO, "ProjectGrease", "annotcmd %d frame=%d strokes=%d", command, frame, pg_annot_stroke_count(annot));
  }
#endif
#endif
  switch (command) {
    case PG_ANNOT_CMD_BEGIN:
      return pg_annot_begin(annot, frame);
    case PG_ANNOT_CMD_ADD_POINT:
      return args && arg_count >= 2 ? pg_annot_add_point(annot, args[0], args[1]) : 0;
    case PG_ANNOT_CMD_END:
      return pg_annot_end(annot);
    case PG_ANNOT_CMD_CANCEL:
      pg_annot_cancel(annot);
      return 1;
    case PG_ANNOT_CMD_ERASE:
      return args && arg_count >= 3 ? pg_annot_erase(annot, frame, args[0], args[1], args[2]) : 0;
    case PG_ANNOT_CMD_CLEAR:
      return pg_annot_clear(annot);
    case PG_ANNOT_CMD_SET_STYLE:
      if (!args || arg_count < 5) return 0;
      pg_annot_set_style(annot, args, args[4]);
      return 1;
    case PG_ANNOT_CMD_SET_VISIBLE:
      if (!args || arg_count < 1) return 0;
      handle->backend.set_annotations_visible(args[0] != 0.0f);
      return 1;
    case PG_ANNOT_CMD_COUNT:
      return pg_annot_stroke_count(annot);
    default:
      return 0;
  }
}

int project_grease_gp_annotation_style(const ProjectGreaseGPHandle *handle, float out[6])
{
  if (!handle || !out) return 0;
  float th = 0.0f;
  if (!pg_annot_get_style(handle->backend.annotation_data(), out, &th)) return 0;
  out[4] = th;
  out[5] = handle->backend.annotations_visible() ? 1.0f : 0.0f;
  return 1;
}

int project_grease_gp_annotation_dump(const ProjectGreaseGPHandle *handle, float *out, int capacity)
{
  return handle ? pg_annot_dump(handle->backend.annotation_data(), out, capacity) : 0;
}

int project_grease_gp_annotation_load(ProjectGreaseGPHandle *handle, const float *data, int count)
{
  if (!ensure_ready(handle)) return 0;
  return pg_annot_load(handle->backend.annotation_data(), data, count);
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

int project_grease_gp_get_stroke_info(const ProjectGreaseGPHandle *handle, int stroke_index, PGStrokeInfo *out)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) || !out) {
    return 0;
  }
  return handle->backend.get_stroke_info(stroke_index, out) ? 1 : 0;
}

int project_grease_gp_get_point_color(const ProjectGreaseGPHandle *handle,
                                      int stroke_index,
                                      int point_index,
                                      float out_rgba[4])
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) || !out_rgba) {
    return 0;
  }
  project_grease::gp::StrokePoint native_point{};
  if (!handle->backend.get_point(stroke_index, point_index, &native_point)) {
    return 0;
  }
  out_rgba[0] = native_point.r;
  out_rgba[1] = native_point.g;
  out_rgba[2] = native_point.b;
  out_rgba[3] = native_point.a;
  return 1;
}

int project_grease_gp_add_stroke(ProjectGreaseGPHandle *handle,
                                 const ProjectGreaseGPPoint *points,
                                 const float *colors,
                                 int count,
                                 const PGStrokeInfo *info)
{
  if (!ensure_ready(handle) || !points || count < 1 || !info) {
    return 0;
  }
  std::vector<project_grease::gp::StrokePoint> native_points(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    project_grease::gp::StrokePoint &dst = native_points[static_cast<size_t>(i)];
    dst.x = points[i].x;
    dst.y = points[i].y;
    dst.z = points[i].z;
    dst.pressure = points[i].pressure;
    dst.strength = points[i].strength;
    dst.time = points[i].time;
    if (colors) {
      dst.r = colors[i * 4];
      dst.g = colors[i * 4 + 1];
      dst.b = colors[i * 4 + 2];
      dst.a = colors[i * 4 + 3];
    }
  }
  return handle->backend.add_stroke(native_points.data(), count, *info) ? 1 : 0;
}

int project_grease_gp_get_layer_info(const ProjectGreaseGPHandle *handle, int index, PGLayerInfo *out)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) || !out) {
    return 0;
  }
  return handle->backend.get_layer_info(index, out) ? 1 : 0;
}

int project_grease_gp_set_layer_opacity(ProjectGreaseGPHandle *handle, int index, float opacity)
{
  return ensure_ready(handle) && handle->backend.set_layer_opacity(index, opacity) ? 1 : 0;
}

int project_grease_gp_vertex_group_count(const ProjectGreaseGPHandle *handle)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) ? handle->backend.vertex_group_count() : 0;
}

int project_grease_gp_vertex_group_name(const ProjectGreaseGPHandle *handle, int group, char *name, int name_capacity)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) &&
                 handle->backend.vertex_group_name(group, name, name_capacity)
             ? 1
             : 0;
}

int project_grease_gp_vertex_group_add(ProjectGreaseGPHandle *handle, const char *name)
{
  return ensure_ready(handle) ? handle->backend.vertex_group_add(name) : -1;
}

int project_grease_gp_vertex_group_remove(ProjectGreaseGPHandle *handle, int group)
{
  return ensure_ready(handle) && handle->backend.vertex_group_remove(group) ? 1 : 0;
}

int project_grease_gp_vertex_group_rename(ProjectGreaseGPHandle *handle, int group, const char *name)
{
  return ensure_ready(handle) && handle->backend.vertex_group_rename(group, name) ? 1 : 0;
}

int project_grease_gp_vertex_group_active(const ProjectGreaseGPHandle *handle)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) ? handle->backend.vertex_group_active() : -1;
}

int project_grease_gp_set_vertex_group_active(ProjectGreaseGPHandle *handle, int group)
{
  return ensure_ready(handle) && handle->backend.set_vertex_group_active(group) ? 1 : 0;
}

int project_grease_gp_point_weight_count(const ProjectGreaseGPHandle *handle, int stroke_index, int point_index)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) ? handle->backend.point_weight_count(stroke_index, point_index) : 0;
}

int project_grease_gp_point_weight_at(const ProjectGreaseGPHandle *handle, int stroke_index, int point_index, int k, int *group, float *weight)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) &&
                 handle->backend.point_weight_at(stroke_index, point_index, k, group, weight)
             ? 1
             : 0;
}

int project_grease_gp_set_point_weight(ProjectGreaseGPHandle *handle, int stroke_index, int point_index, int group, float weight)
{
  return ensure_ready(handle) && handle->backend.set_point_weight(stroke_index, point_index, group, weight) ? 1 : 0;
}

int project_grease_gp_layer_use_mask(const ProjectGreaseGPHandle *handle, int layer_index)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) && handle->backend.layer_use_mask(layer_index) ? 1 : 0;
}

int project_grease_gp_set_layer_use_mask(ProjectGreaseGPHandle *handle, int layer_index, int enabled)
{
  return ensure_ready(handle) && handle->backend.set_layer_use_mask(layer_index, enabled != 0) ? 1 : 0;
}

int project_grease_gp_mask_count(const ProjectGreaseGPHandle *handle, int layer_index)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) ? handle->backend.mask_count(layer_index) : 0;
}

int project_grease_gp_mask_add(ProjectGreaseGPHandle *handle, int layer_index, int mask_layer_index)
{
  return ensure_ready(handle) && handle->backend.mask_add(layer_index, mask_layer_index) ? 1 : 0;
}

int project_grease_gp_mask_remove(ProjectGreaseGPHandle *handle, int layer_index, int mask_index)
{
  return ensure_ready(handle) && handle->backend.mask_remove(layer_index, mask_index) ? 1 : 0;
}

int project_grease_gp_mask_get(const ProjectGreaseGPHandle *handle, int layer_index, int mask_index, char *name, int name_capacity, int *flags)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) &&
                 handle->backend.mask_get(layer_index, mask_index, name, name_capacity, flags)
             ? 1
             : 0;
}

int project_grease_gp_mask_set_flags(ProjectGreaseGPHandle *handle, int layer_index, int mask_index, int flags)
{
  return ensure_ready(handle) && handle->backend.mask_set_flags(layer_index, mask_index, flags) ? 1 : 0;
}

int project_grease_gp_modifier_count(const ProjectGreaseGPHandle *handle, int layer_index)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return 0;
  }
  return handle->backend.modifier_count(layer_index);
}

int project_grease_gp_modifier_add(ProjectGreaseGPHandle *handle, int layer_index, int type)
{
  return ensure_ready(handle) ? handle->backend.modifier_add(layer_index, type) : -1;
}

int project_grease_gp_modifier_remove(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index)
{
  return ensure_ready(handle) && handle->backend.modifier_remove(layer_index, modifier_index) ? 1 : 0;
}

int project_grease_gp_modifier_move(ProjectGreaseGPHandle *handle, int layer_index, int from_index, int to_index)
{
  return ensure_ready(handle) && handle->backend.modifier_move(layer_index, from_index, to_index) ? 1 : 0;
}

int project_grease_gp_modifier_set_enabled(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, int enabled)
{
  return ensure_ready(handle) &&
                 handle->backend.modifier_set_enabled(layer_index, modifier_index, enabled != 0)
             ? 1
             : 0;
}

int project_grease_gp_modifier_set_params(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, const float *params, int count)
{
  return ensure_ready(handle) &&
                 handle->backend.modifier_set_params(layer_index, modifier_index, params, count)
             ? 1
             : 0;
}

int project_grease_gp_modifier_get(const ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, int *type, int *enabled, float *params, int capacity)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return -1;
  }
  return handle->backend.modifier_get(layer_index, modifier_index, type, enabled, params, capacity);
}

int project_grease_gp_modifier_apply(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index)
{
  return ensure_ready(handle) && handle->backend.modifier_apply(layer_index, modifier_index) ? 1 : 0;
}

int project_grease_gp_fx_count(const ProjectGreaseGPHandle *handle, int layer_index)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return 0;
  }
  return handle->backend.fx_count(layer_index);
}

int project_grease_gp_fx_add(ProjectGreaseGPHandle *handle, int layer_index, int type)
{
  return ensure_ready(handle) ? handle->backend.fx_add(layer_index, type) : -1;
}

int project_grease_gp_fx_remove(ProjectGreaseGPHandle *handle, int layer_index, int fx_index)
{
  return ensure_ready(handle) && handle->backend.fx_remove(layer_index, fx_index) ? 1 : 0;
}

int project_grease_gp_fx_move(ProjectGreaseGPHandle *handle, int layer_index, int from_index, int to_index)
{
  return ensure_ready(handle) && handle->backend.fx_move(layer_index, from_index, to_index) ? 1 : 0;
}

int project_grease_gp_fx_set_enabled(ProjectGreaseGPHandle *handle, int layer_index, int fx_index, int enabled)
{
  return ensure_ready(handle) && handle->backend.fx_set_enabled(layer_index, fx_index, enabled != 0) ? 1 : 0;
}

int project_grease_gp_fx_set_target(ProjectGreaseGPHandle *handle, int layer_index, int fx_index, int target)
{
  return ensure_ready(handle) && handle->backend.fx_set_target(layer_index, fx_index, target) ? 1 : 0;
}

int project_grease_gp_fx_target(const ProjectGreaseGPHandle *handle, int layer_index, int fx_index)
{
  return ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) ? handle->backend.fx_target(layer_index, fx_index) : -1;
}

int project_grease_gp_fx_set_params(ProjectGreaseGPHandle *handle, int layer_index, int fx_index, const float *params, int count)
{
  return ensure_ready(handle) && handle->backend.fx_set_params(layer_index, fx_index, params, count) ? 1 : 0;
}

int project_grease_gp_fx_get(const ProjectGreaseGPHandle *handle, int layer_index, int fx_index, int *type, int *enabled, float *params, int capacity)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) {
    return -1;
  }
  return handle->backend.fx_get(layer_index, fx_index, type, enabled, params, capacity);
}

int project_grease_gp_get_material_info(const ProjectGreaseGPHandle *handle, int index, PGMaterialInfo *out)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle)) || !out) {
    return 0;
  }
  return handle->backend.get_material_info(index, out) ? 1 : 0;
}

int project_grease_gp_set_point(ProjectGreaseGPHandle *handle,
                                int stroke_index,
                                int point_index,
                                ProjectGreaseGPPoint point)
{
  if (!ensure_ready(handle)) {
    return 0;
  }
  project_grease::gp::StrokePoint native_point{};
  native_point.x = point.x;
  native_point.y = point.y;
  native_point.z = point.z;
  native_point.pressure = point.pressure;
  native_point.strength = point.strength;
  native_point.time = point.time;
  return handle->backend.set_point(stroke_index, point_index, native_point) ? 1 : 0;
}

int project_grease_gp_material_count(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) return 0;
  return handle->backend.material_count();
}
int project_grease_gp_create_material(ProjectGreaseGPHandle *handle)
{
  return ensure_ready(handle) && handle->backend.create_material() ? 1 : 0;
}
int project_grease_gp_set_material_colors(ProjectGreaseGPHandle *handle, int index, const float stroke_rgba[4], const float fill_rgba[4])
{
  return ensure_ready(handle) && handle->backend.set_material_colors(index, stroke_rgba, fill_rgba) ? 1 : 0;
}
int project_grease_gp_set_material_visibility(ProjectGreaseGPHandle *handle, int index, int visible)
{
  return ensure_ready(handle) && handle->backend.set_material_visibility(index, visible != 0) ? 1 : 0;
}
int project_grease_gp_set_material_fill_enabled(ProjectGreaseGPHandle *handle, int index, int enabled)
{
  return ensure_ready(handle) && handle->backend.set_material_fill_enabled(index, enabled != 0) ? 1 : 0;
}
int project_grease_gp_smooth_stroke(ProjectGreaseGPHandle *handle, int index, float influence, int iterations)
{
  return ensure_ready(handle) && handle->backend.smooth_stroke(index, influence, iterations) ? 1 : 0;
}

int project_grease_gp_sculpt_at(ProjectGreaseGPHandle *handle, int tool, float x, float y, float radius, float influence)
{
  return ensure_ready(handle) && handle->backend.sculpt_at(tool, x, y, radius, influence) ? 1 : 0;
}

int project_grease_gp_apply_legacy_geometry_batch(
    ProjectGreaseGPHandle *handle,
    int stroke_index,
    const ProjectGreaseGPLegacyGeometryOp *operations,
    int operation_count)
{
  if (!ensure_ready(handle) || !operations || operation_count <= 0) {
    return 0;
  }

  std::vector<project_grease::gp::Backend::LegacyGeometryOp> native_operations;
  native_operations.reserve(static_cast<size_t>(operation_count));

  for (int i = 0; i < operation_count; ++i) {
    const ProjectGreaseGPLegacyGeometryOp &src = operations[i];
    project_grease::gp::Backend::LegacyGeometryOp dst{};
    if (src.type < 0 ||
        src.type > static_cast<int>(
                       project_grease::gp::Backend::LegacyGeometryOpType::RandomColor)) {
      return 0;
    }
    dst.type = static_cast<project_grease::gp::Backend::LegacyGeometryOpType>(src.type);
    dst.value0 = src.value0;
    dst.value1 = src.value1;
    dst.value2 = src.value2;
    dst.int0 = src.int0;
    dst.int1 = src.int1;
    dst.flag0 = src.flag0 != 0;
    dst.flag1 = src.flag1 != 0;
    native_operations.push_back(dst);
  }

  return handle->backend.apply_legacy_geometry_batch(
             stroke_index, native_operations.data(), operation_count)
             ? 1
             : 0;
}

int project_grease_gp_apply_blender_modifier(ProjectGreaseGPHandle *handle,
                                              int index,
                                              int modifier_type,
                                              float factor,
                                              int iterations)
{
  return ensure_ready(handle) &&
                 handle->backend.apply_blender_modifier(index, modifier_type, factor, iterations)
             ? 1
             : 0;
}

int project_grease_gp_apply_blender_modifier_named(ProjectGreaseGPHandle *handle,
                                                    int index,
                                                    const char *name,
                                                    float factor,
                                                    int iterations)
{
  if (!ensure_ready(handle) || !name) return 0;
  const std::string key(name);
  int type = -1;
  if (key == "SMOOTH") type = eGpencilModifierType_Smooth;
  else if (key == "SIMPLIFY") type = eGpencilModifierType_Simplify;
  else if (key == "SUBDIVIDE") type = eGpencilModifierType_Subdiv;
  else return 0;
  return handle->backend.apply_blender_modifier(index, type, factor, iterations) ? 1 : 0;
}

int project_grease_gp_apply_blender_modifier_stack(ProjectGreaseGPHandle *handle,
                                                   int index,
                                                   const int *modifier_types,
                                                   int modifier_count,
                                                   float factor,
                                                   int iterations)
{
  if (!ensure_ready(handle) || !modifier_types || modifier_count <= 0) {
    return 0;
  }
  return handle->backend.apply_blender_modifier_stack(
             index, modifier_types, modifier_count, factor, iterations)
             ? 1
             : 0;
}

int project_grease_gp_set_onion_skin(ProjectGreaseGPHandle *handle, int enabled, int before, int after, float opacity)
{
  return ensure_ready(handle) && handle->backend.set_onion_skin(enabled != 0, before, after, opacity) ? 1 : 0;
}
int project_grease_gp_set_multiframe_editing(ProjectGreaseGPHandle *handle, int enabled)
{
  return ensure_ready(handle) && handle->backend.set_multiframe_editing(enabled != 0) ? 1 : 0;
}

int project_grease_gp_fill_stroke(ProjectGreaseGPHandle *handle, int index)
{
  return ensure_ready(handle) && handle->backend.fill_stroke(index) ? 1 : 0;
}


void project_grease_gp_set_fill_screen_map(ProjectGreaseGPHandle *handle, float scale, float origin_x, float origin_y)
{
  if (handle) handle->backend.set_fill_screen_map(scale, origin_x, origin_y);
}

int project_grease_gp_fill_at_screen(ProjectGreaseGPHandle *handle,
                                     const float *rgba,
                                     int width,
                                     int height,
                                     int seed_x,
                                     int seed_y,
                                     int fill_leak,
                                     int dilate_pixels,
                                     int material_index,
                                     float thickness)
{
  if (!ensure_ready(handle)) return 0;
  return handle->backend.fill_at_screen(
      rgba,
      width,
      height,
      seed_x,
      seed_y,
      fill_leak,
      dilate_pixels,
      {material_index, thickness}) ? 1 : 0;
}

int project_grease_gp_frame_end(const ProjectGreaseGPHandle *handle)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) return 1;
  return handle->backend.frame_end();
}
int project_grease_gp_frame_numbers(const ProjectGreaseGPHandle *handle, int *out_frames, int capacity)
{
  if (!ensure_ready(const_cast<ProjectGreaseGPHandle *>(handle))) return 0;
  return handle->backend.frame_numbers(out_frames, capacity);
}
int project_grease_gp_interpolate_frame_eased(ProjectGreaseGPHandle *handle, int source_frame, int target_frame, int result_frame, float factor, int easing_type, int easing_mode)
{
  return ensure_ready(handle) &&
                 handle->backend.interpolate_frame(source_frame, target_frame, result_frame, factor,
                                                   easing_type, easing_mode)
             ? 1
             : 0;
}

int project_grease_gp_interpolate_frame(ProjectGreaseGPHandle *handle, int source_frame, int target_frame, int result_frame, float factor)
{
  return ensure_ready(handle) && handle->backend.interpolate_frame(source_frame, target_frame, result_frame, factor) ? 1 : 0;
}


// ---- Native tool session --------------------------------------------------------------------
// Draw sink: the backend's stroke buffer, as the former per-point JNI calls used it.
static int tool_sink_begin(void *user, int material, float thickness)
{
  auto *handle = static_cast<ProjectGreaseGPHandle *>(user);
  return handle->backend.begin_stroke({material, thickness}) ? 1 : 0;
}
static int tool_sink_add(void *user, float x, float y, float pressure, float strength, float time)
{
  auto *handle = static_cast<ProjectGreaseGPHandle *>(user);
  // Floors as the former Kotlin sender: pressure at GPENCIL_ALPHA_OPACITY_THRESH, strength >= 0.
  return handle->backend.add_point(project_grease::gp::StrokePoint{
             x, y, 0.0f, std::max(pressure, 0.001f), std::max(strength, 0.0f), time})
             ? 1
             : 0;
}
static int tool_sink_end(void *user)
{
  return static_cast<ProjectGreaseGPHandle *>(user)->backend.end_stroke() ? 1 : 0;
}
static void tool_sink_cancel(void *user)
{
  static_cast<ProjectGreaseGPHandle *>(user)->backend.cancel_stroke();
}

int project_grease_gp_tool_samples(ProjectGreaseGPHandle *handle,
                                   int tool,
                                   const float *samples,
                                   int count,
                                   int phase,
                                   const float *params,
                                   int param_count)
{
  if (!ensure_ready(handle)) return 0;
  if (!handle->tool_session) {
    handle->tool_session = pg_tool_session_new();
    if (!handle->tool_session) return 0;
  }
  bGPdata *gpd = handle->backend.document_data();
  // A gesture never survives a document replacement (reset / load / undo): cancel it.
  if (phase != PG_TOOL_PHASE_BEGIN && handle->tool_gpd != gpd) {
    pg_tool_session_samples(handle->tool_session, nullptr, tool, nullptr, 0, PG_TOOL_PHASE_CANCEL,
                            nullptr, 0, nullptr);
    handle->tool_gpd = nullptr;
    return 0;
  }
  if (phase == PG_TOOL_PHASE_BEGIN) handle->tool_gpd = gpd;
  const PGToolDrawSink sink = {handle, tool_sink_begin, tool_sink_add, tool_sink_end, tool_sink_cancel};
  return pg_tool_session_samples(handle->tool_session, gpd, tool, samples, count, phase, params,
                                 param_count, &sink);
}
