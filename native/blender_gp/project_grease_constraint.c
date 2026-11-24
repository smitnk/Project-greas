/* SPDX-License-Identifier: GPL-2.0-or-later
 * Bone constraints and the IK solver of Project Grease's rig, ported from Blender 3.6.23:
 * - blenkernel/intern/constraint.c: verbatim regions (marked "constraint.c:<lines>") for the
 *   evaluation object, space conversion (World / Pose / Local / Local with Parent / Owner Local),
 *   targets, the constraint stack (BKE_constraints_solve) and the Child Of, IK, Limit Rotation,
 *   Copy Location, Copy Rotation and Damped Track types; the other types are not registered.
 * - blenkernel/intern/armature.c BKE_pose_where_is_bone / BKE_pose_where_is (adapted: no Spline IK,
 *   no cyclic offset) and action.c BKE_pose_update_constraint_flags.
 * - ikplugin/intern/ikplugin_api.c dispatch (only the default "Standard" iksolver plugin); the
 *   solver itself is ikplugin/intern/iksolver_plugin.c and intern/iksolver, compiled unmodified.
 * There is no depsgraph or copy-on-write: the rig's Object is the original, so Child Of's
 * "Set Inverse" writes its own data (constraint_find_original_for_update returns NULL). */
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_lib_id.h"

#include "BIK_api.h"
#include "ikplugin/intern/iksolver_plugin.h"

#include "project_grease_armature_bke.h"
#include "project_grease_constraint.h"

typedef struct Depsgraph Depsgraph;
#define CLOG_WARN(...) ((void)0)
/* constraint.c:92 */
#define VALID_CONS_TARGET(ct) ((ct) && (ct->tar))

static void damptrack_do_transform(float matrix[4][4], const float tarvec[3], int track_axis);
/* No copy-on-write here: the evaluated constraint is the original. */
static bConstraint *constraint_find_original_for_update(bConstraintOb *UNUSED(cob),
                                                        bConstraint *UNUSED(con))
{
  return NULL;
}

/* object.cc BKE_object_to_mat4 (no parent, delta transforms or quaternion modes: the rig object
 * keeps DNA defaults). shortcut: Euler rotation modes only, upgrade if the rig object gets a
 * transform. */
void BKE_object_to_mat4(Object *ob, float r_mat[4][4])
{
  loc_eulO_size_to_mat4(r_mat, ob->loc, ob->rot, ob->scale, ob->rotmode > 0 ? ob->rotmode : ROT_MODE_EUL);
}

/* constraint.c:111-114 */
void BKE_constraint_unique_name(bConstraint *con, ListBase *list)
{
  BLI_uniquename(list, con, DATA_("Const"), '.', offsetof(bConstraint, name), sizeof(con->name));
}


/* constraint.c:117-503 */
/* ----------------- Evaluation Loop Preparation --------------- */

bConstraintOb *BKE_constraints_make_evalob(
    Depsgraph *depsgraph, Scene *scene, Object *ob, void *subdata, short datatype)
{
  bConstraintOb *cob;

  /* create regardless of whether we have any data! */
  cob = MEM_callocN(sizeof(bConstraintOb), "bConstraintOb");

  /* NOTE(@ton): For system time, part of de-globalization, code nicer later with local time. */
  cob->scene = scene;
  cob->depsgraph = depsgraph;

  /* based on type of available data */
  switch (datatype) {
    case CONSTRAINT_OBTYPE_OBJECT: {
      /* disregard subdata... calloc should set other values right */
      if (ob) {
        cob->ob = ob;
        cob->type = datatype;

        if (cob->ob->rotmode > 0) {
          /* Should be some kind of Euler order, so use it */
          /* NOTE: Versions <= 2.76 assumed that "default" order
           *       would always get used, so we may seem some rig
           *       breakage as a result. However, this change here
           *       is needed to fix #46599
           */
          cob->rotOrder = ob->rotmode;
        }
        else {
          /* Quats/Axis-Angle, so Eulers should just use default order */
          cob->rotOrder = EULER_ORDER_DEFAULT;
        }
        copy_m4_m4(cob->matrix, ob->object_to_world);
      }
      else {
        unit_m4(cob->matrix);
      }

      copy_m4_m4(cob->startmat, cob->matrix);
      break;
    }
    case CONSTRAINT_OBTYPE_BONE: {
      /* only set if we have valid bone, otherwise default */
      if (ob && subdata) {
        cob->ob = ob;
        cob->pchan = (bPoseChannel *)subdata;
        cob->type = datatype;

        if (cob->pchan->rotmode > 0) {
          /* should be some type of Euler order */
          cob->rotOrder = cob->pchan->rotmode;
        }
        else {
          /* Quats, so eulers should just use default order */
          cob->rotOrder = EULER_ORDER_DEFAULT;
        }

        /* matrix in world-space */
        mul_m4_m4m4(cob->matrix, ob->object_to_world, cob->pchan->pose_mat);
      }
      else {
        unit_m4(cob->matrix);
      }

      copy_m4_m4(cob->startmat, cob->matrix);
      break;
    }
    default: /* other types not yet handled */
      unit_m4(cob->matrix);
      unit_m4(cob->startmat);
      break;
  }

  return cob;
}

void BKE_constraints_clear_evalob(bConstraintOb *cob)
{
  float delta[4][4], imat[4][4];

  /* prevent crashes */
  if (cob == NULL) {
    return;
  }

  /* calculate delta of constraints evaluation */
  invert_m4_m4(imat, cob->startmat);
  /* XXX This would seem to be in wrong order. However, it does not work in 'right' order -
   *     would be nice to understand why premul is needed here instead of usual postmul?
   *     In any case, we **do not get a delta** here (e.g. startmat & matrix having same location,
   *     still gives a 'delta' with non-null translation component :/ ). */
  mul_m4_m4m4(delta, cob->matrix, imat);

  /* copy matrices back to source */
  switch (cob->type) {
    case CONSTRAINT_OBTYPE_OBJECT: {
      /* cob->ob might not exist! */
      if (cob->ob) {
        /* copy new ob-matrix back to owner */
        copy_m4_m4(cob->ob->object_to_world, cob->matrix);

        /* copy inverse of delta back to owner */
        invert_m4_m4(cob->ob->constinv, delta);
      }
      break;
    }
    case CONSTRAINT_OBTYPE_BONE: {
      /* cob->ob or cob->pchan might not exist */
      if (cob->ob && cob->pchan) {
        /* copy new pose-matrix back to owner */
        mul_m4_m4m4(cob->pchan->pose_mat, cob->ob->world_to_object, cob->matrix);

        /* copy inverse of delta back to owner */
        invert_m4_m4(cob->pchan->constinv, delta);
      }
      break;
    }
  }

  /* Free temporary struct. */
  MEM_freeN(cob);
}

/* -------------- Space-Conversion API -------------- */

void BKE_constraint_mat_convertspace(Object *ob,
                                     bPoseChannel *pchan,
                                     bConstraintOb *cob,
                                     float mat[4][4],
                                     short from,
                                     short to,
                                     const bool keep_scale)
{
  float diff_mat[4][4];
  float imat[4][4];

  /* Prevent crashes in these unlikely events. */
  if (ob == NULL || mat == NULL) {
    return;
  }
  /* optimize trick - check if need to do anything */
  if (from == to) {
    return;
  }

  /* are we dealing with pose-channels or objects */
  if (pchan) {
    /* pose channels */
    switch (from) {
      case CONSTRAINT_SPACE_WORLD: /* ---------- FROM WORLDSPACE ---------- */
      {
        if (to == CONSTRAINT_SPACE_CUSTOM) {
          /* World to custom. */
          BLI_assert(cob);
          invert_m4_m4(imat, cob->space_obj_world_matrix);
          mul_m4_m4m4(mat, imat, mat);
        }
        else {
          /* World to pose. */
          invert_m4_m4(imat, ob->object_to_world);
          mul_m4_m4m4(mat, imat, mat);

          /* Use pose-space as stepping stone for other spaces. */
          if (ELEM(to,
                   CONSTRAINT_SPACE_LOCAL,
                   CONSTRAINT_SPACE_PARLOCAL,
                   CONSTRAINT_SPACE_OWNLOCAL)) {
            /* Call self with slightly different values. */
            BKE_constraint_mat_convertspace(
                ob, pchan, cob, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
          }
        }
        break;
      }
      case CONSTRAINT_SPACE_POSE: /* ---------- FROM POSESPACE ---------- */
      {
        /* pose to local */
        if (to == CONSTRAINT_SPACE_LOCAL) {
          if (pchan->bone) {
            BKE_armature_mat_pose_to_bone(pchan, mat, mat);
          }
        }
        /* pose to owner local */
        else if (to == CONSTRAINT_SPACE_OWNLOCAL) {
          /* pose to local */
          if (pchan->bone) {
            BKE_armature_mat_pose_to_bone(pchan, mat, mat);
          }

          /* local to owner local (recursive) */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_LOCAL, to, keep_scale);
        }
        /* pose to local with parent */
        else if (to == CONSTRAINT_SPACE_PARLOCAL) {
          if (pchan->bone) {
            invert_m4_m4(imat, pchan->bone->arm_mat);
            mul_m4_m4m4(mat, imat, mat);
          }
        }
        else {
          /* Pose to world. */
          mul_m4_m4m4(mat, ob->object_to_world, mat);
          /* Use world-space as stepping stone for other spaces. */
          if (to != CONSTRAINT_SPACE_WORLD) {
            /* Call self with slightly different values. */
            BKE_constraint_mat_convertspace(
                ob, pchan, cob, mat, CONSTRAINT_SPACE_WORLD, to, keep_scale);
          }
        }
        break;
      }
      case CONSTRAINT_SPACE_LOCAL: /* ------------ FROM LOCALSPACE --------- */
      {
        /* local to owner local */
        if (to == CONSTRAINT_SPACE_OWNLOCAL) {
          if (pchan->bone) {
            copy_m4_m4(diff_mat, pchan->bone->arm_mat);

            if (cob && cob->pchan && cob->pchan->bone) {
              invert_m4_m4(imat, cob->pchan->bone->arm_mat);
              mul_m4_m4m4(diff_mat, imat, diff_mat);
            }

            zero_v3(diff_mat[3]);
            invert_m4_m4(imat, diff_mat);
            mul_m4_series(mat, diff_mat, mat, imat);
          }
        }
        /* local to pose - do inverse procedure that was done for pose to local */
        else {
          if (pchan->bone) {
            /* we need the posespace_matrix = local_matrix + (parent_posespace_matrix + restpos) */
            BKE_armature_mat_bone_to_pose(pchan, mat, mat);
          }

          /* use pose-space as stepping stone for other spaces */
          if (ELEM(to, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_PARLOCAL, CONSTRAINT_SPACE_CUSTOM))
          {
            /* call self with slightly different values */
            BKE_constraint_mat_convertspace(
                ob, pchan, cob, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
          }
        }
        break;
      }
      case CONSTRAINT_SPACE_OWNLOCAL: { /* -------------- FROM OWNER LOCAL ---------- */
        /* owner local to local */
        if (pchan->bone) {
          copy_m4_m4(diff_mat, pchan->bone->arm_mat);

          if (cob && cob->pchan && cob->pchan->bone) {
            invert_m4_m4(imat, cob->pchan->bone->arm_mat);
            mul_m4_m4m4(diff_mat, imat, diff_mat);
          }

          zero_v3(diff_mat[3]);
          invert_m4_m4(imat, diff_mat);
          mul_m4_series(mat, imat, mat, diff_mat);
        }

        if (to != CONSTRAINT_SPACE_LOCAL) {
          /* call self with slightly different values */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_LOCAL, to, keep_scale);
        }
        break;
      }
      case CONSTRAINT_SPACE_PARLOCAL: /* -------------- FROM LOCAL WITH PARENT ---------- */
      {
        /* local + parent to pose */
        if (pchan->bone) {
          mul_m4_m4m4(mat, pchan->bone->arm_mat, mat);
        }

        /* use pose-space as stepping stone for other spaces */
        if (ELEM(to,
                 CONSTRAINT_SPACE_WORLD,
                 CONSTRAINT_SPACE_LOCAL,
                 CONSTRAINT_SPACE_OWNLOCAL,
                 CONSTRAINT_SPACE_CUSTOM))
        {
          /* call self with slightly different values */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
        }
        break;
      }
      case CONSTRAINT_SPACE_CUSTOM: /* -------------- FROM CUSTOM SPACE ---------- */
      {
        /* Custom to world. */
        BLI_assert(cob);
        mul_m4_m4m4(mat, cob->space_obj_world_matrix, mat);

        /* Use world-space as stepping stone for other spaces. */
        if (to != CONSTRAINT_SPACE_WORLD) {
          /* Call self with slightly different values. */
          BKE_constraint_mat_convertspace(
              ob, pchan, cob, mat, CONSTRAINT_SPACE_WORLD, to, keep_scale);
        }
        break;
      }
    }
  }
  else {
    /* objects */
    if (from == CONSTRAINT_SPACE_WORLD) {
      if (to == CONSTRAINT_SPACE_LOCAL) {
        /* Check if object has a parent. */
        if (ob->parent) {
          /* 'subtract' parent's effects from owner. */
          mul_m4_m4m4(diff_mat, ob->parent->object_to_world, ob->parentinv);
          invert_m4_m4_safe(imat, diff_mat);
          mul_m4_m4m4(mat, imat, mat);
        }
        else {
          /* Local space in this case will have to be defined as local to the owner's
           * transform-property-rotated axes. So subtract this rotation component.
           */
          /* XXX This is actually an ugly hack, local space of a parent-less object *is* the same
           * as global space! Think what we want actually here is some kind of 'Final Space', i.e
           *     . once transformations are applied - users are often confused about this too,
           *     this is not consistent with bones
           *     local space either... Meh :|
           *     --mont29
           */
          BKE_object_to_mat4(ob, diff_mat);
          if (!keep_scale) {
            normalize_m4(diff_mat);
          }
          zero_v3(diff_mat[3]);

          invert_m4_m4_safe(imat, diff_mat);
          mul_m4_m4m4(mat, imat, mat);
        }
      }
      else if (to == CONSTRAINT_SPACE_CUSTOM) {
        /* 'subtract' custom objects's effects from owner. */
        BLI_assert(cob);
        invert_m4_m4_safe(imat, cob->space_obj_world_matrix);
        mul_m4_m4m4(mat, imat, mat);
      }
    }
    else if (from == CONSTRAINT_SPACE_LOCAL) {
      /* check that object has a parent - otherwise this won't work */
      if (ob->parent) {
        /* 'add' parent's effect back to owner */
        mul_m4_m4m4(diff_mat, ob->parent->object_to_world, ob->parentinv);
        mul_m4_m4m4(mat, diff_mat, mat);
      }
      else {
        /* Local space in this case will have to be defined as local to the owner's
         * transform-property-rotated axes. So add back this rotation component.
         */
        /* XXX See comment above for world->local case... */
        BKE_object_to_mat4(ob, diff_mat);
        if (!keep_scale) {
          normalize_m4(diff_mat);
        }
        zero_v3(diff_mat[3]);

        mul_m4_m4m4(mat, diff_mat, mat);
      }
      if (to == CONSTRAINT_SPACE_CUSTOM) {
        /* 'subtract' objects's effects from owner. */
        BLI_assert(cob);
        invert_m4_m4_safe(imat, cob->space_obj_world_matrix);
        mul_m4_m4m4(mat, imat, mat);
      }
    }
    else if (from == CONSTRAINT_SPACE_CUSTOM) {
      /* Custom to world. */
      BLI_assert(cob);
      mul_m4_m4m4(mat, cob->space_obj_world_matrix, mat);

      /* Use world-space as stepping stone for other spaces. */
      if (to != CONSTRAINT_SPACE_WORLD) {
        /* Call self with slightly different values. */
        BKE_constraint_mat_convertspace(
            ob, pchan, cob, mat, CONSTRAINT_SPACE_WORLD, to, keep_scale);
      }
    }
  }
}

/* constraint.c:672-768 (constraint_target_to_mat4) without the mesh / lattice vertex group
 * branches: targets are the rig object or one of its bones. */
static void constraint_target_to_mat4(Object *ob,
                                      const char *substring,
                                      bConstraintOb *cob,
                                      float mat[4][4],
                                      short from,
                                      short to,
                                      short flag,
                                      float headtail)
{
  /* Case OBJECT */
  if (substring[0] == '\0') {
    copy_m4_m4(mat, ob->object_to_world);
    BKE_constraint_mat_convertspace(ob, NULL, cob, mat, from, to, false);
  }
  /* Case BONE */
  else {
    bPoseChannel *pchan;

    pchan = BKE_pose_channel_find_name(ob->pose, substring);
    if (pchan) {
      /* Multiply the PoseSpace accumulation/final matrix for this
       * PoseChannel by the Armature Object's Matrix to get a world-space matrix.
       */
      bool is_bbone = (pchan->bone) && (pchan->bone->segments > 1) &&
                      (flag & CONSTRAINT_BBONE_SHAPE);
      bool full_bbone = (flag & CONSTRAINT_BBONE_SHAPE_FULL) != 0;

      if (headtail < 0.000001f && !(is_bbone && full_bbone)) {
        /* skip length interpolation if set to head */
        mul_m4_m4m4(mat, ob->object_to_world, pchan->pose_mat);
      }
      else if (is_bbone && pchan->bone->segments == pchan->runtime.bbone_segments) {
        /* use point along bbone */
        Mat4 *bbone = pchan->runtime.bbone_pose_mats;
        float tempmat[4][4];
        float loc[3], fac;
        int index;

        /* figure out which segment(s) the headtail value falls in */
        BKE_pchan_bbone_deform_segment_index(pchan, headtail, &index, &fac);

        /* apply full transformation of the segment if requested */
        if (full_bbone) {
          interp_m4_m4m4(tempmat, bbone[index].mat, bbone[index + 1].mat, fac);

          mul_m4_m4m4(tempmat, pchan->pose_mat, tempmat);
        }
        /* only interpolate location */
        else {
          interp_v3_v3v3(loc, bbone[index].mat[3], bbone[index + 1].mat[3], fac);

          copy_m4_m4(tempmat, pchan->pose_mat);
          mul_v3_m4v3(tempmat[3], pchan->pose_mat, loc);
        }

        mul_m4_m4m4(mat, ob->object_to_world, tempmat);
      }
      else {
        float tempmat[4][4], loc[3];

        /* interpolate along length of bone */
        interp_v3_v3v3(loc, pchan->pose_head, pchan->pose_tail, headtail);

        /* use interpolated distance for subtarget */
        copy_m4_m4(tempmat, pchan->pose_mat);
        copy_v3_v3(tempmat[3], loc);

        mul_m4_m4m4(mat, ob->object_to_world, tempmat);
      }
    }
    else {
      copy_m4_m4(mat, ob->object_to_world);
    }

    /* convert matrix space as required */
    BKE_constraint_mat_convertspace(ob, pchan, cob, mat, from, to, false);
  }
}

/* constraint.c:804-827 */
/* This function should be used for the get_target_matrix member of all
 * constraints that are not picky about what happens to their target matrix.
 */
static void default_get_tarmat(struct Depsgraph *UNUSED(depsgraph),
                               bConstraint *con,
                               bConstraintOb *cob,
                               bConstraintTarget *ct,
                               float UNUSED(ctime))
{
  if (VALID_CONS_TARGET(ct)) {
    constraint_target_to_mat4(ct->tar,
                              ct->subtarget,
                              cob,
                              ct->matrix,
                              CONSTRAINT_SPACE_WORLD,
                              ct->space,
                              con->flag,
                              con->headtail);
  }
  else if (ct) {
    unit_m4(ct->matrix);
  }
}


/* constraint.c:851-954 */
/* This following macro should be used for all standard single-target *_get_tars functions
 * to save typing and reduce maintenance woes.
 * (Hopefully all compilers will be happy with the lines with just a space on them.
 * Those are really just to help this code easier to read).
 */
/* TODO: cope with getting rotation order... */
#define SINGLETARGET_GET_TARS(con, datatar, datasubtarget, ct, list) \
  { \
    ct = MEM_callocN(sizeof(bConstraintTarget), "tempConstraintTarget"); \
\
    ct->tar = datatar; \
    STRNCPY(ct->subtarget, datasubtarget); \
    ct->space = con->tarspace; \
    ct->flag = CONSTRAINT_TAR_TEMP; \
\
    if (ct->tar) { \
      if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) { \
        bPoseChannel *pchan = BKE_pose_channel_find_name(ct->tar->pose, ct->subtarget); \
        ct->type = CONSTRAINT_OBTYPE_BONE; \
        ct->rotOrder = (pchan) ? (pchan->rotmode) : EULER_ORDER_DEFAULT; \
      } \
      else if (OB_TYPE_SUPPORT_VGROUP(ct->tar->type) && (ct->subtarget[0])) { \
        ct->type = CONSTRAINT_OBTYPE_VERT; \
        ct->rotOrder = EULER_ORDER_DEFAULT; \
      } \
      else { \
        ct->type = CONSTRAINT_OBTYPE_OBJECT; \
        ct->rotOrder = ct->tar->rotmode; \
      } \
    } \
\
    BLI_addtail(list, ct); \
  } \
  (void)0

/* This following macro should be used for all standard single-target *_get_tars functions
 * to save typing and reduce maintenance woes. It does not do the subtarget related operations
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 * really just to help this code easier to read)
 */
/* TODO: cope with getting rotation order... */
#define SINGLETARGETNS_GET_TARS(con, datatar, ct, list) \
  { \
    ct = MEM_callocN(sizeof(bConstraintTarget), "tempConstraintTarget"); \
\
    ct->tar = datatar; \
    ct->space = con->tarspace; \
    ct->flag = CONSTRAINT_TAR_TEMP; \
\
    if (ct->tar) { \
      ct->type = CONSTRAINT_OBTYPE_OBJECT; \
    } \
    BLI_addtail(list, ct); \
  } \
  (void)0

/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintenance woes.
 * NOTE: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
#define SINGLETARGET_FLUSH_TARS(con, datatar, datasubtarget, ct, list, no_copy) \
  { \
    if (ct) { \
      bConstraintTarget *ctn = ct->next; \
      if (no_copy == 0) { \
        datatar = ct->tar; \
        STRNCPY(datasubtarget, ct->subtarget); \
        con->tarspace = (char)ct->space; \
      } \
\
      BLI_freelinkN(list, ct); \
      ct = ctn; \
    } \
  } \
  (void)0

/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintenance woes. It does not do the subtarget related operations.
 * NOTE: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 * really just to help this code easier to read)
 */
#define SINGLETARGETNS_FLUSH_TARS(con, datatar, ct, list, no_copy) \
  { \
    if (ct) { \
      bConstraintTarget *ctn = ct->next; \
      if (no_copy == 0) { \
        datatar = ct->tar; \
        con->tarspace = (char)ct->space; \
      } \
\
      BLI_freelinkN(list, ct); \
      ct = ctn; \
    } \
  } \
  (void)0

static bool is_custom_space_needed(bConstraint *con)
{
  return con->ownspace == CONSTRAINT_SPACE_CUSTOM || con->tarspace == CONSTRAINT_SPACE_CUSTOM;
}


/* constraint.c:955-1124 */
/* --------- ChildOf Constraint ------------ */

static void childof_new_data(void *cdata)
{
  bChildOfConstraint *data = (bChildOfConstraint *)cdata;

  data->flag = (CHILDOF_LOCX | CHILDOF_LOCY | CHILDOF_LOCZ | CHILDOF_ROTX | CHILDOF_ROTY |
                CHILDOF_ROTZ | CHILDOF_SIZEX | CHILDOF_SIZEY | CHILDOF_SIZEZ |
                CHILDOF_SET_INVERSE);
  unit_m4(data->invmat);
}

static void childof_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bChildOfConstraint *data = con->data;

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int childof_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bChildOfConstraint *data = con->data;
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void childof_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bChildOfConstraint *data = con->data;
    bConstraintTarget *ct = list->first;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void childof_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bChildOfConstraint *data = con->data;
  bConstraintTarget *ct = targets->first;

  /* only evaluate if there is a target */
  if (!VALID_CONS_TARGET(ct)) {
    return;
  }

  float parmat[4][4];
  float inverse_matrix[4][4];
  /* Simple matrix parenting. */
  if ((data->flag & CHILDOF_ALL) == CHILDOF_ALL) {
    copy_m4_m4(parmat, ct->matrix);
    copy_m4_m4(inverse_matrix, data->invmat);
  }
  /* Filter the parent matrix by channel. */
  else {
    float loc[3], eul[3], size[3];
    float loco[3], eulo[3], sizeo[3];

    /* extract components of both matrices */
    copy_v3_v3(loc, ct->matrix[3]);
    mat4_to_eulO(eul, ct->rotOrder, ct->matrix);
    mat4_to_size(size, ct->matrix);

    copy_v3_v3(loco, data->invmat[3]);
    mat4_to_eulO(eulo, cob->rotOrder, data->invmat);
    mat4_to_size(sizeo, data->invmat);

    /* Reset the locked channels to their no-op values. */
    if (!(data->flag & CHILDOF_LOCX)) {
      loc[0] = loco[0] = 0.0f;
    }
    if (!(data->flag & CHILDOF_LOCY)) {
      loc[1] = loco[1] = 0.0f;
    }
    if (!(data->flag & CHILDOF_LOCZ)) {
      loc[2] = loco[2] = 0.0f;
    }
    if (!(data->flag & CHILDOF_ROTX)) {
      eul[0] = eulo[0] = 0.0f;
    }
    if (!(data->flag & CHILDOF_ROTY)) {
      eul[1] = eulo[1] = 0.0f;
    }
    if (!(data->flag & CHILDOF_ROTZ)) {
      eul[2] = eulo[2] = 0.0f;
    }
    if (!(data->flag & CHILDOF_SIZEX)) {
      size[0] = sizeo[0] = 1.0f;
    }
    if (!(data->flag & CHILDOF_SIZEY)) {
      size[1] = sizeo[1] = 1.0f;
    }
    if (!(data->flag & CHILDOF_SIZEZ)) {
      size[2] = sizeo[2] = 1.0f;
    }

    /* Construct the new matrices given the disabled channels. */
    loc_eulO_size_to_mat4(parmat, loc, eul, size, ct->rotOrder);
    loc_eulO_size_to_mat4(inverse_matrix, loco, eulo, sizeo, cob->rotOrder);
  }

  /* If requested, compute the inverse matrix from the computed parent matrix. */
  if (data->flag & CHILDOF_SET_INVERSE) {
    invert_m4_m4(data->invmat, parmat);
    if (cob->pchan != NULL) {
      mul_m4_series(data->invmat, data->invmat, cob->ob->object_to_world);
    }

    copy_m4_m4(inverse_matrix, data->invmat);

    data->flag &= ~CHILDOF_SET_INVERSE;

    /* Write the computed matrix back to the master copy if in COW evaluation. */
    bConstraint *orig_con = constraint_find_original_for_update(cob, con);

    if (orig_con != NULL) {
      bChildOfConstraint *orig_data = orig_con->data;

      copy_m4_m4(orig_data->invmat, data->invmat);
      orig_data->flag &= ~CHILDOF_SET_INVERSE;
    }
  }

  /* Multiply together the target (parent) matrix, parent inverse,
   * and the owner transform matrix to get the effect of this constraint
   * (i.e.  owner is 'parented' to parent). */
  float orig_cob_matrix[4][4];
  copy_m4_m4(orig_cob_matrix, cob->matrix);
  mul_m4_series(cob->matrix, parmat, inverse_matrix, orig_cob_matrix);

  /* Without this, changes to scale and rotation can change location
   * of a parentless bone or a disconnected bone. Even though its set
   * to zero above. */
  if (!(data->flag & CHILDOF_LOCX)) {
    cob->matrix[3][0] = orig_cob_matrix[3][0];
  }
  if (!(data->flag & CHILDOF_LOCY)) {
    cob->matrix[3][1] = orig_cob_matrix[3][1];
  }
  if (!(data->flag & CHILDOF_LOCZ)) {
    cob->matrix[3][2] = orig_cob_matrix[3][2];
  }
}

/* XXX NOTE: con->flag should be CONSTRAINT_SPACEONCE for bone-childof, patched in `readfile.c`. */
static bConstraintTypeInfo CTI_CHILDOF = {
    /*type*/ CONSTRAINT_TYPE_CHILDOF,
    /*size*/ sizeof(bChildOfConstraint),
    /*name*/ N_("Child Of"),
    /*structName*/ "bChildOfConstraint",
    /*free_data*/ NULL,
    /*id_looper*/ childof_id_looper,
    /*copy_data*/ NULL,
    /*new_data*/ childof_new_data,
    /*get_constraint_targets*/ childof_get_tars,
    /*flush_constraint_targets*/ childof_flush_tars,
    /* get a target matrix */ default_get_tarmat,
    /*evaluate_constraint*/ childof_evaluate,
};


/* constraint.c:1309-1414 */
/* --------- Inverse-Kinematics --------- */

static void kinematic_new_data(void *cdata)
{
  bKinematicConstraint *data = (bKinematicConstraint *)cdata;

  data->weight = 1.0f;
  data->orientweight = 1.0f;
  data->iterations = 500;
  data->dist = 1.0f;
  data->flag = CONSTRAINT_IK_TIP | CONSTRAINT_IK_STRETCH | CONSTRAINT_IK_POS;
}

static void kinematic_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bKinematicConstraint *data = con->data;

  /* chain target */
  func(con, (ID **)&data->tar, false, userdata);

  /* poletarget */
  func(con, (ID **)&data->poletar, false, userdata);
}

static int kinematic_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bKinematicConstraint *data = con->data;
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints is used twice here */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
    SINGLETARGET_GET_TARS(con, data->poletar, data->polesubtarget, ct, list);

    return 2;
  }

  return 0;
}

static void kinematic_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bKinematicConstraint *data = con->data;
    bConstraintTarget *ct = list->first;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
    SINGLETARGET_FLUSH_TARS(con, data->poletar, data->polesubtarget, ct, list, no_copy);
  }
}

static void kinematic_get_tarmat(struct Depsgraph *UNUSED(depsgraph),
                                 bConstraint *con,
                                 bConstraintOb *cob,
                                 bConstraintTarget *ct,
                                 float UNUSED(ctime))
{
  bKinematicConstraint *data = con->data;

  if (VALID_CONS_TARGET(ct)) {
    constraint_target_to_mat4(ct->tar,
                              ct->subtarget,
                              cob,
                              ct->matrix,
                              CONSTRAINT_SPACE_WORLD,
                              ct->space,
                              con->flag,
                              con->headtail);
  }
  else if (ct) {
    if (data->flag & CONSTRAINT_IK_AUTO) {
      Object *ob = cob->ob;

      if (ob == NULL) {
        unit_m4(ct->matrix);
      }
      else {
        float vec[3];
        /* move grabtarget into world space */
        mul_v3_m4v3(vec, ob->object_to_world, data->grabtarget);
        copy_m4_m4(ct->matrix, ob->object_to_world);
        copy_v3_v3(ct->matrix[3], vec);
      }
    }
    else {
      unit_m4(ct->matrix);
    }
  }
}

static bConstraintTypeInfo CTI_KINEMATIC = {
    /*type*/ CONSTRAINT_TYPE_KINEMATIC,
    /*size*/ sizeof(bKinematicConstraint),
    /*name*/ N_("IK"),
    /*structName*/ "bKinematicConstraint",
    /*free_data*/ NULL,
    /*id_looper*/ kinematic_id_looper,
    /*copy_data*/ NULL,
    /*new_data*/ kinematic_new_data,
    /*get_constraint_targets*/ kinematic_get_tars,
    /*flush_constraint_targets*/ kinematic_flush_tars,
    /*get_target_matrix*/ kinematic_get_tarmat,
    /* evaluate - solved as separate loop */ NULL,
};


/* constraint.c:1647-1727 */
/* -------- Limit Rotation --------- */

static void rotlimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
  bRotLimitConstraint *data = con->data;
  float loc[3];
  float eul[3];
  float size[3];

  /* This constraint is based on euler rotation math, which doesn't work well with shear.
   * The Y axis is chosen as the main one because constraints are most commonly used on bones.
   * This also allows using the constraint to simply remove shear. */
  orthogonalize_m4_stable(cob->matrix, 1, false);

  /* Only do the complex processing if some limits are actually enabled. */
  if (!(data->flag & (LIMIT_XROT | LIMIT_YROT | LIMIT_ZROT))) {
    return;
  }

  /* Select the Euler rotation order, defaulting to the owner value. */
  short rot_order = cob->rotOrder;

  if (data->euler_order != CONSTRAINT_EULER_AUTO) {
    rot_order = data->euler_order;
  }

  /* Decompose the matrix using the specified order. */
  copy_v3_v3(loc, cob->matrix[3]);
  mat4_to_size(size, cob->matrix);

  mat4_to_eulO(eul, rot_order, cob->matrix);

  /* constraint data uses radians internally */

  /* limiting of euler values... */
  if (data->flag & LIMIT_XROT) {
    if (eul[0] < data->xmin) {
      eul[0] = data->xmin;
    }

    if (eul[0] > data->xmax) {
      eul[0] = data->xmax;
    }
  }
  if (data->flag & LIMIT_YROT) {
    if (eul[1] < data->ymin) {
      eul[1] = data->ymin;
    }

    if (eul[1] > data->ymax) {
      eul[1] = data->ymax;
    }
  }
  if (data->flag & LIMIT_ZROT) {
    if (eul[2] < data->zmin) {
      eul[2] = data->zmin;
    }

    if (eul[2] > data->zmax) {
      eul[2] = data->zmax;
    }
  }

  loc_eulO_size_to_mat4(cob->matrix, loc, eul, size, rot_order);
}

static bConstraintTypeInfo CTI_ROTLIMIT = {
    /*type*/ CONSTRAINT_TYPE_ROTLIMIT,
    /*size*/ sizeof(bRotLimitConstraint),
    /*name*/ N_("Limit Rotation"),
    /*structName*/ "bRotLimitConstraint",
    /*free_data*/ NULL,
    /*id_looper*/ NULL,
    /*copy_data*/ NULL,
    /*new_data*/ NULL,
    /*get_constraint_targets*/ NULL,
    /*flush_constraint_targets*/ NULL,
    /*get_target_matrix*/ NULL,
    /*evaluate_constraint*/ rotlimit_evaluate,
};


/* constraint.c:1796-2069 */
/* ----------- Copy Location ------------- */

static void loclike_new_data(void *cdata)
{
  bLocateLikeConstraint *data = (bLocateLikeConstraint *)cdata;

  data->flag = LOCLIKE_X | LOCLIKE_Y | LOCLIKE_Z;
}

static void loclike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bLocateLikeConstraint *data = con->data;

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int loclike_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bLocateLikeConstraint *data = con->data;
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void loclike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bLocateLikeConstraint *data = con->data;
    bConstraintTarget *ct = list->first;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void loclike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bLocateLikeConstraint *data = con->data;
  bConstraintTarget *ct = targets->first;

  if (VALID_CONS_TARGET(ct)) {
    float offset[3] = {0.0f, 0.0f, 0.0f};

    if (data->flag & LOCLIKE_OFFSET) {
      copy_v3_v3(offset, cob->matrix[3]);
    }

    if (data->flag & LOCLIKE_X) {
      cob->matrix[3][0] = ct->matrix[3][0];

      if (data->flag & LOCLIKE_X_INVERT) {
        cob->matrix[3][0] *= -1;
      }
      cob->matrix[3][0] += offset[0];
    }
    if (data->flag & LOCLIKE_Y) {
      cob->matrix[3][1] = ct->matrix[3][1];

      if (data->flag & LOCLIKE_Y_INVERT) {
        cob->matrix[3][1] *= -1;
      }
      cob->matrix[3][1] += offset[1];
    }
    if (data->flag & LOCLIKE_Z) {
      cob->matrix[3][2] = ct->matrix[3][2];

      if (data->flag & LOCLIKE_Z_INVERT) {
        cob->matrix[3][2] *= -1;
      }
      cob->matrix[3][2] += offset[2];
    }
  }
}

static bConstraintTypeInfo CTI_LOCLIKE = {
    /*type*/ CONSTRAINT_TYPE_LOCLIKE,
    /*size*/ sizeof(bLocateLikeConstraint),
    /*name*/ N_("Copy Location"),
    /*structName*/ "bLocateLikeConstraint",
    /*free_data*/ NULL,
    /*id_looper*/ loclike_id_looper,
    /*copy_data*/ NULL,
    /*new_data*/ loclike_new_data,
    /*get_constraint_targets*/ loclike_get_tars,
    /*flush_constraint_targets*/ loclike_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ loclike_evaluate,
};

/* ----------- Copy Rotation ------------- */

static void rotlike_new_data(void *cdata)
{
  bRotateLikeConstraint *data = (bRotateLikeConstraint *)cdata;

  data->flag = ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z;
}

static void rotlike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bRotateLikeConstraint *data = con->data;

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int rotlike_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bRotateLikeConstraint *data = con->data;
    bConstraintTarget *ct;

    /* standard target-getting macro for single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void rotlike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bRotateLikeConstraint *data = con->data;
    bConstraintTarget *ct = list->first;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

static void rotlike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bRotateLikeConstraint *data = con->data;
  bConstraintTarget *ct = targets->first;

  if (VALID_CONS_TARGET(ct)) {
    float loc[3], size[3], oldrot[3][3], newrot[3][3];
    float eul[3], obeul[3], defeul[3];

    mat4_to_loc_rot_size(loc, oldrot, size, cob->matrix);

    /* Select the Euler rotation order, defaulting to the owner. */
    short rot_order = cob->rotOrder;

    if (data->euler_order != CONSTRAINT_EULER_AUTO) {
      rot_order = data->euler_order;
    }

    /* To allow compatible rotations, must get both rotations in the order of the owner... */
    mat4_to_eulO(obeul, rot_order, cob->matrix);
    /* We must get compatible eulers from the beginning because
     * some of them can be modified below (see bug #21875).
     * Additionally, since this constraint is based on euler rotation math, it doesn't work well
     * with shear. The Y axis is chosen as the main axis when we orthogonalize the matrix because
     * constraints are used most commonly on bones. */
    float mat[4][4];
    copy_m4_m4(mat, ct->matrix);
    orthogonalize_m4_stable(mat, 1, true);
    mat4_to_compatible_eulO(eul, obeul, rot_order, mat);

    /* Prepare the copied euler rotation. */
    bool legacy_offset = false;

    switch (data->mix_mode) {
      case ROTLIKE_MIX_OFFSET:
        legacy_offset = true;
        copy_v3_v3(defeul, obeul);
        break;

      case ROTLIKE_MIX_REPLACE:
        copy_v3_v3(defeul, obeul);
        break;

      default:
        zero_v3(defeul);
    }

    if ((data->flag & ROTLIKE_X) == 0) {
      eul[0] = defeul[0];
    }
    else {
      if (legacy_offset) {
        rotate_eulO(eul, rot_order, 'X', obeul[0]);
      }

      if (data->flag & ROTLIKE_X_INVERT) {
        eul[0] *= -1;
      }
    }

    if ((data->flag & ROTLIKE_Y) == 0) {
      eul[1] = defeul[1];
    }
    else {
      if (legacy_offset) {
        rotate_eulO(eul, rot_order, 'Y', obeul[1]);
      }

      if (data->flag & ROTLIKE_Y_INVERT) {
        eul[1] *= -1;
      }
    }

    if ((data->flag & ROTLIKE_Z) == 0) {
      eul[2] = defeul[2];
    }
    else {
      if (legacy_offset) {
        rotate_eulO(eul, rot_order, 'Z', obeul[2]);
      }

      if (data->flag & ROTLIKE_Z_INVERT) {
        eul[2] *= -1;
      }
    }

    /* Add the euler components together if needed. */
    if (data->mix_mode == ROTLIKE_MIX_ADD) {
      add_v3_v3(eul, obeul);
    }

    /* Good to make eulers compatible again,
     * since we don't know how much they were changed above. */
    compatible_eul(eul, obeul);
    eulO_to_mat3(newrot, eul, rot_order);

    /* Mix the rotation matrices: */
    switch (data->mix_mode) {
      case ROTLIKE_MIX_REPLACE:
      case ROTLIKE_MIX_OFFSET:
      case ROTLIKE_MIX_ADD:
        break;

      case ROTLIKE_MIX_BEFORE:
        mul_m3_m3m3(newrot, newrot, oldrot);
        break;

      case ROTLIKE_MIX_AFTER:
        mul_m3_m3m3(newrot, oldrot, newrot);
        break;

      default:
        BLI_assert(false);
    }

    loc_rot_size_to_mat4(cob->matrix, loc, newrot, size);
  }
}

static bConstraintTypeInfo CTI_ROTLIKE = {
    /*type*/ CONSTRAINT_TYPE_ROTLIKE,
    /*size*/ sizeof(bRotateLikeConstraint),
    /*name*/ N_("Copy Rotation"),
    /*structName*/ "bRotateLikeConstraint",
    /*free_data*/ NULL,
    /*id_looper*/ rotlike_id_looper,
    /*copy_data*/ NULL,
    /*new_data*/ rotlike_new_data,
    /*get_constraint_targets*/ rotlike_get_tars,
    /*flush_constraint_targets*/ rotlike_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ rotlike_evaluate,
};


/* constraint.c:4400-4564 */
/* --------- Damped Track ---------- */

static void damptrack_new_data(void *cdata)
{
  bDampTrackConstraint *data = (bDampTrackConstraint *)cdata;

  data->trackflag = TRACK_Y;
}

static void damptrack_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
  bDampTrackConstraint *data = con->data;

  /* target only */
  func(con, (ID **)&data->tar, false, userdata);
}

static int damptrack_get_tars(bConstraint *con, ListBase *list)
{
  if (con && list) {
    bDampTrackConstraint *data = con->data;
    bConstraintTarget *ct;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);

    return 1;
  }

  return 0;
}

static void damptrack_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
  if (con && list) {
    bDampTrackConstraint *data = con->data;
    bConstraintTarget *ct = list->first;

    /* the following macro is used for all standard single-target constraints */
    SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
  }
}

/* array of direction vectors for the tracking flags */
static const float track_dir_vecs[6][3] = {
    {+1, 0, 0},
    {0, +1, 0},
    {0, 0, +1}, /* TRACK_X,  TRACK_Y,  TRACK_Z */
    {-1, 0, 0},
    {0, -1, 0},
    {0, 0, -1} /* TRACK_NX, TRACK_NY, TRACK_NZ */
};

static void damptrack_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
  bDampTrackConstraint *data = con->data;
  bConstraintTarget *ct = targets->first;

  if (VALID_CONS_TARGET(ct)) {
    float tarvec[3];

    /* find the (unit) direction vector going from the owner to the target */
    sub_v3_v3v3(tarvec, ct->matrix[3], cob->matrix[3]);

    damptrack_do_transform(cob->matrix, tarvec, data->trackflag);
  }
}

static void damptrack_do_transform(float matrix[4][4], const float tarvec_in[3], int track_axis)
{
  /* find the (unit) direction vector going from the owner to the target */
  float tarvec[3];

  if (normalize_v3_v3(tarvec, tarvec_in) != 0.0f) {
    float obvec[3], obloc[3];
    float raxis[3], rangle;
    float rmat[3][3], tmat[4][4];

    /* find the (unit) direction that the axis we're interested in currently points
     * - mul_mat3_m4_v3() only takes the 3x3 (rotation+scaling) components of the 4x4 matrix
     * - the normalization step at the end should take care of any unwanted scaling
     *   left over in the 3x3 matrix we used
     */
    copy_v3_v3(obvec, track_dir_vecs[track_axis]);
    mul_mat3_m4_v3(matrix, obvec);

    if (normalize_v3(obvec) == 0.0f) {
      /* exceptional case - just use the track vector as appropriate */
      copy_v3_v3(obvec, track_dir_vecs[track_axis]);
    }

    copy_v3_v3(obloc, matrix[3]);

    /* determine the axis-angle rotation, which represents the smallest possible rotation
     * between the two rotation vectors (i.e. the 'damping' referred to in the name)
     * - we take this to be the rotation around the normal axis/vector to the plane defined
     *   by the current and destination vectors, which will 'map' the current axis to the
     *   destination vector
     * - the min/max wrappers around (obvec . tarvec) result (stored temporarily in rangle)
     *   are used to ensure that the smallest angle is chosen
     */
    cross_v3_v3v3_hi_prec(raxis, obvec, tarvec);

    rangle = dot_v3v3(obvec, tarvec);
    rangle = acosf(max_ff(-1.0f, min_ff(1.0f, rangle)));

    /* construct rotation matrix from the axis-angle rotation found above
     * - this call takes care to make sure that the axis provided is a unit vector first
     */
    float norm = normalize_v3(raxis);

    if (norm < FLT_EPSILON) {
      /* if dot product is nonzero, while cross is zero, we have two opposite vectors!
       * - this is an ambiguity in the math that needs to be resolved arbitrarily,
       *   or there will be a case where damped track strangely does nothing
       * - to do that, rotate around a different local axis
       */
      float tmpvec[3];

      if (fabsf(rangle) < M_PI - 0.01f) {
        return;
      }

      rangle = M_PI;
      copy_v3_v3(tmpvec, track_dir_vecs[(track_axis + 1) % 6]);
      mul_mat3_m4_v3(matrix, tmpvec);
      cross_v3_v3v3(raxis, obvec, tmpvec);

      if (normalize_v3(raxis) == 0.0f) {
        return;
      }
    }
    else if (norm < 0.1f) {
      /* Near 0 and Pi `arcsin` has way better precision than `arccos`. */
      rangle = (rangle > M_PI_2) ? M_PI - asinf(norm) : asinf(norm);
    }

    axis_angle_normalized_to_mat3(rmat, raxis, rangle);

    /* rotate the owner in the way defined by this rotation matrix, then reapply the location since
     * we may have destroyed that in the process of multiplying the matrix
     */
    unit_m4(tmat);
    mul_m4_m3m4(tmat, rmat, matrix); /* m1, m3, m2 */

    copy_m4_m4(matrix, tmat);
    copy_v3_v3(matrix[3], obloc);
  }
}

static bConstraintTypeInfo CTI_DAMPTRACK = {
    /*type*/ CONSTRAINT_TYPE_DAMPTRACK,
    /*size*/ sizeof(bDampTrackConstraint),
    /*name*/ N_("Damped Track"),
    /*structName*/ "bDampTrackConstraint",
    /*free_data*/ NULL,
    /*id_looper*/ damptrack_id_looper,
    /*copy_data*/ NULL,
    /*new_data*/ damptrack_new_data,
    /*get_constraint_targets*/ damptrack_get_tars,
    /*flush_constraint_targets*/ damptrack_flush_tars,
    /*get_target_matrix*/ default_get_tarmat,
    /*evaluate_constraint*/ damptrack_evaluate,
};


/* constraint.c:5451-5488, the types this rig supports */
static bConstraintTypeInfo *constraintsTypeInfo[NUM_CONSTRAINT_TYPES];
static short CTI_INIT = 1; /* when non-zero, the list needs to be updated */

static void constraints_init_typeinfo(void)
{
  constraintsTypeInfo[CONSTRAINT_TYPE_CHILDOF] = &CTI_CHILDOF;     /* ChildOf Constraint */
  constraintsTypeInfo[CONSTRAINT_TYPE_KINEMATIC] = &CTI_KINEMATIC; /* IK Constraint */
  constraintsTypeInfo[CONSTRAINT_TYPE_ROTLIMIT] = &CTI_ROTLIMIT;   /* Limit Rotation Constraint */
  constraintsTypeInfo[CONSTRAINT_TYPE_ROTLIKE] = &CTI_ROTLIKE;     /* Copy Rotation Constraint */
  constraintsTypeInfo[CONSTRAINT_TYPE_LOCLIKE] = &CTI_LOCLIKE;     /* Copy Location Constraint */
  constraintsTypeInfo[CONSTRAINT_TYPE_DAMPTRACK] = &CTI_DAMPTRACK; /* Damped TrackTo Constraint */
}

/* No ID user counts / libraries in Project Grease. */
#define id_us_min(id) ((void)(id))
#define id_us_plus(id) ((void)(id))
#define id_lib_extern(id) ((void)(id))
#undef ID_IS_LINKED
#define ID_IS_LINKED(id) ((void)(id), 0)

/* constraint.c:5490-5518 */
const bConstraintTypeInfo *BKE_constraint_typeinfo_from_type(int type)
{
  /* initialize the type-info list? */
  if (CTI_INIT) {
    constraints_init_typeinfo();
    CTI_INIT = 0;
  }

  /* only return for valid types */
  if ((type >= CONSTRAINT_TYPE_NULL) && (type < NUM_CONSTRAINT_TYPES)) {
    /* there shouldn't be any segfaults here... */
    return constraintsTypeInfo[type];
  }

  CLOG_WARN(&LOG, "No valid constraint type-info data available. Type = %i", type);

  return NULL;
}

const bConstraintTypeInfo *BKE_constraint_typeinfo_get(bConstraint *con)
{
  /* only return typeinfo for valid constraints */
  if (con) {
    return BKE_constraint_typeinfo_from_type(con->type);
  }

  return NULL;
}


/* constraint.c:5526-5618 */
/**
 * Helper function for #BKE_constraint_free_data() - unlinks references.
 */
static void con_unlink_refs_cb(bConstraint *UNUSED(con),
                               ID **idpoin,
                               bool is_reference,
                               void *UNUSED(userData))
{
  if (*idpoin && is_reference) {
    id_us_min(*idpoin);
  }
}

/** Helper function to invoke the id_looper callback, including custom space. */
static void con_invoke_id_looper(const bConstraintTypeInfo *cti,
                                 bConstraint *con,
                                 ConstraintIDFunc func,
                                 void *userdata)
{
  if (cti->id_looper) {
    cti->id_looper(con, func, userdata);
  }

  func(con, (ID **)&con->space_object, false, userdata);
}

void BKE_constraint_free_data_ex(bConstraint *con, bool do_id_user)
{
  if (con->data) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

    if (cti) {
      /* perform any special freeing constraint may have */
      if (cti->free_data) {
        cti->free_data(con);
      }

      /* unlink the referenced resources it uses */
      if (do_id_user) {
        con_invoke_id_looper(cti, con, con_unlink_refs_cb, NULL);
      }
    }

    /* free constraint data now */
    MEM_freeN(con->data);
  }
}

void BKE_constraint_free_data(bConstraint *con)
{
  BKE_constraint_free_data_ex(con, true);
}

void BKE_constraints_free_ex(ListBase *list, bool do_id_user)
{
  /* Free constraint data and also any extra data */
  LISTBASE_FOREACH (bConstraint *, con, list) {
    BKE_constraint_free_data_ex(con, do_id_user);
  }

  /* Free the whole list */
  BLI_freelistN(list);
}

void BKE_constraints_free(ListBase *list)
{
  BKE_constraints_free_ex(list, true);
}

bool BKE_constraint_remove(ListBase *list, bConstraint *con)
{
  if (con) {
    BKE_constraint_free_data(con);
    BLI_freelinkN(list, con);
    return true;
  }

  return false;
}

bool BKE_constraint_remove_ex(ListBase *list, Object *ob, bConstraint *con, bool clear_dep)
{
  const short type = con->type;
  if (BKE_constraint_remove(list, con)) {
    /* ITASC needs to be rebuilt once a constraint is removed #26920. */
    if (clear_dep && ELEM(type, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
      BIK_clear_data(ob->pose);
    }
    return true;
  }

  return false;
}

/* constraint.c:5743-5848 */

/* Creates a new constraint, initializes its data, and returns it */
static bConstraint *add_new_constraint_internal(const char *name, short type)
{
  bConstraint *con = MEM_callocN(sizeof(bConstraint), "Constraint");
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(type);
  const char *newName;

  /* Set up a generic constraint data-block. */
  con->type = type;
  con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;
  con->enforce = 1.0f;

  /* Only open the main panel when constraints are created, not the sub-panels. */
  con->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT;
  if (ELEM(type, CONSTRAINT_TYPE_ACTION, CONSTRAINT_TYPE_SPLINEIK)) {
    /* Expand the two sub-panels in the cases where the main panel barely has any properties. */
    con->ui_expand_flag |= UI_SUBPANEL_DATA_EXPAND_1 | UI_SUBPANEL_DATA_EXPAND_2;
  }

  /* Determine a basic name, and info */
  if (cti) {
    /* initialize constraint data */
    con->data = MEM_callocN(cti->size, cti->structName);

    /* only constraints that change any settings need this */
    if (cti->new_data) {
      cti->new_data(con->data);
    }

    /* if no name is provided, use the type of the constraint as the name */
    newName = (name && name[0]) ? name : DATA_(cti->name);
  }
  else {
    /* if no name is provided, use the generic "Const" name */
    /* NOTE: any constraint type that gets here really shouldn't get added... */
    newName = (name && name[0]) ? name : DATA_("Const");
  }

  /* copy the name */
  STRNCPY(con->name, newName);

  /* return the new constraint */
  return con;
}

/* Add a newly created constraint to the constraint list. */
static void add_new_constraint_to_list(Object *ob, bPoseChannel *pchan, bConstraint *con)
{
  ListBase *list;

  /* find the constraint stack - bone or object? */
  list = (pchan) ? (&pchan->constraints) : (&ob->constraints);

  if (list) {
    /* add new constraint to end of list of constraints before ensuring that it has a unique name
     * (otherwise unique-naming code will fail, since it assumes element exists in list)
     */
    BLI_addtail(list, con);
    BKE_constraint_unique_name(con, list);

    /* make this constraint the active one */
    BKE_constraints_active_set(list, con);
  }
}

/* if pchan is not NULL then assume we're adding a pose constraint */
static bConstraint *add_new_constraint(Object *ob,
                                       bPoseChannel *pchan,
                                       const char *name,
                                       short type)
{
  bConstraint *con;

  /* add the constraint */
  con = add_new_constraint_internal(name, type);

  add_new_constraint_to_list(ob, pchan, con);

  /* set type+owner specific immutable settings */
  /* TODO: does action constraint need anything here - i.e. spaceonce? */
  switch (type) {
    case CONSTRAINT_TYPE_CHILDOF: {
      /* if this constraint is being added to a posechannel, make sure
       * the constraint gets evaluated in pose-space */
      if (pchan) {
        con->ownspace = CONSTRAINT_SPACE_POSE;
        con->flag |= CONSTRAINT_SPACEONCE;
      }
      break;
    }
    case CONSTRAINT_TYPE_ACTION: {
      /* The Before or Split modes require computing in local space, but
       * for objects the Local space doesn't make sense (#78462, D6095 etc).
       * So only default to Before (Split) if the constraint is on a bone. */
      if (pchan) {
        bActionConstraint *data = con->data;
        data->mix_mode = ACTCON_MIX_BEFORE_SPLIT;
        con->ownspace = CONSTRAINT_SPACE_LOCAL;
      }
      break;
    }
  }

  return con;
}

/* constraint.c:5859-5871 */
/* ......... */

bConstraint *BKE_constraint_add_for_pose(Object *ob,
                                         bPoseChannel *pchan,
                                         const char *name,
                                         short type)
{
  if (pchan == NULL) {
    return NULL;
  }

  return add_new_constraint(ob, pchan, name, type);
}

/* constraint.c:5892-5997 */

/* helper for BKE_constraints_copy(), to be used for making sure that ID's are valid */
static void con_extern_cb(bConstraint *UNUSED(con),
                          ID **idpoin,
                          bool UNUSED(is_reference),
                          void *UNUSED(userData))
{
  if (*idpoin && ID_IS_LINKED(*idpoin)) {
    id_lib_extern(*idpoin);
  }
}

/**
 * Helper for #BKE_constraints_copy(),
 * to be used for making sure that user-counts of copied ID's are fixed up.
 */
static void con_fix_copied_refs_cb(bConstraint *UNUSED(con),
                                   ID **idpoin,
                                   bool is_reference,
                                   void *UNUSED(userData))
{
  /* Increment user-count if this is a reference type. */
  if ((*idpoin) && (is_reference)) {
    id_us_plus(*idpoin);
  }
}

/** Copies a single constraint's data (\a dst must already be a shallow copy of \a src). */
static void constraint_copy_data_ex(bConstraint *dst,
                                    bConstraint *src,
                                    const int flag,
                                    const bool do_extern)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(src);

  /* make a new copy of the constraint's data */
  dst->data = MEM_dupallocN(dst->data);

  /* only do specific constraints if required */
  if (cti) {
    /* perform custom copying operations if needed */
    if (cti->copy_data) {
      cti->copy_data(dst, src);
    }

    /* Fix user-counts for all referenced data that need it. */
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      con_invoke_id_looper(cti, dst, con_fix_copied_refs_cb, NULL);
    }

    /* For proxies we don't want to make external. */
    if (do_extern) {
      /* go over used ID-links for this constraint to ensure that they are valid for proxies */
      con_invoke_id_looper(cti, dst, con_extern_cb, NULL);
    }
  }
}

bConstraint *BKE_constraint_duplicate_ex(bConstraint *src, const int flag, const bool do_extern)
{
  bConstraint *dst = MEM_dupallocN(src);
  constraint_copy_data_ex(dst, src, flag, do_extern);
  dst->next = dst->prev = NULL;
  return dst;
}

bConstraint *BKE_constraint_copy_for_pose(Object *ob, bPoseChannel *pchan, bConstraint *src)
{
  if (pchan == NULL) {
    return NULL;
  }

  bConstraint *new_con = BKE_constraint_duplicate_ex(src, 0, !ID_IS_LINKED(ob));
  add_new_constraint_to_list(ob, pchan, new_con);
  return new_con;
}

bConstraint *BKE_constraint_copy_for_object(Object *ob, bConstraint *src)
{
  bConstraint *new_con = BKE_constraint_duplicate_ex(src, 0, !ID_IS_LINKED(ob));
  add_new_constraint_to_list(ob, NULL, new_con);
  return new_con;
}

void BKE_constraints_copy_ex(ListBase *dst, const ListBase *src, const int flag, bool do_extern)
{
  bConstraint *con, *srccon;

  BLI_listbase_clear(dst);
  BLI_duplicatelist(dst, src);

  for (con = dst->first, srccon = src->first; con && srccon;
       srccon = srccon->next, con = con->next) {
    constraint_copy_data_ex(con, srccon, flag, do_extern);
    if ((flag & LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG) == 0) {
      con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;
    }
  }
}

void BKE_constraints_copy(ListBase *dst, const ListBase *src, bool do_extern)
{
  BKE_constraints_copy_ex(dst, src, 0, do_extern);
}

/* ......... */

/* constraint.c:5999-6033 */
bConstraint *BKE_constraints_find_name(ListBase *list, const char *name)
{
  return BLI_findstring(list, name, offsetof(bConstraint, name));
}

bConstraint *BKE_constraints_active_get(ListBase *list)
{

  /* search for the first constraint with the 'active' flag set */
  if (list) {
    LISTBASE_FOREACH (bConstraint *, con, list) {
      if (con->flag & CONSTRAINT_ACTIVE) {
        return con;
      }
    }
  }

  /* no active constraint found */
  return NULL;
}

void BKE_constraints_active_set(ListBase *list, bConstraint *con)
{

  if (list) {
    LISTBASE_FOREACH (bConstraint *, con_iter, list) {
      if (con_iter == con) {
        con_iter->flag |= CONSTRAINT_ACTIVE;
      }
      else {
        con_iter->flag &= ~CONSTRAINT_ACTIVE;
      }
    }
  }
}

/* constraint.c:6217-6438 */
void BKE_constraint_target_matrix_get(struct Depsgraph *depsgraph,
                                      Scene *scene,
                                      bConstraint *con,
                                      int index,
                                      short ownertype,
                                      void *ownerdata,
                                      float mat[4][4],
                                      float ctime)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
  ListBase targets = {NULL, NULL};
  bConstraintOb *cob;
  bConstraintTarget *ct;

  if (cti && cti->get_constraint_targets) {
    /* make 'constraint-ob' */
    cob = MEM_callocN(sizeof(bConstraintOb), "tempConstraintOb");
    cob->type = ownertype;
    cob->scene = scene;
    cob->depsgraph = depsgraph;
    switch (ownertype) {
      case CONSTRAINT_OBTYPE_OBJECT: /* it is usually this case */
      {
        cob->ob = (Object *)ownerdata;
        cob->pchan = NULL;
        if (cob->ob) {
          copy_m4_m4(cob->matrix, cob->ob->object_to_world);
          copy_m4_m4(cob->startmat, cob->matrix);
        }
        else {
          unit_m4(cob->matrix);
          unit_m4(cob->startmat);
        }
        break;
      }
      case CONSTRAINT_OBTYPE_BONE: /* this may occur in some cases */
      {
        cob->ob = NULL; /* this might not work at all :/ */
        cob->pchan = (bPoseChannel *)ownerdata;
        if (cob->pchan) {
          copy_m4_m4(cob->matrix, cob->pchan->pose_mat);
          copy_m4_m4(cob->startmat, cob->matrix);
        }
        else {
          unit_m4(cob->matrix);
          unit_m4(cob->startmat);
        }
        break;
      }
    }

    /* Initialize the custom space for use in calculating the matrices. */
    BKE_constraint_custom_object_space_init(cob, con);

    /* get targets - we only need the first one though (and there should only be one) */
    cti->get_constraint_targets(con, &targets);

    /* only calculate the target matrix on the first target */
    ct = BLI_findlink(&targets, index);

    if (ct) {
      if (cti->get_target_matrix) {
        cti->get_target_matrix(depsgraph, con, cob, ct, ctime);
      }
      copy_m4_m4(mat, ct->matrix);
    }

    /* free targets + 'constraint-ob' */
    if (cti->flush_constraint_targets) {
      cti->flush_constraint_targets(con, &targets, 1);
    }
    MEM_freeN(cob);
  }
  else {
    /* invalid constraint - perhaps... */
    unit_m4(mat);
  }
}

void BKE_constraint_targets_for_solving_get(struct Depsgraph *depsgraph,
                                            bConstraint *con,
                                            bConstraintOb *cob,
                                            ListBase *targets,
                                            float ctime)
{
  const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

  if (cti && cti->get_constraint_targets) {
    bConstraintTarget *ct;

    /* get targets
     * - constraints should use ct->matrix, not directly accessing values
     * - ct->matrix members have not yet been calculated here!
     */
    cti->get_constraint_targets(con, targets);

    /* The Armature constraint doesn't need ct->matrix for evaluate at all. */
    if (ELEM(cti->type, CONSTRAINT_TYPE_ARMATURE)) {
      return;
    }

    /* set matrices
     * - calculate if possible, otherwise just initialize as identity matrix
     */
    if (cti->get_target_matrix) {
      for (ct = targets->first; ct; ct = ct->next) {
        cti->get_target_matrix(depsgraph, con, cob, ct, ctime);
      }
    }
    else {
      for (ct = targets->first; ct; ct = ct->next) {
        unit_m4(ct->matrix);
      }
    }
  }
}

void BKE_constraint_custom_object_space_init(bConstraintOb *cob, bConstraint *con)
{
  if (con && con->space_object && is_custom_space_needed(con)) {
    /* Basically default_get_tarmat but without the unused parameters. */
    constraint_target_to_mat4(con->space_object,
                              con->space_subtarget,
                              NULL,
                              cob->space_obj_world_matrix,
                              CONSTRAINT_SPACE_WORLD,
                              CONSTRAINT_SPACE_WORLD,
                              0,
                              0);

    return;
  }

  unit_m4(cob->space_obj_world_matrix);
}

/* ---------- Evaluation ----------- */

void BKE_constraints_solve(struct Depsgraph *depsgraph,
                           ListBase *conlist,
                           bConstraintOb *cob,
                           float ctime)
{
  bConstraint *con;
  float oldmat[4][4];
  float enf;

  /* check that there is a valid constraint object to evaluate */
  if (cob == NULL) {
    return;
  }

  /* loop over available constraints, solving and blending them */
  for (con = conlist->first; con; con = con->next) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    ListBase targets = {NULL, NULL};

    /* these we can skip completely (invalid constraints...) */
    if (cti == NULL) {
      continue;
    }
    if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
      continue;
    }
    /* these constraints can't be evaluated anyway */
    if (cti->evaluate_constraint == NULL) {
      continue;
    }
    /* influence == 0 should be ignored */
    if (con->enforce == 0.0f) {
      continue;
    }

    /* influence of constraint
     * - value should have been set from animation data already
     */
    enf = con->enforce;

    /* Initialize the custom space for use in calculating the matrices. */
    BKE_constraint_custom_object_space_init(cob, con);

    /* make copy of world-space matrix pre-constraint for use with blending later */
    copy_m4_m4(oldmat, cob->matrix);

    /* move owner matrix into right space */
    BKE_constraint_mat_convertspace(
        cob->ob, cob->pchan, cob, cob->matrix, CONSTRAINT_SPACE_WORLD, con->ownspace, false);

    /* prepare targets for constraint solving */
    BKE_constraint_targets_for_solving_get(depsgraph, con, cob, &targets, ctime);

    /* Solve the constraint and put result in cob->matrix */
    cti->evaluate_constraint(con, cob, &targets);

    /* clear targets after use
     * - this should free temp targets but no data should be copied back
     *   as constraints may have done some nasty things to it...
     */
    if (cti->flush_constraint_targets) {
      cti->flush_constraint_targets(con, &targets, 1);
    }

    /* move owner back into world-space for next constraint/other business */
    if ((con->flag & CONSTRAINT_SPACEONCE) == 0) {
      BKE_constraint_mat_convertspace(
          cob->ob, cob->pchan, cob, cob->matrix, con->ownspace, CONSTRAINT_SPACE_WORLD, false);
    }

    /* Interpolate the enforcement, to blend result of constraint into final owner transform
     * - all this happens in world-space to prevent any weirdness creeping in
     *   (#26014 and #25725), since some constraints may not convert the solution back to the input
     *   space before blending but all are guaranteed to end up in good "world-space" result.
     */
    /* NOTE: all kind of stuff here before (caused trouble), much easier to just interpolate,
     * or did I miss something? -jahka (r.32105) */
    if (enf < 1.0f) {
      float solution[4][4];
      copy_m4_m4(solution, cob->matrix);
      interp_m4_m4m4(cob->matrix, oldmat, solution, enf);
    }
  }
}

/* ---------------------------------------------------------------------------------------- */
/* action.c:1193-1258 BKE_pose_update_constraint_flags (Follow Path is not registered here) */

void BKE_pose_update_constraint_flags(bPose *pose)
{
  bPoseChannel *pchan, *parchan;
  bConstraint *con;

  /* clear */
  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    pchan->constflag = 0;
  }
  pose->flag &= ~POSE_CONSTRAINTS_TIMEDEPEND;

  /* detect */
  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    for (con = pchan->constraints.first; con; con = con->next) {
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        bKinematicConstraint *data = (bKinematicConstraint *)con->data;

        pchan->constflag |= PCHAN_HAS_IK;

        if (data->tar == NULL || (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0)) {
          pchan->constflag |= PCHAN_HAS_TARGET;
        }

        /* negative rootbone = recalc rootbone index. used in do_versions */
        if (data->rootbone < 0) {
          data->rootbone = 0;

          if (data->flag & CONSTRAINT_IK_TIP) {
            parchan = pchan;
          }
          else {
            parchan = pchan->parent;
          }

          while (parchan) {
            data->rootbone++;
            if ((parchan->bone->flag & BONE_CONNECTED) == 0) {
              break;
            }
            parchan = parchan->parent;
          }
        }
      }
      else if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
        pchan->constflag |= PCHAN_HAS_SPLINEIK;
      }
      else {
        pchan->constflag |= PCHAN_HAS_CONST;
      }
    }
  }
  pose->flag &= ~POSE_CONSTRAINTS_NEED_UPDATE_FLAGS;
}

/* ---------------------------------------------------------------------------------------- */
/* ikplugin_api.c: the "Standard" plugin (iksolver_plugin.c) is the only one (iTaSC is not built) */

void BIK_init_tree(struct Depsgraph *depsgraph, struct Scene *scene, Object *ob, float ctime)
{
  iksolver_initialize_tree(depsgraph, scene, ob, ctime);
}

void BIK_execute_tree(
    struct Depsgraph *depsgraph, struct Scene *scene, Object *ob, bPoseChannel *pchan, float ctime)
{
  iksolver_execute_tree(depsgraph, scene, ob, pchan, ctime);
}

void BIK_release_tree(struct Scene *scene, Object *ob, float ctime)
{
  iksolver_release_tree(scene, ob, ctime);
}

void BIK_clear_data(struct bPose *pose) { iksolver_clear_data(pose); }

/* ---------------------------------------------------------------------------------------- */
/* armature.c:2480-2542 BKE_pose_where_is_bone */

void BKE_pose_where_is_bone(struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            Object *ob,
                            bPoseChannel *pchan,
                            float ctime,
                            bool do_extra)
{
  /* This gives a chan_mat with actions (F-Curve) results. */
  if (do_extra) {
    BKE_pchan_calc_mat(pchan);
  }
  else {
    unit_m4(pchan->chan_mat);
  }

  /* Construct the posemat based on PoseChannels, that we do before applying constraints. */
  /* pose_mat(b) = pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b) */
  BKE_armature_mat_bone_to_pose(pchan, pchan->chan_mat, pchan->pose_mat);

  /* Only rootbones get the cyclic offset (unless user doesn't want that). */
  /* XXX That could be a problem for snapping and other "reverse transform" features... */
  if (!pchan->parent) {
    if ((pchan->bone->flag & BONE_NO_CYCLICOFFSET) == 0) {
      add_v3_v3(pchan->pose_mat[3], ob->pose->cyclic_offset);
    }
  }

  if (do_extra) {
    /* Do constraints */
    if (pchan->constraints.first) {
      bConstraintOb *cob;
      float vec[3];

      /* make a copy of location of PoseChannel for later */
      copy_v3_v3(vec, pchan->pose_mat[3]);

      /* prepare PoseChannel for Constraint solving
       * - makes a copy of matrix, and creates temporary struct to use
       */
      cob = BKE_constraints_make_evalob(depsgraph, scene, ob, pchan, CONSTRAINT_OBTYPE_BONE);

      /* Solve PoseChannel's Constraints */

      /* ctime doesn't alter objects. */
      BKE_constraints_solve(depsgraph, &pchan->constraints, cob, ctime);

      /* cleanup after Constraint Solving
       * - applies matrix back to pchan, and frees temporary struct used
       */
      BKE_constraints_clear_evalob(cob);

      /* prevent constraints breaking a chain */
      if (pchan->bone->flag & BONE_CONNECTED) {
        copy_v3_v3(pchan->pose_mat[3], vec);
      }
    }
  }

  /* calculate head */
  copy_v3_v3(pchan->pose_head, pchan->pose_mat[3]);
  /* calculate tail */
  BKE_pose_where_is_bone_tail(pchan);
}

/* armature.c:2544-2620 BKE_pose_where_is for the rig object (no Scene: ctime only reaches the
 * constraint callbacks, which ignore it; no Spline IK), then the deform dual quaternions that
 * BKE_pose_bone_done (armature_update.c) computes. ARM_RESTPOS reads the rest pose; edit mode
 * keeps evaluating the pose (Project Grease keeps arm->edbo all the time and skips the deform in
 * edit mode instead). */
void pg_bke_pose_where_is(Object *ob)
{
  bArmature *arm = ob->data;
  bPose *pose = ob->pose;
  bPoseChannel *pchan;
  float imat[4][4];
  const float ctime = 0.0f;

  BKE_pose_update_constraint_flags(pose);

  if (arm->flag & ARM_RESTPOS) {
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      Bone *bone = pchan->bone;
      if (bone) {
        copy_m4_m4(pchan->pose_mat, bone->arm_mat);
        copy_v3_v3(pchan->pose_head, bone->arm_head);
        copy_v3_v3(pchan->pose_tail, bone->arm_tail);
      }
    }
  }
  else {
    invert_m4_m4(ob->world_to_object, ob->object_to_world); /* world_to_object is needed */

    /* 1. clear flags */
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      pchan->flag &= ~(POSE_DONE | POSE_CHAIN | POSE_IKTREE | POSE_IKSPLINE);
    }

    /* 2a. construct the IK tree (standard IK) */
    BIK_init_tree(NULL, NULL, ob, ctime);

    /* 3. the main loop, channels are already hierarchical sorted from root to children */
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      /* 4a. if we find an IK root, we handle it separated */
      if (pchan->flag & POSE_IKTREE) {
        BIK_execute_tree(NULL, NULL, ob, pchan, ctime);
      }
      /* 5. otherwise just call the normal solver */
      else if (!(pchan->flag & POSE_DONE)) {
        BKE_pose_where_is_bone(NULL, NULL, ob, pchan, ctime, 1);
      }
    }
    /* 6. release the IK tree */
    BIK_release_tree(NULL, ob, ctime);
  }

  /* calculating deform matrices */
  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->bone) {
      invert_m4_m4(imat, pchan->bone->arm_mat);
      mul_m4_m4m4(pchan->chan_mat, pchan->pose_mat, imat);
      if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
        mat4_to_dquat(&pchan->runtime.deform_dual_quat, pchan->bone->arm_mat, pchan->chan_mat);
      }
    }
  }
}

/* ---------------------------------------------------------------------------------------- */
/* Project Grease rig API (project_grease_constraint.h)                                      */

#include "project_grease_armature.h"
#include "project_grease_armature_intern.h"

void pg_con_list_free(ListBase *list) { BKE_constraints_free_ex(list, false); }

void pg_con_list_copy(ListBase *dst, const ListBase *src, Object *owner)
{
  BKE_constraints_copy_ex(dst, src, LIB_ID_CREATE_NO_USER_REFCOUNT, false);
  if (!owner) return;
  /* the only target object is the rig itself */
  LISTBASE_FOREACH (bConstraint *, con, dst) {
    switch (con->type) {
      case CONSTRAINT_TYPE_KINEMATIC: {
        bKinematicConstraint *d = con->data;
        if (d->tar) d->tar = owner;
        if (d->poletar) d->poletar = owner;
        break;
      }
      case CONSTRAINT_TYPE_CHILDOF: { bChildOfConstraint *d = con->data; if (d->tar) d->tar = owner; break; }
      case CONSTRAINT_TYPE_ROTLIKE: { bRotateLikeConstraint *d = con->data; if (d->tar) d->tar = owner; break; }
      case CONSTRAINT_TYPE_LOCLIKE: { bLocateLikeConstraint *d = con->data; if (d->tar) d->tar = owner; break; }
      case CONSTRAINT_TYPE_DAMPTRACK: { bDampTrackConstraint *d = con->data; if (d->tar) d->tar = owner; break; }
      default: break;
    }
    con->space_object = NULL;
  }
}

static bPoseChannel *con_pchan(const PGArmature *a, int bone)
{
  const char *name = a ? pg_arm_bone_name(a, bone) : NULL;
  return name ? BKE_pose_channel_find_name(&a->pose, name) : NULL;
}

bConstraint *pg_arm_con_at(const PGArmature *a, int bone, int index)
{
  bPoseChannel *pchan = con_pchan(a, bone);
  return pchan && index >= 0 ? BLI_findlink(&pchan->constraints, index) : NULL;
}

int pg_arm_con_count(const PGArmature *a, int bone)
{
  bPoseChannel *pchan = con_pchan(a, bone);
  return pchan ? BLI_listbase_count(&pchan->constraints) : -1;
}

/* Target fields of the supported types: (tar, subtarget) for which 0 / 1. */
static bool con_target_fields(bConstraint *con, int which, Object ***r_tar, char **r_sub)
{
  switch (con->type) {
    case CONSTRAINT_TYPE_KINEMATIC: {
      bKinematicConstraint *d = con->data;
      *r_tar = which ? &d->poletar : &d->tar;
      *r_sub = which ? d->polesubtarget : d->subtarget;
      return true;
    }
    case CONSTRAINT_TYPE_CHILDOF: { bChildOfConstraint *d = con->data; *r_tar = &d->tar; *r_sub = d->subtarget; return which == 0; }
    case CONSTRAINT_TYPE_ROTLIKE: { bRotateLikeConstraint *d = con->data; *r_tar = &d->tar; *r_sub = d->subtarget; return which == 0; }
    case CONSTRAINT_TYPE_LOCLIKE: { bLocateLikeConstraint *d = con->data; *r_tar = &d->tar; *r_sub = d->subtarget; return which == 0; }
    case CONSTRAINT_TYPE_DAMPTRACK: { bDampTrackConstraint *d = con->data; *r_tar = &d->tar; *r_sub = d->subtarget; return which == 0; }
    default: return false;
  }
}

int pg_arm_con_add(PGArmature *a, int bone, int type, const char *name)
{
  bPoseChannel *pchan = con_pchan(a, bone);
  if (!pchan || type <= 0 || type >= NUM_CONSTRAINT_TYPES || !BKE_constraint_typeinfo_from_type(type)) {
    return -1;
  }
  /* constraint_add_exec with setTarget off: no target until pg_arm_con_set_target (so Child Of's
   * automatic Set Inverse happens with its target, as in Blender) */
  bConstraint *con = BKE_constraint_add_for_pose(&a->ob, pchan, name, (short)type);
  pg_arm_update(a);
  return BLI_findindex(&pchan->constraints, con);
}

int pg_arm_con_remove(PGArmature *a, int bone, int index)
{
  bPoseChannel *pchan = con_pchan(a, bone);
  bConstraint *con = pg_arm_con_at(a, bone, index);
  if (!con) return 0;
  BKE_constraint_remove_ex(&pchan->constraints, &a->ob, con, true);
  /* constraint_delete_exec: the new last constraint becomes active */
  if (pchan->constraints.last) BKE_constraints_active_set(&pchan->constraints, pchan->constraints.last);
  pg_arm_update(a);
  return 1;
}

int pg_arm_con_move(PGArmature *a, int bone, int index, int to)
{
  bPoseChannel *pchan = con_pchan(a, bone);
  bConstraint *con = pg_arm_con_at(a, bone, index);
  const int count = pchan ? BLI_listbase_count(&pchan->constraints) : 0;
  if (!con || to < 0 || to >= count) return 0;
  BLI_listbase_link_move(&pchan->constraints, con, to - index);
  pg_arm_update(a);
  return 1;
}

int pg_arm_con_set_target(PGArmature *a, int bone, int index, int which, const char *subtarget)
{
  bConstraint *con = pg_arm_con_at(a, bone, index);
  Object **tar;
  char *sub;
  if (!con || !con_target_fields(con, which, &tar, &sub)) return 0;
  if (!subtarget) {
    *tar = NULL;
    sub[0] = '\0';
  }
  else {
    *tar = &a->ob;
    BLI_strncpy(sub, subtarget, MAX_ID_NAME - 2);
  }
  pg_arm_update(a);
  return 1;
}

static bool space_ok(int s)
{
  return ELEM(s, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_LOCAL,
              CONSTRAINT_SPACE_PARLOCAL, CONSTRAINT_SPACE_OWNLOCAL);
}

static float vget(const float *v, int n, int i, float def) { return i < n && isfinite(v[i]) ? v[i] : def; }

int pg_arm_con_set(PGArmature *a, int bone, int index, const float *v, int n)
{
  bConstraint *con = pg_arm_con_at(a, bone, index);
  if (!con || !v || n < 1) return 0;
  /* RNA ranges (rna_constraint.cc) */
  con->enforce = clamp_f(vget(v, n, 0, con->enforce), 0.0f, 1.0f);
  const int own = (int)vget(v, n, 1, con->ownspace), tars = (int)vget(v, n, 2, con->tarspace);
  if (space_ok(own)) con->ownspace = (char)own;
  if (space_ok(tars)) con->tarspace = (char)tars;
  con->headtail = clamp_f(vget(v, n, 3, con->headtail), 0.0f, 1.0f);
  SET_FLAG_FROM_TEST(con->flag, vget(v, n, 4, (con->flag & CONSTRAINT_OFF) != 0) != 0.0f, CONSTRAINT_OFF);
  const float *t = v + PG_CON_COMMON;
  const int m = n - PG_CON_COMMON;
  switch (con->type) {
    case CONSTRAINT_TYPE_KINEMATIC: {
      bKinematicConstraint *d = con->data;
      d->flag = (short)vget(t, m, 0, d->flag);
      d->rootbone = (short)clamp_i((int)vget(t, m, 1, d->rootbone), 0, 255);
      d->iterations = (short)clamp_i((int)vget(t, m, 2, d->iterations), 0, 10000);
      d->poleangle = clamp_f(vget(t, m, 3, d->poleangle), -M_PI, M_PI);
      d->weight = clamp_f(vget(t, m, 4, d->weight), 0.01f, 1.0f);
      d->orientweight = clamp_f(vget(t, m, 5, d->orientweight), 0.01f, 1.0f);
      break;
    }
    case CONSTRAINT_TYPE_ROTLIKE: {
      bRotateLikeConstraint *d = con->data;
      d->flag = (int)vget(t, m, 0, d->flag);
      d->euler_order = (char)clamp_i((int)vget(t, m, 1, d->euler_order), CONSTRAINT_EULER_AUTO, CONSTRAINT_EULER_ZYX);
      d->mix_mode = (char)clamp_i((int)vget(t, m, 2, d->mix_mode), ROTLIKE_MIX_REPLACE, ROTLIKE_MIX_AFTER);
      break;
    }
    case CONSTRAINT_TYPE_LOCLIKE: {
      bLocateLikeConstraint *d = con->data;
      d->flag = (int)vget(t, m, 0, d->flag);
      break;
    }
    case CONSTRAINT_TYPE_DAMPTRACK: {
      bDampTrackConstraint *d = con->data;
      d->trackflag = clamp_i((int)vget(t, m, 0, d->trackflag), TRACK_X, TRACK_nZ);
      break;
    }
    case CONSTRAINT_TYPE_ROTLIMIT: {
      bRotLimitConstraint *d = con->data;
      d->flag = (short)vget(t, m, 0, d->flag);
      d->flag2 = (short)vget(t, m, 1, d->flag2);
      float *lim[6] = {&d->xmin, &d->xmax, &d->ymin, &d->ymax, &d->zmin, &d->zmax};
      for (int i = 0; i < 6; i++) *lim[i] = vget(t, m, 2 + i, *lim[i]);
      d->euler_order = (char)clamp_i((int)vget(t, m, 8, d->euler_order), CONSTRAINT_EULER_AUTO, CONSTRAINT_EULER_ZYX);
      break;
    }
    case CONSTRAINT_TYPE_CHILDOF: {
      bChildOfConstraint *d = con->data;
      d->flag = (int)vget(t, m, 0, d->flag);
      break;
    }
  }
  pg_arm_update(a);
  return 1;
}

int pg_arm_con_get(const PGArmature *a, int bone, int index, float *r, int cap)
{
  bConstraint *con = pg_arm_con_at(a, bone, index);
  if (!con) return 0;
  float v[PG_CON_MAX_VALUES];
  int n = 0;
  v[n++] = con->type;
  v[n++] = con->enforce;
  v[n++] = con->ownspace;
  v[n++] = con->tarspace;
  v[n++] = con->headtail;
  v[n++] = (con->flag & CONSTRAINT_OFF) ? 1.0f : 0.0f;
  switch (con->type) {
    case CONSTRAINT_TYPE_KINEMATIC: {
      bKinematicConstraint *d = con->data;
      v[n++] = d->flag; v[n++] = d->rootbone; v[n++] = d->iterations; v[n++] = d->poleangle;
      v[n++] = d->weight; v[n++] = d->orientweight;
      break;
    }
    case CONSTRAINT_TYPE_ROTLIKE: {
      bRotateLikeConstraint *d = con->data;
      v[n++] = d->flag; v[n++] = d->euler_order; v[n++] = d->mix_mode;
      break;
    }
    case CONSTRAINT_TYPE_LOCLIKE: v[n++] = ((bLocateLikeConstraint *)con->data)->flag; break;
    case CONSTRAINT_TYPE_DAMPTRACK: v[n++] = ((bDampTrackConstraint *)con->data)->trackflag; break;
    case CONSTRAINT_TYPE_ROTLIMIT: {
      bRotLimitConstraint *d = con->data;
      v[n++] = d->flag; v[n++] = d->flag2; v[n++] = d->xmin; v[n++] = d->xmax; v[n++] = d->ymin;
      v[n++] = d->ymax; v[n++] = d->zmin; v[n++] = d->zmax; v[n++] = d->euler_order;
      break;
    }
    case CONSTRAINT_TYPE_CHILDOF: {
      bChildOfConstraint *d = con->data;
      v[n++] = d->flag;
      memcpy(v + n, d->invmat, sizeof(float[16]));
      n += 16;
      break;
    }
  }
  if (r && cap >= n) memcpy(r, v, sizeof(float) * n);
  return n;
}

bool pg_con_target_get(const PGArmature *a, int bone, int index, int which, const char **r_sub)
{
  bConstraint *con = pg_arm_con_at(a, bone, index);
  Object **tar;
  char *sub;
  if (!con || !con_target_fields(con, which, &tar, &sub)) return false;
  *r_sub = *tar ? sub : NULL;
  return true;
}

int pg_arm_con_childof_inverse(PGArmature *a, int bone, int index, bool set)
{
  bConstraint *con = pg_arm_con_at(a, bone, index);
  if (!con || con->type != CONSTRAINT_TYPE_CHILDOF) return 0;
  bChildOfConstraint *data = con->data;
  if (set) {
    /* childof_set_inverse_exec: the next evaluation computes the inverse */
    data->flag |= CHILDOF_SET_INVERSE;
  }
  else {
    /* childof_clear_inverse_exec */
    data->flag &= ~CHILDOF_SET_INVERSE;
    unit_m4(data->invmat);
  }
  pg_arm_update(a);
  return 1;
}

void pg_con_rename_subtargets(PGArmature *a, const char *oldname, const char *newname)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &a->pose.chanbase) {
    LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
      for (int which = 0; which < 2; which++) {
        Object **tar;
        char *sub;
        if (con_target_fields(con, which, &tar, &sub) && *tar == &a->ob && STREQ(sub, oldname)) {
          BLI_strncpy(sub, newname, MAX_ID_NAME - 2);
        }
      }
    }
  }
}
