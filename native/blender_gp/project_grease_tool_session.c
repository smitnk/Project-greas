/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_tool_session.h. */
#include "project_grease_tool_session.h"
#include "project_grease_blender_mod2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "project_grease_draw_input.h"
#include "project_grease_tool_brushes.h"

struct PGToolSession {
  int tool;       /* open gesture's tool, -1 when none */
  int changed;    /* the open gesture changed the document */
  int samples;    /* samples applied in the current / last gesture */
  PGDrawInput draw;
  PGToolDrawSink sink;
  int draw_open;
  /* drawing guide of the open Draw gesture: type -1 = off; start = first sample */
  int guide_type;
  float guide_cx, guide_cy, guide_angle, guide_spacing;
  int guide_started;
  float guide_sx, guide_sy;
  PGSculptSession *sculpt;
  PGVertexPaintSession *vpaint;
  PGWeightPaintSession *wpaint;
};

PGToolSession *pg_tool_session_new(void)
{
  PGToolSession *s = calloc(1, sizeof(PGToolSession));
  if (s) s->tool = -1;
  return s;
}

static float param(const float *p, int n, int i, float fallback)
{
  return (p != NULL && i < n && isfinite(p[i])) ? p[i] : fallback;
}

static int draw_send(PGToolSession *s, const PGDrawPoint *pts, int n)
{
  int ok = 1;
  for (int i = 0; i < n; i++) {
    if (s->sink.add_point) {
      ok &= s->sink.add_point(s->sink.user, pts[i].x, pts[i].y, pts[i].pressure, pts[i].strength,
                              pts[i].time) != 0;
    }
  }
  return ok;
}

static void close_gesture(PGToolSession *s, int cancel)
{
  switch (s->tool) {
    case PG_TOOL_DRAW:
      if (s->draw_open) {
        if (cancel) {
          pg_draw_input_cancel(&s->draw);
          if (s->sink.cancel) s->sink.cancel(s->sink.user);
        }
        else {
          const PGDrawPoint *out;
          const int n = pg_draw_input_end(&s->draw, &out);
          draw_send(s, out, n);
          if (s->sink.end && s->sink.end(s->sink.user)) s->changed = 1;
        }
      }
      s->draw_open = 0;
      break;
    case PG_TOOL_SCULPT:
      pg_sculpt_session_end(s->sculpt);
      s->sculpt = NULL;
      break;
    case PG_TOOL_VERTEX_PAINT:
      pg_vpaint_session_end(s->vpaint);
      s->vpaint = NULL;
      break;
    case PG_TOOL_WEIGHT_PAINT:
      pg_wpaint_session_end(s->wpaint);
      s->wpaint = NULL;
      break;
    default:
      break;
  }
  s->tool = -1;
}

void pg_tool_session_free(PGToolSession *s)
{
  if (!s) return;
  close_gesture(s, 1);
  pg_draw_input_free(&s->draw);
  free(s);
}

int pg_tool_session_sample_count(const PGToolSession *s) { return s ? s->samples : 0; }

static int open_gesture(PGToolSession *s, struct bGPdata *gpd, int tool, const float *p, int n,
                        const PGToolDrawSink *sink)
{
  s->changed = 0;
  s->samples = 0;
  if (tool == PG_TOOL_DRAW) {
    if (!sink || !sink->begin) return 0;
    s->sink = *sink;
    PGDrawSettings ds;
    pg_draw_settings_default(&ds);
    ds.draw_strength = param(p, n, PG_DRAW_P_STRENGTH, ds.draw_strength);
    ds.use_pressure = param(p, n, PG_DRAW_P_USE_PRESSURE, 1) != 0.0f;
    ds.use_strength_pressure = param(p, n, PG_DRAW_P_USE_STRENGTH_PRESSURE, 0) != 0.0f;
    ds.pressure_curve = param(p, n, PG_DRAW_P_PRESSURE_CURVE, 1);
    ds.strength_curve = param(p, n, PG_DRAW_P_STRENGTH_CURVE, 1);
    ds.active_smooth = param(p, n, PG_DRAW_P_ACTIVE_SMOOTH, 0.35f); /* ACTIVE_SMOOTH, Pencil preset */
    ds.input_samples = (int)param(p, n, PG_DRAW_P_INPUT_SAMPLES, 0);
    ds.lazy_enabled = param(p, n, PG_DRAW_P_LAZY, 0) != 0.0f;
    ds.smooth_stroke_radius = param(p, n, PG_DRAW_P_LAZY_RADIUS, 0);
    ds.smooth_stroke_factor = param(p, n, PG_DRAW_P_LAZY_FACTOR, 0);
    ds.disable_stabilizer = param(p, n, PG_DRAW_P_DISABLE_STABILIZER, 0) != 0.0f;
    ds.manhattan_threshold = (int)param(p, n, PG_DRAW_P_MANHATTAN, 1);
    ds.euclidean_threshold = param(p, n, PG_DRAW_P_EUCLIDEAN, 1);
    ds.jitter = param(p, n, PG_DRAW_P_JITTER, 0);
    ds.draw_angle_factor = param(p, n, PG_DRAW_P_ANGLE_FACTOR, 0);
    ds.draw_angle = param(p, n, PG_DRAW_P_ANGLE, 0);
    ds.synthesize_fast_points = param(p, n, PG_DRAW_P_FAKE_POINTS, 1) != 0.0f;
    ds.px_per_unit = param(p, n, PG_DRAW_P_PX_PER_UNIT, 1);
    {
      const float seed1 = param(p, n, PG_DRAW_P_SEED, 0);
      if (seed1 >= 1.0f && seed1 <= 4294967296.0f) {
        ds.use_seed = 1;
        ds.seed = (unsigned int)((double)seed1 - 1.0);
      }
    }
    const int pn = (int)param(p, n, PG_DRAW_P_PRESSURE_CURVE_N, 0);
    if (pn >= 2 && pn <= 8 && n >= PG_DRAW_P_PRESSURE_CURVE_XY + 2 * pn) {
      pg_curve_set(&ds.pressure_map, &p[PG_DRAW_P_PRESSURE_CURVE_XY], pn);
    }
    const int sn = (int)param(p, n, PG_DRAW_P_STRENGTH_CURVE_N, 0);
    if (sn >= 2 && sn <= 8 && n >= PG_DRAW_P_STRENGTH_CURVE_XY + 2 * sn) {
      pg_curve_set(&ds.strength_map, &p[PG_DRAW_P_STRENGTH_CURVE_XY], sn);
    }
    s->guide_type = (int)param(p, n, PG_DRAW_P_GUIDE_TYPE, 0) - 1;
    s->guide_cx = param(p, n, PG_DRAW_P_GUIDE_CX, 0);
    s->guide_cy = param(p, n, PG_DRAW_P_GUIDE_CY, 0);
    s->guide_angle = param(p, n, PG_DRAW_P_GUIDE_ANGLE, 0);
    s->guide_spacing = param(p, n, PG_DRAW_P_GUIDE_SPACING, 0);
    s->guide_started = 0;
    if (!s->sink.begin(s->sink.user, (int)param(p, n, PG_DRAW_P_MATERIAL, 0),
                       param(p, n, PG_DRAW_P_THICKNESS, 3)))
    {
      return 0;
    }
    pg_draw_input_begin(&s->draw, &ds);
    s->draw_open = 1;
    s->tool = tool;
    return 1;
  }
  PGToolBrushParams bp;
  memset(&bp, 0, sizeof bp);
  bp.brush = (int)param(p, n, PG_TOOL_P_BRUSH, 0);
  bp.radius = param(p, n, PG_TOOL_P_RADIUS, 25);
  bp.strength = param(p, n, PG_TOOL_P_STRENGTH, 1);
  bp.px_per_unit = param(p, n, PG_TOOL_P_PX_PER_UNIT, 1);
  bp.invert = param(p, n, PG_TOOL_P_INVERT, 0) != 0.0f;
  bp.rgb[0] = param(p, n, PG_TOOL_P_R, 0);
  bp.rgb[1] = param(p, n, PG_TOOL_P_G, 0);
  bp.rgb[2] = param(p, n, PG_TOOL_P_B, 0);
  bp.target = (int)param(p, n, PG_TOOL_P_TARGET, 0);
  bp.weight = param(p, n, PG_TOOL_P_WEIGHT, 1);
  bp.seed = (unsigned int)param(p, n, PG_TOOL_P_SEED, 0);
  bp.automask = (int)param(p, n, PG_TOOL_P_AUTOMASK, 0);
  bp.select_mask = (int)param(p, n, PG_TOOL_P_SELECT_MASK, 0);
  bp.curve_preset = (int)param(p, n, PG_TOOL_P_CURVE_PRESET, 0);
  bp.active_material = (int)param(p, n, PG_TOOL_P_ACTIVE_MATERIAL, 0);
  switch (tool) {
    case PG_TOOL_SCULPT:
      s->sculpt = pg_sculpt_session_begin(gpd, &bp);
      if (!s->sculpt) return 0;
      break;
    case PG_TOOL_VERTEX_PAINT:
      s->vpaint = pg_vpaint_session_begin(gpd, &bp);
      if (!s->vpaint) return 0;
      break;
    case PG_TOOL_WEIGHT_PAINT:
      s->wpaint = pg_wpaint_session_begin(gpd, &bp);
      if (!s->wpaint) return 0;
      break;
    default:
      return 0;
  }
  s->tool = tool;
  return 1;
}

static int apply_sample(PGToolSession *s, const float *smp)
{
  const float x = smp[0], y = smp[1], pressure = smp[2], time = smp[3];
  if (!isfinite(x) || !isfinite(y)) return 0;
  s->samples++;
  switch (s->tool) {
    case PG_TOOL_DRAW: {
      const PGDrawPoint *out;
      float gx = x, gy = y;
      if (s->guide_type >= 0 && s->guide_type <= 4) {
        /* gpencil_snap_to_guide(): every input sample is constrained before the draw pipeline;
         * the first sample of the stroke is the guide origin for circular / radial / parallel. */
        if (!s->guide_started) { s->guide_sx = x; s->guide_sy = y; s->guide_started = 1; }
        float r[2];
        pg_guide_snap(s->guide_type, s->guide_cx, s->guide_cy, s->guide_angle, s->guide_spacing,
                      s->guide_sx, s->guide_sy, x, y, r);
        if (isfinite(r[0]) && isfinite(r[1])) { gx = r[0]; gy = r[1]; }
      }
      const int n = pg_draw_input_add(&s->draw, gx, gy, isfinite(pressure) ? pressure : 1.0f,
                                      isfinite(time) ? time : 0.0f, &out);
      return n > 0 ? draw_send(s, out, n) : 0;
    }
    case PG_TOOL_SCULPT:
      return pg_sculpt_session_sample(s->sculpt, x, y, pressure);
    case PG_TOOL_VERTEX_PAINT:
      return pg_vpaint_session_sample(s->vpaint, x, y, pressure);
    case PG_TOOL_WEIGHT_PAINT:
      return pg_wpaint_session_sample(s->wpaint, x, y, pressure);
    default:
      return 0;
  }
}

int pg_tool_session_samples(PGToolSession *s, struct bGPdata *gpd, int tool, const float *samples,
                            int count, int phase, const float *params, int n_params,
                            const PGToolDrawSink *sink)
{
  if (!s || count < 0 || (count > 0 && !samples)) return 0;
  int result = 0;
  if (phase == PG_TOOL_PHASE_CANCEL) {
    close_gesture(s, 1);
    return PG_TOOL_RESULT_OK | PG_TOOL_RESULT_CHANGED;
  }
  if (phase == PG_TOOL_PHASE_BEGIN) {
    if (s->tool >= 0) {
      close_gesture(s, 0);
    }
    if ((!gpd && tool != PG_TOOL_DRAW) || !open_gesture(s, gpd, tool, params, n_params, sink)) return 0;
  }
  else if (s->tool != tool) {
    return 0; /* no gesture of this tool is open */
  }
  result |= PG_TOOL_RESULT_OK;
  int changed = 0;
  for (int i = 0; i < count; i++) {
    changed |= apply_sample(s, samples + (size_t)i * PG_TOOL_SAMPLE_STRIDE);
  }
  if (changed) {
    s->changed = 1;
    result |= PG_TOOL_RESULT_CHANGED;
  }
  if (phase == PG_TOOL_PHASE_END) {
    close_gesture(s, 0);
    result |= PG_TOOL_RESULT_CHANGED;
    if (s->changed) result |= PG_TOOL_RESULT_ENDED;
  }
  return result;
}
