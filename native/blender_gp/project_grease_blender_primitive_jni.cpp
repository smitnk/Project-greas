// SPDX-License-Identifier: GPL-2.0-or-later
//
// JNI entry for Blender 3.6.23 Legacy GP primitive geometry
// (project_grease_blender_primitive.c). Stateless: no GP or renderer handle.

#include <jni.h>

#include <vector>

#include "project_grease_blender_primitive.h"

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGenerateBlenderPrimitive(
    JNIEnv *env, jobject, jint type, jfloatArray anchors_xy, jint edges, jboolean flip)
{
  if (!anchors_xy) {
    return nullptr;
  }
  const jsize length = env->GetArrayLength(anchors_xy);
  if (length < 4 || (length % 2) != 0) {
    return nullptr;
  }
  std::vector<jfloat> anchors(static_cast<size_t>(length));
  env->GetFloatArrayRegion(anchors_xy, 0, length, anchors.data());

  const int anchor_count = static_cast<int>(length / 2);
  const int count = project_grease_blender_primitive_point_count(type, anchor_count, edges);
  if (count <= 0) {
    return nullptr;
  }
  std::vector<float> xy(static_cast<size_t>(count) * 2u);
  const int written = project_grease_blender_primitive_generate(
      type, anchors.data(), anchor_count, edges, flip ? 1 : 0, xy.data(), count);
  if (written != count) {
    return nullptr;
  }
  jfloatArray result = env->NewFloatArray(count * 2);
  if (!result) {
    return nullptr;
  }
  env->SetFloatArrayRegion(result, 0, count * 2, xy.data());
  return result;
}
