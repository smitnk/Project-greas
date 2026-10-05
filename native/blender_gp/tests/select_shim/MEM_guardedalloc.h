/* Test-only stand-in for guarded allocation. */
#pragma once
#include <stdlib.h>
/* Frees through MEM_SAFE_FREE are counted so tests can assert that nothing is leaked (the host
 * test scripts run ASan with leak detection off). The counter lives in test_blender_edit.c. */
extern int pg_test_mem_free_count;
#define MEM_freeN(p) free(p)
#define MEM_SAFE_FREE(v) do { if (v) { pg_test_mem_free_count++; free(v); (v) = NULL; } } while (0)
#define MEM_callocN(size, name) calloc(1, (size))
#define MEM_mallocN(size, name) malloc(size)
#define MEM_reallocN(p, size) realloc((p), (size))
