/* Host test of the B-Bone bridge (RigPanel's Bendy Bones section): PG_ARM_CMD_BBONE /
 * PG_ARM_CMD_POSE_BBONE set the edit bone and pose channel values (rna_armature.c / rna_pose.c
 * bbone_*), PG_ARM_Q_BBONE / PG_ARM_Q_POSE_BBONE read them back, the rig JSON keeps them, and
 * the bent bone's display segments (draw_bone_update_disp_matrix_bbone) leave the straight line. */
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "project_grease_armature.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"

extern "C" void DRW_gpencil_batch_cache_dirty_tag(bGPdata *) {}
extern "C" void DRW_gpencil_batch_cache_free(bGPdata *) {}

using project_grease::gp::Backend;

int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name);
int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap);

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main()
{
  Backend b;
  CHECK(b.initialize());
  CHECK(b.create_document() && b.create_layer("Lines") && b.create_frame(1));
  const float none[1] = {0};
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_CREATE, -1, none, 0, nullptr) == 1);
  const float edit[1] = {1};
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_SET_MODE, -1, edit, 1, nullptr) == 1);
  const float add[4] = {100, 500, 300, 500};
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_BONE_ADD, -1, add, 4, nullptr) == 0);

  float q[32] = {};
  /* defaults: 1 segment, ease 1 / 1, scale 1, curves / rolls 0 */
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_BBONE, 0, 0, q, 32) == 3 + PG_ARM_BBONE);
  CHECK(q[0] == 1 && q[3] == 1 && q[4] == 1 && q[7] == 0 && q[11] == 1 && q[16] == 1);
  printf("default display size %g %g\n", q[1], q[2]);

  /* segments, display x / z, ease in / out, roll in / out, curve in x / z, out x / z, scale in / out */
  const float set[3 + PG_ARM_BBONE] = {6, 0.02f, 0.03f, 0.5f, 2.0f, 0.1f, -0.2f, 0.05f, 0.01f, -0.04f, 0.02f,
                                       1.5f, 1, 1, 0.5f, 1, 1};
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_BBONE, 0, set, 3 + PG_ARM_BBONE, nullptr) == 1);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_BBONE, 0, 0, q, 32) == 3 + PG_ARM_BBONE);
  for (int i = 0; i < 3 + PG_ARM_BBONE; i++) CHECK(std::fabs(q[i] - set[i]) < 1e-6f);
  /* bbone_segments range 1..32 */
  float clamp[3 + PG_ARM_BBONE];
  for (int i = 0; i < 3 + PG_ARM_BBONE; i++) clamp[i] = set[i];
  clamp[0] = 99;
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_BBONE, 0, clamp, 3 + PG_ARM_BBONE, nullptr) == 1);
  pg_arm_bridge_query(b, PG_ARM_Q_BBONE, 0, 0, q, 32);
  CHECK(q[0] == 32);
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_BBONE, 0, set, 3 + PG_ARM_BBONE, nullptr) == 1);
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_BBONE, 0, set, 5, nullptr) == -1); /* too few values */

  /* pose channel offsets */
  const float pose[1] = {2};
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_SET_MODE, -1, pose, 1, nullptr) == 1);
  const float pv[PG_ARM_BBONE] = {0.25f, -0.5f, 0.3f, 0.4f, 0.01f, 0.02f, 0.03f, 0.04f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.7f};
  CHECK(pg_arm_bridge_command(b, PG_ARM_CMD_POSE_BBONE, 0, pv, PG_ARM_BBONE, nullptr) == 1);
  CHECK(pg_arm_bridge_query(b, PG_ARM_Q_POSE_BBONE, 0, 0, q, 32) == PG_ARM_BBONE);
  for (int i = 0; i < PG_ARM_BBONE; i++) CHECK(std::fabs(q[i] - pv[i]) < 1e-6f);

  /* the display segments follow the curve */
  const PGArmature *a = b.armature(false);
  float m[32][4][4];
  CHECK(pg_arm_bone_bbone_matrices(a, 0, m, 32) == 6);
  float off = 0;
  for (int i = 0; i < 6; i++) off = std::fmax(off, std::fabs(m[i][3][1] - m[0][3][1]));
  printf("segment head y offset %g (armature units)\n", off);
  CHECK(off > 0.005f);

  /* the rig JSON keeps them */
  char *json = pg_arm_to_json(a);
  CHECK(json != nullptr);
  PGArmature *c = pg_arm_from_json(json);
  CHECK(c != nullptr);
  if (c) {
    float r[PG_ARM_BBONE], sz[2];
    CHECK(pg_arm_bone_bbone(c, 0, r) == 6);
    for (int i = 0; i < PG_ARM_BBONE; i++) CHECK(std::fabs(r[i] - set[3 + i]) < 1e-5f);
    CHECK(pg_arm_bone_bbone_size(c, 0, sz) && std::fabs(sz[0] - 0.02f) < 1e-6f && std::fabs(sz[1] - 0.03f) < 1e-6f);
    CHECK(pg_arm_pose_bbone(c, 0, r));
    for (int i = 0; i < PG_ARM_BBONE; i++) CHECK(std::fabs(r[i] - pv[i]) < 1e-5f);
    pg_arm_free(c);
  }
  free(json);
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
