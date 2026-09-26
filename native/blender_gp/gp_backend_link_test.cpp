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
  std::fprintf(stderr, "[EDIT] select_stroke\
");
  if (!backend.select_stroke(0)) {
    std::fprintf(stderr, "stroke selection failed: %s\n", backend.last_error());
    return 12;
  }

  std::fprintf(stderr, "[EDIT] point read/write\
");
  project_grease::gp::StrokePoint before{};
  project_grease::gp::StrokePoint edited{
      0.75f, 0.9f, 0.0f, 0.65f, 0.8f, 0.25f};
  if (!backend.get_point(0, 0, &before) ||
      before.x != -0.5f || before.y != -0.25f ||
      before.z != 0.0f || before.pressure != 0.8f ||
      before.strength != 1.0f || before.time != 0.0f ||
      !backend.set_point(0, 0, edited) ||
      !backend.get_point(0, 0, &before) ||
      before.x != edited.x || before.y != edited.y ||
      before.z != edited.z || before.pressure != edited.pressure ||
      before.strength != edited.strength || before.time != edited.time ||
      !backend.render()) {
    std::fprintf(stderr, "stroke point edit/cache invalidation failed: %s\n",
                 backend.last_error());
    return 13;
  }

  std::fprintf(stderr, "[EDIT] point edit render passed\
");

  // Duplicate the edited stroke through Blender's native GP API.
  std::fprintf(stderr, "[DUPLICATE] duplicate edited stroke\n");
  if (!backend.duplicate_stroke(0) || backend.stroke_count() != 2) {
    std::fprintf(stderr, "stroke duplication failed: %s\n", backend.last_error());
    return 14;
  }

  project_grease::gp::StrokePoint duplicate_point{};
  if (!backend.get_point(1, 0, &duplicate_point) ||
      duplicate_point.x != edited.x ||
      duplicate_point.y != edited.y ||
      duplicate_point.z != edited.z ||
      duplicate_point.pressure != edited.pressure ||
      duplicate_point.strength != edited.strength ||
      duplicate_point.time != edited.time ||
      !backend.render()) {
    std::fprintf(stderr, "duplicated stroke data/cache invalidation failed: %s\n",
                 backend.last_error());
    return 15;
  }


  std::fprintf(stderr, "[TRANSFORM] translate duplicated stroke\n");
  if (!backend.translate_stroke(1, 0.5f, -0.25f, 0.1f)) {
    std::fprintf(stderr, "stroke translation failed: %s\n", backend.last_error());
    return 16;
  }

  project_grease::gp::StrokePoint translated_point{};
  if (!backend.get_point(1, 0, &translated_point) ||
      translated_point.x != 1.25f ||
      translated_point.y != 0.65f ||
      translated_point.z != 0.1f ||
      translated_point.pressure != edited.pressure ||
      translated_point.strength != edited.strength ||
      translated_point.time != edited.time ||
      !backend.render()) {
    std::fprintf(stderr, "translated stroke/cache invalidation failed: %s\n",
                 backend.last_error());
    return 17;
  }

  std::fprintf(stderr, "[TRANSFORM] stroke translation/render passed\n");


  std::fprintf(stderr, "[POINTS] trim duplicated stroke to first point\n");
  if (!backend.trim_stroke_points(1, 0, 0, true) ||
      backend.point_count() != 1) {
    std::fprintf(stderr, "stroke point trim failed: %s\n", backend.last_error());
    return 19;
  }

  project_grease::gp::StrokePoint trimmed_point{};
  if (!backend.get_point(1, 0, &trimmed_point) ||
      trimmed_point.x != 1.25f ||
      trimmed_point.y != 0.65f ||
      trimmed_point.z != 0.1f ||
      trimmed_point.pressure != edited.pressure ||
      trimmed_point.strength != edited.strength ||
      trimmed_point.time != edited.time ||
      !backend.render()) {
    std::fprintf(stderr, "trimmed stroke/cache invalidation failed: %s\n",
                 backend.last_error());
    return 20;
  }

  std::fprintf(stderr, "[POINTS] stroke point trim/render passed\n");

  std::fprintf(stderr, "[DELETE] delete duplicated stroke\n");
  if (!backend.delete_stroke(1) || backend.stroke_count() != 1 ||
      !backend.render()) {
    std::fprintf(stderr, "stroke deletion/cache invalidation failed: %s\n",
                 backend.last_error());
    return 18;
  }

  std::fprintf(stderr, "[DONE] edit/duplicate/translate/delete operations passed\n");
  std::puts("Blender legacy GP stroke edit/duplicate/translate/delete test passed");
  std::fflush(stdout);
  return 0;
}
