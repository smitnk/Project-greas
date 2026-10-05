/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Bug-hunt (PG_ASAN) build only. Under AddressSanitizer the linker keeps BLI_mempool_create alive
 * (the app never calls it), and with it its reference to BLI_mutex_init from blenlib threads.cc,
 * which this library does not link. This is that function as Blender defines it. The host fuzzer
 * defines the same (tests/fuzz_edit_commands.cc). */
#include <pthread.h>

void BLI_mutex_init(pthread_mutex_t *mutex)
{
  pthread_mutex_init(mutex, NULL);
}
