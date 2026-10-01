/*
 * Android compatibility wrapper for the original Blender 3.6.23
 * string_utf8.c. The upstream source promotes -Wsign-conversion through a
 * GCC diagnostic pragma. Its headers are pre-included with normal compiler
 * macros, then __GNUC__ is hidden only while the unchanged source is included.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
/* Blender 3.6.23 vendored wcwidth header. */
#include "../../third_party/blender/extern/wcwidth/wcwidth.h"

#include "../../third_party/blender/source/blender/blenlib/BLI_utildefines.h"
#include "../../third_party/blender/source/blender/blenlib/BLI_string.h"
#include "../../third_party/blender/source/blender/blenlib/BLI_string_utf8.h"

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#  pragma push_macro("__GNUC__")
#  undef __GNUC__
#endif

#include "../../third_party/blender/source/blender/blenlib/intern/string_utf8.c"

#if defined(__clang__)
#  pragma pop_macro("__GNUC__")
#  pragma clang diagnostic pop
#endif
