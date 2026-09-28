#include <jni.h>

#include <cstdint>

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
