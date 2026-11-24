/* SPDX-License-Identifier: GPL-2.0-or-later
 * PGArmature layout, shared by project_grease_armature.c and its document IO. */
#pragma once
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"

struct PGArmature {
  bArmature arm;    /* arm.edbo points at edbo */
  ListBase edbo;    /* EditBone: the editable rest pose */
  bPose pose;       /* bPoseChannel, hierarchy order */
  ListBase curves;  /* FCurve of the action */
  ListBase copybuf; /* POSE_OT_copy buffer: bPoseChannel copies (POSE_KEY on the copied ones) */
  int mode;
  /* The rig object (DNA defaults, at the origin): owner and target of the bone constraints
   * (project_grease_constraint.c) and the ID the drivers read (project_grease_driver.c). */
  Object ob;
  ListBase props;   /* PGProp: custom float properties of the object and its bones */
  ListBase drivers; /* FCurve with a ChannelDriver (AnimData.drivers) */
  int show_in_front; /* Object.dtx OB_DRAW_IN_FRONT of the rig object (DNA default off) */
};
