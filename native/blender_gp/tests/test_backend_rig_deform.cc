/* Host test of the Armature deform through the real Backend and bridge, in the order of the device
 * test SweepRigCoreTest.armatureBindPoseKeyScrubUndoSave: a vertical stroke (600,600)->(600,300),
 * bone + extruded child, Parent with Automatic Weights, pose rotate 90 degrees keyed at 1 and 10,
 * frame 10, then Apply of the Armature modifier (MOD_gpencil_legacy_armature.c). The bone's head is
 * the rotation pivot, so the first point stays where it is and the last point swings to (300,600). */
#include <cmath>
#include <cstdio>

#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"
#include "project_grease_modifier_stack.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;
using project_grease::gp::StrokePoint;

int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)

static int cmd(Backend &b, int c, int bone, std::initializer_list<float> args, const char *name = nullptr)
{
  return pg_arm_bridge_command(b, c, bone, args.size() ? args.begin() : nullptr, (int)args.size(), name);
}

int main()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("Lines") && b.create_frame(1));
  StrokePoint pts[4] = {};
  for (int i = 0; i < 4; i++) { pts[i].x = 600; pts[i].y = 600 - 100 * i; pts[i].pressure = pts[i].strength = 1; }
  PGStrokeInfo info = {};
  info.thickness = 10;
  CHECK(b.add_stroke(pts, 4, info));
  StrokePoint first = {}, last = {};
  CHECK(b.get_point(0, 0, &first) && b.get_point(0, 3, &last));
  printf("stored first %g,%g last %g,%g\n", first.x, first.y, last.x, last.y);

  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {600, 600, 600, 300}, "Arm.L") == 0);
  CHECK(cmd(b, PG_ARM_CMD_EXTRUDE, -1, {200, 0}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_BIND, -1, {2}) >= 2);
  int arm = -1;
  for (int i = 0; i < b.modifier_count(0); i++) {
    int type = 0, en = 0;
    float p[PG_MOD_MAX_PARAMS];
    b.modifier_get(0, i, &type, &en, p, PG_MOD_MAX_PARAMS);
    if (type == PG_MOD_ARMATURE) arm = i;
  }
  CHECK(arm >= 0);
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {2}) == 1);
  cmd(b, PG_ARM_CMD_POSE_SELECT, 0, {0});
  CHECK(cmd(b, PG_ARM_CMD_KEY_INSERT, -1, {1}) > 0);
  CHECK(cmd(b, PG_ARM_CMD_POSE_ROTATE, -1, {(float)(M_PI / 2)}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_KEY_INSERT, -1, {10}) > 0);
  cmd(b, PG_ARM_CMD_EVALUATE, -1, {10});

  CHECK(b.modifier_apply(0, arm));
  StrokePoint f2 = {}, l2 = {};
  CHECK(b.get_point(0, 0, &f2) && b.get_point(0, 3, &l2));
  printf("applied first %g,%g last %g,%g\n", f2.x, f2.y, l2.x, l2.y);
  NEAR(f2.x, first.x, 0.5); NEAR(f2.y, first.y, 0.5); /* the pivot */
  /* the tip swings a quarter turn about the head: distance 300 px, at 90 degrees */
  NEAR(std::hypot(l2.x - first.x, l2.y - first.y), std::hypot(last.x - first.x, last.y - first.y), 1.0);
  CHECK(std::fabs(l2.x - last.x) > 100 || std::fabs(l2.y - last.y) > 100);

  printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "OK", checks, failures);
  return failures ? 1 : 0;
}
