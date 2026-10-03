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
extern "C" void project_grease_android_present_set_selection_overlay(int enabled);
extern "C" void project_grease_android_present_set_weight_view(int group);
extern "C" void project_grease_android_present_set_fill_draw_mode(int mode);
extern "C" void project_grease_android_present_set_export_mode(int mode);
extern "C" void project_grease_android_present_get_view_transform(float *zoom, float *pan_x, float *pan_y);
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
  // Fill tool options (Blender brush defaults: fill_leak 3, dilate 1, fill_draw_mode BOTH).
  int fill_leak = 3;
  int fill_dilate = 1;
  int fill_draw_mode = 0;
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

/* Renders the document (and the shape preview) and presents it. */
static bool render_now(Renderer *renderer)
{
  if (!renderer || renderer->display == EGL_NO_DISPLAY ||
      renderer->surface == EGL_NO_SURFACE ||
      renderer->context == EGL_NO_CONTEXT) {
    return false;
  }

  if (eglMakeCurrent(
          renderer->display, renderer->surface, renderer->surface, renderer->context) != EGL_TRUE) {
    return false;
  }

  eglQuerySurface(renderer->display, renderer->surface, EGL_WIDTH, &renderer->width);
  eglQuerySurface(renderer->display, renderer->surface, EGL_HEIGHT, &renderer->height);
  glViewport(0, 0, renderer->width, renderer->height);

  if (renderer->gp_connected) {
    if (!project_grease_gp_render_external_context(renderer->gp_handle)) {
      return false;
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
        return false;
      }
    }
  }
  else {
    // Transport fallback until the Android-compatible Blender GP library is
    // linked into this JNI target.
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  return eglSwapBuffers(renderer->display, renderer->surface) == EGL_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRenderEgl(
    JNIEnv *, jobject, jlong handle)
{
  return render_now(from_handle(handle)) ? JNI_TRUE : JNI_FALSE;
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

/* group >= 0 tints the strokes by that vertex group's weights (Weight Paint mode), -1 is the normal view. */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetWeightView(
    JNIEnv *, jobject, jlong handle, jint group)
{
  if (!from_handle(handle)) return JNI_FALSE;
  project_grease_android_present_set_weight_view(group);
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

  project_grease_android_present_set_fill_draw_mode(renderer->fill_draw_mode);
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
      renderer->fill_leak,
      renderer->fill_dilate,
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

/* Fill tool options: leak size (px, >= 1), dilate (px, negative contracts) and the boundary source
 * (fill_draw_mode: 0 All, 1 Strokes, 2 Edit Lines). */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetFillOptionsEglRenderer(
    JNIEnv *, jobject, jlong handle, jint leak, jint dilate, jint draw_mode)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer) return JNI_FALSE;
  renderer->fill_leak = std::max(1, std::min(100, static_cast<int>(leak)));
  renderer->fill_dilate = std::max(-40, std::min(40, static_cast<int>(dilate)));
  renderer->fill_draw_mode = std::max(0, std::min(2, static_cast<int>(draw_mode)));
  return JNI_TRUE;
}

/* Renders the current frame (modifiers, masks and effects as on screen; no annotations, no open
 * stroke) offscreen at canvas size and returns ARGB pixels, top row first, or null. */
extern "C" JNIEXPORT jintArray JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRenderCanvasPixelsEglRenderer(
    JNIEnv *env, jobject, jlong handle, jint width, jint height, jboolean transparent)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected || width <= 0 || height <= 0 ||
      renderer->display == EGL_NO_DISPLAY || renderer->surface == EGL_NO_SURFACE ||
      renderer->context == EGL_NO_CONTEXT) {
    return nullptr;
  }
  if (eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context) != EGL_TRUE) {
    return nullptr;
  }
  GLint max_size = 0, max_rb = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_rb);
  if (width > max_size || height > max_size || width > max_rb || height > max_rb) return nullptr;
  GLint prev_fbo = 0, vp[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
  glGetIntegerv(GL_VIEWPORT, vp);
  GLuint tex = 0, fbo = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  jintArray result = nullptr;
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
    float zoom = 1.0f, pan_x = 0.0f, pan_y = 0.0f;
    project_grease_android_present_get_view_transform(&zoom, &pan_x, &pan_y);
    // The presenter fits the canvas at 92% x zoom; 1 / 0.92 maps one canvas unit to one pixel.
    project_grease_android_present_set_view_transform(1.0f / 0.92f, 0.0f, 0.0f);
    project_grease_android_present_set_export_mode(transparent ? 2 : 1);
    glViewport(0, 0, width, height);
    const bool ok = project_grease_gp_render_external_context(renderer->gp_handle) != 0;
    std::vector<GLubyte> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    if (ok) glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    project_grease_android_present_set_export_mode(0);
    project_grease_android_present_set_view_transform(zoom, pan_x, pan_y);
    if (ok && glGetError() == GL_NO_ERROR) {
      std::vector<jint> argb(static_cast<size_t>(width) * static_cast<size_t>(height));
      for (int y = 0; y < height; ++y) {
        const GLubyte *row = &pixels[static_cast<size_t>(height - 1 - y) * static_cast<size_t>(width) * 4u];
        for (int x = 0; x < width; ++x) {
          const GLubyte *p = row + static_cast<size_t>(x) * 4u;
          unsigned a = p[3], r = p[0], g = p[1], b = p[2];
          if (!transparent) {
            a = 255;
          }
          else if (a > 0 && a < 255) {
            // Blending accumulates premultiplied color over the cleared (0,0,0,0) target.
            r = std::min(255u, (r * 255u + a / 2u) / a);
            g = std::min(255u, (g * 255u + a / 2u) / a);
            b = std::min(255u, (b * 255u + a / 2u) / a);
          }
          else if (a == 0) {
            r = g = b = 0;
          }
          argb[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
              static_cast<jint>((a << 24) | (r << 16) | (g << 8) | b);
        }
      }
      result = env->NewIntArray(width * height);
      if (result) env->SetIntArrayRegion(result, 0, width * height, argb.data());
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &tex);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  return result;
}

/* Native tool session (project_grease_tool_session.h): one input batch. `samples` holds `count`
 * samples of (x, y, pressure, time) in canvas units; on phase BEGIN the floats after them are the
 * tool parameters. The tool runs on the Legacy GP data, the cache is tagged, and the view is
 * rendered once for the whole batch. Returns the PG_TOOL_RESULT_* bits (0 = refused). */
extern "C" JNIEXPORT jint JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeToolSamples(
    JNIEnv *env, jobject, jlong handle, jint tool, jfloatArray samples, jint count, jint phase)
{
  Renderer *renderer = from_handle(handle);
  if (!renderer || !renderer->gp_connected || count < 0) return 0;
  const jsize length = samples ? env->GetArrayLength(samples) : 0;
  const jsize sample_floats = static_cast<jsize>(count) * 4;
  if (length < sample_floats) return 0;
  std::vector<float> data(static_cast<size_t>(length));
  if (length > 0) env->GetFloatArrayRegion(samples, 0, length, data.data());
  const float *params = length > sample_floats ? data.data() + sample_floats : nullptr;
  const int param_count = static_cast<int>(length - sample_floats);
  const int result = project_grease_gp_tool_samples(
      renderer->gp_handle, tool, data.data(), count, phase, params, param_count);
  if (result != 0 && (result & 2 /* PG_TOOL_RESULT_CHANGED */) != 0) {
    render_now(renderer);
  }
  return result;
}

/* Edit-mode selection overlay: points of the editable strokes, selected ones highlighted. */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetSelectionOverlay(
    JNIEnv *, jobject, jlong handle, jboolean enabled)
{
  if (!from_handle(handle)) return JNI_FALSE;
  project_grease_android_present_set_selection_overlay(enabled ? 1 : 0);
  return JNI_TRUE;
}
