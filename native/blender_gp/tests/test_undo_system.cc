/* Host test of the undo system port (project_grease_undo.h: Blender 3.6.23 undo_system.cc,
 * memfile_undo.cc chunk sharing, editarmature_undo.cc) through the real Backend and bridge:
 * memfile chunk sharing (memory of 32 steps of a 3000-stroke document), named steps, jump
 * (Edit > Undo History), the U.undosteps limit, and armature Edit mode undo that only undoes bones.
 * See tools/run_native_undo_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "DNA_gpencil_legacy_types.h"
#include "project_grease_armature.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"
#include "project_grease_undo.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;
using project_grease::gp::StrokePoint;
using project_grease::gp::StrokeStyle;

int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);
int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap);

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int cmd(Backend &b, int c, int bone, std::initializer_list<float> args, const char *name = nullptr)
{
  return pg_arm_bridge_command(b, c, bone, args.size() ? args.begin() : nullptr, (int)args.size(), name);
}
static int bone_count(Backend &b)
{
  float q[8];
  pg_arm_bridge_query(b, PG_ARM_Q_SUMMARY, -1, 0, q, 8);
  return (int)q[0];
}
static bool stroke(Backend &b, float y, int n = 100)
{
  StrokePoint pts[256];
  for (int i = 0; i < n; i++) pts[i] = StrokePoint{10.0f + i, y, 0, 1, 1, 0};
  return b.create_polyline(pts, n, StrokeStyle{}, false);
}

int main()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("GP_Layer") && b.create_frame(1));

  /* ---- chunk sharing: 3000 strokes, then 32 one-stroke edits ---- */
  for (int i = 0; i < 3000; i++) CHECK(stroke(b, (float)i));
  CHECK(b.select_all(1)); /* Move moves selected points */
  CHECK(b.history_reset());
  CHECK(b.history_undo_count() == 1 && b.history_step_name(0) == "Original");
  const size_t original = b.history_memory();
  for (int i = 0; i < PG_UNDO_STEPS; i++) {
    CHECK(b.translate_stroke(i, 1, 0, 0));
    CHECK(b.history_record("Move"));
  }
  const size_t after = b.history_memory();
  /* The old history kept a full copy per step: (steps) x document. */
  const size_t full_copies = original * (size_t)b.history_undo_count();
  printf("undo memory, %d steps of a 3000-stroke document: full snapshots %zu bytes, shared chunks %zu bytes\n",
         b.history_undo_count(), full_copies, after);
  CHECK(after < original * 2);              /* each step owns one stroke, not the document */
  CHECK(after - original < 32 * 20000);     /* ~ one 100-point stroke per step */

  /* ---- the U.undosteps limit: 32 undoable steps + the current ---- */
  CHECK(b.history_undo_count() == PG_UNDO_STEPS + 1);
  CHECK(b.history_record("Move"));
  CHECK(b.history_undo_count() == PG_UNDO_STEPS + 1);
  CHECK(b.history_active_index() == PG_UNDO_STEPS);
  int undos = 0;
  while (b.history_undo()) undos++;
  CHECK(undos == PG_UNDO_STEPS);
  CHECK(!b.history_can_undo() && b.history_can_redo());
  while (b.history_redo()) {}

  /* ---- named steps and jump ---- */
  b.history_reset();
  const int strokes0 = b.stroke_count();
  CHECK(stroke(b, 5000) && b.history_record("Draw Stroke"));
  CHECK(stroke(b, 5001) && b.history_record("Draw Stroke"));
  CHECK(b.delete_stroke(0) && b.history_record("Delete"));
  CHECK(b.history_undo_count() == 4);
  CHECK(b.history_step_name(1) == "Draw Stroke" && b.history_step_name(3) == "Delete");
  CHECK(b.history_jump(1) && b.history_active_index() == 1 && b.stroke_count() == strokes0 + 1);
  CHECK(b.history_jump(3) && b.stroke_count() == strokes0 + 1 && b.history_active_index() == 3);
  CHECK(b.history_jump(0) && b.stroke_count() == strokes0 && !b.history_can_undo());
  CHECK(b.history_jump(2) && b.stroke_count() == strokes0 + 2);
  CHECK(!b.history_jump(99));
  /* a push from the middle drops the redo tail */
  CHECK(stroke(b, 5002) && b.history_record("Draw Stroke"));
  CHECK(b.history_undo_count() == 4 && !b.history_can_redo());
  /* a deleted stroke in the middle still shares the strokes after it (lookup by content) */
  const size_t before_delete = b.history_memory();
  CHECK(b.delete_stroke(10) && b.history_record("Delete"));
  CHECK(b.history_memory() - before_delete < 20000);

  /* ---- armature Edit mode undo only undoes bones; leaving the mode makes one step ---- */
  b.history_reset();
  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1 && b.history_record("Add Armature"));
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {1}) == 1 && b.history_record("Toggle Edit Mode"));
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {100, 100, 100, 50}, "A") >= 0 && b.history_record("Add Bone"));
  CHECK(cmd(b, PG_ARM_CMD_EXTRUDE, -1, {0, -50}) >= 0 && b.history_record("Extrude"));
  const int bones = bone_count(b);
  CHECK(bones >= 2);
  /* a stroke changed outside the armature's steps is not touched by armature undo */
  const int strokes1 = b.stroke_count();
  CHECK(b.history_undo() && bone_count(b) == bones - 1);
  CHECK(b.stroke_count() == strokes1);
  CHECK(b.history_redo() && bone_count(b) == bones);
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {0}) == 1 && b.history_record("Toggle Edit Mode"));
  const int n = b.history_undo_count();
  CHECK(b.history_step_name(n - 1) == "Toggle Edit Mode");
  float q[8];
  /* one undo goes back into Edit mode with all bones */
  CHECK(b.history_undo() && bone_count(b) == bones);
  pg_arm_bridge_query(b, PG_ARM_Q_SUMMARY, -1, 0, q, 8);
  CHECK((int)q[2] == PG_ARM_MODE_EDIT);
  /* undoing past the mode entry restores the memfile state (object mode, no bones) */
  CHECK(b.history_jump(1) && bone_count(b) == 0);
  pg_arm_bridge_query(b, PG_ARM_Q_SUMMARY, -1, 0, q, 8);
  CHECK((int)q[2] == PG_ARM_MODE_OBJECT);
  CHECK(b.history_jump(n - 1) && bone_count(b) == bones);

  /* ---- U.undomemory on the stack itself: steps owning 100 MB each, 256 MB limit ---- */
  {
    static int decoded = 0;
    static const pg_undo::UndoType fake = {
        "Global Undo", nullptr, [](void *, pg_undo::UndoStep *us) { us->data_size = 100u << 20; return true; },
        [](void *, pg_undo::UndoStep *, int, bool) { decoded++; }, [](pg_undo::UndoStep *) {}, nullptr};
    static const std::vector<const pg_undo::UndoType *> types = {&fake};
    pg_undo::UndoStack st;
    st.types = &types;
    for (int i = 0; i < 6; i++) CHECK(st.push(nullptr, "Step", PG_UNDO_STEPS, PG_UNDO_MEMORY_MB));
    /* from the newest: 100 + 100 fit, the third passes 256 MB and is the oldest kept */
    CHECK(st.steps.size() == 3);
    CHECK(st.undo(nullptr) && st.undo(nullptr) && !st.undo(nullptr) && decoded == 2);
    /* jump decodes every step on the way (BKE_undosys_step_load_data_ex) */
    CHECK(st.load_from_index(nullptr, 2) && decoded == 4 && st.index_of(st.step_active) == 2);
    /* skip steps are passed over by undo */
    st.steps[1]->skip = true;
    CHECK(st.undo(nullptr) && st.index_of(st.step_active) == 0);
  }

  printf("undo system: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
