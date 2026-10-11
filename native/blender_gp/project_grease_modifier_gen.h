/* SPDX-License-Identifier: GPL-2.0-or-later
 * Mirror / Array / Multiple Strokes / Dot Dash as MOD_gpencil_legacy_*.c (project_grease_modifier_gen.c). */
#pragma once
#include "project_grease_modifier_stack.h"
#ifdef __cplusplus
extern "C" {
#endif
/* generateStrokes of a Mirror, Array, Multiply or Dash entry on ctx->gpf (canvas px in and out). */
int pg_mod_gen_run(const PGModContext *ctx, const PGModEntry *e);
#ifdef __cplusplus
}
#endif
