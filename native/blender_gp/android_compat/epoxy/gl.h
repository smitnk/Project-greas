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
/*
 * Pinned Blender 3.6's desktop OpenGL texture headers contain a few enums
 * and 1D entry points that are not part of the GLES 3 core API.
 *
 * These definitions are compatibility-only. They do not advertise hardware
 * support for optional formats. The GP path must use GLES-supported 2D/3D
 * texture targets and formats at runtime.
 */
#ifndef GL_R16_SNORM
#  define GL_R16_SNORM 0x8F98
#endif

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#  define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#  define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#  define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#  define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#  define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

/*
 * Blender's generic GL backend contains legacy 1D texture branches.
 * GLES has no 1D texture target, so the Android probe maps the simple 1D
 * image operations to a 2D texture whose height is one. This keeps the
 * generic source compilable without introducing a desktop GL dependency.
 */
#ifndef GL_TEXTURE_1D
#  define GL_TEXTURE_1D GL_TEXTURE_2D
#endif
#ifndef GL_PROXY_TEXTURE_1D
#  define GL_PROXY_TEXTURE_1D GL_PROXY_TEXTURE_2D
#endif
#ifndef GL_TEXTURE_1D_ARRAY
#  define GL_TEXTURE_1D_ARRAY GL_TEXTURE_2D_ARRAY
#endif
#ifndef GL_PROXY_TEXTURE_1D_ARRAY
#  define GL_PROXY_TEXTURE_1D_ARRAY GL_PROXY_TEXTURE_2D_ARRAY
#endif
#ifndef GL_TEXTURE_CUBE_MAP_ARRAY_ARB
#  ifdef GL_TEXTURE_CUBE_MAP_ARRAY
#    define GL_TEXTURE_CUBE_MAP_ARRAY_ARB GL_TEXTURE_CUBE_MAP_ARRAY
#  endif
#endif

#ifndef GL_PROXY_TEXTURE_2D
/* GLES has no proxy texture targets. Keep this compile-time-only sentinel. */
#  define GL_PROXY_TEXTURE_2D 0x8063
#endif
#ifndef GL_PROXY_TEXTURE_2D_ARRAY
#  define GL_PROXY_TEXTURE_2D_ARRAY 0x8C1D
#endif

#ifndef PROJECT_GREASE_GLES_1D_SHIMS
#  define PROJECT_GREASE_GLES_1D_SHIMS 1

static inline void glTexImage1D(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void *pixels)
{
  (void)border;
  glTexImage2D(target, level, internalformat, width, 1, 0, format, type, pixels);
}

static inline void glTexSubImage1D(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLsizei width,
                                   GLenum format,
                                   GLenum type,
                                   const void *pixels)
{
  glTexSubImage2D(target, level, xoffset, 0, width, 1, format, type, pixels);
}

static inline void glCompressedTexImage1D(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void *data)
{
  (void)border;
  glCompressedTexImage2D(target, level, internalformat, width, 1, 0, imageSize, data);
}

static inline void glCompressedTexSubImage1D(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLsizei width,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void *data)
{
  glCompressedTexSubImage2D(target, level, xoffset, 0, width, 1, format, imageSize, data);
}

#endif /* PROJECT_GREASE_GLES_1D_SHIMS */

