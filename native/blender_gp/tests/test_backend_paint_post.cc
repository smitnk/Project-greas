/* Host test of the Draw commit's Post-Processing (Backend::end_stroke, the app's Draw path through
 * the tool-session sink), against Blender 3.6.23 gpencil_stroke_newfrombuffer() (gpencil_paint.c:
 * 1260-1289): the same input is committed with the panel off (the behaviour before this port) and
 * with Blender's preset values; the result must equal Blender's own functions applied here to a
 * copy of the "off" stroke, in Blender's order. See tools/run_native_modifier_stack_tests.sh. */
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_gp_backend.h"
#include "project_grease_modifier_stack.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}
extern "C" void gpencil_subdivide_stroke(bGPdata *gpd, bGPDstroke *gps, int subdivide);

using namespace project_grease::gp;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static bGPDstroke *draw(Backend &b, float y)
{
  StrokeStyle st;
  st.thickness = 4;
  CHECK(b.begin_stroke(st));
  /* a slightly wavy hand line: many points inside Blender's 0.002-unit simplify tolerance */
  for (int i = 0; i < 40; i++) {
    CHECK(b.add_point(StrokePoint{5.0f * i, y + 0.15f * std::sin(i * 1.7f), 0, 0.5f + 0.01f * i, 1, 0}));
  }
  CHECK(b.end_stroke());
  const bGPDframe *f = static_cast<const bGPDlayer *>(b.document_data()->layers.first)->actframe;
  return static_cast<bGPDstroke *>(f->strokes.last);
}

static bool same(const bGPDstroke *a, const bGPDstroke *b)
{
  if (a->totpoints != b->totpoints) return false;
  for (int i = 0; i < a->totpoints; i++) {
    const bGPDspoint &p = a->points[i], &q = b->points[i];
    if (std::fabs(p.x - q.x) > 1e-4f || std::fabs(p.y - (q.y)) > 1e-4f ||
        std::fabs(p.pressure - q.pressure) > 1e-5f || std::fabs(p.strength - q.strength) > 1e-5f)
      return false;
  }
  return true;
}

/* Blender's sequence on a copy of `off` (translated to `on`'s row). */
static bGPDstroke *reference(bGPdata *gpd, const bGPDstroke *off, float dy, const Backend::LegacyPaintSettings &pp)
{
  bGPDstroke *r = BKE_gpencil_stroke_duplicate(const_cast<bGPDstroke *>(off), true, true);
  for (int i = 0; i < r->totpoints; i++) r->points[i].y += dy;
  if (pp.draw_subdivide > 0) gpencil_subdivide_stroke(gpd, r, pp.draw_subdivide);
  BKE_gpencil_stroke_smooth(r, pp.draw_smooth_factor, pp.draw_smooth_level, true, true, false, false, true, nullptr);
  if (pp.simplify_f > 0.0f) BKE_gpencil_stroke_simplify_adaptive(gpd, r, pp.simplify_f * PG_MOD_PIXELS_PER_UNIT);
  return r;
}

static void check_preset(Backend &b, float row, const Backend::LegacyPaintSettings &pp, const char *name)
{
  CHECK(b.set_legacy_paint_settings({}));
  bGPDstroke *off = draw(b, 0.0f);
  const int off_points = off->totpoints;
  CHECK(b.set_legacy_paint_settings(pp));
  bGPDstroke *on = draw(b, row);
  bGPDstroke *ref = reference(b.document_data(), off, row, pp);
  CHECK(same(on, ref));
  std::printf("%-8s points: before %d, Blender post-process %d (reference %d)\n", name, off_points, on->totpoints,
              ref->totpoints);
  BKE_gpencil_free_stroke(ref);
}

int main()
{
  Backend b;
  CHECK(b.initialize() && b.create_document() && b.create_layer("A") && b.create_frame(1));

  /* BKE_gpencil_brush_preset_set (brush.cc): Pencil 0/1/0/0.002, Pen 0/1/1/0.002, Ink Pen 0.1/1/0/0.002 */
  Backend::LegacyPaintSettings pencil{true, 1, 0.0f, 0, 0.002f, false};
  Backend::LegacyPaintSettings pen{true, 1, 0.0f, 1, 0.002f, false};
  Backend::LegacyPaintSettings ink{true, 1, 0.1f, 0, 0.002f, false};
  Backend::LegacyPaintSettings subdiv_only{true, 1, 0.0f, 2, 0.0f, false};
  check_preset(b, 100, pencil, "Pencil");
  check_preset(b, 200, pen, "Pen");
  check_preset(b, 300, ink, "Ink Pen");
  check_preset(b, 400, subdiv_only, "Subdiv2");

  /* visible effect: Pencil's adaptive simplify drops points of a near-straight line */
  CHECK(b.set_legacy_paint_settings({}));
  const int before = draw(b, 500)->totpoints;
  CHECK(b.set_legacy_paint_settings(pencil));
  CHECK(draw(b, 600)->totpoints < before);
  /* a straight line commits 2 points, as Blender's "line" trace (draw_blender_golden.txt) with every
   * default brush whose Post-Processing is on */
  CHECK(b.begin_stroke(StrokeStyle{}));
  for (int i = 0; i < 101; i++) CHECK(b.add_point(StrokePoint{200.0f + 4 * i, 650, 0, 1, 1, 0.01f * i}));
  CHECK(b.end_stroke());
  CHECK(static_cast<bGPDstroke *>(static_cast<const bGPDlayer *>(b.document_data()->layers.first)->actframe->strokes.last)
            ->totpoints == 2);
  /* subdivide 2 without simplify: (n-1)*4+1 points, as gpencil_subdivide_stroke */
  CHECK(b.set_legacy_paint_settings(subdiv_only));
  CHECK(draw(b, 700)->totpoints == (before - 1) * 4 + 1);
  /* panel off (Airbrush, Ink Pen Rough): unchanged */
  Backend::LegacyPaintSettings disabled{false, 2, 0.5f, 3, 1.0f, true};
  CHECK(b.set_legacy_paint_settings(disabled));
  CHECK(draw(b, 800)->totpoints == before);
  /* RNA ranges (rna_brush.c) */
  CHECK(!b.set_legacy_paint_settings({true, 1, 2.5f, 0, 0.0f, false}));
  CHECK(!b.set_legacy_paint_settings({true, 1, 0.0f, 4, 0.0f, false}));
  CHECK(!b.set_legacy_paint_settings({true, 1, 0.0f, 0, NAN, false}));

  std::printf(failures ? "%d FAILURES\n" : "backend paint post-process tests passed\n", failures);
  return failures ? 1 : 0;
}
