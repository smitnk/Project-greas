/* Test-only stand-in for the Blender header of the same name: just the
 * declarations project_grease_blender_primitive.c needs, with Blender 3.6.23
 * math semantics, so the primitive code can be unit-tested without the
 * pinned Blender tree. Production builds use the real headers. */
#pragma once
typedef struct tGPspoint { float m_xy[2]; float pressure, strength, time; } tGPspoint;
