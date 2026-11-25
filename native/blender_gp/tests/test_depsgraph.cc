/* Host test of the depsgraph port (project_grease_depsgraph.cc, Blender 3.6.23 depsgraph):
 * 1. the operation order of a rig with Child Of, IK, drivers on bones and a driver on a layer's
 *    Time Offset, built with the bones and drivers declared in both orders: every relation of
 *    deg_builder_relations(_rig).cc holds and the results are the same;
 * 2. through the real Backend + bridge: a loaded rig is evaluated once the document is complete
 *    (the rig restored before or after its layers), a pose edit refreshes the drivers reading the
 *    bone (not the action), onion-skin keys N-1 / N+1 deform with the pose of their own frame
 *    (copy-on-write per frame, the original untouched) and an exported frame K with the pose at K.
 * Built by tools/run_native_depsgraph_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_ID.h"
#include "project_grease_armature.h"
#include "project_grease_armature_bke.h"
#include "project_grease_constraint.h"
#include "project_grease_depsgraph.h"
#include "project_grease_driver.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"
#include "project_grease_modifier_stack.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;
using project_grease::gp::StrokePoint;
int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);
int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap);

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)

/* ---- 1. evaluation order ---------------------------------------------------------------- */

static int bone(PGArmature *a, const char *name, float hx, float hy, float tx, float ty)
{
  const float h[3] = {hx, hy, 0.0f}, t[3] = {tx, ty, 0.0f};
  return pg_arm_bone_add(a, name, h, t);
}
static int find_bone(const PGArmature *a, const char *name)
{
  for (int i = 0; i < pg_arm_bone_count(a); i++) {
    if (!strcmp(pg_arm_bone_name(a, i), name)) return i;
  }
  return -1;
}
static void pose_end(const PGArmature *a, const char *name, int which, float r[3])
{
  float info[PG_ARM_BONE_INFO];
  pg_arm_bone_info(a, find_bone(a, name), info);
  copy_v3_v3(r, info + (which ? 29 : 26));
}

struct FakeDoc {
  float offset = 0.0f;
};
static bool fake_get(void *u, const char *path, int, float *r)
{
  if (strcmp(path, "layers[\"L\"].modifiers[0].offset")) return false;
  *r = ((FakeDoc *)u)->offset;
  return true;
}
static bool fake_set(void *u, const char *path, int index, float v)
{
  if (index != 0 || strcmp(path, "layers[\"L\"].modifiers[0].offset")) return false;
  ((FakeDoc *)u)->offset = (float)(int)v;
  return true;
}

static int driver(PGArmature *a, const char *path, int index, const char *expr)
{
  const int d = pg_arm_driver_add(a, path, index, DRIVER_TYPE_PYTHON, false);
  CHECK(d >= 0 && pg_arm_driver_set_expression(a, d, expr));
  return d;
}
static void var_prop(PGArmature *a, int d, const char *path)
{
  const int v = pg_arm_driver_var_add(a, d, "var", DVAR_TYPE_SINGLE_PROP);
  CHECK(v >= 0 && pg_arm_driver_var_set_path(a, d, v, path));
}
static void var_transform(PGArmature *a, int d, const char *bone_name, int chan)
{
  const int v = pg_arm_driver_var_add(a, d, "var", DVAR_TYPE_TRANSFORM_CHAN);
  CHECK(v >= 0 && pg_arm_driver_var_set_transform(a, d, v, bone_name, chan, DTAR_ROTMODE_AUTO, 0));
}

/* position of `op` in the order, -1 when it did not run */
static int at(const std::string &order, const std::string &op)
{
  size_t p = 0;
  int i = 0;
  while (p < order.size()) {
    size_t e = order.find(' ', p);
    if (e == std::string::npos) e = order.size();
    if (order.compare(p, e - p, op) == 0) return i;
    p = e + 1;
    i++;
  }
  return -1;
}
#define BEFORE(order, x, y) do { const int _x = at(order, x), _y = at(order, y); checks++; \
    if (!(_x >= 0 && _y >= 0 && _x < _y)) { printf("FAIL %s:%d: %s (%d) before %s (%d)\n", __FILE__, __LINE__, x, _x, y, _y); failures++; } } while (0)

/* The rig: Owner (Child Of -> Parent), Parent driven by the slider (D1), Upper -> Lower chain with
 * IK on Lower towards Target, Target driven by the slider (D3), the layer's Time Offset driven by
 * Owner's world X (D2). reversed: the bones and drivers are declared the other way round. */
static void test_order(bool reversed, float r_results[8])
{
  PGArmature *a = pg_arm_new();
  auto chain = [&]() {
    const int U = bone(a, "Upper", 0, 0, 0, 1);
    const int L = bone(a, "Lower", 0, 1, sinf(0.2f), 1 + cosf(0.2f));
    pg_arm_edit_select(a, L, BONE_SELECTED, false);
    pg_arm_edit_select(a, U, BONE_SELECTED, true);
    CHECK(pg_arm_parent_set(a, PG_ARM_PAR_CONNECT) == 1);
  };
  if (!reversed) {
    bone(a, "Owner", -2, 0, -2, 1);
    bone(a, "Parent", 2, 0, 2, 1);
    chain();
    bone(a, "Target", 1, 1, 1, 1.5f);
  }
  else {
    bone(a, "Target", 1, 1, 1, 1.5f);
    chain();
    bone(a, "Parent", 2, 0, 2, 1);
    bone(a, "Owner", -2, 0, -2, 1);
  }
  pg_arm_update(a);
  CHECK(pg_arm_prop_add(a, -1, "slider", 0, 0, 1, 0) == 0);
  const int O = find_bone(a, "Owner"), Lo = find_bone(a, "Lower");
  const int c = pg_arm_con_add(a, O, CONSTRAINT_TYPE_CHILDOF, "");
  CHECK(c == 0 && pg_arm_con_set_target(a, O, c, 0, "Parent"));
  const int k = pg_arm_con_add(a, Lo, CONSTRAINT_TYPE_KINEMATIC, "");
  CHECK(k == 0 && pg_arm_con_set_target(a, Lo, k, 0, "Target"));
  const float kv[PG_CON_COMMON + 6] = {1, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD, 0, 0,
                                       CONSTRAINT_IK_TIP | CONSTRAINT_IK_POS, 1, 500, 0, 1, 1};
  CHECK(pg_arm_con_set(a, Lo, k, kv, PG_CON_COMMON + 6));
  auto d1 = [&]() { var_prop(a, driver(a, "pose.bones[\"Parent\"].location", 0, "var"), "[\"slider\"]"); };
  auto d2 = [&]() { var_transform(a, driver(a, "layers[\"L\"].modifiers[0].offset", 0, "var*2"), "Owner", DTAR_TRANSCHAN_LOCX); };
  auto d3 = [&]() { var_prop(a, driver(a, "pose.bones[\"Target\"].location", 1, "var"), "[\"slider\"]"); };
  if (!reversed) { d2(); d3(); d1(); }
  else { d1(); d3(); d2(); }

  FakeDoc doc;
  const PGArmExternal ext = {&doc, fake_get, fake_set};
  int fake_gpd = 0, geometry_evals = 0;
  Depsgraph *g = pg_deg_new(true);
  pg_deg_set_gpencil(g, (const bGPdata *)&fake_gpd, &ext, [](void *u) { (*(int *)u)++; }, &geometry_evals);
  pg_deg_set_rig(g, a, false);
  pg_deg_evaluate_on_framechange(g, 1.0f); /* slider 0: Child Of sets its inverse */
  CHECK(pg_deg_debug_cycles(g) == 0);
  CHECK(geometry_evals == 1);
  float h[3];
  pose_end(a, "Owner", 0, h);
  NEAR(h[0], -2.0, 1e-5);

  /* the slider moves: a property edit tags the rig (DEG_id_tag_update) */
  CHECK(pg_arm_prop_set(a, 0, 1.0f));
  pg_deg_id_tag_update(g, PG_DEG_ID_RIG, ID_RECALC_GEOMETRY);
  pg_deg_evaluate_on_refresh(g);
  char buf[8192];
  pg_deg_debug_last_order(g, buf, sizeof(buf));
  const std::string order = buf;
  /* COW first, the driver of Parent before Parent, Parent done before Owner's Child Of */
  BEFORE(order, "COPY_ON_WRITE()", "POSE_INIT()");
  BEFORE(order, "DRIVER(pose.bones[\"Parent\"].location)", "BONE_POSE_PARENT(Parent)");
  BEFORE(order, "BONE_DONE(Parent)", "BONE_CONSTRAINTS(Owner)");
  BEFORE(order, "BONE_CONSTRAINTS(Owner)", "BONE_READY(Owner)");
  BEFORE(order, "BONE_READY(Owner)", "BONE_DONE(Owner)");
  /* IK: the chain ready, the target done, then the solver, then the chain done */
  BEFORE(order, "DRIVER(pose.bones[\"Target\"].location)", "BONE_POSE_PARENT(Target)");
  BEFORE(order, "POSE_INIT_IK()", "POSE_IK_SOLVER(Lower)");
  BEFORE(order, "BONE_READY(Lower)", "POSE_IK_SOLVER(Lower)");
  BEFORE(order, "BONE_DONE(Target)", "POSE_IK_SOLVER(Lower)");
  BEFORE(order, "POSE_IK_SOLVER(Lower)", "BONE_DONE(Lower)");
  BEFORE(order, "BONE_DONE(Upper)", "BONE_POSE_PARENT(Lower)");
  BEFORE(order, "POSE_IK_SOLVER(Lower)", "POSE_CLEANUP()");
  /* the Time Offset driver reads Owner's final matrix; the GP modifiers run after the pose and
   * the driver */
  BEFORE(order, "BONE_DONE(Owner)", "DRIVER(layers[\"L\"].modifiers[0].offset)");
  BEFORE(order, "DRIVER(layers[\"L\"].modifiers[0].offset)", "GEOMETRY_EVAL_INIT()");
  BEFORE(order, "POSE_DONE()", "GEOMETRY_EVAL()");
  CHECK(geometry_evals == 2);

  /* the results: Owner follows Parent (+1 in x), the Lower tip points at Target's head (1, 2) */
  pose_end(a, "Owner", 0, h);
  NEAR(h[0], -1.0, 1e-5);
  NEAR(doc.offset, -2.0, 0); /* (int)(-1 * 2) */
  float t[3];
  pose_end(a, "Lower", 1, t);
  NEAR(t[0], sqrtf(0.5f), 2e-4);
  NEAR(t[1], 1 + sqrtf(0.5f), 2e-4);
  /* the original holds the flushed values; the evaluated copy is another object */
  PGArmature *cow = pg_deg_rig_evaluated(g);
  CHECK(cow && cow != a && pg_deg_rig_original(g) == a);
  float info[PG_ARM_BONE_INFO];
  pg_arm_bone_info(cow, find_bone(cow, "Owner"), info);
  NEAR(info[26], h[0], 1e-6);
  r_results[0] = h[0];
  r_results[1] = doc.offset;
  r_results[2] = t[0];
  r_results[3] = t[1];

  /* nothing tagged: a refresh runs nothing */
  pg_deg_evaluate_on_refresh(g);
  CHECK(geometry_evals == 2);
  pg_deg_free(g);
  pg_arm_free(a);
}

/* ---- 2. through the Backend ------------------------------------------------------------- */

static int cmd(Backend &b, int c, int bone, std::initializer_list<float> args, const char *name = nullptr)
{
  return pg_arm_bridge_command(b, c, bone, args.size() ? args.begin() : nullptr, (int)args.size(), name);
}
static float time_offset(Backend &b, int layer)
{
  int type = 0, enabled = 0;
  float params[PG_MOD_MAX_PARAMS];
  b.modifier_get(layer, 0, &type, &enabled, params, PG_MOD_MAX_PARAMS);
  return type == PG_MOD_TIME ? params[PG_P_TIME_OFFSET] : -999.0f;
}
static bGPDlayer *layer_at(Backend &b, int i) { return (bGPDlayer *)BLI_findlink(&b.document_data()->layers, i); }
static bGPDframe *key_at(bGPDlayer *l, int framenum)
{
  LISTBASE_FOREACH (bGPDframe *, f, &l->frames) {
    if (f->framenum == framenum) return f;
  }
  return nullptr;
}
static std::vector<float> points(const bGPDframe *f)
{
  std::vector<float> r;
  if (!f || !f->strokes.first) return r;
  const bGPDstroke *gps = (const bGPDstroke *)f->strokes.first;
  for (int i = 0; i < gps->totpoints; i++) {
    r.push_back(gps->points[i].x);
    r.push_back(gps->points[i].y);
  }
  return r;
}
static bool same(const std::vector<float> &x, const std::vector<float> &y, float e = 1e-4f)
{
  if (x.size() != y.size() || x.empty()) return false;
  for (size_t i = 0; i < x.size(); i++) {
    if (std::fabs(x[i] - y[i]) > e) return false;
  }
  return true;
}
static int driver_flag(Backend &b, int d)
{
  float q[64];
  return pg_arm_bridge_query(b, PG_ARM_Q_DRIVER, -1, (float)d, q, 64) >= 3 ? (int)q[2] : -1;
}

/* A rig whose slider drives the Time Offset of layer "Mouth" (offset = slider * 3). */
static std::string saved_rig()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("Mouth") && b.create_frame(1));
  CHECK(b.modifier_add(0, PG_MOD_TIME) == 0);
  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {400, 500, 400, 300}, "Jaw") == 0);
  CHECK(cmd(b, PG_ARM_CMD_PROP_ADD, -1, {1, 0, 1, 0}, "slider") == 0);
  const int d = cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {0, DRIVER_TYPE_PYTHON, 0}, "layers[\"Mouth\"].modifiers[0].offset");
  CHECK(d == 0);
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_ADD, -1, {0, DVAR_TYPE_SINGLE_PROP}, "var") == 0);
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_PATH, -1, {0, 0}, "[\"slider\"]") == 1);
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_EXPRESSION, -1, {0}, "var*3") == 1);
  NEAR(time_offset(b, 0), 3, 0);
  return b.armature_json();
}

static void test_load_then_evaluate(const std::string &json, bool rig_first)
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document());
  /* ProjectDocumentCodec.restore with the rig read before or after the layers */
  if (rig_first) CHECK(b.armature_load_json(json.c_str()));
  CHECK(b.create_layer("Mouth") && b.create_frame(1) && b.select_frame(1)); /* frame changes on the way */
  CHECK(b.create_frame(5) && b.select_frame(5) && b.select_frame(1));
  CHECK(b.modifier_add(0, PG_MOD_TIME) == 0);
  if (!rig_first) CHECK(b.armature_load_json(json.c_str()));
  /* the next draw / query evaluates the complete document */
  b.armature_refresh();
  NEAR(time_offset(b, 0), 3, 0);
  CHECK(driver_flag(b, 0) == 0); /* not DRIVER_FLAG_INVALID */
  CHECK(cmd(b, PG_ARM_CMD_PROP_SET, -1, {0, 0.34f}) == 1);
  NEAR(time_offset(b, 0), 1, 0);
}

static void test_pose_edit_refreshes_drivers()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("Lines") && b.create_frame(1));
  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {400, 500, 400, 300}, "Ctl") == 0);
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {600, 500, 600, 300}, "Follow") == 1);
  /* Follow's location x = Ctl's world X (Transform Channel), declared before anything moves */
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_ADD, -1, {0, DRIVER_TYPE_SUM, 0}, "pose.bones[\"Follow\"].location") == 0);
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_ADD, -1, {0, DVAR_TYPE_TRANSFORM_CHAN}, "x") == 0);
  CHECK(cmd(b, PG_ARM_CMD_DRIVER_VAR_TRANSFORM, -1, {0, 0, DTAR_TRANSCHAN_LOCX, DTAR_ROTMODE_AUTO, 0}, "Ctl") == 1);
  float q[64];
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 1, 0, q, 64) == 10);
  const float x0 = q[0]; /* Ctl's head x in Blender units */
  /* Ctl keyed at its rest pose at frame 1 (an action) */
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {2}) == 1);
  cmd(b, PG_ARM_CMD_POSE_SELECT, 0, {0});
  CHECK(cmd(b, PG_ARM_CMD_KEY_INSERT, -1, {1}) > 0);
  /* a pose edit: 100 px to the right; the driver follows, the edit stays (the action is not run) */
  CHECK(cmd(b, PG_ARM_CMD_POSE_TRANSLATE, -1, {100, 0}) == 1);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 0, 0, q, 64) == 10);
  NEAR(q[0], 100.0f / PG_ARM_PIXELS_PER_UNIT, 1e-5); /* the edit itself */
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 1, 0, q, 64) == 10);
  NEAR(q[0] - x0, 100.0f / PG_ARM_PIXELS_PER_UNIT, 1e-4);
  /* a frame change runs the action: Ctl back at its key, Follow with it */
  CHECK(b.select_frame(1));
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 1, 0, q, 64) == 10);
  NEAR(q[0], x0, 1e-5);
}

/* Keys 1, 2, 3 of one layer, each the vertical stroke (600,600) -> (600,300), weighted to one bone
 * which is keyed at rest at 1 and turned a quarter at 3. */
static void test_per_frame_cow()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("Lines"));
  StrokePoint pts[4] = {};
  for (int i = 0; i < 4; i++) { pts[i].x = 600; pts[i].y = 600 - 100 * i; pts[i].pressure = pts[i].strength = 1; }
  PGStrokeInfo info = {};
  info.thickness = 10;
  CHECK(cmd(b, PG_ARM_CMD_CREATE, -1, {}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_BONE_ADD, -1, {600, 600, 600, 300}, "Arm") == 0);
  for (int f = 1; f <= 3; f++) {
    CHECK(b.create_frame(f) && b.select_frame(f));
    CHECK(b.add_stroke(pts, 4, info));
    CHECK(cmd(b, PG_ARM_CMD_BIND, -1, {2}) >= 0);
  }
  CHECK(cmd(b, PG_ARM_CMD_SET_MODE, -1, {2}) == 1);
  CHECK(b.select_frame(1));
  cmd(b, PG_ARM_CMD_POSE_SELECT, 0, {0});
  CHECK(cmd(b, PG_ARM_CMD_KEY_INSERT, -1, {1}) > 0);
  CHECK(cmd(b, PG_ARM_CMD_POSE_ROTATE, -1, {(float)(M_PI / 2)}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_KEY_INSERT, -1, {3}) > 0);

  bGPDlayer *l = layer_at(b, 0);
  /* each frame as the scene frame: the reference */
  std::vector<float> at_frame[4];
  for (int f = 1; f <= 3; f++) {
    CHECK(b.select_frame(f));
    at_frame[f] = points(b.evaluated_frame(l, key_at(l, f), f));
  }
  CHECK(!same(at_frame[1], at_frame[3]) && !same(at_frame[1], at_frame[2]));
  NEAR(at_frame[1][6], 600, 0.5);  /* rest: the tip stays at (600, 300) */
  NEAR(at_frame[1][7], 300, 0.5);
  NEAR(at_frame[3][0], 600, 0.5);  /* a quarter turn about the head (600, 600) */
  NEAR(at_frame[3][1], 600, 0.5);
  NEAR(std::hypot(at_frame[3][6] - 600, at_frame[3][7] - 600), 300, 1.0);
  NEAR(at_frame[3][7], 600, 1.0);

  /* scene at 2: onion keys 1 and 3 with the pose of their own frame */
  CHECK(b.select_frame(2));
  float pose_before[64], pose_after[64];
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 0, 0, pose_before, 64) == 10);
  const std::vector<float> shown = points(b.evaluated_frame(l, key_at(l, 2), 2));
  CHECK(same(shown, at_frame[2]));
  CHECK(same(points(b.evaluated_frame(l, key_at(l, 1), 1, true)), at_frame[1]));
  CHECK(same(points(b.evaluated_frame(l, key_at(l, 3), 3, true)), at_frame[3]));
  /* the ghosts leave the original rig (the scene's pose at 2) and the shown frame alone */
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE, 0, 0, pose_after, 64) == 10);
  for (int i = 0; i < 10; i++) NEAR(pose_after[i], pose_before[i], 0);
  CHECK(same(points(b.evaluated_frame(l, key_at(l, 2), 2)), at_frame[2]));
  /* the original strokes are not deformed */
  NEAR(points(key_at(l, 3))[6], 600, 1e-4);
  NEAR(points(key_at(l, 3))[7], 300, 1e-4);

  /* export: frame K = 3 rendered while the scene is at 2 uses the pose at 3 */
  CHECK(same(points(b.evaluated_frame(l, key_at(l, 3), 3)), at_frame[3]));
  CHECK(same(points(b.evaluated_frame(l, key_at(l, 1), 1)), at_frame[1]));
  /* a rig edit drops the other frames' copies: frame 3 follows the new key */
  CHECK(b.select_frame(3));
  cmd(b, PG_ARM_CMD_POSE_SELECT, 0, {0});
  CHECK(cmd(b, PG_ARM_CMD_POSE_CLEAR, -1, {2}) == 1);
  CHECK(cmd(b, PG_ARM_CMD_KEY_INSERT, -1, {3}) > 0);
  CHECK(b.select_frame(2));
  CHECK(same(points(b.evaluated_frame(l, key_at(l, 3), 3, true)), at_frame[1]));
}

int main()
{
  float fwd[8], rev[8];
  test_order(false, fwd);
  test_order(true, rev);
  for (int i = 0; i < 4; i++) NEAR(fwd[i], rev[i], 1e-5);
  const std::string json = saved_rig();
  test_load_then_evaluate(json, true);
  test_load_then_evaluate(json, false);
  test_pose_edit_refreshes_drivers();
  test_per_frame_cow();
  printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "OK", checks, failures);
  return failures ? 1 : 0;
}
