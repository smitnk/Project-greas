/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Minimal runtime symbol supplied by Project Grease. The Line Art task, spin-lock, scheduler and
 * timer implementations are the real Blender 3.6.23 sources from the pinned L1 closure.
 * Blender's full global runtime is outside the scoped Android/host closure, so Line Art's
 * G.debug_value access is backed by this zero-initialized Global instance.
 */

#include "BKE_global.h"
#include "BLI_task.h"

Global G = {};

TaskPool *PG_lineart_task_pool_create(void *userdata, eTaskPriority priority)
{
  (void)priority;
  return BLI_task_pool_create_no_threads(userdata);
}
