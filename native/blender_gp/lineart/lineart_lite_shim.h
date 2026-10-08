/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Includes for the generated Line Art source (project_grease_lineart_cpu.cc). The pinned
 * lineart_cpu.cc includes BKE/DEG/RE headers for Mesh, Object, Depsgraph and render access; the
 * generated file replaces those parts with Scene-lite (see lineart_scene_lite_replacements.cc),
 * so only the headers the verbatim Line Art core needs are included here: Line Art's own types,
 * blenlib math/lists/tasks/threads, guarded allocation, the DNA types it names, G and PIL time.
 */
#pragma once

#include "MOD_lineart.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_sort.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_global.h"

#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_lineart_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "lineart_intern.h"

#include "project_grease_lineart_lite.h"
#include "project_grease_scene_lite.h"

#include <algorithm>
#include <cstring>

/*
 * Keep the scoped Line Art closure serial. The host closure may compile Blender's task pool with
 * TBB, while this Project Grease Line Art port intentionally has no parallel dependency graph or
 * thread-safe global runtime around it. Blender's NO_THREADS pool is still the real 3.6.23 task
 * pool implementation and executes each pushed task synchronously.
 */
TaskPool *PG_lineart_task_pool_create(void *userdata, eTaskPriority priority);
#define BLI_task_pool_create PG_lineart_task_pool_create

/* Serial Project Grease Line Art closure: no concurrent worker can touch these locks. */
#define BLI_spin_init PG_lineart_spin_init
#define BLI_spin_lock PG_lineart_spin_lock
#define BLI_spin_unlock PG_lineart_spin_unlock
#define BLI_spin_end PG_lineart_spin_end

void PG_lineart_spin_init(SpinLock *spin);
void PG_lineart_spin_lock(SpinLock *spin);
void PG_lineart_spin_unlock(SpinLock *spin);
void PG_lineart_spin_end(SpinLock *spin);
