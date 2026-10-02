/* guardedalloc stand-ins for host tests that link real pinned C++ blenlib sources (rand.cc). In
 * Blender 3.6 the MEM_* entry points are function-pointer variables (MEM_guardedalloc.h). */
#include <cstdlib>

static void *plain_malloc(size_t len, const char *) { return malloc(len); }
static void *plain_malloc_aligned(size_t len, size_t, const char *) { return malloc(len); }
static void plain_free(void *p) { free(p); }
extern "C" {
void *(*MEM_mallocN)(size_t, const char *) = plain_malloc;
void *(*MEM_mallocN_aligned)(size_t, size_t, const char *) = plain_malloc_aligned;
void (*MEM_freeN)(void *) = plain_free;
}
