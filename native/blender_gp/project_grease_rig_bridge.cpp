/* SPDX-License-Identifier: GPL-2.0-or-later
 * Constraint / custom property / driver commands and queries of the bridge (PG_ARM_CMD_CON_* ..
 * PG_ARM_Q_DRIVER in project_grease_gp_bridge.h), called by project_grease_armature_bridge.cpp. */
#include <climits>
#include <cstring>

#include "project_grease_armature.h"
#include "project_grease_constraint.h"
#include "project_grease_driver.h"
#include "project_grease_gp_backend.h"
#include "project_grease_gp_bridge.h"

using project_grease::gp::Backend;

static int iarg(const float *args, int n, int i, int def = 0) { return args && i < n ? (int)args[i] : def; }
static float farg(const float *args, int n, int i, float def = 0.0f) { return args && i < n ? args[i] : def; }

/* INT_MIN when cmd is not one of these. */
int pg_arm_rig_command(Backend &backend, PGArmature *a, int cmd, int bone, const float *args, int n,
                       const char *name)
{
  const char *str = name ? name : "";
  int r;
  switch (cmd) {
    case PG_ARM_CMD_CON_ADD: r = pg_arm_con_add(a, bone, iarg(args, n, 0), str); break;
    case PG_ARM_CMD_CON_REMOVE: r = pg_arm_con_remove(a, bone, iarg(args, n, 0, -1)); break;
    case PG_ARM_CMD_CON_MOVE: r = pg_arm_con_move(a, bone, iarg(args, n, 0, -1), iarg(args, n, 1, -1)); break;
    case PG_ARM_CMD_CON_TARGET:
      r = pg_arm_con_set_target(a, bone, iarg(args, n, 0, -1), iarg(args, n, 1), iarg(args, n, 2) ? nullptr : str);
      break;
    case PG_ARM_CMD_CON_SET:
      if (n < 2) return -1;
      r = pg_arm_con_set(a, bone, iarg(args, n, 0, -1), args + 1, n - 1);
      break;
    case PG_ARM_CMD_CON_CHILDOF_INVERSE:
      r = pg_arm_con_childof_inverse(a, bone, iarg(args, n, 0, -1), iarg(args, n, 1, 1) != 0);
      break;
    case PG_ARM_CMD_PROP_ADD:
      /* WM_OT_properties_add defaults: 1.0 in 0..1 */
      r = pg_arm_prop_add(a, bone, str, farg(args, n, 0, 1.0f), farg(args, n, 1, 0.0f), farg(args, n, 2, 1.0f),
                          farg(args, n, 3, 1.0f));
      break;
    case PG_ARM_CMD_PROP_REMOVE: r = pg_arm_prop_remove(a, iarg(args, n, 0, -1)); break;
    case PG_ARM_CMD_PROP_SET: r = pg_arm_prop_set(a, iarg(args, n, 0, -1), farg(args, n, 1)); break;
    case PG_ARM_CMD_DRIVER_ADD:
      r = pg_arm_driver_add(a, str, iarg(args, n, 0), iarg(args, n, 1, 1), iarg(args, n, 2) != 0);
      break;
    case PG_ARM_CMD_DRIVER_REMOVE: r = pg_arm_driver_remove(a, iarg(args, n, 0, -1)); break;
    case PG_ARM_CMD_DRIVER_TYPE: r = pg_arm_driver_set_type(a, iarg(args, n, 0, -1), iarg(args, n, 1)); break;
    case PG_ARM_CMD_DRIVER_EXPRESSION: r = pg_arm_driver_set_expression(a, iarg(args, n, 0, -1), str); break;
    case PG_ARM_CMD_DRIVER_VAR_ADD: r = pg_arm_driver_var_add(a, iarg(args, n, 0, -1), str, iarg(args, n, 1)); break;
    case PG_ARM_CMD_DRIVER_VAR_REMOVE: r = pg_arm_driver_var_remove(a, iarg(args, n, 0, -1), iarg(args, n, 1, -1)); break;
    case PG_ARM_CMD_DRIVER_VAR_RENAME: r = pg_arm_driver_var_rename(a, iarg(args, n, 0, -1), iarg(args, n, 1, -1), str); break;
    case PG_ARM_CMD_DRIVER_VAR_PATH: r = pg_arm_driver_var_set_path(a, iarg(args, n, 0, -1), iarg(args, n, 1, -1), str); break;
    case PG_ARM_CMD_DRIVER_VAR_TRANSFORM:
      r = pg_arm_driver_var_set_transform(a, iarg(args, n, 0, -1), iarg(args, n, 1, -1), str, iarg(args, n, 2),
                                          iarg(args, n, 3), iarg(args, n, 4));
      break;
    case PG_ARM_CMD_DRIVER_KEYS:
      if (n < 2 || (n - 2) % 3 != 0) return -1;
      r = pg_arm_driver_set_keys(a, iarg(args, n, 0, -1), args + 2, (n - 2) / 3, iarg(args, n, 1));
      break;
    case PG_ARM_CMD_DRIVER_GENERATOR:
      if (n < 3) return -1;
      r = pg_arm_driver_set_generator(a, iarg(args, n, 0, -1), iarg(args, n, 1), iarg(args, n, 2) != 0, args + 3, n - 3);
      break;
    default:
      return INT_MIN;
  }
  return r; /* the caller tags the depsgraph (Backend::armature_changed) */
}

/* -1 when what is not one of these. */
int pg_arm_rig_query(const PGArmature *a, int what, int bone, float x, float *out, int cap)
{
  float tmp[PG_CON_MAX_VALUES];
  int n = 0;
  switch (what) {
    case PG_ARM_Q_CONSTRAINTS: tmp[0] = (float)pg_arm_con_count(a, bone); n = 1; break;
    case PG_ARM_Q_CONSTRAINT: n = pg_arm_con_get(a, bone, (int)x, tmp, PG_CON_MAX_VALUES); break;
    case PG_ARM_Q_PROPS: tmp[0] = (float)pg_arm_prop_count(a); n = 1; break;
    case PG_ARM_Q_PROP: n = pg_arm_prop_get(a, (int)x, tmp) ? 5 : 0; break;
    case PG_ARM_Q_DRIVERS: tmp[0] = (float)pg_arm_driver_count(a); n = 1; break;
    case PG_ARM_Q_DRIVER: return pg_arm_driver_get(a, (int)x, out, cap); /* fills when it fits */
    default: return -1;
  }
  if (n > cap || !out) return n;
  std::memcpy(out, tmp, sizeof(float) * static_cast<size_t>(n));
  return n;
}
