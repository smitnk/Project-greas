// SPDX-License-Identifier: GPL-2.0-or-later
// JNI for .blend open / save (project_grease_blend_io.h): bytes <-> the project document JSON,
// which the app then loads / saves like its own files. Stateless: no document handle needed.
#include <jni.h>

#include <cstdlib>

#include "project_grease_blend_io.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeBlendToProjectJson(JNIEnv *env, jobject, jbyteArray data)
{
  if (!data) return nullptr;
  const jsize n = env->GetArrayLength(data);
  jbyte *bytes = env->GetByteArrayElements(data, nullptr);
  if (!bytes) return nullptr;
  char err[256] = "";
  char *json = pg_blend_to_project_json(bytes, (size_t)n, err, sizeof(err));
  env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
  if (!json) return nullptr;
  jstring out = env->NewStringUTF(json);
  std::free(json);
  return out;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeProjectJsonToBlend(JNIEnv *env, jobject, jstring json)
{
  if (!json) return nullptr;
  const char *text = env->GetStringUTFChars(json, nullptr);
  if (!text) return nullptr;
  char err[256] = "";
  size_t n = 0;
  void *blend = pg_project_json_to_blend(text, &n, err, sizeof(err));
  env->ReleaseStringUTFChars(json, text);
  if (!blend) return nullptr;
  jbyteArray out = env->NewByteArray((jsize)n);
  if (out) env->SetByteArrayRegion(out, 0, (jsize)n, static_cast<const jbyte *>(blend));
  std::free(blend);
  return out;
}
