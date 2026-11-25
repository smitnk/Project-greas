/* SPDX-License-Identifier: GPL-2.0-or-later
 * Rig commands and queries of the bridge (PG_ARM_CMD_* / PG_ARM_Q_* in project_grease_gp_bridge.h):
 * canvas pixels in, canvas pixels out, pg_arm_* (project_grease_armature.h) in between. */
#include <climits>
#include <cstring>
#include <string>
#include <vector>

#include "project_grease_armature.h"
#include "project_grease_armature_bke.h"
#include "BLI_math_matrix.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"

/* project_grease_rig_bridge.cpp */
int pg_arm_rig_command(project_grease::gp::Backend &backend, PGArmature *a, int cmd, int bone,
                       const float *args, int n, const char *name);
int pg_arm_rig_query(const PGArmature *a, int what, int bone, float x, float *out, int cap);

namespace {

using project_grease::gp::Backend;

void px_to_arm(const float *px2, float r[3])
{
  const float p[3] = {px2[0], px2[1], 0.0f};
  pg_arm_canvas_to_arm(p, r);
}

/* a canvas vector (delta), not a point: the mapping has no translation, so the same */
void px_delta_to_arm(const float *px2, float r[3]) { px_to_arm(px2, r); }

float arg(const float *args, int n, int i, float def = 0.0f) { return args && i < n ? args[i] : def; }

}  // namespace

int pg_arm_bridge_command(Backend &backend, int cmd, int bone, const float *args, int n, const char *name)
{
  if (cmd == PG_ARM_CMD_CREATE) return backend.armature(true) ? 1 : 0;
  if (cmd == PG_ARM_CMD_REMOVE) { backend.armature_remove(); return 1; }
  if (cmd == PG_ARM_CMD_BIND) return backend.armature_bind((int)arg(args, n, 0));
  PGArmature *a = backend.armature(false);
  if (!a) return -1;
  if (const int rig = pg_arm_rig_command(backend, a, cmd, bone, args, n, name); rig != INT_MIN) {
    backend.armature_changed();
    return rig;
  }
  int r = 0;
  float v0[3], v1[3];
  switch (cmd) {
    case PG_ARM_CMD_SET_MODE:
      pg_arm_set_mode(a, (int)arg(args, n, 0));
      r = 1;
      break;
    case PG_ARM_CMD_BONE_ADD: {
      if (n < 4) return -1;
      px_to_arm(args, v0);
      px_to_arm(args + 2, v1);
      r = pg_arm_bone_add(a, name, v0, v1);
      break;
    }
    case PG_ARM_CMD_EXTRUDE:
      if (n < 2) return -1;
      px_delta_to_arm(args, v0);
      r = pg_arm_extrude(a, v0);
      break;
    case PG_ARM_CMD_EDIT_SELECT:
      r = pg_arm_edit_select(a, bone, (int)arg(args, n, 0, 1), arg(args, n, 1) != 0.0f);
      break;
    case PG_ARM_CMD_EDIT_TRANSLATE:
      if (n < 2) return -1;
      px_delta_to_arm(args, v0);
      r = pg_arm_edit_translate(a, v0);
      break;
    case PG_ARM_CMD_EDIT_ROTATE:
      if (n < 3) return -1;
      px_to_arm(args + 1, v0);
      r = pg_arm_edit_rotate(a, args[0], v0);
      break;
    case PG_ARM_CMD_EDIT_SCALE:
      if (n < 3) return -1;
      px_to_arm(args + 1, v0);
      r = pg_arm_edit_scale(a, args[0], v0);
      break;
    case PG_ARM_CMD_BONE_SET: {
      if (n < 5) return -1;
      px_to_arm(args, v0);
      px_to_arm(args + 2, v1);
      r = pg_arm_bone_set(a, bone, v0, v1, args[4]);
      break;
    }
    case PG_ARM_CMD_PARENT_SET:
      r = pg_arm_parent_set(a, (int)arg(args, n, 0, PG_ARM_PAR_OFFSET));
      break;
    case PG_ARM_CMD_PARENT_CLEAR:
      r = pg_arm_parent_clear(a, (int)arg(args, n, 0, PG_ARM_PAR_CLEAR));
      break;
    case PG_ARM_CMD_DELETE:
      r = pg_arm_delete_selected(a);
      break;
    case PG_ARM_CMD_RENAME: {
      char oldn[64], newn[64];
      r = pg_arm_bone_rename(a, bone, name, oldn, newn);
      if (r) backend.armature_bone_renamed(oldn, newn);
      break;
    }
    case PG_ARM_CMD_ENVELOPE:
      if (n < 6) return -1;
      r = pg_arm_bone_set_envelope(a, bone, args[0] / PG_ARM_PIXELS_PER_UNIT, args[1],
                                   args[2] / PG_ARM_PIXELS_PER_UNIT, args[3] / PG_ARM_PIXELS_PER_UNIT,
                                   args[4] != 0.0f, args[5] != 0.0f);
      break;
    case PG_ARM_CMD_POSE_SELECT:
      r = pg_arm_pose_select(a, bone, arg(args, n, 0) != 0.0f);
      break;
    case PG_ARM_CMD_POSE_TRANSLATE:
      if (n < 2) return -1;
      px_delta_to_arm(args, v0);
      r = pg_arm_pose_translate(a, v0);
      break;
    case PG_ARM_CMD_POSE_ROTATE:
      r = pg_arm_pose_rotate(a, arg(args, n, 0), (int)arg(args, n, 1, PG_ARM_AROUND_MEDIAN));
      break;
    case PG_ARM_CMD_POSE_SCALE:
      r = pg_arm_pose_scale(a, arg(args, n, 0, 1.0f), (int)arg(args, n, 1, PG_ARM_AROUND_MEDIAN));
      break;
    case PG_ARM_CMD_POSE_SET:
      if (n < 10) return -1;
      r = pg_arm_pose_set(a, bone, args);
      break;
    case PG_ARM_CMD_POSE_CLEAR:
      r = pg_arm_pose_clear(a, (int)arg(args, n, 0, 7));
      break;
    case PG_ARM_CMD_POSE_COPY:
      r = pg_arm_pose_copy(a);
      break;
    case PG_ARM_CMD_POSE_PASTE:
      r = pg_arm_pose_paste(a, arg(args, n, 0) != 0.0f, arg(args, n, 1) != 0.0f);
      break;
    case PG_ARM_CMD_KEY_INSERT:
      r = pg_arm_key_insert(a, arg(args, n, 0, 1.0f));
      break;
    case PG_ARM_CMD_KEY_DELETE:
      r = pg_arm_key_delete(a, bone, arg(args, n, 0, 1.0f));
      break;
    case PG_ARM_CMD_KEY_INTERPOLATION:
      r = pg_arm_key_set_interpolation(a, bone, arg(args, n, 0, 1.0f), (int)arg(args, n, 1, 2));
      break;
    case PG_ARM_CMD_KEY_MOVE:
      r = pg_arm_key_move(a, bone, arg(args, n, 0, 1.0f), arg(args, n, 1, 1.0f));
      break;
    case PG_ARM_CMD_MIRROR_X:
      pg_arm_set_mirror_x(a, arg(args, n, 0) != 0.0f);
      r = 1;
      break;
    case PG_ARM_CMD_DISPLAY:
      pg_arm_set_display(a, (int)arg(args, n, 0), arg(args, n, 1) != 0.0f);
      r = 1;
      break;
    case PG_ARM_CMD_PICK: {
      if (n < 3) return -1;
      float c2w[4][4], a2c[4][4], a2w[4][4];
      unit_m4(c2w);
      if (n >= 20) std::memcpy(c2w, args + 4, sizeof(c2w));
      unit_m4(a2c); /* pg_arm_arm_to_canvas */
      a2c[0][0] = PG_ARM_PIXELS_PER_UNIT;
      a2c[1][1] = -PG_ARM_PIXELS_PER_UNIT;
      a2c[2][2] = PG_ARM_PIXELS_PER_UNIT;
      mul_m4_m4m4(a2w, c2w, a2c);
      r = pg_arm_pick(a, a2w, args, args[2], arg(args, n, 3) != 0.0f);
      break;
    }
    case PG_ARM_CMD_EVALUATE:
      backend.armature_evaluate(arg(args, n, 0, 1.0f));
      r = 1;
      break;
    default:
      return -1;
  }
  /* a pose / bone edit tags the depsgraph: the drivers reading bones run again (not the action,
   * which would undo the edit until the next key) */
  if (cmd != PG_ARM_CMD_EVALUATE) backend.armature_evaluate(-1.0f, false);
  backend.armature_changed();
  return r;
}

int pg_arm_bridge_query(Backend &backend, int what, int bone, float x, float *out, int cap)
{
  const PGArmature *a = backend.armature(false);
  float tmp[PG_ARM_BONE_INFO];
  if (!a) {
    if (what != PG_ARM_Q_SUMMARY || !out || cap < 4) return 0;
    const float none[4] = {-1.0f, -1.0f, 0.0f, 0.0f}; /* no rig */
    std::memcpy(out, none, sizeof(none));
    return 4;
  }
  if (const int rig = pg_arm_rig_query(a, what, bone, x, out, cap); rig >= 0) return rig;
  int n = 0;
  switch (what) {
    case PG_ARM_Q_SUMMARY: /* bone count, active bone, mode, fcurve count */
      tmp[0] = (float)pg_arm_bone_count(a);
      tmp[1] = (float)pg_arm_active_bone(a);
      tmp[2] = (float)pg_arm_mode(a);
      tmp[3] = (float)pg_arm_fcurve_count(a);
      n = 4;
      break;
    case PG_ARM_Q_BONE: {
      if (!pg_arm_bone_info(a, bone, tmp)) return 0;
      /* positions to canvas px: head, tail, pose head, pose tail; length px */
      for (int off : {0, 3, 26, 29}) {
        float px[3];
        pg_arm_arm_to_canvas(tmp + off, px);
        std::memcpy(tmp + off, px, sizeof(px));
      }
      tmp[9] *= PG_ARM_PIXELS_PER_UNIT;
      n = PG_ARM_BONE_INFO;
      break;
    }
    case PG_ARM_Q_POSE:
      if (!pg_arm_pose_get(a, bone, tmp)) return 0;
      n = 10;
      break;
    case PG_ARM_Q_KEYS: { /* key frames of the bone (the dope sheet row) */
      const int total = pg_arm_bone_keys(a, bone, nullptr, 0);
      std::vector<float> frames(static_cast<size_t>(total));
      pg_arm_bone_keys(a, bone, frames.data(), total);
      const int m = total < cap ? total : cap;
      if (out && m > 0) std::memcpy(out, frames.data(), sizeof(float) * static_cast<size_t>(m));
      return total;
    }
    case PG_ARM_Q_KEY_INTERPOLATION:
      tmp[0] = (float)pg_arm_key_interpolation(a, bone, x);
      n = 1;
      break;
    case PG_ARM_Q_ENVELOPE: /* dist px, weight, rad_head px, rad_tail px */
      if (!pg_arm_bone_envelope(a, bone, tmp)) return 0;
      tmp[0] *= PG_ARM_PIXELS_PER_UNIT;
      tmp[2] *= PG_ARM_PIXELS_PER_UNIT;
      tmp[3] *= PG_ARM_PIXELS_PER_UNIT;
      n = 4;
      break;
    case PG_ARM_Q_DISPLAY:
      tmp[0] = (float)pg_arm_display_type(a);
      tmp[1] = pg_arm_show_in_front(a) ? 1.0f : 0.0f;
      tmp[2] = pg_arm_mirror_x(a) ? 1.0f : 0.0f;
      n = 3;
      break;
    case PG_ARM_Q_SELECTED:
      tmp[0] = pg_arm_bone_selected(a, bone) ? 1.0f : 0.0f;
      n = 1;
      break;
    default:
      return 0;
  }
  if (n > cap || !out) return n;
  std::memcpy(out, tmp, sizeof(float) * static_cast<size_t>(n));
  return n;
}
