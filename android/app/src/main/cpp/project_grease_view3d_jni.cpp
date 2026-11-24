// JNI of the 3D viewport (native/blender_gp/android_gp_view3d.inc), Kotlin View3DNative.
// shortcut: one viewport state per process (the app has one editor view); key it by renderer
// when a second 3D view exists.
#include <jni.h>

struct bGPdata;
struct bGPDstroke;
extern "C" {
void project_grease_view3d_set_active(int active);
int project_grease_view3d_active();
void project_grease_view3d_orbit(float dx, float dy, float ui_scale);
void project_grease_view3d_pan(float dx, float dy);
void project_grease_view3d_zoom(float dfac, float sx, float sy);
void project_grease_view3d_set_axis_view(int view);
void project_grease_view3d_toggle_persp();
void project_grease_view3d_state(float *out);
void project_grease_view3d_set_placement(int align, int lock_axis);
void project_grease_view3d_cursor_at(float sx, float sy);
int project_grease_view3d_select_at(float sx, float sy, int extend, float ui_scale);
void project_grease_view3d_move_selected(float dx, float dy);
int project_grease_view3d_add_object(int type);
int project_grease_view3d_object_count();
int project_grease_view3d_object_get(int i, float *out);
int project_grease_view3d_canvas_to_screen(float x, float y, float z, float *out);
void project_grease_view3d_reset();
void project_grease_view3d_canvas_matrix(float *out);
}

#define V3D_JNI(ret, name) extern "C" JNIEXPORT ret JNICALL Java_com_smitnk_projectgrease_nativebridge_View3DNative_##name

static jfloatArray to_array(JNIEnv *env, const float *v, int n)
{
  jfloatArray a = env->NewFloatArray(n);
  if (a) env->SetFloatArrayRegion(a, 0, n, v);
  return a;
}

V3D_JNI(void, nativeSetActive)(JNIEnv *, jobject, jboolean on) { project_grease_view3d_set_active(on ? 1 : 0); }
V3D_JNI(jboolean, nativeActive)(JNIEnv *, jobject) { return project_grease_view3d_active() ? JNI_TRUE : JNI_FALSE; }
V3D_JNI(void, nativeOrbit)(JNIEnv *, jobject, jfloat dx, jfloat dy, jfloat s) { project_grease_view3d_orbit(dx, dy, s); }
V3D_JNI(void, nativePan)(JNIEnv *, jobject, jfloat dx, jfloat dy) { project_grease_view3d_pan(dx, dy); }
V3D_JNI(void, nativeZoom)(JNIEnv *, jobject, jfloat f, jfloat x, jfloat y) { project_grease_view3d_zoom(f, x, y); }
V3D_JNI(void, nativeSetAxisView)(JNIEnv *, jobject, jint view) { project_grease_view3d_set_axis_view(view); }
V3D_JNI(void, nativeTogglePersp)(JNIEnv *, jobject) { project_grease_view3d_toggle_persp(); }
V3D_JNI(jfloatArray, nativeState)(JNIEnv *env, jobject)
{
  float s[11];
  project_grease_view3d_state(s);
  return to_array(env, s, 11);
}
V3D_JNI(void, nativeSetPlacement)(JNIEnv *, jobject, jint align, jint lock) { project_grease_view3d_set_placement(align, lock); }
V3D_JNI(void, nativeCursorAt)(JNIEnv *, jobject, jfloat x, jfloat y) { project_grease_view3d_cursor_at(x, y); }
V3D_JNI(jint, nativeSelectAt)(JNIEnv *, jobject, jfloat x, jfloat y, jboolean extend, jfloat s)
{
  return project_grease_view3d_select_at(x, y, extend ? 1 : 0, s);
}
V3D_JNI(jint, nativeHitAt)(JNIEnv *, jobject, jfloat x, jfloat y, jfloat s) { return project_grease_view3d_select_at(x, y, -1, s); }
V3D_JNI(void, nativeMoveSelected)(JNIEnv *, jobject, jfloat dx, jfloat dy) { project_grease_view3d_move_selected(dx, dy); }
V3D_JNI(jint, nativeAddObject)(JNIEnv *, jobject, jint type) { return project_grease_view3d_add_object(type); }
V3D_JNI(jint, nativeObjectCount)(JNIEnv *, jobject) { return project_grease_view3d_object_count(); }
V3D_JNI(jfloatArray, nativeObject)(JNIEnv *env, jobject, jint i)
{
  float o[5];
  return project_grease_view3d_object_get(i, o) ? to_array(env, o, 5) : nullptr;
}
V3D_JNI(jfloatArray, nativeCanvasToScreen)(JNIEnv *env, jobject, jfloat x, jfloat y, jfloat z)
{
  float s[2];
  return project_grease_view3d_canvas_to_screen(x, y, z, s) ? to_array(env, s, 2) : nullptr;
}
V3D_JNI(jfloatArray, nativeCanvasMatrix)(JNIEnv *env, jobject)
{
  float m[16];
  project_grease_view3d_canvas_matrix(m);
  return to_array(env, m, 16);
}
V3D_JNI(void, nativeReset)(JNIEnv *, jobject) { project_grease_view3d_reset(); }
