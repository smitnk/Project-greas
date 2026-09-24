#include "project_grease_gp_backend.h"

#include <cstdio>

#include "BKE_gpencil_legacy.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *gpd);
extern "C" void GPENCIL_engine_init(void *ved);

int main() {
  // Force the linker to pull the real GP draw-cache and GP engine objects.
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

  std::puts("native legacy GP stroke creation/link test reached");
  return 0;
}
