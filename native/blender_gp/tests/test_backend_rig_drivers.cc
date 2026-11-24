/* Host test of the rig drivers through the real Backend and bridge (project_grease_gp_backend.cpp,
 * project_grease_armature_bridge.cpp / project_grease_rig_bridge.cpp): the master slider case,
 * a custom property driving two bones, a layer's opacity / hide and its Time Offset modifier
 * (BKE_animsys_write_to_rna_path coercion and the RNA ranges of rna_gpencil_legacy*.c), on frame
 * change, after property edits, and through undo / redo. See tools/run_native_constraints_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstring>

#include "BLI_listbase.h"
#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "project_grease_armature.h"
#include "project_grease_driver.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"
#include "project_grease_modifier_stack.h"

/* The draw module is not part of this host test: the backend only needs these two to be non-null. */
extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;

int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);
int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap);

static int failures = 0;
static int checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)

static int cmd(Backend &b, int c, int bone, std::initializer_list<float> args, const char *name = nullptr)
{
  const float *p = args.size() ? args.begin() : nullptr;
  return pg_arm_bridge_command(b, c, bone, p, (int)args.size(), name);
}

static float time_offset(Backend &b, int layer)
{
  int type = 0, enabled = 0;
  float params[PG_MOD_MAX_PARAMS];
  b.modifier_get(layer, 0, &type, &enabled, params, PG_MOD_MAX_PARAMS);
  return type == PG_MOD_TIME ? params[PG_P_TIME_OFFSET] : -999.0f;
}

static bGPDlayer *layer(Backend &b, int i) { return (bGPDlayer *)BLI_findlink(&b.document_data()->layers, i); }

int main()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("Body") && b.create_frame(1));
  CHECK(b.create_layer("Mouth") && b.create_frame(1));
  CHECK(b.modifier_add(1, PG_MOD_TIME) == 0);

  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {400, 500, 400, 300}, "Brow.L") == 0);
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {600, 500, 600, 300}, "Brow.R") == 1);
  /* the slider: an object property 0 .. 1 */
  CHECK(cmd(b, PG_ARM_CMD_PROP_ADD, -1, {0, 0, 1, 0}, "slider") == 0);
  struct { const char *path; int index; const char *expr; } drivers[] = {
      {"pose.bones[\"Brow.L\"].location", 1, "var*0.1"},
      {"pose.bones[\"Brow.R\"].location", 1, "-var"},
      {"layers[\"Mouth\"].modifiers[0].offset", 0, "var*3.99"}, /* the drawing swap: 0 .. 3 */
      {"layers[\"Mouth\"].opacity", 0, "var*2"},                /* clamped to 0 .. 1 */
      {"layers[\"Body\"].hide", 0, "var"},                      /* > 1 - FLT_EPSILON */
  };
  for (auto &d : drivers) {
    const int i = cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {(float)d.index, DRIVER_TYPE_PYTHON, 0}, d.path);
    CHECK(i >= 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_ADD, -1, {(float)i, DVAR_TYPE_SINGLE_PROP}, "var") == 0);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_PATH, -1, {(float)i, 0}, "[\"slider\"]") == 1);
    CHECK(cmd(b, PG_ARM_CMD_DRIVER_EXPRESSION, -1, {(float)i}, d.expr) == 1);
  }
  float q[64];
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_DRIVERS, -1, 0, q, 64) == 1 && q[0] == 5);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_PROPS, -1, 0, q, 64) == 1 && q[0] == 1);
  CHECK(b.history_record());

  struct { float slider, offset, opacity; bool hide; } steps[] = {
      {0.0f, 0, 0.0f, false}, {0.3f, 1, 0.6f, false}, {0.75f, 2, 1.0f, false}, {1.0f, 3, 1.0f, true}};
  for (auto &s : steps) {
    /* a property edit re-evaluates the drivers at the scene frame */
    CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, s.slider}) == 1);
    CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 0, 0, q, 64) == 10);
    NEAR(q[1], s.slider * 0.1f, 1e-6);
    CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 1, 0, q, 64) == 10);
    NEAR(q[1], -s.slider, 1e-6);
    NEAR(time_offset(b, 1), s.offset, 0);
    NEAR(layer(b, 1)->opacity, s.opacity, 1e-6);
    CHECK(((layer(b, 0)->flag & GP_LAYER_HIDE) != 0) == s.hide);
  }
  /* frame change evaluates them too (a keyed slider would follow its F-Curve) */
  CHECK(b.select_frame(1));
  NEAR(time_offset(b, 1), 3, 0);

  /* the rig with its drivers is in the undo snapshot */
  CHECK(b.history_record());
  CHECK(b.history_undo());
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_DRIVERS, -1, 0, q, 64) == 1 && q[0] == 5);
  CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, 0.3f}) == 1);
  NEAR(time_offset(b, 1), 1, 0);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_PROP, -1, 0, q, 64) == 5);
  NEAR(q[1], 0.3, 1e-6);

  /* saved rig JSON restores the drivers (project document) */
  const std::string json = b.armature_json();
  CHECK(json.find("\"drivers\"") != std::string::npos && json.find("\"props\"") != std::string::npos);
  CHECK(b.armature_load_json(json.c_str()));
  CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, 1.0f}) == 1);
  NEAR(time_offset(b, 1), 3, 0);

  /* a constraint through the bridge: Copy Location of Brow.R onto Brow.L */
  CHECK(cmd(b, PG_ARM_CMD_CON_ADD, 1, {CONSTRAINT_TYPE_LOCLIKE}) == 0);
  CHECK(cmd(b, PG_ARM_CMD_CON_TARGET, 1, {0, 0, 0}, "Brow.L") == 1);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_CONSTRAINTS, 1, 0, q, 64) == 1 && q[0] == 1);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_CONSTRAINT, 1, 0, q, 64) == 1 + 5 + 1 && q[0] == CONSTRAINT_TYPE_LOCLIKE);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_BONE, 1, 0, q, 64) == PG_ARM_BONE_INFO);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_BONE, 0, 0, q, 0) == PG_ARM_BONE_INFO);
  float l[PG_ARM_BONE_INFO], r[PG_ARM_BONE_INFO];
  pg_arm_bridge_query(b, PG_ARM_Q_BONE, 0, 0, l, PG_ARM_BONE_INFO);
  pg_arm_bridge_query(b, PG_ARM_Q_BONE, 1, 0, r, PG_ARM_BONE_INFO);
  NEAR(r[26], l[26], 1e-3); /* pose heads in canvas px */
  NEAR(r[27], l[27], 1e-3);
  CHECK(cmd(b, PG_ARM_CMD_CON_REMOVE, 1, {0}) == 1);
  /* bad arguments */
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {0, 99, 0}, "[\"slider\"]") == -1);
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_KEYS, -1, {0, 1, 0, 0}) == -1); /* not whole triples */

  b.shutdown();
  printf("backend rig drivers: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
