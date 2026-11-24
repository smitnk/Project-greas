/* SPDX-License-Identifier: GPL-2.0-or-later
 * Drivers and custom properties of the rig (see project_grease_driver.h). Ported from Blender
 * 3.6.23, regions marked with their source:
 * - blenkernel/intern/fcurve_driver.c: dvar_eval_transChan, BKE_driver_target_matrix_to_rot_channels,
 *   driver_get_variable_value, evaluate_driver(_sum / _min_max / _python with the simple
 *   expression evaluator only), driver variable naming / freeing / copying (verbatim).
 *   dtar_get_prop_val reads through the path subset of project_grease_driver.h instead of RNA.
 * - blenkernel/intern/fcurve.c evaluate_fcurve_driver / evaluate_fcurve_ex / calculate_fcurve.
 * - blenkernel/intern/fmodifier.c: the Generator modifier and evaluate_value_fmodifiers.
 * - blenkernel/intern/anim_sys.c BKE_animsys_eval_driver / BKE_animsys_write_to_rna_path.
 * - editors/animation/drivers.c alloc_driver_fcurve.
 * The expression evaluator is blenlib/intern/expr_pylike_eval.c, compiled unmodified.
 * Not ported: Python expressions (an expression the simple evaluator rejects gives 0, as a
 * Blender build without Python), Rotational Difference / Distance / Context Property variables,
 * the other F-Modifiers, driver targets in other IDs. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_expr_pylike_eval.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"

#include "project_grease_armature.h"
#include "project_grease_armature_bke.h"
#include "project_grease_armature_intern.h"
#include "project_grease_constraint.h"
#include "project_grease_driver.h"

#define CLOG_ERROR(...) ((void)0)

void BKE_object_to_mat4(Object *ob, float r_mat[4][4]); /* project_grease_constraint.c */

/* AnimationEvalContext of Blender, with the rig and the external properties. */
typedef struct AnimationEvalContext {
  PGArmature *arm;
  const PGArmExternal *ext;
  float eval_time;
} AnimationEvalContext;

/* fcurve_driver.c:35-51 (BKE_fcurve_driver.h) */
#define DRIVER_TARGETS_LOOPER_BEGIN(dvar) \
  { \
    DriverTarget *dtar = &dvar->targets[0]; \
    int tarIndex = 0; \
    for (; tarIndex < MAX_DRIVER_TARGETS; tarIndex++, dtar++)

#define DRIVER_TARGETS_USED_LOOPER_BEGIN(dvar) \
  { \
    DriverTarget *dtar = &dvar->targets[0]; \
    int tarIndex = 0; \
    for (; tarIndex < dvar->num_targets; tarIndex++, dtar++)

#define DRIVER_TARGETS_LOOPER_END \
  } \
  ((void)0)

/* ---------------------------------------------------------------------------------------- */
/* paths (the RNA subset of project_grease_driver.h)                                         */

typedef enum PathKind {
  PATH_INVALID = 0,
  PATH_BONE_LOC,
  PATH_BONE_QUAT,
  PATH_BONE_EUL,
  PATH_BONE_SCALE,
  PATH_BONE_PROP,
  PATH_OB_PROP,
  PATH_EXTERNAL,
} PathKind;

typedef struct PGPath {
  PathKind kind;
  char bone[64];
  char prop[64];
  int index;          /* trailing [i] of a variable path, -1 none */
  char base[256];     /* the path without the trailing index */
} PGPath;

/* ["..."] with \" and \\ escapes (BLI_str_escape). */
static bool parse_quoted(const char **pp, char *out, size_t cap)
{
  const char *p = *pp;
  if (p[0] != '[' || p[1] != '"') return false;
  p += 2;
  size_t n = 0;
  while (*p && *p != '"') {
    if (*p == '\\' && p[1]) p++;
    if (n + 1 >= cap) return false;
    out[n++] = *p++;
  }
  if (p[0] != '"' || p[1] != ']') return false;
  out[n] = '\0';
  *pp = p + 2;
  return true;
}

static bool path_parse(const char *path, PGPath *r, bool with_index)
{
  memset(r, 0, sizeof(*r));
  r->index = -1;
  if (!path || !path[0] || strlen(path) >= sizeof(r->base)) return false;
  BLI_strncpy(r->base, path, sizeof(r->base));
  if (with_index) {
    /* trailing [<int>] */
    size_t len = strlen(r->base);
    if (len > 2 && r->base[len - 1] == ']') {
      char *open = strrchr(r->base, '[');
      if (open && open[1] != '"') {
        char *end;
        long v = strtol(open + 1, &end, 10);
        if (end == r->base + len - 1 && end > open + 1) {
          r->index = (int)v;
          *open = '\0';
        }
      }
    }
  }
  const char *p = r->base;
  if (strncmp(p, "pose.bones", 10) == 0) {
    p += 10;
    if (!parse_quoted(&p, r->bone, sizeof(r->bone))) return false;
    if (p[0] == '[') {
      if (!parse_quoted(&p, r->prop, sizeof(r->prop)) || *p) return false;
      r->kind = PATH_BONE_PROP;
      return true;
    }
    if (STREQ(p, ".location")) r->kind = PATH_BONE_LOC;
    else if (STREQ(p, ".rotation_quaternion")) r->kind = PATH_BONE_QUAT;
    else if (STREQ(p, ".rotation_euler")) r->kind = PATH_BONE_EUL;
    else if (STREQ(p, ".scale")) r->kind = PATH_BONE_SCALE;
    return r->kind != PATH_INVALID;
  }
  if (p[0] == '[') {
    if (!parse_quoted(&p, r->prop, sizeof(r->prop)) || *p) return false;
    r->kind = PATH_OB_PROP;
    return true;
  }
  r->kind = PATH_EXTERNAL;
  return true;
}

static PGProp *prop_lookup(const PGArmature *a, const char *bone, const char *name)
{
  LISTBASE_FOREACH (PGProp *, p, &a->props) {
    if (STREQ(p->bone, bone) && STREQ(p->name, name)) return p;
  }
  return NULL;
}

/* The float a path + index points at in the rig (r_prop for clamping), NULL when invalid or out of
 * bounds. Properties are not arrays (index -1 or 0). */
static float *path_value_ptr(PGArmature *a, const PGPath *p, int index, PGProp **r_prop)
{
  *r_prop = NULL;
  if (p->kind == PATH_OB_PROP || p->kind == PATH_BONE_PROP) {
    if (index > 0) return NULL;
    if (p->kind == PATH_BONE_PROP && !BKE_pose_channel_find_name(&a->pose, p->bone)) return NULL;
    PGProp *prop = prop_lookup(a, p->kind == PATH_OB_PROP ? "" : p->bone, p->prop);
    *r_prop = prop;
    return prop ? &prop->value : NULL;
  }
  bPoseChannel *pchan = BKE_pose_channel_find_name(&a->pose, p->bone);
  if (!pchan || index < 0) return NULL;
  switch (p->kind) {
    case PATH_BONE_LOC: return index < 3 ? &pchan->loc[index] : NULL;
    case PATH_BONE_QUAT: return index < 4 ? &pchan->quat[index] : NULL;
    case PATH_BONE_EUL: return index < 3 ? &pchan->eul[index] : NULL;
    case PATH_BONE_SCALE: return index < 3 ? &pchan->size[index] : NULL;
    default: return NULL;
  }
}

static bool path_read(const AnimationEvalContext *ctx, const PGPath *p, int index, float *r)
{
  if (p->kind == PATH_EXTERNAL) {
    return ctx->ext && ctx->ext->get && ctx->ext->get(ctx->ext->user, p->base, index, r);
  }
  PGProp *prop;
  float *v = path_value_ptr(ctx->arm, p, index, &prop);
  if (!v) return false;
  *r = *v;
  return true;
}

/* anim_sys.c BKE_animsys_write_to_rna_path: floats are clamped to the property's range. */
static bool path_write(const AnimationEvalContext *ctx, const PGPath *p, int index, float value)
{
  if (p->kind == PATH_EXTERNAL) {
    return ctx->ext && ctx->ext->set && ctx->ext->set(ctx->ext->user, p->base, index, value);
  }
  PGProp *prop;
  float *v = path_value_ptr(ctx->arm, p, index, &prop);
  if (!v) return false;
  if (prop) CLAMP(value, prop->min, prop->max);
  *v = value;
  return true;
}

/* ---------------------------------------------------------------------------------------- */
/* fcurve_driver.c: variables                                                                */

/* fcurve_driver.c:149-250 dtar_get_prop_val, reading through path_read */
static float dtar_get_prop_val(const AnimationEvalContext *anim_eval_context,
                               ChannelDriver *driver,
                               DriverVar *UNUSED(dvar),
                               DriverTarget *dtar)
{
  /* Sanity check. */
  if (driver == NULL) {
    return 0.0f;
  }
  PGPath path;
  float value = 0.0f;
  if (dtar->id == NULL || !path_parse(dtar->rna_path, &path, true) ||
      !path_read(anim_eval_context, &path, path.index, &value))
  {
    /* Path couldn't be resolved (or the array index is out of bounds). */
    driver->flag |= DRIVER_FLAG_INVALID;
    dtar->flag |= DTAR_FLAG_INVALID;
    return 0.0f;
  }

  /* If we're still here, we should be ok. */
  dtar->flag &= ~DTAR_FLAG_INVALID;
  return value;
}

/* fcurve_driver.c:349-355 */
static float dvar_eval_singleProp(const AnimationEvalContext *anim_eval_context,
                                  ChannelDriver *driver,
                                  DriverVar *dvar)
{
  /* Just evaluate the first target slot. */
  return dtar_get_prop_val(anim_eval_context, driver, dvar, &dvar->targets[0]);
}

/* fcurve_driver.c:674-778 */
static void quaternion_to_angles(float quat[4], int channel)
{
  if (channel < 0) {
    quat[0] = 2.0f * saacosf(quat[0]);

    for (int i = 1; i < 4; i++) {
      quat[i] = 2.0f * saasinf(quat[i]);
    }
  }
  else if (channel == 0) {
    quat[0] = 2.0f * saacosf(quat[0]);
  }
  else {
    quat[channel] = 2.0f * saasinf(quat[channel]);
  }
}

void BKE_driver_target_matrix_to_rot_channels(
    float mat[4][4], int auto_order, int rotation_mode, int channel, bool angles, float r_buf[4])
{
  float *const quat = r_buf;
  float *const eul = r_buf + 1;

  zero_v4(r_buf);

  if (rotation_mode == DTAR_ROTMODE_AUTO) {
    mat4_to_eulO(eul, auto_order, mat);
  }
  else if (rotation_mode >= DTAR_ROTMODE_EULER_MIN && rotation_mode <= DTAR_ROTMODE_EULER_MAX) {
    mat4_to_eulO(eul, rotation_mode, mat);
  }
  else if (rotation_mode == DTAR_ROTMODE_QUATERNION) {
    mat4_to_quat(quat, mat);

    /* For Transformation constraint convenience, convert to pseudo-angles. */
    if (angles) {
      quaternion_to_angles(quat, channel);
    }
  }
  else if (rotation_mode >= DTAR_ROTMODE_SWING_TWIST_X &&
           rotation_mode <= DTAR_ROTMODE_SWING_TWIST_Z)
  {
    int axis = rotation_mode - DTAR_ROTMODE_SWING_TWIST_X;
    float raw_quat[4], twist;

    mat4_to_quat(raw_quat, mat);

    if (channel == axis + 1) {
      /* If only the twist angle is needed, skip computing swing. */
      twist = quat_split_swing_and_twist(raw_quat, axis, NULL, NULL);
    }
    else {
      twist = quat_split_swing_and_twist(raw_quat, axis, quat, NULL);

      quaternion_to_angles(quat, channel);
    }

    quat[axis + 1] = twist;
  }
  else {
    BLI_assert(false);
  }
}

/* fcurve_driver.c:525-662 */
static float dvar_eval_transChan(const AnimationEvalContext *UNUSED(anim_eval_context),
                                 ChannelDriver *driver,
                                 DriverVar *dvar)
{
  DriverTarget *dtar = &dvar->targets[0];
  Object *ob = (Object *)dtar->id;
  bPoseChannel *pchan;
  float mat[4][4];
  float oldEul[3] = {0.0f, 0.0f, 0.0f};
  bool use_eulers = false;
  short rot_order = ROT_MODE_EUL;

  /* Check if this target has valid data. */
  if ((ob == NULL) || (GS(ob->id.name) != ID_OB)) {
    /* Invalid target, so will not have enough targets. */
    driver->flag |= DRIVER_FLAG_INVALID;
    dtar->flag |= DTAR_FLAG_INVALID;
    return 0.0f;
  }

  /* Try to get pose-channel. */
  pchan = BKE_pose_channel_find_name(ob->pose, dtar->pchan_name);
  if (dtar->pchan_name[0] != '\0' && !pchan) {
    driver->flag |= DRIVER_FLAG_INVALID;
    dtar->flag |= DTAR_FLAG_INVALID;
    return 0.0f;
  }

  /* Target should be valid now. */
  dtar->flag &= ~DTAR_FLAG_INVALID;

  /* Check if object or bone, and get transform matrix accordingly:
   * - "use_eulers" code is used to prevent the problems associated with non-uniqueness
   *   of euler decomposition from matrices #20870.
   * - "local-space" is for #21384, where parent results are not wanted
   *   but #DTAR_FLAG_LOCAL_CONSTS is for all the common "corrective-shapes-for-limbs" situations.
   */
  if (pchan) {
    /* Bone. */
    if (pchan->rotmode > 0) {
      copy_v3_v3(oldEul, pchan->eul);
      rot_order = pchan->rotmode;
      use_eulers = true;
    }

    if (dtar->flag & DTAR_FLAG_LOCALSPACE) {
      if (dtar->flag & DTAR_FLAG_LOCAL_CONSTS) {
        /* Just like how the constraints do it! */
        copy_m4_m4(mat, pchan->pose_mat);
        BKE_constraint_mat_convertspace(
            ob, pchan, NULL, mat, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_LOCAL, false);
      }
      else {
        /* Specially calculate local matrix, since chan_mat is not valid
         * since it stores delta transform of pose_mat so that deforms work
         * so it cannot be used here for "transform" space. */
        BKE_pchan_to_mat4(pchan, mat);
      }
    }
    else {
      /* World-space matrix. */
      mul_m4_m4m4(mat, ob->object_to_world, pchan->pose_mat);
    }
  }
  else {
    /* Object. */
    if (ob->rotmode > 0) {
      copy_v3_v3(oldEul, ob->rot);
      rot_order = ob->rotmode;
      use_eulers = true;
    }

    if (dtar->flag & DTAR_FLAG_LOCALSPACE) {
      if (dtar->flag & DTAR_FLAG_LOCAL_CONSTS) {
        /* Just like how the constraints do it! */
        copy_m4_m4(mat, ob->object_to_world);
        BKE_constraint_mat_convertspace(
            ob, NULL, NULL, mat, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL, false);
      }
      else {
        /* Transforms to matrix. */
        BKE_object_to_mat4(ob, mat);
      }
    }
    else {
      /* World-space matrix - just the good-old one. */
      copy_m4_m4(mat, ob->object_to_world);
    }
  }

  /* Check which transform. */
  if (dtar->transChan >= MAX_DTAR_TRANSCHAN_TYPES) {
    /* Not valid channel. */
    return 0.0f;
  }
  if (dtar->transChan == DTAR_TRANSCHAN_SCALE_AVG) {
    /* Cubic root of the change in volume, equal to the geometric mean
     * of scale over all three axes unless the matrix includes shear. */
    return cbrtf(mat4_to_volume_scale(mat));
  }
  if (ELEM(dtar->transChan, DTAR_TRANSCHAN_SCALEX, DTAR_TRANSCHAN_SCALEY, DTAR_TRANSCHAN_SCALEZ)) {
    /* Extract scale, and choose the right axis,
     * inline 'mat4_to_size'. */
    return len_v3(mat[dtar->transChan - DTAR_TRANSCHAN_SCALEX]);
  }
  if (dtar->transChan >= DTAR_TRANSCHAN_ROTX) {
    /* Extract rotation as eulers (if needed)
     * - definitely if rotation order isn't eulers already
     * - if eulers, then we have 2 options:
     *     a) decompose transform matrix as required, then try to make eulers from
     *        there compatible with original values
     *     b) [NOT USED] directly use the original values (no decomposition)
     *         - only an option for "transform space", if quality is really bad with a)
     */
    float quat[4];
    int channel;

    if (dtar->transChan == DTAR_TRANSCHAN_ROTW) {
      channel = 0;
    }
    else {
      channel = 1 + dtar->transChan - DTAR_TRANSCHAN_ROTX;
      BLI_assert(channel < 4);
    }

    BKE_driver_target_matrix_to_rot_channels(
        mat, rot_order, dtar->rotation_mode, channel, false, quat);

    if (use_eulers && dtar->rotation_mode == DTAR_ROTMODE_AUTO) {
      compatible_eul(quat + 1, oldEul);
    }

    return quat[channel];
  }

  /* Extract location and choose right axis. */
  return mat[3][dtar->transChan];
}

/* fcurve_driver.c:744-793, the two supported variable types */
typedef struct DriverVarTypeInfo {
  float (*get_value)(const AnimationEvalContext *anim_eval_context,
                     ChannelDriver *driver,
                     DriverVar *dvar);
  int num_targets;
  short target_flags[MAX_DRIVER_TARGETS];
} DriverVarTypeInfo;

static const DriverVarTypeInfo dvar_types[MAX_DVAR_TYPES] = {
    [DVAR_TYPE_SINGLE_PROP] = {dvar_eval_singleProp, 1, {0}},
    [DVAR_TYPE_TRANSFORM_CHAN] = {dvar_eval_transChan, 1, {DTAR_FLAG_STRUCT_REF | DTAR_FLAG_ID_OB_ONLY}},
};

static const DriverVarTypeInfo *get_dvar_typeinfo(int type)
{
  /* Check if valid type. */
  if ((type >= 0) && (type < MAX_DVAR_TYPES) && dvar_types[type].get_value) {
    return &dvar_types[type];
  }

  return NULL;
}

/* fcurve_driver.c:1200-1218 (no Python) */
static void BKE_driver_invalidate_expression(ChannelDriver *driver,
                                             bool expr_changed,
                                             bool varname_changed)
{
  if (expr_changed || varname_changed) {
    BLI_expr_pylike_free(driver->expr_simple);
    driver->expr_simple = NULL;
  }
}

/* fcurve_driver.c:796-844 */
static void driver_free_variable(ListBase *variables, DriverVar *dvar)
{
  /* Sanity checks. */
  if (dvar == NULL) {
    return;
  }

  /* Free target vars:
   * - need to go over all of them, not just up to the ones that are used
   *   currently, since there may be some lingering RNA paths from
   *   previous users needing freeing
   */
  DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
    /* Free RNA path if applicable. */
    if (dtar->rna_path) {
      MEM_freeN(dtar->rna_path);
    }
  }
  DRIVER_TARGETS_LOOPER_END;

  /* Remove the variable from the driver. */
  BLI_freelinkN(variables, dvar);
}

static void driver_free_variable_ex(ChannelDriver *driver, DriverVar *dvar)
{
  /* Remove and free the driver variable. */
  driver_free_variable(&driver->variables, dvar);

  /* Since driver variables are cached, the expression needs re-compiling too. */
  BKE_driver_invalidate_expression(driver, false, true);
}

static void driver_variables_copy(ListBase *dst_vars, const ListBase *src_vars)
{
  BLI_assert(BLI_listbase_is_empty(dst_vars));
  BLI_duplicatelist(dst_vars, src_vars);

  LISTBASE_FOREACH (DriverVar *, dvar, dst_vars) {
    /* Need to go over all targets so that we don't leave any dangling paths. */
    DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
      /* Make a copy of target's rna path if available. */
      if (dtar->rna_path) {
        dtar->rna_path = MEM_dupallocN(dtar->rna_path);
      }
    }
    DRIVER_TARGETS_LOOPER_END;
  }
}

/* fcurve_driver.c:846-873 */
static void driver_change_variable_type(DriverVar *dvar, int type)
{
  const DriverVarTypeInfo *dvti = get_dvar_typeinfo(type);

  /* Sanity check. */
  if (ELEM(NULL, dvar, dvti)) {
    return;
  }

  /* Set the new settings. */
  dvar->type = type;
  dvar->num_targets = dvti->num_targets;

  /* Make changes to the targets based on the defines for these types.
   * NOTE: only need to make sure the ones we're using here are valid. */
  DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
    short flags = dvti->target_flags[tarIndex];

    /* Store the flags. */
    dtar->flag = flags;

    /* Object ID types only, or idtype not yet initialized. */
    if ((flags & DTAR_FLAG_ID_OB_ONLY) || (dtar->idtype == 0)) {
      dtar->idtype = ID_OB;
    }
  }
  DRIVER_TARGETS_LOOPER_END;
}

/* fcurve_driver.c:875-942 (no Python keyword check) */
static void driver_variable_name_validate(DriverVar *dvar)
{
  /* Special character blacklist */
  const char special_char_blacklist[] = {
      '~', '`', '!', '@', '#', '$', '%', '^', '&', '*', '+', '=', '-',  '/',  '\\',
      '?', ':', ';', '<', '>', '{', '}', '[', ']', '|', ' ', '.', '\t', '\n', '\r',
  };

  /* Sanity checks. */
  if (dvar == NULL) {
    return;
  }

  /* Clear all invalid-name flags. */
  dvar->flag &= ~DVAR_ALL_INVALID_FLAGS;

  /* 0) Zero-length identifiers are not allowed */
  if (dvar->name[0] == '\0') {
    dvar->flag |= DVAR_FLAG_INVALID_EMPTY;
  }

  /* 1) Must start with a letter */
  /* XXX: We assume that valid unicode letters in other languages are ok too,
   * hence the blacklisting. */
  if (IN_RANGE_INCL(dvar->name[0], '0', '9')) {
    dvar->flag |= DVAR_FLAG_INVALID_START_NUM;
  }
  else if (dvar->name[0] == '_') {
    /* NOTE: We don't allow names to start with underscores
     * (i.e. it helps when ruling out security risks) */
    dvar->flag |= DVAR_FLAG_INVALID_START_CHAR;
  }

  /* 2) Must not contain invalid stuff in the middle of the string */
  if (strchr(dvar->name, ' ')) {
    dvar->flag |= DVAR_FLAG_INVALID_HAS_SPACE;
  }
  if (strchr(dvar->name, '.')) {
    dvar->flag |= DVAR_FLAG_INVALID_HAS_DOT;
  }

  /* 3) Check for special characters - Either at start, or in the middle */
  for (int i = 0; i < sizeof(special_char_blacklist); i++) {
    char *match = strchr(dvar->name, special_char_blacklist[i]);

    if (match == dvar->name) {
      dvar->flag |= DVAR_FLAG_INVALID_START_CHAR;
    }
    else if (match != NULL) {
      dvar->flag |= DVAR_FLAG_INVALID_HAS_SPECIAL;
    }
  }

  /* If any these conditions match, the name is invalid */
  if (dvar->flag & DVAR_ALL_INVALID_FLAGS) {
    dvar->flag |= DVAR_FLAG_INVALID_NAME;
  }
}

/* fcurve_driver.c:982-1046 (no Python) */
static void fcurve_free_driver(FCurve *fcu)
{
  ChannelDriver *driver;
  DriverVar *dvar, *dvarn;

  /* Sanity checks. */
  if (ELEM(NULL, fcu, fcu->driver)) {
    return;
  }
  driver = fcu->driver;

  /* Free driver targets. */
  for (dvar = driver->variables.first; dvar; dvar = dvarn) {
    dvarn = dvar->next;
    driver_free_variable_ex(driver, dvar);
  }

  BLI_expr_pylike_free(driver->expr_simple);

  /* Free driver itself, then set F-Curve's point to this to NULL
   * (as the curve may still be used). */
  MEM_freeN(driver);
  fcu->driver = NULL;
}

static ChannelDriver *fcurve_copy_driver(const ChannelDriver *driver)
{
  ChannelDriver *ndriver;

  /* Sanity checks. */
  if (driver == NULL) {
    return NULL;
  }

  /* Copy all data. */
  ndriver = MEM_dupallocN(driver);
  ndriver->expr_comp = NULL;
  ndriver->expr_simple = NULL;

  /* Copy variables. */

  /* To get rid of refs to non-copied data (that's still used on original). */
  BLI_listbase_clear(&ndriver->variables);
  driver_variables_copy(&ndriver->variables, &driver->variables);

  /* Return the new driver. */
  return ndriver;
}

/* ---------------------------------------------------------------------------------------- */
/* fcurve_driver.c:1046-1157 simple expressions                                              */

/* Index constants for the expression parameter array. */
enum {
  /* Index of the 'frame' variable. */
  VAR_INDEX_FRAME = 0,
  /* Index of the first user-defined driver variable. */
  VAR_INDEX_CUSTOM
};

static ExprPyLike_Parsed *driver_compile_simple_expr_impl(ChannelDriver *driver)
{
  /* Prepare parameter names. */
  int names_len = BLI_listbase_count(&driver->variables);
  const char **names = BLI_array_alloca(names, names_len + VAR_INDEX_CUSTOM);
  int i = VAR_INDEX_CUSTOM;

  names[VAR_INDEX_FRAME] = "frame";

  LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
    names[i++] = dvar->name;
  }

  return BLI_expr_pylike_parse(driver->expression, names, names_len + VAR_INDEX_CUSTOM);
}

static float driver_get_variable_value(const AnimationEvalContext *anim_eval_context,
                                       ChannelDriver *driver,
                                       DriverVar *dvar);

static bool driver_evaluate_simple_expr(const AnimationEvalContext *anim_eval_context,
                                        ChannelDriver *driver,
                                        ExprPyLike_Parsed *expr,
                                        float *result,
                                        float time)
{
  /* Prepare parameter values. */
  int vars_len = BLI_listbase_count(&driver->variables);
  double *vars = BLI_array_alloca(vars, vars_len + VAR_INDEX_CUSTOM);
  int i = VAR_INDEX_CUSTOM;

  vars[VAR_INDEX_FRAME] = time;

  LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
    vars[i++] = driver_get_variable_value(anim_eval_context, driver, dvar);
  }

  /* Evaluate expression. */
  double result_val;
  eExprPyLike_EvalStatus status = BLI_expr_pylike_eval(
      expr, vars, vars_len + VAR_INDEX_CUSTOM, &result_val);

  switch (status) {
    case EXPR_PYLIKE_SUCCESS:
      if (isfinite(result_val)) {
        *result = (float)result_val;
      }
      return true;

    case EXPR_PYLIKE_DIV_BY_ZERO:
    case EXPR_PYLIKE_MATH_ERROR:
      CLOG_ERROR(&LOG, "%s in Driver: '%s'", message, driver->expression);

      driver->flag |= DRIVER_FLAG_INVALID;
      return true;

    default:
      /* Arriving here means a bug, not user error. */
      CLOG_ERROR(&LOG, "simple driver expression evaluation failed: '%s'", driver->expression);
      return false;
  }
}

/* Compile and cache the driver expression if necessary (single-threaded here). */
static bool driver_compile_simple_expr(ChannelDriver *driver)
{
  if (driver->expr_simple != NULL) {
    return true;
  }

  if (driver->type != DRIVER_TYPE_PYTHON) {
    return false;
  }

  driver->expr_simple = driver_compile_simple_expr_impl(driver);
  return true;
}

static bool driver_try_evaluate_simple_expr(const AnimationEvalContext *anim_eval_context,
                                            ChannelDriver *driver,
                                            ChannelDriver *driver_orig,
                                            float *result,
                                            float time)
{
  *result = 0.0f;

  return driver_compile_simple_expr(driver_orig) &&
         BLI_expr_pylike_is_valid(driver_orig->expr_simple) &&
         driver_evaluate_simple_expr(
             anim_eval_context, driver, driver_orig->expr_simple, result, time);
}

/* ---------------------------------------------------------------------------------------- */
/* fcurve_driver.c:1226-1381 evaluation                                                      */

static float driver_get_variable_value(const AnimationEvalContext *anim_eval_context,
                                       ChannelDriver *driver,
                                       DriverVar *dvar)
{
  const DriverVarTypeInfo *dvti;

  /* Sanity check. */
  if (ELEM(NULL, driver, dvar)) {
    return 0.0f;
  }

  /* Call the relevant callbacks to get the variable value
   * using the variable type info, storing the obtained value
   * in `dvar->curval` so that drivers can be debugged. */
  dvti = get_dvar_typeinfo(dvar->type);

  if (dvti && dvti->get_value) {
    dvar->curval = dvti->get_value(anim_eval_context, driver, dvar);
  }
  else {
    dvar->curval = 0.0f;
  }

  return dvar->curval;
}

static void evaluate_driver_sum(const AnimationEvalContext *anim_eval_context,
                                ChannelDriver *driver)
{
  DriverVar *dvar;

  /* Check how many variables there are first (i.e. just one?). */
  if (BLI_listbase_is_single(&driver->variables)) {
    /* Just one target, so just use that. */
    dvar = driver->variables.first;
    driver->curval = driver_get_variable_value(anim_eval_context, driver, dvar);
    return;
  }

  /* More than one target, so average the values of the targets. */
  float value = 0.0f;
  int tot = 0;

  /* Loop through targets, adding (hopefully we don't get any overflow!). */
  for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
    value += driver_get_variable_value(anim_eval_context, driver, dvar);
    tot++;
  }

  /* Perform operations on the total if appropriate. */
  if (driver->type == DRIVER_TYPE_AVERAGE) {
    driver->curval = tot ? (value / (float)tot) : 0.0f;
  }
  else {
    driver->curval = value;
  }
}

static void evaluate_driver_min_max(const AnimationEvalContext *anim_eval_context,
                                    ChannelDriver *driver)
{
  DriverVar *dvar;
  float value = 0.0f;

  /* Loop through the variables, getting the values and comparing them to existing ones. */
  for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
    /* Get value. */
    float tmp_val = driver_get_variable_value(anim_eval_context, driver, dvar);

    /* Store this value if appropriate. */
    if (dvar->prev) {
      /* Check if greater/smaller than the baseline. */
      if (driver->type == DRIVER_TYPE_MAX) {
        /* Max? */
        if (tmp_val > value) {
          value = tmp_val;
        }
      }
      else {
        /* Min? */
        if (tmp_val < value) {
          value = tmp_val;
        }
      }
    }
    else {
      /* First item - make this the baseline for comparisons. */
      value = tmp_val;
    }
  }

  /* Store value in driver. */
  driver->curval = value;
}

static void evaluate_driver_python(ChannelDriver *driver,
                                   ChannelDriver *driver_orig,
                                   const AnimationEvalContext *anim_eval_context)
{
  /* Check for empty or invalid expression. */
  if ((driver_orig->expression[0] == '\0') || (driver_orig->flag & DRIVER_FLAG_INVALID)) {
    driver->curval = 0.0f;
  }
  else if (!driver_try_evaluate_simple_expr(anim_eval_context,
                                            driver,
                                            driver_orig,
                                            &driver->curval,
                                            anim_eval_context->eval_time))
  {
    /* WITH_PYTHON off: a non-simple expression is not evaluated (curval stays 0). */
  }
}

static float evaluate_driver(ChannelDriver *driver,
                             ChannelDriver *driver_orig,
                             const AnimationEvalContext *anim_eval_context)
{
  /* Check if driver can be evaluated. */
  if (driver_orig->flag & DRIVER_FLAG_INVALID) {
    return 0.0f;
  }

  switch (driver->type) {
    case DRIVER_TYPE_AVERAGE: /* Average values of driver targets. */
    case DRIVER_TYPE_SUM:     /* Sum values of driver targets. */
      evaluate_driver_sum(anim_eval_context, driver);
      break;
    case DRIVER_TYPE_MIN: /* Smallest value. */
    case DRIVER_TYPE_MAX: /* Largest value. */
      evaluate_driver_min_max(anim_eval_context, driver);
      break;
    case DRIVER_TYPE_PYTHON: /* Expression. */
      evaluate_driver_python(driver, driver_orig, anim_eval_context);
      break;
    default:
      /* Special 'hack' - just use stored value
       * This is currently used as the mechanism which allows animated settings to be able
       * to be changed via the UI. */
      break;
  }

  /* Return value for driver. */
  return driver->curval;
}

/* ---------------------------------------------------------------------------------------- */
/* fmodifier.c: Generator                                                                    */

/* fmodifier.c:90-122 */
static void fcm_generator_free(FModifier *fcm)
{
  FMod_Generator *data = (FMod_Generator *)fcm->data;

  /* free polynomial coefficients array */
  if (data->coefficients) {
    MEM_freeN(data->coefficients);
  }
}

static void fcm_generator_copy(FModifier *fcm, const FModifier *src)
{
  FMod_Generator *gen = (FMod_Generator *)fcm->data;
  FMod_Generator *ogen = (FMod_Generator *)src->data;

  /* copy coefficients array? */
  if (ogen->coefficients) {
    gen->coefficients = MEM_dupallocN(ogen->coefficients);
  }
}

static void fcm_generator_new_data(void *mdata)
{
  FMod_Generator *data = (FMod_Generator *)mdata;
  float *cp;

  /* set default generator to be linear 0-1 (gradient = 1, y-offset = 0) */
  data->poly_order = 1;
  data->arraysize = 2;
  cp = data->coefficients = MEM_callocN(sizeof(float) * 2, "FMod_Generator_Coefs");
  cp[0] = 0; /* y-offset */
  cp[1] = 1; /* gradient */
}

/* fmodifier.c:124-151 */
static void fcm_generator_verify(FModifier *fcm)
{
  FMod_Generator *data = (FMod_Generator *)fcm->data;

  /* requirements depend on mode */
  switch (data->mode) {
    case FCM_GENERATOR_POLYNOMIAL: /* expanded polynomial expression */
    {
      const int arraysize_new = data->poly_order + 1;
      /* arraysize needs to be order+1, so resize if not */
      if (data->arraysize != arraysize_new) {
        data->coefficients = MEM_recallocN(data->coefficients, sizeof(float) * arraysize_new);
        data->arraysize = arraysize_new;
      }
      break;
    }
    case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* expanded polynomial expression */
    {
      const int arraysize_new = data->poly_order * 2;
      /* arraysize needs to be (2 * order), so resize if not */
      if (data->arraysize != arraysize_new) {
        data->coefficients = MEM_recallocN(data->coefficients, sizeof(float) * arraysize_new);
        data->arraysize = arraysize_new;
      }
      break;
    }
  }
}

/* fmodifier.c:153-226 */
static void fcm_generator_evaluate(
    FCurve *UNUSED(fcu), FModifier *fcm, float *cvalue, float evaltime, void *UNUSED(storage))
{
  FMod_Generator *data = (FMod_Generator *)fcm->data;

  /* behavior depends on mode
   * NOTE: the data in its default state is fine too
   */
  switch (data->mode) {
    case FCM_GENERATOR_POLYNOMIAL: /* expanded polynomial expression */
    {
      /* we overwrite cvalue with the sum of the polynomial */
      float *powers = MEM_callocN(sizeof(float) * data->arraysize, "Poly Powers");
      float value = 0.0f;

      /* for each x^n, precalculate value based on previous one first... this should be
       * faster that calling pow() for each entry
       */
      for (uint i = 0; i < data->arraysize; i++) {
        /* first entry is x^0 = 1, otherwise, calculate based on previous */
        if (i) {
          powers[i] = powers[i - 1] * evaltime;
        }
        else {
          powers[0] = 1;
        }
      }

      /* for each coefficient, add to value, which we'll write to *cvalue in one go */
      for (uint i = 0; i < data->arraysize; i++) {
        value += data->coefficients[i] * powers[i];
      }

      /* only if something changed, write *cvalue in one go */
      if (data->poly_order) {
        if (data->flag & FCM_GENERATOR_ADDITIVE) {
          *cvalue += value;
        }
        else {
          *cvalue = value;
        }
      }

      /* cleanup */
      if (powers) {
        MEM_freeN(powers);
      }
      break;
    }
    case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* Factorized polynomial */
    {
      float value = 1.0f, *cp = NULL;
      uint i;

      /* For each coefficient pair,
       * solve for that bracket before accumulating in value by multiplying. */
      for (cp = data->coefficients, i = 0; (cp) && (i < (uint)data->poly_order); cp += 2, i++) {
        value *= (cp[0] * evaltime + cp[1]);
      }

      /* only if something changed, write *cvalue in one go */
      if (data->poly_order) {
        if (data->flag & FCM_GENERATOR_ADDITIVE) {
          *cvalue += value;
        }
        else {
          *cvalue = value;
        }
      }
      break;
    }
  }
}

/* fmodifier.c:1350-1393 */
static float eval_fmodifier_influence(FModifier *fcm, float evaltime)
{
  float influence;

  /* sanity check */
  if (fcm == NULL) {
    return 0.0f;
  }

  /* should we use influence stored in modifier or not
   * NOTE: this is really just a hack so that we don't need to version patch old files ;)
   */
  if (fcm->flag & FMODIFIER_FLAG_USEINFLUENCE) {
    influence = fcm->influence;
  }
  else {
    influence = 1.0f;
  }

  /* restricted range or full range? */
  if (fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) {
    if ((evaltime < fcm->sfra) || (evaltime > fcm->efra)) {
      /* out of range */
      return 0.0f;
    }
    if ((fcm->blendin != 0.0f) && (evaltime >= fcm->sfra) &&
        (evaltime <= fcm->sfra + fcm->blendin)) {
      /* blend in range */
      float a = fcm->sfra;
      float b = fcm->sfra + fcm->blendin;
      return influence * (evaltime - a) / (b - a);
    }
    if ((fcm->blendout != 0.0f) && (evaltime <= fcm->efra) &&
        (evaltime >= fcm->efra - fcm->blendout)) {
      /* blend out range */
      float a = fcm->efra;
      float b = fcm->efra - fcm->blendout;
      return influence * (evaltime - a) / (b - a);
    }
  }

  /* just return the influence of the modifier */
  return influence;
}

/* fmodifier.c:1453-1497 with the Generator as the only type (no time modifiers, no storage) */
static void evaluate_value_fmodifiers(ListBase *modifiers, FCurve *fcu, float *cvalue, float evaltime)
{
  FModifier *fcm;

  /* sanity checks */
  if (ELEM(NULL, modifiers, modifiers->first)) {
    return;
  }

  if (fcu->flag & FCURVE_MOD_OFF) {
    return;
  }

  /* evaluate modifiers */
  for (fcm = modifiers->first; fcm; fcm = fcm->next) {
    if (fcm->type != FMODIFIER_TYPE_GENERATOR) {
      continue;
    }

    /* Only evaluate if there's a callback for this,
     * and if F-Modifier can be evaluated on this frame. */
    if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) == 0 ||
        ((fcm->sfra <= evaltime) && (fcm->efra >= evaltime)))
    {
      if ((fcm->flag & (FMODIFIER_FLAG_DISABLED | FMODIFIER_FLAG_MUTED)) == 0) {
        float nval = *cvalue;
        fcm_generator_evaluate(fcu, fcm, &nval, evaltime, NULL);

        float influence = eval_fmodifier_influence(fcm, evaltime);
        *cvalue = interpf(nval, *cvalue, influence);
      }
    }
  }
}

/* fmodifier.c:1084-1131 add_fmodifier for the Generator */
static FModifier *add_fmodifier_generator(ListBase *modifiers, FCurve *owner_fcu)
{
  FModifier *fcm = MEM_callocN(sizeof(FModifier), "F-Curve Modifier");
  fcm->type = FMODIFIER_TYPE_GENERATOR;
  fcm->ui_expand_flag = 1; /* UI_PANEL_DATA_EXPAND_ROOT */
  fcm->curve = owner_fcu;
  fcm->influence = 1.0f;
  BLI_addtail(modifiers, fcm);
  STRNCPY(fcm->name, "Generator"); /* BKE_fmodifier_name_set: the type name, unique */
  BLI_uniquename(modifiers, fcm, "Generator", '.', offsetof(FModifier, name), sizeof(fcm->name));
  if (BLI_listbase_is_single(modifiers)) {
    fcm->flag |= FMODIFIER_FLAG_ACTIVE;
  }
  fcm->data = MEM_callocN(sizeof(FMod_Generator), "FMod_Generator");
  fcm_generator_new_data(fcm->data);
  return fcm;
}

static void fmodifiers_free(ListBase *modifiers)
{
  LISTBASE_FOREACH_MUTABLE (FModifier *, fcm, modifiers) {
    if (fcm->type == FMODIFIER_TYPE_GENERATOR && fcm->data) fcm_generator_free(fcm);
    MEM_SAFE_FREE(fcm->data);
    BLI_freelinkN(modifiers, fcm);
  }
}

/* ---------------------------------------------------------------------------------------- */
/* fcurve.c                                                                                  */

/* fcurve.c:2320-2353 evaluate_fcurve_ex (no samples, no time modifiers) */
static float evaluate_fcurve_ex(FCurve *fcu, float evaltime, float cvalue)
{
  const float devaltime = evaltime;

  if (fcu->bezt) {
    cvalue = pg_bke_fcurve_eval_keyframes(fcu, devaltime);
  }

  /* Evaluate modifiers. */
  evaluate_value_fmodifiers(&fcu->modifiers, fcu, &cvalue, devaltime);

  /* If curve can only have integral values, perform truncation (i.e. drop the decimal part)
   * here so that the curve can be sampled correctly.
   */
  if (fcu->flag & FCURVE_INT_VALUES) {
    cvalue = floorf(cvalue + 0.5f);
  }

  return cvalue;
}

/* fcurve.c:2370-2412 */
static float evaluate_fcurve_driver(FCurve *fcu,
                                    ChannelDriver *driver_orig,
                                    const AnimationEvalContext *anim_eval_context)
{
  BLI_assert(fcu->driver != NULL);
  float cvalue = 0.0f;
  float evaltime = anim_eval_context->eval_time;

  /* If there is a driver (only if this F-Curve is acting as 'driver'),
   * evaluate it to find value to use as "evaltime" since drivers essentially act as alternative
   * input (i.e. in place of 'time') for F-Curves. */
  if (fcu->driver) {
    /* Evaltime now serves as input for the curve. */
    evaltime = evaluate_driver(fcu->driver, driver_orig, anim_eval_context);

    /* Only do a default 1-1 mapping if it's unlikely that anything else will set a value... */
    if (fcu->totvert == 0) {
      bool do_linear = true;

      /* Out-of-range F-Modifiers will block, as will those which just plain overwrite the values
       * XXX: additive is a bit more dicey; it really depends then if things are in range or not...
       */
      LISTBASE_FOREACH (FModifier *, fcm, &fcu->modifiers) {
        /* If there are range-restrictions, we must definitely block #36950. */
        if ((fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) == 0 ||
            (fcm->sfra <= evaltime && fcm->efra >= evaltime))
        {
          /* Within range: here it probably doesn't matter,
           * though we'd want to check on additive. */
        }
        else {
          /* Outside range: modifier shouldn't contribute to the curve here,
           * though it does in other areas, so neither should the driver! */
          do_linear = false;
        }
      }

      /* Only copy over results if none of the modifiers disagreed with this. */
      if (do_linear) {
        cvalue = evaltime;
      }
    }
  }

  return evaluate_fcurve_ex(fcu, evaltime, cvalue);
}

/* ---------------------------------------------------------------------------------------- */
/* lifetime                                                                                  */

FCurve *pg_drv_new_fcurve(const char *path, int index)
{
  /* alloc_driver_fcurve (DRIVER_FCURVE_EMPTY part) */
  FCurve *fcu = MEM_callocN(sizeof(FCurve), "FCurve");
  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->auto_smoothing = FCURVE_SMOOTH_CONT_ACCEL; /* U.auto_smoothing_new default */
  fcu->rna_path = BLI_strdup(path);
  fcu->array_index = index;
  fcu->driver = MEM_callocN(sizeof(ChannelDriver), "ChannelDriver");
  return fcu;
}

void pg_drv_fcurve_free(FCurve *fcu)
{
  fcurve_free_driver(fcu);
  fmodifiers_free(&fcu->modifiers);
  MEM_SAFE_FREE(fcu->bezt);
  MEM_SAFE_FREE(fcu->rna_path);
  MEM_freeN(fcu);
}

void pg_drv_free_all(PGArmature *a)
{
  LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &a->drivers) pg_drv_fcurve_free(fcu);
  BLI_listbase_clear(&a->drivers);
  BLI_freelistN(&a->props);
}

void pg_drv_copy_all(PGArmature *dst, const PGArmature *src)
{
  BLI_duplicatelist(&dst->props, &src->props);
  LISTBASE_FOREACH (const FCurve *, fcu, &src->drivers) {
    /* BKE_fcurve_copy */
    FCurve *c = MEM_dupallocN(fcu);
    c->next = c->prev = NULL;
    c->bezt = fcu->bezt ? MEM_dupallocN(fcu->bezt) : NULL;
    c->rna_path = fcu->rna_path ? BLI_strdup(fcu->rna_path) : NULL;
    c->driver = fcurve_copy_driver(fcu->driver);
    LISTBASE_FOREACH (DriverVar *, dvar, &c->driver->variables) {
      DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
        if (dtar->id) dtar->id = &dst->ob.id;
      }
      DRIVER_TARGETS_LOOPER_END;
    }
    BLI_duplicatelist(&c->modifiers, &fcu->modifiers);
    FModifier *s = fcu->modifiers.first;
    LISTBASE_FOREACH (FModifier *, fcm, &c->modifiers) {
      fcm->data = MEM_dupallocN(s->data);
      fcm->curve = c;
      if (fcm->type == FMODIFIER_TYPE_GENERATOR) fcm_generator_copy(fcm, s);
      s = s->next;
    }
    BLI_addtail(&dst->drivers, c);
  }
}

static void rename_in_path(char **path, const char *oldp, const char *newp)
{
  const size_t ol = strlen(oldp);
  if (*path && strncmp(*path, oldp, ol) == 0) {
    const size_t len = strlen(newp) + strlen(*path + ol) + 1;
    char *s = MEM_mallocN(len, "rna_path");
    BLI_snprintf(s, len, "%s%s", newp, *path + ol);
    MEM_freeN(*path);
    *path = s;
  }
}

void pg_drv_rename_bone(PGArmature *a, const char *oldname, const char *newname)
{
  /* BKE_animdata_fix_paths_rename (driver targets) and the bone's ID properties */
  char oldp[MAXBONENAME + 16], newp[MAXBONENAME + 16];
  BLI_snprintf(oldp, sizeof(oldp), "pose.bones[\"%s\"]", oldname);
  BLI_snprintf(newp, sizeof(newp), "pose.bones[\"%s\"]", newname);
  LISTBASE_FOREACH (FCurve *, fcu, &a->drivers) {
    LISTBASE_FOREACH (DriverVar *, dvar, &fcu->driver->variables) {
      DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
        rename_in_path(&dtar->rna_path, oldp, newp);
        if (STREQ(dtar->pchan_name, oldname)) STRNCPY(dtar->pchan_name, newname);
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }
  LISTBASE_FOREACH (PGProp *, p, &a->props) {
    if (p->bone[0] && STREQ(p->bone, oldname)) STRNCPY(p->bone, newname);
  }
}

/* ---------------------------------------------------------------------------------------- */
/* evaluation                                                                                */

/* What a driver writes / reads, for the dependency order. */
static bool driver_writes_bone_transform(const FCurve *fcu, char r_bone[64])
{
  PGPath p;
  if (!path_parse(fcu->rna_path, &p, false)) return false;
  if (!ELEM(p.kind, PATH_BONE_LOC, PATH_BONE_QUAT, PATH_BONE_EUL, PATH_BONE_SCALE)) return false;
  BLI_strncpy(r_bone, p.bone, 64);
  return true;
}

static bool bone_is_or_below(const PGArmature *a, const char *bone, const char *ancestor)
{
  for (bPoseChannel *pc = BKE_pose_channel_find_name(&a->pose, bone); pc; pc = pc->parent) {
    if (STREQ(pc->name, ancestor)) return true;
  }
  return false;
}

/* true when driver `reader` reads what `writer` writes */
static bool driver_depends_on(const PGArmature *a, const FCurve *reader, const FCurve *writer)
{
  char wbone[64];
  const bool w_transform = driver_writes_bone_transform(writer, wbone);
  LISTBASE_FOREACH (const DriverVar *, dvar, &reader->driver->variables) {
    const DriverTarget *dtar = &dvar->targets[0];
    if (dvar->type == DVAR_TYPE_SINGLE_PROP && dtar->rna_path) {
      PGPath p;
      if (!path_parse(dtar->rna_path, &p, true)) continue;
      const int ri = p.index < 0 ? 0 : p.index;
      if (STREQ(p.base, writer->rna_path) && ri == writer->array_index) return true;
    }
    else if (dvar->type == DVAR_TYPE_TRANSFORM_CHAN && w_transform) {
      /* the matrix of a bone follows its own channels and its parents' */
      if (dtar->pchan_name[0] && bone_is_or_below(a, dtar->pchan_name, wbone)) return true;
    }
  }
  return false;
}

static bool driver_reads_pose(const FCurve *fcu)
{
  LISTBASE_FOREACH (const DriverVar *, dvar, &fcu->driver->variables) {
    if (dvar->type == DVAR_TYPE_TRANSFORM_CHAN) return true;
  }
  return false;
}

void pg_drv_evaluate(PGArmature *a, float frame, const PGArmExternal *ext)
{
  pg_bke_pose_where_is(&a->ob);
  const int count = BLI_listbase_count(&a->drivers);
  if (count == 0) return;

  /* Kahn's topological sort on "reader depends on writer"; a cycle keeps list order. */
  FCurve **list = MEM_mallocN(sizeof(FCurve *) * count, __func__);
  FCurve **order = MEM_mallocN(sizeof(FCurve *) * count, __func__);
  int *indegree = MEM_callocN(sizeof(int) * count, __func__);
  bool *done = MEM_callocN(sizeof(bool) * count, __func__);
  bool *dep = MEM_callocN(sizeof(bool) * count * count, __func__); /* dep[r * count + w] */
  int i = 0;
  LISTBASE_FOREACH (FCurve *, fcu, &a->drivers) list[i++] = fcu;
  for (int r = 0; r < count; r++) {
    for (int w = 0; w < count; w++) {
      if (r != w && driver_depends_on(a, list[r], list[w])) {
        dep[r * count + w] = true;
        indegree[r]++;
      }
    }
  }
  for (int n = 0; n < count; n++) {
    int pick = -1;
    for (int k = 0; k < count && pick < 0; k++) {
      if (!done[k] && indegree[k] == 0) pick = k;
    }
    for (int k = 0; k < count && pick < 0; k++) {
      if (!done[k]) pick = k; /* cycle */
    }
    done[pick] = true;
    order[n] = list[pick];
    for (int r = 0; r < count; r++) {
      if (dep[r * count + pick]) indegree[r]--;
    }
  }

  const AnimationEvalContext ctx = {a, ext, frame};
  bool pose_dirty = false;
  for (int n = 0; n < count; n++) {
    FCurve *fcu = order[n];
    ChannelDriver *driver = fcu->driver;
    /* anim_sys.c BKE_animsys_eval_driver */
    if ((fcu->flag & (FCURVE_MUTED | FCURVE_DISABLED)) || !driver ||
        (driver->flag & DRIVER_FLAG_INVALID))
    {
      continue;
    }
    if (pose_dirty && driver_reads_pose(fcu)) {
      pg_bke_pose_where_is(&a->ob);
      pose_dirty = false;
    }
    PGPath path;
    bool ok = false;
    if (path_parse(fcu->rna_path, &path, false)) {
      /* calculate_fcurve: a driver F-Curve is never "empty" */
      const float curval = evaluate_fcurve_driver(fcu, driver, &ctx);
      fcu->curval = curval;
      ok = path_write(&ctx, &path, fcu->array_index, curval);
      char bone[64];
      if (ok && driver_writes_bone_transform(fcu, bone)) pose_dirty = true;
    }
    /* set error-flag if evaluation failed */
    if (!ok) {
      driver->flag |= DRIVER_FLAG_INVALID;
    }
  }
  MEM_freeN(list);
  MEM_freeN(order);
  MEM_freeN(indegree);
  MEM_freeN(done);
  MEM_freeN(dep);
  pg_bke_pose_where_is(&a->ob);
}

/* ---------------------------------------------------------------------------------------- */
/* custom properties                                                                         */

static const char *bone_name_or_empty(const PGArmature *a, int bone, bool *ok)
{
  *ok = true;
  if (bone < 0) return "";
  const char *n = pg_arm_bone_name(a, bone);
  if (!n) *ok = false;
  return n ? n : "";
}

int pg_arm_prop_add(PGArmature *a, int bone, const char *name, float value, float min, float max,
                    float def)
{
  bool ok;
  const char *owner = a ? bone_name_or_empty(a, bone, &ok) : NULL;
  if (!a || !ok || !name || !name[0] || strlen(name) >= 64 || !(min <= max) || !isfinite(value) ||
      !isfinite(def))
  {
    return -1;
  }
  PGProp *p = prop_lookup(a, owner, name);
  if (!p) {
    p = MEM_callocN(sizeof(PGProp), "PGProp");
    STRNCPY(p->bone, owner);
    STRNCPY(p->name, name);
    BLI_addtail(&a->props, p);
  }
  p->min = min;
  p->max = max;
  p->value = clamp_f(value, min, max);
  p->def = def;
  return BLI_findindex(&a->props, p);
}

int pg_arm_prop_remove(PGArmature *a, int index)
{
  PGProp *p = a && index >= 0 ? BLI_findlink(&a->props, index) : NULL;
  if (!p) return 0;
  BLI_freelinkN(&a->props, p);
  return 1;
}

int pg_arm_prop_count(const PGArmature *a) { return a ? BLI_listbase_count(&a->props) : 0; }

int pg_arm_prop_find(const PGArmature *a, int bone, const char *name)
{
  bool ok;
  const char *owner = a && name ? bone_name_or_empty(a, bone, &ok) : NULL;
  PGProp *p = owner && ok ? prop_lookup(a, owner, name) : NULL;
  return p ? BLI_findindex(&a->props, p) : -1;
}

int pg_arm_prop_set(PGArmature *a, int index, float value)
{
  PGProp *p = a && index >= 0 ? BLI_findlink(&a->props, index) : NULL;
  if (!p || !isfinite(value)) return 0;
  p->value = clamp_f(value, p->min, p->max);
  return 1;
}

int pg_arm_prop_get(const PGArmature *a, int index, float r[5])
{
  PGProp *p = a && index >= 0 ? BLI_findlink(&a->props, index) : NULL;
  if (!p) return 0;
  r[0] = p->bone[0] ? (float)pg_arm_bone_find(a, p->bone) : -1.0f;
  r[1] = p->value;
  r[2] = p->min;
  r[3] = p->max;
  r[4] = p->def;
  return 1;
}

const char *pg_arm_prop_name(const PGArmature *a, int index)
{
  PGProp *p = a && index >= 0 ? BLI_findlink(&a->props, index) : NULL;
  return p ? p->name : NULL;
}

/* ---------------------------------------------------------------------------------------- */
/* drivers                                                                                   */

static FCurve *driver_at(const PGArmature *a, int driver)
{
  return a && driver >= 0 ? BLI_findlink(&a->drivers, driver) : NULL;
}

static DriverVar *var_at(FCurve *fcu, int var)
{
  return fcu && var >= 0 ? BLI_findlink(&fcu->driver->variables, var) : NULL;
}

/* An edit makes the driver valid again (rna_DriverTarget_update_data / rna_Driver_update_data). */
static void driver_touched(FCurve *fcu, bool expr_changed, bool varname_changed)
{
  fcu->driver->flag &= ~DRIVER_FLAG_INVALID;
  BKE_driver_invalidate_expression(fcu->driver, expr_changed, varname_changed);
}

int pg_arm_driver_count(const PGArmature *a) { return a ? BLI_listbase_count(&a->drivers) : 0; }

int pg_arm_driver_find(const PGArmature *a, const char *path, int index)
{
  if (!a || !path) return -1;
  int i = 0;
  LISTBASE_FOREACH (const FCurve *, fcu, &a->drivers) {
    if (fcu->rna_path && STREQ(fcu->rna_path, path) && fcu->array_index == index) return i;
    i++;
  }
  return -1;
}

int pg_arm_driver_add(PGArmature *a, const char *path, int index, int type, bool generator)
{
  PGPath p;
  if (!a || !path_parse(path, &p, false) || index < 0 || type < DRIVER_TYPE_AVERAGE ||
      type > DRIVER_TYPE_MAX)
  {
    return -1;
  }
  int found = pg_arm_driver_find(a, path, index);
  if (found >= 0) return found; /* verify_driver_fcurve */
  FCurve *fcu = pg_drv_new_fcurve(path, index);
  fcu->driver->type = type;
  if (generator) {
    add_fmodifier_generator(&fcu->modifiers, fcu);
  }
  else {
    /* add 2 keyframes so that user has something to work with
     * - These are configured to 0,0 and 1,1 to give a 1-1 mapping
     *   which can be easily tweaked from there. */
    pg_bke_insert_vert_fcurve(fcu, 0.0f, 0.0f);
    pg_bke_insert_vert_fcurve(fcu, 1.0f, 1.0f);
    fcu->extend = FCURVE_EXTRAPOLATE_LINEAR;
    pg_bke_fcurve_handles_recalc(fcu);
  }
  BLI_addtail(&a->drivers, fcu);
  return BLI_findindex(&a->drivers, fcu);
}

int pg_arm_driver_remove(PGArmature *a, int driver)
{
  FCurve *fcu = driver_at(a, driver);
  if (!fcu) return 0;
  BLI_remlink(&a->drivers, fcu);
  pg_drv_fcurve_free(fcu);
  return 1;
}

int pg_arm_driver_set_type(PGArmature *a, int driver, int type)
{
  FCurve *fcu = driver_at(a, driver);
  if (!fcu || type < DRIVER_TYPE_AVERAGE || type > DRIVER_TYPE_MAX) return 0;
  fcu->driver->type = type;
  driver_touched(fcu, true, false);
  return 1;
}

int pg_arm_driver_set_expression(PGArmature *a, int driver, const char *expression)
{
  FCurve *fcu = driver_at(a, driver);
  if (!fcu || !expression) return 0;
  STRNCPY(fcu->driver->expression, expression);
  driver_touched(fcu, true, false);
  return 1;
}

static void var_set_name(FCurve *fcu, DriverVar *dvar, const char *name)
{
  STRNCPY(dvar->name, name && name[0] ? name : "var");
  BLI_uniquename(&fcu->driver->variables, dvar, dvar->name, '_', offsetof(DriverVar, name),
                 sizeof(dvar->name));
  driver_variable_name_validate(dvar);
}

int pg_arm_driver_var_add(PGArmature *a, int driver, const char *name, int type)
{
  FCurve *fcu = driver_at(a, driver);
  if (!fcu || !get_dvar_typeinfo(type)) return -1;
  /* driver_add_new_variable */
  DriverVar *dvar = MEM_callocN(sizeof(DriverVar), "DriverVar");
  BLI_addtail(&fcu->driver->variables, dvar);
  var_set_name(fcu, dvar, name);
  driver_change_variable_type(dvar, type);
  dvar->targets[0].id = &a->ob.id; /* the rig object (ID_OB) */
  driver_touched(fcu, false, true);
  return BLI_findindex(&fcu->driver->variables, dvar);
}

int pg_arm_driver_var_remove(PGArmature *a, int driver, int var)
{
  FCurve *fcu = driver_at(a, driver);
  DriverVar *dvar = var_at(fcu, var);
  if (!dvar) return 0;
  driver_free_variable_ex(fcu->driver, dvar);
  driver_touched(fcu, false, true);
  return 1;
}

int pg_arm_driver_var_rename(PGArmature *a, int driver, int var, const char *name)
{
  FCurve *fcu = driver_at(a, driver);
  DriverVar *dvar = var_at(fcu, var);
  if (!dvar || !name) return 0;
  var_set_name(fcu, dvar, name);
  driver_touched(fcu, false, true);
  return 1;
}

int pg_arm_driver_var_set_path(PGArmature *a, int driver, int var, const char *path)
{
  FCurve *fcu = driver_at(a, driver);
  DriverVar *dvar = var_at(fcu, var);
  if (!dvar || !path || dvar->type != DVAR_TYPE_SINGLE_PROP) return 0;
  DriverTarget *dtar = &dvar->targets[0];
  MEM_SAFE_FREE(dtar->rna_path);
  dtar->rna_path = BLI_strdup(path);
  driver_touched(fcu, false, false);
  return 1;
}

int pg_arm_driver_var_set_transform(PGArmature *a, int driver, int var, const char *bone,
                                    int trans_chan, int rotation_mode, int space_flags)
{
  FCurve *fcu = driver_at(a, driver);
  DriverVar *dvar = var_at(fcu, var);
  if (!dvar || dvar->type != DVAR_TYPE_TRANSFORM_CHAN || trans_chan < 0 ||
      trans_chan >= MAX_DTAR_TRANSCHAN_TYPES || rotation_mode < DTAR_ROTMODE_AUTO ||
      rotation_mode > DTAR_ROTMODE_SWING_TWIST_Z)
  {
    return 0;
  }
  DriverTarget *dtar = &dvar->targets[0];
  STRNCPY(dtar->pchan_name, bone ? bone : "");
  dtar->transChan = (short)trans_chan;
  dtar->rotation_mode = (char)rotation_mode;
  dtar->flag = (short)((dtar->flag & ~(DTAR_FLAG_LOCALSPACE | DTAR_FLAG_LOCAL_CONSTS)) |
                       (space_flags & (DTAR_FLAG_LOCALSPACE | DTAR_FLAG_LOCAL_CONSTS)));
  driver_touched(fcu, false, false);
  return 1;
}

int pg_arm_driver_set_keys(PGArmature *a, int driver, const float *xyi, int n, int extend)
{
  FCurve *fcu = driver_at(a, driver);
  if (!fcu || n < 0 || (n > 0 && !xyi)) return 0;
  for (int i = 0; i < n; i++) {
    if (!isfinite(xyi[i * 3]) || !isfinite(xyi[i * 3 + 1])) return 0;
    if (i > 0 && xyi[i * 3] <= xyi[(i - 1) * 3]) return 0; /* increasing x */
  }
  MEM_SAFE_FREE(fcu->bezt);
  fcu->totvert = 0;
  for (int i = 0; i < n; i++) {
    const int k = pg_bke_insert_vert_fcurve(fcu, xyi[i * 3], xyi[i * 3 + 1]);
    if (k >= 0) fcu->bezt[k].ipo = (char)clamp_i((int)xyi[i * 3 + 2], BEZT_IPO_CONST, BEZT_IPO_BEZ);
  }
  fcu->extend = extend ? FCURVE_EXTRAPOLATE_LINEAR : FCURVE_EXTRAPOLATE_CONSTANT;
  pg_bke_fcurve_handles_recalc(fcu);
  return 1;
}

int pg_arm_driver_set_generator(PGArmature *a, int driver, int mode, bool additive,
                                const float *coefficients, int n)
{
  FCurve *fcu = driver_at(a, driver);
  if (!fcu || n < 0 || (n > 0 && !coefficients) ||
      !ELEM(mode, FCM_GENERATOR_POLYNOMIAL, FCM_GENERATOR_POLYNOMIAL_FACTORISED) ||
      (mode == FCM_GENERATOR_POLYNOMIAL_FACTORISED && (n % 2) != 0) || n > 200)
  {
    return 0;
  }
  FModifier *fcm = NULL;
  LISTBASE_FOREACH (FModifier *, m, &fcu->modifiers) {
    if (m->type == FMODIFIER_TYPE_GENERATOR) { fcm = m; break; }
  }
  if (n == 0) {
    if (fcm) {
      fcm_generator_free(fcm);
      MEM_freeN(fcm->data);
      BLI_freelinkN(&fcu->modifiers, fcm);
    }
    return 1;
  }
  if (!fcm) fcm = add_fmodifier_generator(&fcu->modifiers, fcu);
  FMod_Generator *data = fcm->data;
  data->mode = mode;
  data->poly_order = mode == FCM_GENERATOR_POLYNOMIAL ? n - 1 : n / 2;
  SET_FLAG_FROM_TEST(data->flag, additive, FCM_GENERATOR_ADDITIVE);
  fcm_generator_verify(fcm);
  if (data->arraysize < (unsigned)n) return 0;
  memcpy(data->coefficients, coefficients, sizeof(float) * n);
  return 1;
}

int pg_arm_driver_get(const PGArmature *a, int driver, float *r, int cap)
{
  FCurve *fcu = driver_at(a, driver);
  if (!fcu) return 0;
  const int nvars = BLI_listbase_count(&fcu->driver->variables);
  const int n = 8 + nvars * 3;
  if (!r || cap < n) return n;
  bool gen = false;
  LISTBASE_FOREACH (FModifier *, m, &fcu->modifiers) gen |= m->type == FMODIFIER_TYPE_GENERATOR;
  r[0] = fcu->array_index;
  r[1] = fcu->driver->type;
  r[2] = fcu->driver->flag;
  r[3] = fcu->driver->curval;
  r[4] = fcu->curval;
  r[5] = nvars;
  r[6] = fcu->totvert;
  r[7] = gen ? 1.0f : 0.0f;
  int i = 8;
  LISTBASE_FOREACH (DriverVar *, dvar, &fcu->driver->variables) {
    r[i++] = dvar->type;
    r[i++] = dvar->curval;
    r[i++] = dvar->targets[0].flag;
  }
  return n;
}

const char *pg_arm_driver_path(const PGArmature *a, int driver)
{
  FCurve *fcu = driver_at(a, driver);
  return fcu ? fcu->rna_path : NULL;
}

const char *pg_arm_driver_expression(const PGArmature *a, int driver)
{
  FCurve *fcu = driver_at(a, driver);
  return fcu ? fcu->driver->expression : NULL;
}
