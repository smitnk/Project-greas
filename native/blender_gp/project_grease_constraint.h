/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bone constraints of the rig (project_grease_constraint.c, Blender 3.6.23 constraint.c and the
 * Standard IK solver). Each pose channel holds a real bConstraint stack (pchan->constraints),
 * evaluated by BKE_pose_where_is: per bone the stack (BKE_constraints_solve), IK chains by the
 * IK tree. Targets are the rig object ("" subtarget) or one of its bones. Bones are indices, as in
 * project_grease_armature.h; constraints are indices in the bone's stack. */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct ListBase;
typedef struct PGArmature PGArmature;

/* BKE_pose_where_is for the rig object (ob->data the bArmature, ob->pose the bPose). */
void pg_bke_pose_where_is(struct Object *ob);
/* BKE_constraints_free / BKE_constraints_copy (targets are remapped to `owner` when set). */
void pg_con_list_free(struct ListBase *list);
void pg_con_list_copy(struct ListBase *dst, const struct ListBase *src, struct Object *owner);

/* Settings exchanged by pg_arm_con_set / pg_arm_con_get, Blender's own values and bit flags:
 * [0] influence (enforce 0..1), [1] owner space, [2] target space (CONSTRAINT_SPACE_WORLD 0 /
 * POSE 2 / LOCAL 1 / PARLOCAL 3 / OWNLOCAL 6), [3] head/tail (0..1), [4] mute (CONSTRAINT_OFF),
 * then per type from [5]:
 *  KINEMATIC (3): flag (CONSTRAINT_IK_TIP use tail, _STRETCH, _ROT, _POS ...), chain count
 *                 (rootbone 0..255, 0 = whole chain), iterations (0..10000), pole angle (rad),
 *                 weight (0.01..1), orient weight (0.01..1)
 *  ROTLIKE (8):   flag (ROTLIKE_X/Y/Z and _X/Y/Z_INVERT), euler order, mix mode (ROTLIKE_MIX_*)
 *  LOCLIKE (9):   flag (LOCLIKE_X/Y/Z, _X/Y/Z_INVERT, LOCLIKE_OFFSET)
 *  DAMPTRACK (21): track axis (TRACK_X 0 .. TRACK_NZ 5)
 *  ROTLIMIT (5):  flag (LIMIT_XROT/YROT/ZROT), flag2 (LIMIT_TRANSFORM: affect transform),
 *                 xmin, xmax, ymin, ymax, zmin, zmax (rad), euler order
 *  CHILDOF (1):   flag (CHILDOF_LOCX .. CHILDOF_SIZEZ, CHILDOF_SET_INVERSE); get also returns the
 *                 inverse matrix (16, column major) */
#define PG_CON_COMMON 5
#define PG_CON_MAX_VALUES 40

/* BKE_constraint_add_for_pose (named after the type when name is empty, made unique), without a
 * target. Returns the index or -1. */
int pg_arm_con_add(PGArmature *a, int bone, int type, const char *name);
int pg_arm_con_remove(PGArmature *a, int bone, int index);
int pg_arm_con_count(const PGArmature *a, int bone);
/* CONSTRAINT_OT_move_to_index */
int pg_arm_con_move(PGArmature *a, int bone, int index, int to);
/* which 0: target, 1: IK pole target. subtarget "" = the rig object, NULL clears the target. */
int pg_arm_con_set_target(PGArmature *a, int bone, int index, int which, const char *subtarget);
int pg_arm_con_set(PGArmature *a, int bone, int index, const float *v, int n);
/* Returns the number of values (type at r[0], then the layout above shifted by one). */
int pg_arm_con_get(const PGArmature *a, int bone, int index, float *r, int cap);
/* Whether target slot `which` exists; *r_sub is its subtarget, NULL when no target is set. */
bool pg_con_target_get(const PGArmature *a, int bone, int index, int which, const char **r_sub);
/* CONSTRAINT_OT_childof_set_inverse / _clear_inverse. */
int pg_arm_con_childof_inverse(PGArmature *a, int bone, int index, bool set);
/* Subtargets follow a bone rename (ED_armature_bone_rename). */
void pg_con_rename_subtargets(PGArmature *a, const char *oldname, const char *newname);
/* The constraint struct, NULL when out of range. */
struct bConstraint *pg_arm_con_at(const PGArmature *a, int bone, int index);

#ifdef __cplusplus
}
#endif
