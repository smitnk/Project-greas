/* Host test of Draw mode colour (Backend::set_brush_vertex_color): in Vertex Color mode each new
 * stroke carries the brush colour as vertex colour (ED_gpencil_point_vertex_color_set), so changing
 * the colour never recolours earlier strokes; in Material mode strokes keep the zero "no vertex
 * colour" and follow the material. See tools/run_native_modifier_stack_tests.sh. */
#include <cstdio>

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_gp_backend.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using namespace project_grease::gp;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void draw(Backend &b, float y)
{
  StrokeStyle st;
  st.thickness = 4;
  CHECK(b.begin_stroke(st));
  for (int i = 0; i < 10; i++) CHECK(b.add_point(StrokePoint{10.0f * i, y, 0, 1, 1, 0}));
  CHECK(b.end_stroke());
}

int main()
{
  Backend b;
  CHECK(b.initialize() && b.create_document() && b.create_layer("A") && b.create_frame(1));
  const float black[3] = {0, 0, 0}, red[3] = {1, 0, 0};
  b.set_brush_vertex_color(true, black, 1.0f);
  draw(b, 10);
  b.set_brush_vertex_color(true, red, 1.0f);
  draw(b, 20);
  StrokePoint p{};
  CHECK(b.get_point(0, 5, &p) && p.r == 0 && p.a == 1); /* first stroke stays black */
  CHECK(b.get_point(1, 5, &p) && p.r == 1 && p.g == 0 && p.a == 1);
  /* fill colour too (vertex_mode Stroke & Fill) */
  const bGPDframe *f = static_cast<const bGPDlayer *>(b.document_data()->layers.first)->actframe;
  const bGPDstroke *second = f ? static_cast<const bGPDstroke *>(f->strokes.last) : nullptr;
  CHECK(second && second->vert_color_fill[0] == 1 && second->vert_color_fill[3] == 1);
  /* Material mode: no vertex colour */
  b.set_brush_vertex_color(false, red, 1.0f);
  draw(b, 30);
  CHECK(b.get_point(2, 5, &p) && p.a == 0);
  /* Mix Factor and Fill-only mode: points untouched, fill gets the colour at 0.25 */
  b.set_brush_vertex_color(true, red, 0.25f, 1);
  draw(b, 40);
  CHECK(b.get_point(3, 5, &p) && p.a == 0);
  const bGPDstroke *fs = static_cast<const bGPDstroke *>(f->strokes.last);
  CHECK(fs->vert_color_fill[0] == 1 && fs->vert_color_fill[3] == 0.25f);
  /* Stroke-only: fill untouched */
  b.set_brush_vertex_color(true, red, 0.5f, 0);
  draw(b, 50);
  CHECK(b.get_point(4, 5, &p) && p.r == 1 && p.a == 0.5f);
  CHECK(static_cast<const bGPDstroke *>(f->strokes.last)->vert_color_fill[3] == 0);
  std::printf(failures ? "%d FAILURES\n" : "backend colour tests passed\n", failures);
  return failures ? 1 : 0;
}
