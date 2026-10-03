#pragma once

#include <cstdint>
#include "project_grease_legacy_sculpt.h"
#include "project_grease_document_state.h"

// Blender DNA types, declared at global scope so the accessors below name the
// real C structs and not types of project_grease::gp.
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct PGFxEntry;

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
  // `factor` is the linear position between the frames; it is eased with
  // pg_gp_interpolate_easing(easing_type, easing_mode, factor) (PG_EASE_*, default Linear) before the
  // points are mixed, as gpencil_interpolate.c does.
  bool interpolate_frame(int source_frame, int target_frame, int result_frame, float factor,
                         int easing_type = 0, int easing_mode = 0);
  bool duplicate_frame(int source_frame, int target_frame);
  bool delete_frame(int frame_number);
  int stroke_count() const;
  int point_count() const;
  /** Points of the active frame with GP_SPOINT_SELECT (the edit selection). */
  int frame_selected_point_count() const;
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
  bool soft_erase_at(float x, float y, float radius, float strength);
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
  bool material_fill_enabled(int index) const;

  // Save/load state that get_point() does not carry: per-stroke style, layer state and the
  // material palette (see project_grease_document_state.h).
  bool get_stroke_info(int stroke_index, PGStrokeInfo* out) const;
  // Restores a stroke exactly as saved: no paint-stage smoothing, selection untouched.
  bool add_stroke(const StrokePoint* points, int count, const PGStrokeInfo& info);
  bool get_layer_info(int index, PGLayerInfo* out) const;

  // Vertex groups (bGPdata::vertex_group_names; the weights are bGPDstroke::dvert of the points).
  // Group indices are 0-based and are the `def_nr` of the weights; removing a group shifts the
  // higher indices down like BKE_gpencil_vgroup_remove().
  int vertex_group_count() const;
  bool vertex_group_name(int group, char* name, int name_capacity) const;
  int vertex_group_add(const char* name); // returns the new index or -1; names are made unique
  bool vertex_group_remove(int group);
  bool vertex_group_rename(int group, const char* name);
  int vertex_group_active() const;        // -1 when there is none
  bool set_vertex_group_active(int group);
  // Weights of the points of the current frame's strokes (for display, save and load).
  int point_weight_count(int stroke_index, int point_index) const;
  bool point_weight_at(int stroke_index, int point_index, int k, int* group, float* weight) const;
  // Creates the dvert/weight when needed; weight is clamped to 0..1.
  bool set_point_weight(int stroke_index, int point_index, int group, float weight);

  // Layer masks (bGPDlayer::mask_layers, GP_LAYER_USE_MASK, GP_MASK_INVERT/GP_MASK_HIDE): a masked
  // layer is drawn only where the union of its mask layers has coverage (inverted entries: where
  // they have none), as gpencil_engine.c gpencil_draw_mask() does. Masks refer to layers by name.
  bool layer_use_mask(int layer_index) const;
  bool set_layer_use_mask(int layer_index, bool enabled);
  int mask_count(int layer_index) const;
  // Adds the layer `mask_layer_index` (not the layer itself, no duplicates) to the mask list.
  bool mask_add(int layer_index, int mask_layer_index);
  bool mask_remove(int layer_index, int mask_index);
  // Name of the mask entry (empty when out of range) and its flags (bit 0 hidden, bit 1 inverted).
  bool mask_get(int layer_index, int mask_index, char* name, int name_capacity, int* flags) const;
  bool mask_set_flags(int layer_index, int mask_index, int flags);

  // Live (non-destructive) per-layer modifier stack, see project_grease_modifier_stack.h. The stack
  // is document state: strokes are never modified until modifier_apply().
  int modifier_count(int layer_index) const;
  // Appends a modifier with Blender's default parameters; returns its index or -1.
  int modifier_add(int layer_index, int type);
  bool modifier_remove(int layer_index, int modifier_index);
  bool modifier_move(int layer_index, int from_index, int to_index);
  bool modifier_set_enabled(int layer_index, int modifier_index, bool enabled);
  // Replaces the parameters (count values, sanitised); extra values are ignored.
  bool modifier_set_params(int layer_index, int modifier_index, const float* params, int count);
  // Returns the parameter count (and fills type/enabled/params) or -1.
  int modifier_get(int layer_index, int modifier_index, int* type, int* enabled, float* params,
                   int capacity) const;
  // Bakes the modifier into the layer's original strokes and removes it from the stack.
  bool modifier_apply(int layer_index, int modifier_index);
  // The frame to draw for `layer`: an evaluated copy of `current` when the layer has enabled
  // modifiers (cached until an edit, frame change or stack change), else `current` itself.
  const bGPDframe* evaluated_frame(const bGPDlayer* layer, const bGPDframe* current, int frame_number);
  // Number of stack evaluations run so far (cache hits do not count); for tests.
  uint64_t modifier_eval_count() const;
  // Per-layer shader effects (2D post-pass over the rendered layer, see project_grease_shader_fx.h).
  // Document state like the modifier stack: saved, in undo snapshots; strokes are never touched.
  int fx_count(int layer_index) const;
  int fx_add(int layer_index, int type);
  bool fx_remove(int layer_index, int fx_index);
  bool fx_move(int layer_index, int from_index, int to_index);
  bool fx_set_enabled(int layer_index, int fx_index, bool enabled);
  bool fx_set_params(int layer_index, int fx_index, const float* params, int count);
  int fx_get(int layer_index, int fx_index, int* type, int* enabled, float* params, int capacity) const;
  // The effect list of `layer` for the presenter (valid until the next fx_* edit); returns its length.
  int fx_for_layer(const bGPDlayer* layer, const PGFxEntry** entries) const;
  bool set_layer_opacity(int index, float opacity);
  bool get_material_info(int index, PGMaterialInfo* out) const;
  bool smooth_stroke(int index, float influence, int iterations);
  // Focused Legacy GP sculpt brush bridge. Position/strength/thickness use Blender BKE algorithms;
  // grab/push use the same real bGPD point data with a local falloff.
  bool sculpt_at(int tool, float x, float y, float radius, float influence);
  bool sculpt_begin(int tool, float x, float y, float pressure, float radius, float strength, bool invert);
  bool sculpt_update(int tool, float x, float y, float prev_x, float prev_y, float pressure, float radius, float strength, bool invert);
  bool sculpt_end();
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
    Reverse,
    UniformSubdivide,
    Shrink,
    RandomColor,
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

  // Raw Legacy GP document and active layer for focused editor operators
  // (Blender selection port). Valid until the document is reset or shut down.
  bGPdata* document_data() const;
  bGPDlayer* active_layer_data() const;
  /* Re-reads the active frame from the active layer's actframe after an edit command that adds,
   * moves or removes frames behind the backend (blank keyframe, clean duplicate frames). */
  void sync_active_frame();
  /* Annotation data (project_grease_annotations.h), owned by the document; never null after
   * the document is created. */
  bGPdata* annotation_data() const;
  void set_annotations_visible(bool visible);
  bool annotations_visible() const;
  /* Frame number of the active frame (1 without one). */
  int current_frame_number() const;

  // Opaque implementation storage.
  struct Impl;

 private:
  bool render_with_gpu_context();
  Impl* impl_;
};

}  // namespace project_grease::gp
