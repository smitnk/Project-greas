#include "project_grease_gp_backend.h"
#include "project_grease_legacy_primitive.h"
#include "project_grease_legacy_fill.h"

#include <cmath>
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

  std::fprintf(stderr, "[MODIFIER] invoke Blender 3.6.23 Legacy GP Smooth modifier\\n");
  project_grease::gp::StrokePoint modifier_before{};
  if (!backend.get_point(0, 0, &modifier_before) ||
      !backend.apply_blender_modifier(0, eGpencilModifierType_Smooth, 0.5f, 2) ||
      !backend.get_point(0, 0, &edited) ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Smooth modifier failed: %s\\n", backend.last_error());
    return 43;
  }
  if (edited.x == modifier_before.x && edited.y == modifier_before.y) {
    std::fprintf(stderr, "real Blender GP Smooth modifier made no geometry change\\n");
    return 44;
  }
  std::fprintf(stderr, "[MODIFIER] real Blender Legacy GP Smooth callback passed\\n");

  std::fprintf(stderr, "[LASSO] Legacy GP lasso selection\\n");
  const float lasso[] = {
      0.60f, 0.75f,
      0.90f, 0.75f,
      0.90f, 1.05f,
      0.60f, 1.05f,
  };
  if (backend.lasso_select(lasso, 4, false) != 1 ||
      !backend.translate_stroke(0, 0.1f, -0.1f, 0.0f) ||
      !backend.get_point(0, 0, &before) ||
      before.x != 0.85f || before.y != 0.8f ||
      !backend.get_point(0, 1, &edited) ||
      edited.x != 0.5f || edited.y != 0.25f ||
      !backend.translate_stroke(0, -0.1f, 0.1f, 0.0f) ||
      !backend.get_point(0, 0, &before) ||
      before.x != 0.75f || before.y != 0.9f ||
      !backend.render()) {
    std::fprintf(stderr, "Legacy GP lasso point-selection semantics failed: %s\\n",
                 backend.last_error());
    return 33;
  }
  std::fprintf(stderr, "[LASSO] Legacy GP lasso selection/render passed\\n");

  // Duplicate the edited stroke through Blender's native GP API.
  std::fprintf(stderr, "[PRIMITIVE] Legacy GP primitive geometry conformance\\n");
  {
    using project_grease::legacy_gp_primitive::Point;
    using project_grease::legacy_gp_primitive::generate;
    const Point start{0.0f, 0.0f};
    const Point end{2.0f, 1.0f};
    const float eps = 1.0e-5f;

    const auto line = generate(0, start, end, 0.0f, 0.0f, 6);
    if (line.size() != 6 || line.front().x != 0.0f || line.front().y != 0.0f ||
        std::fabs(line.back().x - 2.0f) > eps ||
        std::fabs(line.back().y - 1.0f) > eps ||
        std::fabs(line[3].x - 1.2f) > eps ||
        std::fabs(line[3].y - 0.6f) > eps) {
      std::fprintf(stderr, "Legacy GP line primitive mismatch\\n");
      return 34;
    }

    const auto rect = generate(1, start, end, 0.0f, 0.0f, 1);
    if (rect.size() != 4 ||
        rect[0].x != 0.0f || rect[0].y != 0.0f ||
        rect[1].x != 2.0f || rect[1].y != 0.0f ||
        rect[2].x != 2.0f || rect[2].y != 1.0f ||
        rect[3].x != 0.0f || rect[3].y != 1.0f) {
      std::fprintf(stderr, "Legacy GP rectangle primitive mismatch\\n");
      return 35;
    }

    const auto circle = generate(2, start, end, 0.0f, 0.0f, 8);
    if (circle.size() != 8 ||
        std::fabs(circle[0].x - 2.0f) > eps ||
        std::fabs(circle[0].y - 0.5f) > eps ||
        std::fabs(circle[2].x - 1.0f) > eps ||
        std::fabs(circle[2].y - 1.0f) > eps) {
      std::fprintf(stderr, "Legacy GP circle primitive mismatch\\n");
      return 36;
    }

    const auto arc = generate(3, {0.0f, 0.0f}, {2.0f, 0.0f}, 0.0f, 0.0f, 5);
    if (arc.size() != 5 ||
        std::fabs(arc.front().x - 0.0f) > eps ||
        std::fabs(arc.front().y - 0.0f) > eps ||
        std::fabs(arc[2].x - 1.0f) > eps ||
        std::fabs(arc[2].y + 0.41421356f) > eps ||
        std::fabs(arc.back().x - 2.0f) > eps ||
        std::fabs(arc.back().y) > eps) {
      std::fprintf(stderr, "Legacy GP arc primitive mismatch\\n");
      return 37;
    }
  }
  std::fprintf(stderr, "[PRIMITIVE] Legacy GP primitive geometry conformance passed\\n");

  std::fprintf(stderr, "[FILL] Legacy GP boundary fill conformance\\n");
  {
    using project_grease::legacy_gp_fill::Image;
    using project_grease::legacy_gp_fill::run;

    Image closed(9, 9);
    auto mark = [&closed](int x, int y) {
      float *p = closed.pixel(y * closed.width() + x);
      p[0] = 1.0f;
      p[3] = 1.0f;
    };
    for (int x = 2; x <= 6; ++x) {
      mark(x, 2);
      mark(x, 6);
    }
    for (int y = 2; y <= 6; ++y) {
      mark(2, y);
      mark(6, y);
    }

    const auto closed_result = run(closed, 4, 4, 1, 0);
    if (!closed_result.valid || closed_result.border_contact ||
        closed_result.outline.size() < 4) {
      std::fprintf(stderr, "Legacy GP closed boundary fill mismatch\\n");
      return 38;
    }

    Image open(9, 9);
    auto mark_open = [&open](int x, int y) {
      float *p = open.pixel(y * open.width() + x);
      p[0] = 1.0f;
      p[3] = 1.0f;
    };
    for (int x = 2; x <= 6; ++x) {
      mark_open(x, 2);
      if (x != 4) {
        mark_open(x, 6);
      }
    }
    for (int y = 2; y <= 6; ++y) {
      mark_open(2, y);
      mark_open(6, y);
    }

    const auto open_result = run(open, 4, 4, 1, 0);
    if (open_result.valid || !open_result.border_contact) {
      std::fprintf(stderr, "Legacy GP open boundary leak detection mismatch\\n");
      return 39;
    }
  }
  std::fprintf(stderr, "[FILL] Legacy GP boundary fill conformance passed\\n");

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
  if (!backend.close_stroke(1)) {
    std::fprintf(stderr, "stroke close operation failed: %s\\n", backend.last_error());
    return 25;
  }
  if (backend.point_count() != 7) {
    std::fprintf(stderr,
                 "stroke close point count mismatch: got %d expected 7\\n",
                 backend.point_count());
    return 26;
  }

  project_grease::gp::StrokePoint close_point{};
  // BKE_gpencil_stroke_close() inserts two points here. Point 3 is the
  // halfway interpolation; point 4 is the final near-start point.
  if (!backend.get_point(1, 4, &close_point) ||
      close_point.x <= 1.0f || close_point.x >= 1.01f ||
      close_point.y <= 0.0f || close_point.y >= 0.01f ||
      close_point.z != 0.1f ||
      !backend.render()) {
    std::fprintf(stderr, "closed stroke/cache invalidation failed: %s\\n", backend.last_error());
    return 27;
  }

  std::fprintf(stderr, "[CLOSE] stroke close/render passed\\n");

  std::fprintf(stderr, "[POINTS] trim closed duplicated stroke to first point\\n");
  if (!backend.trim_stroke_points(1, 0, 0, true) ||
      backend.point_count() != 3) {
    std::fprintf(stderr, "stroke point trim failed: %s\n", backend.last_error());
    return 28;
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

  std::fprintf(stderr, "[TRIM] create self-intersecting stroke\\n");
  if (!backend.begin_stroke({0, 5.0f})) {
    std::fprintf(stderr, "trim setup failed: %s\\n", backend.last_error());
    return 41;
  }
  backend.add_point({6.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f});
  backend.add_point({8.0f, 2.0f, 0.0f, 0.9f, 0.9f, 0.1f});
  backend.add_point({6.0f, 4.0f, 0.0f, 0.8f, 0.8f, 0.2f});
  backend.add_point({8.0f, 0.0f, 0.0f, 0.7f, 0.7f, 0.3f});
  backend.add_point({9.0f, 1.0f, 0.0f, 0.6f, 0.6f, 0.4f});
  if (!backend.end_stroke()) {
    std::fprintf(stderr, "trim setup end_stroke failed: %s\\n", backend.last_error());
    return 41;
  }
  const int trim_index = backend.stroke_count() - 1;
  const int trim_points_before = backend.point_count();
  if (!backend.trim_stroke(trim_index) ||
      backend.point_count() >= trim_points_before ||
      !backend.render()) {
    std::fprintf(stderr, "Legacy GP intersection trim failed: %s\\n", backend.last_error());
    return 42;
  }
  std::fprintf(stderr, "[TRIM] Legacy GP first-intersection trim/render passed\\n");

  std::fprintf(stderr, "[DELETE] delete duplicated stroke\n");
  if (!backend.delete_stroke(1) || backend.stroke_count() != 1 ||
      !backend.render()) {
    std::fprintf(stderr, "stroke deletion/cache invalidation failed: %s\n",
                 backend.last_error());
    return 18;
  }

  // Create a fresh five-point stroke and split it at the shared middle point.
  std::fprintf(stderr, "[SPLIT] create five-point stroke\n");
  if (!backend.begin_stroke({0, 5.0f})) {
    std::fprintf(stderr, "split setup failed: %s\\n", backend.last_error());
    return 30;
  }
  backend.add_point({2.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f});
  backend.add_point({2.5f, 0.5f, 0.0f, 0.9f, 0.9f, 0.1f});
  backend.add_point({3.0f, 0.0f, 0.0f, 0.8f, 0.8f, 0.2f});
  backend.add_point({3.5f, -0.5f, 0.0f, 0.7f, 0.7f, 0.3f});
  backend.add_point({4.0f, 0.0f, 0.0f, 0.6f, 0.6f, 0.4f});

  if (!backend.end_stroke()) {
    std::fprintf(stderr, "split setup end_stroke failed: %s\\n", backend.last_error());
    return 30;
  }
  std::fprintf(stderr, "[SPLIT] setup counts: strokes=%d points=%d\\n",
               backend.stroke_count(), backend.point_count());
  if (backend.stroke_count() != 2 || backend.point_count() != 7) {
    std::fprintf(stderr,
                 "split setup count mismatch: strokes=%d points=%d\\n",
                 backend.stroke_count(), backend.point_count());
    return 30;
  }
  if (!backend.render()) {
    std::fprintf(stderr, "split setup render failed: %s\\n", backend.last_error());
    return 30;
  }

  std::fprintf(stderr, "[SPLIT] split at shared middle point\\n");
  if (!backend.split_stroke(1, 2) ||
      backend.stroke_count() != 3 ||
      backend.point_count() != 8 ||
      !backend.render()) {
    std::fprintf(stderr, "stroke split failed: %s\\n", backend.last_error());
    return 31;
  }

  project_grease::gp::StrokePoint split_left{};
  project_grease::gp::StrokePoint split_right{};
  // Blender splits before index 2: left keeps P0,P1; right receives P2,P3,P4.
  if (!backend.get_point(1, 1, &split_left) ||
      !backend.get_point(2, 0, &split_right) ||
      split_left.x != 2.5f || split_left.y != 0.5f ||
      split_left.pressure != 0.9f || split_left.time != 0.1f ||
      split_right.x != 3.0f || split_right.y != 0.0f ||
      split_right.pressure != 0.8f || split_right.time != 0.2f) {
    std::fprintf(stderr, "split point partition failed: %s\\n", backend.last_error());
    return 32;
  }

  std::fprintf(stderr, "[SPLIT] stroke split/render passed\\n");


  std::fprintf(stderr, "[FILL] Legacy GP end-to-end fill path\n");
  {
    constexpr int width = 16;
    constexpr int height = 16;
    float rgba[width * height * 4] = {};
    auto mark_boundary = [&](int x, int y) {
      const int i = (y * width + x) * 4;
      rgba[i + 0] = 1.0f;
      rgba[i + 1] = 1.0f;
      rgba[i + 2] = 1.0f;
      rgba[i + 3] = 1.0f;
    };
    for (int x = 4; x <= 11; ++x) {
      mark_boundary(x, 4);
      mark_boundary(x, 11);
    }
    for (int y = 4; y <= 11; ++y) {
      mark_boundary(4, y);
      mark_boundary(11, y);
    }
    const int before_strokes = backend.stroke_count();
    if (!backend.fill_at_screen(rgba, width, height, 7, 7, 1, 0, {0, 2.0f}) ||
        backend.stroke_count() != before_strokes + 1 ||
        !backend.fill_stroke(backend.stroke_count() - 1) ||
        !backend.render()) {
      std::fprintf(stderr, "Legacy GP end-to-end fill path failed: %s\n", backend.last_error());
      return 40;
    }
  }
  std::fprintf(stderr, "[FILL] Legacy GP end-to-end fill/render passed\n");

  std::fprintf(stderr, "[ERASER] real Legacy GP hard eraser test\n");
  if (!backend.begin_stroke({20.0f, 20.0f}) ||
      !backend.add_point({30.0f, 20.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({40.0f, 20.0f, 0.0f, 1.0f, 1.0f, 0.1f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "eraser setup failed: %s\n", backend.last_error());
    return 47;
  }
  const int eraser_stroke = backend.stroke_count() - 1;
  const int eraser_points_before = backend.point_count();
  if (!backend.erase_at(30.0f, 20.0f, 6.0f) ||
      backend.stroke_count() != eraser_stroke + 1 ||
      backend.point_count() != eraser_points_before - 1 ||
      !backend.render()) {
    std::fprintf(stderr, "Legacy GP eraser edit/cache invalidation failed: %s\n",
                 backend.last_error());
    return 48;
  }
  std::fprintf(stderr, "[ERASER] real Legacy GP erase/render passed\n");

  std::fprintf(stderr, "[LASSO] real Legacy GP lasso selection test\n");
  const float lasso[] = {15.0f, 15.0f, 45.0f, 15.0f, 45.0f, 25.0f, 15.0f, 25.0f};
  backend.clear_selection();
  if (backend.lasso_select(lasso, 4, false) != 1 ||
      !backend.get_point(eraser_stroke, 0, &translated_point) ||
      !backend.render()) {
    std::fprintf(stderr, "Legacy GP lasso selection failed: %s\n", backend.last_error());
    return 49;
  }
  std::fprintf(stderr, "[LASSO] real Legacy GP lasso selection/render passed\n");

  std::fprintf(stderr, "[BULK-EDIT] Legacy GP selection/edit command suite\n");
  if (!backend.begin_stroke({0, 7.0f}) ||
      !backend.add_point({20.0f, 30.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({21.0f, 30.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.add_point({22.0f, 30.0f, 0.0f, 0.8f, 0.8f, 0.2f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "bulk edit setup failed: %s\n", backend.last_error());
    return 50;
  }
  if (!backend.select_all(1) ||
      !backend.reverse_selected_strokes() ||
      backend.select_circle(21.0f, 30.0f, 2.0f, 0) <= 0 ||
      !backend.merge_selected_points(0.001f) ||
      !backend.reorder_selected_strokes(0) ||
      !backend.render()) {
    std::fprintf(stderr, "bulk selection/reverse/merge/reorder failed: %s\n",
                 backend.last_error());
    return 51;
  }
  backend.clear_selection();
  if (backend.select_circle(21.0f, 30.0f, 0.2f, 0) <= 0 ||
      !backend.dissolve_selected_points() ||
      !backend.render()) {
    std::fprintf(stderr, "bulk circle/dissolve failed: %s\n", backend.last_error());
    return 52;
  }
  std::fprintf(stderr, "[BULK-EDIT] selection/reverse/circle/dissolve/merge/reorder passed\n");

  std::fprintf(stderr, "[LAYER-2] join/select-first/grouped Legacy GP conformance\\n");
  if (!backend.begin_stroke({0, 6.0f}) ||
      !backend.add_point({30.0f, 40.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({31.0f, 40.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.end_stroke() ||
      !backend.begin_stroke({0, 6.0f}) ||
      !backend.add_point({32.0f, 40.0f, 0.0f, 0.8f, 0.8f, 0.2f}) ||
      !backend.add_point({33.0f, 40.0f, 0.0f, 0.7f, 0.7f, 0.3f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "layer-2 stroke setup failed: %s\\n", backend.last_error());
    return 53;
  }
  const int layer2_before = backend.stroke_count();
  if (!backend.select_all(1) || !backend.join_selected_strokes() ||
      backend.stroke_count() != layer2_before - 1 || !backend.render()) {
    std::fprintf(stderr, "Legacy GP join selected strokes failed: %s\\n", backend.last_error());
    return 54;
  }
  backend.clear_selection();
  if (!backend.select_first_points(false, false) || !backend.render()) {
    std::fprintf(stderr, "Legacy GP select first points failed: %s\\n", backend.last_error());
    return 55;
  }
  if (!backend.select_grouped(0) || !backend.select_grouped(1) || !backend.render()) {
    std::fprintf(stderr, "Legacy GP grouped selection failed: %s\\n", backend.last_error());
    return 56;
  }
  std::fprintf(stderr, "[LAYER-2] join/select-first/grouped passed\\n");

  std::fprintf(stderr, "[HISTORY] real Legacy GP undo/redo snapshot test\n");
  if (!backend.history_reset()) {
    std::fprintf(stderr, "history reset failed: %s\n", backend.last_error());
    return 43;
  }

  const int history_baseline_strokes = backend.stroke_count();
  if (!backend.begin_stroke({0, 6.0f}) ||
      !backend.add_point({12.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({13.0f, 2.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.end_stroke() ||
      backend.stroke_count() != history_baseline_strokes + 1 ||
      !backend.history_record() ||
      !backend.history_can_undo() ||
      backend.history_can_redo()) {
    std::fprintf(stderr, "history record failed: %s\n", backend.last_error());
    return 44;
  }

  if (!backend.history_undo() ||
      backend.stroke_count() != history_baseline_strokes ||
      !backend.history_can_redo() ||
      backend.history_can_undo()) {
    std::fprintf(stderr, "history undo failed: %s\n", backend.last_error());
    return 45;
  }

  if (!backend.history_redo() ||
      backend.stroke_count() != history_baseline_strokes + 1 ||
      !backend.history_can_undo() ||
      backend.history_can_redo() ||
      !backend.render()) {
    std::fprintf(stderr, "history redo failed: %s\n", backend.last_error());
    return 46;
  }
  std::fprintf(stderr, "[HISTORY] real Legacy GP undo/redo snapshot/render passed\n");


  std::fprintf(stderr, "[PAINT-STAGE] Blender Legacy GP brush commit smoothing\n");
  project_grease::gp::Backend::LegacyPaintSettings paint_settings{};
  paint_settings.draw_smooth_level = 2;
  paint_settings.draw_smooth_factor = 0.5f;
  paint_settings.input_samples = 5;
  paint_settings.smooth_position = true;
  paint_settings.smooth_strength = true;
  if (!backend.set_legacy_paint_settings(paint_settings) ||
      !backend.begin_stroke({0, 7.0f}) ||
      !backend.add_point({40.0f, 50.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({41.0f, 52.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.add_point({42.0f, 50.0f, 0.0f, 0.8f, 0.8f, 0.2f}) ||
      !backend.add_point({43.0f, 52.0f, 0.0f, 0.7f, 0.7f, 0.3f}) ||
      !backend.end_stroke() ||
      backend.stroke_buffer_count() != 0 ||
      !backend.render()) {
    std::fprintf(stderr, "Legacy GP paint-stage smoothing failed: %s\n",
                 backend.last_error());
    return 68;
  }
  if (!backend.set_legacy_paint_settings({})) {
    std::fprintf(stderr, "Legacy GP paint settings reset failed: %s\n",
                 backend.last_error());
    return 69;
  }
  std::fprintf(stderr, "[PAINT-STAGE] brush smoothing/input-sample callbacks passed\n");

  std::fprintf(stderr, "[MODIFIER] real Blender 3.6.23 Legacy GP Simplify modifier\\n");
  if (!backend.begin_stroke({0, 9.0f}) ||
      !backend.add_point({45.0f, 30.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({46.0f, 30.02f, 0.0f, 1.0f, 0.95f, 0.1f}) ||
      !backend.add_point({47.0f, 29.98f, 0.0f, 1.0f, 0.9f, 0.2f}) ||
      !backend.add_point({48.0f, 30.01f, 0.0f, 1.0f, 0.85f, 0.3f}) ||
      !backend.add_point({49.0f, 30.0f, 0.0f, 1.0f, 0.8f, 0.4f}) ||
      !backend.add_point({50.0f, 30.02f, 0.0f, 1.0f, 0.75f, 0.5f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "Simplify modifier setup failed: %s\\n", backend.last_error());
    return 70;
  }
  const int simplify_index = backend.stroke_count() - 1;
  const int simplify_before = backend.point_count();
  if (!backend.apply_blender_modifier(
          simplify_index, eGpencilModifierType_Simplify, 0.1f, 1) ||
      backend.point_count() >= simplify_before ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Simplify modifier failed: %s\\n",
                 backend.last_error());
    return 71;
  }
  std::fprintf(stderr, "[MODIFIER] real Blender Legacy GP Simplify callback passed\\n");

  std::fprintf(stderr, "[MODIFIER] real Blender 3.6.23 Legacy GP Length modifier\\n");
  if (!backend.begin_stroke({0, 11.0f}) ||
      !backend.add_point({55.0f, 40.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({56.0f, 40.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.add_point({57.0f, 40.0f, 0.0f, 0.8f, 0.8f, 0.2f}) ||
      !backend.add_point({58.0f, 40.0f, 0.0f, 0.7f, 0.7f, 0.3f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "Length modifier setup failed: %s\\n", backend.last_error());
    return 72;
  }
  const int length_index = backend.stroke_count() - 1;
  project_grease::gp::StrokePoint length_before{};
  if (!backend.get_point(length_index, 0, &length_before) ||
      !backend.apply_blender_modifier(
          length_index, eGpencilModifierType_Length, 0.25f, 1) ||
      !backend.get_point(length_index, 0, &edited) ||
      (edited.x == length_before.x && edited.y == length_before.y) ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Length modifier failed: %s\\n",
                 backend.last_error());
    return 73;
  }
  std::fprintf(stderr, "[MODIFIER] real Blender Legacy GP Length callback passed\\n");

  std::fprintf(stderr, "[MODIFIER] real Blender 3.6.23 Legacy GP Opacity modifier\\n");
  if (!backend.begin_stroke({0, 12.0f}) ||
      !backend.add_point({59.0f, 45.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({60.0f, 46.0f, 0.0f, 0.8f, 0.8f, 0.1f}) ||
      !backend.add_point({61.0f, 45.0f, 0.0f, 0.6f, 0.6f, 0.2f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "Opacity modifier setup failed: %s\\n", backend.last_error());
    return 74;
  }
  const int opacity_index = backend.stroke_count() - 1;
  project_grease::gp::StrokePoint opacity_before{};
  if (!backend.get_point(opacity_index, 0, &opacity_before) ||
      !backend.apply_blender_modifier(
          opacity_index, eGpencilModifierType_Opacity, 0.5f, 1) ||
      !backend.get_point(opacity_index, 0, &edited) ||
      edited.strength >= opacity_before.strength ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Opacity modifier failed: %s\\n",
                 backend.last_error());
    return 75;
  }
  std::fprintf(stderr, "[MODIFIER] real Blender Legacy GP Opacity callback passed\\n");

  std::fprintf(stderr, "[MODIFIER] real Blender 3.6.23 Legacy GP Color modifier\\n");
  if (!backend.begin_stroke({0, 13.0f}) ||
      !backend.add_point({63.0f, 45.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({64.0f, 46.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.add_point({65.0f, 45.0f, 0.0f, 0.8f, 0.8f, 0.2f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "Color modifier setup failed: %s\\n", backend.last_error());
    return 76;
  }
  const int color_index = backend.stroke_count() - 1;
  project_grease::gp::StrokePoint color_before{};
  if (!backend.get_point(color_index, 0, &color_before) ||
      !backend.apply_blender_modifier(
          color_index, eGpencilModifierType_Color, 0.2f, 1) ||
      !backend.get_point(color_index, 0, &edited) ||
      std::fabs(edited.r - color_before.r) < 1.0e-5f ||
      std::fabs(edited.g - color_before.g) < 1.0e-5f ||
      std::fabs(edited.b - color_before.b) < 1.0e-5f ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Color modifier failed: %s\\n",
                 backend.last_error());
    return 77;
  }
  std::fprintf(stderr, "[MODIFIER] real Blender Legacy GP Color callback passed\\n");

  std::fprintf(stderr, "[MODIFIER-STACK] real Blender Legacy GP modifier stack\\n");
  if (!backend.begin_stroke({0, 10.0f}) ||
      !backend.add_point({50.0f, 10.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({51.0f, 12.0f, 0.0f, 0.8f, 0.9f, 0.1f}) ||
      !backend.add_point({52.0f, 10.0f, 0.0f, 0.6f, 0.8f, 0.2f}) ||
      !backend.add_point({53.0f, 12.0f, 0.0f, 0.4f, 0.7f, 0.3f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "modifier stack setup failed: %s\\n", backend.last_error());
    return 57;
  }

  /*
   * end_stroke() commits the real Blender tGPspoint buffer into bGPDstroke
   * data and clears the temporary editor buffer, matching Blender's paint
   * lifecycle. Validate the committed stroke rather than expecting sbuffer
   * points to survive the commit.
   */
  if (backend.stroke_buffer_count() != 0) {
    std::fprintf(stderr, "real Legacy GP sbuffer was not cleared after commit: %d\\n",
                 backend.stroke_buffer_count());
    return 61;
  }
  const int stack_index = backend.stroke_count() - 1;
  const int stack_points_before = backend.point_count();
  project_grease::gp::StrokePoint stack_before{};
  if (!backend.get_point(stack_index, 1, &stack_before)) {
    std::fprintf(stderr, "modifier stack point read failed: %s\\n", backend.last_error());
    return 58;
  }

  const int modifier_types[] = {
      eGpencilModifierType_Smooth,
      eGpencilModifierType_Thick,
      eGpencilModifierType_Subdiv,
  };
  if (!backend.apply_blender_modifier_stack(
          stack_index, modifier_types, 3, 0.5f, 1) ||
      backend.point_count() <= stack_points_before ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender Legacy GP modifier stack failed: %s\\n",
                 backend.last_error());
    return 59;
  }

  project_grease::gp::StrokePoint stack_after{};
  if (!backend.get_point(stack_index, 1, &stack_after) ||
      stack_after.pressure >= stack_before.pressure) {
    std::fprintf(stderr,
                 "real Blender Legacy GP modifier stack did not apply Thickness pressure change\\n");
    return 60;
  }

  if (backend.stroke_buffer_count() != 0) {
    std::fprintf(stderr, "real Legacy GP sbuffer was not cleared after stroke commit: %d\\n",
                 backend.stroke_buffer_count());
    return 62;
  }


  std::fprintf(stderr, "[BULK-GEOMETRY] Blender Legacy GP geometry API batch\n");
  if (!backend.begin_stroke({0, 8.0f}) ||
      !backend.add_point({60.0f, 20.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({62.0f, 21.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.add_point({64.0f, 20.0f, 0.0f, 0.8f, 0.8f, 0.2f}) ||
      !backend.add_point({66.0f, 21.0f, 0.0f, 0.7f, 0.7f, 0.3f}) ||
      !backend.add_point({68.0f, 20.0f, 0.0f, 0.6f, 0.6f, 0.4f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "bulk geometry setup failed: %s\n", backend.last_error());
    return 63;
  }

  const int bulk_index = backend.stroke_count() - 1;
  project_grease::gp::Backend::LegacyGeometryOp bulk_ops[] = {
      {project_grease::gp::Backend::LegacyGeometryOpType::SmoothStrength, 0.25f},
      {project_grease::gp::Backend::LegacyGeometryOpType::SmoothThickness, 0.25f},
      {project_grease::gp::Backend::LegacyGeometryOpType::SmoothUV, 0.25f},
      {project_grease::gp::Backend::LegacyGeometryOpType::Subdivide, 0.0f, 0.0f, 0.0f, 1, 0},
      {project_grease::gp::Backend::LegacyGeometryOpType::SimplifyAdaptive, 0.01f},
      {project_grease::gp::Backend::LegacyGeometryOpType::Sample, 0.75f, 0.0f, 0.0f, 0, 0, false, false},
      {project_grease::gp::Backend::LegacyGeometryOpType::MergeDistance, 0.01f, 0.0f, 0.0f, 0, 0, true, false},
      {project_grease::gp::Backend::LegacyGeometryOpType::Stretch, 0.25f, 0.1f, 0.25f, 0, 1, false, false},
      {project_grease::gp::Backend::LegacyGeometryOpType::Close},
      {project_grease::gp::Backend::LegacyGeometryOpType::FillTriangulate},
  };

  if (!backend.apply_legacy_geometry_batch(
          bulk_index, bulk_ops, static_cast<int>(sizeof(bulk_ops) / sizeof(bulk_ops[0]))) ||
      !backend.render()) {
    std::fprintf(stderr, "bulk Legacy GP geometry callback batch failed: %s\n",
                 backend.last_error());
    return 64;
  }

  backend.select_stroke(bulk_index);
  project_grease::gp::Backend::LegacyGeometryOp dissolve_op = {
      project_grease::gp::Backend::LegacyGeometryOpType::Dissolve,
      0.0f,
      0.0f,
      0.0f,
      GP_SPOINT_SELECT,
      0,
      false,
      false,
  };
  if (!backend.apply_legacy_geometry_batch(bulk_index, &dissolve_op, 1) ||
      !backend.render()) {
    std::fprintf(stderr, "bulk Legacy GP dissolve callback failed: %s\n",
                 backend.last_error());
    return 65;
  }

  if (!backend.begin_stroke({0, 5.0f}) ||
      !backend.add_point({72.0f, 20.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({76.0f, 24.0f, 0.0f, 0.9f, 0.9f, 0.1f}) ||
      !backend.add_point({72.0f, 24.0f, 0.0f, 0.8f, 0.8f, 0.2f}) ||
      !backend.add_point({76.0f, 20.0f, 0.0f, 0.7f, 0.7f, 0.3f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "bulk trim setup failed: %s\n", backend.last_error());
    return 66;
  }
  const int bulk_trim_index = backend.stroke_count() - 1;
  project_grease::gp::Backend::LegacyGeometryOp trim_op = {
      project_grease::gp::Backend::LegacyGeometryOpType::TrimIntersection};
  if (!backend.apply_legacy_geometry_batch(bulk_trim_index, &trim_op, 1) ||
      !backend.render()) {
    std::fprintf(stderr, "bulk Legacy GP trim callback failed: %s\n",
                 backend.last_error());
    return 67;
  }

  std::fprintf(stderr,
               "[BULK-GEOMETRY] simplify/subdivide/resample/smooth/merge/stretch/"
               "close/dissolve/fill/trim Blender callbacks passed\n");

  std::fprintf(stderr,
               "[PAINT-BUFFER] Blender Legacy GP tGPspoint stroke buffer commit passed\\n");

  std::fprintf(stderr, "[MODIFIER-BULK] Blender 3.6.23 Legacy GP Tint/Offset/Texture callbacks\\n");

  if (!backend.begin_stroke({0, 5.0f}) ||
      !backend.add_point({90.0f, 20.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f}) ||
      !backend.add_point({92.0f, 22.0f, 0.0f, 0.9f, 0.9f, 0.1f, 0.0f, 0.0f, 1.0f, 1.0f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "modifier bulk Tint setup failed: %s\\n", backend.last_error());
    return 78;
  }
  const int tint_index = backend.stroke_count() - 1;
  project_grease::gp::StrokePoint tint_before{};
  project_grease::gp::StrokePoint tint_after{};
  if (!backend.get_point(tint_index, 0, &tint_before) ||
      !backend.apply_blender_modifier(tint_index, eGpencilModifierType_Tint, 0.5f, 1) ||
      !backend.get_point(tint_index, 0, &tint_after) ||
      std::fabs(tint_after.r - tint_before.r) < 1.0e-5f ||
      std::fabs(tint_after.g - tint_before.g) < 1.0e-5f ||
      std::fabs(tint_after.b - tint_before.b) < 1.0e-5f ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Tint modifier failed: %s\\n", backend.last_error());
    return 79;
  }

  if (!backend.begin_stroke({0, 5.0f}) ||
      !backend.add_point({100.0f, 30.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({102.0f, 31.0f, 0.0f, 1.0f, 1.0f, 0.1f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "modifier bulk Offset setup failed: %s\\n", backend.last_error());
    return 80;
  }
  const int offset_index = backend.stroke_count() - 1;
  project_grease::gp::StrokePoint offset_before{};
  project_grease::gp::StrokePoint offset_after{};
  if (!backend.get_point(offset_index, 0, &offset_before) ||
      !backend.apply_blender_modifier(offset_index, eGpencilModifierType_Offset, 1.0f, 1) ||
      !backend.get_point(offset_index, 0, &offset_after) ||
      std::fabs(offset_after.x - (offset_before.x + 1.0f)) > 1.0e-5f ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Offset modifier failed: %s\\n", backend.last_error());
    return 81;
  }

  if (!backend.begin_stroke({0, 5.0f}) ||
      !backend.add_point({110.0f, 40.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}) ||
      !backend.add_point({112.0f, 40.0f, 0.0f, 1.0f, 1.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "modifier bulk Texture setup failed: %s\\n", backend.last_error());
    return 82;
  }
  const int texture_index = backend.stroke_count() - 1;
  project_grease::gp::StrokePoint texture_before{};
  project_grease::gp::StrokePoint texture_after{};
  if (!backend.get_point(texture_index, 0, &texture_before) ||
      !backend.apply_blender_modifier(texture_index, eGpencilModifierType_Texture, 0.5f, 1) ||
      !backend.get_point(texture_index, 0, &texture_after) ||
      std::fabs(texture_after.uv_fac - texture_before.uv_fac) < 1.0e-5f ||
      std::fabs(texture_after.uv_rot - texture_before.uv_rot) < 1.0e-5f ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender GP Texture Mapping modifier failed: %s\\n", backend.last_error());
    return 83;
  }

  std::fprintf(stderr,
               "[MODIFIER-BULK] real Blender Legacy GP Tint -> Offset -> Texture Mapping callbacks passed\\n");


  std::fprintf(stderr, "[MODIFIER-BULK] Blender 3.6.23 Legacy GP Weight Angle / Weight Proximity / Hook callbacks\\n");

  if (!backend.begin_stroke({0, 5.0f}) ||
      !backend.add_point({120.0f, 50.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({124.0f, 54.0f, 0.0f, 1.0f, 1.0f, 0.1f}) ||
      !backend.add_point({128.0f, 50.0f, 0.0f, 1.0f, 1.0f, 0.2f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "weight-angle setup failed: %s\\n", backend.last_error());
    return 84;
  }
  const int weight_angle_index = backend.stroke_count() - 1;
  if (!backend.apply_blender_modifier(
          weight_angle_index, eGpencilModifierType_WeightAngle, 0.0f, 1) ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender Weight Angle modifier failed: %s\\n", backend.last_error());
    return 85;
  }
  float angle_weight = -1.0f;
  if (!backend.get_point_group_weight(weight_angle_index, 1, 0, &angle_weight) ||
      angle_weight < 0.0f || angle_weight > 1.0f) {
    std::fprintf(stderr, "real Blender Weight Angle group weight was not produced: %f\\n", angle_weight);
    return 86;
  }

  if (!backend.begin_stroke({0, 5.0f}) ||
      !backend.add_point({130.0f, 60.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({132.0f, 62.0f, 0.0f, 1.0f, 1.0f, 0.1f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "weight-proximity setup failed: %s\\n", backend.last_error());
    return 87;
  }
  const int weight_prox_index = backend.stroke_count() - 1;
  if (!backend.apply_blender_modifier(
          weight_prox_index, eGpencilModifierType_WeightProximity, 10.0f, 1) ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender Weight Proximity modifier failed: %s\\n", backend.last_error());
    return 88;
  }
  float prox_weight = -1.0f;
  if (!backend.get_point_group_weight(weight_prox_index, 1, 1, &prox_weight) ||
      prox_weight < 0.0f || prox_weight > 1.0f) {
    std::fprintf(stderr, "real Blender Weight Proximity group weight was not produced: %f\\n", prox_weight);
    return 89;
  }

  if (!backend.begin_stroke({0, 5.0f}) ||
      !backend.add_point({140.0f, 70.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({144.0f, 72.0f, 0.0f, 1.0f, 1.0f, 0.1f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "hook setup failed: %s\\n", backend.last_error());
    return 90;
  }
  const int hook_index = backend.stroke_count() - 1;
  project_grease::gp::StrokePoint hook_before{};
  project_grease::gp::StrokePoint hook_after{};
  if (!backend.get_point(hook_index, 0, &hook_before) ||
      !backend.apply_blender_modifier(hook_index, eGpencilModifierType_Hook, 1.0f, 1) ||
      !backend.get_point(hook_index, 0, &hook_after) ||
      std::fabs(hook_after.x - (hook_before.x + 1.0f)) > 1.0e-5f ||
      std::fabs(hook_after.y - (hook_before.y + 0.5f)) > 1.0e-5f ||
      !backend.render()) {
    std::fprintf(stderr, "real Blender Hook modifier failed: %s\\n", backend.last_error());
    return 91;
  }

  std::fprintf(stderr,
               "[MODIFIER-BULK] real Blender Legacy GP Weight Angle -> Weight Proximity -> Hook callbacks passed\\n");

  std::fprintf(stderr, "[GENERATOR] real Blender 3.6.23 Legacy GP Build algorithm closure\\n");
  if (!backend.begin_stroke({0, 6.0f}) ||
      !backend.add_point({180.0f, 80.0f, 0.0f, 1.0f, 1.0f, 0.0f}) ||
      !backend.add_point({184.0f, 82.0f, 0.0f, 1.0f, 1.0f, 0.1f}) ||
      !backend.add_point({188.0f, 84.0f, 0.0f, 1.0f, 1.0f, 0.2f}) ||
      !backend.add_point({192.0f, 86.0f, 0.0f, 1.0f, 1.0f, 0.3f}) ||
      !backend.add_point({196.0f, 88.0f, 0.0f, 1.0f, 1.0f, 0.4f}) ||
      !backend.end_stroke()) {
    std::fprintf(stderr, "Build generator setup failed: %s\\n", backend.last_error()); return 92;
  }
  const int build_before = backend.point_count();
  if (build_before != 5 ||
      !backend.apply_blender_generator(eGpencilModifierType_Build, 0.5f, 1) ||
      backend.point_count() >= build_before || backend.point_count() <= 0 || !backend.render()) {
    std::fprintf(stderr, "real Blender Build generator failed: %s\\n", backend.last_error()); return 93;
  }
  std::fprintf(stderr, "[GENERATOR] real Blender Legacy GP Build deterministic closure passed\\n");

  std::fprintf(stderr,
               "[MODIFIER-STACK] Smooth -> Thickness -> Subdivide real Blender callbacks passed\\n");

  std::fprintf(stderr, "[DONE] edit/duplicate/translate/delete operations passed\n");
  std::puts("Blender legacy GP stroke edit/duplicate/translate/delete test passed");
  std::fflush(stdout);
  return 0;
}
