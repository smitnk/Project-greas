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

void BKE_defvert_array_copy(MDeformVert *dst, const MDeformVert *src, int totvert)
{
  if (!dst || !src || totvert <= 0) {
    return;
  }
  for (int i = 0; i < totvert; ++i) {
    dst[i].totweight = src[i].totweight;
    dst[i].dw = NULL;
    if (src[i].totweight > 0 && src[i].dw) {
      dst[i].dw = MEM_mallocN(sizeof(MDeformWeight) * src[i].totweight,
                              "Project Grease defvert copy");
      memcpy(dst[i].dw, src[i].dw, sizeof(MDeformWeight) * src[i].totweight);
    }
  }
}
