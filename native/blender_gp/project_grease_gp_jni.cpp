#include <jni.h>

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
    JNIEnv *,
    jobject,
    jlong handle,
    jfloat x,
    jfloat y,
    jfloat z,
    jfloat pressure,
    jfloat strength,
    jfloat time)
{
  const ProjectGreaseGPPoint point{
      x, y, z, pressure, strength, time};
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
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreateLayer(JNIEnv *env, jobject, jlong handle, jstring name)
{
  const char *chars = env->GetStringUTFChars(name, nullptr);
  const int result = project_grease_gp_create_layer(from_handle(handle), chars);
  env->ReleaseStringUTFChars(name, chars);
  return result != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSelectLayer(JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_select_layer(from_handle(handle), index) != 0;
}
extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeLayerCount(JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_layer_count(from_handle(handle));
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetLayerVisibility(JNIEnv *, jobject, jlong handle, jint index, jboolean visible)
{ return project_grease_gp_set_layer_visibility(from_handle(handle), index, visible ? 1 : 0) != 0; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetLayerLocked(JNIEnv *, jobject, jlong handle, jint index, jboolean locked)
{ return project_grease_gp_set_layer_locked(from_handle(handle), index, locked ? 1 : 0) != 0; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeMoveLayer(JNIEnv *, jobject, jlong handle, jint from_index, jint to_index)
{ return project_grease_gp_move_layer(from_handle(handle), from_index, to_index) != 0; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDuplicateLayer(JNIEnv *, jobject, jlong handle, jint index)
{ return project_grease_gp_duplicate_layer(from_handle(handle), index) != 0; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDeleteLayer(JNIEnv *, jobject, jlong handle, jint index)
{ return project_grease_gp_delete_layer(from_handle(handle), index) != 0; }
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRenameLayer(JNIEnv *env, jobject, jlong handle, jint index, jstring name)
{
  const char *chars = env->GetStringUTFChars(name, nullptr);
  const int result = project_grease_gp_rename_layer(from_handle(handle), index, chars);
  env->ReleaseStringUTFChars(name, chars);
  return result != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreateFrame(JNIEnv *, jobject, jlong handle, jint frame_number)
{
  return project_grease_gp_create_frame(from_handle(handle), frame_number) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSelectFrame(JNIEnv *, jobject, jlong handle, jint frame_number)
{
  return project_grease_gp_select_frame(from_handle(handle), frame_number) != 0;
}
extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeFrameCount(JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_frame_count(from_handle(handle));
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDuplicateFrame(JNIEnv *, jobject, jlong handle, jint source_frame, jint target_frame)
{
  return project_grease_gp_duplicate_frame(from_handle(handle), source_frame, target_frame) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDeleteFrame(JNIEnv *, jobject, jlong handle, jint frame_number)
{
  return project_grease_gp_delete_frame(from_handle(handle), frame_number) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSelectStroke(JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_select_stroke(from_handle(handle), index) != 0;
}
extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeHitTestStroke(
    JNIEnv *, jobject, jlong handle, jfloat x, jfloat y, jfloat radius)
{
  return project_grease_gp_hit_test_stroke(
      from_handle(handle), x, y, radius);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDeleteStroke(JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_delete_stroke(from_handle(handle), index) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDeleteLastStroke(JNIEnv *, jobject, jlong handle)
{
  return project_grease_gp_delete_last_stroke(from_handle(handle)) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDuplicateStroke(JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_duplicate_stroke(from_handle(handle), index) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeTranslateStroke(JNIEnv *, jobject, jlong handle, jint index, jfloat dx, jfloat dy, jfloat dz)
{
  return project_grease_gp_translate_stroke(from_handle(handle), index, dx, dy, dz) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeFlipStroke(JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_flip_stroke(from_handle(handle), index) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRotateStroke(
    JNIEnv *, jobject, jlong handle, jint index, jfloat radians)
{
  return project_grease_gp_rotate_stroke(from_handle(handle), index, radians) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeScaleStroke(
    JNIEnv *, jobject, jlong handle, jint index, jfloat scale_x, jfloat scale_y)
{
  return project_grease_gp_scale_stroke(from_handle(handle), index, scale_x, scale_y) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeMirrorStroke(
    JNIEnv *, jobject, jlong handle, jint index, jboolean mirror_x, jboolean mirror_y)
{
  return project_grease_gp_mirror_stroke(
             from_handle(handle), index, mirror_x ? 1 : 0, mirror_y ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSubdivideStroke(JNIEnv *, jobject, jlong handle, jint index, jint level)
{
  return project_grease_gp_subdivide_stroke(from_handle(handle), index, level) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCloseStroke(JNIEnv *, jobject, jlong handle, jint index)
{
  return project_grease_gp_close_stroke(from_handle(handle), index) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeTrimStroke(JNIEnv *, jobject, jlong handle, jint index, jint from, jint to, jboolean keep_single_point)
{
  return project_grease_gp_trim_stroke(from_handle(handle), index, from, to, keep_single_point ? 1 : 0) != 0;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSplitStroke(JNIEnv *, jobject, jlong handle, jint index, jint before_index)
{
  return project_grease_gp_split_stroke(from_handle(handle), index, before_index) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreatePrimitive(
    JNIEnv *, jobject, jlong handle, jint type, jfloat x0, jfloat y0, jfloat x1, jfloat y1,
    jfloat startAngle, jfloat endAngle, jint segments, jint materialIndex, jfloat thickness)
{
  return project_grease_gp_create_primitive(from_handle(handle), type, x0, y0, x1, y1,
                                             startAngle, endAngle, segments,
                                             materialIndex, thickness) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreatePolyline(
    JNIEnv *env, jobject, jlong handle, jfloatArray values, jint count,
    jint materialIndex, jfloat thickness, jboolean cyclic)
{
  if (!values || count < 2 || env->GetArrayLength(values) < count * 2) return JNI_FALSE;
  std::vector<jfloat> raw(static_cast<size_t>(count) * 2u);
  env->GetFloatArrayRegion(values, 0, count * 2, raw.data());
  std::vector<ProjectGreaseGPPoint> points(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    points[i] = {raw[i * 2], raw[i * 2 + 1], 0.0f, 1.0f, 1.0f, static_cast<float>(i)};
  }
  return project_grease_gp_create_polyline(from_handle(handle), points.data(), count,
                                            materialIndex, thickness, cyclic ? 1 : 0) != 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeEraseAt(
    JNIEnv *, jobject, jlong handle, jfloat x, jfloat y, jfloat radius)
{
  return project_grease_gp_erase_at(from_handle(handle), x, y, radius) != 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeClearSelection(
    JNIEnv *, jobject, jlong handle)
{
  project_grease_gp_clear_selection(from_handle(handle));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeLassoSelect(
    JNIEnv *env, jobject, jlong handle, jfloatArray values, jint count, jboolean additive)
{
  if (!values || count < 3 || env->GetArrayLength(values) < count * 2) return 0;
  std::vector<jfloat> raw(static_cast<size_t>(count) * 2u);
  env->GetFloatArrayRegion(values, 0, count * 2, raw.data());
  return project_grease_gp_lasso_select(from_handle(handle), raw.data(), count, additive ? 1 : 0);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGetPoint(JNIEnv *env, jobject, jlong handle, jint stroke_index, jint point_index)
{
  ProjectGreaseGPPoint point{};
  if (!project_grease_gp_get_point(from_handle(handle), stroke_index, point_index, &point)) return nullptr;
  const jfloat values[] = {point.x, point.y, point.z, point.pressure, point.strength, point.time};
  jfloatArray result = env->NewFloatArray(6);
  if (!result) return nullptr;
  env->SetFloatArrayRegion(result, 0, 6, values);
  return result;
}
