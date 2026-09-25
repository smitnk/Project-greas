#include "project_grease_gp_backend.h"

#include <cstdio>

#include "BKE_gpencil_legacy.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *gpd);
extern "C" void GPENCIL_engine_init(void *ved);

int main() {
  volatile auto cache_fn = &DRW_gpencil_batch_cache_dirty_tag;
  volatile auto engine_fn = &GPENCIL_engine_init;
  (void)cache_fn;
  (void)engine_fn;

  project_grease::gp::Backend backend;
  if (!backend.initialize()) {
    std::fprintf(stderr, "initialize failed: %s\n", backend.last_error());
    return 1;
  }
  if (!backend.create_document() ||
      !backend.create_layer("Layer 1") ||
      !backend.create_frame(1) ||
      !backend.begin_stroke({0, 3.0f})) {
    std::fprintf(stderr, "setup failed: %s\n", backend.last_error());
    return 2;
  }

  backend.add_point({-1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f});
  backend.add_point({0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.1f});
  backend.add_point({1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.2f});

  if (!backend.end_stroke()) {
    std::fprintf(stderr, "stroke failed: %s\n", backend.last_error());
    return 3;
  }

  if (!backend.render()) {
    std::fprintf(stderr, "frame 1 GP render preparation failed: %s\n", backend.last_error());
    return 4;
  }

  if (!backend.create_frame(2) || !backend.begin_stroke({0, 4.0f})) {
    std::fprintf(stderr, "frame 2 setup failed: %s\n", backend.last_error());
    return 5;
  }

  backend.add_point({-0.5f, -0.25f, 0.0f, 0.8f, 1.0f, 0.0f});
  backend.add_point({0.5f, 0.25f, 0.0f, 0.9f, 1.0f, 0.1f});

  if (!backend.end_stroke() || !backend.render()) {
    std::fprintf(stderr, "frame 2 GP render preparation failed: %s\n", backend.last_error());
    return 6;
  }

  std::puts("persistent Blender GP backend multi-frame test passed");
  return 0;
}
