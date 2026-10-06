/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Includes for the generated shadow stage (project_grease_lineart_shadow.c, C). The pinned
 * lineart_shadow.c includes BKE/DEG/DNA headers for the light object and Depsgraph; its one
 * function that reads them is a Scene-lite replacement, so only Line Art's own types, blenlib,
 * guarded allocation, G and PIL time are needed here, plus the Scene-lite / settings types and
 * the loader callback in project_grease_lineart_cpu.cc.
 */
#pragma once

#include <stdio.h>
#include <string.h>

#include "MOD_lineart.h"

#include "lineart_intern.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "DNA_gpencil_modifier_types.h"
#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "project_grease_lineart_lite.h"
#include "project_grease_scene_lite.h"

void pg_lineart_load_geometries_for_shadow(const PGSceneLite *scene, LineartData *ld);
