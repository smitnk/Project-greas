/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Project Grease focused extraction from Blender 3.6.23 Legacy GP editor.
 *
 * This is the exact ED_gpencil_sbuffer_ensure algorithm from the pinned
 * Blender 3.6.23 source, isolated because the complete desktop gpencil_utils.c
 * pulls RNA/window-manager/editor dependencies that Project Grease deliberately
 * does not port.
 */

#include <stdbool.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "DNA_gpencil_legacy_types.h"
#include "ED_gpencil_legacy.h"

/* Exact constant from Blender 3.6.23 gpencil_intern.h.
 * Keep this local so the focused extraction does not pull the desktop
 * gpencil_intern.h -> ED_numinput.h dependency closure. */
#define GP_STROKE_BUFFER_CHUNK 2048

struct tGPspoint *ED_gpencil_sbuffer_ensure(struct tGPspoint *buffer_array,
                                            int *buffer_size,
                                            int *buffer_used,
                                            bool clear)
{
  struct tGPspoint *p = NULL;

  if (*buffer_used + 1 > *buffer_size) {
    if ((*buffer_size == 0) || (buffer_array == NULL)) {
      p = MEM_callocN(sizeof(struct tGPspoint) * GP_STROKE_BUFFER_CHUNK,
                      "GPencil Sbuffer");
      *buffer_size = GP_STROKE_BUFFER_CHUNK;
    }
    else {
      *buffer_size += GP_STROKE_BUFFER_CHUNK;
      p = MEM_recallocN(buffer_array,
                        sizeof(struct tGPspoint) * *buffer_size);
    }

    if (p == NULL) {
      *buffer_size = *buffer_used = 0;
    }

    buffer_array = p;
  }

  if (clear) {
    *buffer_used = 0;
    if (buffer_array != NULL) {
      memset(buffer_array, 0, sizeof(struct tGPspoint) * *buffer_size);
    }
  }

  return buffer_array;
}
