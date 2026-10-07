/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Runtime pieces the verbatim Line Art core calls that live in parts of Blender Project Grease
 * does not link. Only the global `G` remains here: Line Art reads G.debug_value (timing
 * printouts at 4000) and G.is_break, and Blender defines `G` in blenkernel/intern/blender.c,
 * which is not part of the Project Grease closure.
 *
 * The task pool, BLI_task_parallel_range, spin locks and PIL_check_seconds_timer formerly
 * implemented here are now provided by the real Blender 3.6.23 sources in the L1 closure
 * (blenlib task_pool.cc, task_range.cc, threads.cc, time.c), built without TBB, so they run
 * serially and deterministically. The shadow stage is project_grease_lineart_shadow.c
 * (generated from lineart_shadow.c).
 */

#include "BKE_global.h"

/* G.debug_value is read by Line Art for its timing printouts (value 4000). */
Global G = {};
