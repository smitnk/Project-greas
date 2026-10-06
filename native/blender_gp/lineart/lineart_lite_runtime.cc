/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Runtime pieces the verbatim Line Art core calls that live in parts of Blender Project Grease
 * does not link: a serial BLI_task pool / parallel range (Line Art is run with one thread, which
 * also makes its output deterministic), spin locks (no other thread touches the data), G and PIL
 * time (debug timing only). The shadow stage is project_grease_lineart_shadow.c (generated from
 * lineart_shadow.c).
 */

#include <chrono>
#include <cstdlib>
#include <cstring>

#include "BLI_task.h"
#include "BLI_threads.h"
#include "PIL_time.h"

#include "BKE_global.h"

#include "MOD_lineart.h"
#include "lineart_intern.h"

/* G.debug_value is read by Line Art for its timing printouts (value 4000). */
Global G = {};

double PIL_check_seconds_timer(void)
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void BLI_spin_init(SpinLock *spin) { *spin = SpinLock(); }
void BLI_spin_lock(SpinLock * /*spin*/) {}
void BLI_spin_unlock(SpinLock * /*spin*/) {}
void BLI_spin_end(SpinLock * /*spin*/) {}

struct PGTask {
  TaskRunFunction run;
  void *data;
  bool free_data;
  TaskFreeFunction free_fn;
};

struct TaskPool {
  PGTask *tasks = nullptr;
  int count = 0, cap = 0;
  void *userdata = nullptr;
};

TaskPool *BLI_task_pool_create(void *userdata, eTaskPriority /*priority*/)
{
  TaskPool *pool = static_cast<TaskPool *>(calloc(1, sizeof(TaskPool)));
  pool->userdata = userdata;
  return pool;
}

void BLI_task_pool_push(TaskPool *pool, TaskRunFunction run, void *taskdata, bool free_taskdata, TaskFreeFunction freedata)
{
  if (pool->count == pool->cap) {
    pool->cap = pool->cap ? pool->cap * 2 : 8;
    pool->tasks = static_cast<PGTask *>(realloc(pool->tasks, sizeof(PGTask) * size_t(pool->cap)));
  }
  pool->tasks[pool->count++] = PGTask{run, taskdata, free_taskdata, freedata};
}

void BLI_task_pool_work_and_wait(TaskPool *pool)
{
  /* Tasks may push more tasks; run until the queue is empty, in push order. */
  for (int i = 0; i < pool->count; i++) {
    PGTask t = pool->tasks[i];
    t.run(pool, t.data);
    if (t.free_data) {
      if (t.free_fn) t.free_fn(pool, t.data);
      else free(t.data);
    }
  }
  pool->count = 0;
}

void BLI_task_pool_free(TaskPool *pool)
{
  if (!pool) return;
  free(pool->tasks);
  free(pool);
}

void *BLI_task_pool_user_data(TaskPool *pool) { return pool->userdata; }

void BLI_task_parallel_range(int start, int stop, void *userdata, TaskParallelRangeFunc func, const TaskParallelSettings *settings)
{
  /* One chunk: TLS userdata_chunk is the caller's chunk itself, so no reduce is needed. */
  TaskParallelTLS tls;
  std::memset(&tls, 0, sizeof(tls));
  tls.userdata_chunk = settings ? settings->userdata_chunk : nullptr;
  if (settings && settings->func_init && tls.userdata_chunk) settings->func_init(userdata, tls.userdata_chunk);
  for (int i = start; i < stop; i++) func(userdata, i, &tls);
  if (settings && settings->func_free && tls.userdata_chunk) settings->func_free(userdata, tls.userdata_chunk);
}
