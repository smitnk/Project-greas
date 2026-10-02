/* Host test for the modifier-stack wiring in Backend: storage per layer index, layer operations,
 * undo/redo, Apply and the evaluated-frame cache (project_grease_gp_backend.cpp compiled with
 * __ANDROID__ against the real pinned Blender legacy GP code; the GPU/draw layer is stubbed because
 * nothing here renders). See tools/run_native_modifier_stack_tests.sh. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_blender_edit.h"
#include "project_grease_gp_backend.h"
#include "project_grease_modifier_stack.h"

/* The draw module is not part of this host test: the backend only needs these two to be non-null. */
extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;
using project_grease::gp::StrokePoint;

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static void add_line(Backend &b, float x0, float y0)
{
  StrokePoint pts[5];
  for (int i = 0; i < 5; i++) {
    pts[i] = StrokePoint{x0 + 20.0f * i, y0 + ((i % 2) ? 6.0f : 0.0f), 0.0f, 0.5f, 1.0f, 0.0f};
  }
  PGStrokeInfo info{};
  info.material_index = 0;
  info.thickness = 8.0f;
  CHECK(b.add_stroke(pts, 5, info));
}

static float first_x(const bGPDframe *f)
{
  const bGPDstroke *s = static_cast<const bGPDstroke *>(f->strokes.first);
  return s ? s->points[0].x : -1e9f;
}

static void set_offset(Backend &b, int layer, int mod, float dx)
{
  float params[PG_MOD_MAX_PARAMS];
  int type = 0, enabled = 0;
  const int n = b.modifier_get(layer, mod, &type, &enabled, params, PG_MOD_MAX_PARAMS);
  CHECK(n == pg_mod_param_count(PG_MOD_OFFSET) && type == PG_MOD_OFFSET);
  params[PG_P_OFFSET_MODE] = 3; /* GP_OFFSET_STROKE: non-random */
  params[PG_P_OFFSET_LOC] = dx;
  CHECK(b.modifier_set_params(layer, mod, params, n));
}

int main()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("A") && b.create_frame(1));
  add_line(b, 100.0f, 100.0f);
  bGPdata *gpd = b.document_data();
  bGPDlayer *layer_a = b.active_layer_data();
  bGPDframe *frame_a = static_cast<bGPDframe *>(layer_a->frames.first);
  CHECK(b.history_reset());

  /* --- stack commands --- */
  CHECK(b.modifier_count(0) == 0);
  CHECK(b.modifier_add(0, 99) == -1 && b.modifier_add(5, PG_MOD_OFFSET) == -1);
  CHECK(b.modifier_add(0, PG_MOD_OFFSET) == 0);
  CHECK(b.modifier_add(0, PG_MOD_THICKNESS) == 1);
  CHECK(b.modifier_count(0) == 2);
  int type = 0, enabled = 0;
  float params[PG_MOD_MAX_PARAMS] = {};
  CHECK(b.modifier_get(0, 1, &type, &enabled, params, PG_MOD_MAX_PARAMS) == pg_mod_param_count(PG_MOD_THICKNESS));
  CHECK(type == PG_MOD_THICKNESS && enabled == 1);
  CHECK(b.modifier_get(0, 2, &type, &enabled, params, PG_MOD_MAX_PARAMS) == -1);
  CHECK(b.modifier_move(0, 1, 0));
  CHECK(b.modifier_get(0, 0, &type, nullptr, nullptr, 0) > 0 && type == PG_MOD_THICKNESS);
  CHECK(b.modifier_move(0, 0, 1));
  CHECK(!b.modifier_move(0, 0, 2) && !b.modifier_remove(0, 2));
  set_offset(b, 0, 0, 25.0f);
  /* sanitising: a NaN parameter is replaced */
  {
    float bad[PG_MOD_MAX_PARAMS];
    const int n = b.modifier_get(0, 0, nullptr, nullptr, bad, PG_MOD_MAX_PARAMS);
    bad[PG_P_OFFSET_LOC] = NAN;
    CHECK(b.modifier_set_params(0, 0, bad, n));
    float again[PG_MOD_MAX_PARAMS];
    b.modifier_get(0, 0, nullptr, nullptr, again, PG_MOD_MAX_PARAMS);
    CHECK(std::isfinite(again[PG_P_OFFSET_LOC]));
  }
  set_offset(b, 0, 0, 25.0f);
  CHECK(b.modifier_remove(0, 1));
  CHECK(b.modifier_count(0) == 1);

  /* --- evaluation: originals untouched, cache hits, invalidation --- */
  const uint64_t evals0 = b.modifier_eval_count();
  const bGPDframe *ev = b.evaluated_frame(layer_a, frame_a, 1);
  CHECK(ev != frame_a);
  CHECK(std::fabs(first_x(ev) - 125.0f) < 1e-3f);
  CHECK(std::fabs(first_x(frame_a) - 100.0f) < 1e-3f); /* original unchanged */
  CHECK(b.modifier_eval_count() == evals0 + 1);
  CHECK(b.evaluated_frame(layer_a, frame_a, 1) == ev && b.modifier_eval_count() == evals0 + 1); /* cached */
  CHECK(b.evaluated_frame(layer_a, frame_a, 2) != nullptr && b.modifier_eval_count() == evals0 + 2); /* frame change */
  b.evaluated_frame(layer_a, frame_a, 1);
  CHECK(b.modifier_eval_count() == evals0 + 3);
  b.evaluated_frame(layer_a, frame_a, 1);
  CHECK(b.modifier_eval_count() == evals0 + 3);
  set_offset(b, 0, 0, 40.0f); /* stack change */
  CHECK(std::fabs(first_x(b.evaluated_frame(layer_a, frame_a, 1)) - 140.0f) < 1e-3f);
  CHECK(b.modifier_eval_count() == evals0 + 4);
  add_line(b, 300.0f, 300.0f); /* edit */
  b.evaluated_frame(layer_a, frame_a, 1);
  CHECK(b.modifier_eval_count() == evals0 + 5);
  {
    const bGPDframe *e2 = b.evaluated_frame(layer_a, frame_a, 1);
    CHECK(BLI_listbase_count(&e2->strokes) == 2);
  }
  CHECK(b.modifier_set_enabled(0, 0, false));
  CHECK(b.evaluated_frame(layer_a, frame_a, 1) == frame_a); /* disabled: draw the original */
  CHECK(b.modifier_set_enabled(0, 0, true));

  /* An edit command applied outside the Backend methods (Vertex Paint through pg_gp_edit_dispatch)
   * reports through the batch-cache dirty callback and invalidates the evaluated copy. */
  {
    const uint64_t before = b.modifier_eval_count();
    const bGPDframe *e = b.evaluated_frame(layer_a, frame_a, 1);
    const bGPDstroke *first = static_cast<const bGPDstroke *>(e->strokes.first);
    CHECK(first->points[0].vert_color[3] == 0.0f);
    const float args[11] = {PG_VPAINT_DRAW, 100, 100, 50, 1, 1, 0, 0, PG_PAINT_MODE_STROKE, 0, 0};
    CHECK(pg_gp_edit_dispatch(gpd, layer_a, PG_EDIT_CMD_VERTEX_PAINT, args, 11) == 1);
    const bGPDframe *e2 = b.evaluated_frame(layer_a, frame_a, 1);
    CHECK(b.modifier_eval_count() >= before + 1 && b.modifier_eval_count() <= before + 2);
    const bGPDstroke *painted = static_cast<const bGPDstroke *>(e2->strokes.first);
    CHECK(painted->points[0].vert_color[3] > 0.0f);
    CHECK(static_cast<const bGPDstroke *>(frame_a->strokes.first)->points[0].vert_color[3] > 0.0f); /* original painted too */
  }

  /* --- undo / redo carry the stack --- */
  CHECK(b.history_record());
  CHECK(b.modifier_add(0, PG_MOD_NOISE) == 1 && b.history_record());
  CHECK(b.modifier_count(0) == 2);
  CHECK(b.history_undo());
  CHECK(b.modifier_count(0) == 1);
  CHECK(b.history_redo());
  CHECK(b.modifier_count(0) == 2);
  CHECK(b.history_undo() && b.modifier_count(0) == 1);
  layer_a = b.active_layer_data();
  frame_a = static_cast<bGPDframe *>(layer_a->frames.first);
  {
    const bGPDframe *e3 = b.evaluated_frame(layer_a, frame_a, 1);
    CHECK(e3 != frame_a && std::fabs(first_x(e3) - 140.0f) < 1e-3f); /* restored layer, fresh evaluation */
  }

  /* --- layer operations move the stacks with their layers --- */
  CHECK(b.create_layer("B") && b.create_frame(1));
  CHECK(b.layer_count() == 2 && b.modifier_count(1) == 0 && b.modifier_count(0) == 1);
  CHECK(b.modifier_add(1, PG_MOD_SMOOTH) == 0 && b.modifier_add(1, PG_MOD_SUBDIV) == 1);
  CHECK(b.move_layer(1, 0));
  CHECK(b.modifier_count(0) == 2 && b.modifier_count(1) == 1);
  CHECK(b.modifier_get(0, 0, &type, nullptr, nullptr, 0) > 0 && type == PG_MOD_SMOOTH);
  CHECK(b.modifier_get(1, 0, &type, nullptr, nullptr, 0) > 0 && type == PG_MOD_OFFSET);
  CHECK(b.duplicate_layer(1));
  CHECK(b.layer_count() == 3 && b.modifier_count(2) == 1);
  CHECK(b.modifier_get(2, 0, &type, nullptr, nullptr, 0) > 0 && type == PG_MOD_OFFSET);
  CHECK(b.delete_layer(0));
  CHECK(b.layer_count() == 2 && b.modifier_count(0) == 1 && b.modifier_count(1) == 1);

  /* --- Apply bakes into the originals, removes the entry, and is undoable --- */
  CHECK(b.select_layer(0));
  CHECK(b.select_frame(1));
  CHECK(b.history_record());
  bGPDlayer *layer0 = static_cast<bGPDlayer *>(gpd->layers.first);
  bGPDframe *f0 = static_cast<bGPDframe *>(layer0->frames.first);
  const float x_before = first_x(f0);
  const float expect = x_before + 40.0f;
  const uint64_t shown = b.modifier_eval_count();
  const float live_x = first_x(b.evaluated_frame(layer0, f0, 1));
  CHECK(b.modifier_eval_count() == shown + 1);
  CHECK(b.modifier_apply(0, 0));
  CHECK(b.modifier_count(0) == 0);
  layer0 = static_cast<bGPDlayer *>(gpd->layers.first);
  f0 = static_cast<bGPDframe *>(layer0->frames.first);
  CHECK(std::fabs(first_x(f0) - expect) < 1e-3f);
  CHECK(std::fabs(live_x - first_x(f0)) < 1e-3f); /* apply == live */
  CHECK(b.evaluated_frame(layer0, f0, 1) == f0);   /* empty stack draws the original */
  CHECK(b.history_record());
  CHECK(b.history_undo());
  CHECK(b.modifier_count(0) == 1);
  layer0 = static_cast<bGPDlayer *>(gpd->layers.first);
  f0 = static_cast<bGPDframe *>(layer0->frames.first);
  CHECK(std::fabs(first_x(f0) - x_before) < 1e-3f);

  /* --- interpolation easing: Quad In at t = 0.5 places points 25% of the way --- */
  {
    CHECK(b.reset_document());
    add_line(b, 0.0f, 0.0f); /* frame 1: x = 0, 20, 40, 60, 80 */
    CHECK(b.create_frame(10));
    add_line(b, 100.0f, 0.0f); /* frame 10: x = 100 ... */
    CHECK(b.interpolate_frame(1, 10, 3, 0.5f, 1 /* Quad */, 0 /* In */));
    bGPDlayer *lay = b.active_layer_data();
    bGPDframe *f3 = nullptr;
    for (bGPDframe *f = static_cast<bGPDframe *>(lay->frames.first); f; f = f->next) {
      if (f->framenum == 3) f3 = f;
    }
    CHECK(f3 != nullptr);
    if (f3) {
      CHECK(std::fabs(first_x(f3) - 25.0f) < 1e-3f);
    }
    CHECK(b.interpolate_frame(1, 10, 4, 0.5f)); /* default Linear: halfway */
    for (bGPDframe *f = static_cast<bGPDframe *>(lay->frames.first); f; f = f->next) {
      if (f->framenum == 4) CHECK(std::fabs(first_x(f) - 50.0f) < 1e-3f);
    }
    CHECK(b.interpolate_frame(1, 10, 5, 0.5f, 1, 1 /* Quad Out */));
    for (bGPDframe *f = static_cast<bGPDframe *>(lay->frames.first); f; f = f->next) {
      if (f->framenum == 5) CHECK(std::fabs(first_x(f) - 75.0f) < 1e-3f);
    }
  }

  /* --- layer masks: names, flags, rename/duplicate/delete bookkeeping --- */
  {
    CHECK(b.reset_document());
    CHECK(b.create_layer("B") && b.create_layer("C"));
    CHECK(b.layer_count() == 3 && b.mask_count(0) == 0 && !b.layer_use_mask(0));
    CHECK(!b.mask_add(0, 0) && !b.mask_add(0, 9)); /* not itself, not a missing layer */
    CHECK(b.mask_add(2, 0) && b.mask_add(2, 1) && !b.mask_add(2, 1)); /* no duplicates */
    CHECK(b.mask_count(2) == 2);
    char name[128];
    int flags = -1;
    CHECK(b.mask_get(2, 0, name, sizeof(name), &flags) && std::strcmp(name, "Layer 1") == 0 && flags == 0);
    CHECK(b.mask_set_flags(2, 1, 3) && b.mask_get(2, 1, name, sizeof(name), &flags) && flags == 3);
    CHECK(!b.mask_set_flags(2, 5, 1) && !b.mask_get(2, 5, name, sizeof(name), &flags));
    CHECK(b.set_layer_use_mask(2, true) && b.layer_use_mask(2));
    CHECK(b.rename_layer(1, "Renamed")); /* masks follow a rename */
    CHECK(b.mask_get(2, 1, name, sizeof(name), &flags) && std::strcmp(name, "Renamed") == 0);
    CHECK(b.duplicate_layer(2)); /* the copy has the same masks */
    CHECK(b.layer_count() == 4 && b.mask_count(3) == 2 && b.layer_use_mask(3));
    CHECK(b.history_reset());
    CHECK(b.delete_layer(1)); /* deleting a mask layer removes the references to it */
    CHECK(b.layer_count() == 3 && b.mask_count(1) == 1 && b.mask_count(2) == 1);
    CHECK(b.history_record());
    CHECK(b.history_undo()); /* undo restores the layer and its references */
    CHECK(b.layer_count() == 4 && b.mask_count(2) == 2 && b.mask_count(3) == 2);
    CHECK(b.mask_remove(3, 0) && b.mask_count(3) == 1);
    CHECK(!b.mask_remove(3, 4));
  }

  /* --- vertex groups and point weights --- */
  {
    CHECK(b.reset_document());
    add_line(b, 0.0f, 0.0f);
    CHECK(b.vertex_group_count() == 0 && b.vertex_group_active() == -1);
    CHECK(b.vertex_group_add("Arm") == 0 && b.vertex_group_add("Arm") == 1 && b.vertex_group_add("Leg") == 2);
    char gname[64];
    CHECK(b.vertex_group_name(1, gname, sizeof(gname)) && std::strcmp(gname, "Arm.001") == 0); /* unique */
    CHECK(b.vertex_group_active() == 0 && b.set_vertex_group_active(2) && b.vertex_group_active() == 2);
    CHECK(!b.set_vertex_group_active(3) && !b.vertex_group_remove(7));
    CHECK(b.set_point_weight(0, 0, 0, 0.25f) && b.set_point_weight(0, 0, 2, 1.5f) /* clamped */ &&
          b.set_point_weight(0, 1, 1, 0.5f));
    CHECK(!b.set_point_weight(0, 99, 0, 1.0f) && !b.set_point_weight(5, 0, 0, 1.0f));
    CHECK(b.point_weight_count(0, 0) == 2 && b.point_weight_count(0, 2) == 0);
    int grp = -1;
    float wt = -1.0f;
    CHECK(b.point_weight_at(0, 0, 1, &grp, &wt) && grp == 2 && std::fabs(wt - 1.0f) < 1e-6f);
    CHECK(b.history_reset());
    CHECK(b.vertex_group_rename(0, "Hand") && b.vertex_group_name(0, gname, sizeof(gname)) && std::strcmp(gname, "Hand") == 0);
    CHECK(b.vertex_group_remove(1)); /* "Arm.001": its weight goes, Leg (2) becomes 1 */
    CHECK(b.vertex_group_count() == 2 && b.vertex_group_name(1, gname, sizeof(gname)) && std::strcmp(gname, "Leg") == 0);
    CHECK(b.point_weight_count(0, 1) == 0); /* point 1 only had group 1 */
    CHECK(b.point_weight_at(0, 0, 0, &grp, &wt) && grp == 0 && std::fabs(wt - 0.25f) < 1e-6f);
    CHECK(b.point_weight_at(0, 0, 1, &grp, &wt) && grp == 1 && std::fabs(wt - 1.0f) < 1e-6f); /* shifted */
    CHECK(b.vertex_group_active() == 1); /* the active group followed its shift */
    CHECK(b.history_record());
    CHECK(b.history_undo()); /* undo brings back the group, its name and the weights */
    CHECK(b.vertex_group_count() == 3 && b.vertex_group_name(1, gname, sizeof(gname)) && std::strcmp(gname, "Arm.001") == 0);
    CHECK(b.point_weight_count(0, 1) == 1 && b.vertex_group_active() == 2);
    CHECK(b.history_redo() && b.vertex_group_count() == 2);
  }

  /* --- reset clears every stack --- */
  CHECK(b.reset_document());
  CHECK(b.layer_count() == 1 && b.modifier_count(0) == 0);

  b.shutdown();
  if (failures) {
    printf("%d FAILURES\n", failures);
    return 1;
  }
  printf("backend modifier stack tests passed\n");
  return 0;
}
