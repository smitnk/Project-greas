#include "project_grease_gp_backend.h"

#include <cstdio>

#include "GHOST-c-api.h"
#include "GPU_init_exit.h"

int main()
{
  GHOST_SystemHandle system = GHOST_CreateSystemBackground();
  if (!system) {
    std::fprintf(stderr, "GHOST system creation failed\n");
    return 1;
  }

  GHOST_GLSettings settings = {};
  settings.context_type = GHOST_kDrawingContextTypeOpenGL;
  GHOST_ContextHandle context = GHOST_CreateOpenGLContext(system, settings);
  if (!context) {
    GHOST_DisposeSystem(system);
    std::fprintf(stderr, "GHOST OpenGL context creation failed\n");
    return 2;
  }

  if (GHOST_ActivateOpenGLContext(context) != GHOST_kSuccess) {
    GHOST_DisposeOpenGLContext(system, context);
    GHOST_DisposeSystem(system);
    std::fprintf(stderr, "GHOST OpenGL context activation failed\n");
    return 3;
  }

  project_grease::gp::Backend backend;
  if (!backend.initialize() ||
      !backend.create_document() ||
      !backend.create_layer("Layer 1") ||
      !backend.create_frame(1) ||
      !backend.begin_stroke({0, 3.0f})) {
    std::fprintf(stderr, "GP setup failed: %s\n", backend.last_error());
    GHOST_ReleaseOpenGLContext(context);
    GHOST_DisposeOpenGLContext(system, context);
    GHOST_DisposeSystem(system);
    return 4;
  }

  backend.add_point({-1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f});
  backend.add_point({0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.1f});
  backend.add_point({1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.2f});

  if (!backend.end_stroke()) {
    std::fprintf(stderr, "stroke creation failed: %s\n", backend.last_error());
    return 5;
  }

  // The GHOST context belongs to this test only. The Backend must attach to
  // the already-current GL context without creating or owning GHOST.
  if (!backend.initialize_external_gpu_context()) {
    std::fprintf(stderr, "external GPU initialization failed: %s\n",
                 backend.last_error());
    return 6;
  }

  if (!backend.render_external_context()) {
    std::fprintf(stderr, "external GP render failed: %s\n",
                 backend.last_error());
    return 7;
  }

  std::puts("External Blender GPU context GP render test passed");

  // Backend::shutdown() must not release the externally-owned GHOST context.
  backend.shutdown();
  GHOST_ReleaseOpenGLContext(context);
  GHOST_DisposeOpenGLContext(system, context);
  GHOST_DisposeSystem(system);
  return 0;
}
