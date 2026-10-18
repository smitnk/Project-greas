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
  /* Lattice warp session: a 2x2 linear lattice, (x1, y1) corner +30 in x; repeated updates start from
   * the snapshot, cancel restores it, and unselected points never move. */
  float lb[4];
  CHECK(b.lattice_begin(lb));
  const float x10 = gps->points[10].x, y10 = gps->points[10].y, x4 = gps->points[4].x;
  /* the selected point with the largest y sits at the box's (x0, y1) corner: move that lattice point */
  bGPDspoint *corner = nullptr;
  for (int i = 5; i <= 10; i++)
    if (gps->points[i].y == lb[3]) corner = &gps->points[i];
  CHECK(corner && corner->x == lb[0]);
  float ctrl[8] = {lb[0], lb[1], lb[2], lb[1], lb[0] + 30, lb[3], lb[2], lb[3]};
  CHECK(b.lattice_update(2, 2, ctrl, 0 /* KEY_LINEAR */));
  CHECK(b.lattice_update(2, 2, ctrl, 0));
  CHECK(corner && std::fabs(corner->x - (lb[0] + 30)) < 1e-2f && std::fabs(corner->y - lb[3]) < 1e-2f);
  CHECK(gps->points[4].x == x4);
  b.lattice_end(false);
  CHECK(gps->points[10].x == x10 && gps->points[10].y == y10);
  CHECK(!b.lattice_update(2, 2, ctrl, 0)); /* no session after end */
  /* Crash of 2026-10-15: Warp open, then an edit freed its strokes, then Cancel wrote through the
   * freed pointers. Every editing operator now confirms the Warp first (bridge ensure_operator). */
  CHECK(b.lattice_begin(lb));
  CHECK(b.lattice_update(2, 2, ctrl, 0));
  const float warped = corner->x;
  CHECK(b.modal_confirm());                 /* what the bridge does before the edit */
  CHECK(corner->x == warped);               /* confirm keeps the warp result */
  CHECK(!b.modal_confirm());                /* nothing running any more */
  CHECK(b.delete_last_stroke());            /* frees the stroke the Warp held */
  b.lattice_end(false);                     /* late Cancel from the UI: must touch nothing */
  CHECK(!b.lattice_update(2, 2, ctrl, 0));
  std::printf(failures ? "%d FAILURES\n" : "backend transform tests passed\n", failures);
  return failures ? 1 : 0;
}
