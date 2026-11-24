/* SPDX-License-Identifier: GPL-2.0-or-later
 * Project Grease rig state and operators (see project_grease_armature.h). The Blender code they call
 * is in project_grease_armature_bke.c; the operator bodies here follow the named Blender 3.6.23
 * operators with the context (bContext, RNA, notifiers, X-mirror editing) removed. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"

#include "project_grease_armature.h"
#include "project_grease_armature_bke.h"
#include "project_grease_armature_intern.h"


static const char *const channel_prop[PG_ARM_CHANNELS] = {
    "location", "location", "location", "rotation_quaternion", "rotation_quaternion",
    "rotation_quaternion", "rotation_quaternion", "scale", "scale", "scale"};
static const int channel_index[PG_ARM_CHANNELS] = {0, 1, 2, 0, 1, 2, 3, 0, 1, 2};

/* ---------------------------------------------------------------------------------------- */
/* lifetime                                                                                  */

static void arm_update(PGArmature *a)
{
  pg_bke_armature_from_edit(&a->arm, &a->pose);
  pg_bke_pose_where_is(&a->arm, &a->pose);
}

PGArmature *pg_arm_new(void)
{
  PGArmature *a = MEM_callocN(sizeof(PGArmature), "PGArmature");
  /* _DNA_DEFAULT_bArmature */
  a->arm.deformflag = ARM_DEF_VGROUP | ARM_DEF_ENVELOPE;
  a->arm.flag = ARM_COL_CUSTOM;
  a->arm.layer = 1;
  a->arm.drawtype = ARM_OCTA;
  a->arm.edbo = &a->edbo;
  return a;
}

static void fcurve_free(FCurve *fcu)
{
  MEM_SAFE_FREE(fcu->bezt);
  MEM_SAFE_FREE(fcu->rna_path);
  MEM_freeN(fcu);
}

void pg_arm_free(PGArmature *a)
{
  if (!a) return;
  BLI_freelistN(&a->edbo);
  pg_bke_armature_bonelist_free(&a->arm.bonebase);
  pg_bke_pose_free(&a->pose);
  BLI_freelistN(&a->copybuf);
  LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &a->curves) fcurve_free(fcu);
  MEM_freeN(a);
}

PGArmature *pg_arm_dup(const PGArmature *src)
{
  PGArmature *a = pg_arm_new();
  a->arm.flag = src->arm.flag;
  a->arm.deformflag = src->arm.deformflag;
  a->arm.layer = src->arm.layer;
  a->mode = src->mode;
  a->arm.drawtype = src->arm.drawtype;
  a->show_in_front = src->show_in_front;
  /* edit bones: copy, then remap the parent / active pointers by index */
  LISTBASE_FOREACH (const EditBone *, eb, &src->edbo) {
    EditBone *n = MEM_dupallocN(eb);
    n->next = n->prev = NULL;
    BLI_addtail(&a->edbo, n);
  }
  EditBone *n = a->edbo.first;
  LISTBASE_FOREACH (const EditBone *, eb, &src->edbo) {
    n->parent = eb->parent ? BLI_findlink(&a->edbo, BLI_findindex(&src->edbo, eb->parent)) : NULL;
    n->bbone_prev = n->bbone_next = n->bbone_child = NULL;
    if (eb == src->arm.act_edbone) a->arm.act_edbone = n;
    n = n->next;
  }
  LISTBASE_FOREACH (const bPoseChannel *, pc, &src->pose.chanbase) {
    bPoseChannel *c = MEM_dupallocN(pc);
    c->next = c->prev = NULL;
    c->bone = NULL;
    c->parent = c->child = NULL;
    BLI_addtail(&a->pose.chanbase, c);
  }
  LISTBASE_FOREACH (const bPoseChannel *, pc, &src->copybuf) {
    bPoseChannel *c = MEM_dupallocN(pc);
    c->next = c->prev = NULL;
    c->bone = NULL;
    c->parent = c->child = NULL;
    BLI_addtail(&a->copybuf, c);
  }
  LISTBASE_FOREACH (const FCurve *, fcu, &src->curves) {
    FCurve *c = MEM_dupallocN(fcu);
    c->next = c->prev = NULL;
    c->bezt = fcu->bezt ? MEM_dupallocN(fcu->bezt) : NULL;
    c->rna_path = fcu->rna_path ? BLI_strdup(fcu->rna_path) : NULL;
    BLI_addtail(&a->curves, c);
  }
  arm_update(a);
  return a;
}

int pg_arm_mode(const PGArmature *a) { return a ? a->mode : 0; }

void pg_arm_set_mode(PGArmature *a, int mode)
{
  if (a && mode >= PG_ARM_MODE_OBJECT && mode <= PG_ARM_MODE_POSE) a->mode = mode;
}

void pg_arm_canvas_to_arm(const float px[3], float r_arm[3])
{
  r_arm[0] = px[0] / PG_ARM_PIXELS_PER_UNIT;
  r_arm[1] = -px[1] / PG_ARM_PIXELS_PER_UNIT;
  r_arm[2] = px[2] / PG_ARM_PIXELS_PER_UNIT;
}

void pg_arm_arm_to_canvas(const float arm[3], float r_px[3])
{
  r_px[0] = arm[0] * PG_ARM_PIXELS_PER_UNIT;
  r_px[1] = -arm[1] * PG_ARM_PIXELS_PER_UNIT;
  r_px[2] = arm[2] * PG_ARM_PIXELS_PER_UNIT;
}

void pg_arm_canvas_matrix(float r_mat[4][4])
{
  unit_m4(r_mat);
  r_mat[0][0] = 1.0f / PG_ARM_PIXELS_PER_UNIT;
  r_mat[1][1] = -1.0f / PG_ARM_PIXELS_PER_UNIT;
  r_mat[2][2] = 1.0f / PG_ARM_PIXELS_PER_UNIT;
}

/* ---------------------------------------------------------------------------------------- */
/* bones                                                                                     */

static EditBone *ebone_at(const PGArmature *a, int bone)
{
  return a && bone >= 0 ? BLI_findlink(&a->edbo, bone) : NULL;
}

static bPoseChannel *pchan_at(const PGArmature *a, int bone)
{
  const EditBone *eb = ebone_at(a, bone);
  return eb ? BKE_pose_channel_find_name(&a->pose, eb->name) : NULL;
}

int pg_arm_bone_count(const PGArmature *a) { return a ? BLI_listbase_count(&a->edbo) : 0; }

const char *pg_arm_bone_name(const PGArmature *a, int bone)
{
  const EditBone *eb = ebone_at(a, bone);
  return eb ? eb->name : NULL;
}

int pg_arm_bone_find(const PGArmature *a, const char *name)
{
  return a && name ? BLI_findstringindex(&a->edbo, name, offsetof(EditBone, name)) : -1;
}

int pg_arm_active_bone(const PGArmature *a)
{
  return a && a->arm.act_edbone ? BLI_findindex(&a->edbo, a->arm.act_edbone) : -1;
}

int pg_arm_bone_info(const PGArmature *a, int bone, float *r)
{
  const EditBone *eb = ebone_at(a, bone);
  if (!eb || !r) return 0;
  memset(r, 0, sizeof(float) * PG_ARM_BONE_INFO);
  copy_v3_v3(r, eb->head);
  copy_v3_v3(r + 3, eb->tail);
  r[6] = eb->roll;
  r[7] = eb->parent ? (float)BLI_findindex(&a->edbo, eb->parent) : -1.0f;
  r[8] = (float)eb->flag;
  r[9] = len_v3v3(eb->head, eb->tail);
  const bPoseChannel *pc = BKE_pose_channel_find_name(&a->pose, eb->name);
  if (pc && pc->bone) {
    memcpy(r + 10, pc->bone->arm_mat, sizeof(float[16]));
    copy_v3_v3(r + 26, pc->pose_head);
    copy_v3_v3(r + 29, pc->pose_tail);
    memcpy(r + 32, pc->pose_mat, sizeof(float[16]));
  }
  return 1;
}

int pg_arm_bone_set_envelope(PGArmature *a, int bone, float dist, float weight, float rad_head,
                             float rad_tail, bool deform, bool mult_vg)
{
  EditBone *eb = ebone_at(a, bone);
  if (!eb) return 0;
  /* rna_armature.c ranges: dist / radius 0..1000, weight 0..1000 */
  eb->dist = CLAMPIS(dist, 0.0f, 1000.0f);
  eb->weight = CLAMPIS(weight, 0.0f, 1000.0f);
  eb->rad_head = CLAMPIS(rad_head, 0.0f, 1000.0f);
  eb->rad_tail = CLAMPIS(rad_tail, 0.0f, 1000.0f);
  SET_FLAG_FROM_TEST(eb->flag, !deform, BONE_NO_DEFORM);
  SET_FLAG_FROM_TEST(eb->flag, mult_vg, BONE_MULT_VG_ENV);
  /* a connected child shares the head radius with its parent's tail (rna_EditBone_*_radius) */
  LISTBASE_FOREACH (EditBone *, c, &a->edbo) {
    if (c->parent == eb && (c->flag & BONE_CONNECTED)) c->rad_head = eb->rad_tail;
  }
  if (eb->parent && (eb->flag & BONE_CONNECTED)) eb->parent->rad_tail = eb->rad_head;
  arm_update(a);
  return 1;
}

int pg_arm_bone_envelope(const PGArmature *a, int bone, float r[4])
{
  const EditBone *eb = ebone_at(a, bone);
  if (!eb) return 0;
  r[0] = eb->dist;
  r[1] = eb->weight;
  r[2] = eb->rad_head;
  r[3] = eb->rad_tail;
  return 1;
}

/* ---------------------------------------------------------------------------------------- */
/* edit mode                                                                                 */

static void edit_deselect_all(PGArmature *a)
{
  /* ED_armature_edit_deselect_all */
  LISTBASE_FOREACH (EditBone *, eb, &a->edbo) {
    eb->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
  }
}

int pg_arm_bone_add(PGArmature *a, const char *name, const float head[3], const float tail[3])
{
  if (!a || !head || !tail) return -1;
  /* ARMATURE_OT_bone_primitive_add */
  edit_deselect_all(a);
  EditBone *bone = pg_bke_ebone_add(&a->arm, (name && name[0]) ? name : "Bone");
  a->arm.act_edbone = bone;
  copy_v3_v3(bone->head, head);
  copy_v3_v3(bone->tail, tail);
  bone->flag |= BONE_SELECTED | BONE_ROOTSEL; /* ED_armature_ebone_add sets BONE_TIPSEL */
  arm_update(a);
  return BLI_findindex(&a->edbo, bone);
}

int pg_arm_edit_select(PGArmature *a, int bone, int flags, bool extend)
{
  if (!a) return 0;
  if (!extend) edit_deselect_all(a);
  EditBone *eb = ebone_at(a, bone);
  if (!eb) return bone < 0;
  if (flags & BONE_SELECTED) flags |= BONE_ROOTSEL | BONE_TIPSEL;
  eb->flag |= flags & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);
  /* selecting a connected head selects the parent's tip (ED_armature_edit_select_pick_bone) */
  if ((flags & BONE_ROOTSEL) && eb->parent && (eb->flag & BONE_CONNECTED)) {
    eb->parent->flag |= BONE_TIPSEL;
  }
  pg_bke_edit_sync_selection(&a->edbo);
  a->arm.act_edbone = eb;
  arm_update(a);
  return 1;
}

/* One transformed point of createTransArmatureVerts. */
typedef struct EditTD {
  EditBone *ebo;
  float axismtx[3][3];
  float ival; /* roll before */
} EditTD;

typedef void (*PointFn)(float co[3], const float *arg);

static void fn_translate(float co[3], const float *arg) { add_v3_v3(co, arg); }
static void fn_rotate(float co[3], const float *arg)
{
  /* arg: angle, pivot[3] (rotation about +z) */
  float m[3][3], v[3];
  axis_angle_normalized_to_mat3(m, (const float[3]){0.0f, 0.0f, 1.0f}, arg[0]);
  sub_v3_v3v3(v, co, arg + 1);
  mul_m3_v3(m, v);
  add_v3_v3v3(co, v, arg + 1);
}
static void fn_scale(float co[3], const float *arg)
{
  float v[3];
  sub_v3_v3v3(v, co, arg + 1);
  mul_v3_fl(v, arg[0]);
  add_v3_v3v3(co, v, arg + 1);
}

/* createTransArmatureVerts + the transform + recalcData_edit_armature (transform_convert_armature.c)
 * for translation / rotation / resize, without X-mirror editing and snapping. */
static int edit_transform(PGArmature *a, PointFn fn, const float *arg)
{
  if (!a) return 0;
  const int n = BLI_listbase_count(&a->edbo);
  if (n == 0) return 0;
  EditTD *td = MEM_callocN(sizeof(EditTD) * n, __func__);
  int count = 0;
  LISTBASE_FOREACH (EditBone *, ebo, &a->edbo) {
    ebo->oldlength = ebo->length; /* length is kept up to date below */
    const bool root = (ebo->flag & BONE_ROOTSEL) && !(ebo->parent && (ebo->flag & BONE_CONNECTED));
    const bool tip = (ebo->flag & BONE_TIPSEL) != 0;
    if (!root && !tip) continue;
    pg_bke_ebone_to_mat3(ebo, td[count].axismtx);
    normalize_m3(td[count].axismtx);
    td[count].ebo = ebo;
    td[count].ival = ebo->roll;
    count++;
    if (root) fn(ebo->head, arg);
    if (tip) fn(ebo->tail, arg);
  }
  if (count == 0) {
    MEM_freeN(td);
    return 0;
  }
  LISTBASE_FOREACH (EditBone *, ebo, &a->edbo) {
    EditBone *ebo_parent = (ebo->flag & BONE_CONNECTED) ? ebo->parent : NULL;
    if (ebo_parent) {
      if (ebo_parent->flag & BONE_TIPSEL) {
        copy_v3_v3(ebo->head, ebo_parent->tail);
      }
      else {
        copy_v3_v3(ebo_parent->tail, ebo->head);
      }
    }
    /* on extrude bones, oldlength==0.0f, so we scale radius of points */
    ebo->length = len_v3v3(ebo->head, ebo->tail);
    if (ebo->oldlength == 0.0f) {
      ebo->rad_head = 0.25f * ebo->length;
      ebo->rad_tail = 0.10f * ebo->length;
      ebo->dist = 0.25f * ebo->length;
      if (ebo->parent) {
        if (ebo->rad_head > ebo->parent->rad_tail) {
          ebo->rad_head = ebo->parent->rad_tail;
        }
      }
    }
    else {
      ebo->dist *= ebo->length / ebo->oldlength;
      ebo->rad_head *= ebo->length / ebo->oldlength;
      ebo->rad_tail *= ebo->length / ebo->oldlength;
      ebo->oldlength = ebo->length;
      if (ebo_parent) {
        ebo_parent->rad_tail = ebo->rad_head;
      }
    }
  }
  /* fix roll */
  for (int i = 0; i < count; i++) {
    float vec[3], up_axis[3], qrot[4];
    EditBone *ebo = td[i].ebo;
    copy_v3_v3(up_axis, td[i].axismtx[2]);
    sub_v3_v3v3(vec, ebo->tail, ebo->head);
    normalize_v3(vec);
    rotation_between_vecs_to_quat(qrot, td[i].axismtx[1], vec);
    mul_qt_v3(qrot, up_axis);
    const float roll = pg_bke_ebone_roll_to_vector(ebo, up_axis, false);
    ebo->roll = angle_compat_rad(roll, td[i].ival);
  }
  MEM_freeN(td);
  arm_update(a);
  return count;
}

static void edit_lengths_sync(PGArmature *a)
{
  LISTBASE_FOREACH (EditBone *, ebo, &a->edbo) ebo->length = len_v3v3(ebo->head, ebo->tail);
}

int pg_arm_edit_translate(PGArmature *a, const float delta[3])
{
  if (!a || !delta) return 0;
  edit_lengths_sync(a);
  return edit_transform(a, fn_translate, delta);
}

int pg_arm_edit_rotate(PGArmature *a, float angle, const float pivot[3])
{
  if (!a || !pivot) return 0;
  const float arg[4] = {angle, pivot[0], pivot[1], pivot[2]};
  edit_lengths_sync(a);
  return edit_transform(a, fn_rotate, arg);
}

int pg_arm_edit_scale(PGArmature *a, float factor, const float pivot[3])
{
  if (!a || !pivot) return 0;
  const float arg[4] = {factor, pivot[0], pivot[1], pivot[2]};
  edit_lengths_sync(a);
  return edit_transform(a, fn_scale, arg);
}

int pg_arm_extrude(PGArmature *a, const float delta[3])
{
  if (!a) return 0;
  /* armature_extrude_exec, forked = false, no X-mirror */
  bArmature *arm = &a->arm;
  EditBone *newbone = NULL, *ebone, *first = NULL;
  int totbone = 0, do_extrude;
  for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
    if (ebone->flag & BONE_ROOTSEL) {
      if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
        if (ebone->parent->flag & BONE_TIPSEL) {
          ebone->flag &= ~BONE_ROOTSEL;
        }
      }
    }
  }
  for (ebone = arm->edbo->first; ((ebone) && (ebone != first)); ebone = ebone->next) {
    do_extrude = false;
    if (ebone->flag & (BONE_TIPSEL | BONE_SELECTED)) {
      do_extrude = true;
    }
    else if (ebone->flag & BONE_ROOTSEL) {
      if (!(ebone->parent && (ebone->parent->flag & BONE_TIPSEL))) {
        do_extrude = 2;
      }
    }
    if (!do_extrude) continue;
    totbone++;
    newbone = MEM_callocN(sizeof(EditBone), "extrudebone");
    if (do_extrude == true) {
      copy_v3_v3(newbone->head, ebone->tail);
      copy_v3_v3(newbone->tail, newbone->head);
      newbone->parent = ebone;
      newbone->flag = ebone->flag & (BONE_TIPSEL | BONE_RELATIVE_PARENTING);
      if (newbone->parent) {
        newbone->flag |= BONE_CONNECTED;
      }
    }
    else {
      copy_v3_v3(newbone->head, ebone->head);
      copy_v3_v3(newbone->tail, ebone->head);
      newbone->parent = ebone->parent;
      newbone->flag = BONE_TIPSEL;
      if (newbone->parent && (ebone->flag & BONE_CONNECTED)) {
        newbone->flag |= BONE_CONNECTED;
      }
    }
    newbone->weight = ebone->weight;
    newbone->dist = ebone->dist;
    newbone->xwidth = ebone->xwidth;
    newbone->zwidth = ebone->zwidth;
    newbone->rad_head = ebone->rad_tail;
    newbone->rad_tail = ebone->rad_tail;
    newbone->segments = 1;
    newbone->layer = ebone->layer;
    newbone->roll1 = ebone->roll1;
    newbone->roll2 = ebone->roll2;
    newbone->curve_in_x = ebone->curve_in_x;
    newbone->curve_in_z = ebone->curve_in_z;
    newbone->curve_out_x = ebone->curve_out_x;
    newbone->curve_out_z = ebone->curve_out_z;
    newbone->ease1 = ebone->ease1;
    newbone->ease2 = ebone->ease2;
    copy_v3_v3(newbone->scale_in, ebone->scale_in);
    copy_v3_v3(newbone->scale_out, ebone->scale_out);
    STRNCPY(newbone->name, ebone->name);
    pg_bke_ebone_unique_name(arm->edbo, newbone->name, NULL);
    BLI_addtail(arm->edbo, newbone);
    if (!first) {
      first = newbone;
    }
    ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
  }
  if (totbone == 0) return 0;
  arm->act_edbone = (totbone == 1 && first) ? first : newbone;
  pg_bke_edit_sync_selection(arm->edbo);
  /* the translate that follows: the new bones' tips are the selection (oldlength 0) */
  LISTBASE_FOREACH (EditBone *, eb, &a->edbo) eb->length = len_v3v3(eb->head, eb->tail);
  if (delta) {
    edit_transform(a, fn_translate, delta);
  }
  else {
    arm_update(a);
  }
  return totbone;
}

int pg_arm_bone_set(PGArmature *a, int bone, const float head[3], const float tail[3], float roll)
{
  EditBone *eb = ebone_at(a, bone);
  if (!eb) return 0;
  if (head) copy_v3_v3(eb->head, head);
  if (tail) copy_v3_v3(eb->tail, tail);
  eb->roll = roll;
  /* connected bones share the point: the parent's tail follows a new head, children follow the tail */
  if (head && eb->parent && (eb->flag & BONE_CONNECTED)) copy_v3_v3(eb->parent->tail, eb->head);
  LISTBASE_FOREACH (EditBone *, c, &a->edbo) {
    if (c->parent == eb && (c->flag & BONE_CONNECTED)) copy_v3_v3(c->head, eb->tail);
  }
  arm_update(a);
  return 1;
}

int pg_arm_parent_set(PGArmature *a, int mode)
{
  if (!a) return 0;
  /* armature_parent_set_exec without X-mirror */
  EditBone *actbone = a->arm.act_edbone;
  if (actbone == NULL) return 0;
  bool is_active_only_selected = false;
  if (actbone->flag & BONE_SELECTED) {
    is_active_only_selected = true;
    LISTBASE_FOREACH (EditBone *, ebone, &a->edbo) {
      if ((ebone->flag & BONE_SELECTED) && ebone != actbone) {
        is_active_only_selected = false;
        break;
      }
    }
  }
  int changed = 0;
  if (is_active_only_selected) {
    if (actbone->parent) {
      pg_bke_bone_connect_to_existing_parent(actbone);
      changed = 1;
    }
  }
  else {
    LISTBASE_FOREACH (EditBone *, ebone, &a->edbo) {
      if ((ebone->flag & BONE_SELECTED) && ebone != actbone) {
        pg_bke_bone_connect_to_new_parent(&a->edbo, ebone, actbone, (short)mode);
        changed++;
      }
    }
  }
  arm_update(a);
  return changed;
}

int pg_arm_parent_clear(PGArmature *a, int mode)
{
  if (!a) return 0;
  /* armature_parent_clear_exec */
  int changed = 0;
  LISTBASE_FOREACH (EditBone *, ebone, &a->edbo) {
    if (ebone->flag & BONE_SELECTED) {
      pg_bke_editbone_clear_parent(ebone, mode);
      changed++;
    }
  }
  pg_bke_edit_sync_selection(&a->edbo);
  arm_update(a);
  return changed;
}

int pg_arm_delete_selected(PGArmature *a)
{
  if (!a) return 0;
  /* armature_delete_selected_exec (the pose channels go with the bones in pose rebuild) */
  int changed = 0;
  for (EditBone *cur = a->edbo.first, *next; cur; cur = next) {
    next = cur->next;
    if ((a->arm.layer & cur->layer) && (cur->flag & BONE_SELECTED)) {
      if (cur == a->arm.act_edbone) a->arm.act_edbone = NULL;
      pg_bke_ebone_remove(&a->arm, cur);
      changed++;
    }
  }
  if (changed) {
    pg_bke_edit_sync_selection(&a->edbo);
    arm_update(a);
  }
  return changed;
}

/* BKE_animdata_fix_paths_rename for pose.bones["old"] prefixes */
static void fcurves_rename_bone(PGArmature *a, const char *oldname, const char *newname)
{
  char oldp[MAXBONENAME + 16], newp[MAXBONENAME + 16];
  BLI_snprintf(oldp, sizeof(oldp), "pose.bones[\"%s\"]", oldname);
  BLI_snprintf(newp, sizeof(newp), "pose.bones[\"%s\"]", newname);
  const size_t ol = strlen(oldp);
  LISTBASE_FOREACH (FCurve *, fcu, &a->curves) {
    if (fcu->rna_path && strncmp(fcu->rna_path, oldp, ol) == 0) {
      const size_t len = strlen(newp) + strlen(fcu->rna_path + ol) + 1;
      char *s = MEM_mallocN(len, "rna_path");
      BLI_snprintf(s, len, "%s%s", newp, fcu->rna_path + ol);
      MEM_freeN(fcu->rna_path);
      fcu->rna_path = s;
    }
  }
}

int pg_arm_bone_rename(PGArmature *a, int bone, const char *name, char *r_old, char *r_new)
{
  EditBone *eb = ebone_at(a, bone);
  if (!eb || !name) return 0;
  /* ED_armature_bone_rename (edit mode branch) */
  char oldname[MAXBONENAME], newname[MAXBONENAME];
  STRNCPY(oldname, eb->name);
  STRNCPY(newname, name);
  if (STREQ(oldname, newname)) return 0;
  pg_bke_ebone_unique_name(&a->edbo, newname, eb);
  STRNCPY(eb->name, newname);
  bPoseChannel *pchan = BKE_pose_channel_find_name(&a->pose, oldname);
  if (pchan) STRNCPY(pchan->name, newname);
  fcurves_rename_bone(a, oldname, newname);
  if (r_old) BLI_strncpy(r_old, oldname, MAXBONENAME);
  if (r_new) BLI_strncpy(r_new, newname, MAXBONENAME);
  arm_update(a);
  return 1;
}

/* ---------------------------------------------------------------------------------------- */
/* pose mode                                                                                 */

int pg_arm_pose_select(PGArmature *a, int bone, bool extend)
{
  if (!a) return 0;
  /* ED_armature_pose_select_pick_bone; the edit bone keeps the flag across rebuilds */
  if (!extend) {
    LISTBASE_FOREACH (EditBone *, eb, &a->edbo) eb->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
  }
  EditBone *eb = ebone_at(a, bone);
  if (eb) {
    eb->flag |= BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;
    a->arm.act_edbone = eb;
  }
  arm_update(a);
  return eb != NULL || bone < 0;
}

bool pg_arm_bone_selected(const PGArmature *a, int bone)
{
  const EditBone *eb = ebone_at(a, bone);
  return eb && (eb->flag & BONE_SELECTED);
}

int pg_arm_pose_get(const PGArmature *a, int bone, float r[10])
{
  const bPoseChannel *pc = pchan_at(a, bone);
  if (!pc) return 0;
  copy_v3_v3(r, pc->loc);
  copy_qt_qt(r + 3, pc->quat);
  copy_v3_v3(r + 7, pc->size);
  return 1;
}

int pg_arm_pose_set(PGArmature *a, int bone, const float v[10])
{
  bPoseChannel *pc = pchan_at(a, bone);
  if (!pc) return 0;
  copy_v3_v3(pc->loc, v);
  copy_qt_qt(pc->quat, v + 3);
  copy_v3_v3(pc->size, v + 7);
  pg_bke_pose_where_is(&a->arm, &a->pose);
  return 1;
}

/* set_pose_transflags: selected channels without a selected ancestor */
static bool pchan_is_transform_root(const bPoseChannel *pc)
{
  if (!pc->bone || !(pc->bone->flag & BONE_SELECTED)) return false;
  for (const bPoseChannel *p = pc->parent; p; p = p->parent) {
    if (p->bone && (p->bone->flag & BONE_SELECTED)) return false;
  }
  return true;
}

int pg_arm_pose_translate(PGArmature *a, const float delta[3])
{
  if (!a || !delta) return 0;
  int n = 0;
  LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
    if (!pchan_is_transform_root(pc) || (pc->bone->flag & BONE_CONNECTED)) continue;
    /* pose space -> bone space of the location (td->smtx of the pose bone) */
    BoneParentTransform bpt;
    float m[3][3], d[3];
    BKE_bone_parent_transform_calc_from_pchan(pc, &bpt);
    BKE_bone_parent_transform_invert(&bpt);
    copy_m3_m4(m, bpt.loc_mat);
    mul_v3_m3v3(d, m, delta);
    add_v3_v3(pc->loc, d);
    n++;
  }
  pg_bke_pose_where_is(&a->arm, &a->pose);
  return n;
}

int pg_arm_pose_rotate(PGArmature *a, float angle)
{
  if (!a) return 0;
  int n = 0;
  float rot[3][3];
  axis_angle_normalized_to_mat3(rot, (const float[3]){0.0f, 0.0f, 1.0f}, angle);
  LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
    if (!pchan_is_transform_root(pc)) continue;
    /* new pose matrix: rotated about the head in pose space, back to the channel (bone space) */
    float pose_mat[4][4], rot3[3][3], chan[4][4], oldq[4], loc[3];
    copy_m4_m4(pose_mat, pc->pose_mat);
    copy_m3_m4(rot3, pose_mat);
    mul_m3_m3m3(rot3, rot, rot3);
    copy_m4_m3(pose_mat, rot3);
    copy_v3_v3(pose_mat[3], pc->pose_mat[3]);
    BKE_armature_mat_pose_to_bone(pc, pose_mat, chan);
    copy_qt_qt(oldq, pc->quat);
    copy_v3_v3(loc, pc->loc);
    BKE_pchan_apply_mat4(pc, chan, false);
    copy_v3_v3(pc->loc, loc); /* the head does not move */
    if (dot_qtqt(oldq, pc->quat) < 0.0f) mul_qt_fl(pc->quat, -1.0f); /* keep keys interpolating the short way */
    n++;
  }
  pg_bke_pose_where_is(&a->arm, &a->pose);
  return n;
}

int pg_arm_pose_scale(PGArmature *a, float factor)
{
  if (!a) return 0;
  int n = 0;
  LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
    if (!pchan_is_transform_root(pc)) continue;
    mul_v3_fl(pc->size, factor);
    n++;
  }
  pg_bke_pose_where_is(&a->arm, &a->pose);
  return n;
}

int pg_arm_pose_clear(PGArmature *a, int what)
{
  if (!a) return 0;
  int n = 0;
  LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
    if (!pc->bone || !(pc->bone->flag & BONE_SELECTED)) continue;
    if (what & 1) pg_bke_pchan_clear_loc(pc);
    if (what & 2) pg_bke_pchan_clear_rot(pc);
    if (what & 4) pg_bke_pchan_clear_scale(pc);
    n++;
  }
  pg_bke_pose_where_is(&a->arm, &a->pose);
  return n;
}

int pg_arm_pose_copy(PGArmature *a)
{
  if (!a) return 0;
  /* pose_copy_exec: set_pose_keys then the whole pose goes to the buffer */
  BLI_freelistN(&a->copybuf);
  int n = 0;
  LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
    if (pc->bone && (pc->bone->flag & BONE_SELECTED) && (a->arm.layer & pc->bone->layer)) {
      pc->flag |= POSE_KEY;
      n++;
    }
    else {
      pc->flag &= ~POSE_KEY;
    }
    bPoseChannel *c = MEM_dupallocN(pc);
    c->next = c->prev = NULL;
    c->bone = NULL;
    c->parent = c->child = NULL;
    BLI_addtail(&a->copybuf, c);
  }
  return n;
}

int pg_arm_pose_paste(PGArmature *a, bool flipped, bool selected_mask)
{
  if (!a || BLI_listbase_is_empty(&a->copybuf)) return 0;
  /* pose_paste_exec */
  bool selOnly = selected_mask;
  if (selOnly) {
    bool any = false;
    LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
      if (pc->bone && (pc->bone->flag & BONE_SELECTED)) any = true;
    }
    if (!any) selOnly = false;
  }
  int n = 0;
  LISTBASE_FOREACH (bPoseChannel *, chan, &a->copybuf) {
    if (chan->flag & POSE_KEY) {
      if (pg_bke_pose_bone_do_paste(&a->pose, chan, selOnly, flipped)) n++;
    }
  }
  pg_bke_pose_where_is(&a->arm, &a->pose);
  return n;
}

/* ---------------------------------------------------------------------------------------- */
/* action                                                                                    */

static void bone_path(char *dst, size_t len, const char *bone, int channel)
{
  BLI_snprintf(dst, len, "pose.bones[\"%s\"].%s", bone, channel_prop[channel]);
}

static FCurve *fcurve_find(const PGArmature *a, const char *bone, int channel)
{
  char path[MAXBONENAME + 64];
  bone_path(path, sizeof(path), bone, channel);
  LISTBASE_FOREACH (FCurve *, fcu, &a->curves) {
    if (fcu->array_index == channel_index[channel] && fcu->rna_path && STREQ(fcu->rna_path, path)) {
      return fcu;
    }
  }
  return NULL;
}

/* ED_action_fcurve_ensure */
static FCurve *fcurve_ensure(PGArmature *a, const char *bone, int channel)
{
  FCurve *fcu = fcurve_find(a, bone, channel);
  if (fcu) return fcu;
  char path[MAXBONENAME + 64];
  bone_path(path, sizeof(path), bone, channel);
  fcu = MEM_callocN(sizeof(FCurve), "FCurve"); /* BKE_fcurve_create */
  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->auto_smoothing = FCURVE_SMOOTH_CONT_ACCEL; /* U.auto_smoothing_new default */
  fcu->rna_path = BLI_strdup(path);
  fcu->array_index = channel_index[channel];
  BLI_addtail(&a->curves, fcu);
  return fcu;
}

static float *channel_value(bPoseChannel *pc, int channel)
{
  if (channel < 3) return &pc->loc[channel];
  if (channel < 7) return &pc->quat[channel - 3];
  return &pc->size[channel - 7];
}

int pg_arm_key_insert(PGArmature *a, float frame)
{
  if (!a) return 0;
  int n = 0;
  LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
    if (!pc->bone || !(pc->bone->flag & BONE_SELECTED)) continue;
    for (int c = 0; c < PG_ARM_CHANNELS; c++) {
      FCurve *fcu = fcurve_ensure(a, pc->name, c);
      if (pg_bke_insert_vert_fcurve(fcu, frame, *channel_value(pc, c)) >= 0) n++;
    }
  }
  return n;
}

static int find_key(const FCurve *fcu, float frame)
{
  for (int i = 0; i < fcu->totvert; i++) {
    if (fabsf(fcu->bezt[i].vec[1][0] - frame) < 0.01f) return i;
  }
  return -1;
}

int pg_arm_key_delete(PGArmature *a, int bone, float frame)
{
  const EditBone *eb = ebone_at(a, bone);
  if (!eb) return 0;
  int n = 0;
  for (int c = 0; c < PG_ARM_CHANNELS; c++) {
    FCurve *fcu = fcurve_find(a, eb->name, c);
    if (!fcu) continue;
    const int i = find_key(fcu, frame);
    if (i < 0) continue;
    /* BKE_fcurve_delete_key, then delete the empty curve (ANIM_fcurve_delete_from_animdata) */
    memmove(&fcu->bezt[i], &fcu->bezt[i + 1], sizeof(BezTriple) * (fcu->totvert - i - 1));
    fcu->totvert--;
    n++;
    if (fcu->totvert == 0) {
      BLI_remlink(&a->curves, fcu);
      fcurve_free(fcu);
    }
    else {
      pg_bke_fcurve_handles_recalc(fcu);
    }
  }
  return n;
}

int pg_arm_key_set_interpolation(PGArmature *a, int bone, float frame, int ipo)
{
  const EditBone *eb = ebone_at(a, bone);
  if (!eb || ipo < BEZT_IPO_CONST || ipo > BEZT_IPO_BEZ) return 0;
  int n = 0;
  for (int c = 0; c < PG_ARM_CHANNELS; c++) {
    FCurve *fcu = fcurve_find(a, eb->name, c);
    const int i = fcu ? find_key(fcu, frame) : -1;
    if (i < 0) continue;
    fcu->bezt[i].ipo = (char)ipo; /* ANIM_editkeyframes_ipo set_bezt_* */
    pg_bke_fcurve_handles_recalc(fcu);
    n++;
  }
  return n;
}

static int cmp_float(const void *x, const void *y)
{
  const float a = *(const float *)x, b = *(const float *)y;
  return (a > b) - (a < b);
}

int pg_arm_bone_keys(const PGArmature *a, int bone, float *r_frames, int cap)
{
  const EditBone *eb = ebone_at(a, bone);
  if (!eb) return 0;
  int total = 0;
  for (int c = 0; c < PG_ARM_CHANNELS; c++) {
    const FCurve *fcu = fcurve_find(a, eb->name, c);
    if (fcu) total += fcu->totvert;
  }
  if (total == 0) return 0;
  float *all = MEM_mallocN(sizeof(float) * total, __func__);
  int k = 0;
  for (int c = 0; c < PG_ARM_CHANNELS; c++) {
    const FCurve *fcu = fcurve_find(a, eb->name, c);
    for (int i = 0; fcu && i < fcu->totvert; i++) all[k++] = fcu->bezt[i].vec[1][0];
  }
  qsort(all, total, sizeof(float), cmp_float);
  int n = 0;
  for (int i = 0; i < total; i++) {
    if (n > 0 && fabsf(all[i] - all[i - 1]) < 0.01f) continue;
    if (r_frames && n < cap) r_frames[n] = all[i];
    n++;
  }
  MEM_freeN(all);
  return n;
}

int pg_arm_key_interpolation(const PGArmature *a, int bone, float frame)
{
  const EditBone *eb = ebone_at(a, bone);
  if (!eb) return -1;
  for (int c = 0; c < PG_ARM_CHANNELS; c++) {
    const FCurve *fcu = fcurve_find(a, eb->name, c);
    const int i = fcu ? find_key(fcu, frame) : -1;
    if (i >= 0) return fcu->bezt[i].ipo;
  }
  return -1;
}

FCurve *pg_arm_fcurve(const PGArmature *a, int bone, int channel)
{
  const EditBone *eb = ebone_at(a, bone);
  return eb && channel >= 0 && channel < PG_ARM_CHANNELS ? fcurve_find(a, eb->name, channel) : NULL;
}

int pg_arm_fcurve_count(const PGArmature *a) { return a ? BLI_listbase_count(&a->curves) : 0; }
FCurve *pg_arm_fcurve_at(const PGArmature *a, int index)
{
  return a && index >= 0 ? BLI_findlink(&a->curves, index) : NULL;
}

void pg_arm_evaluate(PGArmature *a, float frame)
{
  if (!a) return;
  /* animsys_evaluate_fcurves on the pose channels the action drives */
  LISTBASE_FOREACH (bPoseChannel *, pc, &a->pose.chanbase) {
    for (int c = 0; c < PG_ARM_CHANNELS; c++) {
      FCurve *fcu = fcurve_find(a, pc->name, c);
      if (fcu && fcu->totvert > 0 && !(fcu->flag & FCURVE_MUTED)) {
        *channel_value(pc, c) = pg_bke_fcurve_eval_keyframes(fcu, frame);
      }
    }
  }
  pg_bke_pose_where_is(&a->arm, &a->pose);
}

void pg_arm_update(PGArmature *a)
{
  if (a) arm_update(a);
}

/* ---------------------------------------------------------------------------------------- */
/* modifier / bind                                                                           */

int pg_arm_deform_stroke(const PGArmature *a, const bGPdata *gpd, bGPDstroke *gps, int deformflag,
                         const char *vgname)
{
  /* MOD_gpencil_legacy_armature.c gpencil_deform_verts; isDisabled without an armature */
  if (!a || !gpd || !gps || gps->totpoints <= 0 || BLI_listbase_is_empty(&a->pose.chanbase)) return 0;
  if (a->mode == PG_ARM_MODE_EDIT) return 0; /* arm->edbo: in edit mode */
  float mat[4][4];
  pg_arm_canvas_matrix(mat);
  float(*vert_coords)[3] = MEM_mallocN(sizeof(float[3]) * gps->totpoints, __func__);
  BKE_gpencil_dvert_ensure(gps);
  for (int i = 0; i < gps->totpoints; i++) copy_v3_v3(vert_coords[i], &gps->points[i].x);
  pg_bke_armature_deform_gpencil_stroke(&a->arm, &a->pose, &gpd->vertex_group_names, mat,
                                        vert_coords, gps->totpoints, deformflag, vgname, gps);
  for (int i = 0; i < gps->totpoints; i++) copy_v3_v3(&gps->points[i].x, vert_coords[i]);
  MEM_freeN(vert_coords);
  BKE_gpencil_stroke_geometry_update((bGPdata *)gpd, gps);
  return 1;
}

int pg_arm_bind_frame(const PGArmature *a, bGPdata *gpd, bGPDframe *gpf, int mode)
{
  if (!a || !gpd || mode < 0 || mode > 3) return -1;
  if (mode == 0) return 0;
  float mat[4][4];
  pg_arm_canvas_matrix(mat);
  /* DEFAULT_RATIO 0.10, DEFAULT_DECAY 0.8; PAR_ARMATURE_ENVELOPE binds as AUTO for GP */
  return pg_bke_gpencil_vgroup_calc_from_armature(gpd, gpf, &a->arm, &a->pose, mat,
                                                  mode == 1 ? 0 : 1, 0.10f, 0.8f);
}

#define PG_ARM_REGISTRY 8
static struct {
  const bGPdata *gpd;
  const PGArmature *arm;
} g_registry[PG_ARM_REGISTRY];

void pg_arm_register(const bGPdata *gpd, const PGArmature *a)
{
  int free_slot = -1;
  for (int i = 0; i < PG_ARM_REGISTRY; i++) {
    if (g_registry[i].gpd == gpd) {
      g_registry[i].arm = a;
      if (!a) g_registry[i].gpd = NULL;
      return;
    }
    if (!g_registry[i].gpd && free_slot < 0) free_slot = i;
  }
  if (a && free_slot >= 0) {
    g_registry[free_slot].gpd = gpd;
    g_registry[free_slot].arm = a;
  }
}

const PGArmature *pg_arm_for_gpd(const bGPdata *gpd)
{
  for (int i = 0; i < PG_ARM_REGISTRY; i++) {
    if (gpd && g_registry[i].gpd == gpd) return g_registry[i].arm;
  }
  return NULL;
}

/* ---- viewport display (overlay_armature.cc reads these) ---- */
int pg_arm_display_type(const PGArmature *a) { return a ? a->arm.drawtype : ARM_OCTA; }
bool pg_arm_show_in_front(const PGArmature *a) { return a && a->show_in_front; }
void pg_arm_set_display(PGArmature *a, int drawtype, bool in_front)
{
  if (!a) return;
  /* rna_armature.c display_type: Octahedral / Stick / B-Bone / Envelope / Wire */
  if (drawtype >= ARM_OCTA && drawtype <= ARM_WIRE) a->arm.drawtype = (char)drawtype;
  a->show_in_front = in_front ? 1 : 0;
}

static float dist_to_seg_v2(const float p[2], const float a[2], const float b[2])
{
  float ab[2] = {b[0] - a[0], b[1] - a[1]}, ap[2] = {p[0] - a[0], p[1] - a[1]};
  const float l2 = ab[0] * ab[0] + ab[1] * ab[1];
  float t = l2 > 0.0f ? (ap[0] * ab[0] + ap[1] * ab[1]) / l2 : 0.0f;
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return hypotf(p[0] - a[0] - t * ab[0], p[1] - a[1] - t * ab[1]);
}
static bool project_win(const float m[4][4], const float co[3], float r[2])
{
  float v[4] = {co[0], co[1], co[2], 1.0f};
  mul_m4_v4(m, v);
  if (v[3] <= 0.0f) return false;
  r[0] = v[0] / v[3];
  r[1] = v[1] / v[3];
  return true;
}

int pg_arm_pick(PGArmature *a, const float arm_to_win[4][4], const float mval[2], float dist,
                bool extend)
{
  if (!a || a->mode == PG_ARM_MODE_OBJECT) return -1;
  const bool edit = a->mode == PG_ARM_MODE_EDIT;
  int best = -1, best_part = BONE_SELECTED, i = 0;
  float best_d = dist;
  LISTBASE_FOREACH (EditBone *, eb, &a->edbo) {
    float h[3], t[3];
    const bPoseChannel *pc = edit ? NULL : BKE_pose_channel_find_name(&a->pose, eb->name);
    copy_v3_v3(h, pc ? pc->pose_head : eb->head);
    copy_v3_v3(t, pc ? pc->pose_tail : eb->tail);
    float sh[2], st[2], sj[2];
    if (project_win(arm_to_win, h, sh) && project_win(arm_to_win, t, st)) {
      /* joint radius: the octahedral point sphere (PT_DEFAULT_RAD 0.05 of the length) or the
       * stick's 10 px circles (2 * 5 px * U.pixelsize, with dist = 75 px * U.pixelsize) */
      float r = dist * (10.0f / 75.0f);
      if (a->arm.drawtype != ARM_LINE) {
        float off[3];
        madd_v3_v3v3fl(off, t, (float[3]){1.0f, 0.0f, 0.0f}, 0.05f * len_v3v3(h, t));
        if (project_win(arm_to_win, off, sj)) r = len_v2v2(sj, st);
      }
      /* edit mode: head / tail spheres are BONESEL_ROOT / BONESEL_TIP (the head only when it is
       * drawn: not connected), win a tie with the body */
      const float d_body = dist_to_seg_v2(mval, sh, st);
      float d_head = len_v2v2(mval, sh) - r, d_tail = len_v2v2(mval, st) - r;
      if (!edit || (eb->parent && (eb->flag & BONE_CONNECTED))) d_head = FLT_MAX;
      if (!edit) d_tail = FLT_MAX;
      d_head = MAX2(d_head, 0.0f);
      d_tail = MAX2(d_tail, 0.0f);
      if (d_tail <= best_d && d_tail <= d_head && d_tail <= d_body) {
        best_d = d_tail, best = i, best_part = BONE_TIPSEL;
      }
      else if (d_head <= best_d && d_head <= d_body) {
        best_d = d_head, best = i, best_part = BONE_ROOTSEL;
      }
      else if (d_body < best_d) {
        best_d = d_body, best = i, best_part = BONE_SELECTED;
      }
    }
    i++;
  }
  /* a tap on nothing deselects all (deselect_all of view3d_select_exec) */
  if (best < 0) {
    if (!extend) {
      if (edit) pg_arm_edit_select(a, -1, 0, false);
      else pg_arm_pose_select(a, -1, false);
    }
    return -1;
  }
  if (edit) pg_arm_edit_select(a, best, best_part, extend);
  else pg_arm_pose_select(a, best, extend);
  return best;
}
