#pragma once

#include <cstdint>

namespace project_grease::gp {

struct StrokePoint {
  float x;
  float y;
  float z;
  float pressure;
  float strength;
  float time;
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 0.0f;
  float uv_fac = 0.0f;
  float uv_rot = 0.0f;
};

struct StrokeStyle {
  int material_index = 0;
  float thickness = 1.0f;
};

class Backend {
 public:
  Backend();
  ~Backend();

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

  // Lifecycle for the minimum native GP proof.
  bool initialize();
  void shutdown();

  // Creates the real Blender legacy GP data objects behind this adapter.
  bool create_document();
  bool reset_document();
  bool create_layer(const char* name);
  bool select_layer(int index);
  int layer_count() const;
  bool set_layer_visibility(int index, bool visible);
  bool set_layer_locked(int index, bool locked);
  bool move_layer(int from_index, int to_index);
  bool duplicate_layer(int index);
  bool delete_layer(int index);
  bool rename_layer(int index, const char* name);

  bool create_frame(int frame_number);
  bool select_frame(int frame_number);
  bool select_frame_or_hold(int frame_number);
  int frame_count() const;
  int frame_end() const;
  int frame_numbers(int *out_frames, int capacity) const;
  bool interpolate_frame(int source_frame, int target_frame, int result_frame, float factor);
  bool duplicate_frame(int source_frame, int target_frame);
  bool delete_frame(int frame_number);
  int stroke_count() const;
  int point_count() const;
  bool select_stroke(int index);
  int hit_test_stroke(float x, float y, float radius) const;
  bool get_point(int stroke_index, int point_index, StrokePoint* out) const;
  // Reads a real Blender Legacy GP deform-group weight from a stroke point.
  bool get_point_group_weight(int stroke_index, int point_index, int group_index, float* out) const;
  bool stroke_center(int stroke_index, float *x, float *y) const;
  bool set_point(int stroke_index, int point_index, const StrokePoint& point);
  bool delete_stroke(int index);
  bool delete_last_stroke();
  bool duplicate_stroke(int index);
  bool translate_stroke(int index, float dx, float dy, float dz);
  bool flip_stroke(int index);
  bool rotate_stroke(int index, float radians);
  bool rotate_stroke_about(int index, float radians, float center_x, float center_y);
  bool scale_stroke(int index, float scale_x, float scale_y);
  bool scale_stroke_about(int index, float scale_x, float scale_y, float center_x, float center_y);
  bool mirror_stroke(int index, bool mirror_x, bool mirror_y);
  bool mirror_stroke_about(int index, bool mirror_x, bool mirror_y, float center_x, float center_y);
  bool subdivide_stroke(int index, int level);
  bool close_stroke(int index);
  // Blender 3.6.23 Legacy GP trim: trim to the first self-intersection/loop.
  bool trim_stroke(int index);
  bool trim_stroke_points(int index, int index_from, int index_to, bool keep_single_point);
  // Real Legacy GP data snapshots used for editor undo/redo. Blender 3.6.23
  // uses BKE_gpencil_data_duplicate(..., internal_copy=true) for undo buffers.
  bool history_reset();
  bool history_record();
  bool history_undo();
  bool history_redo();
  bool history_can_undo() const;
  bool history_can_redo() const;
  bool split_stroke(int index, int before_index);
  bool create_primitive(int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, const StrokeStyle& style);
  bool create_polyline(const StrokePoint* points, int count, const StrokeStyle& style, bool cyclic);
  bool erase_at(float x, float y, float radius);
  void clear_selection();
  int lasso_select(const float* xy, int count, bool additive);
  // Bulk Legacy GP edit commands: selection, point editing and stroke ordering.
  bool select_all(int mode); // 0=set/select, 1=select, 2=invert, 3=deselect
  int select_circle(float x, float y, float radius, int mode); // 0=set, 1=add, 2=sub
  bool reverse_selected_strokes();
  bool dissolve_selected_points();
  bool merge_selected_points(float threshold);
  bool reorder_selected_strokes(int direction); // 0=top, 1=up, 2=down, 3=bottom
  bool join_selected_strokes();
  bool select_first_points(bool only_selected_strokes, bool extend);
  bool select_grouped(int type); // 0=layer, 1=material
  int material_count() const;
  bool create_material();
  bool set_material_colors(int index, const float stroke_rgba[4], const float fill_rgba[4]);
  bool set_material_visibility(int index, bool visible);
  bool set_material_fill_enabled(int index, bool enabled);
  bool smooth_stroke(int index, float influence, int iterations);
  // One batched entry point for the real Blender 3.6.23 Legacy GP geometry API.
  // Each operation dispatches directly to Blender's BKE_gpencil_* implementation;
  // Project Grease does not reimplement the geometry algorithms.
  enum class LegacyGeometryOpType : uint8_t {
    SimplifyAdaptive,
    SimplifyFixed,
    Subdivide,
    TrimIntersection,
    TrimPoints,
    MergeDistance,
    Sample,
    SmoothStrength,
    SmoothThickness,
    SmoothUV,
    Stretch,
    Close,
    Dissolve,
    FillTriangulate,
  };

  struct LegacyGeometryOp {
    LegacyGeometryOpType type;
    float value0 = 0.0f;
    float value1 = 0.0f;
    float value2 = 0.0f;
    int int0 = 0;
    int int1 = 0;
    bool flag0 = false;
    bool flag1 = false;
  };

  bool apply_legacy_geometry_batch(int stroke_index,
                                   const LegacyGeometryOp* operations,
                                   int operation_count);

  // Invoke Blender 3.6.23's real Legacy GP modifier deformStroke callback on one stroke.
  // No Project Grease geometry algorithm is used for the modifier itself.
  bool apply_blender_modifier(int stroke_index, int modifier_type, float factor, int iterations);
  // Executes an ordered real Blender Legacy GP modifier list against the current stroke.
  // The list is owned by the focused adapter only; every operation is Blender's callback.
  bool apply_blender_modifier_stack(int stroke_index,
                                     const int* modifier_types,
                                     int modifier_count,
                                     float factor,
                                     int iterations);
  bool apply_blender_generator(int modifier_type, float factor, int iterations);
  bool set_onion_skin(bool enabled, int before, int after, float opacity);
  bool set_multiframe_editing(bool enabled);
  // Real Blender 3.6.23 Legacy GP fill geometry: triangulates a closed
  // stroke using BKE_gpencil_stroke_fill_triangulate().
  bool fill_stroke(int index);
  // Real Blender 3.6.23 fill boundary extraction fed by Android's rendered mask.
  bool fill_at_screen(const float* rgba,
                      int width,
                      int height,
                      int seed_x,
                      int seed_y,
                      int fill_leak,
                      int dilate_pixels,
                      const StrokeStyle& style);

  // Blender 3.6.23 Legacy GP paint-stage settings used when committing the
  // real tGPspoint buffer into bGPDstroke data. The processing below calls
  // Blender's own GP geometry functions rather than a Project Grease smoother.
  struct LegacyPaintSettings {
    int draw_smooth_level = 0;
    float draw_smooth_factor = 0.0f;
    int input_samples = 0;
    bool smooth_position = true;
    bool smooth_strength = true;
  };

  bool set_legacy_paint_settings(const LegacyPaintSettings& settings);

  // Writes native GP stroke points. No Android Canvas rendering is used.
  bool begin_stroke(const StrokeStyle& style);
  bool add_point(const StrokePoint& point);
  // Number of points currently held in Blender 3.6.23's real GP stroke buffer.
  int stroke_buffer_count() const;
  bool end_stroke();
  bool cancel_stroke();

  // Creates the Blender GPU context on an already-current external OpenGL
  // context. The caller owns the GL/EGL context; Blender does not create or
  // destroy GHOST here. This is the Android integration seam.
  bool initialize_external_gpu_context();

  // Builds the Blender GP draw cache using the externally-owned current GL
  // context. The caller must keep that context current on this thread.
  bool render_external_context();
  bool render_fill_mask();

  // Desktop/native proof path. This owns a temporary GHOST context.
  bool render();

  const char* last_error() const;

  // Opaque implementation storage.
  struct Impl;

 private:
  bool render_with_gpu_context();
  Impl* impl_;
};

}  // namespace project_grease::gp
