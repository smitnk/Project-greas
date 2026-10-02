/* Test-only stand-in for the Blender header of the same name: just the
 * declarations project_grease_blender_primitive.c needs, with Blender 3.6.23
 * math semantics, so the primitive code can be unit-tested without the
 * pinned Blender tree. Production builds use the real headers. */
#pragma once
#include <math.h>
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#  define M_PI_2 1.57079632679489661923
#endif
