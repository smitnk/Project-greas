/* SPDX-License-Identifier: GPL-2.0-or-later
 * Drivers and custom properties of the rig (project_grease_driver.c, Blender 3.6.23
 * fcurve_driver.c / fcurve.c evaluate_fcurve_driver / fmodifier.c Generator / anim_sys.c
 * BKE_animsys_eval_driver, and BLI_expr_pylike_eval.c for Scripted expressions).
 *
 * Drivers are FCurves with a ChannelDriver (the rig's AnimData.drivers). Paths (RNA path subset):
 *   pose.bones["B"].location | rotation_quaternion | rotation_euler | scale   (array, index)
 *   pose.bones["B"]["prop"]   a bone custom property          ["prop"]  an object custom property
 *   layers["L"].opacity | hide, layers["L"].modifiers[N].offset | frame_scale   (external: the GP
 *   data, layer L's modifier N, resolved by the caller through PGArmExternal)
 * Single Property variables read the same paths with the array index in brackets
 * (pose.bones["B"].location[1]). Names are quoted with BLI_str_escape rules (\" and \\).
 * Drivers are indices in creation order, variables indices in the driver, properties indices in
 * creation order. */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PGArmature PGArmature;

/* Custom float property (an IDP_FLOAT ID property with its UI data: min / max / default). */
typedef struct PGProp {
  struct PGProp *next, *prev;
  char bone[64]; /* "" = the rig object */
  char name[64];
  float value, min, max, def;
} PGProp;

/* Properties outside the rig (the GP data and its layers' modifiers): get / set return false when
 * the path does not resolve. set coerces like BKE_animsys_write_to_rna_path (int truncation,
 * boolean > 1 - FLT_EPSILON, RNA range clamp). */
typedef struct PGArmExternal {
  void *user;
  bool (*get)(void *user, const char *path, int index, float *r_value);
  bool (*set)(void *user, const char *path, int index, float value);
} PGArmExternal;

/* The action, then the drivers in dependency order (see pg_drv_evaluate), then the pose. */
void pg_arm_evaluate_ex(PGArmature *a, float frame, const PGArmExternal *ext);

/* ---- custom properties (WM_OT_properties_add / _edit / _remove) ---- */
/* bone -1 = the rig object. An existing name is edited. min > max is refused. Returns the index. */
int pg_arm_prop_add(PGArmature *a, int bone, const char *name, float value, float min, float max,
                    float def);
int pg_arm_prop_remove(PGArmature *a, int index);
int pg_arm_prop_count(const PGArmature *a);
int pg_arm_prop_find(const PGArmature *a, int bone, const char *name);
/* The value clamped to min..max (RNA_property_float_clamp). */
int pg_arm_prop_set(PGArmature *a, int index, float value);
/* bone (-1 object), value, min, max, default */
int pg_arm_prop_get(const PGArmature *a, int index, float r[5]);
const char *pg_arm_prop_name(const PGArmature *a, int index);

/* ---- drivers (ANIM_add_driver / ANIM_remove_driver, the Drivers editor) ---- */
/* verify_driver_fcurve: an existing driver for path/index is returned. type DRIVER_TYPE_AVERAGE
 * 0 / PYTHON 1 (Scripted) / SUM 2 / MIN 3 / MAX 4. generator false: keys (0,0) (1,1), linear
 * extrapolation (DRIVER_FCURVE_KEYFRAMES); true: a Generator modifier (DRIVER_FCURVE_GENERATOR). */
int pg_arm_driver_add(PGArmature *a, const char *path, int index, int type, bool generator);
int pg_arm_driver_remove(PGArmature *a, int driver);
int pg_arm_driver_count(const PGArmature *a);
int pg_arm_driver_find(const PGArmature *a, const char *path, int index);
int pg_arm_driver_set_type(PGArmature *a, int driver, int type);
int pg_arm_driver_set_expression(PGArmature *a, int driver, const char *expression);
/* driver_add_new_variable + driver_change_variable_type (DVAR_TYPE_SINGLE_PROP 0 /
 * TRANSFORM_CHAN 3), named `name` (validated, made unique) or "var". Returns its index. */
int pg_arm_driver_var_add(PGArmature *a, int driver, const char *name, int type);
int pg_arm_driver_var_remove(PGArmature *a, int driver, int var);
int pg_arm_driver_var_rename(PGArmature *a, int driver, int var, const char *name);
/* Single Property: the RNA path (see above). */
int pg_arm_driver_var_set_path(PGArmature *a, int driver, int var, const char *path);
/* Transform Channel: bone ("" = the object), DTAR_TRANSCHAN_* (LOCX 0 .. ROTW 9, SCALE_AVG 10),
 * DTAR_ROTMODE_* and flags DTAR_FLAG_LOCALSPACE (Transform space) | DTAR_FLAG_LOCAL_CONSTS (with
 * LOCALSPACE: Local space). */
int pg_arm_driver_var_set_transform(PGArmature *a, int driver, int var, const char *bone,
                                    int trans_chan, int rotation_mode, int space_flags);
/* Replaces the driver F-Curve's keys: n (x, y, ipo) triples, increasing x; extend
 * FCURVE_EXTRAPOLATE_CONSTANT 0 / LINEAR 1. */
int pg_arm_driver_set_keys(PGArmature *a, int driver, const float *xyi, int n, int extend);
/* The driver F-Curve's Generator modifier: mode FCM_GENERATOR_POLYNOMIAL 0 (coefficients
 * c0 + c1 x + ..., order n - 1) / _FACTORISED 1 ((c0 x + c1)(c2 x + c3)..., order n / 2), additive.
 * n 0 removes it. */
int pg_arm_driver_set_generator(PGArmature *a, int driver, int mode, bool additive,
                                const float *coefficients, int n);
/* array index, type, driver flag (DRIVER_FLAG_INVALID 1), driver value (curval), F-Curve value,
 * variable count, key count, generator (1 / 0), then per variable: type, value, target flag */
int pg_arm_driver_get(const PGArmature *a, int driver, float *r, int cap);
const char *pg_arm_driver_path(const PGArmature *a, int driver);
const char *pg_arm_driver_expression(const PGArmature *a, int driver);

/* ---- rig internals (project_grease_armature.c) ---- */
void pg_drv_free_all(PGArmature *a);
void pg_drv_copy_all(PGArmature *dst, const PGArmature *src);
void pg_drv_rename_bone(PGArmature *a, const char *oldname, const char *newname);
/* Pose, then each driver (BKE_animsys_eval_driver) in a topological order of "writes a property
 * another driver reads" (cycles keep list order); a Transform Channel variable sees the pose
 * re-evaluated after the bone channels written so far; then the final pose. */
void pg_drv_evaluate(PGArmature *a, float frame, const PGArmExternal *ext);
/* Driver list helpers for the document IO. */
struct FCurve *pg_drv_new_fcurve(const char *path, int index);
void pg_drv_fcurve_free(struct FCurve *fcu);

#ifdef __cplusplus
}
#endif
