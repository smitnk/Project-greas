/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_draw_input.h. The stroke points come from Blender's code in
 * project_grease_blender_paint.c; this file only maps canvas units to region pixels and back. */
#include "project_grease_draw_input.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "DNA_gpencil_legacy_types.h"
#include "ED_gpencil_legacy.h"

#include "project_grease_blender_paint.h"

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
  s->px_per_unit = 1.0f;
}

/* blender::RandomNumberGenerator::seed() (BLI_rand.hh), used by BLI_rng_new(). */
void pg_draw_rng_seed(unsigned long long *x, unsigned int seed)
{
  const uint64_t lowseed = 0x330E;
  *x = ((uint64_t)seed << 16) | lowseed;
}

/* RandomNumberGenerator::step() + get_int32() + get_float(), used by BLI_rng_get_float(). */
float pg_draw_rng_get_float(unsigned long long *x)
{
  const uint64_t multiplier = 0x5DEECE66Dll;
  const uint64_t addend = 0xB;
  const uint64_t mask = 0x0000FFFFFFFFFFFFll;
  *x = (multiplier * (uint64_t)*x + addend) & mask;
  return (float)(int32_t)(*x >> 17) / 0x80000000;
}

static float ppu(const PGDrawSettings *s)
{
  return (s->px_per_unit > 0.0f && isfinite(s->px_per_unit)) ? s->px_per_unit : 1.0f;
}

void pg_draw_input_begin(PGDrawInput *d, const PGDrawSettings *s)
{
  pg_paint_session_free(d->paint);
  PGDrawPoint *keep = d->buffer;
  const int cap = d->capacity;
  memset(d, 0, sizeof(*d));
  d->buffer = keep;
  d->capacity = cap;
  if (s) d->settings = *s;
  else pg_draw_settings_default(&d->settings);
  d->settings.px_per_unit = ppu(&d->settings);
  /* gpencil_session_initdata(): rng_seed = PIL_check_seconds_timer_i() & UINT_MAX, then
   * rng_seed ^= POINTER_AS_UINT(p). ED_gpencil_init_random_settings() reads the timer too. */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned int rng_seed = (unsigned int)((long int)tv.tv_sec & UINT32_MAX);
  rng_seed ^= (unsigned int)(uintptr_t)d;
  double clock = (double)tv.tv_sec + tv.tv_usec * 1e-6;
  if (d->settings.use_seed) {
    rng_seed = d->settings.seed;
    clock = (double)d->settings.seed;
  }
  d->paint = pg_paint_session_begin(&d->settings, rng_seed, clock);
  d->active = d->paint != NULL;
}

void pg_draw_input_cancel(PGDrawInput *d)
{
  pg_paint_session_free(d->paint);
  d->paint = NULL;
  d->active = 0;
  d->used = 0;
  d->released = 0;
}

void pg_draw_input_free(PGDrawInput *d)
{
  pg_draw_input_cancel(d);
  free(d->buffer);
  d->buffer = NULL;
  d->capacity = 0;
}

/* Converts sbuffer[from, used) to canvas points. */
static int sync(PGDrawInput *d, int from)
{
  int used;
  const tGPspoint *sb = pg_paint_buffer(d->paint, &used);
  if (used > d->capacity) {
    int cap = d->capacity ? d->capacity : 64;
    while (cap < used) cap *= 2;
    PGDrawPoint *b = realloc(d->buffer, sizeof(PGDrawPoint) * (size_t)cap);
    if (!b) return 0;
    d->buffer = b;
    d->capacity = cap;
  }
  const float k = d->settings.px_per_unit;
  const float smin = fminf(PG_DRAW_STRENGTH_MIN, d->settings.draw_strength);
  for (int i = from < 0 ? 0 : from; i < used; i++) {
    PGDrawPoint *o = &d->buffer[i];
    o->x = sb[i].m_xy[0] / k;
    o->y = -sb[i].m_xy[1] / k;
    o->pressure = sb[i].pressure;
    /* gpencil_stroke_newfrombuffer(): CLAMP(pt->strength, MIN2(GPENCIL_STRENGTH_MIN, draw_strength), 1) */
    o->strength = fminf(fmaxf(sb[i].strength, smin), 1.0f);
    o->time = sb[i].time;
  }
  d->used = used;
  return 1;
}

int pg_draw_input_add(PGDrawInput *d, float x, float y, float pressure, float time_seconds,
                      const PGDrawPoint **r_out)
{
  *r_out = d->buffer;
  if (!d->active || !isfinite(x) || !isfinite(y)) return 0;
  const float k = d->settings.px_per_unit;
  pg_paint_event(d->paint, x * k, -y * k, isfinite(pressure) ? pressure : 1.0f, (double)time_seconds);
  if (!sync(d, d->released)) return 0;
  /* Later events rewrite at most the last PG_DRAW_SMOOTH_HOLD_BACK points; the end truncation
   * removes a trailing run of pressure <= 0.001 and the point before it. */
  int limit = d->used - PG_DRAW_SMOOTH_HOLD_BACK;
  int run = d->used;
  while (run > 0 && d->buffer[run - 1].pressure <= 0.001f) run--;
  if (run < d->used && run - 1 < limit) limit = run - 1;
  if (limit <= d->released) return 0;
  *r_out = d->buffer + d->released;
  const int n = limit - d->released;
  d->released = limit;
  return n;
}

int pg_draw_input_end(PGDrawInput *d, const PGDrawPoint **r_out)
{
  *r_out = d->buffer;
  if (!d->active) return 0;
  pg_paint_truncate(d->paint);
  sync(d, d->released);
  d->active = 0;
  pg_paint_session_free(d->paint);
  d->paint = NULL;
  const int n = d->used > d->released ? d->used - d->released : 0;
  *r_out = d->buffer + d->released;
  d->released += n;
  return n;
}
