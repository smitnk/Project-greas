/* Host repro through Backend + bridge of the device failures of run 45508802411:
 * SweepRigConstraintsTest.childOfSetAndClearInverse / masterSliderDrivesBonesAndDrawingSwap and SweepRigUiTest.addSwapHelperThroughController. */
#include <cmath>
#include <cstdio>
#include <string>

#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "project_grease_armature.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"
#include "project_grease_modifier_stack.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;
int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);
int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap);

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)
static int cmd(Backend &b, int c, int bone, std::initializer_list<float> args, const char *name = nullptr)
{
  return pg_arm_bridge_command(b, c, bone, args.size() ? args.begin() : nullptr, (int)args.size(), name);
}
static float info(Backend &b, int bone, int i)
{
  float q[PG_ARM_BONE_INFO];
  pg_arm_bridge_query(b, PG_ARM_Q_BONE, bone, 0, q, PG_ARM_BONE_INFO);
  return q[i];
}


/* RigCharacterTest.waveAndWalkCycle's left arm (character px, s = 720 * 0.75 / 106, feet at y 648) */
static const float S = 720.0f * 0.75f / 106.0f, OX = 640.0f, OY = 648.0f;
static int cbone(Backend &b, const char *name, float hx, float hy, float tx, float ty, int parent = -1, bool conn = false)
{
  const int i = cmd(b, PG_ARM_CMD_BONE_ADD, -1, {OX + S * hx, OY + S * hy, OX + S * tx, OY + S * ty}, name);
  if (parent >= 0) {
    cmd(b, PG_ARM_CMD_EDIT_SELECT, i, {1, 0});
    cmd(b, PG_ARM_CMD_EDIT_SELECT, parent, {1, 1});
    CHECK(cmd(b, PG_ARM_CMD_PARENT_SET, -1, {conn ? 1.0f : 2.0f}) == 1);
  }
  return i;
}
static float arm_miss(float dx, float dy, float elbow_x)
{
  Backend b;
  CHECK(b.initialize() && b.create_document() && b.create_layer("L") && b.create_frame(1));
  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  cmd(b, PG_ARM_CMD_SET_MODE, -1, {1});
  const int hip = cbone(b, "Hip", 0, -27, 0, -38);
  const int spine = cbone(b, "Spine", 0, -38, 0, -58, hip, true);
  const int ua = cbone(b, "UpperArm.L", 22, -58, elbow_x, -46, spine);
  const int la = cbone(b, "LowerArm.L", elbow_x, -46, 28, -34, ua, true);
  const int hand = cbone(b, "IK.Hand.L", 28, -34, 28, -28);
  cbone(b, "Pole.Arm.L", 40, -46, 40, -40);
  cmd(b, PG_ARM_CMD_SET_MODE, -1, {2});
  CHECK(cmd(b, PG_ARM_CMD_CON_ADD, la, {CONSTRAINT_TYPE_KINEMATIC}) == 0);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, la, {0, 0, 0}, "IK.Hand.L") == 1);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, la, {0, 1, 0}, "Pole.Arm.L") == 1);
  CHECK(cmd(b, PG_ARM_CMD_CON_SET, la, {0, 1, 0, 0, 0, 0, CONSTRAINT_IK_TIP | CONSTRAINT_IK_POS, 2, 500, 0, 1, 1}) == 1);
  cmd(b, PG_ARM_CMD_POSE_SELECT, hand, {0});
  cmd(b, PG_ARM_CMD_POSE_TRANSLATE, -1, {S * dx, S * dy});
  return std::hypot(info(b, la, 29) - info(b, hand, 26), info(b, la, 30) - info(b, hand, 27));
}

int main()
{
  { /* childOfSetAndClearInverse */
    Backend b;
    CHECK(b.initialize() && b.create_document() && b.create_layer("L") && b.create_frame(1));
    CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
    CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {400, 600, 400, 500}, "Child") == 0);
    CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {600, 600, 600, 500}, "Parent") == 1);
    CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {2}) == 1);
    CHECK(cmd(b, PG_ARM_CMD_CON_ADD, 0, {CONSTRAINT_TYPE_CHILDOF}) == 0);
    CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, 0, {0, 0, 0}, "Parent") == 1);
    cmd(b, PG_ARM_CMD_POSE_SELECT, 1, {0});
    CHECK(cmd(b, PG_ARM_CMD_POSE_ROTATE, -1, {(float)(M_PI / 2)}) == 1);
    /* Child is bone 0 and its target bone 1: the target is solved first (depsgraph order), so the
     * child turns 90 degrees about the parent head (600, 600): (400, 600) -> (600, 800) */
    NEAR(info(b, 0, 26), 600, 0.2); NEAR(info(b, 0, 27), 800, 0.2);
    /* Clear Inverse: parmat * owner (childof_evaluate), the rest offset turns with the parent:
     * (0.4, -0.6) -> (0.6, 0.4) + head (0.6, -0.6) = (1.2, -0.2) units = (1200, 200) px */
    CHECK(cmd(b, PG_ARM_CMD_CON_CHILDOF_INVERSE, 0, {0, 0}) == 1);
    NEAR(info(b, 0, 26), 1200, 0.2); NEAR(info(b, 0, 27), 200, 0.2);
    /* Set Inverse with the parent turned: the child goes back to its rest head */
    CHECK(cmd(b, PG_ARM_CMD_CON_CHILDOF_INVERSE, 0, {0, 1}) == 1);
    NEAR(info(b, 0, 26), 400, 0.2); NEAR(info(b, 0, 27), 600, 0.2);
  }
  { /* addSwapHelperThroughController */
    Backend b;
    CHECK(b.initialize() && b.create_document() && b.create_layer("Eyes") && b.create_frame(1));
    CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
    const int mod = b.modifier_add(0, PG_MOD_TIME);
    CHECK(mod == 0);
    float sm[4];
    pg_arm_bridge_query(b, PG_ARM_Q_SUMMARY, -1, 0, sm, 4);
    const int m = (int)sm[2];
    if (m != 1) cmd(b, PG_ARM_CMD_SET_MODE, -1, {1});
    CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {80, 640, 80, 580}, "Swap.Eyes") == 0);
    cmd(b, PG_ARM_CMD_SET_MODE, -1, {(float)m});
    const int d = cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {0, DRIVER_TYPE_PYTHON, 0}, "layers[\"Eyes\"].modifiers[0].offset");
    CHECK(d == 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_ADD, -1, {(float)d, DVAR_TYPE_TRANSFORM_CHAN}, "swap") == 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_TRANSFORM, -1, {(float)d, 0, DTAR_TRANSCHAN_LOCX, 0,
                                                       DTAR_FLAG_LOCALSPACE | DTAR_FLAG_LOCAL_CONSTS}, "Swap.Eyes") == 1);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_EXPRESSION, -1, {(float)d}, "round(swap)") == 1);
    float p[10];
    CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 0, 0, p, 10) == 10);
    p[0] = 4;
    CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_POSE_SET, 0, p, 10, nullptr) >= 0);
    int type = 0, en = 0;
    float params[PG_MOD_MAX_PARAMS];
    b.modifier_get(0, 0, &type, &en, params, PG_MOD_MAX_PARAMS);
    NEAR(params[PG_P_TIME_OFFSET], 4, 0);
  }
  { /* masterSliderDrivesBonesAndDrawingSwap after save / reload: the rig is restored before the
     * layers, its layer drivers must still work once the layers exist */
    std::string json;
    {
      Backend b;
      CHECK(b.initialize() && b.create_document() && b.create_layer("Mouth") && b.create_frame(1));
      CHECK(b.modifier_add(0, PG_MOD_TIME) == 0);
      CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
      CHECK(cmd(b, PG_ARM_CMD_PROP_ADD, -1, {0, 0, 1, 0}, "slider") == 0);
      const int d = cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {0, DRIVER_TYPE_PYTHON, 0}, "layers[\"Mouth\"].modifiers[0].offset");
      CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_ADD, -1, {(float)d, DVAR_TYPE_SINGLE_PROP}, "var") == 0);
      CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_PATH, -1, {(float)d, 0}, "[\"slider\"]") == 1);
      CHECK(cmd(b, PG_ARM_CMD_DRIVER_EXPRESSION, -1, {(float)d}, "var*3.99") == 1);
      CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, 0.75f}) == 1);
      json = b.armature_json();
    }
    { /* ProjectDocumentCodec.restore: layers, their modifiers, then the rig (as Blender evaluates
       * a file once all of it is read) */
      Backend b;
      CHECK(b.initialize() && b.create_document() && b.create_layer("Lines") && b.create_frame(1));
      CHECK(b.create_layer("Mouth") && b.create_frame(1));
      CHECK(b.modifier_add(1, PG_MOD_TIME) == 0);
      CHECK(b.select_frame(1));
      CHECK(b.armature_load_json(json.c_str()));
      CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, 1.0f}) == 1);
      int type = 0, en = 0;
      float params[PG_MOD_MAX_PARAMS];
      b.modifier_get(1, 0, &type, &en, params, PG_MOD_MAX_PARAMS);
      NEAR(params[PG_P_TIME_OFFSET], 3, 0); /* 1.0 * 3.99; a new Time Offset is 1 */
    }
    Backend b;
    CHECK(b.initialize() && b.create_document());
    CHECK(b.armature_load_json(json.c_str())); /* rig before the layers (an older restore order) */
    CHECK(b.create_layer("Mouth") && b.create_frame(1));
    CHECK(b.modifier_add(0, PG_MOD_TIME) == 0);
    /* a new Time Offset has offset 1 (DNA default): 1.0 * 3.99 -> 3 tells the driver ran */
    CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, 1.0f}) == 1);
    int type = 0, en = 0;
    float params[PG_MOD_MAX_PARAMS];
    b.modifier_get(0, 0, &type, &en, params, PG_MOD_MAX_PARAMS);
    NEAR(params[PG_P_TIME_OFFSET], 3, 0);
    float c[16];
    pg_arm_bridge_query(b, PG_ARM_Q_DRIVER, -1, 0, c, 16);
    CHECK(((int)c[2] & DRIVER_FLAG_INVALID) == 0);
  }
  { /* the wave keys (6,-44) / (12,-38) from rest put the target 23.3 / 22.8 char px from the
     * shoulder. A straight rest arm (elbow x 25) is singular for the Jacobian solver: it cannot shorten,
     * so it misses by its spare length (24.74 - 23.32) * S = 7.2 px, as on the device and in Blender
     * (an IK chain needs a slight rest bend). Bent toward the pole (elbow x 26) it reaches. */
    NEAR(arm_miss(6, -44, 25), (24.739f - 23.324f) * S, 0.3);
    CHECK(arm_miss(6, -44, 26) < 3);
    CHECK(arm_miss(12, -38, 26) < 3);
    CHECK(arm_miss(6, -38, 26) < 3);
  }
  printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "OK", checks, failures);
  return failures ? 1 : 0;
}
