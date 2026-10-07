/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Runtime pieces the verbatim Line Art core calls that live in parts of Blender Project Grease
 * does not link. Only the global `G` and the scoped serial Line Art lock adapter remain here:
 * Line Art reads G.debug_value (timing printouts at 4000) and G.is_break, while Blender defines
 * G in blenkernel/intern/blender.c, which is not part of the Project Grease closure.
 *
 * The task pool, BLI_task_parallel_range and PIL_check_seconds_timer are provided by the real
 * Blender 3.6.23 sources in the L1 closure (task_pool.cc, task_range.cc, task_scheduler.cc,
 * threads.cc and time.c), built without TBB, so this scoped Line Art execution is serialized.
 * The generated Line Art translation unit therefore maps its BLI_spin_* calls to the no-op
 * PG_lineart_spin_* adapter below. This preserves the pre-L1 serial execution contract without
 * overriding Blender's real BLI_spin_* symbols used elsewhere in the closure.
 */

#include "BKE_global.h"
#include "BLI_threads.h"

Global G = {};

/*
 * The Line Art task pool is serial in this Project Grease closure (no TBB). Its lock-protected
 * containers are therefore not concurrently accessed. Keep the SpinLock object initialized so
 * the generated code can still pass the exact Blender lock fields around, but do not activate
 * pthread spin-lock synchronization inside the serial worker.
 */
extern "C" void PG_lineart_spin_init(SpinLock * /*spin*/) {}
extern "C" void PG_lineart_spin_lock(SpinLock * /*spin*/) {}
extern "C" void PG_lineart_spin_unlock(SpinLock * /*spin*/) {}
extern "C" void PG_lineart_spin_end(SpinLock * /*spin*/) {}
