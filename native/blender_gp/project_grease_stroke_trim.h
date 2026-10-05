/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bGPdata;
struct bGPDstroke;

/**
 * Trims a stroke to its first self-intersection, like BKE_gpencil_stroke_trim(), and also when the
 * crossing lies exactly on one of the stroke's own points (see project_grease_stroke_trim.c).
 * Returns false when the stroke does not cross itself.
 */
bool pg_gpencil_stroke_trim(struct bGPdata *gpd, struct bGPDstroke *gps);

#ifdef __cplusplus
}
#endif
