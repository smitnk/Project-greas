/* SPDX-License-Identifier: GPL-2.0-or-later
 * Blender 3.6.23 gpencil_paint.c draw-stroke input, see project_grease_blender_paint.c. Positions are
 * region pixels (y up); project_grease_draw_input.c converts from / to canvas units. */
#pragma once
#include "project_grease_draw_input.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tGPspoint;
typedef struct PGPaintSession PGPaintSession;

/* Brush, paint session and an empty sbuffer; rng_seed / random_clock replace
 * gpencil_session_initdata()'s timer seed and ED_gpencil_init_random_settings()'s timer. */
PGPaintSession *pg_paint_session_begin(const PGDrawSettings *s, unsigned int rng_seed,
                                       double random_clock);
void pg_paint_session_free(PGPaintSession *ps);
/* One mouse-move event of gpencil_draw_modal(). */
void pg_paint_event(PGPaintSession *ps, float x, float y, float pressure, double time);
/* The trailing low-pressure truncation of gpencil_stroke_newfrombuffer(). */
void pg_paint_truncate(PGPaintSession *ps);
const struct tGPspoint *pg_paint_buffer(const PGPaintSession *ps, int *r_used);

#ifdef __cplusplus
}
#endif
