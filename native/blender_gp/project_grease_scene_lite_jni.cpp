// SPDX-License-Identifier: GPL-2.0-or-later
//
// JNI for Scene-lite (project_grease_scene_lite.h): the 3D reference scene Line Art will read.
// Its own handle, independent of the GP document and renderer.

#include <jni.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
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
  settings.use_multiple_levels = 1;
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

/* Line Art strokes (pg_lineart_compute_strokes(), default modifier settings, occlusion levels
 * 0..level_end) in frame-buffer coordinates: [stroke_count, { point_count, edge_type, level,
 * x0, y0, x1, y1, ... }]. Null on failure. */
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArtStrokes(JNIEnv *env, jobject, jlong handle, jint level_end)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene) return nullptr;
  PGLineartSettings settings;
  pg_lineart_settings_default(&settings);
  settings.level_end = level_end < 0 ? 0 : (level_end > 128 ? 128 : level_end);
  settings.use_multiple_levels = 1;
  PGLineartStrokes strokes;
  if (pg_lineart_compute_strokes(scene, &settings, &strokes) < 0) return nullptr;
  std::vector<float> out;
  out.reserve(1u + size_t(strokes.stroke_count) * 3u + size_t(strokes.point_count) * 2u);
  out.push_back(float(strokes.stroke_count));
  for (int i = 0; i < strokes.stroke_count; i++) {
    const PGLineartStroke &s = strokes.strokes[i];
    out.insert(out.end(), {float(s.point_count), float(s.edge_type), float(s.level)});
    out.insert(out.end(), strokes.image + size_t(s.first) * 2u, strokes.image + size_t(s.first + s.point_count) * 2u);
  }
  pg_lineart_free_strokes(&strokes);
  return to_java(env, out);
}

namespace {
/* nativeSceneLiteLineArtStrokes output layout for a strokes result (frees the strokes). */
jfloatArray strokes_to_java(JNIEnv *env, PGLineartStrokes &strokes)
{
  std::vector<float> out;
  out.reserve(1u + size_t(strokes.stroke_count) * 3u + size_t(strokes.point_count) * 2u);
  out.push_back(float(strokes.stroke_count));
  for (int i = 0; i < strokes.stroke_count; i++) {
    const PGLineartStroke &s = strokes.strokes[i];
    out.insert(out.end(), {float(s.point_count), float(s.edge_type), float(s.level)});
    out.insert(out.end(), strokes.image + size_t(s.first) * 2u, strokes.image + size_t(s.first + s.point_count) * 2u);
  }
  pg_lineart_free_strokes(&strokes);
  return to_java(env, out);
}

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

/* Line Art strokes with the modifier's options (PGLineartSettings); same output as
 * nativeSceneLiteLineArtStrokes. ints: edge_types, calculation_flags, use_multiple_levels,
 * level_start, level_end, stroke_types, source_type, source_index, modifier_flags, mask_switches,
 * material_mask_bits, intersection_mask, shadow_selection, silhouette_selection, light type (-1 =
 * no light_contour_object, else PG_LITE_LIGHT_*). floats: crease_threshold (rad), overscan,
 * chaining_image_threshold, chain_smooth_tolerance, angle_splitting_threshold, stroke_depth_offset,
 * shadow_camera_near / far / size, light yaw / pitch / distance (an orbit around the origin, like
 * the camera's). sourceVertexGroup may be null. Null on failure. */
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArtStrokesEx(JNIEnv *env, jobject, jlong handle, jintArray ints, jfloatArray floats, jstring sourceVertexGroup)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene || !ints || !floats || env->GetArrayLength(ints) < 15 || env->GetArrayLength(floats) < 12) return nullptr;
  jint iv[15];
  jfloat fv[12];
  env->GetIntArrayRegion(ints, 0, 15, iv);
  env->GetFloatArrayRegion(floats, 0, 12, fv);
  PGLineartSettings st;
  pg_lineart_settings_default(&st);
  st.edge_types = iv[0] & 0x1ff;
  st.calculation_flags = iv[1];
  st.use_multiple_levels = iv[2] != 0;
  st.level_start = clampi(iv[3], 0, 128);
  st.level_end = clampi(iv[4], 0, 128);
  st.stroke_types = iv[5] & 0x1ff;
  st.source_type = clampi(iv[6], 0, 2);
  st.source_index = iv[7];
  st.modifier_flags = iv[8];
  st.mask_switches = iv[9] & 0xff;
  st.material_mask_bits = iv[10] & 0xff;
  st.intersection_mask = iv[11] & 0xff;
  st.shadow_selection = clampi(iv[12], 0, 3);
  st.silhouette_selection = clampi(iv[13], 0, 2);
  st.crease_threshold = fv[0];
  st.overscan = fv[1];
  st.chaining_image_threshold = fv[2];
  st.chain_smooth_tolerance = fv[3];
  st.angle_splitting_threshold = fv[4];
  st.stroke_depth_offset = fv[5];
  st.shadow_camera_near = fv[6];
  st.shadow_camera_far = fv[7];
  st.shadow_camera_size = fv[8];
  if (sourceVertexGroup) {
    const char *name = env->GetStringUTFChars(sourceVertexGroup, nullptr);
    if (name) {
      snprintf(st.source_vertex_group, sizeof(st.source_vertex_group), "%s", name);
      env->ReleaseStringUTFChars(sourceVertexGroup, name);
    }
  }
  scene->light.present = iv[14] >= 0;
  if (scene->light.present) {
    scene->light.type = iv[14];
    PGCameraLite tmp;
    pg_lite_camera_default(&tmp);
    const float target[3] = {0.0f, 0.0f, 0.0f};
    pg_lite_camera_orbit(&tmp, target, fv[9], fv[10], fv[11]);
    std::memcpy(scene->light.matrix_world, tmp.matrix_world, sizeof(tmp.matrix_world));
  }
  PGLineartStrokes strokes;
  if (pg_lineart_compute_strokes(scene, &st, &strokes) < 0) return nullptr;
  return strokes_to_java(env, strokes);
}

/* Object line art (ObjectLineArt): usage (PG_LITE_USAGE_*), flags (PG_LITE_OBJECT_*), own crease
 * threshold (radians), own intersection priority, and its collection (-1 = master). */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteSetObjectLineArt(JNIEnv *, jobject, jlong handle, jint index, jint usage, jint flags, jfloat crease, jint priority, jint collection)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene || index < 0 || index >= scene->totobject || collection < -1 || collection >= scene->totcollection) return JNI_FALSE;
  PGObjectLite &ob = scene->objects[index];
  ob.line_art_usage = usage;
  ob.line_art_flags = flags;
  ob.line_art_crease_threshold = crease;
  ob.line_art_intersection_priority = clampi(priority, 0, 255);
  ob.collection = collection;
  return JNI_TRUE;
}

/* Adds a collection under parent (-1 = master) with line art usage / flags / intersection mask /
 * priority; returns its index, -1 on failure. */
extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteAddCollection(JNIEnv *env, jobject, jlong handle, jstring name, jint parent, jint usage, jint flags, jint mask, jint priority)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene) return -1;
  const char *n = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
  const int c = pg_lite_add_collection(scene, n ? n : "Collection", parent);
  if (n) env->ReleaseStringUTFChars(name, n);
  if (c < 0) return -1;
  PGCollectionLite &col = scene->collections[c];
  col.lineart_usage = usage;
  col.lineart_flags = flags;
  col.lineart_intersection_mask = mask & 0xff;
  col.lineart_intersection_priority = clampi(priority, 0, 255);
  return c;
}

/* Material line art (MaterialLineArt): flags (PG_LITE_MATERIAL_*), mask bits, occlusion, priority,
 * back-face culling. */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteSetMaterialLineArt(JNIEnv *, jobject, jlong handle, jint index, jint flags, jint maskBits, jint occlusion, jint priority, jboolean backfaceCulling)
{
  PGSceneLite *scene = scene_from(handle);
  if (!scene || index < 0 || index >= scene->totmaterial) return JNI_FALSE;
  PGMaterialLite &m = scene->materials[index];
  m.lineart_flags = flags;
  m.material_mask_bits = maskBits & 0xff;
  m.mat_occlusion = clampi(occlusion, 0, 255);
  m.intersection_priority = clampi(priority, 0, 255);
  m.use_backface_culling = backfaceCulling ? 1 : 0;
  return JNI_TRUE;
}

/* Names of the scene's objects (kind 0), materials (1) or collections (2). */
extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteNames(JNIEnv *env, jobject, jlong handle, jint kind)
{
  PGSceneLite *scene = scene_from(handle);
  jclass string_class = env->FindClass("java/lang/String");
  if (!scene || !string_class) return nullptr;
  const int n = kind == 0 ? scene->totobject : (kind == 1 ? scene->totmaterial : scene->totcollection);
  jobjectArray result = env->NewObjectArray(n, string_class, nullptr);
  for (int i = 0; result && i < n; i++) {
    const char *name = kind == 0 ? scene->objects[i].name : (kind == 1 ? scene->materials[i].name : scene->collections[i].name);
    jstring s = env->NewStringUTF(name);
    env->SetObjectArrayElement(result, i, s);
    env->DeleteLocalRef(s);
  }
  return result;
}
