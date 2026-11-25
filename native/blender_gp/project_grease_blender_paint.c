/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Blender 3.6.23 Legacy GP draw-stroke input (source/blender/editors/gpencil_legacy/gpencil_paint.c),
 * compiled from the original source. The VERBATIM regions below are byte-identical copies
 * (tools/verify_blender_verbatim.py):
 *   gpencil_paint.c:81-103     paint status / add-result / runtime flag enums
 *   gpencil_paint.c:344-385    gpencil_stroke_filtermval
 *   gpencil_paint.c:474-906    gpencil_brush_jitter, gpencil_brush_angle, gpencil_smooth_buffer,
 *                              gpencil_smooth_segment, gpencil_apply_randomness,
 *                              gpencil_stroke_addpoint
 *   gpencil_paint.c:2815-2898  gpencil_draw_apply
 *   gpencil_paint.c:3383-3512  gpencil_brush_angle_segment, gpencil_add_arc_points
 *   gpencil_paint.c:3581-3640  gpencil_add_fake_points
 *   gpencil_utils.c:2826-2869  ED_gpencil_init_random_settings
 * Not verbatim (shims, below): the tGPsdata paint session (only the members these functions read),
 * the window-manager event, and the 3D-view calls. The region is the canvas: region pixels are
 * (x, -y) * px_per_unit (y up as in a Blender region), there is no 3D reprojection, the stroke
 * origin is 0 and the point uv length is measured in canvas units. The event handling of
 * gpencil_draw_modal() / gpencil_draw_apply_event() / gpencil_draw_invoke() that the app needs is
 * restated in pg_paint_event() / pg_paint_begin() with the same statements.
 */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "MEM_guardedalloc.h"

#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_gpencil_legacy.h"

/* types the editor headers expect from BKE_context.h / WM_types.h / RNA_types.h */
typedef struct bContext bContext;
typedef struct wmOperator wmOperator;
typedef struct Depsgraph Depsgraph;
typedef struct Material Material;
typedef struct Object Object;
#include "RNA_types.h"

#include "ED_gpencil_legacy.h"

#include "editors/gpencil_legacy/gpencil_intern.h"

#include "project_grease_blender_paint.h"

/* ---- shims: window manager, context, 3D view ------------------------------------------------- */
typedef struct wmEvent {
  int mval[2];
} wmEvent;

/* gpencil_paint.c MIN_MANHATTEN_PX / MIN_EUCLIDEAN_PX read U (UserDef); the session carries them. */
#define MIN_MANHATTEN_PX (p->manhattan_px)
#define MIN_EUCLIDEAN_PX (p->euclidean_px)

/* Timer of ED_gpencil_init_random_settings(): the session's clock (fixed by a caller seed). */
#define PIL_check_seconds_timer() (pg_paint_random_clock)
static double pg_paint_random_clock;

#define ED_gpencil_init_random_settings pg_paint_init_random_settings
#define ED_gpencil_sbuffer_vertex_color_set(...) ((void)0) /* the backend sets brush vertex color */
#define ED_gpencil_sbuffer_update_eval(...) ((void)0)      /* no evaluated copy of the buffer */
#define ED_gpencil_toggle_brush_cursor(...) ((void)0)
#define BKE_report(...) ((void)0)
#define ED_gpencil_tpoint_to_point pg_paint_tpoint_to_point
#define ED_gpencil_project_point_to_plane(...) ((void)0) /* 2D canvas: already on the plane */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */
typedef enum eGPencil_PaintStatus {
  GP_STATUS_IDLING = 0, /* stroke isn't in progress yet */
  GP_STATUS_PAINTING,   /* a stroke is in progress */
  GP_STATUS_ERROR,      /* something wasn't correctly set up */
  GP_STATUS_DONE,       /* painting done */
} eGPencil_PaintStatus;

/* Return flags for adding points to stroke buffer */
typedef enum eGP_StrokeAdd_Result {
  GP_STROKEADD_INVALID = -2,  /* error occurred - insufficient info to do so */
  GP_STROKEADD_OVERFLOW = -1, /* error occurred - cannot fit any more points */
  GP_STROKEADD_NORMAL,        /* point was successfully added */
  GP_STROKEADD_FULL,          /* cannot add any more points to buffer */
} eGP_StrokeAdd_Result;

/* Runtime flags */
typedef enum eGPencil_PaintFlags {
  GP_PAINTFLAG_FIRSTRUN = (1 << 0), /* operator just started */
  GP_PAINTFLAG_SELECTMASK = (1 << 3),
  GP_PAINTFLAG_HARD_ERASER = (1 << 4),
  GP_PAINTFLAG_STROKE_ERASER = (1 << 5),
  GP_PAINTFLAG_REQ_VECTOR = (1 << 6),
} eGPencil_PaintFlags;
/* END VERBATIM */

/* gpencil_paint.c tGPsdata: only the members the ported functions use, same names and types. */
typedef struct tGPsdata {
  bContext *C;
  Scene *scene;
  struct Depsgraph *depsgraph;
  Object *ob;
  Object *ob_eval;
  ARegion *region;
  struct {
    void *data;
  } ownerPtr; /* PointerRNA: the GP object */
  bGPdata *gpd;
  bGPDlayer *gpl;
  eGPencil_PaintStatus status;
  eGPencil_PaintModes paintmode;
  eGPencil_PaintFlags flags;
  float mval[2];
  float mvalo[2];
  float mvali[2];
  float pressure;
  float opressure;
  double inittime;
  double curtime;
  double ocurtime;
  Material *material;
  Brush *brush;
  short straight;
  int lock_axis;
  RNG *rng;
  bool shift;
  float totpixlen;
  bool disable_stabilizer;
  GpRandomSettings random_settings;
  /* shim: U.gp_manhattandist / U.gp_euclideandist */
  int manhattan_px;
  int euclidean_px;
} tGPsdata;

/* gpencil_get_3d_reference(): the drawing origin; the canvas plane passes through 0. */
static void gpencil_get_3d_reference(tGPsdata *UNUSED(p), float vec[3])
{
  zero_v3(vec);
}

/* ED_gpencil_tpoint_to_point(): region pixels back to canvas units (regiondata holds px/unit). */
static void pg_paint_tpoint_to_point(ARegion *region,
                                     float UNUSED(origin[3]),
                                     const tGPspoint *tpt,
                                     bGPDspoint *pt)
{
  const float ppu = *(const float *)region->regiondata;
  pt->x = tpt->m_xy[0] / ppu;
  pt->y = -tpt->m_xy[1] / ppu;
  pt->z = 0.0f;
}

/* Only reached for GP_PAINTMODE_ERASER, guides and a full buffer: none happen in this session
 * (eraser and guides are separate tools; ED_gpencil_sbuffer_ensure() grows without limit). */
static void gpencil_stroke_doeraser(tGPsdata *UNUSED(p)) {}
static void gpencil_paint_strokeend(tGPsdata *UNUSED(p)) {}
static void gpencil_paint_initstroke(tGPsdata *UNUSED(p),
                                     eGPencil_PaintModes UNUSED(paintmode),
                                     struct Depsgraph *UNUSED(depsgraph))
{
}
static void gpencil_snap_to_guide(const tGPsdata *UNUSED(p),
                                  const GP_Sculpt_Guide *UNUSED(guide),
                                  float UNUSED(point[2]))
{
}
static void gpencil_add_guide_points(const tGPsdata *UNUSED(p),
                                     const GP_Sculpt_Guide *UNUSED(guide),
                                     const float UNUSED(start[2]),
                                     const float UNUSED(end[2]),
                                     int UNUSED(segments))
{
}


/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */
/* Stroke Editing ---------------------------- */
/* check if the current mouse position is suitable for adding a new point */
static bool gpencil_stroke_filtermval(tGPsdata *p, const float mval[2], const float mvalo[2])
{
  Brush *brush = p->brush;
  int dx = (int)fabsf(mval[0] - mvalo[0]);
  int dy = (int)fabsf(mval[1] - mvalo[1]);
  brush->gpencil_settings->flag &= ~GP_BRUSH_STABILIZE_MOUSE_TEMP;

  /* if buffer is empty, just let this go through (i.e. so that dots will work) */
  if (p->gpd->runtime.sbuffer_used == 0) {
    return true;
  }
  /* if lazy mouse, check minimum distance */
  if (GPENCIL_LAZY_MODE(brush, p->shift) && (!p->disable_stabilizer)) {
    brush->gpencil_settings->flag |= GP_BRUSH_STABILIZE_MOUSE_TEMP;
    if ((dx * dx + dy * dy) > (brush->smooth_stroke_radius * brush->smooth_stroke_radius)) {
      return true;
    }

    /* If the mouse is moving within the radius of the last move,
     * don't update the mouse position. This allows sharp turns. */
    copy_v2_v2(p->mval, p->mvalo);
    return false;
  }
  /* check if mouse moved at least certain distance on both axes (best case)
   * - aims to eliminate some jitter-noise from input when trying to draw straight lines freehand
   */
  if ((dx > MIN_MANHATTEN_PX) && (dy > MIN_MANHATTEN_PX)) {
    return true;
  }
  /* Check if the distance since the last point is significant enough:
   * - Prevents points being added too densely
   * - Distance here doesn't use `sqrt` to prevent slowness.
   *   We should still be safe from overflows though.
   */
  if ((dx * dx + dy * dy) > MIN_EUCLIDEAN_PX * MIN_EUCLIDEAN_PX) {
    return true;
  }
  /* mouse 'didn't move' */
  return false;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */
/* Apply jitter to stroke point. */
static void gpencil_brush_jitter(bGPdata *gpd, tGPspoint *pt, const float amplitude)
{
  const float axis[2] = {0.0f, 1.0f};
  /* Jitter is applied perpendicular to the mouse movement vector (2D space). */
  float mvec[2];
  /* Mouse movement in ints -> floats. */
  if (gpd->runtime.sbuffer_used > 1) {
    tGPspoint *pt_prev = pt - 1;
    sub_v2_v2v2(mvec, pt->m_xy, pt_prev->m_xy);
    normalize_v2(mvec);
    /* Rotate mvec by 90 degrees... */
    float angle = angle_v2v2(mvec, axis);
    /* Reduce noise in the direction of the stroke. */
    mvec[0] *= cos(angle);
    mvec[1] *= sin(angle);

    /* Scale by displacement amount, and apply. */
    madd_v2_v2fl(pt->m_xy, mvec, amplitude * 10.0f);
  }
}

/* Apply pressure change depending of the angle of the stroke to simulate a pen with shape */
static void gpencil_brush_angle(bGPdata *gpd, Brush *brush, tGPspoint *pt, const float mval[2])
{
  float mvec[2];
  float sen = brush->gpencil_settings->draw_angle_factor; /* sensitivity */
  float fac;

  /* default angle of brush in radians */
  float angle = brush->gpencil_settings->draw_angle;
  /* angle vector of the brush with full thickness */
  const float v0[2] = {cos(angle), sin(angle)};

  /* Apply to first point (only if there are 2 points because before no data to do it ) */
  if (gpd->runtime.sbuffer_used == 1) {
    sub_v2_v2v2(mvec, mval, (pt - 1)->m_xy);
    normalize_v2(mvec);

    /* uses > 1.0f to get a smooth transition in first point */
    fac = 1.4f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
    (pt - 1)->pressure = (pt - 1)->pressure - (sen * fac);

    CLAMP((pt - 1)->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
  }

  /* apply from second point */
  if (gpd->runtime.sbuffer_used >= 1) {
    sub_v2_v2v2(mvec, mval, (pt - 1)->m_xy);
    normalize_v2(mvec);

    fac = 1.0f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
    /* interpolate with previous point for smoother transitions */
    pt->pressure = interpf(pt->pressure - (sen * fac), (pt - 1)->pressure, 0.3f);
    CLAMP(pt->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
  }
}

/**
 * Apply smooth to buffer while drawing
 * to smooth point C, use 2 before (A, B) and current point (D):
 *
 * `A----B-----C------D`
 *
 * \param p: Temp data
 * \param inf: Influence factor
 * \param idx: Index of the last point (need minimum 3 points in the array)
 */
static void gpencil_smooth_buffer(tGPsdata *p, float inf, int idx)
{
  bGPdata *gpd = p->gpd;
  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
  const short num_points = gpd->runtime.sbuffer_used;

  /* Do nothing if not enough points to smooth out */
  if ((num_points < 3) || (idx < 3) || (inf == 0.0f)) {
    return;
  }

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  const float steps = (idx < 4) ? 3.0f : 4.0f;

  tGPspoint *pta = idx >= 4 ? &points[idx - 4] : NULL;
  tGPspoint *ptb = idx >= 3 ? &points[idx - 3] : NULL;
  tGPspoint *ptc = idx >= 2 ? &points[idx - 2] : NULL;
  tGPspoint *ptd = &points[idx - 1];

  float sco[2] = {0.0f};
  float a[2], b[2], c[2], d[2];
  float pressure = 0.0f;
  float strength = 0.0f;
  const float average_fac = 1.0f / steps;

  /* Compute smoothed coordinate by taking the ones nearby */
  if (pta) {
    copy_v2_v2(a, pta->m_xy);
    madd_v2_v2fl(sco, a, average_fac);
    pressure += pta->pressure * average_fac;
    strength += pta->strength * average_fac;
  }
  if (ptb) {
    copy_v2_v2(b, ptb->m_xy);
    madd_v2_v2fl(sco, b, average_fac);
    pressure += ptb->pressure * average_fac;
    strength += ptb->strength * average_fac;
  }
  if (ptc) {
    copy_v2_v2(c, ptc->m_xy);
    madd_v2_v2fl(sco, c, average_fac);
    pressure += ptc->pressure * average_fac;
    strength += ptc->strength * average_fac;
  }
  if (ptd) {
    copy_v2_v2(d, ptd->m_xy);
    madd_v2_v2fl(sco, d, average_fac);
    pressure += ptd->pressure * average_fac;
    strength += ptd->strength * average_fac;
  }

  /* Based on influence factor, blend between original and optimal smoothed coordinate but not
   * for Guide mode. */
  if (!guide->use_guide) {
    interp_v2_v2v2(c, c, sco, inf);
    copy_v2_v2(ptc->m_xy, c);
  }
  /* Interpolate pressure. */
  ptc->pressure = interpf(ptc->pressure, pressure, inf);
  /* Interpolate strength. */
  ptc->strength = interpf(ptc->strength, strength, inf);
}

/* Helper: Apply smooth to segment from Index to Index */
static void gpencil_smooth_segment(bGPdata *gpd, const float inf, int from_idx, int to_idx)
{
  const short num_points = to_idx - from_idx;
  /* Do nothing if not enough points to smooth out */
  if ((num_points < 3) || (inf == 0.0f)) {
    return;
  }

  if (from_idx <= 2) {
    return;
  }

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  const float average_fac = 0.25f;

  for (int i = from_idx; i < to_idx + 1; i++) {

    tGPspoint *pta = i >= 3 ? &points[i - 3] : NULL;
    tGPspoint *ptb = i >= 2 ? &points[i - 2] : NULL;
    tGPspoint *ptc = i >= 1 ? &points[i - 1] : &points[i];
    tGPspoint *ptd = &points[i];

    float sco[2] = {0.0f};
    float pressure = 0.0f;
    float strength = 0.0f;

    /* Compute smoothed coordinate by taking the ones nearby */
    if (pta) {
      madd_v2_v2fl(sco, pta->m_xy, average_fac);
      pressure += pta->pressure * average_fac;
      strength += pta->strength * average_fac;
    }
    else {
      madd_v2_v2fl(sco, ptc->m_xy, average_fac);
      pressure += ptc->pressure * average_fac;
      strength += ptc->strength * average_fac;
    }

    if (ptb) {
      madd_v2_v2fl(sco, ptb->m_xy, average_fac);
      pressure += ptb->pressure * average_fac;
      strength += ptb->strength * average_fac;
    }
    else {
      madd_v2_v2fl(sco, ptc->m_xy, average_fac);
      pressure += ptc->pressure * average_fac;
      strength += ptc->strength * average_fac;
    }

    madd_v2_v2fl(sco, ptc->m_xy, average_fac);
    pressure += ptc->pressure * average_fac;
    strength += ptc->strength * average_fac;

    madd_v2_v2fl(sco, ptd->m_xy, average_fac);
    pressure += ptd->pressure * average_fac;
    strength += ptd->strength * average_fac;

    /* Based on influence factor, blend between original and optimal smoothed coordinate. */
    interp_v2_v2v2(ptc->m_xy, ptc->m_xy, sco, inf);

    /* Interpolate pressure. */
    ptc->pressure = interpf(ptc->pressure, pressure, inf);
    /* Interpolate strength. */
    ptc->strength = interpf(ptc->strength, strength, inf);
  }
}

static void gpencil_apply_randomness(tGPsdata *p,
                                     BrushGpencilSettings *brush_settings,
                                     tGPspoint *pt,
                                     const bool press,
                                     const bool strength,
                                     const bool uv)
{
  bGPdata *gpd = p->gpd;
  GpRandomSettings random_settings = p->random_settings;
  float value = 0.0f;
  /* Apply randomness to pressure. */
  if ((brush_settings->draw_random_press > 0.0f) && (press)) {
    if ((brush_settings->flag2 & GP_BRUSH_USE_PRESS_AT_STROKE) == 0) {
      float rand = BLI_rng_get_float(p->rng) * 2.0f - 1.0f;
      value = 1.0 + rand * 2.0 * brush_settings->draw_random_press;
    }
    else {
      value = 1.0 + random_settings.pressure * brush_settings->draw_random_press;
    }

    /* Apply random curve. */
    if (brush_settings->flag2 & GP_BRUSH_USE_PRESSURE_RAND_PRESS) {
      value *= BKE_curvemapping_evaluateF(
          brush_settings->curve_rand_pressure, 0, random_settings.pen_press);
    }

    pt->pressure *= value;
    CLAMP(pt->pressure, 0.1f, 1.0f);
  }

  /* Apply randomness to color strength. */
  if ((brush_settings->draw_random_strength) && (strength)) {
    if ((brush_settings->flag2 & GP_BRUSH_USE_STRENGTH_AT_STROKE) == 0) {
      float rand = BLI_rng_get_float(p->rng) * 2.0f - 1.0f;
      value = 1.0 + rand * brush_settings->draw_random_strength;
    }
    else {
      value = 1.0 + random_settings.strength * brush_settings->draw_random_strength;
    }

    /* Apply random curve. */
    if (brush_settings->flag2 & GP_BRUSH_USE_STRENGTH_RAND_PRESS) {
      value *= BKE_curvemapping_evaluateF(
          brush_settings->curve_rand_pressure, 0, random_settings.pen_press);
    }

    pt->strength *= value;
    CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
  }

  /* Apply randomness to uv texture rotation. */
  if ((brush_settings->uv_random > 0.0f) && (uv)) {
    if ((brush_settings->flag2 & GP_BRUSH_USE_UV_AT_STROKE) == 0) {
      float rand = BLI_hash_int_01(BLI_hash_int_2d((int)pt->m_xy[0], gpd->runtime.sbuffer_used)) *
                       2.0f -
                   1.0f;
      value = rand * M_PI_2 * brush_settings->uv_random;
    }
    else {
      value = random_settings.uv * M_PI_2 * brush_settings->uv_random;
    }

    /* Apply random curve. */
    if (brush_settings->flag2 & GP_BRUSH_USE_UV_RAND_PRESS) {
      value *= BKE_curvemapping_evaluateF(
          brush_settings->curve_rand_uv, 0, random_settings.pen_press);
    }

    pt->uv_rot += value;
    CLAMP(pt->uv_rot, -M_PI_2, M_PI_2);
  }
}

/* add current stroke-point to buffer (returns whether point was successfully added) */
static short gpencil_stroke_addpoint(tGPsdata *p,
                                     const float mval[2],
                                     float pressure,
                                     double curtime)
{
  bGPdata *gpd = p->gpd;
  Brush *brush = p->brush;
  BrushGpencilSettings *brush_settings = p->brush->gpencil_settings;
  tGPspoint *pt;
  Object *obact = (Object *)p->ownerPtr.data;
  RegionView3D *rv3d = p->region->regiondata;

  /* check painting mode */
  if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
    /* straight lines only - i.e. only store start and end point in buffer */
    if (gpd->runtime.sbuffer_used == 0) {
      /* first point in buffer (start point) */
      pt = (tGPspoint *)(gpd->runtime.sbuffer);

      /* store settings */
      copy_v2_v2(pt->m_xy, mval);
      /* Pressure values are unreliable, so ignore for now, see #44932. */
      pt->pressure = 1.0f;
      pt->strength = 1.0f;
      pt->time = (float)(curtime - p->inittime);

      /* increment buffer size */
      gpd->runtime.sbuffer_used++;
    }
    else {
      /* just reset the endpoint to the latest value
       * - assume that pointers for this are always valid...
       */
      pt = ((tGPspoint *)(gpd->runtime.sbuffer) + 1);

      /* store settings */
      copy_v2_v2(pt->m_xy, mval);
      /* Pressure values are unreliable, so ignore for now, see #44932. */
      pt->pressure = 1.0f;
      pt->strength = 1.0f;
      pt->time = (float)(curtime - p->inittime);

      /* now the buffer has 2 points (and shouldn't be allowed to get any larger) */
      gpd->runtime.sbuffer_used = 2;
    }

    /* can keep carrying on this way :) */
    return GP_STROKEADD_NORMAL;
  }

  if (p->paintmode == GP_PAINTMODE_DRAW) { /* normal drawing */
    /* check if still room in buffer or add more */
    gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
        gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, false);

    /* Check the buffer was created. */
    if (gpd->runtime.sbuffer == NULL) {
      return GP_STROKEADD_INVALID;
    }

    /* get pointer to destination point */
    pt = ((tGPspoint *)(gpd->runtime.sbuffer) + gpd->runtime.sbuffer_used);

    /* store settings */
    pt->strength = brush_settings->draw_strength;
    pt->pressure = 1.0f;
    pt->uv_rot = 0.0f;
    copy_v2_v2(pt->m_xy, mval);

    /* pressure */
    if (brush_settings->flag & GP_BRUSH_USE_PRESSURE) {
      pt->pressure *= BKE_curvemapping_evaluateF(brush_settings->curve_sensitivity, 0, pressure);
    }

    /* color strength */
    if (brush_settings->flag & GP_BRUSH_USE_STRENGTH_PRESSURE) {
      pt->strength *= BKE_curvemapping_evaluateF(brush_settings->curve_strength, 0, pressure);
      CLAMP(pt->strength, MIN2(GPENCIL_STRENGTH_MIN, brush_settings->draw_strength), 1.0f);
    }

    /* Set vertex colors for buffer. */
    ED_gpencil_sbuffer_vertex_color_set(p->depsgraph,
                                        p->ob,
                                        p->scene->toolsettings,
                                        p->brush,
                                        p->material,
                                        p->random_settings.hsv,
                                        p->random_settings.pen_press);

    if (brush_settings->flag & GP_BRUSH_GROUP_RANDOM) {
      /* Apply jitter to position */
      if (brush_settings->draw_jitter > 0.0f) {
        float rand = BLI_rng_get_float(p->rng) * 2.0f - 1.0f;
        float jitpress = 1.0f;
        if (brush_settings->flag & GP_BRUSH_USE_JITTER_PRESSURE) {
          jitpress = BKE_curvemapping_evaluateF(brush_settings->curve_jitter, 0, pressure);
        }
        /* FIXME the +2 means minimum jitter is 4 which is a bit strange for UX. */
        const float exp_factor = brush_settings->draw_jitter + 2.0f;
        const float fac = rand * square_f(exp_factor) * jitpress;
        gpencil_brush_jitter(gpd, pt, fac);
      }

      /* Apply other randomness. */
      gpencil_apply_randomness(p, brush_settings, pt, true, true, true);
    }

    /* apply angle of stroke to brush size */
    if (brush_settings->draw_angle_factor != 0.0f) {
      gpencil_brush_angle(gpd, brush, pt, mval);
    }

    /* point time */
    pt->time = (float)(curtime - p->inittime);

    /* point uv (only 3d view) */
    if (gpd->runtime.sbuffer_used > 0) {
      tGPspoint *ptb = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_used - 1;
      bGPDspoint spt, spt2;

      /* get origin to reproject point */
      float origin[3];
      gpencil_get_3d_reference(p, origin);
      /* reproject current */
      ED_gpencil_tpoint_to_point(p->region, origin, pt, &spt);
      ED_gpencil_project_point_to_plane(
          p->scene, obact, p->gpl, rv3d, origin, p->lock_axis - 1, &spt);

      /* reproject previous */
      ED_gpencil_tpoint_to_point(p->region, origin, ptb, &spt2);
      ED_gpencil_project_point_to_plane(
          p->scene, obact, p->gpl, rv3d, origin, p->lock_axis - 1, &spt2);
      p->totpixlen += len_v3v3(&spt.x, &spt2.x);
      pt->uv_fac = p->totpixlen;
    }
    else {
      p->totpixlen = 0.0f;
      pt->uv_fac = 0.0f;
    }

    /* increment counters */
    gpd->runtime.sbuffer_used++;

    /* Smooth while drawing previous points with a reduction factor for previous. */
    if (brush->gpencil_settings->active_smooth > 0.0f) {
      for (int s = 0; s < 3; s++) {
        gpencil_smooth_buffer(p,
                              brush->gpencil_settings->active_smooth * ((3.0f - s) / 3.0f),
                              gpd->runtime.sbuffer_used - s);
      }
    }

    /* Update evaluated data. */
    ED_gpencil_sbuffer_update_eval(gpd, p->ob_eval);

    return GP_STROKEADD_NORMAL;
  }
  /* return invalid state for now... */
  return GP_STROKEADD_INVALID;
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */
static void gpencil_draw_apply(bContext *C, wmOperator *op, tGPsdata *p, Depsgraph *depsgraph)
{
  bGPdata *gpd = p->gpd;
  tGPspoint *pt = NULL;

  /* handle drawing/erasing -> test for erasing first */
  if (p->paintmode == GP_PAINTMODE_ERASER) {
    /* do 'live' erasing now */
    gpencil_stroke_doeraser(p);

    /* store used values */
    copy_v2_v2(p->mvalo, p->mval);
    p->opressure = p->pressure;
  }
  /* Only add current point to buffer if mouse moved
   * (even though we got an event, it might be just noise). */
  else if (gpencil_stroke_filtermval(p, p->mval, p->mvalo)) {

    /* if lazy mouse, interpolate the last and current mouse positions */
    if (GPENCIL_LAZY_MODE(p->brush, p->shift) && (!p->disable_stabilizer)) {
      float now_mouse[2];
      float last_mouse[2];
      copy_v2_v2(now_mouse, p->mval);
      copy_v2_v2(last_mouse, p->mvalo);
      interp_v2_v2v2(now_mouse, now_mouse, last_mouse, p->brush->smooth_stroke_factor);
      copy_v2_v2(p->mval, now_mouse);

      GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
      bool is_speed_guide = ((guide->use_guide) &&
                             (p->brush && (p->brush->gpencil_tool == GPAINT_TOOL_DRAW)));
      if (is_speed_guide) {
        gpencil_snap_to_guide(p, guide, p->mval);
      }
    }

    /* try to add point */
    short ok = gpencil_stroke_addpoint(p, p->mval, p->pressure, p->curtime);

    /* handle errors while adding point */
    if (ELEM(ok, GP_STROKEADD_FULL, GP_STROKEADD_OVERFLOW)) {
      /* finish off old stroke */
      gpencil_paint_strokeend(p);
      /* And start a new one!!! Else, projection errors! */
      gpencil_paint_initstroke(p, p->paintmode, depsgraph);

      /* start a new stroke, starting from previous point */
      /* XXX Must manually reset inittime... */
      /* XXX We only need to reuse previous point if overflow! */
      if (ok == GP_STROKEADD_OVERFLOW) {
        p->inittime = p->ocurtime;
        gpencil_stroke_addpoint(p, p->mvalo, p->opressure, p->ocurtime);
      }
      else {
        p->inittime = p->curtime;
      }
      gpencil_stroke_addpoint(p, p->mval, p->pressure, p->curtime);
    }
    else if (ok == GP_STROKEADD_INVALID) {
      /* the painting operation cannot continue... */
      BKE_report(op->reports, RPT_ERROR, "Cannot paint stroke");
      p->status = GP_STATUS_ERROR;

      return;
    }

    /* store used values */
    copy_v2_v2(p->mvalo, p->mval);
    p->opressure = p->pressure;
    p->ocurtime = p->curtime;

    pt = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_used - 1;
    if (p->paintmode != GP_PAINTMODE_ERASER) {
      ED_gpencil_toggle_brush_cursor(C, true, pt->m_xy);
    }
  }
  else if ((p->brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP) &&
           (gpd->runtime.sbuffer_used > 0))
  {
    pt = (tGPspoint *)gpd->runtime.sbuffer + gpd->runtime.sbuffer_used - 1;
    if (p->paintmode != GP_PAINTMODE_ERASER) {
      ED_gpencil_toggle_brush_cursor(C, true, pt->m_xy);
    }
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */
static void gpencil_brush_angle_segment(tGPsdata *p, tGPspoint *pt_prev, tGPspoint *pt)
{
  Brush *brush = p->brush;
  /* Sensitivity. */
  const float sen = brush->gpencil_settings->draw_angle_factor;
  /* Default angle of brush in radians */
  const float angle = brush->gpencil_settings->draw_angle;

  float mvec[2];
  float fac;

  /* angle vector of the brush with full thickness */
  const float v0[2] = {cos(angle), sin(angle)};

  sub_v2_v2v2(mvec, pt->m_xy, pt_prev->m_xy);
  normalize_v2(mvec);
  fac = 1.0f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
  /* interpolate with previous point for smoother transitions */
  pt->pressure = interpf(pt->pressure - (sen * fac), pt_prev->pressure, 0.3f);
  CLAMP(pt->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
}

/* Add arc points between two mouse events using the previous segment to determine the vertex of
 * the arc.
 *        /+ CTL
 *       / |
 *      /  |
 * PtA +...|...+ PtB
 *    /
 *   /
 *  + PtA - 1
 * /
 * CTL is the vertex of the triangle created between PtA and PtB */
static void gpencil_add_arc_points(tGPsdata *p, const float mval[2], int segments)
{
  bGPdata *gpd = p->gpd;
  BrushGpencilSettings *brush_settings = p->brush->gpencil_settings;

  if (gpd->runtime.sbuffer_used < 3) {
    tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
    /* Apply other randomness to first points. */
    for (int i = 0; i < gpd->runtime.sbuffer_used; i++) {
      tGPspoint *pt = &points[i];
      gpencil_apply_randomness(p, brush_settings, pt, false, false, true);
    }
    return;
  }
  int idx_prev = gpd->runtime.sbuffer_used;

  /* Add space for new arc points. */
  gpd->runtime.sbuffer_used += segments - 1;

  /* Check if still room in buffer or add more. */
  gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      gpd->runtime.sbuffer, &gpd->runtime.sbuffer_size, &gpd->runtime.sbuffer_used, false);

  tGPspoint *points = (tGPspoint *)gpd->runtime.sbuffer;
  tGPspoint *pt = NULL;
  tGPspoint *pt_before = &points[idx_prev - 1]; /* current - 2 */
  tGPspoint *pt_prev = &points[idx_prev - 2];   /* previous */

  /* Create two vectors, previous and half way of the actual to get the vertex of the triangle
   * for arc curve.
   */
  float v_prev[2], v_cur[2], v_half[2];
  sub_v2_v2v2(v_cur, mval, pt_prev->m_xy);

  sub_v2_v2v2(v_prev, pt_prev->m_xy, pt_before->m_xy);
  interp_v2_v2v2(v_half, pt_prev->m_xy, mval, 0.5f);
  sub_v2_v2(v_half, pt_prev->m_xy);

  /* If angle is too sharp undo all changes and return. */
  const float min_angle = DEG2RADF(120.0f);
  float angle = angle_v2v2(v_prev, v_half);
  if (angle < min_angle) {
    gpd->runtime.sbuffer_used -= segments - 1;
    return;
  }

  /* Project the half vector to the previous vector and calculate the mid projected point. */
  float dot = dot_v2v2(v_prev, v_half);
  float l = len_squared_v2(v_prev);
  if (l > 0.0f) {
    mul_v2_fl(v_prev, dot / l);
  }

  /* Calc the position of the control point. */
  float ctl[2];
  add_v2_v2v2(ctl, pt_prev->m_xy, v_prev);

  float step = M_PI_2 / (float)(segments + 1);
  float a = step;

  float midpoint[2], start[2], end[2], cp1[2], corner[2];
  mid_v2_v2v2(midpoint, pt_prev->m_xy, mval);
  copy_v2_v2(start, pt_prev->m_xy);
  copy_v2_v2(end, mval);
  copy_v2_v2(cp1, ctl);

  corner[0] = midpoint[0] - (cp1[0] - midpoint[0]);
  corner[1] = midpoint[1] - (cp1[1] - midpoint[1]);
  float stepcolor = 1.0f / segments;

  tGPspoint *pt_step = pt_prev;
  for (int i = 0; i < segments; i++) {
    pt = &points[idx_prev + i - 1];
    pt->m_xy[0] = corner[0] + (end[0] - corner[0]) * sinf(a) + (start[0] - corner[0]) * cosf(a);
    pt->m_xy[1] = corner[1] + (end[1] - corner[1]) * sinf(a) + (start[1] - corner[1]) * cosf(a);

    /* Set pressure and strength equals to previous. It will be smoothed later. */
    pt->pressure = pt_prev->pressure;
    pt->strength = pt_prev->strength;
    /* Interpolate vertex color. */
    interp_v4_v4v4(
        pt->vert_color, pt_before->vert_color, pt_prev->vert_color, stepcolor * (i + 1));

    /* Apply angle of stroke to brush size to interpolated points but slightly attenuated. */
    if (brush_settings->draw_angle_factor != 0.0f) {
      gpencil_brush_angle_segment(p, pt_step, pt);
      CLAMP(pt->pressure, pt_prev->pressure * 0.5f, 1.0f);
      /* Use the previous interpolated point for next segment. */
      pt_step = pt;
    }

    /* Apply other randomness. */
    gpencil_apply_randomness(p, brush_settings, pt, false, false, true);

    a += step;
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */

/**
 * Add fake points for missing mouse movements when the artist draw very fast creating an arc
 * with the vertex in the middle of the segment and using the angle of the previous segment.
 */
static void gpencil_add_fake_points(const wmEvent *event, tGPsdata *p)
{
  Brush *brush = p->brush;
  /* Lazy mode do not use fake events. */
  if (GPENCIL_LAZY_MODE(brush, p->shift) && (!p->disable_stabilizer)) {
    return;
  }

  GP_Sculpt_Guide *guide = &p->scene->toolsettings->gp_sculpt.guide;
  int input_samples = brush->gpencil_settings->input_samples;
  bool is_speed_guide = ((guide->use_guide) &&
                         (p->brush && (p->brush->gpencil_tool == GPAINT_TOOL_DRAW)));

  /* TODO: ensure sampling enough points when using circular guide,
   * but the arc must be around the center. (see if above to check other guides only). */
  if (is_speed_guide && (guide->type == GP_GUIDE_CIRCULAR)) {
    input_samples = GP_MAX_INPUT_SAMPLES;
  }

  if (input_samples == 0) {
    return;
  }

  int samples = GP_MAX_INPUT_SAMPLES - input_samples + 1;

  float mouse_prv[2], mouse_cur[2];
  float min_dist = 4.0f * samples;

  copy_v2_v2(mouse_prv, p->mvalo);
  copy_v2fl_v2i(mouse_cur, event->mval);

  /* get distance in pixels */
  float dist = len_v2v2(mouse_prv, mouse_cur);

  /* get distance for circular guide */
  if (is_speed_guide && (guide->type == GP_GUIDE_CIRCULAR)) {
    float middle[2];
    gpencil_snap_to_guide(p, guide, mouse_prv);
    gpencil_snap_to_guide(p, guide, mouse_cur);
    mid_v2_v2v2(middle, mouse_cur, mouse_prv);
    gpencil_snap_to_guide(p, guide, middle);
    dist = len_v2v2(mouse_prv, middle) + len_v2v2(middle, mouse_cur);
  }

  if ((dist > 3.0f) && (dist > min_dist)) {
    int slices = (dist / min_dist) + 1;

    if (is_speed_guide) {
      gpencil_add_guide_points(p, guide, mouse_prv, mouse_cur, slices);
    }
    else {
      gpencil_add_arc_points(p, mouse_cur, slices);
    }
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_utils.c */
void ED_gpencil_init_random_settings(Brush *brush,
                                     const int mval[2],
                                     GpRandomSettings *random_settings)
{
  int seed = ((uint)ceil(PIL_check_seconds_timer()) + 1) % 128;
  /* Use mouse position to get randomness. */
  int ix = mval[0] * seed;
  int iy = mval[1] * seed;
  int iz = ix + iy * seed;
  zero_v3(random_settings->hsv);

  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  /* Random to Hue. */
  if (brush_settings->random_hue > 0.0f) {
    float rand = BLI_hash_int_01(BLI_hash_int_2d(ix, iy)) * 2.0f - 1.0f;
    random_settings->hsv[0] = rand * brush_settings->random_hue * 0.5f;
  }
  /* Random to Saturation. */
  if (brush_settings->random_saturation > 0.0f) {
    float rand = BLI_hash_int_01(BLI_hash_int_2d(iy, ix)) * 2.0f - 1.0f;
    random_settings->hsv[1] = rand * brush_settings->random_saturation;
  }
  /* Random to Value. */
  if (brush_settings->random_value > 0.0f) {
    float rand = BLI_hash_int_01(BLI_hash_int_2d(ix * iz, iy * iz)) * 2.0f - 1.0f;
    random_settings->hsv[2] = rand * brush_settings->random_value;
  }

  /* Random to pressure. */
  if (brush_settings->draw_random_press > 0.0f) {
    random_settings->pressure = BLI_hash_int_01(BLI_hash_int_2d(ix + iz, iy + iz)) * 2.0f - 1.0f;
  }

  /* Random to color strength. */
  if (brush_settings->draw_random_strength) {
    random_settings->strength = BLI_hash_int_01(BLI_hash_int_2d(ix + iy, iy + iz + ix)) * 2.0f -
                                1.0f;
  }

  /* Random to uv texture rotation. */
  if (brush_settings->uv_random > 0.0f) {
    random_settings->uv = BLI_hash_int_01(BLI_hash_int_2d(iy + iz, ix * iz)) * 2.0f - 1.0f;
  }
}
/* END VERBATIM */

/* ---- the paint session --------------------------------------------------------------------- */
struct PGPaintSession {
  tGPsdata p;
  bGPdata gpd;
  Brush brush;
  BrushGpencilSettings settings;
  Scene scene;
  ToolSettings ts;
  ARegion region;
  float px_per_unit;
};

static CurveMapping *pg_paint_curve(const PGCurve *c)
{
  /* BKE_brush_init_gpencil_settings(): BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f) */
  CurveMapping *cumap = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  if (c && c->built && c->totpoint >= 2) {
    CurveMap *cuma = &cumap->cm[0];
    MEM_freeN(cuma->curve);
    cuma->totpoint = c->totpoint;
    cuma->curve = MEM_callocN(sizeof(CurveMapPoint) * (size_t)c->totpoint, "pg brush curve");
    for (int i = 0; i < c->totpoint; i++) {
      cuma->curve[i].x = c->x[i];
      cuma->curve[i].y = c->y[i];
      cuma->curve[i].flag = c->vector[i] ? CUMA_HANDLE_VECTOR : 0;
    }
    BKE_curvemapping_changed(cumap, false);
  }
  BKE_curvemapping_init(cumap);
  return cumap;
}

PGPaintSession *pg_paint_session_begin(const PGDrawSettings *s, unsigned int rng_seed,
                                       double random_clock)
{
  PGPaintSession *ps = MEM_callocN(sizeof(*ps), "PGPaintSession");
  BrushGpencilSettings *bs = &ps->settings;
  bs->draw_strength = s->draw_strength;
  bs->flag = (s->use_pressure ? GP_BRUSH_USE_PRESSURE : 0) |
             (s->use_strength_pressure ? GP_BRUSH_USE_STRENGTH_PRESSURE : 0) |
             (s->lazy_enabled ? GP_BRUSH_STABILIZE_MOUSE : 0) | GP_BRUSH_USE_JITTER_PRESSURE |
             ((s->jitter > 0.0f || s->random_press > 0.0f || s->random_strength > 0.0f ||
               s->uv_random > 0.0f) ?
                  GP_BRUSH_GROUP_RANDOM :
                  0);
  bs->flag2 = (int)s->random_flag2;
  bs->draw_jitter = s->jitter;
  bs->draw_random_press = s->random_press;
  bs->draw_random_strength = s->random_strength;
  bs->uv_random = s->uv_random;
  bs->draw_angle_factor = s->draw_angle_factor;
  bs->draw_angle = s->draw_angle;
  bs->active_smooth = s->active_smooth;
  bs->input_samples = s->synthesize_fast_points ? s->input_samples : 0;
  bs->curve_sensitivity = pg_paint_curve(&s->pressure_map);
  bs->curve_strength = pg_paint_curve(&s->strength_map);
  bs->curve_jitter = pg_paint_curve(NULL);
  bs->curve_rand_pressure = pg_paint_curve(NULL);
  bs->curve_rand_strength = pg_paint_curve(NULL);
  bs->curve_rand_uv = pg_paint_curve(NULL);
  ps->brush.gpencil_settings = bs;
  ps->brush.gpencil_tool = GPAINT_TOOL_DRAW;
  ps->brush.smooth_stroke_radius = (int)s->smooth_stroke_radius;
  ps->brush.smooth_stroke_factor = s->smooth_stroke_factor;
  ps->scene.toolsettings = &ps->ts; /* gp_sculpt.guide.use_guide = 0 */
  ps->px_per_unit = s->px_per_unit;
  ps->region.regiondata = &ps->px_per_unit;

  tGPsdata *p = &ps->p;
  p->scene = &ps->scene;
  p->region = &ps->region;
  p->gpd = &ps->gpd;
  p->brush = &ps->brush;
  p->paintmode = GP_PAINTMODE_DRAW;
  p->status = GP_STATUS_PAINTING;
  p->disable_stabilizer = s->disable_stabilizer != 0;
  p->manhattan_px = s->manhattan_threshold;
  p->euclidean_px = (int)s->euclidean_threshold;
  /* gpencil_session_initdata(): p->rng = BLI_rng_new(rng_seed) */
  p->rng = BLI_rng_new(rng_seed);
  /* gpencil_paint_initstroke(): sets GP_PAINTFLAG_FIRSTRUN */
  p->flags |= GP_PAINTFLAG_FIRSTRUN;
  ps->gpd.runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      ps->gpd.runtime.sbuffer, &ps->gpd.runtime.sbuffer_size, &ps->gpd.runtime.sbuffer_used, true);
  ps->gpd.runtime.sbuffer_sflag = 0;
  pg_paint_random_clock = random_clock;
  return ps;
}

void pg_paint_session_free(PGPaintSession *ps)
{
  if (!ps) {
    return;
  }
  BrushGpencilSettings *bs = &ps->settings;
  BKE_curvemapping_free(bs->curve_sensitivity);
  BKE_curvemapping_free(bs->curve_strength);
  BKE_curvemapping_free(bs->curve_jitter);
  BKE_curvemapping_free(bs->curve_rand_pressure);
  BKE_curvemapping_free(bs->curve_rand_strength);
  BKE_curvemapping_free(bs->curve_rand_uv);
  BLI_rng_free(ps->p.rng);
  MEM_SAFE_FREE(ps->gpd.runtime.sbuffer);
  MEM_freeN(ps);
}

/* One touch sample in region pixels: gpencil_draw_modal() (MOUSEMOVE while GP_STATUS_PAINTING,
 * gpencil_paint.c:3838-3859) -> gpencil_draw_apply_event() (2901-3080, the parts without
 * guides, straight lines, eraser or the Windows tablet first-touch hack). */
void pg_paint_event(PGPaintSession *ps, float x, float y, float pressure, double time)
{
  tGPsdata *p = &ps->p;
  wmEvent ev;
  ev.mval[0] = (int)x;
  ev.mval[1] = (int)y;
  const wmEvent *event = &ev;

  /* gpencil_draw_invoke() on the first event: ED_gpencil_init_random_settings(brush, event->mval) */
  if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
    ED_gpencil_init_random_settings(p->brush, event->mval, &p->random_settings);
  }

  int size_before = p->gpd->runtime.sbuffer_used;
  if (((p->flags & GP_PAINTFLAG_FIRSTRUN) == 0) && (p->paintmode != GP_PAINTMODE_ERASER)) {
    gpencil_add_fake_points(event, p);
  }

  /* gpencil_draw_apply_event(); the event position is the float touch position (as the exec
   * path's "mouse" property) instead of the integer event->mval */
  p->mval[0] = x;
  p->mval[1] = y;
  p->shift = false;
  p->curtime = time;
  p->pressure = pressure;
  p->random_settings.pen_press = pow(p->pressure, 3.0f);
  if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
    p->flags &= ~GP_PAINTFLAG_FIRSTRUN;
    p->opressure = p->pressure;
    p->inittime = p->ocurtime = p->curtime;
    p->straight = 0;
    copy_v2_v2(p->mvalo, p->mval);
    copy_v2_v2(p->mvali, p->mval);
  }
  gpencil_draw_apply(p->C, NULL, p, p->depsgraph);

  int size_after = p->gpd->runtime.sbuffer_used;
  /* Smooth segments if some fake points were added (need loop to get cumulative smooth).
   * the 0.15 value gets a good result in Windows and Linux. */
  if (size_after - size_before > 1) {
    for (int r = 0; r < 5; r++) {
      gpencil_smooth_segment(p->gpd, 0.15f, size_before - 1, size_after - 1);
    }
  }
}

/* gpencil_stroke_newfrombuffer() up to the point copy (gpencil_paint.c:1009-1022): for very low
 * pressure at the end, truncate the stroke. */
void pg_paint_truncate(PGPaintSession *ps)
{
  tGPsdata *p = &ps->p;
  bGPdata *gpd = p->gpd;
  tGPspoint *ptc;
/* BEGIN VERBATIM source/blender/editors/gpencil_legacy/gpencil_paint.c */
  /* For very low pressure at the end, truncate stroke. */
  if (p->paintmode == GP_PAINTMODE_DRAW) {
    int last_i = gpd->runtime.sbuffer_used - 1;
    while (last_i > 0) {
      ptc = (tGPspoint *)gpd->runtime.sbuffer + last_i;
      if (ptc->pressure > 0.001f) {
        break;
      }
      gpd->runtime.sbuffer_used = last_i - 1;
      CLAMP_MIN(gpd->runtime.sbuffer_used, 1);

      last_i--;
    }
  }
/* END VERBATIM */
}

const tGPspoint *pg_paint_buffer(const PGPaintSession *ps, int *r_used)
{
  *r_used = ps->gpd.runtime.sbuffer_used;
  return (const tGPspoint *)ps->gpd.runtime.sbuffer;
}
