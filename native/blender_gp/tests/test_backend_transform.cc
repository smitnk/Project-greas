/* Host test of Backend::transform_selected_points / selected_points_bounds (lasso Shear and
 * Perspective): only selected points move, by the given projective matrix. */
#include <cmath>
#include <cstdio>

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_gp_backend.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using namespace project_grease::gp;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main()
{
  Backend b;
  CHECK(b.initialize() && b.create_document() && b.create_layer("A") && b.create_frame(1));
  StrokeStyle st;
  CHECK(b.begin_stroke(st));
  for (int i = 0; i <= 10; i++) CHECK(b.add_point(StrokePoint{10.0f * i, 50.0f + i, 0, 1, 1, 0}));
  CHECK(b.end_stroke());
  float bounds[4];
  CHECK(!b.selected_points_bounds(bounds));
  const float shear[9] = {1, 0.5f, -25, 0, 1, 0, 0, 0, 1};
  CHECK(!b.transform_selected_points(shear)); /* nothing selected */
  bGPDstroke *gps = static_cast<bGPDstroke *>(static_cast<bGPDlayer *>(b.document_data()->layers.first)->actframe->strokes.first);
  for (int i = 5; i <= 10; i++) gps->points[i].flag |= GP_SPOINT_SELECT;
  CHECK(b.selected_points_bounds(bounds) && bounds[0] == 50 && bounds[2] == 100 && bounds[1] == 55 && bounds[3] == 60);
  CHECK(b.transform_selected_points(shear));
  CHECK(gps->points[4].x == 40);                                        /* unselected stays */
  CHECK(std::fabs(gps->points[6].x - (60 + 0.5f * 56 - 25)) < 1e-4f);   /* x += 0.5 * y - 25 */
  /* projective: w = 1 + 0.01 x halves... checks the divide */
  const float persp[9] = {1, 0, 0, 0, 1, 0, 0.01f, 0, 1};
  const float x7 = gps->points[7].x, y7 = gps->points[7].y;
  CHECK(b.transform_selected_points(persp));
  const float w = 0.01f * x7 + 1;
  CHECK(std::fabs(gps->points[7].x - x7 / w) < 1e-4f && std::fabs(gps->points[7].y - y7 / w) < 1e-4f);
  std::printf(failures ? "%d FAILURES\n" : "backend transform tests passed\n", failures);
  return failures ? 1 : 0;
}
