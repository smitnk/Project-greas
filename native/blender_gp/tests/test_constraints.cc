/* Host test of the rig's bone constraints, IK and drivers (project_grease_constraint.c,
 * project_grease_driver.c) against Blender 3.6.23's maths. The bones lie in the armature's XY
 * plane (a bone along +y with roll 0 has an identity rest matrix, one along +x the -90 degree
 * z rotation), so every expected value below follows from the ported formulas by hand; the
 * derivation is written next to each check. Built by tools/run_native_constraints_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "MEM_guardedalloc.h"

#include "project_grease_armature.h"
#include "project_grease_armature_bke.h"
#include "project_grease_constraint.h"
#include "project_grease_driver.h"

static int failures = 0;
static int checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)

static int bone(PGArmature *a, const char *name, float hx, float hy, float tx, float ty)
{
  const float h[3] = {hx, hy, 0.0f}, t[3] = {tx, ty, 0.0f};
  return pg_arm_bone_add(a, name, h, t);
}

/* pose head / tail of a bone (armature space) */
static void pose_ends(const PGArmature *a, int b, float head[3], float tail[3])
{
  float info[PG_ARM_BONE_INFO];
  pg_arm_bone_info(a, b, info);
  copy_v3_v3(head, info + 26);
  copy_v3_v3(tail, info + 29);
}

#define NEAR_END(a, b, which, x, y, e) do { float _h[3], _t[3]; pose_ends(a, b, _h, _t); \
    const float *_p = (which) ? _t : _h; NEAR(_p[0], x, e); NEAR(_p[1], y, e); NEAR(_p[2], 0.0, e); } while (0)

/* loc 0, rotation theta about the bone's z, scale 1 */
static void pose_rot_z(PGArmature *a, int b, float theta, float lx = 0, float ly = 0)
{
  const float v[10] = {lx, ly, 0, cosf(theta / 2), 0, 0, sinf(theta / 2), 1, 1, 1};
  pg_arm_pose_set(a, b, v);
}

/* common settings: influence, owner space, target space, head/tail, mute; then the type's */
static void con_set(PGArmature *a, int b, int c, float own, float tar, std::initializer_list<float> type_values,
                    float influence = 1.0f, float headtail = 0.0f)
{
  float v[PG_CON_MAX_VALUES] = {influence, own, tar, headtail, 0.0f};
  int n = PG_CON_COMMON;
  for (float f : type_values) v[n++] = f;
  CHECK(pg_arm_con_set(a, b, c, v, n));
}

static void test_ik()
{
  /* Two-bone chain of length 1 + 1 from the origin, the second bent 0.2 rad clockwise so the
   * solver has a bend plane; target bone head at (1, 1): |target| = sqrt(2), so the elbow is a
   * right angle, at (0, 1) or (1, 0). IK_QJacobianSolver::ConstrainPoleVector turns the root
   * about the root->goal axis until its x axis (pole angle 0) faces the pole: with the pole at
   * (2, 0) the rest chain already does (no twist) and the clockwise bend gives the elbow (0, 1);
   * the pole at (0, 2) (or pole angle pi) needs the half turn, which mirrors the elbow to (1, 0). */
  for (int pass = 0; pass < 3; pass++) {
    PGArmature *a = pg_arm_new();
    const int A = bone(a, "Upper", 0, 0, 0, 1);
    const int B = bone(a, "Lower", 0, 1, sinf(0.2f), 1 + cosf(0.2f));
    pg_arm_edit_select(a, B, BONE_SELECTED, false);
    pg_arm_edit_select(a, A, BONE_SELECTED, true);
    CHECK(pg_arm_parent_set(a, PG_ARM_PAR_CONNECT) == 1);
    const int T = bone(a, "Target", 1, 1, 1, 1.5f);
    const int P = pass == 1 ? bone(a, "Pole", 0, 2, 0, 2.5f) : bone(a, "Pole", 2, 0, 2, 0.5f);
    pg_arm_update(a);

    const int c = pg_arm_con_add(a, B, CONSTRAINT_TYPE_KINEMATIC, "");
    CHECK(c == 0);
    float v[PG_CON_MAX_VALUES];
    CHECK(pg_arm_con_get(a, B, c, v, PG_CON_MAX_VALUES) == 1 + PG_CON_COMMON + 6);
    /* kinematic_new_data: weight 1, orientweight 1, iterations 500, TIP | STRETCH | POS */
    NEAR(v[0], CONSTRAINT_TYPE_KINEMATIC, 0);
    NEAR(v[6], CONSTRAINT_IK_TIP | CONSTRAINT_IK_STRETCH | CONSTRAINT_IK_POS, 0);
    NEAR(v[8], 500, 0);
    NEAR(v[10], 1.0, 0);
    /* no target yet: initialize_posetree skips it, the chain keeps its rest pose */
    NEAR_END(a, B, 1, sinf(0.2f), 1 + cosf(0.2f), 1e-6);

    CHECK(pg_arm_con_set_target(a, B, c, 0, "Target"));
    CHECK(pg_arm_con_set_target(a, B, c, 1, "Pole"));
    /* use tail, stretch off, chain 2, 500 iterations, pole angle 0 / pi */
    con_set(a, B, c, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD,
            {CONSTRAINT_IK_TIP | CONSTRAINT_IK_POS, 2, 500, pass == 2 ? float(M_PI) : 0.0f, 1, 1});
    const bool flipped = pass != 0;
    NEAR_END(a, B, 1, 1.0, 1.0, 2e-4);                        /* the tip reaches the target */
    NEAR_END(a, A, 0, 0.0, 0.0, 1e-6);                        /* the root stays */
    NEAR_END(a, A, 1, flipped ? 1.0 : 0.0, flipped ? 0.0 : 1.0, 2e-4); /* the elbow */
    NEAR_END(a, B, 0, flipped ? 1.0 : 0.0, flipped ? 0.0 : 1.0, 2e-4); /* connected */
    (void)T;
    (void)P;

    /* influence 0: the constraint is skipped (con->enforce == 0) */
    con_set(a, B, c, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD,
            {CONSTRAINT_IK_TIP | CONSTRAINT_IK_POS, 2, 500, 0, 1, 1}, 0.0f);
    NEAR_END(a, B, 1, sinf(0.2f), 1 + cosf(0.2f), 1e-5);
    /* chain 1: only the lower bone turns, about its head (0, 1), towards (1, 1): tail (1, 1) */
    con_set(a, B, c, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD, {CONSTRAINT_IK_TIP | CONSTRAINT_IK_POS, 1, 500, 0, 1, 1});
    CHECK(pg_arm_con_set_target(a, B, c, 1, nullptr)); /* no pole */
    NEAR_END(a, A, 1, 0.0, 1.0, 1e-6);
    NEAR_END(a, B, 1, 1.0, 1.0, 2e-4);

    /* the rig copy (undo snapshot) and the JSON keep the constraint */
    PGArmature *d = pg_arm_dup(a);
    NEAR_END(d, B, 1, 1.0, 1.0, 2e-4);
    char *json = pg_arm_to_json(a);
    PGArmature *r = pg_arm_from_json(json);
    CHECK(r && pg_arm_con_count(r, B) == 1);
    float w[PG_CON_MAX_VALUES];
    pg_arm_con_get(r, B, 0, w, PG_CON_MAX_VALUES);
    pg_arm_con_get(a, B, 0, v, PG_CON_MAX_VALUES);
    for (int i = 0; i < 1 + PG_CON_COMMON + 6; i++) NEAR(w[i], v[i], 1e-6);
    NEAR_END(r, B, 1, 1.0, 1.0, 2e-4);
    const char *sub = nullptr;
    CHECK(pg_con_target_get(r, B, 0, 0, &sub) && sub && strcmp(sub, "Target") == 0);
    CHECK(pg_con_target_get(r, B, 0, 1, &sub) && sub == nullptr);
    free(json);
    pg_arm_free(r);
    pg_arm_free(d);
    pg_arm_free(a);
  }
}

static void test_copy_rotation()
{
  /* Owner along +x (rest = -90 deg about z), target along +y posed 30 deg about z. */
  PGArmature *a = pg_arm_new();
  const int O = bone(a, "Owner", 0, 0, 1, 0);
  const int T = bone(a, "Target", 3, 0, 3, 1);
  pose_rot_z(a, T, float(M_PI / 6));
  const int c = pg_arm_con_add(a, O, CONSTRAINT_TYPE_ROTLIKE, "");
  CHECK(pg_arm_con_set_target(a, O, c, 0, "Target"));
  const float s30 = 0.5f, c30 = cosf(float(M_PI / 6));
  /* World / World: the owner takes the target's world rotation (y axis (-sin30, cos30)) */
  NEAR_END(a, O, 1, -s30, c30, 1e-6);
  NEAR_END(a, O, 0, 0.0, 0.0, 1e-6);
  /* Local / Local: the owner's local rotation becomes 30 deg: +x turned by 30 deg */
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_LOCAL, {ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z, CONSTRAINT_EULER_AUTO, ROTLIKE_MIX_REPLACE});
  NEAR_END(a, O, 1, c30, s30, 1e-6);
  /* invert Z: -30 deg */
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_LOCAL,
          {ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z | ROTLIKE_Z_INVERT, CONSTRAINT_EULER_AUTO, ROTLIKE_MIX_REPLACE});
  NEAR_END(a, O, 1, c30, -s30, 1e-6);
  /* mix Add with the owner's own 10 deg: 40 deg */
  pose_rot_z(a, O, float(M_PI / 18));
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_LOCAL, {ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z, CONSTRAINT_EULER_AUTO, ROTLIKE_MIX_ADD});
  NEAR_END(a, O, 1, cosf(float(M_PI * 40 / 180)), sinf(float(M_PI * 40 / 180)), 1e-6);
  /* only X (no Z): Replace keeps the owner's own z (10 deg) */
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_LOCAL, {ROTLIKE_X, CONSTRAINT_EULER_AUTO, ROTLIKE_MIX_REPLACE});
  NEAR_END(a, O, 1, cosf(float(M_PI / 18)), sinf(float(M_PI / 18)), 1e-6);
  /* influence 0.5, Replace in Local: interp_m4_m4m4 halfway between 10 and 30 deg = 20 deg */
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_LOCAL, {ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z, CONSTRAINT_EULER_AUTO, ROTLIKE_MIX_REPLACE}, 0.5f);
  NEAR_END(a, O, 1, cosf(float(M_PI / 9)), sinf(float(M_PI / 9)), 1e-5);
  /* Local with Parent target space: arm_mat^-1 * pose_mat of the target (identity rest): 30 deg */
  pose_rot_z(a, O, 0);
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_PARLOCAL, {ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z, CONSTRAINT_EULER_AUTO, ROTLIKE_MIX_REPLACE});
  NEAR_END(a, O, 1, c30, s30, 1e-6);
  /* mute */
  float v[PG_CON_MAX_VALUES] = {1, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD, 0, 1};
  CHECK(pg_arm_con_set(a, O, c, v, PG_CON_COMMON));
  NEAR_END(a, O, 1, 1.0, 0.0, 1e-6);
  pg_arm_free(a);
}

static void test_copy_location()
{
  PGArmature *a = pg_arm_new();
  const int O = bone(a, "Owner", 0, 0, 0, 1);
  const int T = bone(a, "Target", 2, 3, 2, 4);
  const int c = pg_arm_con_add(a, O, CONSTRAINT_TYPE_LOCLIKE, "");
  CHECK(pg_arm_con_set_target(a, O, c, 0, "Target"));
  NEAR_END(a, O, 0, 2.0, 3.0, 1e-6); /* loclike_new_data: X | Y | Z */
  NEAR_END(a, O, 1, 2.0, 4.0, 1e-6);
  /* head/tail 1: the target's tail (2, 4) */
  con_set(a, O, c, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD, {LOCLIKE_X | LOCLIKE_Y | LOCLIKE_Z}, 1.0f, 1.0f);
  NEAR_END(a, O, 0, 2.0, 4.0, 1e-6);
  /* offset: the owner's own location 0.1 along x is added */
  const float loc[10] = {0.1f, 0, 0, 1, 0, 0, 0, 1, 1, 1};
  pg_arm_pose_set(a, O, loc);
  con_set(a, O, c, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD, {LOCLIKE_X | LOCLIKE_Y | LOCLIKE_Z | LOCLIKE_OFFSET});
  NEAR_END(a, O, 0, 2.1, 3.0, 1e-6);
  /* X only, inverted: x = -2, y keeps 0 */
  con_set(a, O, c, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD, {LOCLIKE_X | LOCLIKE_X_INVERT});
  NEAR_END(a, O, 0, -2.0, 0.0, 1e-6);
  /* Local / Local: the target's local location (pose loc 0.5 along its y = world y) */
  const float tl[10] = {0, 0.5f, 0, 1, 0, 0, 0, 1, 1, 1};
  pg_arm_pose_set(a, T, tl);
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_LOCAL, {LOCLIKE_X | LOCLIKE_Y | LOCLIKE_Z});
  NEAR_END(a, O, 0, 0.0, 0.5, 1e-6);
  pg_arm_free(a);
}

static void test_damped_track()
{
  const float r = float(M_SQRT1_2);
  struct { int axis; float tx, ty; } cases[] = {
      {TRACK_Y, r, r},    /* y towards (1, 1) */
      {TRACK_nY, -r, -r}, /* -y towards the target */
      {TRACK_X, -r, r},   /* x towards the target: +45 deg, y = (-sin45, cos45) */
      {TRACK_nX, r, -r},  /* -x towards the target: -135 deg */
  };
  for (auto &k : cases) {
    PGArmature *a = pg_arm_new();
    const int O = bone(a, "Owner", 0, 0, 0, 1);
    bone(a, "Target", 1, 1, 1, 2);
    const int c = pg_arm_con_add(a, O, CONSTRAINT_TYPE_DAMPTRACK, "");
    CHECK(pg_arm_con_set_target(a, O, c, 0, "Target"));
    con_set(a, O, c, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_WORLD, {float(k.axis)});
    NEAR_END(a, O, 1, k.tx, k.ty, 1e-6);
    pg_arm_free(a);
  }
}

static void test_limit_rotation()
{
  PGArmature *a = pg_arm_new();
  const int O = bone(a, "Owner", 0, 0, 0, 1);
  pose_rot_z(a, O, float(M_PI / 3));
  const int c = pg_arm_con_add(a, O, CONSTRAINT_TYPE_ROTLIMIT, "");
  NEAR_END(a, O, 1, -sinf(float(M_PI / 3)), cosf(float(M_PI / 3)), 1e-6); /* no limits on */
  /* Z in -0.5 .. 0.5, Local, affect transform */
  con_set(a, O, c, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_WORLD,
          {LIMIT_ZROT, LIMIT_TRANSFORM, 0, 0, 0, 0, -0.5f, 0.5f, CONSTRAINT_EULER_AUTO});
  NEAR_END(a, O, 1, -sinf(0.5f), cosf(0.5f), 1e-6);
  float v[PG_CON_MAX_VALUES];
  pg_arm_con_get(a, O, c, v, PG_CON_MAX_VALUES);
  NEAR(v[7], LIMIT_TRANSFORM, 0);
  /* the other way: -60 deg -> -0.5 */
  pose_rot_z(a, O, float(-M_PI / 3));
  pg_arm_update(a);
  NEAR_END(a, O, 1, sinf(0.5f), cosf(0.5f), 1e-6);
  /* within the range: unchanged */
  pose_rot_z(a, O, 0.25f);
  pg_arm_update(a);
  NEAR_END(a, O, 1, -sinf(0.25f), cosf(0.25f), 1e-6);
  pg_arm_free(a);
}

static void test_child_of()
{
  PGArmature *a = pg_arm_new();
  const int O = bone(a, "Owner", 0, 0, 0, 1);
  const int T = bone(a, "Parent", 2, 0, 2, 1);
  const int c = pg_arm_con_add(a, O, CONSTRAINT_TYPE_CHILDOF, "");
  float v[PG_CON_MAX_VALUES];
  pg_arm_con_get(a, O, c, v, PG_CON_MAX_VALUES);
  /* add_new_constraint: pose space for bones, CHILDOF_ALL | SET_INVERSE pending */
  NEAR(v[2], CONSTRAINT_SPACE_POSE, 0);
  CHECK(((int)v[6] & CHILDOF_SET_INVERSE) != 0);
  CHECK(pg_arm_con_set_target(a, O, c, 0, "Parent"));
  /* the first evaluation with the target sets the inverse: the owner does not move */
  pg_arm_con_get(a, O, c, v, PG_CON_MAX_VALUES);
  CHECK(((int)v[6] & CHILDOF_SET_INVERSE) == 0);
  NEAR(v[7 + 12], -2.0, 1e-6); /* invmat translation = -(2, 0, 0) */
  NEAR_END(a, O, 0, 0.0, 0.0, 1e-6);
  NEAR_END(a, O, 1, 0.0, 1.0, 1e-6);
  /* the parent turns 90 deg about its head (2, 0): the owner follows, head (0,0) -> (2,-2),
   * direction +y -> -x */
  pose_rot_z(a, T, float(M_PI_2));
  pg_arm_update(a);
  NEAR_END(a, O, 0, 2.0, -2.0, 1e-5);
  NEAR_END(a, O, 1, 1.0, -2.0, 1e-5);
  /* clear inverse: owner = parent * own pose: head (2, 0), tail (1, 0) */
  CHECK(pg_arm_con_childof_inverse(a, O, c, false));
  NEAR_END(a, O, 0, 2.0, 0.0, 1e-5);
  NEAR_END(a, O, 1, 1.0, 0.0, 1e-5);
  /* set inverse again: back to the owner's own pose */
  CHECK(pg_arm_con_childof_inverse(a, O, c, true));
  NEAR_END(a, O, 0, 0.0, 0.0, 1e-5);
  NEAR_END(a, O, 1, 0.0, 1.0, 1e-5);
  /* location only: the parent's rotation is ignored, its move is followed; Set Inverse recomputes
   * invmat from the filtered parent */
  pose_rot_z(a, T, 0);
  CHECK(pg_arm_con_childof_inverse(a, O, c, true));
  con_set(a, O, c, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_WORLD, {CHILDOF_LOCX | CHILDOF_LOCY | CHILDOF_LOCZ | CHILDOF_SET_INVERSE});
  pose_rot_z(a, T, float(M_PI_2), 0, 0.5f);
  pg_arm_update(a);
  /* pose location is in the bone's rest space (chan_mat = loc * rot * size), so the parent head
   * moves by (0, 0.5) and the owner with it, unrotated */
  NEAR_END(a, O, 0, 0.0, 0.5, 1e-5);
  NEAR_END(a, O, 1, 0.0, 1.5, 1e-5);
  /* JSON keeps the inverse matrix */
  char *json = pg_arm_to_json(a);
  PGArmature *r = pg_arm_from_json(json);
  NEAR_END(r, O, 0, 0.0, 0.5, 1e-5);
  free(json);
  pg_arm_free(r);
  /* removal */
  CHECK(pg_arm_con_remove(a, O, c) == 1 && pg_arm_con_count(a, O) == 0);
  NEAR_END(a, O, 0, 0.0, 0.0, 1e-6);
  pg_arm_free(a);
}

static void test_stack_and_rename()
{
  /* Copy Location then Limit Rotation: the stack runs in order; move swaps them */
  PGArmature *a = pg_arm_new();
  const int O = bone(a, "Owner", 0, 0, 0, 1);
  bone(a, "T", 1, 1, 1, 2);
  const int c0 = pg_arm_con_add(a, O, CONSTRAINT_TYPE_LOCLIKE, "");
  const int c1 = pg_arm_con_add(a, O, CONSTRAINT_TYPE_DAMPTRACK, "");
  CHECK(c0 == 0 && c1 == 1);
  CHECK(pg_arm_con_set_target(a, O, 0, 0, "T") && pg_arm_con_set_target(a, O, 1, 0, "T"));
  /* copy location puts the owner on the target, then damped track has no direction: unchanged */
  NEAR_END(a, O, 0, 1.0, 1.0, 1e-6);
  NEAR_END(a, O, 1, 1.0, 2.0, 1e-6);
  CHECK(pg_arm_con_move(a, O, 1, 0));
  /* tracking first (towards (1,1)), then the location copy */
  NEAR_END(a, O, 0, 1.0, 1.0, 1e-6);
  NEAR_END(a, O, 1, 1.0 + M_SQRT1_2, 1.0 + M_SQRT1_2, 1e-6);
  /* names are BKE_constraint_unique_name'd */
  const int c2 = pg_arm_con_add(a, O, CONSTRAINT_TYPE_DAMPTRACK, "");
  CHECK(strcmp(pg_arm_con_at(a, O, c2)->name, "Damped Track.001") == 0);
  /* bone rename: subtargets follow (ED_armature_bone_rename) */
  char o[64], n[64];
  CHECK(pg_arm_bone_rename(a, 1, "Goal", o, n));
  const char *sub;
  CHECK(pg_con_target_get(a, O, 0, 0, &sub) && strcmp(sub, "Goal") == 0);
  /* bad input */
  CHECK(pg_arm_con_add(a, O, 999, "") == -1 && pg_arm_con_add(a, 99, CONSTRAINT_TYPE_LOCLIKE, "") == -1);
  CHECK(pg_arm_con_add(a, O, CONSTRAINT_TYPE_SPLINEIK, "") == -1); /* not supported */
  CHECK(!pg_arm_con_set(a, O, 9, nullptr, 0) && !pg_arm_con_move(a, O, 0, 7));
  pg_arm_free(a);
}

/* ---- drivers ---- */

static int add_var_transform(PGArmature *a, int d, const char *name, const char *bone_name, int chan, int flags)
{
  const int v = pg_arm_driver_var_add(a, d, name, DVAR_TYPE_TRANSFORM_CHAN);
  CHECK(v >= 0 && pg_arm_driver_var_set_transform(a, d, v, bone_name, chan, DTAR_ROTMODE_AUTO, flags));
  return v;
}

static int add_var_prop(PGArmature *a, int d, const char *name, const char *path)
{
  const int v = pg_arm_driver_var_add(a, d, name, DVAR_TYPE_SINGLE_PROP);
  CHECK(v >= 0 && pg_arm_driver_var_set_path(a, d, v, path));
  return v;
}

static float prop_value(const PGArmature *a, int bone_index, const char *name)
{
  float r[5] = {0};
  pg_arm_prop_get(a, pg_arm_prop_find(a, bone_index, name), r);
  return r[1];
}

static void test_bone_drives_bone()
{
  PGArmature *a = pg_arm_new();
  const int A = bone(a, "Ctrl", 0, 0, 0, 1);
  const int B = bone(a, "Driven", 2, 0, 2, 1);
  const int C = bone(a, "Follow", 4, 0, 4, 1);
  pose_rot_z(a, A, 0.4f);
  /* C, first in the list, reads B's world Z rotation, which the later drivers write: the
   * topological pass runs it after them, on B's re-evaluated pose */
  const int dc = pg_arm_driver_add(a, "pose.bones[\"Follow\"].location", 0, DRIVER_TYPE_AVERAGE, false);
  add_var_transform(a, dc, "rot", "Driven", DTAR_TRANSCHAN_ROTZ, 0);
  /* B's quaternion: w = cos(var / 2), z = sin(var / 2) with var = A's Z rotation (Transform space):
   * B turns 0.4 like A */
  const int dw = pg_arm_driver_add(a, "pose.bones[\"Driven\"].rotation_quaternion", 0, DRIVER_TYPE_PYTHON, false);
  const int dz = pg_arm_driver_add(a, "pose.bones[\"Driven\"].rotation_quaternion", 3, DRIVER_TYPE_PYTHON, false);
  CHECK(dc == 0 && dw == 1 && dz == 2);
  CHECK(pg_arm_driver_add(a, "pose.bones[\"Driven\"].rotation_quaternion", 3, DRIVER_TYPE_PYTHON, false) == dz);
  add_var_transform(a, dw, "var", "Ctrl", DTAR_TRANSCHAN_ROTZ, DTAR_FLAG_LOCALSPACE);
  add_var_transform(a, dz, "var", "Ctrl", DTAR_TRANSCHAN_ROTZ, DTAR_FLAG_LOCALSPACE);
  CHECK(pg_arm_driver_set_expression(a, dw, "cos(var * 0.5)"));
  CHECK(pg_arm_driver_set_expression(a, dz, "sin(var * 0.5)"));
  /* B's location x = var * 2 + 0.1 (= 0.9) and y = -var (= -0.4): world space, World matrix */
  const int dx = pg_arm_driver_add(a, "pose.bones[\"Driven\"].location", 0, DRIVER_TYPE_PYTHON, false);
  add_var_transform(a, dx, "var", "Ctrl", DTAR_TRANSCHAN_ROTZ, 0);
  CHECK(pg_arm_driver_set_expression(a, dx, "var*2+0.1"));
  const int dy = pg_arm_driver_add(a, "pose.bones[\"Driven\"].location", 1, DRIVER_TYPE_PYTHON, false);
  add_var_transform(a, dy, "var", "Ctrl", DTAR_TRANSCHAN_ROTZ, DTAR_FLAG_LOCALSPACE | DTAR_FLAG_LOCAL_CONSTS);
  CHECK(pg_arm_driver_set_expression(a, dy, "-var"));
  CHECK(pg_arm_driver_count(a) == 5);
  pg_arm_evaluate(a, 1.0f);
  float p[10];
  pg_arm_pose_get(a, B, p);
  NEAR(p[3], cosf(0.2f), 1e-6);
  NEAR(p[6], sinf(0.2f), 1e-6);
  NEAR(p[0], 0.9, 1e-6);
  NEAR(p[1], -0.4, 1e-6);
  /* B pose: rest at (2,0) + loc (0.9, -0.4), turned 0.4 */
  NEAR_END(a, B, 0, 2.9, -0.4, 1e-6);
  NEAR_END(a, B, 1, 2.9 - sinf(0.4f), -0.4 + cosf(0.4f), 1e-6);
  pg_arm_pose_get(a, C, p);
  NEAR(p[0], 0.4, 1e-6);
  float r[64];
  const int n = pg_arm_driver_get(a, dx, r, 64);
  CHECK(n == 8 + 3);
  NEAR(r[3], 0.9, 1e-6); /* driver curval */
  NEAR(r[4], 0.9, 1e-6); /* keys (0,0) (1,1) with linear extrapolation: identity */
  NEAR(r[9], 0.4, 1e-6); /* var value */
  /* the driver F-Curve maps: keys (0, 0) (1, 10) linear: 9 */
  const float keys[6] = {0, 0, BEZT_IPO_LIN, 1, 10, BEZT_IPO_LIN};
  CHECK(pg_arm_driver_set_keys(a, dx, keys, 2, 1));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_pose_get(a, B, p);
  NEAR(p[0], 9.0, 1e-5);
  /* no keys and a Generator 1 + 2 x: 2.8 */
  CHECK(pg_arm_driver_set_keys(a, dx, nullptr, 0, 0));
  const float gen[2] = {1, 2};
  CHECK(pg_arm_driver_set_generator(a, dx, FCM_GENERATOR_POLYNOMIAL, false, gen, 2));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_pose_get(a, B, p);
  NEAR(p[0], 2.8, 1e-6);
  /* factorised (2x + 1)(x - 1) at x = 0.9: 2.8 * -0.1 = -0.28; additive adds the 1-1 mapped 0.9 */
  const float fac[4] = {2, 1, 1, -1};
  CHECK(pg_arm_driver_set_generator(a, dx, FCM_GENERATOR_POLYNOMIAL_FACTORISED, false, fac, 4));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_pose_get(a, B, p);
  NEAR(p[0], -0.28, 1e-6);
  CHECK(pg_arm_driver_set_generator(a, dx, FCM_GENERATOR_POLYNOMIAL_FACTORISED, true, fac, 4));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_pose_get(a, B, p);
  NEAR(p[0], 0.9 - 0.28, 1e-6);
  /* "var" maps 1-1: 0.9 is gone, 0.4 */
  CHECK(pg_arm_driver_set_generator(a, dx, 0, false, nullptr, 0));
  CHECK(pg_arm_driver_set_expression(a, dx, "var"));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_pose_get(a, B, p);
  NEAR(p[0], 0.4, 1e-6);
  /* an expression the simple evaluator rejects (Python only) gives 0; division by zero sets
   * DRIVER_FLAG_INVALID until the driver is edited */
  CHECK(pg_arm_driver_set_expression(a, dx, "bpy.data.objects[0].location.x"));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_pose_get(a, B, p);
  NEAR(p[0], 0.0, 1e-7);
  CHECK(pg_arm_driver_set_expression(a, dx, "var / 0"));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_driver_get(a, dx, r, 64);
  CHECK(((int)r[2] & DRIVER_FLAG_INVALID) != 0);
  CHECK(pg_arm_driver_set_expression(a, dx, "var"));
  pg_arm_driver_get(a, dx, r, 64);
  CHECK(((int)r[2] & DRIVER_FLAG_INVALID) == 0);
  /* renaming the control bone keeps the drivers working */
  char o[64], nn[64];
  CHECK(pg_arm_bone_rename(a, A, "Control", o, nn));
  pg_arm_evaluate(a, 1.0f);
  pg_arm_pose_get(a, B, p);
  NEAR(p[6], sinf(0.2f), 1e-6);
  /* undo copy + JSON round trip evaluate the same */
  PGArmature *d = pg_arm_dup(a);
  char *json = pg_arm_to_json(a);
  PGArmature *rj = pg_arm_from_json(json);
  CHECK(rj && pg_arm_driver_count(rj) == 5);
  for (PGArmature *x : {d, rj}) {
    pose_rot_z(x, A, 0.6f);
    pg_arm_evaluate(x, 1.0f);
    pg_arm_pose_get(x, B, p);
    NEAR(p[6], sinf(0.3f), 1e-6);
    NEAR(p[1], -0.6, 1e-6);
  }
  free(json);
  pg_arm_free(rj);
  pg_arm_free(d);
  /* removal */
  CHECK(pg_arm_driver_remove(a, dc) && pg_arm_driver_count(a) == 4);
  pg_arm_free(a);
}

static void test_driver_types_and_order()
{
  PGArmature *a = pg_arm_new();
  const int A = bone(a, "Ctrl", 0, 0, 0, 1);
  pose_rot_z(a, A, 0.4f);
  /* object properties; WM_OT_properties_add defaults are 1.0 in 0 .. 1 */
  CHECK(pg_arm_prop_add(a, -1, "k", 0.25f, 0, 1, 1) == 0);
  CHECK(pg_arm_prop_add(a, -1, "out", 0, -10, 10, 0) == 1);
  CHECK(pg_arm_prop_add(a, A, "bone_prop", 0, -10, 10, 0) == 2);
  CHECK(pg_arm_prop_add(a, -1, "bad", 0, 1, -1, 0) == -1); /* min > max */
  CHECK(pg_arm_prop_set(a, 0, 7.0f) && prop_value(a, -1, "k") == 1.0f); /* clamped */
  CHECK(pg_arm_prop_set(a, 0, 0.25f));
  struct { int type; float expect; } types[] = {
      {DRIVER_TYPE_AVERAGE, (0.4f + 0.25f) / 2}, {DRIVER_TYPE_SUM, 0.65f},
      {DRIVER_TYPE_MIN, 0.25f}, {DRIVER_TYPE_MAX, 0.4f}};
  const int d = pg_arm_driver_add(a, "[\"out\"]", 0, DRIVER_TYPE_AVERAGE, false);
  add_var_transform(a, d, "rot", "Ctrl", DTAR_TRANSCHAN_ROTZ, DTAR_FLAG_LOCALSPACE);
  add_var_prop(a, d, "k", "[\"k\"]");
  for (auto &t : types) {
    CHECK(pg_arm_driver_set_type(a, d, t.type));
    pg_arm_evaluate(a, 1.0f);
    NEAR(prop_value(a, -1, "out"), t.expect, 1e-6);
  }
  /* Scripted with both variables and the frame: rot * k + frame = 0.1 + 3 */
  CHECK(pg_arm_driver_set_type(a, d, DRIVER_TYPE_PYTHON) && pg_arm_driver_set_expression(a, d, "rot * k + frame"));
  pg_arm_evaluate(a, 3.0f);
  NEAR(prop_value(a, -1, "out"), 3.1, 1e-6);
  /* a driven property is clamped to its range: 30 -> 10 */
  CHECK(pg_arm_driver_set_expression(a, d, "frame"));
  pg_arm_evaluate(a, 30.0f);
  NEAR(prop_value(a, -1, "out"), 10.0, 1e-6);
  /* variable names: validated and unique ("var", "var_001" ...) */
  const int v2 = pg_arm_driver_var_add(a, d, "k", DVAR_TYPE_SINGLE_PROP);
  CHECK(v2 == 2);
  /* chain: "bone_prop" = 2 * k is written by a driver created AFTER the one reading it
   * ("out" = bone_prop + 1): the topological pass evaluates the writer first */
  CHECK(pg_arm_driver_var_remove(a, d, v2));
  CHECK(pg_arm_driver_remove(a, d));
  const int reader = pg_arm_driver_add(a, "[\"out\"]", 0, DRIVER_TYPE_PYTHON, false);
  add_var_prop(a, reader, "var", "pose.bones[\"Ctrl\"][\"bone_prop\"]");
  CHECK(pg_arm_driver_set_expression(a, reader, "var + 1"));
  const int writer = pg_arm_driver_add(a, "pose.bones[\"Ctrl\"][\"bone_prop\"]", 0, DRIVER_TYPE_PYTHON, false);
  add_var_prop(a, writer, "var", "[\"k\"]");
  CHECK(pg_arm_driver_set_expression(a, writer, "var * 2"));
  pg_arm_evaluate(a, 1.0f);
  NEAR(prop_value(a, A, "bone_prop"), 0.5, 1e-6);
  NEAR(prop_value(a, -1, "out"), 1.5, 1e-6); /* not 1.0 (the stale bone_prop) */
  /* a Single Property variable on an array element and an invalid path */
  const int arr = pg_arm_driver_add(a, "[\"k\"]", 0, DRIVER_TYPE_SUM, false);
  add_var_prop(a, arr, "var", "pose.bones[\"Ctrl\"].rotation_quaternion[3]");
  pg_arm_evaluate(a, 1.0f);
  NEAR(prop_value(a, -1, "k"), sinf(0.2f), 1e-6);
  CHECK(pg_arm_driver_var_set_path(a, arr, 0, "pose.bones[\"Ctrl\"].rotation_quaternion[4]")); /* out of bounds */
  pg_arm_evaluate(a, 1.0f);
  float r[16];
  pg_arm_driver_get(a, arr, r, 16);
  CHECK(((int)r[2] & DRIVER_FLAG_INVALID) != 0 && ((int)r[10] & DTAR_FLAG_INVALID) != 0);
  CHECK(pg_arm_driver_add(a, "not.a.path\"", -1, DRIVER_TYPE_SUM, false) == -1);
  /* properties survive the JSON round trip */
  char *json = pg_arm_to_json(a);
  PGArmature *rj = pg_arm_from_json(json);
  CHECK(pg_arm_prop_count(rj) == 3);
  float pr[5];
  pg_arm_prop_get(rj, pg_arm_prop_find(rj, A, "bone_prop"), pr);
  NEAR(pr[0], A, 0);
  NEAR(pr[2], -10, 0);
  NEAR(pr[3], 10, 0);
  free(json);
  pg_arm_free(rj);
  pg_arm_free(a);
}

/* External properties (layers / Time Offset modifiers) resolved by the caller, as the backend does. */
struct FakeDoc {
  float opacity = 1.0f;
  float offset = 0.0f;
  bool hide = false;
};
static bool fake_get(void *u, const char *path, int, float *r)
{
  FakeDoc *d = (FakeDoc *)u;
  if (!strcmp(path, "layers[\"Mouth\"].opacity")) { *r = d->opacity; return true; }
  return false;
}
static bool fake_set(void *u, const char *path, int index, float v)
{
  FakeDoc *d = (FakeDoc *)u;
  if (index != 0) return false;
  if (!strcmp(path, "layers[\"Mouth\"].opacity")) { d->opacity = std::fmin(std::fmax(v, 0.0f), 1.0f); return true; }
  if (!strcmp(path, "layers[\"Mouth\"].hide")) { d->hide = v > 1.0f - FLT_EPSILON; return true; }
  if (!strcmp(path, "layers[\"Mouth\"].modifiers[0].offset")) { d->offset = (float)(int)v; return true; }
  return false;
}

static void test_master_slider()
{
  /* One object property "slider" (0 .. 1) drives two bones and the Time Offset of a layer (the
   * drawing swap), plus the layer's opacity. */
  PGArmature *a = pg_arm_new();
  const int L = bone(a, "Brow.L", -1, 0, -1, 1);
  const int R = bone(a, "Brow.R", 1, 0, 1, 1);
  CHECK(pg_arm_prop_add(a, -1, "slider", 0, 0, 1, 0) == 0);
  const int dl = pg_arm_driver_add(a, "pose.bones[\"Brow.L\"].location", 1, DRIVER_TYPE_PYTHON, false);
  add_var_prop(a, dl, "var", "[\"slider\"]");
  CHECK(pg_arm_driver_set_expression(a, dl, "var*0.5"));
  const int dr = pg_arm_driver_add(a, "pose.bones[\"Brow.R\"].location", 1, DRIVER_TYPE_PYTHON, false);
  add_var_prop(a, dr, "var", "[\"slider\"]");
  CHECK(pg_arm_driver_set_expression(a, dr, "-var"));
  const int dt = pg_arm_driver_add(a, "layers[\"Mouth\"].modifiers[0].offset", 0, DRIVER_TYPE_PYTHON, false);
  add_var_prop(a, dt, "var", "[\"slider\"]");
  CHECK(pg_arm_driver_set_expression(a, dt, "var*3.99"));
  const int dop = pg_arm_driver_add(a, "layers[\"Mouth\"].opacity", 0, DRIVER_TYPE_SUM, false);
  add_var_prop(a, dop, "var", "[\"slider\"]");
  /* a driver reading the layer opacity (external get): hide = opacity * 2 */
  const int dh = pg_arm_driver_add(a, "layers[\"Mouth\"].hide", 0, DRIVER_TYPE_PYTHON, false);
  add_var_prop(a, dh, "var", "layers[\"Mouth\"].opacity");
  CHECK(pg_arm_driver_set_expression(a, dh, "var * 2"));
  FakeDoc doc;
  const PGArmExternal ext = {&doc, fake_get, fake_set};
  struct { float slider, offset; bool hide; } steps[] = {{0, 0, false}, {0.3f, 1, false}, {0.5f, 1, true}, {1, 3, true}};
  for (auto &s : steps) {
    CHECK(pg_arm_prop_set(a, 0, s.slider));
    pg_arm_evaluate_ex(a, 1.0f, &ext);
    float p[10];
    pg_arm_pose_get(a, L, p);
    NEAR(p[1], s.slider * 0.5f, 1e-6);
    pg_arm_pose_get(a, R, p);
    NEAR(p[1], -s.slider, 1e-6);
    NEAR(doc.offset, s.offset, 0);         /* (int)(slider * 3.99) */
    NEAR(doc.opacity, s.slider, 1e-6);
    CHECK(doc.hide == s.hide);             /* opacity * 2 > 1 - FLT_EPSILON */
    NEAR_END(a, L, 0, -1.0, s.slider * 0.5f, 1e-6);
  }
  /* without the document the external drivers fail (DRIVER_FLAG_INVALID), the bones still work */
  pg_arm_evaluate(a, 1.0f);
  float r[16];
  pg_arm_driver_get(a, dt, r, 16);
  CHECK(((int)r[2] & DRIVER_FLAG_INVALID) != 0);
  pg_arm_free(a);
}

int main()
{
  test_ik();
  test_copy_rotation();
  test_copy_location();
  test_damped_track();
  test_limit_rotation();
  test_child_of();
  test_stack_and_rename();
  test_bone_drives_bone();
  test_driver_types_and_order();
  test_master_slider();
  printf("constraints: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
