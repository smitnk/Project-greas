/* SPDX-License-Identifier: GPL-2.0-or-later
 * Entry points of project_grease_armature_bke.c (Blender 3.6.23 armature code, verbatim + adapted).
 * Everything works on real DNA: bArmature with an EditBone list (arm->edbo) as the editable rest
 * pose, the Bone tree rebuilt from it, and a bPose. */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bArmature;
struct bPose;
struct bPoseChannel;
struct bGPdata;
struct bGPDframe;
struct bGPDlayer;
struct bGPDstroke;
struct EditBone;
struct FCurve;
struct ListBase;

/* ED_armature_from_edit + BKE_pose_rebuild: rebuilds arm->bonebase (rest matrices) from arm->edbo
 * and makes `pose` hold one channel per bone in hierarchy order, keeping existing channels' values. */
void pg_bke_armature_from_edit(struct bArmature *arm, struct bPose *pose);
/* BKE_pose_where_is (with constraints and IK) is pg_bke_pose_where_is in project_grease_constraint.h. */
void pg_bke_armature_bonelist_free(struct ListBase *lb);
void pg_bke_pose_free(struct bPose *pose);
/* BKE_pose_eval_bbone_segments for every channel (B-Bone segment cache of the current pose). */
void pg_bke_pose_bbone_segments(struct bPose *pose);
struct bPoseChannel *pg_bke_pose_channel_ensure(struct bPose *pose, const char *name);
/* pose_bone_do_paste (pose_transform.c) without ID properties. */
struct bPoseChannel *pg_bke_pose_bone_do_paste(struct bPose *pose, struct bPoseChannel *chan,
                                               bool selOnly, bool flip);
void pg_bke_pchan_clear_loc(struct bPoseChannel *pchan);
void pg_bke_pchan_clear_rot(struct bPoseChannel *pchan);
void pg_bke_pchan_clear_scale(struct bPoseChannel *pchan);
/* BKE_armature_deform_coords_with_gpencil_stroke: target_to_world maps the stroke's coordinates
 * into armature space (the armature object sits at the origin). */
void pg_bke_armature_deform_gpencil_stroke(const struct bArmature *arm, const struct bPose *pose,
                                           const struct ListBase *defbase,
                                           const float target_to_world[4][4],
                                           float (*vert_coords)[3], int vert_coords_len,
                                           int deformflag, const char *defgrp_name,
                                           struct bGPDstroke *gps_target);
/* gpencil_object_vgroup_calc_from_armature for one layer: mode GP_ARMATURE_NAME (0) creates the
 * empty groups, GP_ARMATURE_AUTO (1) also fills them (ratio / decay as DEFAULT_RATIO 0.1 /
 * DEFAULT_DECAY 0.8) in `actframe`, and with is_multiedit in every GP_FRAME_SELECT frame of `gpl`.
 * Returns the number of groups added. */
int pg_bke_gpencil_vgroup_calc_from_armature(struct bGPdata *gpd, struct bGPDlayer *gpl,
                                             struct bGPDframe *actframe, bool is_multiedit,
                                             const struct bArmature *arm, const struct bPose *pose,
                                             const float target_to_world[4][4], int mode,
                                             float ratio, float decay);
/* fcurve_eval_keyframes (no modifiers / drivers). */
float pg_bke_fcurve_eval_keyframes(struct FCurve *fcu, float evaltime);
/* ED_armature_ebone_add, ED_armature_ebone_unique_name, ED_armature_ebone_remove (verbatim). */
struct EditBone *pg_bke_ebone_add(struct bArmature *arm, const char *name);
void pg_bke_ebone_unique_name(struct ListBase *ebones, char *name, struct EditBone *bone);
void pg_bke_ebone_remove(struct bArmature *arm, struct EditBone *ebone);
/* bone_connect_to_new_parent / bone_connect_to_existing_parent / editbone_clear_parent. */
void pg_bke_bone_connect_to_new_parent(struct ListBase *edbo, struct EditBone *selbone,
                                       struct EditBone *actbone, short mode);
void pg_bke_bone_connect_to_existing_parent(struct EditBone *bone);
void pg_bke_editbone_clear_parent(struct EditBone *ebone, int mode);
/* insert_vert_fcurve (keyframing.c) with INSERTKEY_NO_USERPREF: Bezier, Auto Clamped handles. */
int pg_bke_insert_vert_fcurve(struct FCurve *fcu, float x, float y);
void pg_bke_fcurve_handles_recalc(struct FCurve *fcu);
void pg_bke_edit_sync_selection(struct ListBase *edbo);
float pg_bke_ebone_roll_to_vector(const struct EditBone *bone, const float align_axis[3], bool axis_only);
void pg_bke_ebone_to_mat3(struct EditBone *ebone, float r_mat[3][3]);
float pg_bke_distfactor_to_bone(const float vec[3], const float b1[3], const float b2[3], float rad1,
                                float rad2, float rdist);

#define PG_ARM_PAR_CONNECT 1 /* ARM_PAR_CONNECT */
#define PG_ARM_PAR_OFFSET 2  /* ARM_PAR_OFFSET */
#define PG_ARM_PAR_CLEAR 1   /* ARM_PAR_CLEAR */
#define PG_ARM_PAR_CLEAR_DISCONNECT 2

#ifdef __cplusplus
}
#endif
