/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Blender 3.6.23 armature, pose, F-Curve and armature-deform code used by the Project Grease rig
 * (project_grease_armature.c). Every BEGIN/END VERBATIM region is a byte-for-byte copy of the pinned
 * source (checked by tools/verify_blender_verbatim.py). The ADAPTED functions at the end replace
 * the parts that need the depsgraph, constraints, ID properties or an Object, and say what changed.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"
#include "BLI_easing.h"

#include "BLT_translation.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_userdef_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil_legacy.h"

#include "ED_keyframing.h"
#include "ED_armature.h"

#include "project_grease_armature_bke.h"
#include "project_grease_constraint.h"

/* armature_deform.c / armature_utils.c / gpencil_armature.c / pose_transform.c context the regions
 * below rely on. */
extern UserDef U;
typedef struct Mesh Mesh;
void bone_free(bArmature *arm, EditBone *bone);
/* fcurve.c / keyframing.c globals: CLOG is not linked on Android and G.debug is always 0 here. */
static const char LOG = 0;
#define CLOG_WARN(log, ...) ((void)(log))
#define CLOG_ERROR(log, ...) ((void)(log))
static struct { int debug; } G;
#define G_DEBUG 1
#define SMALL -1.0e-10
#define BEZKEYTYPE(bezt) ((bezt)->hide)

/* BEGIN VERBATIM source/blender/blenlib/intern/easing.c */
float BLI_easing_back_ease_in(
    float time, float begin, float change, float duration, float overshoot)
{
  time /= duration;
  return change * time * time * ((overshoot + 1) * time - overshoot) + begin;
}

float BLI_easing_back_ease_out(
    float time, float begin, float change, float duration, float overshoot)
{
  time = time / duration - 1;
  return change * (time * time * ((overshoot + 1) * time + overshoot) + 1) + begin;
}

float BLI_easing_back_ease_in_out(
    float time, float begin, float change, float duration, float overshoot)
{
  overshoot *= 1.525f;
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * (time * time * ((overshoot + 1) * time - overshoot)) + begin;
  }
  time -= 2.0f;
  return change / 2 * (time * time * ((overshoot + 1) * time + overshoot) + 2) + begin;
}

float BLI_easing_bounce_ease_out(float time, float begin, float change, float duration)
{
  time /= duration;
  if (time < (1 / 2.75f)) {
    return change * (7.5625f * time * time) + begin;
  }
  if (time < (2 / 2.75f)) {
    time -= (1.5f / 2.75f);
    return change * ((7.5625f * time) * time + 0.75f) + begin;
  }
  if (time < (2.5f / 2.75f)) {
    time -= (2.25f / 2.75f);
    return change * ((7.5625f * time) * time + 0.9375f) + begin;
  }
  time -= (2.625f / 2.75f);
  return change * ((7.5625f * time) * time + 0.984375f) + begin;
}

float BLI_easing_bounce_ease_in(float time, float begin, float change, float duration)
{
  return change - BLI_easing_bounce_ease_out(duration - time, 0, change, duration) + begin;
}

float BLI_easing_bounce_ease_in_out(float time, float begin, float change, float duration)
{
  if (time < duration / 2) {
    return BLI_easing_bounce_ease_in(time * 2, 0, change, duration) * 0.5f + begin;
  }
  return BLI_easing_bounce_ease_out(time * 2 - duration, 0, change, duration) * 0.5f +
         change * 0.5f + begin;
}

float BLI_easing_circ_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return -change * (sqrtf(1 - time * time) - 1) + begin;
}

float BLI_easing_circ_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return change * sqrtf(1 - time * time) + begin;
}

float BLI_easing_circ_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return -change / 2 * (sqrtf(1 - time * time) - 1) + begin;
  }
  time -= 2.0f;
  return change / 2 * (sqrtf(1 - time * time) + 1) + begin;
}

float BLI_easing_cubic_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time * time + begin;
}

float BLI_easing_cubic_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return change * (time * time * time + 1) + begin;
}

float BLI_easing_cubic_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time * time + begin;
  }
  time -= 2.0f;
  return change / 2 * (time * time * time + 2) + begin;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenlib/intern/easing.c */
static const float pow_min = 0.0009765625f; /* = 2^(-10) */
static const float pow_scale = 1.0f / (1.0f - 0.0009765625f);

float BLI_easing_expo_ease_in(float time, float begin, float change, float duration)
{
  if (time == 0.0) {
    return begin;
  }
  return change * (powf(2, 10 * (time / duration - 1)) - pow_min) * pow_scale + begin;
}

float BLI_easing_expo_ease_out(float time, float begin, float change, float duration)
{
  if (time == 0.0) {
    return begin;
  }
  return change * (1 - (powf(2, -10 * time / duration) - pow_min) * pow_scale) + begin;
}

float BLI_easing_expo_ease_in_out(float time, float begin, float change, float duration)
{
  float duration_half = duration / 2.0f;
  float change_half = change / 2.0f;
  if (time <= duration_half) {
    return BLI_easing_expo_ease_in(time, begin, change_half, duration_half);
  }
  return BLI_easing_expo_ease_out(
      time - duration_half, begin + change_half, change_half, duration_half);
}

float BLI_easing_linear_ease(float time, float begin, float change, float duration)
{
  return change * time / duration + begin;
}

float BLI_easing_quad_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time + begin;
}

float BLI_easing_quad_ease_out(float time, float begin, float change, float duration)
{
  time /= duration;
  return -change * time * (time - 2) + begin;
}

float BLI_easing_quad_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time + begin;
  }
  time -= 1.0f;
  return -change / 2 * (time * (time - 2) - 1) + begin;
}

float BLI_easing_quart_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time * time * time + begin;
}

float BLI_easing_quart_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return -change * (time * time * time * time - 1) + begin;
}

float BLI_easing_quart_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time * time * time + begin;
  }
  time -= 2.0f;
  return -change / 2 * (time * time * time * time - 2) + begin;
}

float BLI_easing_quint_ease_in(float time, float begin, float change, float duration)
{
  time /= duration;
  return change * time * time * time * time * time + begin;
}
float BLI_easing_quint_ease_out(float time, float begin, float change, float duration)
{
  time = time / duration - 1;
  return change * (time * time * time * time * time + 1) + begin;
}
float BLI_easing_quint_ease_in_out(float time, float begin, float change, float duration)
{
  if ((time /= duration / 2) < 1.0f) {
    return change / 2 * time * time * time * time * time + begin;
  }
  time -= 2.0f;
  return change / 2 * (time * time * time * time * time + 2) + begin;
}

float BLI_easing_sine_ease_in(float time, float begin, float change, float duration)
{
  return -change * cosf(time / duration * (float)M_PI_2) + change + begin;
}

float BLI_easing_sine_ease_out(float time, float begin, float change, float duration)
{
  return change * sinf(time / duration * (float)M_PI_2) + begin;
}

float BLI_easing_sine_ease_in_out(float time, float begin, float change, float duration)
{
  return -change / 2 * (cosf((float)M_PI * time / duration) - 1) + begin;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/armature.c */
/* Compute a set of bezier parameter values that produce approximately equally spaced points. */
static void equalize_cubic_bezier(const float control[4][3],
                                  int temp_segments,
                                  int final_segments,
                                  const float *segment_scales,
                                  float *r_t_points)
{
  float(*coords)[3] = BLI_array_alloca(coords, temp_segments + 1);
  float *pdist = BLI_array_alloca(pdist, temp_segments + 1);

  /* Compute the first pass of bezier point coordinates. */
  for (int i = 0; i < 3; i++) {
    BKE_curve_forward_diff_bezier(control[0][i],
                                  control[1][i],
                                  control[2][i],
                                  control[3][i],
                                  &coords[0][i],
                                  temp_segments,
                                  sizeof(*coords));
  }

  /* Calculate the length of the polyline at each point. */
  pdist[0] = 0.0f;

  for (int i = 0; i < temp_segments; i++) {
    pdist[i + 1] = pdist[i] + len_v3v3(coords[i], coords[i + 1]);
  }

  /* Go over distances and calculate new parameter values. */
  float dist_step = pdist[temp_segments];
  float dist = 0, sum = 0;

  for (int i = 0; i < final_segments; i++) {
    sum += segment_scales[i];
  }

  dist_step /= sum;

  r_t_points[0] = 0.0f;

  for (int i = 1, nr = 1; i <= final_segments; i++) {
    dist += segment_scales[i - 1] * dist_step;

    /* We're looking for location (distance) 'dist' in the array. */
    while ((nr < temp_segments) && (dist >= pdist[nr])) {
      nr++;
    }

    float fac = (pdist[nr] - dist) / (pdist[nr] - pdist[nr - 1]);

    r_t_points[i] = (nr - fac) / temp_segments;
  }

  r_t_points[final_segments] = 1.0f;
}

/* Evaluate bezier position and tangent at a specific parameter value
 * using the De Casteljau algorithm. */
static void evaluate_cubic_bezier(const float control[4][3],
                                  float t,
                                  float r_pos[3],
                                  float r_tangent[3])
{
  float layer1[3][3];
  interp_v3_v3v3(layer1[0], control[0], control[1], t);
  interp_v3_v3v3(layer1[1], control[1], control[2], t);
  interp_v3_v3v3(layer1[2], control[2], control[3], t);

  float layer2[2][3];
  interp_v3_v3v3(layer2[0], layer1[0], layer1[1], t);
  interp_v3_v3v3(layer2[1], layer1[1], layer1[2], t);

  sub_v3_v3v3(r_tangent, layer2[1], layer2[0]);
  madd_v3_v3v3fl(r_pos, layer2[0], r_tangent, t);
}

void BKE_pchan_bbone_handles_get(bPoseChannel *pchan, bPoseChannel **r_prev, bPoseChannel **r_next)
{
  if (pchan->bone->bbone_prev_type == BBONE_HANDLE_AUTO) {
    /* Use connected parent. */
    if (pchan->bone->flag & BONE_CONNECTED) {
      *r_prev = pchan->parent;
    }
    else {
      *r_prev = NULL;
    }
  }
  else {
    /* Use the provided bone as prev - leave blank to eliminate this effect altogether. */
    *r_prev = pchan->bbone_prev;
  }

  if (pchan->bone->bbone_next_type == BBONE_HANDLE_AUTO) {
    /* Use connected child. */
    *r_next = pchan->child;
  }
  else {
    /* Use the provided bone as next - leave blank to eliminate this effect altogether. */
    *r_next = pchan->bbone_next;
  }
}

void BKE_pchan_bbone_spline_params_get(struct bPoseChannel *pchan,
                                       const bool rest,
                                       struct BBoneSplineParameters *param)
{
  bPoseChannel *next, *prev;
  Bone *bone = pchan->bone;
  float imat[4][4], posemat[4][4], tmpmat[4][4];
  float delta[3];

  memset(param, 0, sizeof(*param));

  param->segments = bone->segments;
  param->length = bone->length;

  if (!rest) {
    float scale[3];

    /* Check if we need to take non-uniform bone scaling into account. */
    mat4_to_size(scale, pchan->pose_mat);

    if (fabsf(scale[0] - scale[1]) > 1e-6f || fabsf(scale[1] - scale[2]) > 1e-6f) {
      param->do_scale = true;
      copy_v3_v3(param->scale, scale);
    }
  }

  BKE_pchan_bbone_handles_get(pchan, &prev, &next);

  /* Find the handle points, since this is inside bone space, the
   * first point = (0, 0, 0)
   * last point =  (0, length, 0) */
  if (rest) {
    invert_m4_m4(imat, pchan->bone->arm_mat);
  }
  else if (param->do_scale) {
    copy_m4_m4(posemat, pchan->pose_mat);
    normalize_m4(posemat);
    invert_m4_m4(imat, posemat);
  }
  else {
    invert_m4_m4(imat, pchan->pose_mat);
  }

  float prev_scale[3], next_scale[3];

  copy_v3_fl(prev_scale, 1.0f);
  copy_v3_fl(next_scale, 1.0f);

  if (prev) {
    float h1[3];
    bool done = false;

    param->use_prev = true;

    /* Transform previous point inside this bone space. */
    if (bone->bbone_prev_type == BBONE_HANDLE_RELATIVE) {
      /* Use delta movement (from restpose), and apply this relative to the current bone's head. */
      if (rest) {
        /* In restpose, arm_head == pose_head */
        zero_v3(param->prev_h);
        done = true;
      }
      else {
        sub_v3_v3v3(delta, prev->pose_head, prev->bone->arm_head);
        sub_v3_v3v3(h1, pchan->pose_head, delta);
      }
    }
    else if (bone->bbone_prev_type == BBONE_HANDLE_TANGENT) {
      /* Use bone direction by offsetting so that its tail meets current bone's head */
      if (rest) {
        sub_v3_v3v3(delta, prev->bone->arm_tail, prev->bone->arm_head);
        sub_v3_v3v3(h1, bone->arm_head, delta);
      }
      else {
        sub_v3_v3v3(delta, prev->pose_tail, prev->pose_head);
        sub_v3_v3v3(h1, pchan->pose_head, delta);
      }
    }
    else {
      /* Apply special handling for smoothly joining B-Bone chains */
      param->prev_bbone = (prev->bone->segments > 1);

      /* Use bone head as absolute position. */
      copy_v3_v3(h1, rest ? prev->bone->arm_head : prev->pose_head);
    }

    if (!done) {
      mul_v3_m4v3(param->prev_h, imat, h1);
    }

    if (!param->prev_bbone) {
      /* Find the previous roll to interpolate. */
      mul_m4_m4m4(param->prev_mat, imat, rest ? prev->bone->arm_mat : prev->pose_mat);

      /* Retrieve the local scale of the bone if necessary. */
      if ((bone->bbone_prev_flag & BBONE_HANDLE_SCALE_ANY) && !rest) {
        BKE_armature_mat_pose_to_bone(prev, prev->pose_mat, tmpmat);
        mat4_to_size(prev_scale, tmpmat);
      }
    }
  }

  if (next) {
    float h2[3];
    bool done = false;

    param->use_next = true;

    /* Transform next point inside this bone space. */
    if (bone->bbone_next_type == BBONE_HANDLE_RELATIVE) {
      /* Use delta movement (from restpose), and apply this relative to the current bone's tail. */
      if (rest) {
        /* In restpose, arm_head == pose_head */
        copy_v3_fl3(param->next_h, 0.0f, param->length, 0.0);
        done = true;
      }
      else {
        sub_v3_v3v3(delta, next->pose_head, next->bone->arm_head);
        add_v3_v3v3(h2, pchan->pose_tail, delta);
      }
    }
    else if (bone->bbone_next_type == BBONE_HANDLE_TANGENT) {
      /* Use bone direction by offsetting so that its head meets current bone's tail */
      if (rest) {
        sub_v3_v3v3(delta, next->bone->arm_tail, next->bone->arm_head);
        add_v3_v3v3(h2, bone->arm_tail, delta);
      }
      else {
        sub_v3_v3v3(delta, next->pose_tail, next->pose_head);
        add_v3_v3v3(h2, pchan->pose_tail, delta);
      }
    }
    else {
      /* Apply special handling for smoothly joining B-Bone chains */
      param->next_bbone = (next->bone->segments > 1);

      /* Use bone tail as absolute position. */
      copy_v3_v3(h2, rest ? next->bone->arm_tail : next->pose_tail);
    }

    if (!done) {
      mul_v3_m4v3(param->next_h, imat, h2);
    }

    /* Find the next roll to interpolate as well. */
    mul_m4_m4m4(param->next_mat, imat, rest ? next->bone->arm_mat : next->pose_mat);

    /* Retrieve the local scale of the bone if necessary. */
    if ((bone->bbone_next_flag & BBONE_HANDLE_SCALE_ANY) && !rest) {
      BKE_armature_mat_pose_to_bone(next, next->pose_mat, tmpmat);
      mat4_to_size(next_scale, tmpmat);
    }
  }

  /* Add effects from bbone properties over the top
   * - These properties allow users to hand-animate the
   *   bone curve/shape, without having to resort to using
   *   extra bones
   * - The "bone" level offsets are for defining the restpose
   *   shape of the bone (e.g. for curved eyebrows for example).
   *   -> In the viewport, it's needed to define what the rest pose
   *      looks like
   *   -> For "rest == 0", we also still need to have it present
   *      so that we can "cancel out" this restpose when it comes
   *      time to deform some geometry, it won't cause double transforms.
   * - The "pchan" level offsets are the ones that animators actually
   *   end up animating
   */
  {
    param->ease1 = bone->ease1 + (!rest ? pchan->ease1 : 0.0f);
    param->ease2 = bone->ease2 + (!rest ? pchan->ease2 : 0.0f);

    param->roll1 = bone->roll1 + (!rest ? pchan->roll1 : 0.0f);
    param->roll2 = bone->roll2 + (!rest ? pchan->roll2 : 0.0f);

    if (bone->bbone_flag & BBONE_ADD_PARENT_END_ROLL) {
      if (prev) {
        if (prev->bone) {
          param->roll1 += prev->bone->roll2;
        }

        if (!rest) {
          param->roll1 += prev->roll2;
        }
      }
    }

    copy_v3_v3(param->scale_in, bone->scale_in);
    copy_v3_v3(param->scale_out, bone->scale_out);

    if (!rest) {
      mul_v3_v3(param->scale_in, pchan->scale_in);
      mul_v3_v3(param->scale_out, pchan->scale_out);
    }

    /* Extra curve x / z */
    param->curve_in_x = bone->curve_in_x + (!rest ? pchan->curve_in_x : 0.0f);
    param->curve_in_z = bone->curve_in_z + (!rest ? pchan->curve_in_z : 0.0f);

    param->curve_out_x = bone->curve_out_x + (!rest ? pchan->curve_out_x : 0.0f);
    param->curve_out_z = bone->curve_out_z + (!rest ? pchan->curve_out_z : 0.0f);

    if (bone->bbone_flag & BBONE_SCALE_EASING) {
      param->ease1 *= param->scale_in[1];
      param->curve_in_x *= param->scale_in[1];
      param->curve_in_z *= param->scale_in[1];

      param->ease2 *= param->scale_out[1];
      param->curve_out_x *= param->scale_out[1];
      param->curve_out_z *= param->scale_out[1];
    }

    /* Custom handle scale. */
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_X) {
      param->scale_in[0] *= prev_scale[0];
    }
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_Y) {
      param->scale_in[1] *= prev_scale[1];
    }
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_Z) {
      param->scale_in[2] *= prev_scale[2];
    }
    if (bone->bbone_prev_flag & BBONE_HANDLE_SCALE_EASE) {
      param->ease1 *= prev_scale[1];
      param->curve_in_x *= prev_scale[1];
      param->curve_in_z *= prev_scale[1];
    }

    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_X) {
      param->scale_out[0] *= next_scale[0];
    }
    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_Y) {
      param->scale_out[1] *= next_scale[1];
    }
    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_Z) {
      param->scale_out[2] *= next_scale[2];
    }
    if (bone->bbone_next_flag & BBONE_HANDLE_SCALE_EASE) {
      param->ease2 *= next_scale[1];
      param->curve_out_x *= next_scale[1];
      param->curve_out_z *= next_scale[1];
    }
  }
}

void BKE_pchan_bbone_spline_setup(bPoseChannel *pchan,
                                  const bool rest,
                                  const bool for_deform,
                                  Mat4 *result_array)
{
  BBoneSplineParameters param;

  BKE_pchan_bbone_spline_params_get(pchan, rest, &param);

  pchan->bone->segments = BKE_pchan_bbone_spline_compute(&param, for_deform, result_array);
}

void BKE_pchan_bbone_handles_compute(const BBoneSplineParameters *param,
                                     float h1[3],
                                     float *r_roll1,
                                     float h2[3],
                                     float *r_roll2,
                                     bool ease,
                                     bool offsets)
{
  float mat3[3][3];
  float length = param->length;
  float epsilon = 1e-5 * length;

  if (param->do_scale) {
    length *= param->scale[1];
  }

  *r_roll1 = *r_roll2 = 0.0f;

  if (param->use_prev) {
    copy_v3_v3(h1, param->prev_h);

    if (param->prev_bbone) {
      /* If previous bone is B-bone too, use average handle direction. */
      h1[1] -= length;
    }

    if (normalize_v3(h1) < epsilon) {
      copy_v3_fl3(h1, 0.0f, -1.0f, 0.0f);
    }

    negate_v3(h1);

    if (!param->prev_bbone) {
      /* Find the previous roll to interpolate. */
      copy_m3_m4(mat3, param->prev_mat);
      mat3_vec_to_roll(mat3, h1, r_roll1);
    }
  }
  else {
    h1[0] = 0.0f;
    h1[1] = 1.0;
    h1[2] = 0.0f;
  }

  if (param->use_next) {
    copy_v3_v3(h2, param->next_h);

    /* If next bone is B-bone too, use average handle direction. */
    if (param->next_bbone) {
      /* pass */
    }
    else {
      h2[1] -= length;
    }

    if (normalize_v3(h2) < epsilon) {
      copy_v3_fl3(h2, 0.0f, 1.0f, 0.0f);
    }

    /* Find the next roll to interpolate as well. */
    copy_m3_m4(mat3, param->next_mat);
    mat3_vec_to_roll(mat3, h2, r_roll2);
  }
  else {
    h2[0] = 0.0f;
    h2[1] = 1.0f;
    h2[2] = 0.0f;
  }

  if (ease) {
    const float circle_factor = length * (cubic_tangent_factor_circle_v3(h1, h2) / 0.75f);

    const float hlength1 = param->ease1 * circle_factor;
    const float hlength2 = param->ease2 * circle_factor;

    /* and only now negate h2 */
    mul_v3_fl(h1, hlength1);
    mul_v3_fl(h2, -hlength2);
  }

  /* Add effects from bbone properties over the top
   * - These properties allow users to hand-animate the
   *   bone curve/shape, without having to resort to using
   *   extra bones
   * - The "bone" level offsets are for defining the rest-pose
   *   shape of the bone (e.g. for curved eyebrows for example).
   *   -> In the viewport, it's needed to define what the rest pose
   *      looks like
   *   -> For "rest == 0", we also still need to have it present
   *      so that we can "cancel out" this rest-pose when it comes
   *      time to deform some geometry, it won't cause double transforms.
   * - The "pchan" level offsets are the ones that animators actually
   *   end up animating
   */
  if (offsets) {
    /* Add extra rolls. */
    *r_roll1 += param->roll1;
    *r_roll2 += param->roll2;

    /* Extra curve x / y */
    /* NOTE:
     * Scale correction factors here are to compensate for some random floating-point glitches
     * when scaling up the bone or its parent by a factor of approximately 8.15/6, which results
     * in the bone length getting scaled up too (from 1 to 8), causing the curve to flatten out.
     */
    const float xscale_correction = (param->do_scale) ? param->scale[0] : 1.0f;
    const float zscale_correction = (param->do_scale) ? param->scale[2] : 1.0f;

    h1[0] += param->curve_in_x * xscale_correction;
    h1[2] += param->curve_in_z * zscale_correction;

    h2[0] += param->curve_out_x * xscale_correction;
    h2[2] += param->curve_out_z * zscale_correction;
  }
}

static void make_bbone_spline_matrix(BBoneSplineParameters *param,
                                     const float scalemats[2][4][4],
                                     const float pos[3],
                                     const float axis[3],
                                     float roll,
                                     float scalex,
                                     float scalez,
                                     float result[4][4])
{
  float mat3[3][3];

  vec_roll_to_mat3(axis, roll, mat3);

  copy_m4_m3(result, mat3);
  copy_v3_v3(result[3], pos);

  if (param->do_scale) {
    /* Correct for scaling when this matrix is used in scaled space. */
    mul_m4_series(result, scalemats[0], result, scalemats[1]);
  }

  /* BBone scale... */
  mul_v3_fl(result[0], scalex);
  mul_v3_fl(result[2], scalez);
}

/* Fade from first to second derivative when the handle is very short. */
static void ease_handle_axis(const float deriv1[3], const float deriv2[3], float r_axis[3])
{
  const float gap = 0.1f;

  copy_v3_v3(r_axis, deriv1);

  const float len2 = len_squared_v3(deriv2);
  if (UNLIKELY(len2 == 0.0f)) {
    return;
  }
  const float len1 = len_squared_v3(deriv1);
  const float ratio = len1 / len2;
  if (ratio < gap * gap) {
    madd_v3_v3fl(r_axis, deriv2, gap - sqrtf(ratio));
  }
}

int BKE_pchan_bbone_spline_compute(BBoneSplineParameters *param,
                                   const bool for_deform,
                                   Mat4 *result_array)
{
  float scalemats[2][4][4];
  float bezt_controls[4][3];
  float h1[3], roll1, h2[3], roll2, prev[3], cur[3], axis[3];
  float length = param->length;

  if (param->do_scale) {
    size_to_mat4(scalemats[1], param->scale);
    invert_m4_m4(scalemats[0], scalemats[1]);

    length *= param->scale[1];
  }

  BKE_pchan_bbone_handles_compute(param, h1, &roll1, h2, &roll2, true, true);

  /* Make curve. */
  CLAMP_MAX(param->segments, MAX_BBONE_SUBDIV);

  copy_v3_fl3(bezt_controls[3], 0.0f, length, 0.0f);
  add_v3_v3v3(bezt_controls[2], bezt_controls[3], h2);
  copy_v3_v3(bezt_controls[1], h1);
  zero_v3(bezt_controls[0]);

  /* Compute lengthwise segment scale. */
  float segment_scales[MAX_BBONE_SUBDIV];

  CLAMP_MIN(param->scale_in[1], 0.0001f);
  CLAMP_MIN(param->scale_out[1], 0.0001f);

  const float log_scale_in_len = logf(param->scale_in[1]);
  const float log_scale_out_len = logf(param->scale_out[1]);

  for (int i = 0; i < param->segments; i++) {
    const float fac = ((float)i) / (param->segments - 1);
    segment_scales[i] = expf(interpf(log_scale_out_len, log_scale_in_len, fac));
  }

  /* Compute segment vertex offsets along the curve length. */
  float bezt_points[MAX_BBONE_SUBDIV + 1];

  equalize_cubic_bezier(
      bezt_controls, MAX_BBONE_SUBDIV, param->segments, segment_scales, bezt_points);

  /* Deformation uses N+1 matrices computed at points between the segments. */
  if (for_deform) {
    /* Bezier derivatives. */
    float bezt_deriv1[3][3], bezt_deriv2[2][3];

    for (int i = 0; i < 3; i++) {
      sub_v3_v3v3(bezt_deriv1[i], bezt_controls[i + 1], bezt_controls[i]);
    }
    for (int i = 0; i < 2; i++) {
      sub_v3_v3v3(bezt_deriv2[i], bezt_deriv1[i + 1], bezt_deriv1[i]);
    }

    /* End points require special handling to fix zero length handles. */
    ease_handle_axis(bezt_deriv1[0], bezt_deriv2[0], axis);
    make_bbone_spline_matrix(param,
                             scalemats,
                             bezt_controls[0],
                             axis,
                             roll1,
                             param->scale_in[0],
                             param->scale_in[2],
                             result_array[0].mat);

    for (int a = 1; a < param->segments; a++) {
      evaluate_cubic_bezier(bezt_controls, bezt_points[a], cur, axis);

      float fac = ((float)a) / param->segments;
      float roll = interpf(roll2, roll1, fac);
      float scalex = interpf(param->scale_out[0], param->scale_in[0], fac);
      float scalez = interpf(param->scale_out[2], param->scale_in[2], fac);

      make_bbone_spline_matrix(
          param, scalemats, cur, axis, roll, scalex, scalez, result_array[a].mat);
    }

    negate_v3(bezt_deriv2[1]);
    ease_handle_axis(bezt_deriv1[2], bezt_deriv2[1], axis);
    make_bbone_spline_matrix(param,
                             scalemats,
                             bezt_controls[3],
                             axis,
                             roll2,
                             param->scale_out[0],
                             param->scale_out[2],
                             result_array[param->segments].mat);
  }
  /* Other code (e.g. display) uses matrices for the segments themselves. */
  else {
    zero_v3(prev);

    for (int a = 0; a < param->segments; a++) {
      evaluate_cubic_bezier(bezt_controls, bezt_points[a + 1], cur, axis);

      sub_v3_v3v3(axis, cur, prev);

      float fac = (a + 0.5f) / param->segments;
      float roll = interpf(roll2, roll1, fac);
      float scalex = interpf(param->scale_out[0], param->scale_in[0], fac);
      float scalez = interpf(param->scale_out[2], param->scale_in[2], fac);

      make_bbone_spline_matrix(
          param, scalemats, prev, axis, roll, scalex, scalez, result_array[a].mat);
      copy_v3_v3(prev, cur);
    }
  }

  return param->segments;
}

static void allocate_bbone_cache(bPoseChannel *pchan, int segments)
{
  bPoseChannel_Runtime *runtime = &pchan->runtime;

  if (runtime->bbone_segments != segments) {
    BKE_pose_channel_free_bbone_cache(runtime);

    runtime->bbone_segments = segments;
    runtime->bbone_rest_mats = MEM_malloc_arrayN(
        1 + (uint)segments, sizeof(Mat4), "bPoseChannel_Runtime::bbone_rest_mats");
    runtime->bbone_pose_mats = MEM_malloc_arrayN(
        1 + (uint)segments, sizeof(Mat4), "bPoseChannel_Runtime::bbone_pose_mats");
    runtime->bbone_deform_mats = MEM_malloc_arrayN(
        2 + (uint)segments, sizeof(Mat4), "bPoseChannel_Runtime::bbone_deform_mats");
    runtime->bbone_dual_quats = MEM_malloc_arrayN(
        1 + (uint)segments, sizeof(DualQuat), "bPoseChannel_Runtime::bbone_dual_quats");
  }
}

void BKE_pchan_bbone_segments_cache_compute(bPoseChannel *pchan)
{
  bPoseChannel_Runtime *runtime = &pchan->runtime;
  Bone *bone = pchan->bone;
  int segments = bone->segments;

  BLI_assert(segments > 1);

  /* Allocate the cache if needed. */
  allocate_bbone_cache(pchan, segments);

  /* Compute the shape. */
  Mat4 *b_bone = runtime->bbone_pose_mats;
  Mat4 *b_bone_rest = runtime->bbone_rest_mats;
  Mat4 *b_bone_mats = runtime->bbone_deform_mats;
  DualQuat *b_bone_dual_quats = runtime->bbone_dual_quats;
  int a;

  BKE_pchan_bbone_spline_setup(pchan, false, true, b_bone);
  BKE_pchan_bbone_spline_setup(pchan, true, true, b_bone_rest);

  /* Compute deform matrices. */
  /* first matrix is the inverse arm_mat, to bring points in local bone space
   * for finding out which segment it belongs to */
  invert_m4_m4(b_bone_mats[0].mat, bone->arm_mat);

  /* then we make the b_bone_mats:
   * - first transform to local bone space
   * - translate over the curve to the bbone mat space
   * - transform with b_bone matrix
   * - transform back into global space */

  for (a = 0; a <= bone->segments; a++) {
    float tmat[4][4];

    invert_m4_m4(tmat, b_bone_rest[a].mat);
    mul_m4_series(b_bone_mats[a + 1].mat,
                  pchan->chan_mat,
                  bone->arm_mat,
                  b_bone[a].mat,
                  tmat,
                  b_bone_mats[0].mat);

    /* Compute the orthonormal object space rest matrix of the segment. */
    mul_m4_m4m4(tmat, bone->arm_mat, b_bone_rest[a].mat);
    normalize_m4(tmat);

    mat4_to_dquat(&b_bone_dual_quats[a], tmat, b_bone_mats[a + 1].mat);
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/armature.c */
void BKE_pchan_bbone_deform_segment_index(const bPoseChannel *pchan,
                                          float pos,
                                          int *r_index,
                                          float *r_blend_next)
{
  int segments = pchan->bone->segments;

  CLAMP(pos, 0.0f, 1.0f);

  /* Calculate the indices of the 2 affecting b_bone segments.
   * Integer part is the first segment's index.
   * Integer part plus 1 is the second segment's index.
   * Fractional part is the blend factor. */
  float pre_blend = pos * (float)segments;

  int index = (int)floorf(pre_blend);
  CLAMP(index, 0, segments - 1);

  float blend = pre_blend - index;
  CLAMP(blend, 0.0f, 1.0f);

  *r_index = index;
  *r_blend_next = blend;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/armature.c */
void BKE_bone_offset_matrix_get(const Bone *bone, float offs_bone[4][4])
{
  BLI_assert(bone->parent != NULL);

  /* Bone transform itself. */
  copy_m4_m3(offs_bone, bone->bone_mat);

  /* The bone's root offset (is in the parent's coordinate system). */
  copy_v3_v3(offs_bone[3], bone->head);

  /* Get the length translation of parent (length along y axis). */
  offs_bone[3][1] += bone->parent->length;
}

void BKE_bone_parent_transform_calc_from_pchan(const bPoseChannel *pchan,
                                               BoneParentTransform *r_bpt)
{
  const Bone *bone, *parbone;
  const bPoseChannel *parchan;

  /* set up variables for quicker access below */
  bone = pchan->bone;
  parbone = bone->parent;
  parchan = pchan->parent;

  if (parchan) {
    float offs_bone[4][4];
    /* yoffs(b-1) + root(b) + bonemat(b). */
    BKE_bone_offset_matrix_get(bone, offs_bone);

    BKE_bone_parent_transform_calc_from_matrices(bone->flag,
                                                 bone->inherit_scale_mode,
                                                 offs_bone,
                                                 parbone->arm_mat,
                                                 parchan->pose_mat,
                                                 r_bpt);
  }
  else {
    BKE_bone_parent_transform_calc_from_matrices(
        bone->flag, bone->inherit_scale_mode, bone->arm_mat, NULL, NULL, r_bpt);
  }
}

void BKE_bone_parent_transform_calc_from_matrices(int bone_flag,
                                                  int inherit_scale_mode,
                                                  const float offs_bone[4][4],
                                                  const float parent_arm_mat[4][4],
                                                  const float parent_pose_mat[4][4],
                                                  BoneParentTransform *r_bpt)
{
  copy_v3_fl(r_bpt->post_scale, 1.0f);

  if (parent_pose_mat) {
    const bool use_rotation = (bone_flag & BONE_HINGE) == 0;
    const bool full_transform = use_rotation && inherit_scale_mode == BONE_INHERIT_SCALE_FULL;

    /* Compose the rotscale matrix for this bone. */
    if (full_transform) {
      /* Parent pose rotation and scale. */
      mul_m4_m4m4(r_bpt->rotscale_mat, parent_pose_mat, offs_bone);
    }
    else {
      float tmat[4][4], tscale[3];

      /* If using parent pose rotation: */
      if (use_rotation) {
        copy_m4_m4(tmat, parent_pose_mat);

        /* Normalize the matrix when needed. */
        switch (inherit_scale_mode) {
          case BONE_INHERIT_SCALE_FULL:
          case BONE_INHERIT_SCALE_FIX_SHEAR:
            /* Keep scale and shear. */
            break;

          case BONE_INHERIT_SCALE_NONE:
          case BONE_INHERIT_SCALE_AVERAGE:
            /* Remove scale and shear from parent. */
            orthogonalize_m4_stable(tmat, 1, true);
            break;

          case BONE_INHERIT_SCALE_ALIGNED:
            /* Remove shear and extract scale. */
            orthogonalize_m4_stable(tmat, 1, false);
            normalize_m4_ex(tmat, r_bpt->post_scale);
            break;

          case BONE_INHERIT_SCALE_NONE_LEGACY:
            /* Remove only scale - bad legacy way. */
            normalize_m4(tmat);
            break;

          default:
            BLI_assert_unreachable();
        }
      }
      /* If removing parent pose rotation: */
      else {
        copy_m4_m4(tmat, parent_arm_mat);

        /* Copy the parent scale when needed. */
        switch (inherit_scale_mode) {
          case BONE_INHERIT_SCALE_FULL:
            /* Ignore effects of shear. */
            mat4_to_size(tscale, parent_pose_mat);
            rescale_m4(tmat, tscale);
            break;

          case BONE_INHERIT_SCALE_FIX_SHEAR:
            /* Take the effects of parent shear into account to get exact volume. */
            mat4_to_size_fix_shear(tscale, parent_pose_mat);
            rescale_m4(tmat, tscale);
            break;

          case BONE_INHERIT_SCALE_ALIGNED:
            mat4_to_size_fix_shear(r_bpt->post_scale, parent_pose_mat);
            break;

          case BONE_INHERIT_SCALE_NONE:
          case BONE_INHERIT_SCALE_AVERAGE:
          case BONE_INHERIT_SCALE_NONE_LEGACY:
            /* Keep unscaled. */
            break;

          default:
            BLI_assert_unreachable();
        }
      }

      /* Apply the average parent scale when needed. */
      if (inherit_scale_mode == BONE_INHERIT_SCALE_AVERAGE) {
        mul_mat3_m4_fl(tmat, cbrtf(fabsf(mat4_to_volume_scale(parent_pose_mat))));
      }

      mul_m4_m4m4(r_bpt->rotscale_mat, tmat, offs_bone);

      /* Remove remaining shear when needed, preserving volume. */
      if (inherit_scale_mode == BONE_INHERIT_SCALE_FIX_SHEAR) {
        orthogonalize_m4_stable(r_bpt->rotscale_mat, 1, false);
      }
    }

    /* Compose the loc matrix for this bone. */
    /* NOTE: That version does not modify bone's loc when HINGE/NO_SCALE options are set. */

    /* In this case, use the object's space *orientation*. */
    if (bone_flag & BONE_NO_LOCAL_LOCATION) {
      /* XXX I'm sure that code can be simplified! */
      float bone_loc[4][4], bone_rotscale[3][3], tmat4[4][4], tmat3[3][3];
      unit_m4(bone_loc);
      unit_m4(r_bpt->loc_mat);
      unit_m4(tmat4);

      mul_v3_m4v3(bone_loc[3], parent_pose_mat, offs_bone[3]);

      unit_m3(bone_rotscale);
      copy_m3_m4(tmat3, parent_pose_mat);
      mul_m3_m3m3(bone_rotscale, tmat3, bone_rotscale);

      copy_m4_m3(tmat4, bone_rotscale);
      mul_m4_m4m4(r_bpt->loc_mat, bone_loc, tmat4);
    }
    /* Those flags do not affect position, use plain parent transform space! */
    else if (!full_transform) {
      mul_m4_m4m4(r_bpt->loc_mat, parent_pose_mat, offs_bone);
    }
    /* Else (i.e. default, usual case),
     * just use the same matrix for rotation/scaling, and location. */
    else {
      copy_m4_m4(r_bpt->loc_mat, r_bpt->rotscale_mat);
    }
  }
  /* Root bones. */
  else {
    /* Rotation/scaling. */
    copy_m4_m4(r_bpt->rotscale_mat, offs_bone);
    /* Translation. */
    if (bone_flag & BONE_NO_LOCAL_LOCATION) {
      /* Translation of arm_mat, without the rotation. */
      unit_m4(r_bpt->loc_mat);
      copy_v3_v3(r_bpt->loc_mat[3], offs_bone[3]);
    }
    else {
      copy_m4_m4(r_bpt->loc_mat, r_bpt->rotscale_mat);
    }
  }
}

void BKE_bone_parent_transform_clear(struct BoneParentTransform *bpt)
{
  unit_m4(bpt->rotscale_mat);
  unit_m4(bpt->loc_mat);
  copy_v3_fl(bpt->post_scale, 1.0f);
}

void BKE_bone_parent_transform_invert(struct BoneParentTransform *bpt)
{
  invert_m4(bpt->rotscale_mat);
  invert_m4(bpt->loc_mat);
  invert_v3_safe(bpt->post_scale);
}

void BKE_bone_parent_transform_combine(const struct BoneParentTransform *in1,
                                       const struct BoneParentTransform *in2,
                                       struct BoneParentTransform *result)
{
  mul_m4_m4m4(result->rotscale_mat, in1->rotscale_mat, in2->rotscale_mat);
  mul_m4_m4m4(result->loc_mat, in1->loc_mat, in2->loc_mat);
  mul_v3_v3v3(result->post_scale, in1->post_scale, in2->post_scale);
}

void BKE_bone_parent_transform_apply(const struct BoneParentTransform *bpt,
                                     const float inmat[4][4],
                                     float outmat[4][4])
{
  /* in case inmat == outmat */
  float tmploc[3];
  copy_v3_v3(tmploc, inmat[3]);

  mul_m4_m4m4(outmat, bpt->rotscale_mat, inmat);
  mul_v3_m4v3(outmat[3], bpt->loc_mat, tmploc);
  rescale_m4(outmat, bpt->post_scale);
}

void BKE_armature_mat_pose_to_bone(bPoseChannel *pchan,
                                   const float inmat[4][4],
                                   float outmat[4][4])
{
  BoneParentTransform bpt;

  BKE_bone_parent_transform_calc_from_pchan(pchan, &bpt);
  BKE_bone_parent_transform_invert(&bpt);
  BKE_bone_parent_transform_apply(&bpt, inmat, outmat);
}

void BKE_armature_mat_bone_to_pose(bPoseChannel *pchan,
                                   const float inmat[4][4],
                                   float outmat[4][4])
{
  BoneParentTransform bpt;

  BKE_bone_parent_transform_calc_from_pchan(pchan, &bpt);
  BKE_bone_parent_transform_apply(&bpt, inmat, outmat);
}

void BKE_armature_loc_pose_to_bone(bPoseChannel *pchan, const float inloc[3], float outloc[3])
{
  float xLocMat[4][4];
  float nLocMat[4][4];

  /* build matrix for location */
  unit_m4(xLocMat);
  copy_v3_v3(xLocMat[3], inloc);

  /* get bone-space cursor matrix and extract location */
  BKE_armature_mat_pose_to_bone(pchan, xLocMat, nLocMat);
  copy_v3_v3(outloc, nLocMat[3]);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/armature.c */
void BKE_pchan_mat3_to_rot(bPoseChannel *pchan, const float mat[3][3], bool use_compat)
{
  BLI_ASSERT_UNIT_M3(mat);

  switch (pchan->rotmode) {
    case ROT_MODE_QUAT:
      mat3_normalized_to_quat(pchan->quat, mat);
      break;
    case ROT_MODE_AXISANGLE:
      mat3_normalized_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, mat);
      break;
    default: /* euler */
      if (use_compat) {
        mat3_normalized_to_compatible_eulO(pchan->eul, pchan->eul, pchan->rotmode, mat);
      }
      else {
        mat3_normalized_to_eulO(pchan->eul, pchan->rotmode, mat);
      }
      break;
  }
}

void BKE_pchan_rot_to_mat3(const bPoseChannel *pchan, float r_mat[3][3])
{
  /* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
  if (pchan->rotmode > 0) {
    /* Euler rotations (will cause gimbal lock,
     * but this can be alleviated a bit with rotation orders) */
    eulO_to_mat3(r_mat, pchan->eul, pchan->rotmode);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    /* axis-angle - not really that great for 3D-changing orientations */
    axis_angle_to_mat3(r_mat, pchan->rotAxis, pchan->rotAngle);
  }
  else {
    /* quats are normalized before use to eliminate scaling issues */
    float quat[4];

    /* NOTE: we now don't normalize the stored values anymore,
     * since this was kind of evil in some cases but if this proves to be too problematic,
     * switch back to the old system of operating directly on the stored copy. */
    normalize_qt_qt(quat, pchan->quat);
    quat_to_mat3(r_mat, quat);
  }
}

void BKE_pchan_apply_mat4(bPoseChannel *pchan, const float mat[4][4], bool use_compat)
{
  float rot[3][3];
  mat4_to_loc_rot_size(pchan->loc, rot, pchan->size, mat);
  BKE_pchan_mat3_to_rot(pchan, rot, use_compat);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/armature.c */
void mat3_to_vec_roll(const float mat[3][3], float r_vec[3], float *r_roll)
{
  if (r_vec) {
    copy_v3_v3(r_vec, mat[1]);
  }

  if (r_roll) {
    mat3_vec_to_roll(mat, mat[1], r_roll);
  }
}

void mat3_vec_to_roll(const float mat[3][3], const float vec[3], float *r_roll)
{
  float vecmat[3][3], vecmatinv[3][3], rollmat[3][3], q[4];

  /* Compute the orientation relative to the vector with zero roll. */
  vec_roll_to_mat3(vec, 0.0f, vecmat);
  invert_m3_m3(vecmatinv, vecmat);
  mul_m3_m3m3(rollmat, vecmatinv, mat);

  /* Extract the twist angle as the roll value. */
  mat3_to_quat(q, rollmat);

  *r_roll = quat_split_swing_and_twist(q, 1, NULL, NULL);
}

void vec_roll_to_mat3_normalized(const float nor[3], const float roll, float r_mat[3][3])
{
  /**
   * Given `v = (v.x, v.y, v.z)` our (normalized) bone vector, we want the rotation matrix M
   * from the Y axis (so that `M * (0, 1, 0) = v`).
   * - The rotation axis a lays on XZ plane, and it is orthonormal to v,
   *   hence to the projection of v onto XZ plane.
   * - `a = (v.z, 0, -v.x)`
   *
   * We know a is eigenvector of M (so M * a = a).
   * Finally, we have w, such that M * w = (0, 1, 0)
   * (i.e. the vector that will be aligned with Y axis once transformed).
   * We know w is symmetric to v by the Y axis.
   * - `w = (-v.x, v.y, -v.z)`
   *
   * Solving this, we get (x, y and z being the components of v):
   * <pre>
   *     ┌ (x^2 * y + z^2) / (x^2 + z^2),   x,   x * z * (y - 1) / (x^2 + z^2) ┐
   * M = │  x * (y^2 - 1)  / (x^2 + z^2),   y,    z * (y^2 - 1)  / (x^2 + z^2) │
   *     └ x * z * (y - 1) / (x^2 + z^2),   z,   (x^2 + z^2 * y) / (x^2 + z^2) ┘
   * </pre>
   *
   * This is stable as long as v (the bone) is not too much aligned with +/-Y
   * (i.e. x and z components are not too close to 0).
   *
   * Since v is normalized, we have `x^2 + y^2 + z^2 = 1`,
   * hence `x^2 + z^2 = 1 - y^2 = (1 - y)(1 + y)`.
   *
   * This allows to simplifies M like this:
   * <pre>
   *     ┌ 1 - x^2 / (1 + y),   x,     -x * z / (1 + y) ┐
   * M = │                -x,   y,                   -z │
   *     └  -x * z / (1 + y),   z,    1 - z^2 / (1 + y) ┘
   * </pre>
   *
   * Written this way, we see the case v = +Y is no more a singularity.
   * The only one
   * remaining is the bone being aligned with -Y.
   *
   * Let's handle
   * the asymptotic behavior when bone vector is reaching the limit of y = -1.
   * Each of the four corner elements can vary from -1 to 1,
   * depending on the axis a chosen for doing the rotation.
   * And the "rotation" here is in fact established by mirroring XZ plane by that given axis,
   * then inversing the Y-axis.
   * For sufficiently small x and z, and with y approaching -1,
   * all elements but the four corner ones of M will degenerate.
   * So let's now focus on these corner elements.
   *
   * We rewrite M so that it only contains its four corner elements,
   * and combine the `1 / (1 + y)` factor:
   * <pre>
   *                    ┌ 1 + y - x^2,        -x * z ┐
   * M* = 1 / (1 + y) * │                            │
   *                    └      -x * z,   1 + y - z^2 ┘
   * </pre>
   *
   * When y is close to -1, computing 1 / (1 + y) will cause severe numerical instability,
   * so we use a different approach based on x and z as inputs.
   * We know `y^2 = 1 - (x^2 + z^2)`, and `y < 0`, hence `y = -sqrt(1 - (x^2 + z^2))`.
   *
   * Since x and z are both close to 0, we apply the binomial expansion to the second order:
   * `y = -sqrt(1 - (x^2 + z^2)) = -1 + (x^2 + z^2) / 2 + (x^2 + z^2)^2 / 8`, which allows
   * eliminating the problematic `1` constant.
   *
   * A first order expansion allows simplifying to this, but second order is more precise:
   * <pre>
   *                        ┌  z^2 - x^2,  -2 * x * z ┐
   * M* = 1 / (x^2 + z^2) * │                         │
   *                        └ -2 * x * z,   x^2 - z^2 ┘
   * </pre>
   *
   * P.S. In the end, this basically is a heavily optimized version of Damped Track +Y.
   */

  const float SAFE_THRESHOLD = 6.1e-3f;     /* Theta above this value has good enough precision. */
  const float CRITICAL_THRESHOLD = 2.5e-4f; /* True singularity if XZ distance is below this. */
  const float THRESHOLD_SQUARED = CRITICAL_THRESHOLD * CRITICAL_THRESHOLD;

  const float x = nor[0];
  const float y = nor[1];
  const float z = nor[2];

  float theta = 1.0f + y;                /* Remapping Y from [-1,+1] to [0,2]. */
  const float theta_alt = x * x + z * z; /* Squared distance from origin in x,z plane. */
  float rMatrix[3][3], bMatrix[3][3];

  BLI_ASSERT_UNIT_V3(nor);

  /* Determine if the input is far enough from the true singularity of this type of
   * transformation at (0,-1,0), where roll becomes 0/0 undefined without a limit.
   *
   * When theta is close to zero (nor is aligned close to negative Y Axis),
   * we have to check we do have non-null X/Z components as well.
   * Also, due to float precision errors, nor can be (0.0, -0.99999994, 0.0) which results
   * in theta being close to zero. This will cause problems when theta is used as divisor.
   */
  if (theta > SAFE_THRESHOLD || theta_alt > THRESHOLD_SQUARED) {
    /* nor is *not* aligned to negative Y-axis (0,-1,0). */

    bMatrix[0][1] = -x;
    bMatrix[1][0] = x;
    bMatrix[1][1] = y;
    bMatrix[1][2] = z;
    bMatrix[2][1] = -z;

    if (theta <= SAFE_THRESHOLD) {
      /* When nor is close to negative Y axis (0,-1,0) the theta precision is very bad,
       * so recompute it from x and z instead, using the series expansion for sqrt. */
      theta = theta_alt * 0.5f + theta_alt * theta_alt * 0.125f;
    }

    bMatrix[0][0] = 1 - x * x / theta;
    bMatrix[2][2] = 1 - z * z / theta;
    bMatrix[2][0] = bMatrix[0][2] = -x * z / theta;
  }
  else {
    /* nor is very close to negative Y axis (0,-1,0): use simple symmetry by Z axis. */
    unit_m3(bMatrix);
    bMatrix[0][0] = bMatrix[1][1] = -1.0;
  }

  /* Make Roll matrix */
  axis_angle_normalized_to_mat3(rMatrix, nor, roll);

  /* Combine and output result */
  mul_m3_m3m3(r_mat, rMatrix, bMatrix);
}

void vec_roll_to_mat3(const float vec[3], const float roll, float r_mat[3][3])
{
  float nor[3];

  normalize_v3_v3(nor, vec);
  vec_roll_to_mat3_normalized(nor, roll, r_mat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Bone Matrix Calculation (Recursive)
 * \{ */

void BKE_armature_where_is_bone(Bone *bone, const Bone *bone_parent, const bool use_recursion)
{
  float vec[3];

  /* Bone Space */
  sub_v3_v3v3(vec, bone->tail, bone->head);
  bone->length = len_v3(vec);
  vec_roll_to_mat3(vec, bone->roll, bone->bone_mat);

  /* this is called on old file reading too... */
  if (bone->xwidth == 0.0f) {
    bone->xwidth = 0.1f;
    bone->zwidth = 0.1f;
    bone->segments = 1;
  }

  if (bone_parent) {
    float offs_bone[4][4];
    /* yoffs(b-1) + root(b) + bonemat(b) */
    BKE_bone_offset_matrix_get(bone, offs_bone);

    /* Compose the matrix for this bone. */
    mul_m4_m4m4(bone->arm_mat, bone_parent->arm_mat, offs_bone);
  }
  else {
    copy_m4_m3(bone->arm_mat, bone->bone_mat);
    copy_v3_v3(bone->arm_mat[3], bone->head);
  }

  /* and the kiddies */
  if (use_recursion) {
    bone_parent = bone;
    for (bone = bone->childbase.first; bone; bone = bone->next) {
      BKE_armature_where_is_bone(bone, bone_parent, use_recursion);
    }
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/armature.c */
void BKE_pchan_to_mat4(const bPoseChannel *pchan, float r_chanmat[4][4])
{
  float smat[3][3];
  float rmat[3][3];
  float tmat[3][3];

  /* get scaling matrix */
  size_to_mat3(smat, pchan->size);

  /* get rotation matrix */
  BKE_pchan_rot_to_mat3(pchan, rmat);

  /* calculate matrix of bone (as 3x3 matrix, but then copy the 4x4) */
  mul_m3_m3m3(tmat, rmat, smat);
  copy_m4_m3(r_chanmat, tmat);

  /* prevent action channels breaking chains */
  /* need to check for bone here, CONSTRAINT_TYPE_ACTION uses this call */
  if ((pchan->bone == NULL) || !(pchan->bone->flag & BONE_CONNECTED)) {
    copy_v3_v3(r_chanmat[3], pchan->loc);
  }
}

void BKE_pchan_calc_mat(bPoseChannel *pchan)
{
  /* this is just a wrapper around the copy of this function which calculates the matrix
   * and stores the result in any given channel
   */
  BKE_pchan_to_mat4(pchan, pchan->chan_mat);
}

void BKE_pose_where_is_bone_tail(bPoseChannel *pchan)
{
  float vec[3];

  copy_v3_v3(vec, pchan->pose_mat[1]);
  mul_v3_fl(vec, pchan->bone->length);
  add_v3_v3v3(pchan->pose_tail, pchan->pose_head, vec);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/armature_deform.c */
static void pchan_deform_accumulate(const DualQuat *deform_dq,
                                    const float deform_mat[4][4],
                                    const float co_in[3],
                                    float weight,
                                    float co_accum[3],
                                    DualQuat *dq_accum,
                                    float mat_accum[3][3])
{
  if (weight == 0.0f) {
    return;
  }

  if (dq_accum) {
    BLI_assert(!co_accum);

    add_weighted_dq_dq(dq_accum, deform_dq, weight);
  }
  else {
    float tmp[3];
    mul_v3_m4v3(tmp, deform_mat, co_in);

    sub_v3_v3(tmp, co_in);
    madd_v3_v3fl(co_accum, tmp, weight);

    if (mat_accum) {
      float tmpmat[3][3];
      copy_m3_m4(tmpmat, deform_mat);

      madd_m3_m3m3fl(mat_accum, mat_accum, tmpmat, weight);
    }
  }
}

static void b_bone_deform(const bPoseChannel *pchan,
                          const float co[3],
                          float weight,
                          float vec[3],
                          DualQuat *dq,
                          float defmat[3][3])
{
  const DualQuat *quats = pchan->runtime.bbone_dual_quats;
  const Mat4 *mats = pchan->runtime.bbone_deform_mats;
  const float(*mat)[4] = mats[0].mat;
  float blend, y;
  int index;

  /* Transform co to bone space and get its y component. */
  y = mat[0][1] * co[0] + mat[1][1] * co[1] + mat[2][1] * co[2] + mat[3][1];

  /* Calculate the indices of the 2 affecting b_bone segments. */
  BKE_pchan_bbone_deform_segment_index(pchan, y / pchan->bone->length, &index, &blend);

  pchan_deform_accumulate(
      &quats[index], mats[index + 1].mat, co, weight * (1.0f - blend), vec, dq, defmat);
  pchan_deform_accumulate(
      &quats[index + 1], mats[index + 2].mat, co, weight * blend, vec, dq, defmat);
}

float distfactor_to_bone(
    const float vec[3], const float b1[3], const float b2[3], float rad1, float rad2, float rdist)
{
  float dist_sq;
  float bdelta[3];
  float pdelta[3];
  float hsqr, a, l, rad;

  sub_v3_v3v3(bdelta, b2, b1);
  l = normalize_v3(bdelta);

  sub_v3_v3v3(pdelta, vec, b1);

  a = dot_v3v3(bdelta, pdelta);
  hsqr = len_squared_v3(pdelta);

  if (a < 0.0f) {
    /* If we're past the end of the bone, do a spherical field attenuation thing */
    dist_sq = len_squared_v3v3(b1, vec);
    rad = rad1;
  }
  else if (a > l) {
    /* If we're past the end of the bone, do a spherical field attenuation thing */
    dist_sq = len_squared_v3v3(b2, vec);
    rad = rad2;
  }
  else {
    dist_sq = (hsqr - (a * a));

    if (l != 0.0f) {
      rad = a / l;
      rad = rad * rad2 + (1.0f - rad) * rad1;
    }
    else {
      rad = rad1;
    }
  }

  a = rad * rad;
  if (dist_sq < a) {
    return 1.0f;
  }

  l = rad + rdist;
  l *= l;
  if (rdist == 0.0f || dist_sq >= l) {
    return 0.0f;
  }

  a = sqrtf(dist_sq) - rad;
  return 1.0f - (a * a) / (rdist * rdist);
}

static float dist_bone_deform(
    const bPoseChannel *pchan, float vec[3], DualQuat *dq, float mat[3][3], const float co[3])
{
  const Bone *bone = pchan->bone;
  float fac, contrib = 0.0;

  if (bone == NULL) {
    return 0.0f;
  }

  fac = distfactor_to_bone(
      co, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);

  if (fac > 0.0f) {
    fac *= bone->weight;
    contrib = fac;
    if (contrib > 0.0f) {
      if (bone->segments > 1 && pchan->runtime.bbone_segments == bone->segments) {
        b_bone_deform(pchan, co, fac, vec, dq, mat);
      }
      else {
        pchan_deform_accumulate(
            &pchan->runtime.deform_dual_quat, pchan->chan_mat, co, fac, vec, dq, mat);
      }
    }
  }

  return contrib;
}

static void pchan_bone_deform(const bPoseChannel *pchan,
                              float weight,
                              float vec[3],
                              DualQuat *dq,
                              float mat[3][3],
                              const float co[3],
                              float *contrib)
{
  const Bone *bone = pchan->bone;

  if (!weight) {
    return;
  }

  if (bone->segments > 1 && pchan->runtime.bbone_segments == bone->segments) {
    b_bone_deform(pchan, co, weight, vec, dq, mat);
  }
  else {
    pchan_deform_accumulate(
        &pchan->runtime.deform_dual_quat, pchan->chan_mat, co, weight, vec, dq, mat);
  }

  (*contrib) += weight;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Deform #BKE_armature_deform_coords API
 *
 * #BKE_armature_deform_coords and related functions.
 * \{ */

typedef struct ArmatureUserdata {
  const Object *ob_arm;
  const Mesh *me_target;
  float (*vert_coords)[3];
  float (*vert_deform_mats)[3][3];
  float (*vert_coords_prev)[3];

  bool use_envelope;
  bool use_quaternion;
  bool invert_vgroup;
  bool use_dverts;

  int armature_def_nr;

  const MDeformVert *dverts;
  int dverts_len;

  bPoseChannel **pchan_from_defbase;
  int defbase_len;

  float premat[4][4];
  float postmat[4][4];

  /** Specific data types. */
  struct {
    int cd_dvert_offset;
  } bmesh;
} ArmatureUserdata;

static void armature_vert_task_with_dvert(const ArmatureUserdata *data,
                                          const int i,
                                          const MDeformVert *dvert)
{
  float(*const vert_coords)[3] = data->vert_coords;
  float(*const vert_deform_mats)[3][3] = data->vert_deform_mats;
  float(*const vert_coords_prev)[3] = data->vert_coords_prev;
  const bool use_envelope = data->use_envelope;
  const bool use_quaternion = data->use_quaternion;
  const bool use_dverts = data->use_dverts;
  const int armature_def_nr = data->armature_def_nr;

  DualQuat sumdq, *dq = NULL;
  const bPoseChannel *pchan;
  float *co, dco[3];
  float sumvec[3], summat[3][3];
  float *vec = NULL, (*smat)[3] = NULL;
  float contrib = 0.0f;
  float armature_weight = 1.0f; /* default to 1 if no overall def group */
  float prevco_weight = 0.0f;   /* weight for optional cached vertexcos */

  if (use_quaternion) {
    memset(&sumdq, 0, sizeof(DualQuat));
    dq = &sumdq;
  }
  else {
    zero_v3(sumvec);
    vec = sumvec;

    if (vert_deform_mats) {
      zero_m3(summat);
      smat = summat;
    }
  }

  if (armature_def_nr != -1 && dvert) {
    armature_weight = BKE_defvert_find_weight(dvert, armature_def_nr);

    if (data->invert_vgroup) {
      armature_weight = 1.0f - armature_weight;
    }

    /* hackish: the blending factor can be used for blending with vert_coords_prev too */
    if (vert_coords_prev) {
      /* This weight specifies the contribution from the coordinates at the start of this
       * modifier evaluation, while armature_weight is normally the opposite of that. */
      prevco_weight = 1.0f - armature_weight;
      armature_weight = 1.0f;
    }
  }

  /* check if there's any  point in calculating for this vert */
  if (vert_coords_prev) {
    if (prevco_weight == 1.0f) {
      return;
    }

    /* get the coord we work on */
    co = vert_coords_prev[i];
  }
  else {
    if (armature_weight == 0.0f) {
      return;
    }

    /* get the coord we work on */
    co = vert_coords[i];
  }

  /* Apply the object's matrix */
  mul_m4_v3(data->premat, co);

  if (use_dverts && dvert && dvert->totweight) { /* use weight groups ? */
    const MDeformWeight *dw = dvert->dw;
    int deformed = 0;
    uint j;
    for (j = dvert->totweight; j != 0; j--, dw++) {
      const uint index = dw->def_nr;
      if (index < data->defbase_len && (pchan = data->pchan_from_defbase[index])) {
        float weight = dw->weight;
        const Bone *bone = pchan->bone;

        deformed = 1;

        if (bone && bone->flag & BONE_MULT_VG_ENV) {
          weight *= distfactor_to_bone(
              co, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);
        }

        pchan_bone_deform(pchan, weight, vec, dq, smat, co, &contrib);
      }
    }
    /* If there are vertex-groups but not groups with bones (like for soft-body groups). */
    if (deformed == 0 && use_envelope) {
      for (pchan = data->ob_arm->pose->chanbase.first; pchan; pchan = pchan->next) {
        if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
          contrib += dist_bone_deform(pchan, vec, dq, smat, co);
        }
      }
    }
  }
  else if (use_envelope) {
    for (pchan = data->ob_arm->pose->chanbase.first; pchan; pchan = pchan->next) {
      if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
        contrib += dist_bone_deform(pchan, vec, dq, smat, co);
      }
    }
  }

  /* actually should be EPSILON? weight values and contrib can be like 10e-39 small */
  if (contrib > 0.0001f) {
    if (use_quaternion) {
      normalize_dq(dq, contrib);

      if (armature_weight != 1.0f) {
        copy_v3_v3(dco, co);
        mul_v3m3_dq(dco, (vert_deform_mats) ? summat : NULL, dq);
        sub_v3_v3(dco, co);
        mul_v3_fl(dco, armature_weight);
        add_v3_v3(co, dco);
      }
      else {
        mul_v3m3_dq(co, (vert_deform_mats) ? summat : NULL, dq);
      }

      smat = summat;
    }
    else {
      mul_v3_fl(vec, armature_weight / contrib);
      add_v3_v3v3(co, vec, co);
    }

    if (vert_deform_mats) {
      float pre[3][3], post[3][3], tmpmat[3][3];

      copy_m3_m4(pre, data->premat);
      copy_m3_m4(post, data->postmat);
      copy_m3_m3(tmpmat, vert_deform_mats[i]);

      if (!use_quaternion) { /* quaternion already is scale corrected */
        mul_m3_fl(smat, armature_weight / contrib);
      }

      mul_m3_series(vert_deform_mats[i], post, smat, pre, tmpmat);
    }
  }

  /* always, check above code */
  mul_m4_v3(data->postmat, co);

  /* interpolate with previous modifier position using weight group */
  if (vert_coords_prev) {
    float mw = 1.0f - prevco_weight;
    vert_coords[i][0] = prevco_weight * vert_coords[i][0] + mw * co[0];
    vert_coords[i][1] = prevco_weight * vert_coords[i][1] + mw * co[1];
    vert_coords[i][2] = prevco_weight * vert_coords[i][2] + mw * co[2];
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/action.c */
bPoseChannel *BKE_pose_channel_find_name(const bPose *pose, const char *name)
{
  if (ELEM(NULL, pose, name) || (name[0] == '\0')) {
    return NULL;
  }

  if (pose->chanhash) {
    return BLI_ghash_lookup(pose->chanhash, (const void *)name);
  }

  return BLI_findstring(&pose->chanbase, name, offsetof(bPoseChannel, name));
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/fcurve.c */
static int BKE_fcurve_bezt_binarysearch_index_ex(const BezTriple array[],
                                                 const float frame,
                                                 const int arraylen,
                                                 const float threshold,
                                                 bool *r_replace)
{
  int start = 0, end = arraylen;
  int loopbreaker = 0, maxloop = arraylen * 2;

  /* Initialize replace-flag first. */
  *r_replace = false;

  /* Sneaky optimizations (don't go through searching process if...):
   * - Keyframe to be added is to be added out of current bounds.
   * - Keyframe to be added would replace one of the existing ones on bounds.
   */
  if (arraylen <= 0 || array == NULL) {
    CLOG_WARN(&LOG, "encountered invalid array");
    return 0;
  }

  /* Check whether to add before/after/on. */
  /* 'First' Keyframe (when only one keyframe, this case is used) */
  float framenum = array[0].vec[1][0];
  if (IS_EQT(frame, framenum, threshold)) {
    *r_replace = true;
    return 0;
  }
  if (frame < framenum) {
    return 0;
  }

  /* 'Last' Keyframe */
  framenum = array[(arraylen - 1)].vec[1][0];
  if (IS_EQT(frame, framenum, threshold)) {
    *r_replace = true;
    return (arraylen - 1);
  }
  if (frame > framenum) {
    return arraylen;
  }

  /* Most of the time, this loop is just to find where to put it
   * 'loopbreaker' is just here to prevent infinite loops.
   */
  for (loopbreaker = 0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
    /* Compute and get midpoint. */

    /* We calculate the midpoint this way to avoid int overflows... */
    const int mid = start + ((end - start) / 2);

    const float midfra = array[mid].vec[1][0];

    /* Check if exactly equal to midpoint. */
    if (IS_EQT(frame, midfra, threshold)) {
      *r_replace = true;
      return mid;
    }

    /* Repeat in upper/lower half. */
    if (frame > midfra) {
      start = mid + 1;
    }
    else if (frame < midfra) {
      end = mid - 1;
    }
  }

  /* Print error if loop-limit exceeded. */
  if (loopbreaker == (maxloop - 1)) {
    CLOG_ERROR(&LOG, "search taking too long");

    /* Include debug info. */
    CLOG_ERROR(&LOG,
               "\tround = %d: start = %d, end = %d, arraylen = %d",
               loopbreaker,
               start,
               end,
               arraylen);
  }

  /* Not found, so return where to place it. */
  return start;
}

int BKE_fcurve_bezt_binarysearch_index(const BezTriple array[],
                                       const float frame,
                                       const int arraylen,
                                       bool *r_replace)
{
  /* This is just a wrapper which uses the default threshold. */
  return BKE_fcurve_bezt_binarysearch_index_ex(
      array, frame, arraylen, BEZT_BINARYSEARCH_THRESH, r_replace);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/fcurve.c */
void BKE_fcurve_active_keyframe_set(FCurve *fcu, const BezTriple *active_bezt)
{
  if (active_bezt == NULL) {
    fcu->active_keyframe_index = FCURVE_ACTIVE_KEYFRAME_NONE;
    return;
  }

  /* Gracefully handle out-of-bounds pointers. Ideally this would do a BLI_assert() as well, but
   * then the unit tests would break in debug mode. */
  const ptrdiff_t offset = active_bezt - fcu->bezt;
  if (offset < 0 || offset >= fcu->totvert) {
    fcu->active_keyframe_index = FCURVE_ACTIVE_KEYFRAME_NONE;
    return;
  }

  /* The active keyframe should always be selected. */
  BLI_assert_msg(BEZT_ISSEL_ANY(active_bezt), "active keyframe must be selected");

  fcu->active_keyframe_index = (int)offset;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/fcurve.c */
eFCU_Cycle_Type BKE_fcurve_get_cycle_type(const FCurve *fcu)
{
  FModifier *fcm = fcu->modifiers.first;

  if (!fcm || fcm->type != FMODIFIER_TYPE_CYCLES) {
    return FCU_CYCLE_NONE;
  }

  if (fcm->flag & (FMODIFIER_FLAG_DISABLED | FMODIFIER_FLAG_MUTED)) {
    return FCU_CYCLE_NONE;
  }

  if (fcm->flag & (FMODIFIER_FLAG_RANGERESTRICT | FMODIFIER_FLAG_USEINFLUENCE)) {
    return FCU_CYCLE_NONE;
  }

  FMod_Cycles *data = (FMod_Cycles *)fcm->data;

  if (data && data->after_cycles == 0 && data->before_cycles == 0) {
    if (data->before_mode == FCM_EXTRAPOLATE_CYCLIC && data->after_mode == FCM_EXTRAPOLATE_CYCLIC)
    {
      return FCU_CYCLE_PERFECT;
    }

    if (ELEM(data->before_mode, FCM_EXTRAPOLATE_CYCLIC, FCM_EXTRAPOLATE_CYCLIC_OFFSET) &&
        ELEM(data->after_mode, FCM_EXTRAPOLATE_CYCLIC, FCM_EXTRAPOLATE_CYCLIC_OFFSET))
    {
      return FCU_CYCLE_OFFSET;
    }
  }

  return FCU_CYCLE_NONE;
}

bool BKE_fcurve_is_cyclic(const FCurve *fcu)
{
  return BKE_fcurve_get_cycle_type(fcu) != FCU_CYCLE_NONE;
}

/* Shifts 'in' by the difference in coordinates between 'to' and 'from',
 * using 'out' as the output buffer.
 * When 'to' and 'from' are end points of the loop, this moves the 'in' point one loop cycle.
 */
static BezTriple *cycle_offset_triple(
    bool cycle, BezTriple *out, const BezTriple *in, const BezTriple *from, const BezTriple *to)
{
  if (!cycle) {
    return NULL;
  }

  memcpy(out, in, sizeof(BezTriple));

  float delta[3];
  sub_v3_v3v3(delta, to->vec[1], from->vec[1]);

  for (int i = 0; i < 3; i++) {
    add_v3_v3(out->vec[i], delta);
  }

  return out;
}

void BKE_fcurve_handles_recalc_ex(FCurve *fcu, eBezTriple_Flag handle_sel_flag)
{
  int a = fcu->totvert;

  /* Error checking:
   * - Need at least two points.
   * - Need bezier keys.
   * - Only bezier-interpolation has handles (for now).
   */
  if (ELEM(NULL, fcu, fcu->bezt) || (a < 2) /*|| ELEM(fcu->ipo, BEZT_IPO_CONST, BEZT_IPO_LIN) */) {
    return;
  }

  /* If the first modifier is Cycles, smooth the curve through the cycle. */
  BezTriple *first = &fcu->bezt[0], *last = &fcu->bezt[fcu->totvert - 1];
  BezTriple tmp;

  const bool cycle = BKE_fcurve_is_cyclic(fcu) && BEZT_IS_AUTOH(first) && BEZT_IS_AUTOH(last);

  /* Get initial pointers. */
  BezTriple *bezt = fcu->bezt;
  BezTriple *prev = cycle_offset_triple(cycle, &tmp, &fcu->bezt[fcu->totvert - 2], last, first);
  BezTriple *next = (bezt + 1);

  /* Loop over all beztriples, adjusting handles. */
  while (a--) {
    /* Clamp timing of handles to be on either side of beztriple. */
    if (bezt->vec[0][0] > bezt->vec[1][0]) {
      bezt->vec[0][0] = bezt->vec[1][0];
    }
    if (bezt->vec[2][0] < bezt->vec[1][0]) {
      bezt->vec[2][0] = bezt->vec[1][0];
    }

    /* Calculate auto-handles. */
    BKE_nurb_handle_calc_ex(bezt, prev, next, handle_sel_flag, true, fcu->auto_smoothing);

    /* For automatic ease in and out. */
    if (BEZT_IS_AUTOH(bezt) && !cycle) {
      /* Only do this on first or last beztriple. */
      if (ELEM(a, 0, fcu->totvert - 1)) {
        /* Set both handles to have same horizontal value as keyframe. */
        if (fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT) {
          bezt->vec[0][1] = bezt->vec[2][1] = bezt->vec[1][1];
          /* Remember that these keyframes are special, they don't need to be adjusted. */
          bezt->auto_handle_type = HD_AUTOTYPE_LOCKED_FINAL;
        }
      }
    }

    /* Avoid total smoothing failure on duplicate keyframes (can happen during grab). */
    if (prev && prev->vec[1][0] >= bezt->vec[1][0]) {
      prev->auto_handle_type = bezt->auto_handle_type = HD_AUTOTYPE_LOCKED_FINAL;
    }

    /* Advance pointers for next iteration. */
    prev = bezt;

    if (a == 1) {
      next = cycle_offset_triple(cycle, &tmp, &fcu->bezt[1], first, last);
    }
    else if (next != NULL) {
      next++;
    }

    bezt++;
  }

  /* If cyclic extrapolation and Auto Clamp has triggered, ensure it is symmetric. */
  if (cycle && (first->auto_handle_type != HD_AUTOTYPE_NORMAL ||
                last->auto_handle_type != HD_AUTOTYPE_NORMAL))
  {
    first->vec[0][1] = first->vec[2][1] = first->vec[1][1];
    last->vec[0][1] = last->vec[2][1] = last->vec[1][1];
    first->auto_handle_type = last->auto_handle_type = HD_AUTOTYPE_LOCKED_FINAL;
  }

  /* Do a second pass for auto handle: compute the handle to have 0 acceleration step. */
  if (fcu->auto_smoothing != FCURVE_SMOOTH_NONE) {
    BKE_nurb_handle_smooth_fcurve(fcu->bezt, fcu->totvert, cycle);
  }
}

void BKE_fcurve_handles_recalc(FCurve *fcu)
{
  BKE_fcurve_handles_recalc_ex(fcu, SELECT);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/fcurve.c */
void BKE_fcurve_correct_bezpart(const float v1[2], float v2[2], float v3[2], const float v4[2])
{
  float h1[2], h2[2], len1, len2, len, fac;

  /* Calculate handle deltas. */
  h1[0] = v1[0] - v2[0];
  h1[1] = v1[1] - v2[1];

  h2[0] = v4[0] - v3[0];
  h2[1] = v4[1] - v3[1];

  /* Calculate distances:
   * - len  = Span of time between keyframes.
   * - len1 = Length of handle of start key.
   * - len2 = Length of handle of end key.
   */
  len = v4[0] - v1[0];
  len1 = fabsf(h1[0]);
  len2 = fabsf(h2[0]);

  /* If the handles have no length, no need to do any corrections. */
  if ((len1 + len2) == 0.0f) {
    return;
  }

  /* To prevent looping or rewinding, handles cannot
   * exceed the adjacent key-frames time position. */
  if (len1 > len) {
    fac = len / len1;
    v2[0] = (v1[0] - fac * h1[0]);
    v2[1] = (v1[1] - fac * h1[1]);
  }

  if (len2 > len) {
    fac = len / len2;
    v3[0] = (v4[0] - fac * h2[0]);
    v3[1] = (v4[1] - fac * h2[1]);
  }
}

/**
 * Find roots of cubic equation (c0 + c1 x + c2 x^2 + c3 x^3)
 * \return number of roots in `o`.
 *
 * \note it is up to the caller to allocate enough memory for `o`.
 */
static int solve_cubic(double c0, double c1, double c2, double c3, float *o)
{
  double a, b, c, p, q, d, t, phi;
  int nr = 0;

  if (c3 != 0.0) {
    a = c2 / c3;
    b = c1 / c3;
    c = c0 / c3;
    a = a / 3;

    p = b / 3 - a * a;
    q = (2 * a * a * a - a * b + c) / 2;
    d = q * q + p * p * p;

    if (d > 0.0) {
      t = sqrt(d);
      o[0] = (float)(sqrt3d(-q + t) + sqrt3d(-q - t) - a);

      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        return 1;
      }
      return 0;
    }

    if (d == 0.0) {
      t = sqrt3d(-q);
      o[0] = (float)(2 * t - a);

      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        nr++;
      }
      o[nr] = (float)(-t - a);

      if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
        return nr + 1;
      }
      return nr;
    }

    phi = acos(-q / sqrt(-(p * p * p)));
    t = sqrt(-p);
    p = cos(phi / 3);
    q = sqrt(3 - 3 * p * p);
    o[0] = (float)(2 * t * p - a);

    if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
      nr++;
    }
    o[nr] = (float)(-t * (p + q) - a);

    if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
      nr++;
    }
    o[nr] = (float)(-t * (p - q) - a);

    if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
      return nr + 1;
    }
    return nr;
  }
  a = c2;
  b = c1;
  c = c0;

  if (a != 0.0) {
    /* Discriminant */
    p = b * b - 4 * a * c;

    if (p > 0) {
      p = sqrt(p);
      o[0] = (float)((-b - p) / (2 * a));

      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        nr++;
      }
      o[nr] = (float)((-b + p) / (2 * a));

      if ((o[nr] >= (float)SMALL) && (o[nr] <= 1.000001f)) {
        return nr + 1;
      }
      return nr;
    }

    if (p == 0) {
      o[0] = (float)(-b / (2 * a));
      if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
        return 1;
      }
    }

    return 0;
  }

  if (b != 0.0) {
    o[0] = (float)(-c / b);

    if ((o[0] >= (float)SMALL) && (o[0] <= 1.000001f)) {
      return 1;
    }
    return 0;
  }

  if (c == 0.0) {
    o[0] = 0.0;
    return 1;
  }

  return 0;
}

/* Find root(s) ('zero') of a Bezier curve. */
static int findzero(float x, float q0, float q1, float q2, float q3, float *o)
{
  const double c0 = q0 - x;
  const double c1 = 3.0f * (q1 - q0);
  const double c2 = 3.0f * (q0 - 2.0f * q1 + q2);
  const double c3 = q3 - q0 + 3.0f * (q1 - q2);

  return solve_cubic(c0, c1, c2, c3, o);
}

static void berekeny(float f1, float f2, float f3, float f4, float *o, int b)
{
  float t, c0, c1, c2, c3;
  int a;

  c0 = f1;
  c1 = 3.0f * (f2 - f1);
  c2 = 3.0f * (f1 - 2.0f * f2 + f3);
  c3 = f4 - f1 + 3.0f * (f2 - f3);

  for (a = 0; a < b; a++) {
    t = o[a];
    o[a] = c0 + t * c1 + t * t * c2 + t * t * t * c3;
  }
}

static void fcurve_bezt_free(FCurve *fcu)
{
  MEM_SAFE_FREE(fcu->bezt);
  fcu->totvert = 0;
}

bool BKE_fcurve_bezt_subdivide_handles(struct BezTriple *bezt,
                                       struct BezTriple *prev,
                                       struct BezTriple *next,
                                       float *r_pdelta)
{
  /* The four points that make up this section of the Bezier curve. */
  const float *prev_coords = prev->vec[1];
  float *prev_handle_right = prev->vec[2];
  float *next_handle_left = next->vec[0];
  const float *next_coords = next->vec[1];

  float *new_handle_left = bezt->vec[0];
  const float *new_coords = bezt->vec[1];
  float *new_handle_right = bezt->vec[2];

  if (new_coords[0] <= prev_coords[0] || new_coords[0] >= next_coords[0]) {
    /* The new keyframe is outside the (prev_coords, next_coords) range. */
    return false;
  }

  /* Apply evaluation-time limits and compute the effective curve. */
  BKE_fcurve_correct_bezpart(prev_coords, prev_handle_right, next_handle_left, next_coords);
  float roots[4];
  if (!findzero(new_coords[0],
                prev_coords[0],
                prev_handle_right[0],
                next_handle_left[0],
                next_coords[0],
                roots))
  {
    return false;
  }

  const float t = roots[0]; /* Percentage of the curve at which the split should occur. */
  if (t <= 0.0f || t >= 1.0f) {
    /* The split would occur outside the curve, which isn't possible. */
    return false;
  }

  /* De Casteljau split, requires three iterations of splitting.
   * See https://pomax.github.io/bezierinfo/#decasteljau */
  float split1[3][2], split2[2][2], split3[2];
  interp_v2_v2v2(split1[0], prev_coords, prev_handle_right, t);
  interp_v2_v2v2(split1[1], prev_handle_right, next_handle_left, t);
  interp_v2_v2v2(split1[2], next_handle_left, next_coords, t);
  interp_v2_v2v2(split2[0], split1[0], split1[1], t);
  interp_v2_v2v2(split2[1], split1[1], split1[2], t);
  interp_v2_v2v2(split3, split2[0], split2[1], t);

  /* Update the existing handles. */
  copy_v2_v2(prev_handle_right, split1[0]);
  copy_v2_v2(next_handle_left, split1[2]);

  float diff_coords[2];
  sub_v2_v2v2(diff_coords, new_coords, split3);
  add_v2_v2v2(new_handle_left, split2[0], diff_coords);
  add_v2_v2v2(new_handle_right, split2[1], diff_coords);

  *r_pdelta = diff_coords[1];
  return true;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/fcurve.c */
static float fcurve_eval_keyframes_extrapolate(
    FCurve *fcu, BezTriple *bezts, float evaltime, int endpoint_offset, int direction_to_neighbor)
{
  /* The first/last keyframe. */
  const BezTriple *endpoint_bezt = bezts + endpoint_offset;
  /* The second (to last) keyframe. */
  const BezTriple *neighbor_bezt = endpoint_bezt + direction_to_neighbor;

  if (endpoint_bezt->ipo == BEZT_IPO_CONST || fcu->extend == FCURVE_EXTRAPOLATE_CONSTANT ||
      (fcu->flag & FCURVE_DISCRETE_VALUES) != 0)
  {
    /* Constant (BEZT_IPO_HORIZ) extrapolation or constant interpolation, so just extend the
     * endpoint's value. */
    return endpoint_bezt->vec[1][1];
  }

  if (endpoint_bezt->ipo == BEZT_IPO_LIN) {
    /* Use the next center point instead of our own handle for linear interpolated extrapolate. */
    if (fcu->totvert == 1) {
      return endpoint_bezt->vec[1][1];
    }

    const float dx = endpoint_bezt->vec[1][0] - evaltime;
    float fac = neighbor_bezt->vec[1][0] - endpoint_bezt->vec[1][0];

    /* Prevent division by zero. */
    if (fac == 0.0f) {
      return endpoint_bezt->vec[1][1];
    }

    fac = (neighbor_bezt->vec[1][1] - endpoint_bezt->vec[1][1]) / fac;
    return endpoint_bezt->vec[1][1] - (fac * dx);
  }

  /* Use the gradient of the second handle (later) of neighbor to calculate the gradient and thus
   * the value of the curve at evaluation time. */
  const int handle = direction_to_neighbor > 0 ? 0 : 2;
  const float dx = endpoint_bezt->vec[1][0] - evaltime;
  float fac = endpoint_bezt->vec[1][0] - endpoint_bezt->vec[handle][0];

  /* Prevent division by zero. */
  if (fac == 0.0f) {
    return endpoint_bezt->vec[1][1];
  }

  fac = (endpoint_bezt->vec[1][1] - endpoint_bezt->vec[handle][1]) / fac;
  return endpoint_bezt->vec[1][1] - (fac * dx);
}

static float fcurve_eval_keyframes_interpolate(const FCurve *fcu,
                                               const BezTriple *bezts,
                                               float evaltime)
{
  const float eps = 1.e-8f;
  uint a;

  /* Evaltime occurs somewhere in the middle of the curve. */
  bool exact = false;

  /* Use binary search to find appropriate keyframes...
   *
   * The threshold here has the following constraints:
   * - 0.001 is too coarse:
   *   We get artifacts with 2cm driver movements at 1BU = 1m (see #40332).
   *
   * - 0.00001 is too fine:
   *   Weird errors, like selecting the wrong keyframe range (see #39207), occur.
   *   This lower bound was established in b888a32eee8147b028464336ad2404d8155c64dd.
   */
  a = BKE_fcurve_bezt_binarysearch_index_ex(bezts, evaltime, fcu->totvert, 0.0001, &exact);
  const BezTriple *bezt = bezts + a;

  if (exact) {
    /* Index returned must be interpreted differently when it sits on top of an existing keyframe
     * - That keyframe is the start of the segment we need (see action_bug_2.blend in #39207).
     */
    return bezt->vec[1][1];
  }

  /* Index returned refers to the keyframe that the eval-time occurs *before*
   * - hence, that keyframe marks the start of the segment we're dealing with.
   */
  const BezTriple *prevbezt = (a > 0) ? (bezt - 1) : bezt;

  /* Use if the key is directly on the frame, in rare cases this is needed else we get 0.0 instead.
   * XXX: consult #39207 for examples of files where failure of these checks can cause issues. */
  if (fabsf(bezt->vec[1][0] - evaltime) < eps) {
    return bezt->vec[1][1];
  }

  if (evaltime < prevbezt->vec[1][0] || bezt->vec[1][0] < evaltime) {
    if (G.debug & G_DEBUG) {
      printf("   ERROR: failed eval - p=%f b=%f, t=%f (%f)\n",
             prevbezt->vec[1][0],
             bezt->vec[1][0],
             evaltime,
             fabsf(bezt->vec[1][0] - evaltime));
    }
    return 0.0f;
  }

  /* Evaltime occurs within the interval defined by these two keyframes. */
  const float begin = prevbezt->vec[1][1];
  const float change = bezt->vec[1][1] - prevbezt->vec[1][1];
  const float duration = bezt->vec[1][0] - prevbezt->vec[1][0];
  const float time = evaltime - prevbezt->vec[1][0];
  const float amplitude = prevbezt->amplitude;
  const float period = prevbezt->period;

  /* Value depends on interpolation mode. */
  if ((prevbezt->ipo == BEZT_IPO_CONST) || (fcu->flag & FCURVE_DISCRETE_VALUES) || (duration == 0))
  {
    /* Constant (evaltime not relevant, so no interpolation needed). */
    return prevbezt->vec[1][1];
  }

  switch (prevbezt->ipo) {
    /* Interpolation ...................................... */
    case BEZT_IPO_BEZ: {
      float v1[2], v2[2], v3[2], v4[2], opl[32];

      /* Bezier interpolation. */
      /* (v1, v2) are the first keyframe and its 2nd handle. */
      v1[0] = prevbezt->vec[1][0];
      v1[1] = prevbezt->vec[1][1];
      v2[0] = prevbezt->vec[2][0];
      v2[1] = prevbezt->vec[2][1];
      /* (v3, v4) are the last keyframe's 1st handle + the last keyframe. */
      v3[0] = bezt->vec[0][0];
      v3[1] = bezt->vec[0][1];
      v4[0] = bezt->vec[1][0];
      v4[1] = bezt->vec[1][1];

      if (fabsf(v1[1] - v4[1]) < FLT_EPSILON && fabsf(v2[1] - v3[1]) < FLT_EPSILON &&
          fabsf(v3[1] - v4[1]) < FLT_EPSILON)
      {
        /* Optimization: If all the handles are flat/at the same values,
         * the value is simply the shared value (see #40372 -> F91346).
         */
        return v1[1];
      }
      /* Adjust handles so that they don't overlap (forming a loop). */
      BKE_fcurve_correct_bezpart(v1, v2, v3, v4);

      /* Try to get a value for this position - if failure, try another set of points. */
      if (!findzero(evaltime, v1[0], v2[0], v3[0], v4[0], opl)) {
        if (G.debug & G_DEBUG) {
          printf("    ERROR: findzero() failed at %f with %f %f %f %f\n",
                 evaltime,
                 v1[0],
                 v2[0],
                 v3[0],
                 v4[0]);
        }
        return 0.0;
      }

      berekeny(v1[1], v2[1], v3[1], v4[1], opl, 1);
      return opl[0];
    }
    case BEZT_IPO_LIN:
      /* Linear - simply linearly interpolate between values of the two keyframes. */
      return BLI_easing_linear_ease(time, begin, change, duration);

    /* Easing ............................................ */
    case BEZT_IPO_BACK:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_back_ease_in(time, begin, change, duration, prevbezt->back);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_back_ease_out(time, begin, change, duration, prevbezt->back);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_back_ease_in_out(time, begin, change, duration, prevbezt->back);

        default: /* Default/Auto: same as ease out. */
          return BLI_easing_back_ease_out(time, begin, change, duration, prevbezt->back);
      }
      break;

    case BEZT_IPO_BOUNCE:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_bounce_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_bounce_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_bounce_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease out. */
          return BLI_easing_bounce_ease_out(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_CIRC:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_circ_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_circ_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_circ_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_circ_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_CUBIC:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_cubic_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_cubic_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_cubic_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_cubic_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_ELASTIC:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_elastic_ease_in(time, begin, change, duration, amplitude, period);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_elastic_ease_in_out(time, begin, change, duration, amplitude, period);

        default: /* Default/Auto: same as ease out. */
          return BLI_easing_elastic_ease_out(time, begin, change, duration, amplitude, period);
      }
      break;

    case BEZT_IPO_EXPO:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_expo_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_expo_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_expo_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_expo_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_QUAD:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quad_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quad_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quad_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_quad_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_QUART:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quart_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quart_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quart_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_quart_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_QUINT:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_quint_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_quint_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_quint_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_quint_ease_in(time, begin, change, duration);
      }
      break;

    case BEZT_IPO_SINE:
      switch (prevbezt->easing) {
        case BEZT_IPO_EASE_IN:
          return BLI_easing_sine_ease_in(time, begin, change, duration);
        case BEZT_IPO_EASE_OUT:
          return BLI_easing_sine_ease_out(time, begin, change, duration);
        case BEZT_IPO_EASE_IN_OUT:
          return BLI_easing_sine_ease_in_out(time, begin, change, duration);

        default: /* Default/Auto: same as ease in. */
          return BLI_easing_sine_ease_in(time, begin, change, duration);
      }
      break;

    default:
      return prevbezt->vec[1][1];
  }

  return 0.0f;
}

/* Calculate F-Curve value for 'evaltime' using #BezTriple keyframes. */
static float fcurve_eval_keyframes(FCurve *fcu, BezTriple *bezts, float evaltime)
{
  if (evaltime <= bezts->vec[1][0]) {
    return fcurve_eval_keyframes_extrapolate(fcu, bezts, evaltime, 0, +1);
  }

  const BezTriple *lastbezt = bezts + fcu->totvert - 1;
  if (lastbezt->vec[1][0] <= evaltime) {
    return fcurve_eval_keyframes_extrapolate(fcu, bezts, evaltime, fcu->totvert - 1, -1);
  }

  return fcurve_eval_keyframes_interpolate(fcu, bezts, evaltime);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/animation/keyframing.c */
static void replace_bezt_keyframe_ypos(BezTriple *dst, const BezTriple *bezt)
{
  /* just change the values when replacing, so as to not overwrite handles */
  float dy = bezt->vec[1][1] - dst->vec[1][1];

  /* just apply delta value change to the handle values */
  dst->vec[0][1] += dy;
  dst->vec[1][1] += dy;
  dst->vec[2][1] += dy;

  dst->f1 = bezt->f1;
  dst->f2 = bezt->f2;
  dst->f3 = bezt->f3;

  /* TODO: perform some other operations? */
}

int insert_bezt_fcurve(FCurve *fcu, const BezTriple *bezt, eInsertKeyFlags flag)
{
  int i = 0;

  /* are there already keyframes? */
  if (fcu->bezt) {
    bool replace;
    i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, bezt->vec[1][0], fcu->totvert, &replace);

    /* replace an existing keyframe? */
    if (replace) {
      /* sanity check: 'i' may in rare cases exceed arraylen */
      if ((i >= 0) && (i < fcu->totvert)) {
        if (flag & INSERTKEY_OVERWRITE_FULL) {
          fcu->bezt[i] = *bezt;
        }
        else {
          replace_bezt_keyframe_ypos(&fcu->bezt[i], bezt);
        }

        if (flag & INSERTKEY_CYCLE_AWARE) {
          /* If replacing an end point of a cyclic curve without offset,
           * modify the other end too. */
          if (ELEM(i, 0, fcu->totvert - 1) && BKE_fcurve_get_cycle_type(fcu) == FCU_CYCLE_PERFECT)
          {
            replace_bezt_keyframe_ypos(&fcu->bezt[i == 0 ? fcu->totvert - 1 : 0], bezt);
          }
        }
      }
    }
    /* Keyframing modes allow not replacing the keyframe. */
    else if ((flag & INSERTKEY_REPLACE) == 0) {
      /* insert new - if we're not restricted to replacing keyframes only */
      BezTriple *newb = MEM_callocN((fcu->totvert + 1) * sizeof(BezTriple), "beztriple");

      /* Add the beztriples that should occur before the beztriple to be pasted
       * (originally in fcu). */
      if (i > 0) {
        memcpy(newb, fcu->bezt, i * sizeof(BezTriple));
      }

      /* add beztriple to paste at index i */
      *(newb + i) = *bezt;

      /* add the beztriples that occur after the beztriple to be pasted (originally in fcu) */
      if (i < fcu->totvert) {
        memcpy(newb + i + 1, fcu->bezt + i, (fcu->totvert - i) * sizeof(BezTriple));
      }

      /* replace (+ free) old with new, only if necessary to do so */
      MEM_freeN(fcu->bezt);
      fcu->bezt = newb;

      fcu->totvert++;
    }
    else {
      return -1;
    }
  }
  /* no keyframes already, but can only add if...
   * 1) keyframing modes say that keyframes can only be replaced, so adding new ones won't know
   * 2) there are no samples on the curve
   *    NOTE: maybe we may want to allow this later when doing samples -> bezt conversions,
   *    but for now, having both is asking for trouble
   */
  else if ((flag & INSERTKEY_REPLACE) == 0 && (fcu->fpt == NULL)) {
    /* create new keyframes array */
    fcu->bezt = MEM_callocN(sizeof(BezTriple), "beztriple");
    *(fcu->bezt) = *bezt;
    fcu->totvert = 1;
  }
  /* cannot add anything */
  else {
    /* return error code -1 to prevent any misunderstandings */
    return -1;
  }

  /* we need to return the index, so that some tools which do post-processing can
   * detect where we added the BezTriple in the array
   */
  return i;
}

/**
 * Update the FCurve to allow insertion of `bezt` without modifying the curve shape.
 *
 * Checks whether it is necessary to apply Bezier subdivision due to involvement of non-auto
 * handles. If necessary, changes `bezt` handles from Auto to Aligned.
 *
 * \param bezt: key being inserted
 * \param prev: keyframe before that key
 * \param next: keyframe after that key
 */
static void subdivide_nonauto_handles(const FCurve *fcu,
                                      BezTriple *bezt,
                                      BezTriple *prev,
                                      BezTriple *next)
{
  if (prev->ipo != BEZT_IPO_BEZ || bezt->ipo != BEZT_IPO_BEZ) {
    return;
  }

  /* Don't change Vector handles, or completely auto regions. */
  const bool bezt_auto = BEZT_IS_AUTOH(bezt) || (bezt->h1 == HD_VECT && bezt->h2 == HD_VECT);
  const bool prev_auto = BEZT_IS_AUTOH(prev) || (prev->h2 == HD_VECT);
  const bool next_auto = BEZT_IS_AUTOH(next) || (next->h1 == HD_VECT);
  if (bezt_auto && prev_auto && next_auto) {
    return;
  }

  /* Subdivide the curve. */
  float delta;
  if (!BKE_fcurve_bezt_subdivide_handles(bezt, prev, next, &delta)) {
    return;
  }

  /* Decide when to force auto to manual. */
  if (!BEZT_IS_AUTOH(bezt)) {
    return;
  }
  if ((prev_auto || next_auto) && fcu->auto_smoothing == FCURVE_SMOOTH_CONT_ACCEL) {
    const float hx = bezt->vec[1][0] - bezt->vec[0][0];
    const float dx = bezt->vec[1][0] - prev->vec[1][0];

    /* This mode always uses 1/3 of key distance for handle x size. */
    const bool auto_works_well = fabsf(hx - dx / 3.0f) < 0.001f;
    if (auto_works_well) {
      return;
    }
  }

  /* Turn off auto mode. */
  bezt->h1 = bezt->h2 = HD_ALIGN;
}

int insert_vert_fcurve(
    FCurve *fcu, float x, float y, eBezTriple_KeyframeType keyframe_type, eInsertKeyFlags flag)
{
  BezTriple beztr = {{{0}}};
  uint oldTot = fcu->totvert;
  int a;

  /* set all three points, for nicer start position
   * NOTE: +/- 1 on vec.x for left and right handles is so that 'free' handles work ok...
   */
  beztr.vec[0][0] = x - 1.0f;
  beztr.vec[0][1] = y;
  beztr.vec[1][0] = x;
  beztr.vec[1][1] = y;
  beztr.vec[2][0] = x + 1.0f;
  beztr.vec[2][1] = y;
  beztr.f1 = beztr.f2 = beztr.f3 = SELECT;

  /* set default handle types and interpolation mode */
  if (flag & INSERTKEY_NO_USERPREF) {
    /* for Py-API, we want scripts to have predictable behavior,
     * hence the option to not depend on the userpref defaults
     */
    beztr.h1 = beztr.h2 = HD_AUTO_ANIM;
    beztr.ipo = BEZT_IPO_BEZ;
  }
  else {
    /* For UI usage - defaults should come from the user-preferences and/or tool-settings. */
    beztr.h1 = beztr.h2 = U.keyhandles_new; /* use default handle type here */

    /* use default interpolation mode, with exceptions for int/discrete values */
    beztr.ipo = U.ipo_new;
  }

  /* interpolation type used is constrained by the type of values the curve can take */
  if (fcu->flag & FCURVE_DISCRETE_VALUES) {
    beztr.ipo = BEZT_IPO_CONST;
  }
  else if ((beztr.ipo == BEZT_IPO_BEZ) && (fcu->flag & FCURVE_INT_VALUES)) {
    beztr.ipo = BEZT_IPO_LIN;
  }

  /* set keyframe type value (supplied), which should come from the scene settings in most cases */
  BEZKEYTYPE(&beztr) = keyframe_type;

  /* set default values for "easing" interpolation mode settings
   * NOTE: Even if these modes aren't currently used, if users switch
   *       to these later, we want these to work in a sane way out of
   *       the box.
   */

  /* "back" easing - this value used to be used when overshoot=0, but that
   *                 introduced discontinuities in how the param worked. */
  beztr.back = 1.70158f;

  /* "elastic" easing - values here were hand-optimized for a default duration of
   *                    ~10 frames (typical mograph motion length) */
  beztr.amplitude = 0.8f;
  beztr.period = 4.1f;

  /* add temp beztriple to keyframes */
  a = insert_bezt_fcurve(fcu, &beztr, flag);
  BKE_fcurve_active_keyframe_set(fcu, &fcu->bezt[a]);

  /* what if 'a' is a negative index?
   * for now, just exit to prevent any segfaults
   */
  if (a < 0) {
    return -1;
  }

  /* Set handle-type and interpolation. */
  if ((fcu->totvert > 2) && (flag & INSERTKEY_REPLACE) == 0) {
    BezTriple *bezt = (fcu->bezt + a);

    /* Set interpolation from previous (if available),
     * but only if we didn't just replace some keyframe:
     * - Replacement is indicated by no-change in number of verts.
     * - When replacing, the user may have specified some interpolation that should be kept.
     */
    if (fcu->totvert > oldTot) {
      if (a > 0) {
        bezt->ipo = (bezt - 1)->ipo;
      }
      else if (a < fcu->totvert - 1) {
        bezt->ipo = (bezt + 1)->ipo;
      }

      if (0 < a && a < (fcu->totvert - 1) && (flag & INSERTKEY_OVERWRITE_FULL) == 0) {
        subdivide_nonauto_handles(fcu, bezt, bezt - 1, bezt + 1);
      }
    }
  }

  /* don't recalculate handles if fast is set
   * - this is a hack to make importers faster
   * - we may calculate twice (due to auto-handle needing to be calculated twice)
   */
  if ((flag & INSERTKEY_FAST) == 0) {
    BKE_fcurve_handles_recalc(fcu);
  }

  /* return the index at which the keyframe was added */
  return a;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_utils.c */
void ED_armature_edit_sync_selection(ListBase *edbo)
{
  EditBone *ebo;

  for (ebo = edbo->first; ebo; ebo = ebo->next) {
    /* if bone is not selectable, we shouldn't alter this setting... */
    if ((ebo->flag & BONE_UNSELECTABLE) == 0) {
      if ((ebo->flag & BONE_CONNECTED) && (ebo->parent)) {
        if (ebo->parent->flag & BONE_TIPSEL) {
          ebo->flag |= BONE_ROOTSEL;
        }
        else {
          ebo->flag &= ~BONE_ROOTSEL;
        }
      }

      if ((ebo->flag & BONE_TIPSEL) && (ebo->flag & BONE_ROOTSEL)) {
        ebo->flag |= BONE_SELECTED;
      }
      else {
        ebo->flag &= ~BONE_SELECTED;
      }
    }
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_utils.c */
void ED_armature_ebone_remove_ex(bArmature *arm, EditBone *exBone, bool clear_connected)
{
  EditBone *curBone;

  /* Find any bones that refer to this bone */
  for (curBone = arm->edbo->first; curBone; curBone = curBone->next) {
    if (curBone->parent == exBone) {
      curBone->parent = exBone->parent;
      if (clear_connected) {
        curBone->flag &= ~BONE_CONNECTED;
      }
    }
  }

  bone_free(arm, exBone);
}

void ED_armature_ebone_remove(bArmature *arm, EditBone *exBone)
{
  ED_armature_ebone_remove_ex(arm, exBone, true);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_edit.c */
float ED_armature_ebone_roll_to_vector(const EditBone *bone,
                                       const float align_axis[3],
                                       const bool axis_only)
{
  float mat[3][3], nor[3];
  float vec[3], align_axis_proj[3], roll = 0.0f;

  BLI_ASSERT_UNIT_V3(align_axis);

  sub_v3_v3v3(nor, bone->tail, bone->head);

  /* If tail == head or the bone is aligned with the axis... */
  if (normalize_v3(nor) <= FLT_EPSILON ||
      (fabsf(dot_v3v3(align_axis, nor)) >= (1.0f - FLT_EPSILON))) {
    return roll;
  }

  vec_roll_to_mat3_normalized(nor, 0.0f, mat);

  /* project the new_up_axis along the normal */
  project_v3_v3v3_normalized(vec, align_axis, nor);
  sub_v3_v3v3(align_axis_proj, align_axis, vec);

  if (axis_only) {
    if (angle_v3v3(align_axis_proj, mat[2]) > (float)(M_PI_2)) {
      negate_v3(align_axis_proj);
    }
  }

  roll = angle_v3v3(align_axis_proj, mat[2]);

  cross_v3_v3v3(vec, mat[2], align_axis_proj);

  if (dot_v3v3(vec, nor) < 0.0f) {
    return -roll;
  }
  return roll;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_utils.c */
void ED_armature_ebone_to_mat3(EditBone *ebone, float r_mat[3][3])
{
  float delta[3], roll;

  /* Find the current bone matrix */
  sub_v3_v3v3(delta, ebone->tail, ebone->head);
  roll = ebone->roll;
  if (!normalize_v3(delta)) {
    /* Use the orientation of the parent bone if any. */
    const EditBone *ebone_parent = ebone->parent;
    if (ebone_parent) {
      sub_v3_v3v3(delta, ebone_parent->tail, ebone_parent->head);
      normalize_v3(delta);
      roll = ebone_parent->roll;
    }
  }

  vec_roll_to_mat3_normalized(delta, roll, r_mat);
}

void ED_armature_ebone_to_mat4(EditBone *ebone, float r_mat[4][4])
{
  float m3[3][3];

  ED_armature_ebone_to_mat3(ebone, m3);

  copy_m4_m3(r_mat, m3);
  copy_v3_v3(r_mat[3], ebone->head);
}

void ED_armature_ebone_from_mat3(EditBone *ebone, const float mat[3][3])
{
  float vec[3], roll;
  const float len = len_v3v3(ebone->head, ebone->tail);

  mat3_to_vec_roll(mat, vec, &roll);

  madd_v3_v3v3fl(ebone->tail, ebone->head, vec, len);
  ebone->roll = roll;
}

void ED_armature_ebone_from_mat4(EditBone *ebone, const float mat[4][4])
{
  float mat3[3][3];

  copy_m3_m4(mat3, mat);
  /* We want normalized matrix here, to be consistent with ebone_to_mat. */
  BLI_ASSERT_UNIT_M3(mat3);

  sub_v3_v3(ebone->tail, ebone->head);
  copy_v3_v3(ebone->head, mat[3]);
  add_v3_v3(ebone->tail, mat[3]);
  ED_armature_ebone_from_mat3(ebone, mat3);
}

EditBone *ED_armature_ebone_find_name(const ListBase *edbo, const char *name)
{
  return BLI_findstring(edbo, name, offsetof(EditBone, name));
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_utils.c */
EditBone *ED_armature_ebone_get_mirrored(const ListBase *edbo, EditBone *ebo)
{
  char name_flip[MAXBONENAME];

  if (ebo == NULL) {
    return NULL;
  }

  BLI_string_flip_side_name(name_flip, ebo->name, false, sizeof(name_flip));

  if (!STREQ(name_flip, ebo->name)) {
    return ED_armature_ebone_find_name(edbo, name_flip);
  }

  return NULL;
}

/* ------------------------------------- */

void armature_select_mirrored_ex(bArmature *arm, const int flag)
{
  BLI_assert((flag & ~(BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) == 0);
  /* Select mirrored bones */
  if (arm->flag & ARM_MIRROR_EDIT) {
    EditBone *curBone, *ebone_mirr;

    for (curBone = arm->edbo->first; curBone; curBone = curBone->next) {
      if (arm->layer & curBone->layer) {
        if (curBone->flag & flag) {
          ebone_mirr = ED_armature_ebone_get_mirrored(arm->edbo, curBone);
          if (ebone_mirr) {
            ebone_mirr->flag |= (curBone->flag & flag);
          }
        }
      }
    }
  }
}

void armature_select_mirrored(bArmature *arm)
{
  armature_select_mirrored_ex(arm, BONE_SELECTED);
}

void armature_tag_select_mirrored(bArmature *arm)
{
  EditBone *curBone;

  /* always untag */
  for (curBone = arm->edbo->first; curBone; curBone = curBone->next) {
    curBone->flag &= ~BONE_DONE;
  }

  /* Select mirrored bones */
  if (arm->flag & ARM_MIRROR_EDIT) {
    for (curBone = arm->edbo->first; curBone; curBone = curBone->next) {
      if (arm->layer & curBone->layer) {
        if (curBone->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL)) {
          EditBone *ebone_mirr = ED_armature_ebone_get_mirrored(arm->edbo, curBone);
          if (ebone_mirr && (ebone_mirr->flag & BONE_SELECTED) == 0) {
            ebone_mirr->flag |= BONE_DONE;
          }
        }
      }
    }

    for (curBone = arm->edbo->first; curBone; curBone = curBone->next) {
      if (curBone->flag & BONE_DONE) {
        EditBone *ebone_mirr = ED_armature_ebone_get_mirrored(arm->edbo, curBone);
        curBone->flag |= ebone_mirr->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);
      }
    }
  }
}

void armature_tag_unselect(bArmature *arm)
{
  EditBone *curBone;

  for (curBone = arm->edbo->first; curBone; curBone = curBone->next) {
    if (curBone->flag & BONE_DONE) {
      curBone->flag &= ~(BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL | BONE_DONE);
    }
  }
}

/* ------------------------------------- */

void ED_armature_ebone_transform_mirror_update(bArmature *arm, EditBone *ebo, bool check_select)
{
  /* TODO: When this function is called by property updates,
   * canceling the value change will not restore mirrored bone correctly. */

  /* Currently check_select==true when this function is called from a transform operator,
   * eg. from 3d viewport. */

  /* no layer check, correct mirror is more important */
  if (!check_select || ebo->flag & (BONE_TIPSEL | BONE_ROOTSEL)) {
    EditBone *eboflip = ED_armature_ebone_get_mirrored(arm->edbo, ebo);
    if (eboflip) {
      /* We assume X-axis flipping for now. */

      /* Always mirror roll, since it can be changed by moving either head or tail. */
      eboflip->roll = -ebo->roll;

      if (!check_select || ebo->flag & BONE_TIPSEL) {
        /* Mirror tail properties. */

        eboflip->tail[0] = -ebo->tail[0];
        eboflip->tail[1] = ebo->tail[1];
        eboflip->tail[2] = ebo->tail[2];
        eboflip->rad_tail = ebo->rad_tail;
        eboflip->curve_out_x = -ebo->curve_out_x;
        eboflip->curve_out_z = ebo->curve_out_z;
        copy_v3_v3(eboflip->scale_out, ebo->scale_out);
        eboflip->ease2 = ebo->ease2;
        eboflip->roll2 = -ebo->roll2;

        /* Also move connected children, in case children's name aren't mirrored properly. */
        EditBone *children;
        for (children = arm->edbo->first; children; children = children->next) {
          if (children->parent == eboflip && children->flag & BONE_CONNECTED) {
            copy_v3_v3(children->head, eboflip->tail);
            children->rad_head = ebo->rad_tail;
          }
        }
      }

      if (!check_select || ebo->flag & BONE_ROOTSEL) {
        /* Mirror head properties. */
        eboflip->head[0] = -ebo->head[0];
        eboflip->head[1] = ebo->head[1];
        eboflip->head[2] = ebo->head[2];
        eboflip->rad_head = ebo->rad_head;

        eboflip->curve_in_x = -ebo->curve_in_x;
        eboflip->curve_in_z = ebo->curve_in_z;
        copy_v3_v3(eboflip->scale_in, ebo->scale_in);
        eboflip->ease1 = ebo->ease1;
        eboflip->roll1 = -ebo->roll1;

        /* Also move connected parent, in case parent's name isn't mirrored properly. */
        if (eboflip->parent && eboflip->flag & BONE_CONNECTED) {
          EditBone *parent = eboflip->parent;
          copy_v3_v3(parent->tail, eboflip->head);
          parent->rad_tail = ebo->rad_head;
        }
      }

      if (!check_select || ebo->flag & BONE_SELECTED) {
        /* Mirror bone body properties (both head and tail are selected). */
        /* TODO: These values can also be changed from pose mode,
         * so only mirroring them in edit mode is not ideal. */
        eboflip->dist = ebo->dist;
        eboflip->weight = ebo->weight;

        eboflip->segments = ebo->segments;
        eboflip->xwidth = ebo->xwidth;
        eboflip->zwidth = ebo->zwidth;
      }
    }
  }
}
/* END VERBATIM */

/* ED_armature_edit_transform_mirror_update (armature_utils.c:436-442) on the bArmature. */
void pg_bke_edit_transform_mirror_update(bArmature *arm)
{
  LISTBASE_FOREACH (EditBone *, ebo, arm->edbo) {
    ED_armature_ebone_transform_mirror_update(arm, ebo, true);
  }
}

/* BEGIN VERBATIM source/blender/editors/armature/armature_utils.c */
static void armature_finalize_restpose(ListBase *bonelist, ListBase *editbonelist)
{
  Bone *curBone;
  EditBone *ebone;

  for (curBone = bonelist->first; curBone; curBone = curBone->next) {
    /* Set bone's local head/tail.
     * Note that it's important to use final parent's restpose (arm_mat) here,
     * instead of setting those values from editbone's matrix (see #46010). */
    if (curBone->parent) {
      float parmat_inv[4][4];

      invert_m4_m4(parmat_inv, curBone->parent->arm_mat);

      /* Get the new head and tail */
      sub_v3_v3v3(curBone->head, curBone->arm_head, curBone->parent->arm_tail);
      sub_v3_v3v3(curBone->tail, curBone->arm_tail, curBone->parent->arm_tail);

      mul_mat3_m4_v3(parmat_inv, curBone->head);
      mul_mat3_m4_v3(parmat_inv, curBone->tail);
    }
    else {
      copy_v3_v3(curBone->head, curBone->arm_head);
      copy_v3_v3(curBone->tail, curBone->arm_tail);
    }

    /* Set local matrix and arm_mat (restpose).
     * Do not recurse into children here, armature_finalize_restpose() is already recursive. */
    BKE_armature_where_is_bone(curBone, curBone->parent, false);

    /* Find the associated editbone */
    for (ebone = editbonelist->first; ebone; ebone = ebone->next) {
      if (ebone->temp.bone == curBone) {
        float premat[3][3];
        float postmat[3][3];
        float difmat[3][3];
        float imat[3][3];

        /* Get the ebone premat and its inverse. */
        ED_armature_ebone_to_mat3(ebone, premat);
        invert_m3_m3(imat, premat);

        /* Get the bone postmat. */
        copy_m3_m4(postmat, curBone->arm_mat);

        mul_m3_m3m3(difmat, imat, postmat);

#if 0
        printf("Bone %s\n", curBone->name);
        print_m4("premat", premat);
        print_m4("postmat", postmat);
        print_m4("difmat", difmat);
        printf("Roll = %f\n", RAD2DEGF(-atan2(difmat[2][0], difmat[2][2])));
#endif

        curBone->roll = -atan2f(difmat[2][0], difmat[2][2]);

        /* And set rest-position again. */
        BKE_armature_where_is_bone(curBone, curBone->parent, false);
        break;
      }
    }

    /* Recurse into children... */
    armature_finalize_restpose(&curBone->childbase, editbonelist);
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_naming.c */
static bool editbone_unique_check(void *arg, const char *name)
{
  struct {
    ListBase *lb;
    void *bone;
  } *data = arg;
  EditBone *dupli = ED_armature_ebone_find_name(data->lb, name);
  return dupli && dupli != data->bone;
}

void ED_armature_ebone_unique_name(ListBase *ebones, char *name, EditBone *bone)
{
  struct {
    ListBase *lb;
    void *bone;
  } data;
  data.lb = ebones;
  data.bone = bone;

  BLI_uniquename_cb(editbone_unique_check, &data, DATA_("Bone"), '.', name, sizeof(bone->name));
}

/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_add.c */
EditBone *ED_armature_ebone_add(bArmature *arm, const char *name)
{
  EditBone *bone = MEM_callocN(sizeof(EditBone), "eBone");

  STRNCPY(bone->name, name);
  ED_armature_ebone_unique_name(arm->edbo, bone->name, NULL);

  BLI_addtail(arm->edbo, bone);

  bone->flag |= BONE_TIPSEL;
  bone->weight = 1.0f;
  bone->dist = 0.25f;
  bone->xwidth = 0.1f;
  bone->zwidth = 0.1f;
  bone->rad_head = 0.10f;
  bone->rad_tail = 0.05f;
  bone->segments = 1;
  bone->layer = arm->layer;

  /* Bendy-Bone parameters */
  bone->roll1 = 0.0f;
  bone->roll2 = 0.0f;
  bone->curve_in_x = 0.0f;
  bone->curve_in_z = 0.0f;
  bone->curve_out_x = 0.0f;
  bone->curve_out_z = 0.0f;
  bone->ease1 = 1.0f;
  bone->ease2 = 1.0f;

  copy_v3_fl(bone->scale_in, 1.0f);
  copy_v3_fl(bone->scale_out, 1.0f);

  return bone;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_relations.c */
/* armature parenting options */
#define ARM_PAR_CONNECT 1
#define ARM_PAR_OFFSET 2

/* armature un-parenting options */
#define ARM_PAR_CLEAR 1
#define ARM_PAR_CLEAR_DISCONNECT 2

/* check for null, before calling! */
static void bone_connect_to_existing_parent(EditBone *bone)
{
  bone->flag |= BONE_CONNECTED;
  copy_v3_v3(bone->head, bone->parent->tail);
  bone->rad_head = bone->parent->rad_tail;
}

static void bone_connect_to_new_parent(ListBase *edbo,
                                       EditBone *selbone,
                                       EditBone *actbone,
                                       short mode)
{
  EditBone *ebone;
  float offset[3];

  if ((selbone->parent) && (selbone->flag & BONE_CONNECTED)) {
    selbone->parent->flag &= ~BONE_TIPSEL;
  }

  /* make actbone the parent of selbone */
  selbone->parent = actbone;

  /* in actbone tree we cannot have a loop */
  for (ebone = actbone->parent; ebone; ebone = ebone->parent) {
    if (ebone->parent == selbone) {
      ebone->parent = NULL;
      ebone->flag &= ~BONE_CONNECTED;
    }
  }

  if (mode == ARM_PAR_CONNECT) {
    /* Connected: Child bones will be moved to the parent tip */
    selbone->flag |= BONE_CONNECTED;
    sub_v3_v3v3(offset, actbone->tail, selbone->head);

    copy_v3_v3(selbone->head, actbone->tail);
    selbone->rad_head = actbone->rad_tail;

    add_v3_v3(selbone->tail, offset);

    /* offset for all its children */
    for (ebone = edbo->first; ebone; ebone = ebone->next) {
      EditBone *par;

      for (par = ebone->parent; par; par = par->parent) {
        if (par == selbone) {
          add_v3_v3(ebone->head, offset);
          add_v3_v3(ebone->tail, offset);
          break;
        }
      }
    }
  }
  else {
    /* Offset: Child bones will retain their distance from the parent tip */
    selbone->flag &= ~BONE_CONNECTED;
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/armature_relations.c */
static void editbone_clear_parent(EditBone *ebone, int mode)
{
  if (ebone->parent) {
    /* for nice selection */
    ebone->parent->flag &= ~BONE_TIPSEL;
  }

  if (mode == 1) {
    ebone->parent = NULL;
  }
  ebone->flag &= ~BONE_CONNECTED;
}

/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/pose_transform.c */
static void pchan_clear_scale(bPoseChannel *pchan)
{
  if ((pchan->protectflag & OB_LOCK_SCALEX) == 0) {
    pchan->size[0] = 1.0f;
  }
  if ((pchan->protectflag & OB_LOCK_SCALEY) == 0) {
    pchan->size[1] = 1.0f;
  }
  if ((pchan->protectflag & OB_LOCK_SCALEZ) == 0) {
    pchan->size[2] = 1.0f;
  }

  pchan->ease1 = 0.0f;
  pchan->ease2 = 0.0f;

  copy_v3_fl(pchan->scale_in, 1.0f);
  copy_v3_fl(pchan->scale_out, 1.0f);
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/pose_transform.c */
static void pchan_clear_loc(bPoseChannel *pchan)
{
  if ((pchan->protectflag & OB_LOCK_LOCX) == 0) {
    pchan->loc[0] = 0.0f;
  }
  if ((pchan->protectflag & OB_LOCK_LOCY) == 0) {
    pchan->loc[1] = 0.0f;
  }
  if ((pchan->protectflag & OB_LOCK_LOCZ) == 0) {
    pchan->loc[2] = 0.0f;
  }
}
/* Clear the Location. When X-mirror is enabled,
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/armature/pose_transform.c */
static void pchan_clear_rot(bPoseChannel *pchan)
{
  if (pchan->protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) {
    /* check if convert to eulers for locking... */
    if (pchan->protectflag & OB_LOCK_ROT4D) {
      /* perform clamping on a component by component basis */
      if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        if ((pchan->protectflag & OB_LOCK_ROTW) == 0) {
          pchan->rotAngle = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTX) == 0) {
          pchan->rotAxis[0] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTY) == 0) {
          pchan->rotAxis[1] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTZ) == 0) {
          pchan->rotAxis[2] = 0.0f;
        }

        /* check validity of axis - axis should never be 0,0,0
         * (if so, then we make it rotate about y). */
        if (IS_EQF(pchan->rotAxis[0], pchan->rotAxis[1]) &&
            IS_EQF(pchan->rotAxis[1], pchan->rotAxis[2])) {
          pchan->rotAxis[1] = 1.0f;
        }
      }
      else if (pchan->rotmode == ROT_MODE_QUAT) {
        if ((pchan->protectflag & OB_LOCK_ROTW) == 0) {
          pchan->quat[0] = 1.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTX) == 0) {
          pchan->quat[1] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTY) == 0) {
          pchan->quat[2] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTZ) == 0) {
          pchan->quat[3] = 0.0f;
        }
      }
      else {
        /* the flag may have been set for the other modes, so just ignore the extra flag... */
        if ((pchan->protectflag & OB_LOCK_ROTX) == 0) {
          pchan->eul[0] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTY) == 0) {
          pchan->eul[1] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTZ) == 0) {
          pchan->eul[2] = 0.0f;
        }
      }
    }
    else {
      /* perform clamping using euler form (3-components) */
      float eul[3], oldeul[3], quat1[4] = {0};
      float qlen = 0.0f;

      if (pchan->rotmode == ROT_MODE_QUAT) {
        qlen = normalize_qt_qt(quat1, pchan->quat);
        quat_to_eul(oldeul, quat1);
      }
      else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        axis_angle_to_eulO(oldeul, EULER_ORDER_DEFAULT, pchan->rotAxis, pchan->rotAngle);
      }
      else {
        copy_v3_v3(oldeul, pchan->eul);
      }

      eul[0] = eul[1] = eul[2] = 0.0f;

      if (pchan->protectflag & OB_LOCK_ROTX) {
        eul[0] = oldeul[0];
      }
      if (pchan->protectflag & OB_LOCK_ROTY) {
        eul[1] = oldeul[1];
      }
      if (pchan->protectflag & OB_LOCK_ROTZ) {
        eul[2] = oldeul[2];
      }

      if (pchan->rotmode == ROT_MODE_QUAT) {
        eul_to_quat(pchan->quat, eul);

        /* restore original quat size */
        mul_qt_fl(pchan->quat, qlen);

        /* quaternions flip w sign to accumulate rotations correctly */
        if ((quat1[0] < 0.0f && pchan->quat[0] > 0.0f) ||
            (quat1[0] > 0.0f && pchan->quat[0] < 0.0f)) {
          mul_qt_fl(pchan->quat, -1.0f);
        }
      }
      else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, eul, EULER_ORDER_DEFAULT);
      }
      else {
        copy_v3_v3(pchan->eul, eul);
      }
    }
  } /* Duplicated in source/blender/editors/object/object_transform.cc */
  else {
    if (pchan->rotmode == ROT_MODE_QUAT) {
      unit_qt(pchan->quat);
    }
    else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
      /* by default, make rotation of 0 radians around y-axis (roll) */
      unit_axis_angle(pchan->rotAxis, &pchan->rotAngle);
    }
    else {
      zero_v3(pchan->eul);
    }
  }

  /* Clear also Bendy Bone stuff - Roll is obvious,
   * but Curve X/Y stuff is also kind of rotational in nature... */
  pchan->roll1 = 0.0f;
  pchan->roll2 = 0.0f;

  pchan->curve_in_x = 0.0f;
  pchan->curve_in_z = 0.0f;
  pchan->curve_out_x = 0.0f;
  pchan->curve_out_z = 0.0f;
}
/* Clear the rotation. When X-mirror is enabled,
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_armature.c */
static float get_weight(float dist, float decay_rad, float dif_rad)
{
  float weight = 1.0f;
  if (dist < decay_rad) {
    weight = 1.0f;
  }
  else {
    weight = interpf(0.0f, 0.9f, (dist - decay_rad) / dif_rad);
  }

  return weight;
}

/* END VERBATIM */
/* ---------------------------------------------------------------------------------------- */
/* ADAPTED code: each function names its Blender 3.6.23 original and what differs.           */

/* bone_free (armature_utils.c) without IDP_FreeProperty: Project Grease bones have no ID props. */
void bone_free(bArmature *arm, EditBone *bone)
{
  if (arm->act_edbone == bone) {
    arm->act_edbone = NULL;
  }
  LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
    if (ebone->bbone_next == bone) {
      ebone->bbone_next = NULL;
    }
    if (ebone->bbone_prev == bone) {
      ebone->bbone_prev = NULL;
    }
  }
  BLI_freelinkN(arm->edbo, bone);
}

/* BKE_armature_bonelist_free without ID properties. */
void pg_bke_armature_bonelist_free(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    pg_bke_armature_bonelist_free(&bone->childbase);
  }
  BLI_freelistN(lb);
}

/* BEGIN VERBATIM source/blender/blenkernel/intern/action.c */
void BKE_pose_channel_free_bbone_cache(bPoseChannel_Runtime *runtime)
{
  runtime->bbone_segments = 0;
  MEM_SAFE_FREE(runtime->bbone_rest_mats);
  MEM_SAFE_FREE(runtime->bbone_pose_mats);
  MEM_SAFE_FREE(runtime->bbone_deform_mats);
  MEM_SAFE_FREE(runtime->bbone_dual_quats);
}
/* END VERBATIM */

/* BKE_pose_eval_bbone_segments (armature_update.c:963-980) for every channel, without the
 * depsgraph: the segment cache (rest / pose / deform matrices, dual quaternions) follows the pose. */
void pg_bke_pose_bbone_segments(bPose *pose)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone != NULL && pchan->bone->segments > 1) {
      BKE_pchan_bbone_segments_cache_compute(pchan);
    }
    else {
      BKE_pose_channel_free_bbone_cache(&pchan->runtime);
    }
  }
}

void pg_bke_pose_free(bPose *pose)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    pg_con_list_free(&pchan->constraints);
    BKE_pose_channel_free_bbone_cache(&pchan->runtime);
  }
  BLI_freelistN(&pose->chanbase);
}

/* BKE_pose_channel_ensure (action.c) without the session uuid. */
bPoseChannel *pg_bke_pose_channel_ensure(bPose *pose, const char *name)
{
  bPoseChannel *chan = BKE_pose_channel_find_name(pose, name);
  if (chan) {
    return chan;
  }
  chan = MEM_callocN(sizeof(bPoseChannel), "verifyPoseChannel");
  STRNCPY(chan->name, name);
  copy_v3_fl(chan->custom_scale_xyz, 1.0f);
  unit_qt(chan->quat);
  unit_axis_angle(chan->rotAxis, &chan->rotAngle);
  chan->size[0] = chan->size[1] = chan->size[2] = 1.0f;
  copy_v3_fl(chan->scale_in, 1.0f);
  copy_v3_fl(chan->scale_out, 1.0f);
  chan->limitmin[0] = chan->limitmin[1] = chan->limitmin[2] = -M_PI;
  chan->limitmax[0] = chan->limitmax[1] = chan->limitmax[2] = M_PI;
  unit_m4(chan->constinv);
  chan->protectflag = OB_LOCK_ROT4D; /* lock by components by default */
  BLI_addtail(&pose->chanbase, chan);
  return chan;
}

/* rebuild_pose_bone + BKE_pose_rebuild (armature.c): channels follow the bone hierarchy
 * depth-first; channels whose bone is gone are removed (BKE_pose_channels_clear_with_null_bone). */
static int rebuild_pose_bone(bPose *pose, Bone *bone, bPoseChannel *parchan, int counter,
                             Bone **r_last_visited_bone_p)
{
  bPoseChannel *pchan = pg_bke_pose_channel_ensure(pose, bone->name);
  pchan->bone = bone;
  pchan->parent = parchan;
  bPoseChannel *pchan_prev = pchan->prev;
  const Bone *last_visited_bone = *r_last_visited_bone_p;
  if ((pchan_prev == NULL && last_visited_bone != NULL) ||
      (pchan_prev != NULL && pchan_prev->bone != last_visited_bone))
  {
    pchan_prev = last_visited_bone != NULL ?
                     BKE_pose_channel_find_name(pose, last_visited_bone->name) :
                     NULL;
    BLI_remlink(&pose->chanbase, pchan);
    BLI_insertlinkafter(&pose->chanbase, pchan_prev, pchan);
  }
  *r_last_visited_bone_p = pchan->bone;
  counter++;
  for (bone = bone->childbase.first; bone; bone = bone->next) {
    counter = rebuild_pose_bone(pose, bone, pchan, counter, r_last_visited_bone_p);
    if (bone->flag & BONE_CONNECTED) {
      pchan->child = BKE_pose_channel_find_name(pose, bone->name);
    }
  }
  return counter;
}

/* The "Remove zero sized bones" step of ED_armature_from_edit (armature_utils.c). Blender runs it
 * only when the edit bones are flushed (leaving Edit mode), so it is not in pg_bke_armature_from_edit,
 * which runs on every update: a bone dragged to zero length must survive the drag. */
void pg_bke_armature_remove_zero_bones(bArmature *arm)
{
  EditBone *eBone, *neBone;

  /* Remove zero sized bones, this gives unstable rest-poses. */
  for (eBone = arm->edbo->first; eBone; eBone = neBone) {
    float len_sq = len_squared_v3v3(eBone->head, eBone->tail);
    neBone = eBone->next;
    if (len_sq <= square_f(0.000001f)) {
      EditBone *fBone;
      for (fBone = arm->edbo->first; fBone; fBone = fBone->next) {
        if (fBone->parent == eBone) {
          fBone->parent = eBone->parent;
        }
      }
      bone_free(arm, eBone);
    }
  }

}

/* ED_armature_from_edit (armature_utils.c) without Main / ID properties / B-Bone handles, then
 * BKE_pose_rebuild. The edit bones stay (arm->edbo is Project Grease's editable rest pose). */
void pg_bke_armature_from_edit(bArmature *arm, bPose *pose)
{
  EditBone *eBone, *neBone;
  Bone *newBone;

  pg_bke_armature_bonelist_free(&arm->bonebase);
  arm->act_bone = NULL;

  for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
    newBone = MEM_callocN(sizeof(Bone), "bone");
    eBone->temp.bone = newBone;
    STRNCPY(newBone->name, eBone->name);
    copy_v3_v3(newBone->arm_head, eBone->head);
    copy_v3_v3(newBone->arm_tail, eBone->tail);
    newBone->arm_roll = eBone->roll;
    newBone->flag = eBone->flag;
    newBone->inherit_scale_mode = eBone->inherit_scale_mode;
    if (eBone == arm->act_edbone) {
      arm->act_bone = newBone;
    }
    newBone->roll = 0.0f;
    newBone->weight = eBone->weight;
    newBone->dist = eBone->dist;
    newBone->xwidth = eBone->xwidth;
    newBone->zwidth = eBone->zwidth;
    newBone->rad_head = eBone->rad_head;
    newBone->rad_tail = eBone->rad_tail;
    newBone->segments = eBone->segments;
    newBone->layer = eBone->layer;
    newBone->roll1 = eBone->roll1;
    newBone->roll2 = eBone->roll2;
    newBone->curve_in_x = eBone->curve_in_x;
    newBone->curve_in_z = eBone->curve_in_z;
    newBone->curve_out_x = eBone->curve_out_x;
    newBone->curve_out_z = eBone->curve_out_z;
    newBone->ease1 = eBone->ease1;
    newBone->ease2 = eBone->ease2;
    copy_v3_v3(newBone->scale_in, eBone->scale_in);
    copy_v3_v3(newBone->scale_out, eBone->scale_out);
  }
  for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
    newBone = eBone->temp.bone;
    if (eBone->parent) {
      newBone->parent = eBone->parent->temp.bone;
      BLI_addtail(&newBone->parent->childbase, newBone);
    }
    else {
      BLI_addtail(&arm->bonebase, newBone);
    }
  }
  armature_finalize_restpose(&arm->bonebase, arm->edbo);

  /* BKE_pose_rebuild */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    pchan->bone = NULL;
    pchan->child = NULL;
  }
  Bone *prev = NULL;
  int counter = 0;
  for (Bone *bone = arm->bonebase.first; bone; bone = bone->next) {
    counter = rebuild_pose_bone(pose, bone, NULL, counter, &prev);
  }
  for (bPoseChannel *pchan = pose->chanbase.first, *next; pchan; pchan = next) {
    next = pchan->next;
    if (pchan->bone == NULL) {
      pg_con_list_free(&pchan->constraints);
      BKE_pose_channel_free_bbone_cache(&pchan->runtime);
      BLI_freelinkN(&pose->chanbase, pchan);
    }
  }
}

/* BKE_pose_where_is is in project_grease_constraint.c (with constraints and IK). */

/* armature_deform_coords_impl (armature_deform.c), the gps_target branch, single-threaded. The
 * armature object is at the origin; the GP object's matrix is target_to_world. */
void pg_bke_armature_deform_gpencil_stroke(const bArmature *arm, const bPose *pose,
                                           const ListBase *defbase,
                                           const float target_to_world[4][4],
                                           float (*vert_coords)[3], int vert_coords_len,
                                           int deformflag, const char *defgrp_name,
                                           bGPDstroke *gps_target)
{
  bPoseChannel **pchan_from_defbase = NULL;
  const MDeformVert *dverts = gps_target->dvert;
  const int dverts_len = dverts ? gps_target->totpoints : 0;
  const bool use_envelope = (deformflag & ARM_DEF_ENVELOPE) != 0;
  const bool use_quaternion = (deformflag & ARM_DEF_QUATERNION) != 0;
  const bool invert_vgroup = (deformflag & ARM_DEF_INVERT_VGROUP) != 0;
  bool use_dverts = false;
  int armature_def_nr = -1;
  int defbase_len;

  if (pose == NULL) {
    return;
  }
  /* BKE_id_defgroup_name_index */
  armature_def_nr = defgrp_name && defgrp_name[0] ?
                        BLI_findstringindex(defbase, defgrp_name, offsetof(bDeformGroup, name)) :
                        -1;
  defbase_len = BLI_listbase_count(defbase);

  if (deformflag & ARM_DEF_VGROUP) {
    if (dverts) {
      use_dverts = true;
    }
    if (use_dverts) {
      pchan_from_defbase = MEM_callocN(sizeof(*pchan_from_defbase) * (defbase_len + 1),
                                       "defnrToBone");
      int i;
      LISTBASE_FOREACH_INDEX (bDeformGroup *, dg, defbase, i) {
        pchan_from_defbase[i] = BKE_pose_channel_find_name(pose, dg->name);
        if (pchan_from_defbase[i]) {
          if (pchan_from_defbase[i]->bone->flag & BONE_NO_DEFORM) {
            pchan_from_defbase[i] = NULL;
          }
        }
      }
    }
  }

  /* armature_vert_task_with_dvert reads data->ob_arm->pose */
  Object ob_arm;
  memset(&ob_arm, 0, sizeof(ob_arm));
  ob_arm.pose = (bPose *)pose;
  ob_arm.data = (void *)arm;

  ArmatureUserdata data = {
      .ob_arm = &ob_arm,
      .me_target = NULL,
      .vert_coords = vert_coords,
      .vert_deform_mats = NULL,
      .vert_coords_prev = NULL,
      .use_envelope = use_envelope,
      .use_quaternion = use_quaternion,
      .invert_vgroup = invert_vgroup,
      .use_dverts = use_dverts,
      .armature_def_nr = armature_def_nr,
      .dverts = dverts,
      .dverts_len = dverts_len,
      .pchan_from_defbase = pchan_from_defbase,
      .defbase_len = defbase_len,
  };
  /* the depsgraph runs BKE_pose_eval_bbone_segments before the modifier reads the cache */
  pg_bke_pose_bbone_segments((bPose *)pose);
  float obinv[4][4];
  invert_m4_m4(obinv, target_to_world);
  copy_m4_m4(data.postmat, obinv); /* obinv * armature object_to_world (identity) */
  invert_m4_m4(data.premat, data.postmat);

  for (int i = 0; i < vert_coords_len; i++) {
    /* armature_vert_task */
    const MDeformVert *dvert = NULL;
    if ((data.use_dverts || data.armature_def_nr != -1) && data.dverts && i < data.dverts_len) {
      dvert = data.dverts + i;
    }
    armature_vert_task_with_dvert(&data, i, dvert);
  }
  MEM_SAFE_FREE(pchan_from_defbase);
}

/* gpencil_armature.c: gpencil_object_vgroup_calc_from_armature + gpencil_add_verts_to_dgroups for
 * the frames of one layer. Blender also weights a B-Bone as its straight head-tail segment. */
static bDeformGroup *gp_defgroup_find(bGPdata *gpd, const char *name)
{
  return BLI_findstring(&gpd->vertex_group_names, name, offsetof(bDeformGroup, name));
}

static bDeformGroup *gp_defgroup_add_name(bGPdata *gpd, const char *name)
{
  /* BKE_object_defgroup_add_name: new group, unique name, made active */
  bDeformGroup *dg = MEM_callocN(sizeof(bDeformGroup), "add deformGroup");
  STRNCPY(dg->name, name);
  BLI_addtail(&gpd->vertex_group_names, dg);
  BLI_uniquename(&gpd->vertex_group_names, dg, DATA_("Group"), '.', offsetof(bDeformGroup, name),
                 sizeof(dg->name));
  gpd->vertex_group_active_index = BLI_listbase_count(&gpd->vertex_group_names);
  return dg;
}

static void gp_collect_bones(const ListBase *lb, const Bone **list, int *n)
{
  /* gpencil_bone_looper order: the bone, its children, then its next sibling */
  LISTBASE_FOREACH (const Bone *, bone, lb) {
    if (!(bone->flag & BONE_HIDDEN_P) && !(bone->flag & BONE_NO_DEFORM)) {
      if (list) {
        list[*n] = bone;
      }
      (*n)++;
    }
    gp_collect_bones(&bone->childbase, list, n);
  }
}

static int gp_add_empty_groups(bGPdata *gpd, const ListBase *lb)
{
  /* vgroup_add_unique_bone_cb */
  int count = 0;
  LISTBASE_FOREACH (const Bone *, bone, lb) {
    if (!(bone->flag & BONE_NO_DEFORM) && !gp_defgroup_find(gpd, bone->name)) {
      gp_defgroup_add_name(gpd, bone->name);
      count++;
    }
    count += gp_add_empty_groups(gpd, &bone->childbase);
  }
  return count;
}

int pg_bke_gpencil_vgroup_calc_from_armature(bGPdata *gpd, bGPDlayer *gpl, bGPDframe *actframe,
                                             bool is_multiedit, const bArmature *arm,
                                             const bPose *pose, const float target_to_world[4][4],
                                             int mode, float ratio, float decay)
{
  UNUSED_VARS(pose);
  const int defbase_add = gp_add_empty_groups(gpd, &arm->bonebase);
  if (mode != 1 /* GP_ARMATURE_AUTO */ || (is_multiedit ? gpl == NULL : actframe == NULL)) {
    return defbase_add;
  }

  int numbones = 0;
  gp_collect_bones(&arm->bonebase, NULL, &numbones);
  if (numbones == 0) {
    return defbase_add;
  }
  const Bone **bonelist = MEM_callocN(numbones * sizeof(Bone *), "bonelist");
  numbones = 0;
  gp_collect_bones(&arm->bonebase, bonelist, &numbones);

  /* dgroup_skinnable_cb */
  bDeformGroup **dgrouplist = MEM_callocN(numbones * sizeof(bDeformGroup *), "dgrouplist");
  float(*root)[3] = MEM_callocN(sizeof(float[3]) * numbones, "root");
  float(*tip)[3] = MEM_callocN(sizeof(float[3]) * numbones, "tip");
  float *radsqr = MEM_callocN(sizeof(float) * numbones, "radsqr");
  for (int j = 0; j < numbones; j++) {
    const Bone *bone = bonelist[j];
    bDeformGroup *defgroup = NULL;
    if (arm->layer & bone->layer) {
      if (!(defgroup = gp_defgroup_find(gpd, bone->name))) {
        defgroup = gp_defgroup_add_name(gpd, bone->name);
      }
      else if (defgroup->flag & DG_LOCK_WEIGHT) {
        defgroup = NULL;
      }
    }
    dgrouplist[j] = defgroup;
    copy_v3_v3(root[j], bone->arm_head);
    copy_v3_v3(tip[j], bone->arm_tail);
    radsqr[j] = len_squared_v3v3(root[j], tip[j]) * ratio;
  }

  /* gpencil_add_verts_to_dgroups "loop all strokes" (one layer; actframe is the frame shown, not
   * gpl->actframe, which the Project Grease backend does not keep) */
  bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : actframe;
  for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
  if ((gpf == actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    BKE_gpencil_dvert_ensure(gps);
    float(*verts)[3] = MEM_callocN(gps->totpoints * sizeof(*verts), __func__);
    bGPDspoint *pt;
    int i;
    for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
      copy_v3_v3(verts[i], &pt->x);
      mul_m4_v3(target_to_world, verts[i]);
    }
    for (int j = 0; j < numbones; j++) {
      int def_nr = BLI_findindex(&gpd->vertex_group_names, dgrouplist[j]);
      if (def_nr < 0) {
        continue;
      }
      float decay_rad = radsqr[j] - (radsqr[j] * decay);
      float dif_rad = radsqr[j] - decay_rad;
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        MDeformVert *dvert = &gps->dvert[i];
        float weight;
        float dist = dist_squared_to_line_segment_v3(verts[i], root[j], tip[j]);
        if (dist > radsqr[j]) {
          weight = 0.0f;
          dist = len_squared_v3v3(root[j], verts[i]);
          if (dist < radsqr[j]) {
            weight = get_weight(dist, decay_rad, dif_rad);
          }
          else {
            dist = len_squared_v3v3(tip[j], verts[i]);
            if (dist < radsqr[j]) {
              weight = get_weight(dist, decay_rad, dif_rad);
            }
          }
        }
        else {
          weight = get_weight(dist, decay_rad, dif_rad);
        }
        MDeformWeight *dw = BKE_defvert_ensure_index(dvert, def_nr);
        if (dw) {
          dw->weight = weight;
        }
      }
    }
    MEM_SAFE_FREE(verts);
  }
  }
  /* If not multi-edit, exit loop. */
  if (!is_multiedit) {
    break;
  }
  }
  MEM_freeN((void *)bonelist);
  MEM_freeN(dgrouplist);
  MEM_freeN(root);
  MEM_freeN(tip);
  MEM_freeN(radsqr);
  return defbase_add;
}

/* pose_bone_do_paste (pose_transform.c) without ID properties (bPose instead of Object). */
bPoseChannel *pg_bke_pose_bone_do_paste(bPose *pose, bPoseChannel *chan, bool selOnly, bool flip)
{
  char name[MAXBONENAME];
  if (flip) {
    BLI_string_flip_side_name(name, chan->name, false, sizeof(name));
  }
  else {
    STRNCPY(name, chan->name);
  }
  bPoseChannel *pchan = BKE_pose_channel_find_name(pose, name);
  if (pchan == NULL) {
    return NULL;
  }
  if (selOnly && (pchan->bone->flag & BONE_SELECTED) == 0) {
    return NULL;
  }
  copy_v3_v3(pchan->loc, chan->loc);
  copy_v3_v3(pchan->size, chan->size);
  pchan->flag = chan->flag;
  if (pchan->rotmode == chan->rotmode) {
    if (pchan->rotmode > 0) {
      copy_v3_v3(pchan->eul, chan->eul);
    }
    else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
      copy_v3_v3(pchan->rotAxis, chan->rotAxis);
      pchan->rotAngle = chan->rotAngle;
    }
    else {
      copy_qt_qt(pchan->quat, chan->quat);
    }
  }
  else if (pchan->rotmode > 0) {
    if (chan->rotmode == ROT_MODE_AXISANGLE) {
      axis_angle_to_eulO(pchan->eul, pchan->rotmode, chan->rotAxis, chan->rotAngle);
    }
    else {
      quat_to_eulO(pchan->eul, pchan->rotmode, chan->quat);
    }
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    if (chan->rotmode > 0) {
      eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, chan->eul, chan->rotmode);
    }
    else {
      quat_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, chan->quat);
    }
  }
  else {
    if (chan->rotmode > 0) {
      eulO_to_quat(pchan->quat, chan->eul, chan->rotmode);
    }
    else {
      axis_angle_to_quat(pchan->quat, chan->rotAxis, pchan->rotAngle);
    }
  }
  pchan->curve_in_x = chan->curve_in_x;
  pchan->curve_in_z = chan->curve_in_z;
  pchan->curve_out_x = chan->curve_out_x;
  pchan->curve_out_z = chan->curve_out_z;
  pchan->roll1 = chan->roll1;
  pchan->roll2 = chan->roll2;
  pchan->ease1 = chan->ease1;
  pchan->ease2 = chan->ease2;
  copy_v3_v3(pchan->scale_in, chan->scale_in);
  copy_v3_v3(pchan->scale_out, chan->scale_out);
  if (flip) {
    pchan->loc[0] *= -1;
    pchan->curve_in_x *= -1;
    pchan->curve_out_x *= -1;
    pchan->roll1 *= -1;
    pchan->roll2 *= -1;
    if (pchan->rotmode > 0) {
      pchan->eul[1] *= -1;
      pchan->eul[2] *= -1;
    }
    else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
      float eul[3];
      axis_angle_to_eulO(eul, EULER_ORDER_DEFAULT, pchan->rotAxis, pchan->rotAngle);
      eul[1] *= -1;
      eul[2] *= -1;
      eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, eul, EULER_ORDER_DEFAULT);
    }
    else {
      float eul[3];
      normalize_qt(pchan->quat);
      quat_to_eul(eul, pchan->quat);
      eul[1] *= -1;
      eul[2] *= -1;
      eul_to_quat(pchan->quat, eul);
    }
  }
  return pchan;
}

/* Exported wrappers of the static verbatim functions above. */
void pg_bke_pchan_clear_loc(bPoseChannel *pchan) { pchan_clear_loc(pchan); }
void pg_bke_pchan_clear_rot(bPoseChannel *pchan) { pchan_clear_rot(pchan); }
void pg_bke_pchan_clear_scale(bPoseChannel *pchan) { pchan_clear_scale(pchan); }
float pg_bke_fcurve_eval_keyframes(FCurve *fcu, float evaltime)
{
  return fcu->totvert ? fcurve_eval_keyframes(fcu, fcu->bezt, evaltime) : 0.0f;
}
EditBone *pg_bke_ebone_add(bArmature *arm, const char *name) { return ED_armature_ebone_add(arm, name); }
void pg_bke_ebone_unique_name(ListBase *ebones, char *name, EditBone *bone)
{
  ED_armature_ebone_unique_name(ebones, name, bone);
}
void pg_bke_ebone_remove(bArmature *arm, EditBone *ebone) { ED_armature_ebone_remove(arm, ebone); }
void pg_bke_bone_connect_to_new_parent(ListBase *edbo, EditBone *selbone, EditBone *actbone, short mode)
{
  bone_connect_to_new_parent(edbo, selbone, actbone, mode);
}
void pg_bke_bone_connect_to_existing_parent(EditBone *bone) { bone_connect_to_existing_parent(bone); }
void pg_bke_editbone_clear_parent(EditBone *ebone, int mode) { editbone_clear_parent(ebone, mode); }
int pg_bke_insert_vert_fcurve(FCurve *fcu, float x, float y)
{
  return insert_vert_fcurve(fcu, x, y, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_NO_USERPREF);
}
void pg_bke_fcurve_handles_recalc(FCurve *fcu) { BKE_fcurve_handles_recalc(fcu); }
float pg_bke_distfactor_to_bone(const float vec[3], const float b1[3], const float b2[3], float rad1,
                                float rad2, float rdist)
{
  return distfactor_to_bone(vec, b1, b2, rad1, rad2, rdist);
}
void pg_bke_edit_sync_selection(ListBase *edbo) { ED_armature_edit_sync_selection(edbo); }
float pg_bke_ebone_roll_to_vector(const EditBone *bone, const float align_axis[3], bool axis_only)
{
  return ED_armature_ebone_roll_to_vector(bone, align_axis, axis_only);
}
void pg_bke_ebone_to_mat3(EditBone *ebone, float r_mat[3][3]) { ED_armature_ebone_to_mat3(ebone, r_mat); }
