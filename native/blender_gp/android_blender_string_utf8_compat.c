/*
 * Android compatibility translation unit for the original Blender 3.6.23
 * UTF-8 string implementation.
 *
 * The upstream source is compiled unchanged; this wrapper only scopes the
 * Android/Clang diagnostic policy to the required legacy translation unit.
 */
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif

#include "../../third_party/blender/source/blender/blenlib/intern/string_utf8.c"

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
