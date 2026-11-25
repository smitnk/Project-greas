/* Host test of the Project Grease rig (project_grease_armature*.c / .cc) against Blender 3.6.23's
 * maths. Expected values are Blender's own: armature_test.cc / fcurve_test.cc of the pinned tree,
 * or worked out from the formulas of the ported functions (written next to each check).
 * Built by tools/run_native_armature_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_armature.h"

#include "project_grease_armature.h"
#include "project_grease_armature_bke.h"
#include "project_grease_modifier_stack.h"

static int failures = 0;
static int checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) do { checks++; const double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (e))) { \
    printf("FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, _a, _b); failures++; } } while (0)

static void no_cache(bGPdata *) {}

static void test_bone_matrices()
{
  /* armature_test.cc vec_roll_to_mat3_normalized cases */
  struct { float v[3]; float roll; float m[3][3]; } cases[] = {
      {{1, 0, 0}, 0.0f, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}},
      {{-1, 0, 0}, 0.0f, {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}}},
      {{0, 0, -1}, 0.0f, {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}},
      {{1, 1, 1}, 0.0f, {{0.788675129f, -0.577350259f, -0.211324856f}, {0.577350259f, 0.577350259f, 0.577350259f},
                         {-0.211324856f, -0.577350259f, 0.788675129f}}},
      {{-1, -1, -1}, 0.0f, {{0.211324856f, 0.577350259f, -0.788675129f}, {-0.577350259f, -0.577350259f, -0.577350259f},
                            {-0.788675129f, 0.577350259f, 0.211324856f}}},
      {{1, 1, 1}, float(M_PI_2), {{0.211324856f, 0.577350259f, -0.788675129f}, {0.577350259f, 0.577350259f, 0.577350259f},
                                  {0.788675129f, -0.577350259f, -0.211324856f}}},
  };
  for (auto &c : cases) {
    PGArmature *a = pg_arm_new();
    const float head[3] = {0.5f, -0.25f, 0.1f};
    float tail[3];
    add_v3_v3v3(tail, head, c.v);
    const int b = pg_arm_bone_add(a, "Bone", head, tail);
    pg_arm_bone_set(a, b, head, tail, c.roll);
    float info[PG_ARM_BONE_INFO];
    CHECK(pg_arm_bone_info(a, b, info));
    const float(*arm_mat)[4] = (const float(*)[4])(info + 10);
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) NEAR(arm_mat[i][j], c.m[i][j], 1e-6);
    NEAR(arm_mat[3][0], head[0], 1e-7);
    NEAR(arm_mat[3][1], head[1], 1e-7);
    NEAR(arm_mat[3][2], head[2], 1e-7);
    NEAR(info[9], len_v3(c.v), 1e-6);
    pg_arm_free(a);
  }
  /* armature_test.cc mat3_vec_to_roll Rotationmatrix: roll of an x rotation of 1.234 about (1,1,0) */
  float rot[3][3], roll;
  const float eul[3] = {1.234f, 0.0f, 0.0f};
  eul_to_mat3(rot, eul);
  const float xy[3] = {1.0f, 1.0f, 0.0f};
  mat3_vec_to_roll(rot, xy, &roll);
  NEAR(roll, 0.57158958f, 1e-6);

  /* Child bone: Bone.head is in the parent's tail space (armature_finalize_restpose), arm_mat =
   * parent arm_mat * offs_bone (BKE_armature_where_is_bone). Parent along +y of length 1, connected
   * child along +x: child local head (0,0,0), arm_mat rows = Rotate90_Z_CW, translation (0,1,0). */
  PGArmature *a = pg_arm_new();
  const float h0[3] = {0, 0, 0}, t0[3] = {0, 1, 0}, t1[3] = {1, 1, 0};
  const int p = pg_arm_bone_add(a, "Root", h0, t0);
  float d[3];
  sub_v3_v3v3(d, t1, t0);
  CHECK(pg_arm_extrude(a, d) == 1);
  const int c = pg_arm_bone_count(a) - 1;
  CHECK(strcmp(pg_arm_bone_name(a, c), "Root.001") == 0); /* ED_armature_ebone_unique_name */
  float info[PG_ARM_BONE_INFO];
  pg_arm_bone_info(a, c, info);
  CHECK((int)info[7] == p);
  CHECK(((int)info[8] & BONE_CONNECTED) != 0);
  const float(*m)[4] = (const float(*)[4])(info + 10);
  NEAR(m[0][1], -1.0, 1e-6);
  NEAR(m[1][0], 1.0, 1e-6);
  NEAR(m[2][2], 1.0, 1e-6);
  NEAR(m[3][0], 0.0, 1e-6);
  NEAR(m[3][1], 1.0, 1e-6);
  /* extruded bones get envelope radii from the length (recalcData_edit_armature, oldlength 0):
   * rad_head = min(0.25 * len, parent rad_tail 0.05), rad_tail 0.1 * len, dist 0.25 * len */
  float env[4];
  pg_arm_bone_envelope(a, c, env);
  NEAR(env[0], 0.25, 1e-6);
  NEAR(env[2], 0.05, 1e-6);
  NEAR(env[3], 0.1, 1e-6);
  pg_arm_free(a);
}

static void test_pose_evaluation()
{
  /* Root (0,0,0)->(0,1,0), connected child (0,1,0)->(0,2,0). Root rotated 90 deg about its local z
   * (quat (cos45, 0, 0, sin45)): pose_mat(root) = arm_mat * chan_mat, so the root's tail goes to
   * (-1, 0, 0) and the child (inheriting) points along -x: head (-1,0,0), tail (-2,0,0). */
  PGArmature *a = pg_arm_new();
  const float h0[3] = {0, 0, 0}, t0[3] = {0, 1, 0};
  const int r = pg_arm_bone_add(a, "Root", h0, t0);
  const float d[3] = {0, 1, 0};
  pg_arm_extrude(a, d);
  const int c = 1;
  float v[10] = {0, 0, 0, (float)M_SQRT1_2, 0, 0, (float)M_SQRT1_2, 1, 1, 1};
  CHECK(pg_arm_pose_set(a, r, v));
  float info[PG_ARM_BONE_INFO];
  pg_arm_bone_info(a, r, info);
  NEAR(info[29], -1.0, 1e-6);
  NEAR(info[30], 0.0, 1e-6);
  pg_arm_bone_info(a, c, info);
  NEAR(info[26], -1.0, 1e-6);
  NEAR(info[27], 0.0, 1e-6);
  NEAR(info[29], -2.0, 1e-6);
  NEAR(info[30], 0.0, 1e-6);
  /* location is ignored for a connected bone (BKE_pchan_to_mat4) but applies to the root:
   * loc is in the bone's rest space, root y axis is +y so loc (0.5, 0, 0) moves along x */
  float cv[10] = {3, 3, 3, 1, 0, 0, 0, 1, 1, 1};
  pg_arm_pose_set(a, c, cv);
  pg_arm_bone_info(a, c, info);
  NEAR(info[26], -1.0, 1e-6);
  float rv[10] = {0.5f, 0, 0, 1, 0, 0, 0, 2, 2, 2};
  pg_arm_pose_set(a, r, rv);
  pg_arm_bone_info(a, r, info);
  NEAR(info[26], 0.5, 1e-6);
  NEAR(info[29], 0.5, 1e-6);
  NEAR(info[30], 2.0, 1e-6); /* scale 2 doubles the length */

  /* rotate in pose mode about the view axis: +90 deg turns the root's +y onto -x */
  float ident[10] = {0, 0, 0, 1, 0, 0, 0, 1, 1, 1};
  pg_arm_pose_set(a, r, ident);
  pg_arm_pose_set(a, c, ident);
  pg_arm_pose_select(a, r, false);
  CHECK(pg_arm_pose_rotate(a, float(M_PI_2), PG_ARM_AROUND_MEDIAN) == 1);
  pg_arm_bone_info(a, c, info);
  NEAR(info[29], -2.0, 1e-5);
  NEAR(info[30], 0.0, 1e-5);
  pg_arm_pose_get(a, r, v);
  NEAR(v[3], M_SQRT1_2, 1e-6);
  NEAR(v[6], M_SQRT1_2, 1e-6);
  /* translate: delta in armature space -> location in bone space (bone y = +y) */
  const float delta[3] = {0.25f, 0.0f, 0.0f};
  pg_arm_pose_set(a, r, ident);
  pg_arm_pose_translate(a, delta);
  pg_arm_pose_get(a, r, v);
  NEAR(v[0], 0.25, 1e-6);
  pg_arm_bone_info(a, r, info);
  NEAR(info[26], 0.25, 1e-6);
  /* clear (pchan_clear_loc / rot / scale) */
  pg_arm_pose_scale(a, 2.0f, PG_ARM_AROUND_MEDIAN);
  pg_arm_pose_clear(a, 1 | 2 | 4);
  pg_arm_pose_get(a, r, v);
  for (int i = 0; i < 10; i++) NEAR(v[i], ident[i], 0);
  pg_arm_free(a);
}

static void test_fcurves()
{
  /* fcurve_test.cc: insert_vert_fcurve(1, 7) and (2, 13) with INSERTKEY_NO_USERPREF on
   * BKE_fcurve_create (auto_smoothing 0 there) */
  FCurve *fcu = static_cast<FCurve *>(MEM_callocN(sizeof(FCurve), "fcu"));
  CHECK(pg_bke_insert_vert_fcurve(fcu, 1.0f, 7.0f) == 0);
  CHECK(pg_bke_insert_vert_fcurve(fcu, 2.0f, 13.0f) == 1);
  CHECK(fcu->bezt[0].ipo == BEZT_IPO_BEZ);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.0f), 7.0, 1e-6);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.25f), 7.8297067f, 1e-5);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.50f), 10.0f, 1e-5);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.75f), 12.170294f, 1e-5);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 2.0f - 0.00008f), 13.0, 1e-5);
  fcu->bezt[0].vec[0][0] = 0.71855f;
  fcu->bezt[0].vec[0][1] = 6.22482f;
  fcu->bezt[0].vec[2][0] = 1.35148f;
  fcu->bezt[0].vec[2][1] = 7.96806f;
  fcu->bezt[1].vec[0][0] = 1.66667f;
  fcu->bezt[1].vec[0][1] = 10.4136f;
  fcu->bezt[1].vec[2][0] = 2.33333f;
  fcu->bezt[1].vec[2][1] = 15.5864f;
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.25f), 7.945497f, 1e-5);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.50f), 9.3495407f, 1e-5);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.75f), 11.088551f, 1e-5);
  fcu->extend = FCURVE_EXTRAPOLATE_LINEAR; /* ExtrapolationBezierKeys */
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 0.75f), 6.3114409f, 1e-5);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, -0.50f), 2.8686447f, 1e-5);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 2.75f), 18.81946f, 1e-4);
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 3.50f), 24.63892f, 1e-4);
  fcu->extend = FCURVE_EXTRAPOLATE_CONSTANT;
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 3.50f), 13.0, 1e-6);
  fcu->bezt[0].ipo = BEZT_IPO_LIN; /* InterpolationLinear */
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.25f), 8.5, 1e-5);
  fcu->bezt[0].ipo = BEZT_IPO_CONST; /* InterpolationConstant */
  NEAR(pg_bke_fcurve_eval_keyframes(fcu, 1.5f), 7.0, 1e-6);
  MEM_freeN(fcu->bezt);
  MEM_freeN(fcu);

  /* Keys through the rig: LocRotScale on a selected bone, scrub between them */
  PGArmature *a = pg_arm_new();
  const float h[3] = {0, 0, 0}, t[3] = {0, 1, 0};
  const int b = pg_arm_bone_add(a, "Bone", h, t);
  float v[10] = {0, 0, 0, 1, 0, 0, 0, 1, 1, 1};
  pg_arm_pose_set(a, b, v);
  CHECK(pg_arm_key_insert(a, 1.0f) == PG_ARM_CHANNELS);
  v[0] = 6.0f;
  pg_arm_pose_set(a, b, v);
  CHECK(pg_arm_key_insert(a, 2.0f) == PG_ARM_CHANNELS);
  CHECK(pg_arm_fcurve_count(a) == PG_ARM_CHANNELS);
  FCurve *loc = pg_arm_fcurve(a, b, 0);
  CHECK(loc && strcmp(loc->rna_path, "pose.bones[\"Bone\"].location") == 0 && loc->array_index == 0);
  CHECK(pg_arm_fcurve(a, b, 6) && pg_arm_fcurve(a, b, 6)->array_index == 3);
  float frames[8];
  CHECK(pg_arm_bone_keys(a, b, frames, 8) == 2);
  NEAR(frames[0], 1.0, 0);
  NEAR(frames[1], 2.0, 0);
  /* Keys made by the rig use Blender's default smoothing (Continuous Acceleration), which sets the
   * handle x size to 1/3 of the key distance (keyframing.c subdivide_nonauto_handles) and both
   * auto-clamped end keys flat: y(t) = 6 (3t^2 - 2t^3) with x linear in t, so at 1.25 y = 0.9375
   * (fcurve_test's 7.8297 is the same keys with smoothing None, checked above). */
  pg_arm_evaluate(a, 1.25f);
  pg_arm_pose_get(a, b, v);
  NEAR(v[0], 0.9375, 1e-5);
  pg_arm_evaluate(a, 1.5f);
  pg_arm_pose_get(a, b, v);
  NEAR(v[0], 3.0, 1e-5);
  CHECK(pg_arm_key_set_interpolation(a, b, 1.0f, BEZT_IPO_LIN) == PG_ARM_CHANNELS);
  CHECK(pg_arm_key_interpolation(a, b, 1.0f) == BEZT_IPO_LIN);
  pg_arm_evaluate(a, 1.25f);
  pg_arm_pose_get(a, b, v);
  NEAR(v[0], 1.5, 1e-5);
  pg_arm_key_set_interpolation(a, b, 1.0f, BEZT_IPO_CONST);
  pg_arm_evaluate(a, 1.9f);
  pg_arm_pose_get(a, b, v);
  NEAR(v[0], 0.0, 1e-6);
  /* a key replaced in place keeps the count (insert_bezt_fcurve replace) */
  pg_arm_key_insert(a, 2.0f);
  CHECK(pg_arm_bone_keys(a, b, frames, 8) == 2);
  CHECK(pg_arm_key_delete(a, b, 2.0f) == PG_ARM_CHANNELS);
  CHECK(pg_arm_bone_keys(a, b, frames, 8) == 1);
  /* dope sheet Grab: key 1 -> 5 stays sorted; landing on another key merges into one */
  pg_arm_key_insert(a, 3.0f);
  CHECK(pg_arm_key_move(a, b, 1.0f, 5.0f) == PG_ARM_CHANNELS);
  CHECK(pg_arm_bone_keys(a, b, frames, 8) == 2);
  NEAR(frames[0], 3.0, 1e-6);
  NEAR(frames[1], 5.0, 1e-6);
  CHECK(pg_arm_fcurve(a, b, 0)->bezt[0].vec[1][0] < pg_arm_fcurve(a, b, 0)->bezt[1].vec[1][0]);
  CHECK(pg_arm_key_move(a, b, 5.0f, 3.0f) == PG_ARM_CHANNELS);
  CHECK(pg_arm_bone_keys(a, b, frames, 8) == 1);
  CHECK(pg_arm_key_move(a, b, 9.0f, 3.0f) == 0);
  pg_arm_free(a);
}

static bGPDstroke *stroke_with_points(bGPDframe *gpf, const float (*canvas)[2], int n)
{
  bGPDstroke *gps = BKE_gpencil_stroke_new(0, n, 10);
  for (int i = 0; i < n; i++) {
    gps->points[i].x = canvas[i][0];
    gps->points[i].y = canvas[i][1];
    gps->points[i].z = 0;
    gps->points[i].pressure = 1;
    gps->points[i].strength = 1;
  }
  BLI_addtail(&gpf->strokes, gps);
  return gps;
}

static void test_deform_and_bind()
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache;
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, 1);
  /* bone from canvas (0,0) up to (0,-1000) = armature (0,0)->(0,1) */
  PGArmature *a = pg_arm_new();
  const float h[3] = {0, 0, 0}, t[3] = {0, 1, 0};
  const int b = pg_arm_bone_add(a, "Bone", h, t);
  /* bind points: on the bone, 0.1 away (dist^2 0.01 < decay_rad 0.02 -> 1), sqrt(0.06) away
   * (interpf(0, 0.9, (0.06 - 0.02) / 0.08) = 0.45), past the tip sphere (0), and 0.2 beyond the
   * tip inside the sphere (dist^2 0.04 to the tip: interpf(0, 0.9, 0.25) = 0.675). radsqr = len^2 *
   * DEFAULT_RATIO = 0.1, decay_rad = radsqr * (1 - DEFAULT_DECAY) = 0.02. */
  const float pts[5][2] = {{0, -500}, {100, -500}, {244.948974f, -500}, {0, -2000}, {0, -1200}};
  bGPDstroke *gps = stroke_with_points(gpf, pts, 5);
  CHECK(pg_arm_bind_frame(a, gpd, gpf, 1) == 1); /* With Empty Groups: group "Bone" */
  CHECK(BLI_listbase_count(&gpd->vertex_group_names) == 1);
  CHECK(strcmp(static_cast<bDeformGroup *>(gpd->vertex_group_names.first)->name, "Bone") == 0);
  CHECK(pg_arm_bind_frame(a, gpd, gpf, 2) == 0); /* Automatic Weights: group exists */
  const float expect[5] = {1.0f, 1.0f, 0.45f, 0.0f, 0.675f};
  for (int i = 0; i < 5; i++) NEAR(BKE_defvert_find_weight(&gps->dvert[i], 0), expect[i], 1e-5);
  CHECK(BKE_defvert_find_index(&gps->dvert[3], 0) != nullptr); /* written as 0 (BKE_defvert_ensure_index) */

  /* Armature modifier, vertex groups: rotate the bone 90 deg about z: armature (x, y) -> (-y, x),
   * canvas (cx, cy) -> (cy, -cx) for full weight; weight w blends linearly. */
  float v[10] = {0, 0, 0, (float)M_SQRT1_2, 0, 0, (float)M_SQRT1_2, 1, 1, 1};
  pg_arm_pose_set(a, b, v);
  pg_arm_register(gpd, a);
  bGPDstroke *e = BKE_gpencil_stroke_duplicate(gps, true, true);
  CHECK(pg_arm_deform_stroke(a, gpd, e, ARM_DEF_VGROUP, nullptr));
  NEAR(e->points[0].x, -500.0, 1e-3);
  NEAR(e->points[0].y, 0.0, 1e-3);
  /* weight 0.45 on (244.9, -500): the sum is divided by the total contribution (contrib 0.45), so
   * one bone moves it fully: arm (0.2449, 0.5) -> (-0.5, 0.2449) = canvas (-500, -244.9) */
  NEAR(e->points[2].x, -500.0, 1e-2);
  NEAR(e->points[2].y, -244.948974, 1e-2);
  NEAR(e->points[3].x, 0.0, 1e-3); /* weight 0: contrib 0 -> untouched */
  NEAR(e->points[3].y, -2000.0, 1e-3);
  BKE_gpencil_free_stroke(e);
  /* invert vertex group with the armature group itself: weight 1 -> armature_weight 0 -> no move */
  e = BKE_gpencil_stroke_duplicate(gps, true, true);
  pg_arm_deform_stroke(a, gpd, e, ARM_DEF_VGROUP | ARM_DEF_INVERT_VGROUP, "Bone");
  NEAR(e->points[0].x, 0.0, 1e-4);
  NEAR(e->points[0].y, -500.0, 1e-4);
  /* and point 2 (weight 0.45) moves by bone weight * (1 - 0.45) */
  NEAR(e->points[2].x, 244.948974 + 0.55 * (-500.0 - 244.948974), 1e-2);
  BKE_gpencil_free_stroke(e);
  /* envelope only: distfactor_to_bone with dist 0.25, rad 0.1/0.05: (0.1, 0.5) is inside the
   * radius (0.075 at the middle)? no: 0.1 > 0.075 -> 1 - ((0.1 - 0.075)^2 / 0.25^2) = 0.99 */
  NEAR(pg_bke_distfactor_to_bone((const float[3]){0.1f, 0.5f, 0}, h, t, 0.1f, 0.05f, 0.25f), 0.99, 1e-6);
  NEAR(pg_bke_distfactor_to_bone((const float[3]){0.0f, 0.5f, 0}, h, t, 0.1f, 0.05f, 0.25f), 1.0, 0);
  NEAR(pg_bke_distfactor_to_bone((const float[3]){0.5f, 0.5f, 0}, h, t, 0.1f, 0.05f, 0.25f), 0.0, 0);
  e = BKE_gpencil_stroke_duplicate(gps, true, true);
  BKE_gpencil_free_stroke_weights(e);
  MEM_SAFE_FREE(e->dvert);
  pg_arm_deform_stroke(a, gpd, e, ARM_DEF_ENVELOPE, nullptr);
  /* single bone: contrib-normalised, so any envelope weight > 0 moves the point fully */
  NEAR(e->points[1].x, -500.0, 1e-2); /* (100,-500): arm (0.1,0.5) -> (-0.5,0.1) = canvas (-500,-100) */
  NEAR(e->points[1].y, -100.0, 1e-2);
  BKE_gpencil_free_stroke(e);

  /* Preserve volume (dual quaternions): two bones from the same head, one rotated 90 deg, a point
   * weighted 0.5 / 0.5: linear blending pulls it towards the head (length * cos 45), the dual
   * quaternion blend keeps the distance from the head. */
  PGArmature *a2 = pg_arm_new();
  const int b1 = pg_arm_bone_add(a2, "A", h, t);
  const int b2 = pg_arm_bone_add(a2, "B", h, t);
  float q[10] = {0, 0, 0, (float)M_SQRT1_2, 0, 0, (float)M_SQRT1_2, 1, 1, 1};
  pg_arm_pose_set(a2, b2, q);
  (void)b1;
  bGPdata *gpd2 = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd2"));
  bGPDlayer *gpl2 = BKE_gpencil_layer_addnew(gpd2, "L", true, false);
  bGPDframe *gpf2 = BKE_gpencil_frame_addnew(gpl2, 1);
  const float one[1][2] = {{0, -1000}};
  bGPDstroke *s = stroke_with_points(gpf2, one, 1);
  pg_arm_bind_frame(a2, gpd2, gpf2, 1);
  BKE_gpencil_dvert_ensure(s);
  BKE_defvert_ensure_index(&s->dvert[0], 0)->weight = 0.5f;
  BKE_defvert_ensure_index(&s->dvert[0], 1)->weight = 0.5f;
  bGPDstroke *lin = BKE_gpencil_stroke_duplicate(s, true, true);
  bGPDstroke *dq = BKE_gpencil_stroke_duplicate(s, true, true);
  pg_arm_deform_stroke(a2, gpd2, lin, ARM_DEF_VGROUP, nullptr);
  pg_arm_deform_stroke(a2, gpd2, dq, ARM_DEF_VGROUP | ARM_DEF_QUATERNION, nullptr);
  NEAR(std::hypot(lin->points[0].x, lin->points[0].y), 1000.0 * M_SQRT1_2, 1e-2);
  NEAR(std::hypot(dq->points[0].x, dq->points[0].y), 1000.0, 1e-2);
  NEAR(dq->points[0].x, -1000.0 * std::sin(M_PI / 4), 1e-2); /* halfway: 45 deg */
  BKE_gpencil_free_stroke(lin);
  BKE_gpencil_free_stroke(dq);

  /* through the modifier stack (PG_MOD_ARMATURE on the layer) */
  PGModEntry entry;
  memset(&entry, 0, sizeof(entry));
  entry.type = PG_MOD_ARMATURE;
  entry.enabled = 1;
  pg_mod_defaults(PG_MOD_ARMATURE, entry.params);
  CHECK(entry.params[PG_P_ARMATURE_USE_VGROUPS] == 1.0f);
  bGPDframe eval;
  memset(&eval, 0, sizeof(eval));
  pg_mod_eval_frame(gpd, gpl, gpf, &entry, 1, 1, &eval);
  bGPDstroke *es = static_cast<bGPDstroke *>(eval.strokes.first);
  CHECK(es != nullptr);
  if (es) {
    NEAR(es->points[0].x, -500.0, 1e-3);
    NEAR(gps->points[0].x, 0.0, 0); /* originals untouched */
  }
  pg_mod_eval_free(&eval);
  /* in edit mode the armature does not deform (arm->edbo) */
  pg_arm_set_mode(a, PG_ARM_MODE_EDIT);
  e = BKE_gpencil_stroke_duplicate(gps, true, true);
  CHECK(pg_arm_deform_stroke(a, gpd, e, ARM_DEF_VGROUP, nullptr) == 0);
  BKE_gpencil_free_stroke(e);
  pg_arm_register(gpd, nullptr);

  pg_arm_free(a);
  pg_arm_free(a2);
  for (bGPdata *g : {gpd, gpd2}) {
    BKE_gpencil_free_layers(&g->layers);
    BLI_freelistN(&g->vertex_group_names);
    MEM_freeN(g);
  }
}

/* B-Bone deform (armature.c BKE_pchan_bbone_spline_setup / BKE_pchan_bbone_segments_cache_compute,
 * armature_deform.c b_bone_deform): a straight bone (0,0)->(0,1) with 2 segments and Roll Out 90
 * deg in the pose. The spline stays straight (both handles along +Y, symmetric, so the equalised
 * middle point is y = 0.5); segment rolls are interpf(roll2, roll1, a / segments): 0 at the head,
 * 45 deg in the middle, 90 deg at the tail. Rolling about the bone axis (armature Y) keeps y and
 * turns x into x cos(roll). b_bone_deform blends the two matrices around y / length * segments. */
static void test_bbone_deform()
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache;
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, 1);
  PGArmature *a = pg_arm_new();
  const float h[3] = {0, 0, 0}, t[3] = {0, 1, 0};
  const int b = pg_arm_bone_add(a, "Bone", h, t);
  float bb[PG_ARM_BBONE];
  CHECK(pg_arm_bone_bbone(a, b, bb) == 1); /* ED_armature_ebone_add: 1 segment, ease 1, scale 1 */
  NEAR(bb[0], 1.0, 0);
  NEAR(bb[1], 1.0, 0);
  NEAR(bb[8], 1.0, 0);
  NEAR(bb[13], 1.0, 0);
  CHECK(pg_arm_bone_set_bbone(a, b, 40, bb)); /* clamped to 32 */
  CHECK(pg_arm_bone_bbone(a, b, bb) == 32);
  CHECK(pg_arm_bone_set_bbone(a, b, 2, bb));
  /* canvas px = arm * 1000, y down: arm (0.1, 0.5), (0.1, 0.25), (0.1, 1) */
  const float pts[3][2] = {{100, -500}, {100, -250}, {100, -1000}};
  bGPDstroke *gps = stroke_with_points(gpf, pts, 3);
  pg_arm_bind_frame(a, gpd, gpf, 1);
  BKE_gpencil_dvert_ensure(gps);
  for (int i = 0; i < 3; i++) BKE_defvert_ensure_index(&gps->dvert[i], 0)->weight = 1.0f;
  float pb[PG_ARM_BBONE];
  CHECK(pg_arm_pose_bbone(a, b, pb));
  pb[3] = (float)M_PI_2; /* roll2 (Roll Out) */
  CHECK(pg_arm_pose_set_bbone(a, b, pb));
  bGPDstroke *e = BKE_gpencil_stroke_duplicate(gps, true, true);
  CHECK(pg_arm_deform_stroke(a, gpd, e, ARM_DEF_VGROUP, nullptr));
  const double c45 = std::cos(M_PI / 4);
  NEAR(e->points[0].x, 100.0 * c45, 1e-2);        /* y 0.5: index 1, blend 0 -> roll 45 */
  NEAR(e->points[0].y, -500.0, 1e-2);
  NEAR(e->points[1].x, 100.0 * (1 + c45) / 2, 1e-2); /* y 0.25: index 0, blend 0.5 */
  NEAR(e->points[1].y, -250.0, 1e-2);
  NEAR(e->points[2].x, 0.0, 1e-2);                /* the tail: roll 90 */
  NEAR(std::fabs(e->points[2].z), 100.0, 1e-2);
  BKE_gpencil_free_stroke(e);
  /* 1 segment: no B-Bone, the roll offsets do nothing (bone->segments > 1 test) */
  bb[0] = 1.0f;
  CHECK(pg_arm_bone_set_bbone(a, b, 1, bb));
  e = BKE_gpencil_stroke_duplicate(gps, true, true);
  pg_arm_deform_stroke(a, gpd, e, ARM_DEF_VGROUP, nullptr);
  NEAR(e->points[0].x, 100.0, 1e-3);
  BKE_gpencil_free_stroke(e);
  /* Curve In X 0.5 on 4 segments in the pose bends the spline: the head stays (deform matrix 0 is
   * at the bone head with no handle change), the middle moves along +x in armature space. */
  CHECK(pg_arm_bone_set_bbone(a, b, 4, bb));
  pb[3] = 0.0f;
  pb[4] = 0.5f;
  CHECK(pg_arm_pose_set_bbone(a, b, pb));
  e = BKE_gpencil_stroke_duplicate(gps, true, true);
  pg_arm_deform_stroke(a, gpd, e, ARM_DEF_VGROUP, nullptr);
  CHECK(e->points[0].x > 100.0f + 50.0f); /* the curve bulges by ~0.75 * 0.5 * 3/4 * ... */
  BKE_gpencil_free_stroke(e);
  /* the B-Bone values round-trip through the document JSON */
  char *json = pg_arm_to_json(a);
  PGArmature *r = pg_arm_from_json(json);
  float rb[PG_ARM_BBONE], rp[PG_ARM_BBONE];
  CHECK(pg_arm_bone_bbone(r, 0, rb) == 4);
  CHECK(pg_arm_pose_bbone(r, 0, rp));
  NEAR(rp[4], 0.5, 1e-6);
  NEAR(rb[8], 1.0, 1e-6);
  std::free(json);
  pg_arm_free(r);
  pg_arm_free(a);
  BKE_gpencil_free_layers(&gpd->layers);
  BLI_freelistN(&gpd->vertex_group_names);
  MEM_freeN(gpd);
}

/* Bind frames (gpencil_add_verts_to_dgroups): the active frame, and with multi-frame editing the
 * selected frames too. */
static void test_multiframe_bind()
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache;
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *f1 = BKE_gpencil_frame_addnew(gpl, 1);
  bGPDframe *f2 = BKE_gpencil_frame_addnew(gpl, 10);
  bGPDframe *f3 = BKE_gpencil_frame_addnew(gpl, 20);
  f1->flag &= ~GP_FRAME_SELECT;
  f2->flag &= ~GP_FRAME_SELECT;
  f3->flag |= GP_FRAME_SELECT;
  PGArmature *a = pg_arm_new();
  const float h[3] = {0, 0, 0}, t[3] = {0, 1, 0};
  pg_arm_bone_add(a, "Bone", h, t);
  const float on[1][2] = {{0, -500}}, near[1][2] = {{244.948974f, -500}};
  bGPDstroke *s1 = stroke_with_points(f1, on, 1);
  bGPDstroke *s2 = stroke_with_points(f2, near, 1);
  bGPDstroke *s3 = stroke_with_points(f3, on, 1);
  /* the single-frame bind only weights the frame given */
  CHECK(pg_arm_bind_frame(a, gpd, f2, 2) == 1);
  CHECK(s1->dvert == nullptr);
  /* layer bind, multi-frame editing off: the active frame only (Blender's parent_set default) */
  CHECK(pg_arm_bind_layer(a, gpd, gpl, f1, 2) == 0);
  CHECK(s1->dvert && s3->dvert == nullptr);
  NEAR(BKE_defvert_find_weight(&s1->dvert[0], 0), 1.0, 1e-6);
  /* on: the active frame and the GP_FRAME_SELECT ones (f3), not the unselected f2 again */
  BKE_defvert_ensure_index(&s2->dvert[0], 0)->weight = 0.0f;
  gpd->flag |= GP_DATA_STROKE_MULTIEDIT | GP_DATA_STROKE_EDITMODE; /* GPENCIL_MULTIEDIT_SESSIONS_ON */
  CHECK(pg_arm_bind_layer(a, gpd, gpl, f1, 2) == 0);
  CHECK(s3->dvert != nullptr);
  NEAR(BKE_defvert_find_weight(&s3->dvert[0], 0), 1.0, 1e-6);
  NEAR(BKE_defvert_find_weight(&s2->dvert[0], 0), 0.0, 0);
  CHECK(!(f1->flag & GP_FRAME_SELECT) && !(f2->flag & GP_FRAME_SELECT) && (f3->flag & GP_FRAME_SELECT));
  pg_arm_free(a);
  BKE_gpencil_free_layers(&gpd->layers);
  BLI_freelistN(&gpd->vertex_group_names);
  MEM_freeN(gpd);
}

/* X-Axis Mirror (armature_utils.c ED_armature_ebone_get_mirrored /
 * ED_armature_ebone_transform_mirror_update, armature_add.c armature_extrude_exec): moving the tip of
 * Arm.L also moves the tip of Arm.R to (-x, y, z) and sets its roll to -roll; extrude makes the
 * mirrored bone too. */
static void test_mirror_edit()
{
  PGArmature *a = pg_arm_new();
  const float lh[3] = {0.2f, 0, 0}, lt[3] = {0.5f, 0, 0}, rh[3] = {-0.2f, 0, 0}, rt[3] = {-0.5f, 0, 0};
  const int l = pg_arm_bone_add(a, "Arm.L", lh, lt);
  const int r = pg_arm_bone_add(a, "Arm.R", rh, rt);
  float info[PG_ARM_BONE_INFO];
  const float up[3] = {0, 0.3f, 0};
  /* off: the other side stays */
  pg_arm_edit_select(a, l, BONE_TIPSEL, false);
  pg_arm_edit_translate(a, up);
  pg_arm_bone_info(a, r, info);
  NEAR(info[4], 0.0, 0);
  pg_arm_edit_translate(a, (const float[3]){0, -0.3f, 0});
  /* on */
  pg_arm_set_mirror_x(a, true);
  CHECK(pg_arm_mirror_x(a));
  CHECK(pg_arm_edit_translate(a, up) == 1);
  pg_arm_bone_info(a, r, info);
  NEAR(info[0], -0.2, 1e-6); /* head not selected: stays */
  NEAR(info[3], -0.5, 1e-6);
  NEAR(info[4], 0.3, 1e-6);
  float linfo[PG_ARM_BONE_INFO];
  pg_arm_bone_info(a, l, linfo);
  NEAR(info[6], -linfo[6], 1e-6); /* roll */
  /* whole bone selected: head too */
  pg_arm_edit_select(a, l, BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL, false);
  pg_arm_edit_translate(a, (const float[3]){0.1f, 0, 0});
  pg_arm_bone_info(a, r, info);
  NEAR(info[0], -0.3, 1e-6);
  NEAR(info[3], -0.6, 1e-6);
  /* extrude from the tip of Arm.L: Arm.L.001 and its mirror Arm.R.001 (parented to Arm.R) */
  pg_arm_edit_select(a, l, BONE_TIPSEL, false);
  CHECK(pg_arm_extrude(a, (const float[3]){0.2f, 0.1f, 0}) == 2);
  const int l1 = pg_arm_bone_find(a, "Arm.L.001"), r1 = pg_arm_bone_find(a, "Arm.R.001");
  CHECK(l1 >= 0 && r1 >= 0);
  pg_arm_bone_info(a, l1, linfo);
  pg_arm_bone_info(a, r1, info);
  NEAR(linfo[3], 0.8, 1e-6);
  NEAR(linfo[4], 0.4, 1e-6);
  NEAR(info[0], -0.6, 1e-6);
  NEAR(info[3], -0.8, 1e-6);
  NEAR(info[4], 0.4, 1e-6);
  CHECK((int)info[7] == r);
  /* saved with the document */
  char *json = pg_arm_to_json(a);
  PGArmature *b = pg_arm_from_json(json);
  CHECK(pg_arm_mirror_x(b));
  std::free(json);
  pg_arm_free(b);
  pg_arm_free(a);
}

/* Pose rotate / scale pivot (transform_generics.c calculateCenterMedian, transform_mode.c
 * ElementRotation_ex / ElementResize): two unconnected bones with heads at (0,0) and (2,0) turn 90
 * deg about their median (1,0): the heads go to (1,-1) and (1,1). Individual Origins keeps them. */
static void test_pose_pivot()
{
  PGArmature *a = pg_arm_new();
  const float h1[3] = {0, 0, 0}, t1[3] = {0, 1, 0}, h2[3] = {2, 0, 0}, t2[3] = {2, 1, 0};
  const int b1 = pg_arm_bone_add(a, "A", h1, t1);
  const int b2 = pg_arm_bone_add(a, "B", h2, t2);
  pg_arm_set_mode(a, PG_ARM_MODE_POSE);
  pg_arm_pose_select(a, b1, false);
  pg_arm_pose_select(a, b2, true);
  float info[PG_ARM_BONE_INFO];
  CHECK(pg_arm_pose_rotate(a, float(M_PI_2), PG_ARM_AROUND_MEDIAN) == 2);
  pg_arm_bone_info(a, b1, info);
  NEAR(info[26], 1.0, 1e-5);
  NEAR(info[27], -1.0, 1e-5);
  NEAR(info[29], 0.0, 1e-5); /* tail: head + (-1, 0) */
  NEAR(info[30], -1.0, 1e-5);
  pg_arm_bone_info(a, b2, info);
  NEAR(info[26], 1.0, 1e-5);
  NEAR(info[27], 1.0, 1e-5);
  /* scale 2 about the median (1,0): heads (1,-1) -> (1,-2), (1,1) -> (1,2); size 2 */
  CHECK(pg_arm_pose_scale(a, 2.0f, PG_ARM_AROUND_MEDIAN) == 2);
  pg_arm_bone_info(a, b1, info);
  NEAR(info[26], 1.0, 1e-5);
  NEAR(info[27], -2.0, 1e-5);
  float v[10];
  pg_arm_pose_get(a, b1, v);
  NEAR(v[7], 2.0, 1e-6);
  /* Individual Origins: each about its own head */
  pg_arm_pose_rotate(a, float(-M_PI_2), PG_ARM_AROUND_INDIVIDUAL);
  pg_arm_bone_info(a, b1, info);
  NEAR(info[26], 1.0, 1e-5);
  NEAR(info[27], -2.0, 1e-5);
  NEAR(info[29], 1.0, 1e-5); /* back along +y, length 1 * scale 2 */
  NEAR(info[30], 0.0, 1e-5);
  pg_arm_pose_scale(a, 0.5f, PG_ARM_AROUND_INDIVIDUAL);
  pg_arm_bone_info(a, b2, info);
  NEAR(info[26], 1.0, 1e-5);
  NEAR(info[27], 2.0, 1e-5);
  pg_arm_free(a);
}

static void test_edit_ops_and_io()
{
  PGArmature *a = pg_arm_new();
  const float h[3] = {0, 0, 0}, t[3] = {0, 1, 0}, h2[3] = {1, 0, 0}, t2[3] = {1, 1, 0};
  const int r = pg_arm_bone_add(a, "", h, t);
  CHECK(strcmp(pg_arm_bone_name(a, r), "Bone") == 0);
  const int b = pg_arm_bone_add(a, "Bone", h2, t2);
  CHECK(strcmp(pg_arm_bone_name(a, b), "Bone.001") == 0);
  /* parent with keep offset: selected b, active r */
  pg_arm_edit_select(a, b, BONE_SELECTED, false);
  pg_arm_edit_select(a, r, BONE_SELECTED, true);
  CHECK(pg_arm_parent_set(a, PG_ARM_PAR_OFFSET) == 1);
  float info[PG_ARM_BONE_INFO];
  pg_arm_bone_info(a, b, info);
  CHECK((int)info[7] == r && !((int)info[8] & BONE_CONNECTED));
  NEAR(info[0], 1.0, 0); /* not moved */
  /* connected: b moves so its head is on r's tail, keeping its vector */
  pg_arm_parent_set(a, PG_ARM_PAR_CONNECT);
  pg_arm_bone_info(a, b, info);
  CHECK(((int)info[8] & BONE_CONNECTED) != 0);
  NEAR(info[0], 0.0, 1e-7);
  NEAR(info[1], 1.0, 1e-7);
  NEAR(info[4], 2.0, 1e-7);
  /* edit translate of r's tip moves b's head too */
  pg_arm_edit_select(a, r, BONE_TIPSEL, false);
  const float dx[3] = {0.5f, 0, 0};
  CHECK(pg_arm_edit_translate(a, dx) == 1);
  pg_arm_bone_info(a, b, info);
  NEAR(info[0], 0.5, 1e-6);
  /* clear parent (disconnect keeps the parent) */
  pg_arm_edit_select(a, b, BONE_SELECTED, false);
  pg_arm_parent_clear(a, PG_ARM_PAR_CLEAR_DISCONNECT);
  pg_arm_bone_info(a, b, info);
  CHECK((int)info[7] == r && !((int)info[8] & BONE_CONNECTED));
  pg_arm_parent_clear(a, PG_ARM_PAR_CLEAR);
  pg_arm_bone_info(a, b, info);
  CHECK((int)info[7] == -1);
  /* rename to a used name -> unique; F-Curve paths follow */
  pg_arm_pose_select(a, b, false);
  pg_arm_key_insert(a, 1.0f);
  char oldn[64], newn[64];
  CHECK(pg_arm_bone_rename(a, b, "Bone", oldn, newn) == 1); /* "Bone" is taken: unique name is its own */
  CHECK(strcmp(oldn, "Bone.001") == 0 && strcmp(newn, "Bone.001") == 0);
  CHECK(pg_arm_bone_rename(a, b, "Spine", oldn, newn) == 1);
  CHECK(pg_arm_fcurve(a, b, 0) != nullptr);
  CHECK(strcmp(pg_arm_fcurve(a, b, 0)->rna_path, "pose.bones[\"Spine\"].location") == 0);
  CHECK(pg_arm_bone_rename(a, b, "Arm.L", oldn, newn) == 1);
  pg_arm_bone_rename(a, r, "Arm.R", oldn, newn);
  /* paste flipped: pose of Arm.L goes to Arm.R with loc.x and the quaternion's y/z euler negated */
  float v[10];
  float e[3] = {0.1f, 0.2f, 0.3f}, quat[4];
  eul_to_quat(quat, e);
  const float lv[10] = {0.5f, 0.25f, 0.1f, quat[0], quat[1], quat[2], quat[3], 1, 2, 3};
  pg_arm_pose_set(a, b, lv);
  pg_arm_pose_select(a, b, false);
  CHECK(pg_arm_pose_copy(a) == 1);
  CHECK(pg_arm_pose_paste(a, true, false) == 1);
  pg_arm_pose_get(a, r, v);
  NEAR(v[0], -0.5, 1e-7);
  NEAR(v[1], 0.25, 1e-7);
  float fe[3] = {0.1f, -0.2f, -0.3f}, fq[4];
  eul_to_quat(fq, fe);
  for (int i = 0; i < 4; i++) NEAR(v[3 + i], fq[i], 1e-6);
  NEAR(v[8], 2.0, 0);
  /* unflipped paste goes to the same name */
  pg_arm_pose_paste(a, false, false);
  pg_arm_pose_get(a, b, v);
  NEAR(v[0], 0.5, 1e-7);

  /* JSON round trip and dup (undo snapshot) keep everything */
  char *json = pg_arm_to_json(a);
  CHECK(json != nullptr);
  PGArmature *c = pg_arm_from_json(json);
  PGArmature *d = pg_arm_dup(a);
  CHECK(c != nullptr);
  for (PGArmature *x : {c, d}) {
    CHECK(pg_arm_bone_count(x) == pg_arm_bone_count(a));
    CHECK(strcmp(pg_arm_bone_name(x, b), "Arm.L") == 0);
    CHECK(pg_arm_fcurve_count(x) == pg_arm_fcurve_count(a));
    float i0[PG_ARM_BONE_INFO], i1[PG_ARM_BONE_INFO];
    for (int k = 0; k < pg_arm_bone_count(a); k++) {
      pg_arm_bone_info(a, k, i0);
      pg_arm_bone_info(x, k, i1);
      for (int j = 0; j < PG_ARM_BONE_INFO; j++) NEAR(i1[j], i0[j], 1e-6);
    }
    pg_arm_pose_get(x, r, v);
    NEAR(v[0], -0.5, 1e-7);
  }
  char *json2 = pg_arm_to_json(c);
  CHECK(strcmp(json, json2) == 0);
  free(json);
  free(json2);
  CHECK(pg_arm_from_json("{\"bones\":[{\"parent\":0}]}") != nullptr); /* self parent dropped */
  CHECK(pg_arm_from_json("not json") == nullptr);
  /* delete: children go to the parent */
  pg_arm_edit_select(a, r, BONE_SELECTED, false);
  CHECK(pg_arm_delete_selected(a) == 1);
  CHECK(pg_arm_bone_count(a) == 1);
  pg_arm_free(a);
  pg_arm_free(c);
  pg_arm_free(d);
}

/* L10 9: display settings saved with the rig; click select (pg_arm_pick, overlay picking). */
static void test_display_and_pick()
{
  PGArmature *a = pg_arm_new();
  CHECK(pg_arm_display_type(a) == ARM_OCTA && !pg_arm_show_in_front(a)); /* DNA defaults */
  const float ys[3] = {100, 200, 300};
  for (float y : ys) {
    float hp[3] = {100, y, 0}, tp[3] = {300, y, 0}, h3[3], t3[3];
    pg_arm_canvas_to_arm(hp, h3);
    pg_arm_canvas_to_arm(tp, t3);
    pg_arm_bone_add(a, "Bone", h3, t3);
  }
  pg_arm_set_display(a, ARM_LINE, true);
  char *json = pg_arm_to_json(a);
  PGArmature *b = pg_arm_from_json(json);
  CHECK(b && pg_arm_display_type(b) == ARM_LINE && pg_arm_show_in_front(b));
  PGArmature *c = pg_arm_dup(a);
  CHECK(pg_arm_display_type(c) == ARM_LINE && pg_arm_show_in_front(c));
  free(json);
  pg_arm_free(b);
  pg_arm_free(c);
  {
    PGArmature *old = pg_arm_from_json("{\"version\":1,\"bones\":[]}");
    CHECK(pg_arm_display_type(old) == ARM_OCTA && !pg_arm_show_in_front(old));
    pg_arm_free(old);
  }
  /* armature space -> canvas px (as PG_ARM_CMD_PICK without a view matrix) */
  float m[4][4];
  unit_m4(m);
  m[0][0] = PG_ARM_PIXELS_PER_UNIT;
  m[1][1] = -PG_ARM_PIXELS_PER_UNIT;
  m[2][2] = PG_ARM_PIXELS_PER_UNIT;
  const float dist = 75.0f;
  pg_arm_set_mode(a, PG_ARM_MODE_OBJECT);
  const float mid1[2] = {200, 210};
  CHECK(pg_arm_pick(a, m, mid1, dist, false) == -1); /* object mode: no bone select */
  pg_arm_set_mode(a, PG_ARM_MODE_EDIT);
  CHECK(pg_arm_pick(a, m, mid1, dist, false) == 1);
  CHECK(pg_arm_active_bone(a) == 1);
  float info[PG_ARM_BONE_INFO];
  pg_arm_bone_info(a, 1, info);
  CHECK(((int)info[8] & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) == (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL));
  pg_arm_bone_info(a, 0, info);
  CHECK(((int)info[8] & BONE_SELECTED) == 0);
  /* near the tail of bone 2: the tip only (BONESEL_TIP) */
  const float tip2[2] = {303, 302};
  CHECK(pg_arm_pick(a, m, tip2, dist, false) == 2);
  pg_arm_bone_info(a, 2, info);
  CHECK(((int)info[8] & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) == BONE_TIPSEL);
  pg_arm_bone_info(a, 1, info);
  CHECK(((int)info[8] & BONE_SELECTED) == 0);
  /* extend keeps the others */
  const float mid0[2] = {150, 95};
  CHECK(pg_arm_pick(a, m, mid0, dist, true) == 0);
  CHECK(pg_arm_bone_selected(a, 0));
  pg_arm_bone_info(a, 2, info);
  CHECK(((int)info[8] & BONE_TIPSEL) != 0);
  /* beyond ED_view3d_select_dist_px of every bone: deselect all */
  const float far[2] = {200, 500};
  CHECK(pg_arm_pick(a, m, far, dist, false) == -1);
  for (int i = 0; i < 3; i++) {
    pg_arm_bone_info(a, i, info);
    CHECK(((int)info[8] & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) == 0);
  }
  /* nearest wins: between bone 0 (y 100) and bone 1 (y 200), closer to bone 1 */
  pg_arm_set_mode(a, PG_ARM_MODE_POSE);
  const float between[2] = {200, 160};
  CHECK(pg_arm_pick(a, m, between, dist, false) == 1);
  CHECK(pg_arm_bone_selected(a, 1) && !pg_arm_bone_selected(a, 0));
  /* pose mode follows the posed bone: rotate bone 0 by 90 degrees about its head */
  pg_arm_pose_select(a, 0, false);
  pg_arm_pose_rotate(a, float(M_PI_2), PG_ARM_AROUND_MEDIAN);
  const float posed[2] = {100, 0}; /* the tail swung up the canvas (-y) */
  CHECK(pg_arm_pick(a, m, posed, 10.0f, false) == 0);
  pg_arm_free(a);
}

int main()
{
  test_bone_matrices();
  test_pose_evaluation();
  test_fcurves();
  test_deform_and_bind();
  test_bbone_deform();
  test_multiframe_bind();
  test_mirror_edit();
  test_pose_pivot();
  test_edit_ops_and_io();
  test_display_and_pick();
  printf("armature: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
