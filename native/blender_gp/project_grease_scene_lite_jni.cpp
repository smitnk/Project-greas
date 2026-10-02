// SPDX-License-Identifier: GPL-2.0-or-later
//
// JNI for Scene-lite (project_grease_scene_lite.h): the 3D reference scene Line Art will read.
// Its own handle, independent of the GP document and renderer.

#include <jni.h>

#include <cstdint>
#include <vector>

#include "project_grease_lineart_lite.h"
#include "project_grease_scene_lite.h"

namespace {
PGSceneLite *scene_from(jlong value) { return reinterpret_cast<PGSceneLite *>(static_cast<uintptr_t>(value)); }

jfloatArray to_java(JNIEnv *env, const std::vector<float> &v)
{
  jfloatArray result = env->NewFloatArray(static_cast<jsize>(v.size()));
  if (result && !v.empty()) env->SetFloatArrayRegion(result, 0, static_cast<jsize>(v.size()), v.data());
  return result;
}
}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteCreate(JNIEnv *, jobject)
{
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(pg_lite_scene_create()));
}

extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteFree(JNIEnv *, jobject, jlong handle)
{
  pg_lite_scene_free(scene_from(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteClear(JNIEnv *, jobject, jlong handle)
{
  pg_lite_scene_clear(scene_from(handle));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLoadObj(JNIEnv *env, jobject, jlong handle, jbyteArray text)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene || !text) return 0;
  const jsize n = env->GetArrayLength(text);
  std::vector<char> bytes(static_cast<size_t>(n));
  if (n > 0) env->GetByteArrayRegion(text, 0, n, reinterpret_cast<jbyte *>(bytes.data()));
  return pg_lite_load_obj(scene, bytes.data(), static_cast<int>(n));
}

/* objects, vertices, triangles, edges, loose edges */
extern "C" JNIEXPORT jintArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteStats(JNIEnv *env, jobject, jlong handle)
{
  int counts[5];
  pg_lite_stats(scene_from(handle), counts);
  jintArray result = env->NewIntArray(5);
  if (result) env->SetIntArrayRegion(result, 0, 5, counts);
  return result;
}

/* type, lens, ortho_scale, sensor_x, sensor_y, sensor_fit, shift_x, shift_y, clip_start,
 * clip_end, target x/y/z, yaw, pitch, distance, render width, render height */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteSetCamera(JNIEnv *env, jobject, jlong handle, jfloatArray params)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene || !params || env->GetArrayLength(params) < 18) return JNI_FALSE;
  float p[18];
  env->GetFloatArrayRegion(params, 0, 18, p);
  PGCameraLite &c = scene->camera;
  c.type = p[0] != 0.0f ? 1 : 0;
  c.lens = p[1] > 1.0f ? p[1] : 1.0f;
  c.ortho_scale = p[2] > 0.001f ? p[2] : 0.001f;
  c.sensor_x = p[3] > 1.0f ? p[3] : 1.0f;
  c.sensor_y = p[4] > 1.0f ? p[4] : 1.0f;
  c.sensor_fit = (p[5] >= 0.0f && p[5] <= 2.0f) ? static_cast<int>(p[5]) : 0;
  c.shift_x = p[6];
  c.shift_y = p[7];
  c.clip_start = p[8] > 1e-6f ? p[8] : 1e-6f;
  c.clip_end = p[9] > c.clip_start ? p[9] : c.clip_start + 1.0f;
  const float target[3] = {p[10], p[11], p[12]};
  pg_lite_camera_orbit(&c, target, p[13], p[14], p[15]);
  scene->width = p[16] >= 1.0f ? static_cast<int>(p[16]) : 1;
  scene->height = p[17] >= 1.0f ? static_cast<int>(p[17]) : 1;
  return JNI_TRUE;
}

/* Every mesh edge (not the triangulation diagonals) projected with Line Art's camera, as
 * x0, y0, x1, y1 in frame-buffer coordinates (-1..1); edges with an end behind the camera are
 * skipped. A preview of the reference, not Line Art output (no occlusion, all edge types). */
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteProjectEdges(JNIEnv *env, jobject, jlong handle)
{
  PGSceneLite *scene = scene_from(handle);
  std::vector<float> out;
  if (!scene) return to_java(env, out);
  double vp[4][4], sx, sy;
  pg_lite_view_projection(&scene->camera, scene->width, scene->height, 0.0f, vp);
  pg_lite_camera_shift(&scene->camera, scene->width, scene->height, &sx, &sy);
  for (int o = 0; o < scene->totobject; o++) {
    const PGObjectLite &ob = scene->objects[o];
    const PGMeshLite &me = ob.mesh;
    for (int e = 0; e < me.totedge; e++) {
      if (me.edges[e].flag & PG_LITE_EDGE_POLY_INTERNAL) continue;
      double fb[2][2];
      bool visible = true;
      for (int k = 0; k < 2 && visible; k++) {
        const float *v = me.verts[me.edges[e].v[k]];
        float world[3];
        for (int i = 0; i < 3; i++) {
          world[i] = ob.matrix_world[0][i] * v[0] + ob.matrix_world[1][i] * v[1] + ob.matrix_world[2][i] * v[2] + ob.matrix_world[3][i];
        }
        visible = pg_lite_project(vp, sx, sy, world, fb[k]) != 0;
      }
      if (!visible) continue;
      out.insert(out.end(), {float(fb[0][0]), float(fb[0][1]), float(fb[1][0]), float(fb[1][1])});
    }
  }
  return to_java(env, out);
}

/* Line Art (project_grease_lineart_lite.h) on the reference scene with a default modifier's
 * settings and occlusion levels 0..level_end: x0, y0, x1, y1, occlusion, edge type per segment, in
 * frame-buffer coordinates. Empty array when nothing is in view, null on failure. */
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArt(JNIEnv *env, jobject, jlong handle, jint level_end)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene) return nullptr;
  PGLineartSettings settings;
  pg_lineart_settings_default(&settings);
  settings.level_end = level_end < 0 ? 0 : (level_end > 128 ? 128 : level_end);
  PGLineartSegment *segments = nullptr;
  const int n = pg_lineart_compute(scene, &settings, &segments);
  if (n < 0) return nullptr;
  std::vector<float> out;
  out.reserve(static_cast<size_t>(n) * 6u);
  for (int i = 0; i < n; i++) {
    const PGLineartSegment &s = segments[i];
    out.insert(out.end(), {s.x0, s.y0, s.x1, s.y1, float(s.occlusion), float(s.edge_type)});
  }
  pg_lineart_free_segments(segments);
  return to_java(env, out);
}
