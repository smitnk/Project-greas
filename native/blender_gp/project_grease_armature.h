/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Grease Pencil armature rig for Project Grease (Blender 3.6.23 armature / pose / action / armature
 * modifier / "Parent with Automatic Weights", see project_grease_armature_bke.c for the ported code).
 *
 * One PGArmature per document. It holds real DNA: a bArmature whose EditBone list (arm.edbo) is the
 * editable rest pose (head / tail / roll / parent / connected, in armature space), the Bone tree and
 * rest matrices rebuilt from it after every edit (ED_armature_from_edit + BKE_armature_where_is), a
 * bPose with one bPoseChannel per bone (loc / quaternion / scale, BKE_pose_where_is), and the bone
 * F-Curves of its action (rna paths pose.bones["<name>"].location / rotation_quaternion / scale).
 *
 * Space: armature space is Blender units in the canvas plane, x right, y up, z towards the viewer:
 * a canvas point (px, py) is (px, -py, 0) / PG_ARM_PIXELS_PER_UNIT (the same 1000 px per unit as the
 * modifier stack). Bone envelope defaults (dist 0.25, radius 0.1 / 0.05) are therefore 250 / 100 /
 * 50 canvas pixels, as in a Blender scene where the drawing is 1 unit wide per 1000 px.
 * Pose rotation from the canvas is about the view axis (+z), so a bone lying in the canvas plane
 * with roll 0 turns about its own local z axis.
 *
 * Bones are addressed by their index in arm.edbo (creation order; deleting renumbers).
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDframe;
struct bGPDstroke;
struct FCurve;

typedef struct PGArmature PGArmature;

#define PG_ARM_PIXELS_PER_UNIT 1000.0f
enum { PG_ARM_MODE_OBJECT = 0, PG_ARM_MODE_EDIT = 1, PG_ARM_MODE_POSE = 2 };
/* Channels keyed by Insert Keyframe (LocRotScale): location[0..2], rotation_quaternion[0..3],
 * scale[0..2]. */
#define PG_ARM_CHANNELS 10

PGArmature *pg_arm_new(void);
PGArmature *pg_arm_dup(const PGArmature *src);
void pg_arm_free(PGArmature *a);
int pg_arm_mode(const PGArmature *a);
void pg_arm_set_mode(PGArmature *a, int mode);
/* canvas px <-> armature space (see above) */
void pg_arm_canvas_to_arm(const float px[3], float r_arm[3]);
void pg_arm_arm_to_canvas(const float arm[3], float r_px[3]);
/* The 4x4 matrix of the canvas -> armature mapping (the GP object's matrix seen by the rig). */
void pg_arm_canvas_matrix(float r_mat[4][4]);

/* ---- bones (rest pose) ---- */
int pg_arm_bone_count(const PGArmature *a);
const char *pg_arm_bone_name(const PGArmature *a, int bone);
int pg_arm_bone_find(const PGArmature *a, const char *name);
int pg_arm_active_bone(const PGArmature *a);
/* r: head[3], tail[3], roll, parent index (-1), flag (BONE_*), length, rest arm_mat[16],
 * pose head[3], pose tail[3], pose_mat[16]: PG_ARM_BONE_INFO floats. */
#define PG_ARM_BONE_INFO 48
int pg_arm_bone_info(const PGArmature *a, int bone, float *r);
/* envelope: dist, weight, rad_head, rad_tail; deform 0 sets BONE_NO_DEFORM, mult_vg BONE_MULT_VG_ENV */
int pg_arm_bone_set_envelope(PGArmature *a, int bone, float dist, float weight, float rad_head,
                             float rad_tail, bool deform, bool mult_vg);
int pg_arm_bone_envelope(const PGArmature *a, int bone, float r[4]);

/* ---- edit mode (ARMATURE_OT_*) ---- */
/* bone_primitive_add: deselects all, adds a bone named `name` ("Bone" if empty, made unique),
 * selects it and makes it active. Returns its index. */
int pg_arm_bone_add(PGArmature *a, const char *name, const float head[3], const float tail[3]);
/* select: flags BONE_SELECTED (both ends) / BONE_ROOTSEL / BONE_TIPSEL; extend keeps the others.
 * bone -1 with extend false deselects all. Makes the bone active. */
int pg_arm_edit_select(PGArmature *a, int bone, int flags, bool extend);
/* armature_extrude_exec (forked false) on the selection, then moves the new tips by delta (the
 * translate that follows E). Returns the number of new bones; the last one is active. */
int pg_arm_extrude(PGArmature *a, const float delta[3]);
/* Transform of the selected heads / tails (TFM_TRANSLATION / TFM_ROTATION about +z / TFM_RESIZE)
 * with recalcData_edit_armature: connected heads follow, envelope radii scale with the length,
 * roll is kept relative to the bone (the "fix roll" pass). */
int pg_arm_edit_translate(PGArmature *a, const float delta[3]);
int pg_arm_edit_rotate(PGArmature *a, float angle, const float pivot[3]);
int pg_arm_edit_scale(PGArmature *a, float factor, const float pivot[3]);
/* Direct values (the N panel): head, tail and roll of one bone. Connected children follow. */
int pg_arm_bone_set(PGArmature *a, int bone, const float head[3], const float tail[3], float roll);
/* ARMATURE_OT_parent_set: selected bones get the active bone as parent, mode PG_ARM_PAR_CONNECT /
 * PG_ARM_PAR_OFFSET (keep offset). */
int pg_arm_parent_set(PGArmature *a, int mode);
/* ARMATURE_OT_parent_clear on the selected bones: PG_ARM_PAR_CLEAR / _CLEAR_DISCONNECT. */
int pg_arm_parent_clear(PGArmature *a, int mode);
/* ARMATURE_OT_delete: removes the selected bones (children move to the removed bone's parent). */
int pg_arm_delete_selected(PGArmature *a);
/* ED_armature_bone_rename: unique name (BLI_uniquename_cb "Bone", '.'); the pose channel and the
 * bone's F-Curve paths follow. r_old / r_new (64 bytes) receive the names for renaming the vertex
 * group of the same name. Returns 1 when renamed. */
int pg_arm_bone_rename(PGArmature *a, int bone, const char *name, char *r_old, char *r_new);

/* ---- pose mode ---- */
int pg_arm_pose_select(PGArmature *a, int bone, bool extend);
bool pg_arm_bone_selected(const PGArmature *a, int bone);
/* loc[3], quat[4], size[3] (bone space, as bPoseChannel stores them) */
int pg_arm_pose_get(const PGArmature *a, int bone, float r[10]);
int pg_arm_pose_set(PGArmature *a, int bone, const float v[10]);
/* Transform of the selected channels in armature space (children of selected bones follow their
 * parent and are not transformed themselves): translate the head (unconnected bones), rotate about
 * +z around each bone's head, scale its local axes. */
int pg_arm_pose_translate(PGArmature *a, const float delta[3]);
int pg_arm_pose_rotate(PGArmature *a, float angle);
int pg_arm_pose_scale(PGArmature *a, float factor);
/* POSE_OT_loc_clear / rot_clear / scale_clear on the selected channels: what = 1 loc, 2 rot, 4 scale */
int pg_arm_pose_clear(PGArmature *a, int what);
/* POSE_OT_copy (selected bones) / POSE_OT_paste (flipped: BLI_string_flip_side_name, x mirror) */
int pg_arm_pose_copy(PGArmature *a);
int pg_arm_pose_paste(PGArmature *a, bool flipped, bool selected_mask);

/* ---- action ---- */
/* Insert Keyframe, keying set LocRotScale, on the selected channels at `frame` (insert_vert_fcurve:
 * Bezier, Auto Clamped, Continuous Acceleration smoothing). Returns the number of keys written. */
int pg_arm_key_insert(PGArmature *a, float frame);
/* Deletes the keys of `bone` at `frame` (all its channels). */
int pg_arm_key_delete(PGArmature *a, int bone, float frame);
/* Interpolation of the keys of `bone` at `frame`: BEZT_IPO_CONST 0 / LIN 1 / BEZ 2 */
int pg_arm_key_set_interpolation(PGArmature *a, int bone, float frame, int ipo);
/* Sorted distinct key frames of `bone` (all its channels); returns the count (may exceed cap). */
int pg_arm_bone_keys(const PGArmature *a, int bone, float *r_frames, int cap);
/* Interpolation of the first key of `bone` found at `frame`, or -1. */
int pg_arm_key_interpolation(const PGArmature *a, int bone, float frame);
/* F-Curve of a bone channel (0..PG_ARM_CHANNELS-1), NULL when not keyed. */
struct FCurve *pg_arm_fcurve(const PGArmature *a, int bone, int channel);
int pg_arm_fcurve_count(const PGArmature *a);
struct FCurve *pg_arm_fcurve_at(const PGArmature *a, int index);
/* Animation evaluation at `frame`: every keyed channel takes its F-Curve value, then the pose is
 * evaluated (BKE_pose_where_is). */
void pg_arm_evaluate(PGArmature *a, float frame);
/* Re-evaluates the pose matrices only (after setting values). */
void pg_arm_update(PGArmature *a);

/* ---- armature modifier / bind ---- */
/* MOD_gpencil_legacy_armature deformStroke on one stroke whose points are canvas px, with the
 * Armature modifier's deformflag (ARM_DEF_VGROUP / ARM_DEF_ENVELOPE / ARM_DEF_QUATERNION /
 * ARM_DEF_INVERT_VGROUP) and vertex group name (NULL / "" none). Nothing happens in edit mode
 * (Blender skips armatures in edit mode). Returns 1 when the stroke was deformed. */
int pg_arm_deform_stroke(const PGArmature *a, const struct bGPdata *gpd, struct bGPDstroke *gps,
                         int deformflag, const char *vgname);
/* ED_gpencil_add_armature_weights on frame gpf: mode 0 Armature (nothing), 1 With Empty Groups,
 * 2 With Automatic Weights, 3 With Envelope Weights (Blender 3.6 handles it like Automatic for GP).
 * Returns the number of vertex groups added, or -1. */
int pg_arm_bind_frame(const PGArmature *a, struct bGPdata *gpd, struct bGPDframe *gpf, int mode);

/* ---- viewport display ---- */
/* bArmature.drawtype (ARM_OCTA 0 default / ARM_LINE 1 stick / B-Bone 2 / Envelope 3 / Wire 4; only
 * Octahedral and Stick are drawn, the others draw as Octahedral) and the rig object's In Front
 * (Object.dtx OB_DRAW_IN_FRONT, DNA default off). Both are saved with the rig. */
int pg_arm_display_type(const PGArmature *a);
bool pg_arm_show_in_front(const PGArmature *a);
void pg_arm_set_display(PGArmature *a, int drawtype, bool in_front);
/* Click select of a bone (ed_armature_pick_bone / ED_armature_edit_select_pick_bone, VIEW3D_SELECT_
 * PICK_NEAREST): the bone nearest to mval within dist (window units of arm_to_win, a homogeneous
 * armature space -> window matrix; ED_view3d_select_dist_px is 75 px * U.pixelsize). Pose mode
 * selects the bone (pose head .. tail); edit mode the head / tail joint or the whole bone. A miss
 * deselects all unless extend. Object mode does nothing. Returns the bone or -1. */
int pg_arm_pick(PGArmature *a, const float arm_to_win[4][4], const float mval[2], float dist,
                bool extend);

/* Project document form (project_grease_armature_io.cc): a JSON object with the edit bones, the pose
 * channel values, the F-Curves and the mode. pg_arm_to_json returns a malloc'd string (free()). */
char *pg_arm_to_json(const PGArmature *a);
/* NULL when `json` is not an armature object. */
PGArmature *pg_arm_from_json(const char *json);

/* Document registry for the modifier stack: the armature of `gpd` (NULL to unregister). */
void pg_arm_register(const struct bGPdata *gpd, const PGArmature *a);
const PGArmature *pg_arm_for_gpd(const struct bGPdata *gpd);

#ifdef __cplusplus
}
#endif
