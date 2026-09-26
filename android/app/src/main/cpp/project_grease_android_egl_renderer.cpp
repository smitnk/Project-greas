#include <jni.h>

#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <cstdint>

namespace {

struct Renderer {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  ANativeWindow *window = nullptr;
  int width = 0;
  int height = 0;
};

Renderer *from_handle(jlong value)
{
  return reinterpret_cast<Renderer *>(static_cast<uintptr_t>(value));
}

jlong to_handle(Renderer *renderer)
{
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(renderer));
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

  const EGLint config_attributes[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE,
  };

  EGLConfig config = nullptr;
  EGLint config_count = 0;
  if (eglChooseConfig(
          renderer.display, config_attributes, &config, 1, &config_count) != EGL_TRUE ||
      config_count != 1) {
    eglTerminate(renderer.display);
    renderer.display = EGL_NO_DISPLAY;
    return false;
  }

  const EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE,
  };

  renderer.context = eglCreateContext(
      renderer.display, config, EGL_NO_CONTEXT, context_attributes);
  if (renderer.context == EGL_NO_CONTEXT) {
    eglTerminate(renderer.display);
    renderer.display = EGL_NO_DISPLAY;
    return false;
  }

  return true;
}

bool attach_window(Renderer &renderer, ANativeWindow *window)
{
  if (!window || renderer.display == EGL_NO_DISPLAY ||
      renderer.context == EGL_NO_CONTEXT) {
    return false;
  }

  const EGLint config_attributes[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE,
  };

  EGLConfig config = nullptr;
  EGLint config_count = 0;
  if (eglChooseConfig(
          renderer.display, config_attributes, &config, 1, &config_count) != EGL_TRUE ||
      config_count != 1) {
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
  return true;
}

void detach_window(Renderer &renderer)
{
  if (renderer.display == EGL_NO_DISPLAY) {
    return;
  }

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
  glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

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
