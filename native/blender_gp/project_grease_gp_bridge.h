#pragma once

#include "project_grease_document_state.h"

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

typedef struct ProjectGreaseGPLegacyPaintSettings {
  int draw_smooth_level;
  float draw_smooth_factor;
  int input_samples;
  int smooth_position;
  int smooth_strength;
} ProjectGreaseGPLegacyPaintSettings;

int project_grease_gp_set_legacy_paint_settings(
    ProjectGreaseGPHandle *handle,
    ProjectGreaseGPLegacyPaintSettings settings);

int project_grease_gp_begin_stroke(
    ProjectGreaseGPHandle *handle,
    int material_index,
    float thickness);

int project_grease_gp_add_point(
    ProjectGreaseGPHandle *handle,
    ProjectGreaseGPPoint point);

int project_grease_gp_end_stroke(ProjectGreaseGPHandle *handle);
int project_grease_gp_cancel_stroke(ProjectGreaseGPHandle *handle);
/* Native tool session (project_grease_tool_session.h): one input batch of `count` samples
 * (x, y, pressure, time); on PG_TOOL_PHASE_BEGIN `params` are the tool parameters. Returns the
 * PG_TOOL_RESULT_* bits (0 = refused). */
int project_grease_gp_tool_samples(ProjectGreaseGPHandle *handle, int tool, const float *samples,
                                   int count, int phase, const float *params, int param_count);
/* Ends any open tool session / Lasso Warp on the current document (safe no-op when idle). Brush edits
 * made so far stay. Returns 1 when the document changed and the caller must record an undo step
 * (result had PG_TOOL_RESULT_ENDED). A BEGIN that cut off an earlier gesture reports the same via
 * PG_TOOL_RESULT_ENDED in its result. */
int project_grease_gp_cancel_tool_session(ProjectGreaseGPHandle *handle);
int project_grease_gp_lattice_active(const ProjectGreaseGPHandle *handle);
/* The EGL context was replaced: forget the document's GPU caches (new context current, nothing drawn yet). */
void project_grease_gp_context_reset(ProjectGreaseGPHandle *handle);
int project_grease_gp_initialize_external_gpu(ProjectGreaseGPHandle *handle);
int project_grease_gp_render_external_context(ProjectGreaseGPHandle *handle);
int project_grease_gp_render_fill_mask(ProjectGreaseGPHandle *handle);
int project_grease_gp_render(ProjectGreaseGPHandle *handle);

int project_grease_gp_stroke_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_point_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_selected_point_count(const ProjectGreaseGPHandle *handle);

int project_grease_gp_create_layer(ProjectGreaseGPHandle *handle, const char *name);
int project_grease_gp_reset_document(ProjectGreaseGPHandle *handle);
int project_grease_gp_select_layer(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_layer_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_set_layer_visibility(ProjectGreaseGPHandle *handle, int index, int visible);
int project_grease_gp_set_layer_locked(ProjectGreaseGPHandle *handle, int index, int locked);
int project_grease_gp_move_layer(ProjectGreaseGPHandle *handle, int from_index, int to_index);
int project_grease_gp_duplicate_layer(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_delete_layer(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_rename_layer(ProjectGreaseGPHandle *handle, int index, const char *name);
/* Batch 21: document query (project_grease_blender_edit8.h PG_DOC_Q_*) and material slot names
 * (Material.id.name without the "MA" prefix; empty when never named). */
int project_grease_gp_doc_query(const ProjectGreaseGPHandle *handle, int what, const float *args, int arg_count,
                                float *out, int capacity);
int project_grease_gp_material_name(const ProjectGreaseGPHandle *handle, int slot, char *out, int capacity);
int project_grease_gp_set_material_name(ProjectGreaseGPHandle *handle, int slot, const char *name);
int project_grease_gp_create_frame(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_select_frame(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_select_frame_or_hold(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_frame_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_frame_end(const ProjectGreaseGPHandle *handle);
int project_grease_gp_frame_numbers(const ProjectGreaseGPHandle *handle, int *out_frames, int capacity);
int project_grease_gp_interpolate_frame(ProjectGreaseGPHandle *handle, int source_frame, int target_frame, int result_frame, float factor);
/* easing_type/easing_mode: PG_EASE_* of project_grease_blender_edit.h */
int project_grease_gp_interpolate_frame_eased(ProjectGreaseGPHandle *handle, int source_frame, int target_frame, int result_frame, float factor, int easing_type, int easing_mode);
int project_grease_gp_duplicate_frame(ProjectGreaseGPHandle *handle, int source_frame, int target_frame);
int project_grease_gp_delete_frame(ProjectGreaseGPHandle *handle, int frame_number);
int project_grease_gp_select_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_stroke_selected(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_move_stroke(ProjectGreaseGPHandle *handle, int from, int to);
int project_grease_gp_stroke_center(const ProjectGreaseGPHandle *handle, int index, float *x, float *y);
int project_grease_gp_hit_test_stroke(
    const ProjectGreaseGPHandle *handle,
    float x,
    float y,
    float radius);
int project_grease_gp_delete_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_delete_last_stroke(ProjectGreaseGPHandle *handle);
int project_grease_gp_duplicate_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_translate_stroke(ProjectGreaseGPHandle *handle, int index, float dx, float dy, float dz);
int project_grease_gp_flip_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_rotate_stroke(ProjectGreaseGPHandle *handle, int index, float radians);
int project_grease_gp_rotate_stroke_about(ProjectGreaseGPHandle *handle, int index, float radians, float center_x, float center_y);
int project_grease_gp_scale_stroke(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y);
int project_grease_gp_scale_stroke_about(ProjectGreaseGPHandle *handle, int index, float scale_x, float scale_y, float center_x, float center_y);
int project_grease_gp_mirror_stroke(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y);
int project_grease_gp_mirror_stroke_about(ProjectGreaseGPHandle *handle, int index, int mirror_x, int mirror_y, float center_x, float center_y);
int project_grease_gp_subdivide_stroke(ProjectGreaseGPHandle *handle, int index, int level);
int project_grease_gp_close_stroke(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_trim_stroke(ProjectGreaseGPHandle *handle, int index, int from, int to, int keep_single_point);
int project_grease_gp_trim_stroke_to_intersection(ProjectGreaseGPHandle *handle, int index);
int project_grease_gp_split_stroke(ProjectGreaseGPHandle *handle, int index, int before_index);
int project_grease_gp_history_reset(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_record(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_undo(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_redo(ProjectGreaseGPHandle *handle);
int project_grease_gp_history_can_undo(const ProjectGreaseGPHandle *handle);
int project_grease_gp_history_undo_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_history_can_redo(const ProjectGreaseGPHandle *handle);
int project_grease_gp_create_primitive(ProjectGreaseGPHandle *handle, int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, int material_index, float thickness);
int project_grease_gp_generate_primitive_preview(int type, float x0, float y0, float x1, float y1, float start_angle, float end_angle, int segments, float *xy, int capacity, int *count);
int project_grease_gp_create_polyline(ProjectGreaseGPHandle *handle, const ProjectGreaseGPPoint *points, int count, int material_index, float thickness, int cyclic);
int project_grease_gp_erase_at(ProjectGreaseGPHandle *handle, float x, float y, float radius);
int project_grease_gp_soft_erase_at(ProjectGreaseGPHandle *handle, float x, float y, float radius, float strength);
void project_grease_gp_set_brush_vertex_color(ProjectGreaseGPHandle *handle, int enabled, const float rgb_linear[3], float factor, int mode);
int project_grease_gp_active_layer(const ProjectGreaseGPHandle *handle);
int project_grease_gp_transform_selected(ProjectGreaseGPHandle *handle, const float m[9]);
int project_grease_gp_selected_bounds(const ProjectGreaseGPHandle *handle, float r_bounds[4]);
int project_grease_gp_lattice_begin(ProjectGreaseGPHandle *handle, float r_bounds[4]);
int project_grease_gp_lattice_update(ProjectGreaseGPHandle *handle, int nu, int nv, const float *ctrl, int interp);
void project_grease_gp_lattice_end(ProjectGreaseGPHandle *handle, int keep);
int project_grease_gp_erase(ProjectGreaseGPHandle *handle, float x, float y, float radius, int mode,
                            float draw_strength, float pressure, float era_strength, float era_thickness);
/* Eraser drag as Blender's modal GPENCIL_OT_draw (eraser): phase 0 begin / 1 move / 2 end / 3 cancel;
 * returns the WM status (RUNNING_MODAL 1, CANCELLED 2, FINISHED 4). */
int project_grease_gp_erase_modal(ProjectGreaseGPHandle *handle, int phase, int changed);
/* Last palette from GPENCIL_OT_extract_palette_vertex / material_to_vertex_color: count, and up to
 * capacity 0xRRGGBB colours in out. */
int project_grease_gp_last_palette(int *out, int capacity);
/* Blender's auto-keying before an edit on a held frame (the eraser and transform do it with
 * BKE_gpencil_frame_addcopy): every editable layer whose drawing at cfra is held from an earlier
 * key gets a copy of it as its own key at cfra, so the edit leaves the earlier frames alone.
 * Returns how many keys were added. */
int project_grease_gp_autokey_held(ProjectGreaseGPHandle *handle, int cfra);
/* Curve edit mode (pg_curve_*): on / off, curve points (13 floats each; returns the count), a click
 * on a point (part 0 handle 1, 1 point, 2 handle 2; layer -1 deselects), moving the selection. */
int project_grease_gp_curve_edit_set(ProjectGreaseGPHandle *handle, int on);
int project_grease_gp_curve_edit_on(const ProjectGreaseGPHandle *handle);
int project_grease_gp_curve_points(const ProjectGreaseGPHandle *handle, float *out, int capacity);
int project_grease_gp_curve_select(ProjectGreaseGPHandle *handle, int layer, int stroke, int point, int part, int extend);
int project_grease_gp_curve_translate(ProjectGreaseGPHandle *handle, float dx, float dy);
/* The fill tool's help lines on the current frame (pg_fill_help_lines): writes segments (5 floats)
 * then circles (4 floats) into out; returns the segment count, *r_circles the circle count. */
int project_grease_gp_fill_help_lines(ProjectGreaseGPHandle *handle, float extend_fac, int extend_mode, int collide,
                                      int layer_mode, float px_per_unit, float canvas_w, float canvas_h,
                                      float *out_seg, int max_seg, float *out_circ, int max_circ, int *r_circles);
void project_grease_gp_clear_selection(ProjectGreaseGPHandle *handle);
int project_grease_gp_lasso_select(ProjectGreaseGPHandle *handle, const float *xy, int count, int additive,
                                  float px_per_unit);
/* Blender operator by idname (project_grease_wm.h): returns the WM status (FINISHED 4 /
 * CANCELLED 2) in the low byte, the operator type flags (OPTYPE_UNDO 2) shifted by 8 and the
 * active material slot afterwards (actcol, 1-based; material operators change it) shifted by 16. */
int project_grease_gp_operator_call(ProjectGreaseGPHandle *handle, const char *idname, int cfra,
                                    int actcol, const float *props, int prop_count);
int project_grease_gp_apply_edit_command(ProjectGreaseGPHandle *handle,
                                         int command,
                                         const float *args,
                                         int arg_count);
int project_grease_gp_material_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_create_material(ProjectGreaseGPHandle *handle);
int project_grease_gp_set_material_colors(ProjectGreaseGPHandle *handle, int index, const float stroke_rgba[4], const float fill_rgba[4]);
int project_grease_gp_set_material_visibility(ProjectGreaseGPHandle *handle, int index, int visible);
int project_grease_gp_set_material_fill_enabled(ProjectGreaseGPHandle *handle, int index, int enabled);
int project_grease_gp_set_material_stroke_enabled(ProjectGreaseGPHandle *handle, int index, int enabled);
int project_grease_gp_smooth_stroke(ProjectGreaseGPHandle *handle, int index, float influence, int iterations);
int project_grease_gp_apply_blender_modifier(ProjectGreaseGPHandle *handle, int index, int modifier_type, float factor, int iterations);
int project_grease_gp_apply_blender_modifier_named(ProjectGreaseGPHandle *handle, int index, const char *name, float factor, int iterations);
typedef struct ProjectGreaseGPLegacyGeometryOp {
  int type;
  float value0;
  float value1;
  float value2;
  int int0;
  int int1;
  int flag0;
  int flag1;
} ProjectGreaseGPLegacyGeometryOp;

/* Batched direct calls into Blender 3.6.23 Legacy GP BKE geometry algorithms. */
int project_grease_gp_apply_legacy_geometry_batch(
    ProjectGreaseGPHandle *handle,
    int stroke_index,
    const ProjectGreaseGPLegacyGeometryOp *operations,
    int operation_count);
int project_grease_gp_apply_blender_modifier_stack(ProjectGreaseGPHandle *handle,
                                                   int index,
                                                   const int *modifier_types,
                                                   int modifier_count,
                                                   float factor,
                                                   int iterations);
int project_grease_gp_set_onion_skin(ProjectGreaseGPHandle *handle, int enabled, int before, int after, float opacity);
int project_grease_gp_set_multiframe_editing(ProjectGreaseGPHandle *handle, int enabled);
int project_grease_gp_fill_stroke(ProjectGreaseGPHandle *handle, int index);
void project_grease_gp_set_fill_screen_map(ProjectGreaseGPHandle *handle, float scale, float origin_x, float origin_y, float rotation);
/* Brush fill_factor (Precision). fill_at_screen: fill_leak <= 0 uses ceil(3 * fill_factor). */
void project_grease_gp_set_fill_factor(ProjectGreaseGPHandle *handle, float factor);
int project_grease_gp_fill_at_screen(ProjectGreaseGPHandle *handle,
                                     const float *rgba,
                                     int width,
                                     int height,
                                     int seed_x,
                                     int seed_y,
                                     int fill_leak,
                                     int dilate_pixels,
                                     int material_index,
                                     float thickness);
/* As fill_at_screen with the mask already at fill size (mask_w x mask_h, see
 * project_grease_gp_fill_render_size); seed in view pixels of width x height. */
int project_grease_gp_fill_at_mask(ProjectGreaseGPHandle *handle, const float *rgba, int mask_w, int mask_h,
                                   int width, int height, int seed_x, int seed_y, int fill_leak,
                                   int dilate_pixels, int material_index, float thickness);
/* Fill in two steps for a background thread: the outline from a fill-size mask (no document
 * change; returns an opaque job, NULL when there is no closed area) and its commit as a stroke on the
 * document thread. project_grease_gp_fill_job_free releases a job that is not committed. */
struct PGFillJob;
struct PGFillJob *project_grease_gp_fill_outline(ProjectGreaseGPHandle *handle, const float *rgba, int mask_w,
                                                 int mask_h, int width, int height, int seed_x, int seed_y,
                                                 int fill_leak, int dilate_pixels);
/* Worker-safe variant: the capture (document thread) takes a snapshot of everything the outline
 * needs; project_grease_gp_fill_outline_snapshot then never touches the handle or the document.
 * The job remembers the frame / layer / document of the snapshot and project_grease_gp_fill_commit
 * refuses (returns 0, document untouched) when they changed. Free the snapshot with _snapshot_free. */
struct PGFillSnapshot;
struct PGFillSnapshot *project_grease_gp_fill_snapshot(ProjectGreaseGPHandle *handle);
void project_grease_gp_fill_snapshot_free(struct PGFillSnapshot *snap);
struct PGFillJob *project_grease_gp_fill_outline_snapshot(const struct PGFillSnapshot *snap, const float *rgba,
                                                          int mask_w, int mask_h, int width, int height,
                                                          int seed_x, int seed_y, int fill_leak, int dilate_pixels);
int project_grease_gp_fill_commit(ProjectGreaseGPHandle *handle, struct PGFillJob *job, int material_index,
                                  float thickness);
void project_grease_gp_fill_job_free(struct PGFillJob *job);
/* Fill image size for a view dimension at the current fill factor. */
int project_grease_gp_fill_render_size(const ProjectGreaseGPHandle *handle, int view_size);
/* Save/load state (stroke style, layer state, palette); structs live in project_grease_document_state.h. */
int project_grease_gp_get_stroke_info(const ProjectGreaseGPHandle *handle, int stroke_index, PGStrokeInfo *out);
/* Vertex colors are passed separately: `colors` holds 4 floats (RGBA) per point and may be NULL
 * (no vertex color). */
int project_grease_gp_add_stroke(ProjectGreaseGPHandle *handle,
                                 const ProjectGreaseGPPoint *points,
                                 const float *colors,
                                 int count,
                                 const PGStrokeInfo *info);
int project_grease_gp_get_point_color(const ProjectGreaseGPHandle *handle,
                                      int stroke_index,
                                      int point_index,
                                      float out_rgba[4]);
int project_grease_gp_get_layer_info(const ProjectGreaseGPHandle *handle, int index, PGLayerInfo *out);
int project_grease_gp_set_layer_opacity(ProjectGreaseGPHandle *handle, int index, float opacity);
/* Vertex groups and point weights (see Backend::vertex_group_*). */
int project_grease_gp_vertex_group_count(const ProjectGreaseGPHandle *handle);
int project_grease_gp_vertex_group_name(const ProjectGreaseGPHandle *handle, int group, char *name, int name_capacity);
int project_grease_gp_vertex_group_add(ProjectGreaseGPHandle *handle, const char *name); /* index or -1 */
int project_grease_gp_vertex_group_remove(ProjectGreaseGPHandle *handle, int group);
int project_grease_gp_vertex_group_rename(ProjectGreaseGPHandle *handle, int group, const char *name);
int project_grease_gp_vertex_group_active(const ProjectGreaseGPHandle *handle);
int project_grease_gp_set_vertex_group_active(ProjectGreaseGPHandle *handle, int group);
int project_grease_gp_point_weight_count(const ProjectGreaseGPHandle *handle, int stroke_index, int point_index);
int project_grease_gp_point_weight_at(const ProjectGreaseGPHandle *handle, int stroke_index, int point_index, int k, int *group, float *weight);
int project_grease_gp_set_point_weight(ProjectGreaseGPHandle *handle, int stroke_index, int point_index, int group, float weight);

/* Layer masks (names refer to layers; flags: bit 0 hidden, bit 1 inverted). */
int project_grease_gp_layer_use_mask(const ProjectGreaseGPHandle *handle, int layer_index);
int project_grease_gp_set_layer_use_mask(ProjectGreaseGPHandle *handle, int layer_index, int enabled);
int project_grease_gp_mask_count(const ProjectGreaseGPHandle *handle, int layer_index);
int project_grease_gp_mask_add(ProjectGreaseGPHandle *handle, int layer_index, int mask_layer_index);
int project_grease_gp_mask_remove(ProjectGreaseGPHandle *handle, int layer_index, int mask_index);
int project_grease_gp_mask_get(const ProjectGreaseGPHandle *handle, int layer_index, int mask_index, char *name, int name_capacity, int *flags);
int project_grease_gp_mask_set_flags(ProjectGreaseGPHandle *handle, int layer_index, int mask_index, int flags);

/* Live per-layer modifier stack (project_grease_modifier_stack.h). */
int project_grease_gp_modifier_count(const ProjectGreaseGPHandle *handle, int layer_index);
int project_grease_gp_modifier_add(ProjectGreaseGPHandle *handle, int layer_index, int type); /* index or -1 */
int project_grease_gp_modifier_remove(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index);
int project_grease_gp_modifier_move(ProjectGreaseGPHandle *handle, int layer_index, int from_index, int to_index);
int project_grease_gp_modifier_set_enabled(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, int enabled);
int project_grease_gp_modifier_set_params(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, const float *params, int count);
/* Returns the parameter count or -1; type/enabled/params are filled when non-NULL. */
int project_grease_gp_modifier_get(const ProjectGreaseGPHandle *handle, int layer_index, int modifier_index, int *type, int *enabled, float *params, int capacity);
int project_grease_gp_modifier_apply(ProjectGreaseGPHandle *handle, int layer_index, int modifier_index);

/* Per-layer shader effects (project_grease_shader_fx.h): a 2D post-pass over the rendered layer. */
int project_grease_gp_fx_count(const ProjectGreaseGPHandle *handle, int layer_index);
int project_grease_gp_fx_add(ProjectGreaseGPHandle *handle, int layer_index, int type); /* index or -1 */
int project_grease_gp_fx_remove(ProjectGreaseGPHandle *handle, int layer_index, int fx_index);
int project_grease_gp_fx_move(ProjectGreaseGPHandle *handle, int layer_index, int from_index, int to_index);
int project_grease_gp_fx_set_enabled(ProjectGreaseGPHandle *handle, int layer_index, int fx_index, int enabled);
int project_grease_gp_fx_set_target(ProjectGreaseGPHandle *handle, int layer_index, int fx_index, int target);
int project_grease_gp_fx_target(const ProjectGreaseGPHandle *handle, int layer_index, int fx_index);
int project_grease_gp_fx_set_params(ProjectGreaseGPHandle *handle, int layer_index, int fx_index, const float *params, int count);
/* Returns the parameter count or -1; type/enabled/params are filled when non-NULL. */
int project_grease_gp_fx_get(const ProjectGreaseGPHandle *handle, int layer_index, int fx_index, int *type, int *enabled, float *params, int capacity);
int project_grease_gp_get_material_info(const ProjectGreaseGPHandle *handle, int index, PGMaterialInfo *out);
int project_grease_gp_get_point(const ProjectGreaseGPHandle *handle, int stroke_index, int point_index, ProjectGreaseGPPoint *out);
int project_grease_gp_set_point(ProjectGreaseGPHandle *handle, int stroke_index, int point_index, ProjectGreaseGPPoint point);

/* Annotations (project_grease_annotations.h) at the active frame. */
enum {
  PG_ANNOT_CMD_BEGIN = 0,      /* - */
  PG_ANNOT_CMD_ADD_POINT = 1,  /* x, y */
  PG_ANNOT_CMD_END = 2,        /* - */
  PG_ANNOT_CMD_CANCEL = 3,     /* - */
  PG_ANNOT_CMD_ERASE = 4,      /* x, y, radius */
  PG_ANNOT_CMD_CLEAR = 5,      /* - */
  PG_ANNOT_CMD_SET_STYLE = 6,  /* r, g, b, a, thickness_px */
  PG_ANNOT_CMD_SET_VISIBLE = 7,/* visible */
  PG_ANNOT_CMD_COUNT = 8,      /* -: returns the stroke count */
};
int project_grease_gp_annotation_command(ProjectGreaseGPHandle *handle, int command, const float *args, int arg_count);
/* r, g, b, a, thickness, visible. */
int project_grease_gp_annotation_style(const ProjectGreaseGPHandle *handle, float out[6]);
int project_grease_gp_annotation_dump(const ProjectGreaseGPHandle *handle, float *out, int capacity);
int project_grease_gp_annotation_load(ProjectGreaseGPHandle *handle, const float *data, int count);

/* Rig (project_grease_armature.h; project_grease_armature_bridge.cpp). Points and distances are
 * canvas px; angles radians counter-clockwise on screen; bones are indices (creation order).
 * command returns the operator's result (count / new bone index) or -1 when there is no rig. */
enum {
  PG_ARM_CMD_CREATE = 0,          /* - */
  PG_ARM_CMD_REMOVE = 1,          /* - */
  PG_ARM_CMD_SET_MODE = 2,        /* mode: 0 object, 1 edit, 2 pose */
  PG_ARM_CMD_BONE_ADD = 3,        /* head x, y, tail x, y; name */
  PG_ARM_CMD_EXTRUDE = 4,         /* dx, dy of the new tips */
  PG_ARM_CMD_EDIT_SELECT = 5,     /* bone; flags (BONE_SELECTED 1 / ROOTSEL 2 / TIPSEL 4), extend */
  PG_ARM_CMD_EDIT_TRANSLATE = 6,  /* dx, dy */
  PG_ARM_CMD_EDIT_ROTATE = 7,     /* angle, pivot x, y */
  PG_ARM_CMD_EDIT_SCALE = 8,      /* factor, pivot x, y */
  PG_ARM_CMD_BONE_SET = 9,        /* bone; head x, y, tail x, y, roll */
  PG_ARM_CMD_PARENT_SET = 10,     /* mode 1 connected / 2 keep offset (active bone is the parent) */
  PG_ARM_CMD_PARENT_CLEAR = 11,   /* mode 1 clear / 2 disconnect */
  PG_ARM_CMD_DELETE = 12,         /* - (selected bones) */
  PG_ARM_CMD_RENAME = 13,         /* bone; name */
  PG_ARM_CMD_ENVELOPE = 14,       /* bone; dist px, weight, rad head px, rad tail px, deform, mult vg */
  PG_ARM_CMD_POSE_SELECT = 15,    /* bone (-1 none); extend */
  PG_ARM_CMD_POSE_TRANSLATE = 16, /* dx, dy */
  PG_ARM_CMD_POSE_ROTATE = 17,    /* angle[, pivot: 3 median point (default) / 2 individual origins] */
  PG_ARM_CMD_POSE_SCALE = 18,     /* factor[, pivot as rotate] */
  PG_ARM_CMD_POSE_SET = 19,       /* bone; loc[3], quat[4], scale[3] (bone space, Blender units) */
  PG_ARM_CMD_POSE_CLEAR = 20,     /* what: 1 loc, 2 rot, 4 scale */
  PG_ARM_CMD_POSE_COPY = 21,      /* - */
  PG_ARM_CMD_POSE_PASTE = 22,     /* flipped, selected only */
  PG_ARM_CMD_KEY_INSERT = 23,     /* frame (LocRotScale on the selected bones) */
  PG_ARM_CMD_KEY_DELETE = 24,     /* bone; frame */
  PG_ARM_CMD_KEY_INTERPOLATION = 25, /* bone; frame, ipo (0 constant, 1 linear, 2 bezier) */
  PG_ARM_CMD_BIND = 26,           /* mode 0 armature, 1 empty groups, 2 automatic, 3 envelope */
  PG_ARM_CMD_EVALUATE = 27,       /* frame */
  PG_ARM_CMD_DISPLAY = 28,        /* drawtype (ARM_OCTA 0 / ARM_LINE 1 ...), show in front */
  PG_ARM_CMD_KEY_MOVE = 30,       /* bone; frame, to (dope sheet key move) */
  PG_ARM_CMD_MIRROR_X = 31,       /* X-Axis Mirror on (1) / off (0) */
  PG_ARM_CMD_PICK = 29,           /* x, y, dist, extend[, canvas px -> window 4x4 (16, column major)];
                                     no matrix: x, y, dist in canvas px. Returns the bone or -1 */
};
enum {
  PG_ARM_Q_SUMMARY = 0,  /* bone count (-1 no rig), active bone, mode, F-Curve count */
  PG_ARM_Q_BONE = 1,     /* PG_ARM_BONE_INFO floats, positions in canvas px */
  PG_ARM_Q_POSE = 2,     /* loc[3], quat[4], scale[3] */
  PG_ARM_Q_KEYS = 3,     /* sorted key frames of the bone; returns the total */
  PG_ARM_Q_KEY_INTERPOLATION = 4, /* ipo of the bone's key at frame x, -1 none */
  PG_ARM_Q_ENVELOPE = 5, /* dist px, weight, rad head px, rad tail px */
  PG_ARM_Q_SELECTED = 6, /* 1 / 0 */
  PG_ARM_Q_DISPLAY = 7,  /* drawtype, show in front, X-Axis Mirror */
};
/* Constraints, custom properties and drivers (project_grease_constraint.h / project_grease_driver.h,
 * project_grease_rig_bridge.cpp). Strings go in `name`; constraint / property / driver names, paths
 * and expressions are read from the rig JSON. Each command re-evaluates the drivers and the pose at
 * the scene frame. */
enum {
  PG_ARM_CMD_CON_ADD = 40,          /* bone; type (CONSTRAINT_TYPE_*); name. Returns the index */
  PG_ARM_CMD_CON_REMOVE = 41,       /* bone; index */
  PG_ARM_CMD_CON_MOVE = 42,         /* bone; index, to */
  PG_ARM_CMD_CON_TARGET = 43,       /* bone; index, which (0 target / 1 pole), clear; name = bone
                                       ("" the rig object) */
  PG_ARM_CMD_CON_SET = 44,          /* bone; index, then pg_arm_con_set values */
  PG_ARM_CMD_CON_CHILDOF_INVERSE = 45, /* bone; index, set (1) / clear (0) */
  PG_ARM_CMD_PROP_ADD = 46,         /* bone (-1 object); value, min, max, default; name. Index */
  PG_ARM_CMD_PROP_REMOVE = 47,      /* index */
  PG_ARM_CMD_PROP_SET = 48,         /* index, value */
  PG_ARM_CMD_DRIVER_ADD = 49,       /* array index, type (DRIVER_TYPE_*), generator; name = path */
  PG_ARM_CMD_DRIVER_REMOVE = 50,    /* driver */
  PG_ARM_CMD_DRIVER_TYPE = 51,      /* driver, type */
  PG_ARM_CMD_DRIVER_EXPRESSION = 52, /* driver; name = expression */
  PG_ARM_CMD_DRIVER_VAR_ADD = 53,   /* driver, type (DVAR_TYPE_SINGLE_PROP 0 / TRANSFORM_CHAN 3);
                                       name. Returns the variable index */
  PG_ARM_CMD_DRIVER_VAR_REMOVE = 54, /* driver, var */
  PG_ARM_CMD_DRIVER_VAR_RENAME = 55, /* driver, var; name */
  PG_ARM_CMD_DRIVER_VAR_PATH = 56,  /* driver, var; name = RNA path (Single Property) */
  PG_ARM_CMD_DRIVER_VAR_TRANSFORM = 57, /* driver, var, channel, rotation mode, space flags; name =
                                           bone (Transform Channel) */
  PG_ARM_CMD_DRIVER_KEYS = 58,      /* driver, extend, then (x, y, ipo) triples */
  PG_ARM_CMD_DRIVER_GENERATOR = 59, /* driver, mode, additive, coefficients (none: removes) */
};
enum {
  PG_ARM_Q_CONSTRAINTS = 20, /* bone: constraint count */
  PG_ARM_Q_CONSTRAINT = 21,  /* bone, x = index: pg_arm_con_get values */
  PG_ARM_Q_PROPS = 22,       /* property count */
  PG_ARM_Q_PROP = 23,        /* x = index: bone, value, min, max, default */
  PG_ARM_Q_DRIVERS = 24,     /* driver count */
  PG_ARM_Q_DRIVER = 25,      /* x = index: pg_arm_driver_get values */
};
int project_grease_gp_armature_command(ProjectGreaseGPHandle *handle, int command, int bone,
                                       const float *args, int arg_count, const char *name);
/* Fills out (capacity floats) and returns the number of values (may exceed capacity: nothing written). */
int project_grease_gp_armature_query(const ProjectGreaseGPHandle *handle, int what, int bone, float x,
                                     float *out, int capacity);
/* Bone name into name (capacity bytes); 0 when out of range. */
int project_grease_gp_armature_bone_name(const ProjectGreaseGPHandle *handle, int bone, char *name, int capacity);
/* Project document JSON of the rig ("" none): returns the length; copies when capacity > length. */
int project_grease_gp_armature_json(const ProjectGreaseGPHandle *handle, char *out, int capacity);
int project_grease_gp_armature_load_json(ProjectGreaseGPHandle *handle, const char *json);

const char *project_grease_gp_last_error(
    const ProjectGreaseGPHandle *handle);

#ifdef __cplusplus
}
#endif
