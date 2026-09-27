/* Project Grease Android GLES compatibility shim for the pinned Blender 3.6 GPU probe.
 * This file intentionally replaces libepoxy's desktop GL header only for Android probe builds.
 * It does not emulate desktop OpenGL; unsupported APIs must remain visible as compile failures.
 */
#pragma once

#ifndef __ANDROID__
#  error "Project Grease Android GLES shim is Android-only"
#endif

#ifndef GL_GLEXT_PROTOTYPES
#  define GL_GLEXT_PROTOTYPES 1
#endif

#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
