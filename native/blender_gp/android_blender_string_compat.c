/*
 * Android compatibility translation unit for the original Blender 3.6.23
 * string implementation.
 *
 * The upstream source is compiled unchanged. Android/NDK warning policy in
 * this focused target can promote Blender's legacy sign-conversion diagnostics
 * to errors; keep that policy local to this translation unit instead of
 * modifying the upstream Blender source.
 */
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif

#include "../../third_party/blender/source/blender/blenlib/intern/string.c"

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
