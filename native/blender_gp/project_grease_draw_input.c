/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_draw_input.h. Float operations follow the former Kotlin engine one to one
 * (Kotlin's Float math functions go through double: hypot, pow, acos, cos, sin). */
#include "project_grease_draw_input.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float fclamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float khypot(float a, float b) { return (float)hypot((double)a, (double)b); }
static float kcos(float a) { return (float)cos((double)a); }
static float ksin(float a) { return (float)sin((double)a); }
static float kacos(float a) { return (float)acos((double)a); }

float pg_draw_interpf(float target, float origin, float fac)
{
  return fac * target + (1.0f - fac) * origin;
}

void pg_draw_settings_default(PGDrawSettings *s)
{
  memset(s, 0, sizeof(*s));
  s->draw_strength = 1.0f;
  s->use_pressure = 1;
  s->pressure_curve = 1.0f;
  s->strength_curve = 1.0f;
  s->manhattan_threshold = 1;
  s->euclidean_threshold = 1.0f;
  s->synthesize_fast_points = 1;
}

static int ensure(PGDrawInput *d, int n)
{
  if (n <= d->capacity) return 1;
  int cap = d->capacity ? d->capacity : 64;
  while (cap < n) cap *= 2;
  PGDrawPoint *b = realloc(d->buffer, sizeof(PGDrawPoint) * (size_t)cap);
  if (!b) return 0;
  d->buffer = b;
  d->capacity = cap;
  return 1;
}

static void push(PGDrawInput *d, PGDrawPoint p)
{
  if (ensure(d, d->used + 1)) d->buffer[d->used++] = p;
}

void pg_draw_input_begin(PGDrawInput *d, const PGDrawSettings *s)
{
  PGDrawPoint *keep = d->buffer;
  const int cap = d->capacity;
  memset(d, 0, sizeof(*d));
  d->buffer = keep;
  d->capacity = cap;
  if (s) d->settings = *s;
  else pg_draw_settings_default(&d->settings);
  d->active = 1;
  d->rng = 0x6D2B79F5u;
}

void pg_draw_input_cancel(PGDrawInput *d)
{
  d->active = 0;
  d->used = 0;
  d->released = 0;
}

void pg_draw_input_free(PGDrawInput *d)
{
  free(d->buffer);
  memset(d, 0, sizeof(*d));
}

static int hold_back(const PGDrawInput *d)
{
  if (d->settings.input_samples > 0 || d->settings.active_smooth > 0.0f) return PG_DRAW_SMOOTH_HOLD_BACK;
  if (d->settings.draw_angle_factor != 0.0f) return 1;
  return 0;
}

static int release(PGDrawInput *d, const PGDrawPoint **r_out)
{
  int end = d->used - hold_back(d);
  if (end < d->released) end = d->released;
  const int n = end - d->released;
  *r_out = d->buffer + d->released;
  d->released = end;
  return n > 0 ? n : 0;
}

/* Returns a value in [-1, 1] (BLI_rng_get_float() * 2 - 1). */
static float next_random(PGDrawInput *d)
{
  d->rng = d->rng * 1664525u + 1013904223u;
  return (float)((d->rng >> 8) & 0x00FFFFFFu) / 16777215.0f * 2.0f - 1.0f;
}

/* gpencil_stroke_filtermval() with the lazy-mouse and Manhattan/Euclidean tests. */
static int filter_mval(const PGDrawInput *d, float x, float y)
{
  const int dx = (int)fabsf(x - d->last_input_x);
  const int dy = (int)fabsf(y - d->last_input_y);
  const PGDrawSettings *s = &d->settings;
  if (s->lazy_enabled && !s->disable_stabilizer) {
    return (float)(dx * dx + dy * dy) > s->smooth_stroke_radius * s->smooth_stroke_radius;
  }
  if (dx > s->manhattan_threshold && dy > s->manhattan_threshold) return 1;
  const float euclid = s->euclidean_threshold < 0.0f ? 0.0f : s->euclidean_threshold;
  return (float)(dx * dx + dy * dy) > euclid * euclid;
}

/* gpencil_smooth_buffer(): smooths point C (idx - 2) using A, B and D. */
static void smooth_buffer(PGDrawInput *d, float inf, int idx)
{
  if (d->used < 3 || idx < 3 || inf == 0.0f) return;
  const float steps = (idx < 4) ? 3.0f : 4.0f;
  const float average_fac = 1.0f / steps;
  const PGDrawPoint *pts[4] = {idx >= 4 ? &d->buffer[idx - 4] : NULL, &d->buffer[idx - 3],
                               &d->buffer[idx - 2], &d->buffer[idx - 1]};
  float sx = 0, sy = 0, pressure = 0, strength = 0;
  for (int k = 0; k < 4; k++) {
    if (!pts[k]) continue;
    sx += pts[k]->x * average_fac;
    sy += pts[k]->y * average_fac;
    pressure += pts[k]->pressure * average_fac;
    strength += pts[k]->strength * average_fac;
  }
  PGDrawPoint c = d->buffer[idx - 2];
  c.x = c.x + (sx - c.x) * inf;
  c.y = c.y + (sy - c.y) * inf;
  c.pressure = pg_draw_interpf(d->buffer[idx - 2].pressure, pressure, inf);
  c.strength = pg_draw_interpf(d->buffer[idx - 2].strength, strength, inf);
  d->buffer[idx - 2] = c;
}

/* gpencil_smooth_segment(). */
static void smooth_segment(PGDrawInput *d, float inf, int from, int to)
{
  if (to - from < 3 || inf == 0.0f) return;
  if (from <= 2) return;
  const float average_fac = 0.25f;
  for (int i = from; i <= to; i++) {
    const PGDrawPoint ptc = (i >= 1) ? d->buffer[i - 1] : d->buffer[i];
    const PGDrawPoint pta = (i >= 3) ? d->buffer[i - 3] : ptc;
    const PGDrawPoint ptb = (i >= 2) ? d->buffer[i - 2] : ptc;
    const PGDrawPoint ptd = d->buffer[i];
    const PGDrawPoint *pts[4] = {&pta, &ptb, &ptc, &ptd};
    float sx = 0, sy = 0, pressure = 0, strength = 0;
    for (int k = 0; k < 4; k++) {
      sx += pts[k]->x * average_fac;
      sy += pts[k]->y * average_fac;
      pressure += pts[k]->pressure * average_fac;
      strength += pts[k]->strength * average_fac;
    }
    const int target = (i >= 1) ? i - 1 : i;
    PGDrawPoint out = ptc;
    out.x = ptc.x + (sx - ptc.x) * inf;
    out.y = ptc.y + (sy - ptc.y) * inf;
    out.pressure = pg_draw_interpf(ptc.pressure, pressure, inf);
    out.strength = pg_draw_interpf(ptc.strength, strength, inf);
    d->buffer[target] = out;
  }
}

static float angle_between(float ax, float ay, float bx, float by)
{
  const float al = khypot(ax, ay), bl = khypot(bx, by);
  if (al <= 1e-6f || bl <= 1e-6f) return 0.0f;
  const float cosine = fclamp((ax * bx + ay * by) / (al * bl), -1.0f, 1.0f);
  return kacos(cosine);
}

/* gpencil_stroke_addpoint() for GP_PAINTMODE_DRAW. */
static void append_point(PGDrawInput *d, float x, float y, float input_pressure, float absolute_time,
                         float mouse_x, float mouse_y)
{
  const PGDrawSettings *s = &d->settings;
  const float pressure01 = fclamp(input_pressure, 0.0f, 1.0f);
  float pressure = 1.0f;
  /* gpencil_stroke_addpoint(): pressure *= BKE_curvemapping_evaluateF(brush curve_sensitivity) */
  if (s->use_pressure) {
    if (s->pressure_map.built) {
      pressure *= pg_curve_evaluate(&s->pressure_map, pressure01);
    }
    else {
      const double c = s->pressure_curve < 0.01f ? 0.01f : s->pressure_curve;
      pressure *= (float)pow((double)pressure01, c);
    }
  }
  float strength = s->draw_strength;
  if (s->use_strength_pressure) {
    if (s->strength_map.built) {
      strength *= pg_curve_evaluate(&s->strength_map, pressure01);
    }
    else {
      const double c = s->strength_curve < 0.01f ? 0.01f : s->strength_curve;
      strength *= (float)pow((double)pressure01, c);
    }
  }
  strength = fclamp(strength, fminf(PG_DRAW_STRENGTH_MIN, s->draw_strength), 1.0f);

  float px = x, py = y;
  const int used = d->used;
  /* gpencil_brush_jitter(): perpendicular displacement, needs two buffered points. */
  if (s->jitter > 0.0f && used > 1) {
    const PGDrawPoint *previous = &d->buffer[used - 1];
    const float vx = px - previous->x, vy = py - previous->y;
    const float length = khypot(vx, vy);
    if (length > 1e-6f) {
      const float mx = vx / length, my = vy / length;
      const float angle = kacos(fclamp(my, -1.0f, 1.0f));
      const float exponent = s->jitter + 2.0f;
      const float amplitude = next_random(d) * exponent * exponent;
      px += mx * kcos(angle) * amplitude * 10.0f;
      py += my * ksin(angle) * amplitude * 10.0f;
    }
  }
  /* gpencil_brush_angle(): uses the raw mouse position. */
  if (s->draw_angle_factor != 0.0f && used >= 1) {
    const float sen = s->draw_angle_factor;
    const float v0x = kcos(s->draw_angle), v0y = ksin(s->draw_angle);
    const int prev_index = used - 1;
    PGDrawPoint previous = d->buffer[prev_index];
    const float mvx = mouse_x - previous.x, mvy = mouse_y - previous.y;
    const float length = khypot(mvx, mvy);
    const float nx = length > 1e-6f ? mvx / length : 0.0f;
    const float ny = length > 1e-6f ? mvy / length : 0.0f;
    if (used == 1) {
      /* "uses > 1.0f to get a smooth transition in first point" */
      const float fac = 1.4f - fabsf(v0x * nx + v0y * ny);
      previous.pressure = fclamp(previous.pressure - sen * fac, PG_DRAW_ALPHA_OPACITY_THRESH, 1.0f);
      d->buffer[prev_index] = previous;
    }
    const float fac = 1.0f - fabsf(v0x * nx + v0y * ny);
    pressure = fclamp(pg_draw_interpf(pressure - sen * fac, previous.pressure, 0.3f),
                      PG_DRAW_ALPHA_OPACITY_THRESH, 1.0f);
  }
  float t = absolute_time - d->initial_time;
  PGDrawPoint p = {px, py, pressure, strength, t < 0.0f ? 0.0f : t};
  push(d, p);
  /* "Smooth while drawing previous points with a reduction factor for previous." */
  if (s->active_smooth > 0.0f) {
    for (int k = 0; k < 3; k++) {
      smooth_buffer(d, s->active_smooth * ((3.0f - (float)k) / 3.0f), d->used - k);
    }
  }
}

/* gpencil_add_arc_points(): the first arc point REPLACES the last buffered point. */
static void add_arc_points(PGDrawInput *d, float mouse_x, float mouse_y, int segments, float time_seconds)
{
  if (d->used < 3) return;
  const PGDrawPoint pt_before = d->buffer[d->used - 1];
  const PGDrawPoint pt_prev = d->buffer[d->used - 2];
  float v_prev_x = pt_prev.x - pt_before.x, v_prev_y = pt_prev.y - pt_before.y;
  const float v_half_x = (pt_prev.x + mouse_x) * 0.5f - pt_prev.x;
  const float v_half_y = (pt_prev.y + mouse_y) * 0.5f - pt_prev.y;
  const float angle = angle_between(v_prev_x, v_prev_y, v_half_x, v_half_y);
  if (angle < (float)(120.0 * M_PI / 180.0)) return;
  const float dot = v_prev_x * v_half_x + v_prev_y * v_half_y;
  const float length_sq = v_prev_x * v_prev_x + v_prev_y * v_prev_y;
  if (length_sq > 0.0f) {
    v_prev_x *= dot / length_sq;
    v_prev_y *= dot / length_sq;
  }
  const float ctl_x = pt_prev.x + v_prev_x, ctl_y = pt_prev.y + v_prev_y;
  const float step = (float)(M_PI / 2.0) / (float)(segments + 1);
  float a = step;
  const float midpoint_x = (pt_prev.x + mouse_x) * 0.5f, midpoint_y = (pt_prev.y + mouse_y) * 0.5f;
  const float corner_x = midpoint_x - (ctl_x - midpoint_x);
  const float corner_y = midpoint_y - (ctl_y - midpoint_y);
  const PGDrawSettings *s = &d->settings;
  PGDrawPoint *arc = malloc(sizeof(PGDrawPoint) * (size_t)segments);
  if (!arc) return;
  PGDrawPoint pt_step = pt_prev;
  for (int i = 0; i < segments; i++) {
    const float x = corner_x + (mouse_x - corner_x) * ksin(a) + (pt_prev.x - corner_x) * kcos(a);
    const float y = corner_y + (mouse_y - corner_y) * ksin(a) + (pt_prev.y - corner_y) * kcos(a);
    float pressure = pt_prev.pressure;
    if (s->draw_angle_factor != 0.0f) {
      const float sen = s->draw_angle_factor;
      const float v0x = kcos(s->draw_angle), v0y = ksin(s->draw_angle);
      const float mvx = x - pt_step.x, mvy = y - pt_step.y;
      const float length = khypot(mvx, mvy);
      const float nx = length > 1e-6f ? mvx / length : 0.0f;
      const float ny = length > 1e-6f ? mvy / length : 0.0f;
      const float fac = 1.0f - fabsf(v0x * nx + v0y * ny);
      pressure = fclamp(pg_draw_interpf(pressure - sen * fac, pt_step.pressure, 0.3f),
                        PG_DRAW_ALPHA_OPACITY_THRESH, 1.0f);
      pressure = fclamp(pressure, pt_prev.pressure * 0.5f, 1.0f);
    }
    const float t = (float)(i + 1) / (float)segments;
    const float time = d->previous_time + (time_seconds - d->previous_time) * t;
    const float rel = time - d->initial_time;
    PGDrawPoint point = {x, y, pressure, pt_prev.strength, rel < 0.0f ? 0.0f : rel};
    arc[i] = point;
    if (s->draw_angle_factor != 0.0f) pt_step = point;
    a += step;
  }
  d->used--;
  for (int i = 0; i < segments; i++) push(d, arc[i]);
  free(arc);
}

/* gpencil_add_fake_points() (no guides). */
static void add_fake_points(PGDrawInput *d, float mouse_x, float mouse_y, float time_seconds)
{
  const PGDrawSettings *s = &d->settings;
  if (s->lazy_enabled && !s->disable_stabilizer) return;
  int input_samples = s->input_samples;
  if (input_samples < 0) input_samples = 0;
  if (input_samples > PG_DRAW_MAX_INPUT_SAMPLES) input_samples = PG_DRAW_MAX_INPUT_SAMPLES;
  if (input_samples == 0) return;
  const int samples = PG_DRAW_MAX_INPUT_SAMPLES - input_samples + 1;
  const float min_dist = 4.0f * (float)samples;
  const float dist = khypot(mouse_x - d->last_input_x, mouse_y - d->last_input_y);
  if (dist > 3.0f && dist > min_dist) {
    const int slices = (int)(dist / min_dist) + 1;
    add_arc_points(d, mouse_x, mouse_y, slices, time_seconds);
  }
}

int pg_draw_input_add(PGDrawInput *d, float x, float y, float pressure, float time_seconds,
                      const PGDrawPoint **r_out)
{
  *r_out = d->buffer;
  if (!d->active) return 0;
  if (d->used == 0) {
    d->initial_time = time_seconds;
    d->last_input_x = x;
    d->last_input_y = y;
    d->previous_time = time_seconds;
    append_point(d, x, y, pressure, time_seconds, x, y);
    return release(d, r_out);
  }
  if (!filter_mval(d, x, y)) return 0;
  const float mouse_x = x, mouse_y = y;
  const int size_before = d->used;
  /* gpencil_draw_modal(): fake points are added BEFORE gpencil_draw_apply_event(). */
  if (d->settings.synthesize_fast_points) add_fake_points(d, mouse_x, mouse_y, time_seconds);
  /* gpencil_draw_apply(): lazy mouse interpolates the current and the last position. */
  float px = mouse_x, py = mouse_y;
  if (d->settings.lazy_enabled && !d->settings.disable_stabilizer) {
    const float factor = d->settings.smooth_stroke_factor;
    px = mouse_x + (d->last_input_x - mouse_x) * factor;
    py = mouse_y + (d->last_input_y - mouse_y) * factor;
  }
  append_point(d, px, py, pressure, time_seconds, mouse_x, mouse_y);
  d->last_input_x = px;
  d->last_input_y = py;
  d->previous_time = time_seconds;
  /* gpencil_draw_modal(): smooth the segment when fake points were added. */
  const int size_after = d->used;
  if (size_after - size_before > 1) {
    for (int k = 0; k < 5; k++) smooth_segment(d, 0.15f, size_before - 1, size_after - 1);
  }
  return release(d, r_out);
}

int pg_draw_input_end(PGDrawInput *d, const PGDrawPoint **r_out)
{
  *r_out = d->buffer;
  if (!d->active) return 0;
  d->active = 0;
  /* gpencil_stroke_newfrombuffer(): "For very low pressure at the end, truncate stroke." */
  int last_index = d->used - 1;
  int used = d->used;
  while (last_index > 0) {
    if (d->buffer[last_index].pressure > PG_DRAW_ALPHA_OPACITY_THRESH) break;
    used = (last_index - 1) > 1 ? (last_index - 1) : 1;
    last_index--;
  }
  const int final_size = used > d->released ? used : d->released;
  if (d->used > final_size) d->used = final_size;
  *r_out = d->buffer + d->released;
  const int n = d->used - d->released;
  d->released = d->used;
  return n > 0 ? n : 0;
}
