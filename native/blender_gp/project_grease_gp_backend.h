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
  bool duplicate_frame(int source_frame, int target_frame);
  bool delete_frame(int frame_number);
  int stroke_count() const;
  int point_count() const;
  bool select_stroke(int index);
  int hit_test_stroke(float x, float y, float radius) const;
  bool get_point(int stroke_index, int point_index, StrokePoint* out) const;
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
  bool trim_stroke_points(int index, int index_from, int index_to, bool keep_single_point);
  bool split_stroke(int index, int before_index);
  bool create_primitive(int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, const StrokeStyle& style);
  bool create_polyline(const StrokePoint* points, int count, const StrokeStyle& style, bool cyclic);
  bool erase_at(float x, float y, float radius);
  void clear_selection();
  int lasso_select(const float* xy, int count, bool additive);
  int material_count() const;
  bool create_material();
  bool set_material_colors(int index, const float stroke_rgba[4], const float fill_rgba[4]);
  bool set_material_visibility(int index, bool visible);
  bool set_material_fill_enabled(int index, bool enabled);
  bool smooth_stroke(int index, float influence, int iterations);
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

  // Writes native GP stroke points. No Android Canvas rendering is used.
  bool begin_stroke(const StrokeStyle& style);
  bool add_point(const StrokePoint& point);
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

 private:
  bool render_with_gpu_context();
  struct Impl;
  Impl* impl_;
};

}  // namespace project_grease::gp
