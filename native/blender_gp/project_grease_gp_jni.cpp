#include <jni.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "project_grease_gp_bridge.h"

namespace {

ProjectGreaseGPHandle *from_handle(jlong value)
{
  return reinterpret_cast<ProjectGreaseGPHandle *>(
      static_cast<uintptr_t>(value));
}

jlong to_handle(ProjectGreaseGPHandle *handle)
{
  return static_cast<jlong>(
      reinterpret_cast<uintptr_t>(handle));
}

}  // namespace

// GPNative is a Kotlin object, so these external functions are instance
// methods and receive a jobject receiver (not a jclass receiver).
extern "C" JNIEXPORT jlong JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreate(
    JNIEnv *, jobject)
{
  return to_handle(project_grease_gp_create());
}

extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDestroy(
    JNIEnv *, jobject, jlong handle)
{
  project_grease_gp_destroy(from_handle(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeBeginStroke(
    JNIEnv *, jobject, jlong handle, jint material_index, jfloat thickness)
{
  return project_grease_gp_begin_stroke(
             from_handle(handle), material_index, thickness) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeAddPoint(
    JNIEnv *, jobject, jlong handle, jfloat x, jfloat y, jfloat z,
    jfloat pressure, jfloat strength, jfloat time)
{
  const ProjectGreaseGPPoint point{x, y, z, pressure, strength, time};
  return project_grease_gp_add_point(from_handle(handle), point) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeEndStroke(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_end_stroke(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRender(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_render(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeStrokeCount(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_stroke_count(from_handle(handle));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativePointCount(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_point_count(from_handle(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreateLayer(
    JNIEnv *env, jobject, jlong handle, jstring name)
{
  const char *chars = env->GetStringUTFChars(name, nullptr);
  if (!chars) return JNI_FALSE;
  const int result = project_grease_gp_create_layer(from_handle(handle), chars);
  env->ReleaseStringUTFChars(name, chars);
  return result != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSelectLayer(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_select_layer(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeLayerCount(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_layer_count(from_handle(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetLayerVisibility(
    JNIEnv *, jobject, jlong handle, jint index, jboolean visible)
{
  return project_grease_gp_set_layer_visibility(
             from_handle(handle), index, visible ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetLayerLocked(
    JNIEnv *, jobject, jlong handle, jint index, jboolean locked)
{
  return project_grease_gp_set_layer_locked(
             from_handle(handle), index, locked ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeMoveLayer(
    JNIEnv *, jobject, jlong handle, jint from_index, jint to_index)
{
  return project_grease_gp_move_layer(
             from_handle(handle), from_index, to_index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDuplicateLayer(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_duplicate_layer(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge.GPNative_nativeDeleteLayer(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_delete_layer(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRenameLayer(
    JNIEnv *env, jobject, jlong handle, jint index, jstring name)
{
  const char *chars = env->GetStringUTFChars(name, nullptr);
  if (!chars) return JNI_FALSE;
  const int result = project_grease_gp_rename_layer(from_handle(handle), index, chars);
  env->ReleaseStringUTFChars(name, chars);
  return result != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreateFrame(
    JNIEnv *, jobject, jlong handle, jint frame_number)
{
  return project_grease_gp_create_frame(from_handle(handle), frame_number) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge.GPNative_nativeSelectFrame(
    JNIEnv *, jobject, jlong handle, jint frame_number)
{
  return project_grease_gp_select_frame(from_handle(handle), frame_number) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge.GPNative_nativeSelectFrameOrHold(
    JNIEnv *, jobject, jlong handle, jint frame_number)
{
  return project_grease_gp_select_frame_or_hold(from_handle(handle), frame_number) != 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge.GPNative_nativeFrameCount(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_frame_count(from_handle(handle));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge.GPNative_nativeFrameEnd(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_frame_end(from_handle(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk.projectgrease.nativebridge.GPNative_nativeDuplicateFrame(
    JNIEnv *, jobject, jlong handle, jint source_frame, jint target_frame)
{
  return project_grease_gp_duplicate_frame(
             from_handle(handle), source_frame, target_frame) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeDeleteFrame(
    JNIEnv *, jobject, jlong handle, jint frame_number)
{
  return project_grease_gp_delete_frame(from_handle(handle), frame_number) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSelectStroke(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_select_stroke(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeHitTestStroke(
    JNIEnv *, jobject, jlong handle, jfloat x, jfloat y, jfloat radius)
{
  return project_grease_gp_hit_test_stroke(from_handle(handle), x, y, radius);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeStrokeCenter(
    JNIEnv *env, jobject, jlong handle, jint index)
{
  float x = 0.0f, y = 0.0f;
  if (!project_grease_gp_stroke_center(from_handle(handle), index, &x, &y)) {
    return nullptr;
  }
  const jfloat values[] = {x, y};
  jfloatArray result = env->NewFloatArray(2);
  if (!result) return nullptr;
  env->SetFloatArrayRegion(result, 0, 2, values);
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeDeleteStroke(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_delete_stroke(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeDeleteLastStroke(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_delete_last_stroke(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeDuplicateStroke(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_duplicate_stroke(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeTranslateStroke(
    JNIEnv *, jobject, jlong handle, jint index, jfloat dx, jfloat dy, jfloat dz)
{
  return project_grease_gp_translate_stroke(
             from_handle(handle), index, dx, dy, dz) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeFlipStroke(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_flip_stroke(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeRotateStroke(
    JNIEnv *, jobject, jlong handle, jint index, jfloat radians)
{
  return project_grease_gp_rotate_stroke(from_handle(handle), index, radians) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeRotateStrokeAbout(
    JNIEnv *, jobject, jlong handle, jint index, jfloat radians,
    jfloat center_x, jfloat center_y)
{
  return project_grease_gp_rotate_stroke_about(
      from_handle(handle), index, radians, center_x, center_y) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeScaleStroke(
    JNIEnv *, jobject, jlong handle, jint index, jfloat scale_x, jfloat scale_y)
{
  return project_grease_gp_scale_stroke(
             from_handle(handle), index, scale_x, scale_y) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeScaleStrokeAbout(
    JNIEnv *, jobject, jlong handle, jint index, jfloat scale_x, jfloat scale_y,
    jfloat center_x, jfloat center_y)
{
  return project_grease_gp_scale_stroke_about(
      from_handle(handle), index, scale_x, scale_y, center_x, center_y) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeMirrorStroke(
    JNIEnv *, jobject, jlong handle, jint index, jboolean mirror_x, jboolean mirror_y)
{
  return project_grease_gp_mirror_stroke(
             from_handle(handle), index, mirror_x ? 1 : 0, mirror_y ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeMirrorStrokeAbout(
    JNIEnv *, jobject, jlong handle, jint index, jboolean mirror_x, jboolean mirror_y,
    jfloat center_x, jfloat center_y)
{
  return project_grease_gp_mirror_stroke_about(
      from_handle(handle), index, mirror_x ? 1 : 0, mirror_y ? 1 : 0,
      center_x, center_y) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSubdivideStroke(
    JNIEnv *, jobject, jlong handle, jint index, jint level)
{
  return project_grease_gp_subdivide_stroke(from_handle(handle), index, level) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeCloseStroke(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_close_stroke(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeTrimStroke(
    JNIEnv *, jobject, jlong handle, jint index, jint from, jint to, jboolean keep_single_point)
{
  return project_grease_gp_trim_stroke(
             from_handle(handle), index, from, to, keep_single_point ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeTrimStrokeToIntersection(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_trim_stroke_to_intersection(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSplitStroke(
    JNIEnv *, jobject, jlong handle, jint index, jint before_index)
{
  return project_grease_gp_split_stroke(
             from_handle(handle), index, before_index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeCreatePrimitive(
    JNIEnv *, jobject, jlong handle, jint type,
    jfloat x0, jfloat y0, jfloat x1, jfloat y1,
    jfloat start_angle, jfloat end_angle, jint segments,
    jint material_index, jfloat thickness)
{
  return project_grease_gp_create_primitive(
      from_handle(handle), type, x0, y0, x1, y1,
      start_angle, end_angle, segments, material_index, thickness) != 0;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeGeneratePrimitivePreview(
    JNIEnv *env, jobject, jint type,
    jfloat x0, jfloat y0, jfloat x1, jfloat y1,
    jfloat start_angle, jfloat end_angle, jint segments)
{
  const int capacity = std::max(2, segments * 4 + 16);
  std::vector<float> xy(static_cast<size_t>(capacity) * 2u);
  int count = 0;
  if (!project_grease_gp_generate_primitive_preview(
          type, x0, y0, x1, y1, start_angle, end_angle, segments,
          xy.data(), capacity, &count)) {
    return nullptr;
  }

  jfloatArray result = env->NewFloatArray(count * 2);
  if (!result) return nullptr;
  env->SetFloatArrayRegion(result, 0, count * 2, xy.data());
  return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeCreatePolyline(
    JNIEnv *env, jobject, jlong handle, jfloatArray points_xy, jint count,
    jint material_index, jfloat thickness, jboolean cyclic)
{
  if (!points_xy || count < 2 || env->GetArrayLength(points_xy) < count * 2) {
    return JNI_FALSE;
  }

  std::vector<jfloat> xy(static_cast<size_t>(count) * 2u);
  env->GetFloatArrayRegion(points_xy, 0, count * 2, xy.data());

  std::vector<ProjectGreaseGPPoint> points(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    points[static_cast<size_t>(i)] = {
        xy[static_cast<size_t>(i) * 2u],
        xy[static_cast<size_t>(i) * 2u + 1u],
        0.0f, 1.0f, 1.0f, 0.0f};
  }

  return project_grease_gp_create_polyline(
             from_handle(handle), points.data(), count,
             material_index, thickness, cyclic ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk_projectgrease.nativebridge.GPNative_nativeEraseAt(
    JNIEnv *, jobject, jlong handle, jfloat x, jfloat y, jfloat radius)
{
  return project_grease_gp_erase_at(from_handle(handle), x, y, radius) != 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeClearSelection(
    JNIEnv *, jobject, jlong handle)
{
  project_grease_gp_clear_selection(from_handle(handle));
}

extern "C" JNIEXPORT jint JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeLassoSelect(
    JNIEnv *env, jobject, jlong handle, jfloatArray points_xy, jint count,
    jboolean additive)
{
  if (!points_xy || count < 3 || env->GetArrayLength(points_xy) < count * 2) {
    return 0;
  }
  std::vector<jfloat> xy(static_cast<size_t>(count) * 2u);
  env->GetFloatArrayRegion(points_xy, 0, count * 2, xy.data());
  return project_grease_gp_lasso_select(
      from_handle(handle), xy.data(), count, additive ? 1 : 0);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeGetPoint(
    JNIEnv *env, jobject, jlong handle, jint stroke_index, jint point_index)
{
  ProjectGreaseGPPoint point{};
  if (!project_grease_gp_get_point(
          from_handle(handle), stroke_index, point_index, &point)) {
    return nullptr;
  }

  const jfloat values[] = {
      point.x, point.y, point.z,
      point.pressure, point.strength, point.time};
  jfloatArray result = env->NewFloatArray(6);
  if (!result) return nullptr;
  env->SetFloatArrayRegion(result, 0, 6, values);
  return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeMaterialCount(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_material_count(from_handle(handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeCreateMaterial(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_create_material(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSetMaterialColors(
    JNIEnv *env, jobject, jlong handle, jint index,
    jfloatArray stroke, jfloatArray fill)
{
  if (!stroke || !fill ||
      env->GetArrayLength(stroke) < 4 ||
      env->GetArrayLength(fill) < 4) {
    return JNI_FALSE;
  }

  jfloat stroke_rgba[4];
  jfloat fill_rgba[4];
  env->GetFloatArrayRegion(stroke, 0, 4, stroke_rgba);
  env->GetFloatArrayRegion(fill, 0, 4, fill_rgba);

  return project_grease_gp_set_material_colors(
             from_handle(handle), index, stroke_rgba, fill_rgba) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSetMaterialVisibility(
    JNIEnv *, jobject, jlong handle, jint index, jboolean visible)
{
  return project_grease_gp_set_material_visibility(
             from_handle(handle), index, visible ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSetMaterialFillEnabled(
    JNIEnv *, jobject, jlong handle, jint index, jboolean enabled)
{
  return project_grease_gp_set_material_fill_enabled(
             from_handle(handle), index, enabled ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSmoothStroke(
    JNIEnv *, jobject, jlong handle, jint index, jfloat influence, jint iterations)
{
  return project_grease_gp_smooth_stroke(
             from_handle(handle), index, influence, iterations) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSetOnionSkin(
    JNIEnv *, jobject, jlong handle, jboolean enabled,
    jint before, jint after, jfloat opacity)
{
  return project_grease_gp_set_onion_skin(
             from_handle(handle), enabled ? 1 : 0, before, after, opacity) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeSetMultiframeEditing(
    JNIEnv *, jobject, jlong handle, jboolean enabled)
{
  return project_grease_gp_set_multiframe_editing(
             from_handle(handle), enabled ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeFillStroke(
    JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_fill_stroke(from_handle(handle), index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeHistoryReset(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_history_reset(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeHistoryRecord(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_history_record(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeHistoryUndo(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_history_undo(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeHistoryRedo(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_history_redo(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeHistoryCanUndo(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_history_can_undo(from_handle(handle)) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com.smitnk.projectgrease.nativebridge.GPNative_nativeHistoryCanRedo(
    JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_history_can_redo(from_handle(handle)) != 0;
}
