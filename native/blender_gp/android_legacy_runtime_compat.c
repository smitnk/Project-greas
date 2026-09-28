#include <stdarg.h>
#include <string.h>

#include "DNA_ID.h"
#include "DNA_meshdata_types.h"
#include "DNA_userdef_types.h"
#include "MEM_guardedalloc.h"
#include "CLG_log.h"

/* Minimal Android runtime instance consumed by the pinned Legacy GP API. */
UserDef U;

void DEG_id_tag_update(struct ID *id, unsigned int flags)
{
  (void)id;
  (void)flags;
}

void CLG_logref_init(CLG_LogRef *clg_ref)
{
  if (clg_ref) {
    clg_ref->type = NULL;
  }
}

void CLG_log_str(CLG_LogType *lg,
                 enum CLG_Severity severity,
                 const char *file_line,
                 const char *fn,
                 const char *message)
{
  (void)lg; (void)severity; (void)file_line; (void)fn; (void)message;
}

void CLG_logf(CLG_LogType *lg,
              enum CLG_Severity severity,
              const char *file_line,
              const char *fn,
              const char *format,
              ...)
{
  (void)lg; (void)severity; (void)file_line; (void)fn; (void)format;
}


/* Android has no Blender UI icon registry; Legacy GP layer deletion only needs the registry release hook. */
void BKE_icon_delete(void *id)
{
  (void)id;
}


/*
 * Blender 3.6.23 DNA C++ shallow-copy helpers.
 *
 * DNA_defs.h deliberately declares these as an external runtime seam so the
 * generated DNA metadata does not need to pull Blender's full runtime.
 * Keep the implementation byte-for-byte semantic: these operations are
 * ordinary raw-memory operations used by DNA_DEFINE_CXX_METHODS.
 */
void _DNA_internal_memcpy(void *dst, const void *src, size_t size)
{
  memcpy(dst, src, size);
}

void _DNA_internal_memzero(void *dst, size_t size)
{
  memset(dst, 0, size);
}

void _DNA_internal_swap(void *a, void *b, size_t size)
{
  if (a == b || size == 0) {
    return;
  }

  unsigned char *pa = (unsigned char *)a;
  unsigned char *pb = (unsigned char *)b;
  for (size_t i = 0; i < size; ++i) {
    unsigned char tmp = pa[i];
    pa[i] = pb[i];
    pb[i] = tmp;
  }
}
