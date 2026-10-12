/* Host test of Backend::erase (gpencil_stroke_doeraser on the active frame) end to end: a stroke
 * committed through the paint buffer (begin/add/end_stroke, as the draw tool session does), erased
 * with the settings of Blender 3.6.23's eraser presets (BKE_gpencil_brush_preset_set). Expected
 * values follow from gpencil_stroke_eraser_dostroke's own rules; a comparison with Blender itself
 * running the same input is UNVERIFIED. See tools/run_native_modifier_stack_tests.sh. */
#include <cmath>
#include <cstdio>

#include "DNA_gpencil_legacy_types.h"
#include "project_grease_gp_backend.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;
using project_grease::gp::StrokePoint;
using project_grease::gp::StrokeStyle;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static int points_of(Backend &b, int s)
{
  StrokePoint p;
  int n = 0;
  while (b.get_point(s, n, &p)) n++;
  return n;
}
static float x_of(Backend &b, int s, int i)
{
  StrokePoint p{};
  b.get_point(s, i < 0 ? points_of(b, s) + i : i, &p);
  return p.x;
}
/* x 0..300 at y, one point every 3 canvas units (dense touch input) */
static void line(Backend &b, float y)
{
  StrokeStyle st;
  st.thickness = 10;
  CHECK(b.begin_stroke(st));
  for (float x = 0; x <= 300; x += 3) CHECK(b.add_point(StrokePoint{x, y, 0, 1, 1, 0}));
  CHECK(b.end_stroke());
}
static void fresh(Backend &b)
{
  CHECK(b.reset_document() && b.create_layer("A") && b.create_frame(1));
}

int main()
{
  Backend b;
  CHECK(b.initialize() && b.create_document());

  /* Eraser Point (GP_BRUSH_ERASER_HARD): only the points inside the radius go, the stroke splits. */
  fresh(b);
  line(b, 100);
  CHECK(b.erase(150, 100, 20, 0, 1, 1, 1, 1));
  CHECK(b.stroke_count() == 2);
  CHECK(points_of(b, 0) == 44 && points_of(b, 1) == 44);
  CHECK(std::fabs(x_of(b, 0, -1) - 129) < 0.5f && std::fabs(x_of(b, 1, 0) - 171) < 0.5f);
  /* outside every stroke: nothing changes, false */
  CHECK(!b.erase(150, 200, 20, 0, 1, 1, 1, 1));
  CHECK(b.stroke_count() == 2);

  /* Eraser Stroke (GP_BRUSH_ERASER_STROKE): every stroke with a point in the radius is freed,
   * strokes outside it stay. */
  fresh(b);
  line(b, 100);
  line(b, 110);
  line(b, 200);
  CHECK(b.erase(150, 105, 20, 2, 1, 1, 1, 1));
  CHECK(b.stroke_count() == 1);
  StrokePoint p{};
  CHECK(b.get_point(0, 0, &p) && p.y == 200);

  /* Eraser Hard (GP_BRUSH_ERASER_SOFT, draw_strength 1, era_strength_f 100, era_thickness_f 50):
   * one pass only thins: the point under the centre is pt2, pt1 and pt0 of three segments, so its
   * pressure drops by 0.1 * 0.5 * (0.5 + 1 + 0.5) = 0.1; repeated passes cull the centre. */
  fresh(b);
  line(b, 100);
  CHECK(b.erase(150, 100, 20, 1, 1, 1, 1, 0.5f));
  CHECK(b.stroke_count() == 1 && points_of(b, 0) == 101);
  CHECK(b.get_point(0, 50, &p) && std::fabs(p.pressure - 0.9f) < 1e-4f && p.strength < 1.0f);
  for (int i = 0; i < 40; i++) b.erase(150, 100, 20, 1, 1, 1, 1, 0.5f);
  CHECK(b.stroke_count() == 2);
  CHECK(x_of(b, 0, 0) == 0 && x_of(b, 1, -1) == 300);

  /* invalid settings are refused */
  CHECK(!b.erase(150, 100, 20, 3, 1, 1, 1, 1));
  CHECK(!b.erase(150, 100, 0, 0, 1, 1, 1, 1));
  CHECK(!b.erase(NAN, 100, 20, 0, 1, 1, 1, 1));

  std::printf(failures ? "%d FAILURES\n" : "backend eraser tests passed\n", failures);
  return failures ? 1 : 0;
}
