#include <jni.h>

#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES3/gl3.h>

#include <cstdint>

namespace {

using GPHandle = void *;

extern "C" GPHandle project_grease_gp_create(void) __attribute__((weak));
extern "C" void project_grease_gp_destroy(GPHandle) __attribute__((weak));
extern "C" int project_grease_gp_initialize_external_gpu(GPHandle) __attribute__((weak));
extern "C" int project_grease_gp_render_external_context(GPHandle) __attribute__((weak));

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

  if (project_grease_gp_destroy) {
    project_grease_gp_destroy(renderer.gp_handle);
  }

  renderer.gp_handle = nullptr;
  renderer.gp_connected = false;
}

bool connect_blender_gp(Renderer &renderer)
{
  disconnect_blender_gp(renderer);

  if (!project_grease_gp_create ||
      !project_grease_gp_initialize_external_gpu ||
      !project_grease_gp_render_external_context) {
    return false;
  }

  GPHandle handle = project_grease_gp_create();
  if (!handle) {
    return false;
  }

  if (!project_grease_gp_initialize_external_gpu(handle)) {
    if (project_grease_gp_destroy) {
      project_grease_gp_destroy(handle);
    }
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

  // Destroy the GP backend while its external GL context is still current.
  disconnect_blender_gp(renderer);

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
