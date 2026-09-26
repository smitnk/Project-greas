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

  std::fprintf(stderr, "[TRANSFORM] flip duplicated stroke\n");
  if (!backend.flip_stroke(1)) {
    std::fprintf(stderr, "stroke flip failed: %s\n", backend.last_error());
    return 21;
  }

  project_grease::gp::StrokePoint flipped_first{};
  project_grease::gp::StrokePoint flipped_last{};
  if (!backend.get_point(1, 0, &flipped_first) ||
      !backend.get_point(1, 1, &flipped_last) ||
      flipped_first.x != 1.0f ||
      flipped_first.y != 0.0f ||
      flipped_first.z != 0.1f ||
      flipped_first.pressure != 0.9f ||
      flipped_first.strength != 1.0f ||
      flipped_first.time != 0.1f ||
      flipped_last.x != 1.25f ||
      flipped_last.y != 0.65f ||
      flipped_last.z != 0.1f ||
      flipped_last.pressure != edited.pressure ||
      flipped_last.strength != edited.strength ||
      flipped_last.time != edited.time ||
      !backend.render()) {
    std::fprintf(stderr, "flipped stroke/cache invalidation failed: %s\n",
                 backend.last_error());
    return 22;
  }

  std::fprintf(stderr, "[TRANSFORM] stroke flip/render passed\n");

  std::fprintf(stderr, "[SUBDIVIDE] subdivide flipped duplicated stroke\n");
  if (!backend.subdivide_stroke(1, 1) ||
      backend.point_count() != 5) {
    std::fprintf(stderr, "stroke subdivision failed: %s\\n", backend.last_error());
    return 23;
  }

  project_grease::gp::StrokePoint subdivided_mid{};
  if (!backend.get_point(1, 1, &subdivided_mid) ||
      subdivided_mid.x != 1.125f ||
      subdivided_mid.y != 0.325f ||
      subdivided_mid.z != 0.1f ||
      subdivided_mid.pressure != 0.775f ||
      subdivided_mid.strength != 0.9f ||
      subdivided_mid.time != 0.0f ||
      !backend.render()) {
    std::fprintf(stderr, "subdivided stroke/cache invalidation failed: %s\\n", backend.last_error());
    return 24;
  }

  std::fprintf(stderr, "[SUBDIVIDE] stroke subdivision/render passed\\n");

  std::fprintf(stderr, "[CLOSE] close subdivided duplicated stroke\\n");
  if (!backend.close_stroke(1) || backend.point_count() != 5) {
    std::fprintf(stderr, "stroke close failed: %s\\n", backend.last_error());
    return 25;
  }

  project_grease::gp::StrokePoint close_point{};
  if (!backend.get_point(1, 4, &close_point) ||
      close_point.x <= 1.0f || close_point.x >= 1.01f ||
      close_point.y <= 0.0f || close_point.y >= 0.01f ||
      close_point.z != 0.1f ||
      !backend.render()) {
    std::fprintf(stderr, "closed stroke/cache invalidation failed: %s\\n", backend.last_error());
    return 26;
  }

  std::fprintf(stderr, "[CLOSE] stroke close/render passed\\n");

  std::fprintf(stderr, "[POINTS] trim closed duplicated stroke to first point\\n");
  if (!backend.trim_stroke_points(1, 0, 0, true) ||
      backend.point_count() != 3) {
    std::fprintf(stderr, "stroke point trim failed: %s\n", backend.last_error());
    return 19;
  }

  project_grease::gp::StrokePoint trimmed_point{};
  project_grease::gp::StrokePoint unexpected_point{};
  if (!backend.get_point(1, 0, &trimmed_point) ||
      backend.get_point(1, 1, &unexpected_point) ||
      trimmed_point.x != 1.0f ||
      trimmed_point.y != 0.0f ||
      trimmed_point.z != 0.1f ||
      trimmed_point.pressure != 0.9f ||
      trimmed_point.strength != 1.0f ||
      trimmed_point.time != 0.1f ||
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
