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

  if (backend.frame_count() != 2 || backend.stroke_count() != 1 ||
      backend.point_count() != 2) {
    std::fprintf(stderr,
                 "layer 1/frame 2 inspection failed: frames=%d strokes=%d points=%d\n",
                 backend.frame_count(), backend.stroke_count(), backend.point_count());
    return 7;
  }

  // Create a second layer without destroying the first layer or GPU session.
  if (!backend.create_layer("Layer 2") ||
      !backend.create_frame(1) ||
      !backend.begin_stroke({0, 2.0f})) {
    std::fprintf(stderr, "layer 2 setup failed: %s\n", backend.last_error());
    return 8;
  }

  backend.add_point({0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f});
  backend.add_point({0.25f, 0.25f, 0.0f, 1.0f, 1.0f, 0.1f});

  if (!backend.end_stroke() || !backend.render()) {
    std::fprintf(stderr, "layer 2 render preparation failed: %s\n", backend.last_error());
    return 9;
  }

  if (backend.layer_count() != 2 || backend.frame_count() != 1 ||
      backend.stroke_count() != 1 || backend.point_count() != 2) {
    std::fprintf(stderr,
                 "layer 2 inspection failed: layers=%d frames=%d strokes=%d points=%d\n",
                 backend.layer_count(), backend.frame_count(),
                 backend.stroke_count(), backend.point_count());
    return 10;
  }

  // Switch back to layer 1 and frame 2 without rebuilding the document.
  if (!backend.select_layer(0) ||
      !backend.select_frame(2) ||
      backend.frame_count() != 2 ||
      backend.stroke_count() != 1 ||
      backend.point_count() != 2 ||
      !backend.render()) {
    std::fprintf(stderr, "layer/frame selection failed: %s\n", backend.last_error());
    return 11;
  }

  // Exercise editing on an existing Blender GP stroke after frame selection.
  if (!backend.select_stroke(0)) {
    std::fprintf(stderr, "stroke selection failed: %s\n", backend.last_error());
    return 12;
  }

  project_grease::gp::StrokePoint before{};
  project_grease::gp::StrokePoint edited{
      0.75f, 0.9f, 0.0f, 0.65f, 0.8f, 0.25f};
  if (!backend.get_point(0, 0, &before) ||
      !backend.set_point(0, 0, edited) ||
      !backend.get_point(0, 0, &before) ||
      before.x != edited.x || before.y != edited.y ||
      before.pressure != edited.pressure || before.strength != edited.strength ||
      !backend.render()) {
    std::fprintf(stderr, "stroke point edit/cache invalidation failed: %s\n",
                 backend.last_error());
    return 13;
  }

  // Add a second stroke, then remove it through the native GP list.
  if (!backend.begin_stroke({0, 2.0f})) {
    std::fprintf(stderr, "second stroke setup failed: %s\n", backend.last_error());
    return 14;
  }
  backend.add_point({1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f});
  backend.add_point({1.5f, 1.5f, 0.0f, 1.0f, 1.0f, 0.1f});
  if (!backend.end_stroke() || backend.stroke_count() != 2 ||
      !backend.delete_stroke(1) || backend.stroke_count() != 1 ||
      !backend.render()) {
    std::fprintf(stderr, "stroke deletion/cache invalidation failed: %s\n",
                 backend.last_error());
    return 15;
  }

  std::puts("Blender legacy GP stroke editing and cache invalidation test passed");
  return 0;
}
