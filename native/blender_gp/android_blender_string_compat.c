/*
 * Android compatibility wrapper for the original Blender 3.6.23 string.c.
 *
 * Blender's source intentionally promotes -Wsign-conversion to an error with
 * a GCC diagnostic pragma. Android Clang is already compiling the focused
 * closure with strict warnings, but this legacy source contains four known
 * size_t -> int conversions. Pre-include the source's headers normally, then
 * temporarily hide __GNUC__ while including the unchanged upstream source so
 * its diagnostic pragma is skipped. No Blender source is modified.
 */
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../third_party/blender/intern/guardedalloc/MEM_guardedalloc.h"
#include "../../third_party/blender/source/blender/blenlib/BLI_dynstr.h"
#include "../../third_party/blender/source/blender/blenlib/BLI_string.h"
#include "../../third_party/blender/source/blender/blenlib/BLI_utildefines.h"

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#  pragma push_macro("__GNUC__")
#  undef __GNUC__
#endif

#include "../../third_party/blender/source/blender/blenlib/intern/string.c"

#if defined(__clang__)
#  pragma pop_macro("__GNUC__")
#  pragma clang diagnostic pop
#endif
