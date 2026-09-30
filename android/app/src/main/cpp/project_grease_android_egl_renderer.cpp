#include <algorithm>
#include <jni.h>

#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES3/gl3.h>

#include <cstdint>
#include <vector>

#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"

namespace {

using GPHandle = ProjectGreaseGPHandle *;

extern "C" void project_grease_android_present_reset(void);
extern "C" void project_grease_android_present_set_color(float r, float g, float b, float a);
extern "C" void project_grease_android_present_set_canvas_size(int width, int height);
extern "C" void project_grease_android_present_set_view_transform(float zoom, float pan_x, float pan_y);
extern "C" int project_grease_android_present_pending_stroke(
    const project_grease::gp::StrokePoint *points, int count, float thickness);

struct Renderer {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  ANativeWindow *window = nullptr;
  int width = 0;
  int height = 0;
  int gles_version = 0;
  GPHandle gp_handle = nullptr;
  bool gp_connected = false;
  std::vector<ProjectGreaseGPPoint> preview_points;
  float preview_thickness = 1.0f;
};

Renderer *from_handle(jlong value)
{
  return reinterpret_cast<Renderer *>(static_cast<uintptr_t>(value));
}

jlong to_handle(Renderer *renderer)
{
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(renderer));
}

bool choose_config(Renderer &renderer, EGLConfig &config, bool es3)
{
  const EGLint renderable = es3 ? EGL_OPENGL_ES3_BIT_KHR : EGL_OPENGL_ES2_BIT;
  const EGLint config_attributes[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, renderable,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE,
  };

  EGLint config_count = 0;
  return eglChooseConfig(
             renderer.display, config_attributes, &config, 1, &config_count) == EGL_TRUE &&
         config_count == 1;
}

bool create_context(Renderer &renderer, EGLConfig config, int version)
{
  const EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, version,
      EGL_NONE,
  };

  renderer.context = eglCreateContext(
      renderer.display, config, EGL_NO_CONTEXT, context_attributes);
  if (renderer.context == EGL_NO_CONTEXT) {
    return false;
  }

  renderer.gles_version = version;
  return true;
}

bool initialize_egl(Renderer &renderer)
{
  renderer.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (renderer.display == EGL_NO_DISPLAY) {
    return false;
  }

  EGLint major = 0;
  EGLint minor = 0;
  if (eglInitialize(renderer.display, &major, &minor) != EGL_TRUE) {
    renderer.display = EGL_NO_DISPLAY;
    return false;
  }

  if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
    eglTerminate(renderer.display);
    renderer.display = EGL_NO_DISPLAY;
    return false;
  }

  EGLConfig config = nullptr;
  if (choose_config(renderer, config, true) && create_context(renderer, config, 3)) {
    return true;
  }

  if (renderer.context != EGL_NO_CONTEXT) {
    eglDestroyContext(renderer.display, renderer.context);
    renderer.context = EGL_NO_CONTEXT;
  }

  if (!choose_config(renderer, config, false) || !create_context(renderer, config, 2)) {
    eglTerminate(renderer.display);
    renderer.display = EGL_NO_DISPLAY;
    return false;
  }

  return true;
}

void disconnect_blender_gp(Renderer &renderer)
{
  if (!renderer.gp_handle) {
    renderer.gp_connected = false;
    return;
  }

  project_grease_gp_destroy(renderer.gp_handle);

  renderer.gp_handle = nullptr;
  renderer.gp_connected = false;
}

bool connect_blender_gp(Renderer &renderer)
{
  disconnect_blender_gp(renderer);

  GPHandle handle = project_grease_gp_create();
  if (!handle) {
    return false;
  }

  if (!project_grease_gp_initialize_external_gpu(handle)) {
    project_grease_gp_destroy(handle);
    return false;
  }

  renderer.gp_handle = handle;
  renderer.gp_connected = true;
  return true;
}

bool attach_window(Renderer &renderer, ANativeWindow *window)
{
  if (!window || renderer.display == EGL_NO_DISPLAY ||
      renderer.context == EGL_NO_CONTEXT) {
    return false;
  }

  const bool es3 = renderer.gles_version >= 3;
  EGLConfig config = nullptr;
  if (!choose_config(renderer, config, es3)) {
    return false;
  }

  renderer.surface = eglCreateWindowSurface(
      renderer.display, config, window, nullptr);
  if (renderer.surface == EGL_NO_SURFACE) {
    return false;
  }

  if (eglMakeCurrent(
          renderer.display, renderer.surface, renderer.surface, renderer.context) != EGL_TRUE) {
    eglDestroySurface(renderer.display, renderer.surface);
    renderer.surface = EGL_NO_SURFACE;
    return false;
  }

  renderer.window = window;
  renderer.width = 0;
  renderer.height = 0;
  eglQuerySurface(renderer.display, renderer.surface, EGL_WIDTH, &renderer.width);
  eglQuerySurface(renderer.display, renderer.surface, EGL_HEIGHT, &renderer.height);

  glViewport(0, 0, renderer.width, renderer.height);

  // Connect only after Android's EGL/GLES context is current on this thread.
  // The Blender GP backend does not create or own the Android EGL objects.
  connect_blender_gp(renderer);
  return true;
}

void detach_window(Renderer &renderer)
{
  if (renderer.display == EGL_NO_DISPLAY) {
    return;
  }

  // Destroy GP and presentation resources while the external GL context is still current.
  disconnect_blender_gp(renderer);
  project_grease_android_present_reset();

  eglMakeCurrent(
      renderer.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  if (renderer.surface != EGL_NO_SURFACE) {
    eglDestroySurface(renderer.display, renderer.surface);
    renderer.surface = EGL_NO_SURFACE;
  }

  if (renderer.window) {
    ANativeWindow_release(renderer.window);
    renderer.window = nullptr;
  }

  renderer.width = 0;
  renderer.height = 0;
}

void destroy_renderer(Renderer &renderer)
{
  detach_window(renderer);

  if (renderer.display != EGL_NO_DISPLAY) {
    if (renderer.context != EGL_NO_CONTEXT) {
      eglDestroyContext(renderer.display, renderer.context);
      renderer.context = EGL_NO_CONTEXT;
    }

    eglTerminate(renderer.display);
    renderer.display = EGL_NO_DISPLAY;
  }
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreateEglRenderer(
    JNIEnv *, jobject)
{
  auto *renderer = new Renderer();
  if (!initialize_egl(*renderer)) {
    delete renderer;
    return 0;
  }
  return to_handle(renderer);
}

extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDestroyEglRenderer(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer) {
    return;
  }

  destroy_renderer(*renderer);
  delete renderer;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeResetDocumentEgl(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected ||
      renderer->display == EGL_NO_DISPLAY || renderer->surface == EGL_NO_SURFACE ||
      renderer->context == EGL_NO_CONTEXT) return JNI_FALSE;
  if (eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context) != EGL_TRUE) {
    return JNI_FALSE;
  }
  return project_grease_gp_reset_document(renderer->gp_handle) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGetGpHandle(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected) return 0;
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(renderer->gp_handle));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeAttachSurface(
    JNIEnv *env, jobject, jlong handle, jobject surface)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !surface) {
    return JNI_FALSE;
  }

  detach_window(*renderer);

  ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
  if (!window) {
    return JNI_FALSE;
  }

  if (!attach_window(*renderer, window)) {
    ANativeWindow_release(window);
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeDetachSurface(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (renderer) {
    detach_window(*renderer);
  }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRenderEgl(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || renderer->display == EGL_NO_DISPLAY ||
      renderer->surface == EGL_NO_SURFACE ||
      renderer->context == EGL_NO_CONTEXT) {
    return JNI_FALSE;
  }

  if (eglMakeCurrent(
          renderer->display, renderer->surface, renderer->surface, renderer->context) != EGL_TRUE) {
    return JNI_FALSE;
  }

  eglQuerySurface(renderer->display, renderer->surface, EGL_WIDTH, &renderer->width);
  eglQuerySurface(renderer->display, renderer->surface, EGL_HEIGHT, &renderer->height);
  glViewport(0, 0, renderer->width, renderer->height);

  if (renderer->gp_connected) {
    if (!project_grease_gp_render_external_context(renderer->gp_handle)) {
      return JNI_FALSE;
    }
    if (!renderer->preview_points.empty()) {
      std::vector<project_grease::gp::StrokePoint> preview_points;
      preview_points.reserve(renderer->preview_points.size());
      for (const ProjectGreaseGPPoint &point : renderer->preview_points) {
        preview_points.push_back(
            project_grease::gp::StrokePoint{
                point.x, point.y, point.z, point.pressure, point.strength, point.time});
      }
      if (!project_grease_android_present_pending_stroke(
              preview_points.data(),
              static_cast<int>(preview_points.size()),
              renderer->preview_thickness)) {
        return JNI_FALSE;
      }
    }
  }
  else {
    // Transport fallback until the Android-compatible Blender GP library is
    // linked into this JNI target.
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  return eglSwapBuffers(renderer->display, renderer->surface) == EGL_TRUE
             ? JNI_TRUE
             : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeBeginStrokeEglRenderer(
    JNIEnv *, jobject, jlong handle, jint material_index, jfloat thickness)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected) {
    return JNI_FALSE;
  }
  return project_grease_gp_begin_stroke(
             renderer->gp_handle, material_index, thickness) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeAddPointEglRenderer(
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
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected) {
    return JNI_FALSE;
  }
  ProjectGreaseGPPoint point{x, y, z, pressure, strength, time};
  return project_grease_gp_add_point(renderer->gp_handle, point) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetPreviewStrokeEglRenderer(
    JNIEnv *env, jobject, jlong handle, jfloatArray packed, jfloat thickness)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !packed) return JNI_FALSE;
  const jsize length = env->GetArrayLength(packed);
  if (length <= 0 || (length % 3) != 0) {
    renderer->preview_points.clear();
    return JNI_TRUE;
  }
  std::vector<jfloat> values(static_cast<size_t>(length));
  env->GetFloatArrayRegion(packed, 0, length, values.data());
  renderer->preview_points.clear();
  renderer->preview_points.reserve(static_cast<size_t>(length / 3));
  for (jsize n = 0; n < length; n += 3) {
    renderer->preview_points.push_back(
        ProjectGreaseGPPoint{values[n], values[n + 1], 0.0f, values[n + 2], 1.0f, 0.0f});
  }
  renderer->preview_thickness = thickness;
  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetCanvasSize(
    JNIEnv *, jobject, jlong handle, jint width, jint height)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || width <= 0 || height <= 0) return JNI_FALSE;
  project_grease_android_present_set_canvas_size(width, height);
  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetViewTransform(
    JNIEnv *, jobject, jlong handle, jfloat zoom, jfloat pan_x, jfloat pan_y)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer) return JNI_FALSE;
  if (renderer->display == EGL_NO_DISPLAY || renderer->surface == EGL_NO_SURFACE) {
    return JNI_FALSE;
  }
  if (eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context) != EGL_TRUE) {
    return JNI_FALSE;
  }
  project_grease_android_present_set_view_transform(
      std::max(0.1f, std::min(8.0f, zoom)), pan_x, pan_y);
  return JNI_TRUE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativePickColorEglRenderer(
    JNIEnv *, jobject, jlong handle, jint x, jint y)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected ||
      renderer->display == EGL_NO_DISPLAY ||
      renderer->surface == EGL_NO_SURFACE ||
      renderer->context == EGL_NO_CONTEXT) {
    return 0;
  }
  if (eglMakeCurrent(
          renderer->display, renderer->surface, renderer->surface, renderer->context) != EGL_TRUE) {
    return 0;
  }
  eglQuerySurface(renderer->display, renderer->surface, EGL_WIDTH, &renderer->width);
  eglQuerySurface(renderer->display, renderer->surface, EGL_HEIGHT, &renderer->height);
  if (x < 0 || y < 0 || x >= renderer->width || y >= renderer->height) {
    return 0;
  }
  // Render the current Legacy GP document before sampling it.
  if (!project_grease_gp_render_external_context(renderer->gp_handle)) {
    return 0;
  }
  const GLint read_y = renderer->height - 1 - y;
  GLubyte rgba[4] = {0, 0, 0, 0};
  glReadPixels(x, read_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  if (glGetError() != GL_NO_ERROR) {
    return 0;
  }
  return (static_cast<jint>(rgba[3]) << 24) |
         (static_cast<jint>(rgba[0]) << 16) |
         (static_cast<jint>(rgba[1]) << 8) |
         static_cast<jint>(rgba[2]);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetStrokeColorEglRenderer(
    JNIEnv *, jobject, jlong handle, jfloat r, jfloat g, jfloat b, jfloat a)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer) return JNI_FALSE;
  if (renderer->display == EGL_NO_DISPLAY || renderer->surface == EGL_NO_SURFACE) {
    return JNI_FALSE;
  }
  if (eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context) != EGL_TRUE) {
    return JNI_FALSE;
  }
  project_grease_android_present_set_color(r, g, b, a);
  return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeClearPreviewStrokeEglRenderer(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer) return JNI_FALSE;
  renderer->preview_points.clear();
  return JNI_TRUE;
}


extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeFillAtEglRenderer(
    JNIEnv *,
    jobject,
    jlong handle,
    jint seed_x,
    jint seed_y,
    jint material_index,
    jfloat thickness)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected ||
      renderer->display == EGL_NO_DISPLAY ||
      renderer->surface == EGL_NO_SURFACE ||
      renderer->context == EGL_NO_CONTEXT) {
    return JNI_FALSE;
  }

  if (eglMakeCurrent(renderer->display,
                     renderer->surface,
                     renderer->surface,
                     renderer->context) != EGL_TRUE) {
    return JNI_FALSE;
  }

  if (!project_grease_gp_render_fill_mask(renderer->gp_handle)) {
    return JNI_FALSE;
  }

  const int width = renderer->width;
  const int height = renderer->height;
  if (width < 3 || height < 3) return JNI_FALSE;

  std::vector<GLubyte> pixels(
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  if (glGetError() != GL_NO_ERROR) {
    return JNI_FALSE;
  }

  std::vector<float> rgba(pixels.size());
  for (size_t i = 0; i < pixels.size(); ++i) {
    rgba[i] = static_cast<float>(pixels[i]) / 255.0f;
  }

  const bool filled = project_grease_gp_fill_at_screen(
      renderer->gp_handle,
      rgba.data(),
      width,
      height,
      seed_x,
      seed_y,
      3,
      0,
      material_index,
      thickness) != 0;

  const bool restored = project_grease_gp_render_external_context(renderer->gp_handle) != 0;
  return (filled && restored) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCancelStrokeEglRenderer(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected) return JNI_FALSE;
  return project_grease_gp_cancel_stroke(renderer->gp_handle) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeEndStrokeEglRenderer(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected) {
    return JNI_FALSE;
  }
  return project_grease_gp_end_stroke(renderer->gp_handle) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeEglReady(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  return renderer &&
                 renderer->display != EGL_NO_DISPLAY &&
                 renderer->context != EGL_NO_CONTEXT
             ? JNI_TRUE
             : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGlesVersion(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  return renderer ? renderer->gles_version : 0;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeBlenderGpConnected(
    JNIEnv *, jobject, jlong handle)
{
  Renderer *renderer = from_handle(handle);
  return renderer && renderer->gp_connected ? JNI_TRUE : JNI_FALSE;
}
