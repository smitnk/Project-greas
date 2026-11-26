/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_transform.h. Ported from Blender 3.6.23 source/blender/editors/transform:
 *   transform_convert_gpencil_legacy.c  createTransGPencil, createTransGPencil_strokes / _curves,
 *                                       createTransGPencil_center_get, get_bezt_sel_triple_flag,
 *                                       recalcData_gpencil_strokes
 *   transform_convert.c                 set_prop_dist (with_dist = true, as for GPencil),
 *                                       transform_around_single_fallback_ex
 *   transform_generics.c                calculatePropRatio, calculateCenterMedian / Bound / Cursor
 *   transform_mode.c                    ElementRotation_ex, ElementResize (T_POINTS, GP falloff)
 *   transform_mode_translate.c          applyTranslationValue (td->factor * multi_frame_falloff)
 *   transform_mode_rotate.c             transdata_elem_rotate (angle * td->factor)
 *   transform_mode_mirror.c             ElementMirror (no proportional, no falloff)
 *   transform_snap.cc                   snap_increment_apply_ex; t->snap of rotate (5 deg) / resize (0.1)
 *   transform_constraints.c             constraintNumInput, axisProjection, isPlaneProjectionViewAligned,
 *                                       planeProjection, projection_matrix_calc, applyAxisConstraintVec
 *                                       / Size / Rot, constraints_rotation_impl, setUserConstraint,
 *                                       start / stopConstraint, getConstraintSpaceDimension
 *   transform_mode_translate/rotate/resize.c  applyTranslation / applyRotation / applyResize with
 *                                       their numeric input and T_INPUT_IS_VALUES_FINAL branches,
 *                                       large_rotation_limit; init*'s NumInput setup
 * The view is the app's: orthographic, looking down -Z (viewinv[2] = +Z, viewinv[1] = +Y).
 * Not ported: orientations other than Global / Local, precision
 * (Shift) snapping, auto-key frame copy (the app keys the frame before the gesture), layer matrices. */
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_colortools.h"
#include "BKE_curve.h"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "project_grease_numinput.h"
#include "project_grease_transform.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* TransData, reduced to what the GP converters and the four modes use. */
enum { TD_SELECTED = 1 << 0 };
typedef struct PGTransData {
  float *loc;
  float iloc[3];
  float center[3];
  float factor, dist, rdist;
  int flag;
  bGPDstroke *extra; /* td->extra: the stroke, for the falloff and the recalc */
  BezTriple *bezt;   /* hdata (initTransDataCurveHandles): restored on cancel */
  char ih1, ih2;
} PGTransData;

struct PGTransInfo {
  bGPdata *gpd;
  int mode;
  int around; /* t->around, after transform_around_single_fallback_ex */
  PGTransformOptions o;
  bool is_curve_edit;
  PGTransData *data;
  int data_len;
  float center[3]; /* tc->center_local (identity object matrix) */
  RNG *rng;
  /* t->con, t->spacemtx, t->num, t->idx_max and the last applied value (re-applied on a change) */
  int con_mode;
  float spacemtx[3][3], spacemtx_inv[3][3], con_pmtx[3][3];
  PGNumInput num;
  int idx_max;
  float values[3];
  int input;
};

enum { CON_APPLY = 1 << 0, CON_AXIS0 = 1 << 1, CON_AXIS1 = 1 << 2, CON_AXIS2 = 1 << 3 };
static const float VIEW_Z[3] = {0, 0, 1}, VIEW_Y[3] = {0, 1, 0}; /* viewinv[2], viewinv[1] of the top view */

/* ts->gp_sculpt.cur_falloff as scene.cc makes it: a 0..1 curve reset to CURVE_PRESET_GAUSS. */
static CurveMapping *pgx_frame_falloff_curve(void)
{
  static CurveMapping *cur_falloff = NULL;
  if (cur_falloff == NULL) {
    cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    BKE_curvemapping_init(cur_falloff);
    BKE_curvemap_reset(cur_falloff->cm, &cur_falloff->clipr, CURVE_PRESET_GAUSS, CURVEMAP_SLOPE_POSITIVE);
    BKE_curvemapping_init(cur_falloff);
  }
  return cur_falloff;
}

/* ED_gpencil_stroke_can_use_direct() for a VIEW_3D area. */
static bool pgx_stroke_can_use(const bGPDstroke *gps)
{
  if (gps->flag & GP_STROKE_3DSPACE) return true;
  if (gps->flag & GP_STROKE_2DIMAGE) return false;
  if (gps->flag & GP_STROKE_2DSPACE) return false;
  return true;
}

/* ED_gpencil_stroke_material_editable(), with BKE_gpencil_material_settings reading gpd->mat[]. */
static bool pgx_stroke_material_editable(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  const int index = gps->mat_nr;
  if (gpd->mat == NULL || index < 0 || index >= (int)gpd->totcol || gpd->mat[index] == NULL) return true;
  const MaterialGPencilStyle *gp_style = gpd->mat[index]->gp_style;
  if (gp_style != NULL) {
    if (gp_style->flag & GP_MATERIAL_HIDE) return false;
    if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (gp_style->flag & GP_MATERIAL_LOCKED)) return false;
  }
  return true;
}

static bool pgx_stroke_ok(const bGPdata *gpd, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  return pgx_stroke_can_use(gps) && pgx_stroke_material_editable(gpd, gpl, gps);
}

static void createTransGPencil_center_get(bGPDstroke *gps, float r_center[3])
{
  bGPDspoint *pt;
  int i;
  r_center[0] = r_center[1] = r_center[2] = 0.0f;
  int tot_sel = 0;
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      r_center[0] += pt->x;
      r_center[1] += pt->y;
      r_center[2] += pt->z;
      tot_sel++;
    }
  }
  if (tot_sel > 0) {
    for (int k = 0; k < 3; k++) r_center[k] *= 1.0f / tot_sel;
  }
}

#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)
#define SEL_ALL ((1 << 0) | (1 << 1) | (1 << 2))
static short get_bezt_sel_triple_flag(BezTriple *bezt, const bool handles_visible)
{
  short flag = 0;
  if (handles_visible) {
    flag = ((bezt->f1 & SELECT) ? SEL_F1 : 0) | ((bezt->f2 & SELECT) ? SEL_F2 : 0) |
           ((bezt->f3 & SELECT) ? SEL_F3 : 0);
  }
  else if (bezt->f2 & SELECT) {
    flag = SEL_ALL;
  }
  /* Special case for auto & aligned handles */
  if ((flag != SEL_ALL) && (flag & SEL_F2)) {
    if (ELEM(bezt->h1, HD_AUTO, HD_ALIGN) && ELEM(bezt->h2, HD_AUTO, HD_ALIGN)) {
      flag = SEL_ALL;
    }
  }
  return flag;
}

/* The frames a layer's transform visits: the active one, plus the selected ones in multi-frame. */
#define PGX_FRAMES_BEGIN(gpl, is_multiedit, gpf)                                                  \
  for (bGPDframe *gpf = (is_multiedit) ? (bGPDframe *)(gpl)->frames.first : (gpl)->actframe; gpf; \
       gpf = (is_multiedit) ? gpf->next : NULL)                                                   \
    if ((gpf == (gpl)->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit)))

static float pgx_frame_falloff(bGPDlayer *gpl, bGPDframe *gpf, bool is_multiedit, bool use_mf, int f_init, int f_end)
{
  if (is_multiedit && use_mf) {
    return BKE_gpencil_multiframe_falloff_calc(gpf, gpl->actframe->framenum, f_init, f_end,
                                               pgx_frame_falloff_curve());
  }
  return 1.0f;
}

/* createTransGPencil_strokes: pass 1 counts, pass 2 (td != NULL) fills. */
static int pgx_strokes(PGTransInfo *t, PGTransData *td, bool is_multiedit, bool use_mf, bool is_prop_edit,
                       bool is_prop_edit_connected)
{
  int n = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &t->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    int f_init = 0, f_end = 0;
    if (td && use_mf) BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
    PGX_FRAMES_BEGIN (gpl, is_multiedit, gpf) {
      const float falloff = td ? pgx_frame_falloff(gpl, gpf, is_multiedit, use_mf, f_init, f_end) : 1.0f;
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (!pgx_stroke_ok(t->gpd, gpl, gps)) continue;
        /* What we need to include depends on proportional editing settings... */
        const bool stroke_ok = is_prop_edit ? (is_prop_edit_connected ? (gps->flag & GP_STROKE_SELECT) != 0 : true) :
                                              (gps->flag & GP_STROKE_SELECT) != 0;
        if (!stroke_ok || gps->totpoints == 0) continue;
        float center[3];
        if (td) {
          gps->runtime.multi_frame_falloff = falloff;
          createTransGPencil_center_get(gps, center);
        }
        for (int i = 0; i < gps->totpoints; i++) {
          bGPDspoint *pt = &gps->points[i];
          if (!is_prop_edit && !(pt->flag & GP_SPOINT_SELECT)) continue;
          if (td) {
            PGTransData *d = &td[n];
            memcpy(d->iloc, &pt->x, sizeof(float[3]));
            /* Only copy center in local origins. */
            if ((gps->flag & GP_STROKE_SELECT) && (t->o.pivot == PG_TFM_PIVOT_INDIVIDUAL)) {
              memcpy(d->center, center, sizeof(float[3]));
            }
            else {
              memcpy(d->center, &pt->x, sizeof(float[3]));
            }
            d->loc = &pt->x;
            d->flag = (pt->flag & GP_SPOINT_SELECT) ? TD_SELECTED : 0;
            d->extra = gps;
          }
          n++;
        }
        /* calc_distanceCurveVerts is skipped: set_prop_dist(t, true) overwrites td->dist for GPencil. */
      }
    }
  }
  return n;
}

/* createTransGPencil_curves with v3d->overlay.handle_display = CURVE_HANDLE_SELECTED (the default). */
static int pgx_curves(PGTransInfo *t, PGTransData *td, bool is_multiedit, bool use_mf, bool is_prop_edit,
                      int *r_sel_points, int *r_curve_points, int *r_sel_curve_points)
{
  int n = 0, tot_sel_points = 0, tot_curve_points = 0, tot_sel_curve_points = 0;
  const bool use_around_origins_for_handles_test = (t->around == PG_TFM_PIVOT_INDIVIDUAL) &&
                                                   ELEM(t->mode, PG_TFM_ROTATION, PG_TFM_RESIZE);
  LISTBASE_FOREACH (bGPDlayer *, gpl, &t->gpd->layers) {
    if (!BKE_gpencil_layer_is_editable(gpl) || gpl->actframe == NULL) continue;
    int f_init = 0, f_end = 0;
    if (td && use_mf) BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
    PGX_FRAMES_BEGIN (gpl, is_multiedit, gpf) {
      const float falloff = td ? pgx_frame_falloff(gpl, gpf, is_multiedit, use_mf, f_init, f_end) : 1.0f;
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (!pgx_stroke_ok(t->gpd, gpl, gps) || gps->editcurve == NULL) continue;
        if (td) gps->runtime.multi_frame_falloff = falloff;
        bool need_handle_recalc = false;
        bGPDcurve *gpc = gps->editcurve;
        for (int i = 0; i < gpc->tot_curve_points; i++) {
          bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
          BezTriple *bezt = &gpc_pt->bezt;
          if (bezt->hide) continue;
          const bool handles_visible = (gpc_pt->flag & GP_CURVE_POINT_SELECT) != 0;
          const short sel_flag = get_bezt_sel_triple_flag(bezt, handles_visible);
          if (!td) {
            if (sel_flag & (SEL_F1 | SEL_F2 | SEL_F3)) {
              tot_sel_points += ((sel_flag & SEL_F1) != 0) + ((sel_flag & SEL_F2) != 0) + ((sel_flag & SEL_F3) != 0);
              tot_sel_curve_points++;
            }
            if (is_prop_edit) {
              n += 3;
              tot_curve_points++;
            }
            continue;
          }
          bool bezt_use = false;
          bool hdata = false;
          for (int j = 0; j < 3; j++) {
            const bool is_ctrl_point = (j == 1);
            const bool sel = sel_flag & (1 << j);
            if (is_prop_edit || sel) {
              PGTransData *d = &td[n];
              memcpy(d->iloc, bezt->vec[j], sizeof(float[3]));
              d->loc = bezt->vec[j];
              const bool rotate_around_ctrl = !handles_visible || (t->around == PG_TFM_PIVOT_INDIVIDUAL) ||
                                              (bezt->f2 & SELECT);
              memcpy(d->center, bezt->vec[rotate_around_ctrl ? 1 : j], sizeof(float[3]));
              if (!handles_visible || is_ctrl_point) {
                d->flag = (bezt->f2 & SELECT) ? TD_SELECTED : 0;
              }
              else {
                d->flag = sel ? TD_SELECTED : 0;
              }
              if (!hdata && (!is_ctrl_point || ((sel_flag & SEL_F1 & SEL_F3) == 0))) {
                d->bezt = bezt;
                d->ih1 = bezt->h1;
                d->ih2 = bezt->h2;
                hdata = true;
              }
              d->extra = gps;
              n++;
            }
            bezt_use |= sel;
          }
          /* Update the handle types so transformation is possible */
          if (bezt_use) {
            BKE_nurb_bezt_handle_test(bezt, SELECT, handles_visible, use_around_origins_for_handles_test);
            need_handle_recalc = true;
          }
        }
        if (need_handle_recalc) BKE_gpencil_editcurve_recalculate_handles(gps);
      }
    }
  }
  if (!td) {
    if (!is_prop_edit) n = tot_sel_points;
    *r_sel_points = tot_sel_points;
    *r_curve_points = tot_curve_points;
    *r_sel_curve_points = tot_sel_curve_points;
  }
  return n;
}

static float pgx_len_v3v3(const float a[3], const float b[3])
{
  const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
  return sqrtf(x * x + y * y + z * z);
}

/* set_prop_dist(t, with_dist = true). use_island: transdata_check_local_islands (LOCAL_ORIGINS on
 * OB_GPENCIL_LEGACY). shortcut: a brute-force nearest search instead of the KD-tree (same nearest
 * distance), O(unselected * selected) once per gesture; use BLI_kdtree_3d if drawings get huge. */
static void set_prop_dist(PGTransInfo *t)
{
  const bool use_island = (t->around == PG_TFM_PIVOT_INDIVIDUAL);
  for (int a = 0; a < t->data_len; a++) {
    PGTransData *td = &t->data[a];
    if (td->flag & TD_SELECTED) {
      td->rdist = 0.0f;
      continue;
    }
    const float *vec = use_island ? td->iloc : td->center;
    int best = -1;
    float best_d = FLT_MAX;
    for (int b = 0; b < t->data_len; b++) {
      const PGTransData *s = &t->data[b];
      if (!(s->flag & TD_SELECTED)) continue;
      const float d = pgx_len_v3v3(vec, use_island ? s->iloc : s->center);
      if (d < best_d) { best_d = d; best = b; }
    }
    td->rdist = -1.0f;
    if (best != -1) {
      td->rdist = best_d;
      if (use_island) memcpy(td->center, t->data[best].center, sizeof(float[3]));
    }
    td->dist = td->rdist;
  }
}

float pg_transform_prop_factor(int prop_mode, float rdist, float prop_size)
{
  float dist = (prop_size - rdist) / prop_size;
  /* Clamp to positive numbers. */
  if (dist < 0.0f) dist = 0.0f;
  switch (prop_mode) {
    case PROP_SHARP: return dist * dist;
    case PROP_SMOOTH: return 3.0f * dist * dist - 2.0f * dist * dist * dist;
    case PROP_ROOT: return sqrtf(dist);
    case PROP_LIN: return dist;
    case PROP_CONST: return 1.0f;
    case PROP_SPHERE: return sqrtf(2 * dist - dist * dist);
    case PROP_INVSQUARE: return dist * (2.0f - dist);
    default: return 1;
  }
}

/* calculatePropRatio. Connected and not connected read the same distance here: td->dist == td->rdist
 * after set_prop_dist(with_dist), and the GP converters never set TD_NOTCONNECTED. */
static void calculatePropRatio(PGTransInfo *t)
{
  for (int i = 0; i < t->data_len; i++) {
    PGTransData *td = &t->data[i];
    if (!t->o.prop_edit || (td->flag & TD_SELECTED)) {
      td->factor = 1.0f;
    }
    else if ((t->o.prop_connected ? td->dist : td->rdist) > t->o.prop_size) {
      td->factor = 0.0f;
    }
    else if (t->o.prop_mode == PROP_RANDOM) {
      if (t->rng == NULL) {
        /* Lazy initialization. */
        t->rng = BLI_rng_new((unsigned int)time(NULL));
      }
      float dist = (t->o.prop_size - (t->o.prop_connected ? td->dist : td->rdist)) / t->o.prop_size;
      if (dist < 0.0f) dist = 0.0f;
      td->factor = BLI_rng_get_float(t->rng) * dist;
    }
    else {
      td->factor = pg_transform_prop_factor(t->o.prop_mode, t->o.prop_connected ? td->dist : td->rdist,
                                            t->o.prop_size);
    }
  }
}

/* calculateCenter_FromAround: median / bounds of the selected td->center, or the cursor. */
static void calculateCenter(PGTransInfo *t)
{
  float partial[3] = {0, 0, 0}, min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  int total = 0;
  for (int i = 0; i < t->data_len; i++) {
    const PGTransData *td = &t->data[i];
    if (!(td->flag & TD_SELECTED)) continue;
    for (int k = 0; k < 3; k++) {
      partial[k] += td->center[k];
      if (td->center[k] < min[k]) min[k] = td->center[k];
      if (td->center[k] > max[k]) max[k] = td->center[k];
    }
    total++;
  }
  if (t->around == PG_TFM_PIVOT_CURSOR) {
    t->center[0] = t->o.cursor[0];
    t->center[1] = t->o.cursor[1];
    t->center[2] = 0.0f;
  }
  else if (t->around == PG_TFM_PIVOT_BOUNDS) {
    if (total) for (int k = 0; k < 3; k++) t->center[k] = 0.5f * (min[k] + max[k]);
  }
  else { /* median; Individual Origins uses the median for the helpline */
    if (total) for (int k = 0; k < 3; k++) t->center[k] = partial[k] / (float)total;
  }
}

PGTransInfo *pg_transform_begin(bGPdata *gpd, int mode, const PGTransformOptions *o)
{
  if (gpd == NULL || o == NULL || mode < PG_TFM_TRANSLATION || mode > PG_TFM_MIRROR) return NULL;
  if (o->pivot < PG_TFM_PIVOT_MEDIAN || o->pivot > PG_TFM_PIVOT_CURSOR) return NULL;
  if (!isfinite(o->cursor[0]) || !isfinite(o->cursor[1]) || !isfinite(o->snap_translate)) return NULL;
  if (o->prop_edit && !(isfinite(o->prop_size) && o->prop_size > 0.0f)) return NULL;
  PGTransInfo *t = MEM_callocN(sizeof(PGTransInfo), __func__);
  t->gpd = gpd;
  t->mode = mode;
  t->o = *o;
  /* TRANSFORM_OT_mirror has no proportional editing (P_PROPORTIONAL is not in its flags). */
  if (mode == PG_TFM_MIRROR) t->o.prop_edit = t->o.prop_connected = 0;
  t->around = t->o.pivot;
  /* GPENCIL_MULTIEDIT_SESSIONS_ON without the mode bits: the app keeps multi-frame editing on the flag alone */
  const bool is_multiedit = (gpd->flag & GP_DATA_STROKE_MULTIEDIT) != 0;
  const bool use_mf = t->o.multiframe_falloff != 0;
  const bool is_prop_edit = t->o.prop_edit != 0, is_prop_edit_connected = is_prop_edit && t->o.prop_connected;
  t->is_curve_edit = GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd);

  if (t->is_curve_edit) {
    int sel_points, curve_points, sel_curve_points;
    t->data_len = pgx_curves(t, NULL, is_multiedit, use_mf, is_prop_edit, &sel_points, &curve_points, &sel_curve_points);
    if (((is_prop_edit && !is_prop_edit_connected) ? curve_points : sel_points) == 0) t->data_len = 0;
    /* transform_around_single_fallback_ex */
    const int data_len_pt = is_prop_edit ? curve_points : sel_curve_points;
    if (data_len_pt == 1 && ELEM(t->around, PG_TFM_PIVOT_BOUNDS, PG_TFM_PIVOT_MEDIAN) &&
        ELEM(mode, PG_TFM_ROTATION, PG_TFM_RESIZE)) {
      t->around = PG_TFM_PIVOT_INDIVIDUAL;
    }
  }
  else {
    t->data_len = pgx_strokes(t, NULL, is_multiedit, use_mf, is_prop_edit, is_prop_edit_connected);
  }
  if (t->data_len > 0) {
    t->data = MEM_callocN(sizeof(PGTransData) * (size_t)t->data_len, __func__);
    if (t->is_curve_edit) {
      int unused;
      pgx_curves(t, t->data, is_multiedit, use_mf, is_prop_edit, &unused, &unused, &unused);
    }
    else {
      pgx_strokes(t, t->data, is_multiedit, use_mf, is_prop_edit, is_prop_edit_connected);
    }
  }
  bool any_selected = false;
  for (int i = 0; i < t->data_len; i++) any_selected |= (t->data[i].flag & TD_SELECTED) != 0;
  /* Nothing selected (proportional editing alone adds unselected points): nothing to transform. */
  if (!any_selected) {
    pg_transform_end(t, 1);
    return NULL;
  }
  if (is_prop_edit) set_prop_dist(t);
  calculatePropRatio(t);
  calculateCenter(t);
  /* init{Translation,Rotation,Resize}: the NumInput set-up; spacemtx of Global (and Local, below) */
  unit_m3(t->spacemtx);
  unit_m3(t->spacemtx_inv);
  pg_numinput_init(&t->num);
  t->idx_max = mode == PG_TFM_TRANSLATION || mode == PG_TFM_RESIZE ? 2 : 0;
  t->num.idx_max = (short)t->idx_max;
  for (int i = 0; i < 3; i++) {
    t->num.unit_type[i] = mode == PG_TFM_TRANSLATION ? PG_UNIT_LENGTH : mode == PG_TFM_ROTATION ? PG_UNIT_ROTATION : PG_UNIT_NONE;
    if (mode == PG_TFM_RESIZE) t->num.val_flag[i] |= PG_NUM_NULL_ONE;
  }
  if (mode == PG_TFM_RESIZE) t->num.flag |= PG_NUM_AFFECT_ALL;
  t->values[0] = t->values[1] = t->values[2] = mode == PG_TFM_RESIZE ? 1.0f : 0.0f;
  return t;
}

/* ---- transform_constraints.c ---- */
static int getConstraintSpaceDimension(const PGTransInfo *t)
{
  return ((t->con_mode & CON_AXIS0) != 0) + ((t->con_mode & CON_AXIS1) != 0) + ((t->con_mode & CON_AXIS2) != 0);
}

static void projection_matrix_calc(const PGTransInfo *t, float r_pmtx[3][3])
{
  unit_m3(r_pmtx);
  for (int i = 0; i < 3; i++) if (!(t->con_mode & (CON_AXIS0 << i))) zero_v3(r_pmtx[i]);
  float mat[3][3];
  mul_m3_m3m3(mat, r_pmtx, t->spacemtx_inv);
  mul_m3_m3m3(r_pmtx, t->spacemtx, mat);
}

static void constraintNumInput(const PGTransInfo *t, float vec[3])
{
  const int mode = t->con_mode;
  if (!(mode & CON_APPLY)) return;
  const float nval = t->mode == PG_TFM_RESIZE ? 1.0f : 0.0f; /* T_NULL_ONE */
  const int dims = getConstraintSpaceDimension(t);
  const int axis = mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2);
  if (dims == 2) {
    if (axis == (CON_AXIS0 | CON_AXIS1)) { vec[2] = nval; }
    else if (axis == (CON_AXIS1 | CON_AXIS2)) { vec[2] = vec[1]; vec[1] = vec[0]; vec[0] = nval; }
    else if (axis == (CON_AXIS0 | CON_AXIS2)) { vec[2] = vec[1]; vec[1] = nval; }
  }
  else if (dims == 1) {
    if (mode & CON_AXIS0) { vec[1] = nval; vec[2] = nval; }
    else if (mode & CON_AXIS1) { vec[1] = vec[0]; vec[0] = nval; vec[2] = nval; }
    else if (mode & CON_AXIS2) { vec[2] = vec[0]; vec[0] = nval; vec[1] = nval; }
  }
}

/* orthographic: view_vector_calc is viewinv[2] whatever the focus; viewAxisCorrectCenter is a no-op */
static void axisProjection(const PGTransInfo *t, const float axis[3], const float in[3], float out[3])
{
  if (is_zero_v3(in)) return;
  float angle = fabsf(angle_v3v3(axis, VIEW_Z));
  if (angle > (float)M_PI_2) angle = (float)M_PI - angle;
  if (angle < DEG2RADF(5.0f)) {
    /* view parallel to the constraint: vertical motion, quadratic */
    float vec[3];
    project_v3_v3v3(vec, in, VIEW_Y);
    float factor = dot_v3v3(VIEW_Y, vec) * 2.0f;
    factor = factor < 0.0f ? factor * -factor : factor * factor;
    normalize_v3_v3_length(out, axis, -factor);
    return;
  }
  float vec[3], v[3], plane[3];
  cross_v3_v3v3(plane, VIEW_Z, axis);
  project_v3_v3v3(vec, in, plane);
  sub_v3_v3v3(vec, in, vec);
  add_v3_v3v3(v, vec, t->center);
  const float factor = dot_v3v3(axis, VIEW_Z);
  if (1.0f - fabsf(factor) < 0.0002f) {
    mul_v3_v3fl(out, axis, factor > 0 ? 1000000000.0f : -1000000000.0f);
    return;
  }
  float mul;
  if (isect_ray_ray_v3(t->center, axis, v, VIEW_Z, &mul, NULL)) mul_v3_v3fl(out, axis, mul);
  for (int i = 0; i < 3; i++) if (!isfinite(out[i])) out[i] = 0.0f;
}

static void constraint_plane_calc(const PGTransInfo *t, float r_plane[4])
{
  const float *cv[2];
  int n = 0;
  for (int i = 0; i < 3 && n < 2; i++) if (t->con_mode & (CON_AXIS0 << i)) cv[n++] = t->spacemtx[i];
  cross_v3_v3v3(r_plane, cv[0], cv[1]);
  normalize_v3(r_plane);
  r_plane[3] = -dot_v3v3(r_plane, t->center);
}

static void applyAxisConstraintVec(const PGTransInfo *t, const float in[3], float out[3])
{
  copy_v3_v3(out, in);
  if (!(t->con_mode & CON_APPLY)) return;
  mul_m3_v3(t->con_pmtx, out);
  const int dims = getConstraintSpaceDimension(t);
  if (dims == 2) {
    if (!is_zero_v3(out)) {
      float plane[4];
      constraint_plane_calc(t, plane);
      if (!(fabsf(dot_v3v3(plane, VIEW_Z)) < 0.001f)) { /* !isPlaneProjectionViewAligned: planeProjection */
        float pos[3], factor;
        add_v3_v3v3(pos, in, t->center);
        if (isect_ray_plane_v3(pos, VIEW_Z, plane, &factor, false)) madd_v3_v3v3fl(out, in, VIEW_Z, factor);
      }
    }
  }
  else if (dims == 1) {
    const int a = (t->con_mode & CON_AXIS0) ? 0 : (t->con_mode & CON_AXIS1) ? 1 : 2;
    axisProjection(t, t->spacemtx[a], in, out);
  }
}

static void applyAxisConstraintSize(const PGTransInfo *t, float r_smat[3][3])
{
  if (!(t->con_mode & CON_APPLY)) return;
  for (int i = 0; i < 3; i++) if (!(t->con_mode & (CON_AXIS0 << i))) r_smat[i][i] = 1.0f;
  float tmat[3][3];
  mul_m3_m3m3(tmat, r_smat, t->spacemtx_inv);
  mul_m3_m3m3(r_smat, t->spacemtx, tmat);
}

static void constraints_rotation_impl(const PGTransInfo *t, float r_axis[3], float *r_angle)
{
  const int mode = t->con_mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2);
  const int a = (mode == CON_AXIS0 || mode == (CON_AXIS1 | CON_AXIS2)) ? 0 :
                (mode == CON_AXIS1 || mode == (CON_AXIS0 | CON_AXIS2)) ? 1 : 2;
  copy_v3_v3(r_axis, t->spacemtx[a]);
  /* don't flip axis if num input or values final (CON_NOFLIP is never set here) */
  if (r_angle && !(pg_numinput_has(&t->num) || t->input == PG_TFM_INPUT_FINAL)) {
    if (dot_v3v3(r_axis, VIEW_Z) > 0.0f) *r_angle = -(*r_angle);
  }
}

int pg_transform_set_constraint(PGTransInfo *t, int axes, int orient)
{
  if (t == NULL || axes < 0 || axes > 7 || (orient != PG_TFM_ORIENT_GLOBAL && orient != PG_TFM_ORIENT_LOCAL)) return 0;
  if (t->mode == PG_TFM_MIRROR) return 0;
  /* shortcut: Local is the object's rotation (transform_orientations.c V3D_ORIENT_LOCAL), and the app's GP
   * object has none, so both give the identity; take ob->object_to_world when objects get a transform. */
  unit_m3(t->spacemtx);
  invert_m3_m3(t->spacemtx_inv, t->spacemtx);
  if (axes == 0) {
    t->con_mode = 0; /* stopConstraint */
    t->num.idx_max = (short)t->idx_max;
  }
  else {
    t->con_mode = CON_APPLY | (axes << 1); /* setConstraint + startConstraint */
    projection_matrix_calc(t, t->con_pmtx);
    t->num.idx_max = (short)min_ii(getConstraintSpaceDimension(t) - 1, t->idx_max);
  }
  return pg_transform_apply_ex(t, t->values, t->input);
}

int pg_transform_numinput(PGTransInfo *t, int key, char *text, int text_len)
{
  if (t == NULL || t->mode == PG_TFM_MIRROR) return 0;
  const int used = pg_numinput_handle(&t->num, key);
  if (used) pg_transform_apply_ex(t, t->values, PG_TFM_INPUT_MOUSE);
  if (text) pg_numinput_output(&t->num, text, text_len);
  return used;
}

int pg_transform_has_numinput(const PGTransInfo *t)
{
  return t && pg_numinput_has(&t->num);
}

float pg_transform_snap_increment(float v, float inc)
{
  return inc != 0.0f ? inc * roundf(v / inc) : v;
}

static float pgx_falloff(const PGTransData *td)
{
  return td->extra ? td->extra->runtime.multi_frame_falloff : 1.0f;
}

/* recalcData_gpencil_strokes: each stroke once (consecutive td share their stroke). */
static void pgx_recalc(PGTransInfo *t)
{
  const bGPDstroke *last = NULL;
  for (int i = 0; i < t->data_len; i++) {
    bGPDstroke *gps = t->data[i].extra;
    if (gps == NULL || gps == last) continue;
    last = gps;
    if (t->is_curve_edit && gps->editcurve != NULL) {
      BKE_gpencil_editcurve_recalculate_handles(gps);
      gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
    }
    BKE_gpencil_stroke_geometry_update(t->gpd, gps);
  }
  BKE_gpencil_batch_cache_dirty_tag(t->gpd);
}

int pg_transform_apply(PGTransInfo *t, const float values[2])
{
  if (values == NULL) return 0;
  const float v[3] = {values[0], values[1], t && t->mode == PG_TFM_RESIZE ? values[0] : 0.0f};
  return pg_transform_apply_ex(t, v, PG_TFM_INPUT_MOUSE);
}

static float large_rotation_limit(float angle)
{
  const float angle_max = (float)(M_PI * 2000.0);
  if (fabsf(angle) > angle_max) {
    const float angle_sign = angle < 0.0f ? -1.0f : 1.0f;
    angle = angle_sign * (fmodf(fabsf(angle), (float)(M_PI * 2.0)) + angle_max);
  }
  return angle;
}

int pg_transform_apply_ex(PGTransInfo *t, const float values[3], int input)
{
  if (t == NULL || values == NULL || !isfinite(values[0]) || !isfinite(values[1]) || !isfinite(values[2])) return 0;
  if (input != PG_TFM_INPUT_MOUSE && input != PG_TFM_INPUT_FINAL) return 0;
  const float in[3] = {values[0], values[1], values[2]};
  memmove(t->values, in, sizeof(in)); /* values may be t->values */
  t->input = input;
  float v[3] = {in[0], in[1], in[2]};
  const bool final = input == PG_TFM_INPUT_FINAL;
  const bool con = (t->con_mode & CON_APPLY) != 0;
  const bool snap = t->o.snap && !final;
  float gdir[3] = {0, 0, 0}, axis[3] = {0, 0, 0}, angle = 0.0f, smat[3][3];
  if (t->mode == PG_TFM_TRANSLATION) { /* applyTranslation */
    if (final) {
      mul_v3_m3v3(gdir, t->spacemtx, v);
    }
    else if (pg_numinput_apply(&t->num, gdir)) {
      if (con) {
        const int a = (t->con_mode & CON_AXIS0) ? 0 : (t->con_mode & CON_AXIS1) ? 1 : 2;
        mul_v3_v3fl(gdir, t->spacemtx[a], gdir[0]);
      }
      else {
        float g[3];
        mul_v3_m3v3(g, t->spacemtx, gdir);
        copy_v3_v3(gdir, g);
      }
    }
    else {
      if (snap) { /* transform_snap_increment: t->snap[0] */
        v[0] = pg_transform_snap_increment(v[0], t->o.snap_translate);
        v[1] = pg_transform_snap_increment(v[1], t->o.snap_translate);
        v[2] = pg_transform_snap_increment(v[2], t->o.snap_translate);
      }
      applyAxisConstraintVec(t, v, gdir);
    }
  }
  else if (t->mode == PG_TFM_ROTATION) { /* applyRotation */
    angle = v[0];
    if (con) constraints_rotation_impl(t, axis, &angle);
    else negate_v3_v3(axis, t->spacemtx[2]); /* orient_axis Z */
    if (final) {
      pg_numinput_apply(&t->num, &angle); /* stores val_org, as Blender */
    }
    else if (pg_numinput_apply(&t->num, &angle)) {
      angle = large_rotation_limit(angle);
    }
    else if (snap) {
      angle = pg_transform_snap_increment(angle, (float)(5.0 * M_PI / 180.0));
    }
    normalize_v3(axis);
  }
  else if (t->mode == PG_TFM_RESIZE) { /* applyResize */
    if (!final) {
      if (snap) for (int i = 0; i < 3; i++) v[i] = pg_transform_snap_increment(v[i], 0.1f);
      if (pg_numinput_apply(&t->num, v)) constraintNumInput(t, v);
    }
    size_to_mat3(smat, v);
    applyAxisConstraintSize(t, smat);
  }
  const int maxis = (int)lroundf(v[0]);
  if (t->mode == PG_TFM_MIRROR && maxis != 0 && maxis != 1) return 0;
  const bool local = (t->around == PG_TFM_PIVOT_INDIVIDUAL); /* transdata_check_local_center */
  for (int i = 0; i < t->data_len; i++) {
    PGTransData *td = &t->data[i];
    const float *c = local ? td->center : t->center;
    float vec[3] = {td->iloc[0] - c[0], td->iloc[1] - c[1], td->iloc[2] - c[2]};
    float out[3];
    switch (t->mode) {
      case PG_TFM_TRANSLATION: { /* applyTranslationValue */
        const float f = td->factor * pgx_falloff(td);
        madd_v3_v3v3fl(out, td->iloc, gdir, f);
        break;
      }
      case PG_TFM_ROTATION: {
        /* ElementRotation_ex: interp_m3_m3m3(identity, R, multi_frame_falloff) of a pure rotation
         * is the rotation by the scaled angle (axis_angle_normalized_to_mat3). */
        const float a = angle * (t->o.prop_edit ? td->factor : 1.0f) * pgx_falloff(td);
        float m[3][3];
        axis_angle_normalized_to_mat3(m, axis, a);
        mul_m3_v3(m, vec);
        add_v3_v3v3(out, c, vec);
        break;
      }
      case PG_TFM_RESIZE: { /* ElementResize (T_POINTS): iloc + (mat * vec - vec) * factor */
        float sv[3];
        mul_v3_m3v3(sv, smat, vec);
        sub_v3_v3(sv, vec);
        madd_v3_v3v3fl(out, td->iloc, sv, td->factor * pgx_falloff(td));
        break;
      }
      default: /* ElementMirror: reflect_v3_v3v3 on the global axis */
        vec[maxis] = -vec[maxis];
        add_v3_v3v3(out, vec, c);
        break;
    }
    memcpy(td->loc, out, sizeof(out));
  }
  pgx_recalc(t);
  return 1;
}

static void pgx_free(PGTransInfo *t)
{
  if (t->rng) BLI_rng_free(t->rng);
  MEM_SAFE_FREE(t->data);
  MEM_freeN(t);
}

void pg_transform_end(PGTransInfo *t, int cancel)
{
  if (t == NULL) return;
  if (cancel && t->data_len > 0) {
    /* restoreTransObjects: positions, then the handle types (restoreElement / hdata) */
    for (int i = 0; i < t->data_len; i++) {
      PGTransData *td = &t->data[i];
      memcpy(td->loc, td->iloc, sizeof(td->iloc));
      if (td->bezt) {
        td->bezt->h1 = td->ih1;
        td->bezt->h2 = td->ih2;
      }
    }
    pgx_recalc(t);
  }
  pgx_free(t);
}

void pg_transform_discard(PGTransInfo *t)
{
  if (t) pgx_free(t);
}
