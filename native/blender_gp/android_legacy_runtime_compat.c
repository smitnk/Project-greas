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

