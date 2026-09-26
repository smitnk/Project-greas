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

  bool create_frame(int frame_number);
  bool select_frame(int frame_number);
  int frame_count() const;
  int stroke_count() const;
  int point_count() const;
  bool select_stroke(int index);
  bool get_point(int stroke_index, int point_index, StrokePoint* out) const;
  bool set_point(int stroke_index, int point_index, const StrokePoint& point);
  bool delete_stroke(int index);
  bool delete_last_stroke();
  bool duplicate_stroke(int index);

  // Writes native GP stroke points. No Android Canvas rendering is used.
  bool begin_stroke(const StrokeStyle& style);
  bool add_point(const StrokePoint& point);
  bool end_stroke();

  // Rendering will be connected to the Blender GP draw/DRW path after
  // the minimal dependency target is linked.
  bool render();

  const char* last_error() const;

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace project_grease::gp
