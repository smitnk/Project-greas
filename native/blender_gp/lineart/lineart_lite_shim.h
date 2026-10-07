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
  * Project Grease executes this scoped Line Art closure with Blender's task system built without
  * TBB, so all Line Art workers are serialized. Keep Blender's lock API available to the rest of
  * the closure, but use a scoped no-op lock for this serial-only generated Line Art translation
  * unit. The previous Project Grease runtime used the same serialization guarantee; enabling the
  * real pthread spin lock here introduced a regression/hang without adding synchronization value.
  */
 extern "C" {
 void PG_lineart_spin_init(SpinLock *spin);
 void PG_lineart_spin_lock(SpinLock *spin);
 void PG_lineart_spin_unlock(SpinLock *spin);
 void PG_lineart_spin_end(SpinLock *spin);
 }
 #define BLI_spin_init PG_lineart_spin_init
 #define BLI_spin_lock PG_lineart_spin_lock
 #define BLI_spin_unlock PG_lineart_spin_unlock
 /*
 * The real Blender task pool executes pushes immediately when built without TBB. The previous
 * Project Grease Line Art runtime queued all workers and ran them in push order at work_and_wait().
 * Use Blender's suspended pool to preserve that observable serial scheduling contract while still
 * using the real Blender 3.6.23 task-pool implementation.
 */
#define BLI_task_pool_create BLI_task_pool_create_suspended
#define BLI_spin_end PG_lineart_spin_end
