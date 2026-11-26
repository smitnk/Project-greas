/* Host test of SweepRigUiTest.rigFromTheUi's extrude through the real Backend and bridge, in the
 * device order: Add armature (Edit mode), Add bone, history record (RigPanel done()), then the armed
 * Extrude drag of RigTouch.startExtrude (extrude(1,0), translate the new tail under the finger) and
 * the history record on UP (ARMATURE_OT_extrude_move). */
#include <cmath>
#include <cstdio>

#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;

int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);
int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap);

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int cmd(Backend &b, int c, int bone, std::initializer_list<float> args)
{
  return pg_arm_bridge_command(b, c, bone, args.size() ? args.begin() : nullptr, (int)args.size(), nullptr);
}
static float summary(Backend &b, int i)
{
  float q[8] = {};
  pg_arm_bridge_query(b, PG_ARM_Q_SUMMARY, -1, 0, q, 8);
  return q[i];
}

int main()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("Lines") && b.create_frame(1));
  CHECK(b.history_reset());
  const float cx = 960, cy = 540;
  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {1}) == 1);
  b.history_record();
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {cx, cy, cx, cy - 100}) == 0);
  b.history_record();
  CHECK(summary(b, 0) == 1);
  printf("before extrude: count %g active %g mode %g\n", summary(b, 0), summary(b, 1), summary(b, 2));
  const int ext = cmd(b, PG_ARM_CMD_EXTRUDE, -1, {1, 0});
  printf("extrude -> %d, count %g active %g\n", ext, summary(b, 0), summary(b, 1));
  CHECK(ext == 1);
  CHECK(summary(b, 0) == 2);
  /* the DOWN lands on bone 0's tail = the new bone's head (cx, cy - 100): jumping the new tail under
   * the finger there makes it zero length, which Edit mode keeps (ED_armature_from_edit runs on exit) */
  float info[64] = {};
  pg_arm_bridge_query(b, PG_ARM_Q_BONE, 1, 0, info, 64);
  const float downx = cx, downy = cy - 100;
  CHECK(std::fabs(info[0] - downx) < 0.01f && std::fabs(info[1] - downy) < 0.01f);
  b.history_record();
  {
    Backend z; /* the tail jumped onto its head, on a copy of the sequence: the bone stays in Edit mode */
    CHECK(z.initialize() && z.create_document() && z.create_layer("L") && z.create_frame(1));
    cmd(z, PG_ARM_CMD_CREATE, -1, {}); cmd(z, PG_ARM_CMD_SET_MODE, -1, {1});
    cmd(z, PG_ARM_CMD_BONE_ADD, -1, {cx, cy, cx, cy - 100}); cmd(z, PG_ARM_CMD_EXTRUDE, -1, {1, 0});
    cmd(z, PG_ARM_CMD_EDIT_TRANSLATE, -1, {downx - (downx + 1), 0});
    CHECK(summary(z, 0) == 2);
    cmd(z, PG_ARM_CMD_SET_MODE, -1, {0});
    CHECK(summary(z, 0) == 1);
  }
  /* the fixed RigTouch path: the finger is on the new head, so the 1 px tail stays and the drag
   * deltas (taken from down + 1 px) move it: tail = head + (finger - down) */
  float lastx = downx + 1, lasty = downy;
  for (int s = 1; s <= 12; s++) {
    const float mx = downx + 80.0f * s / 12, my = downy - 80.0f * s / 12;
    cmd(b, PG_ARM_CMD_EDIT_TRANSLATE, -1, {mx - lastx, my - lasty});
    lastx = mx; lasty = my;
    CHECK(summary(b, 0) == 2);
  }
  b.history_record();
  CHECK(summary(b, 0) == 2);
  pg_arm_bridge_query(b, PG_ARM_Q_BONE, 1, 0, info, 64);
  printf("bone 1 head %g,%g tail %g,%g parent %g\n", info[0], info[1], info[3], info[4], info[7]);
  CHECK(std::fabs(info[3] - (cx + 80)) < 2 && std::fabs(info[4] - (cy - 180)) < 2);
  CHECK(info[7] == 0);
  printf("test_backend_rig_extrude_ui: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
