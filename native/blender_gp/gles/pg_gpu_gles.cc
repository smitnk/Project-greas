/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * L8 part 2: the part of Blender 3.6.23's GPU module API that the Grease Pencil draw engine calls
 * (shaders from create-info names, uniform buffers, 2D textures, frame-buffers), on OpenGL ES 3.2.
 * Vertex / index buffers and batches are Blender's own gpu_vertex_buffer.cc / gpu_index_buffer.cc /
 * gpu_batch.cc on android_gpu_backend.cpp. Shaders are the pre-generated sources of
 * gles/generated (tools/gles_shaders, Blender's own shader assembly), not assembled at run time.
 *
 * ADAPTED from source/blender/gpu/opengl (gl_texture.cc, gl_framebuffer.cc, gl_uniform_buffer.cc,
 * gl_shader.cc): the same GL calls for the GLES 3.2 subset; formats GLES 3.2 does not have are
 * listed in pg_gl_format().
 */
#include <GLES3/gl32.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "GPU_framebuffer.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "pg_gpu_gles.h"

#include "generated/pg_gles_shaders.inc"

#if defined(__ANDROID__) && __has_include(<android/log.h>)
#  include <android/log.h>
#  define PG_GLES_LOG(...) __android_log_print(ANDROID_LOG_ERROR, "ProjectGreaseGLES", __VA_ARGS__)
#else
#  define PG_GLES_LOG(...) (std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))
#endif

struct GPUShader {
  GLuint program = 0;
  std::string name;
  std::map<std::string, GLint> uniforms;
  std::map<std::string, GLint> blocks;   /* binding point */
  std::map<std::string, GLint> samplers; /* texture unit */
};

struct GPUUniformBuf {
  GLuint id = 0;
  size_t size = 0;
};

struct GPUTexture {
  GLuint id = 0;
  int w = 0, h = 0;
  eGPUTextureFormat format = GPU_RGBA8;
  bool depth = false;
  bool stencil = false;
};

struct GPUFrameBuffer {
  GLuint id = 0;
  GPUTexture *depth = nullptr;
  GPUTexture *color[8] = {};
  int w = 0, h = 0;
};

static std::string g_last_error;
static GPUFrameBuffer *g_active_fb = nullptr;
static GLint g_backbuffer = 0;

const char *pg_gpu_gles_last_error()
{
  return g_last_error.c_str();
}

static void set_error(const std::string &e)
{
  g_last_error = e;
  PG_GLES_LOG("%s", e.c_str());
}

/* -------------------------------------------------------------------- */
/** \name Shaders (create-info name -> generated GLES 3.2 sources)
 * \{ */

static GLuint compile(GLenum type, const char *src, const char *info, std::string &log)
{
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = GL_FALSE;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char buf[2048] = {};
    glGetShaderInfoLog(s, sizeof(buf) - 1, nullptr, buf);
    log = std::string(info) + (type == GL_VERTEX_SHADER ? " vert: " : " frag: ") + buf;
    glDeleteShader(s);
    return 0;
  }
  return s;
}

const char *pg_gles_shader_source(const char *info_name, int fragment)
{
  for (const PGGlesShaderSource &src : pg_gles_shader_sources) {
    if (std::strcmp(src.info, info_name) == 0) {
      return fragment ? src.frag : src.vert;
    }
  }
  return nullptr;
}

GPUShader *GPU_shader_create_from_info_name(const char *info_name)
{
  GPUShader *sh = new GPUShader();
  sh->name = info_name;
  const char *vs = pg_gles_shader_source(info_name, 0);
  const char *fs = pg_gles_shader_source(info_name, 1);
  if (vs == nullptr || fs == nullptr) {
    set_error(std::string("no generated GLES shader for ") + info_name);
    return sh;
  }
  std::string log;
  GLuint v = compile(GL_VERTEX_SHADER, vs, info_name, log);
  GLuint f = v ? compile(GL_FRAGMENT_SHADER, fs, info_name, log) : 0;
  if (!v || !f) {
    set_error(log);
    if (v) {
      glDeleteShader(v);
    }
    return sh;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = GL_FALSE;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char buf[2048] = {};
    glGetProgramInfoLog(p, sizeof(buf) - 1, nullptr, buf);
    set_error(std::string(info_name) + " link: " + buf);
    glDeleteProgram(p);
    return sh;
  }
  sh->program = p;
  /* Uniforms (samplers get their unit from layout(binding)). */
  GLint count = 0;
  glGetProgramiv(p, GL_ACTIVE_UNIFORMS, &count);
  for (GLint i = 0; i < count; i++) {
    char name[256];
    GLsizei len = 0;
    GLint size = 0;
    GLenum type = 0;
    glGetActiveUniform(p, (GLuint)i, sizeof(name), &len, &size, &type, name);
    std::string n(name, len);
    if (n.size() > 3 && n.compare(n.size() - 3, 3, "[0]") == 0) {
      n.resize(n.size() - 3);
    }
    const GLint loc = glGetUniformLocation(p, name);
    if (loc < 0) {
      continue; /* member of a uniform block */
    }
    sh->uniforms[n] = loc;
    if (type == GL_SAMPLER_2D || type == GL_SAMPLER_BUFFER || type == GL_INT_SAMPLER_BUFFER ||
        type == GL_UNSIGNED_INT_SAMPLER_BUFFER || type == GL_SAMPLER_2D_ARRAY ||
        type == GL_SAMPLER_3D || type == GL_SAMPLER_CUBE || type == GL_INT_SAMPLER_2D ||
        type == GL_UNSIGNED_INT_SAMPLER_2D || type == GL_SAMPLER_2D_SHADOW)
    {
      GLint unit = 0;
      glGetUniformiv(p, loc, &unit);
      sh->samplers[n] = unit;
    }
  }
  glGetProgramiv(p, GL_ACTIVE_UNIFORM_BLOCKS, &count);
  for (GLint i = 0; i < count; i++) {
    char name[256];
    GLsizei len = 0;
    glGetActiveUniformBlockName(p, (GLuint)i, sizeof(name), &len, name);
    GLint binding = 0;
    glGetActiveUniformBlockiv(p, (GLuint)i, GL_UNIFORM_BLOCK_BINDING, &binding);
    sh->blocks[std::string(name, len)] = binding;
  }
  return sh;
}

void GPU_shader_free(GPUShader *shader)
{
  if (shader == nullptr) {
    return;
  }
  if (shader->program) {
    glDeleteProgram(shader->program);
  }
  delete shader;
}

unsigned int pg_gpu_shader_program(const GPUShader *sh)
{
  return sh ? sh->program : 0;
}

int pg_gpu_shader_uniform(const GPUShader *sh, const char *name)
{
  if (sh == nullptr) {
    return -1;
  }
  auto it = sh->uniforms.find(name);
  return it == sh->uniforms.end() ? -1 : it->second;
}

int pg_gpu_shader_block_binding(const GPUShader *sh, const char *name)
{
  if (sh == nullptr) {
    return -1;
  }
  auto it = sh->blocks.find(name);
  return it == sh->blocks.end() ? -1 : it->second;
}

int pg_gpu_shader_sampler_unit(const GPUShader *sh, const char *name)
{
  if (sh == nullptr) {
    return -1;
  }
  auto it = sh->samplers.find(name);
  return it == sh->samplers.end() ? -1 : it->second;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform buffers (gl_uniform_buffer.cc)
 * \{ */

GPUUniformBuf *GPU_uniformbuf_create_ex(size_t size, const void *data, const char * /*name*/)
{
  GPUUniformBuf *ubo = new GPUUniformBuf();
  ubo->size = size;
  glGenBuffers(1, &ubo->id);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo->id);
  glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)size, data, GL_DYNAMIC_DRAW);
  return ubo;
}

void GPU_uniformbuf_update(GPUUniformBuf *ubo, const void *data)
{
  glBindBuffer(GL_UNIFORM_BUFFER, ubo->id);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, (GLsizeiptr)ubo->size, data);
}

void GPU_uniformbuf_free(GPUUniformBuf *ubo)
{
  if (ubo == nullptr) {
    return;
  }
  glDeleteBuffers(1, &ubo->id);
  delete ubo;
}

void pg_gpu_uniformbuf_bind(const GPUUniformBuf *ubo, int binding)
{
  if (ubo != nullptr && binding >= 0) {
    glBindBufferBase(GL_UNIFORM_BUFFER, (GLuint)binding, ubo->id);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Textures (gl_texture.cc, to_gl_internal_format / to_gl_data_format)
 * \{ */

struct PGGLFormat {
  GLenum internal, format, type;
  bool depth, stencil, is_float;
};

static PGGLFormat pg_gl_format(eGPUTextureFormat f)
{
  switch (f) {
    case GPU_RGBA8: return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false};
    case GPU_RGBA16F: return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, false, true};
    case GPU_RGBA32F: return {GL_RGBA32F, GL_RGBA, GL_FLOAT, false, false, true};
    case GPU_RG8: return {GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false, false, false};
    case GPU_R8: return {GL_R8, GL_RED, GL_UNSIGNED_BYTE, false, false, false};
    /* shortcut: GLES 3.2 has no R16 UNORM without EXT_texture_norm16; R16F holds the same mask
     * values (only render masks use R16). Use R16 when that extension is present. */
    case GPU_R16: return {GL_R16F, GL_RED, GL_HALF_FLOAT, false, false, true};
    case GPU_R16F: return {GL_R16F, GL_RED, GL_HALF_FLOAT, false, false, true};
    case GPU_R32F: return {GL_R32F, GL_RED, GL_FLOAT, false, false, true};
    case GPU_RG16F: return {GL_RG16F, GL_RG, GL_HALF_FLOAT, false, false, true};
    case GPU_R11F_G11F_B10F: return {GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT, false, false, true};
    case GPU_SRGB8_A8: return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false};
    case GPU_DEPTH24_STENCIL8:
      return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, true, true, false};
    case GPU_DEPTH_COMPONENT24:
      return {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, true, false, false};
    case GPU_DEPTH32F_STENCIL8:
      return {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, true, true, false};
    default:
      return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false};
  }
}

static int pg_comp_len(GLenum format)
{
  switch (format) {
    case GL_RED: return 1;
    case GL_RG: return 2;
    case GL_RGB: return 3;
    default: return 4;
  }
}

/* Upload \a data given as \a data_format (GPU_DATA_FLOAT or GPU_DATA_UBYTE). GLES does not
 * convert between client and internal types, so float data for 8-bit formats and ubyte data for
 * float formats are converted here, as the desktop GL driver would. */
static void pg_upload(GPUTexture *tex, eGPUDataFormat data_format, const void *data)
{
  const PGGLFormat gf = pg_gl_format(tex->format);
  const int n = tex->w * tex->h * pg_comp_len(gf.format);
  glBindTexture(GL_TEXTURE_2D, tex->id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  if (data_format == GPU_DATA_FLOAT && !gf.is_float && gf.type == GL_UNSIGNED_BYTE) {
    std::vector<unsigned char> b((size_t)n);
    const float *f = (const float *)data;
    for (int i = 0; i < n; i++) {
      const float v = f[i] < 0.0f ? 0.0f : (f[i] > 1.0f ? 1.0f : f[i]);
      b[(size_t)i] = (unsigned char)(v * 255.0f + 0.5f);
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->w, tex->h, gf.format, GL_UNSIGNED_BYTE, b.data());
  }
  else if (data_format == GPU_DATA_FLOAT && gf.is_float) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->w, tex->h, gf.format, GL_FLOAT, data);
  }
  else if (data_format == GPU_DATA_UBYTE && !gf.is_float) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->w, tex->h, gf.format, GL_UNSIGNED_BYTE, data);
  }
  else {
    set_error("pg_upload: unsupported data / texture format pair");
  }
}

GPUTexture *GPU_texture_create_2d(const char * /*name*/,
                                  int width,
                                  int height,
                                  int /*mip_len*/,
                                  eGPUTextureFormat format,
                                  eGPUTextureUsage /*usage*/,
                                  const float *data)
{
  GPUTexture *tex = new GPUTexture();
  tex->w = width < 1 ? 1 : width;
  tex->h = height < 1 ? 1 : height;
  tex->format = format;
  const PGGLFormat gf = pg_gl_format(format);
  tex->depth = gf.depth;
  tex->stencil = gf.stencil;
  glGenTextures(1, &tex->id);
  glBindTexture(GL_TEXTURE_2D, tex->id);
  glTexStorage2D(GL_TEXTURE_2D, 1, gf.internal, tex->w, tex->h);
  /* GPUSamplerState default: no filter, clamp to edge (gl_texture.cc sampler defaults). */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (data != nullptr) {
    pg_upload(tex, GPU_DATA_FLOAT, data);
  }
  return tex;
}

void GPU_texture_update(GPUTexture *texture, eGPUDataFormat data_format, const void *data)
{
  pg_upload(texture, data_format, data);
}

void GPU_texture_filter_mode(GPUTexture *texture, bool use_filter)
{
  glBindTexture(GL_TEXTURE_2D, texture->id);
  const GLint f = use_filter ? GL_LINEAR : GL_NEAREST;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
}

void pg_gpu_texture_wrap_mode(GPUTexture *texture, int repeat)
{
  glBindTexture(GL_TEXTURE_2D, texture->id);
  const GLint w = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, w);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, w);
}

void GPU_texture_free(GPUTexture *texture)
{
  if (texture == nullptr) {
    return;
  }
  glDeleteTextures(1, &texture->id);
  delete texture;
}

int GPU_texture_width(const GPUTexture *texture)
{
  return texture->w;
}

int GPU_texture_height(const GPUTexture *texture)
{
  return texture->h;
}

eGPUTextureFormat GPU_texture_format(const GPUTexture *texture)
{
  return texture->format;
}

void pg_gpu_texture_bind(const GPUTexture *tex, int unit)
{
  if (tex != nullptr && unit >= 0) {
    glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
    glBindTexture(GL_TEXTURE_2D, tex->id);
  }
}

unsigned int pg_gpu_texture_gl(const GPUTexture *tex)
{
  return tex ? tex->id : 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame-buffers (gl_framebuffer.cc)
 * \{ */

GPUFrameBuffer *GPU_framebuffer_create(const char * /*name*/)
{
  GPUFrameBuffer *fb = new GPUFrameBuffer();
  glGenFramebuffers(1, &fb->id);
  return fb;
}

void GPU_framebuffer_free(GPUFrameBuffer *framebuffer)
{
  if (framebuffer == nullptr) {
    return;
  }
  if (g_active_fb == framebuffer) {
    g_active_fb = nullptr;
  }
  if (framebuffer->id) {
    glDeleteFramebuffers(1, &framebuffer->id);
  }
  delete framebuffer;
}

static void pg_fb_draw_buffers(GPUFrameBuffer *fb)
{
  GLenum bufs[8];
  int n = 0;
  for (int i = 0; i < 8; i++) {
    bufs[i] = fb->color[i] ? GL_COLOR_ATTACHMENT0 + (GLenum)i : GL_NONE;
    if (fb->color[i]) {
      n = i + 1;
    }
  }
  if (n == 0) {
    bufs[0] = GL_NONE;
    n = 1;
  }
  glDrawBuffers(n, bufs);
}

void GPU_framebuffer_config_array(GPUFrameBuffer *fb, const GPUAttachment *config, int config_len)
{
  glBindFramebuffer(GL_FRAMEBUFFER, fb->id);
  /* First attachment is the depth (GPU_framebuffer_ensure_config contract). */
  for (int i = 0; i < config_len; i++) {
    const GPUAttachment &a = config[i];
    const bool leave = a.tex == nullptr && a.layer == -1 && a.mip == -1; /* GPU_ATTACHMENT_LEAVE */
    if (leave) {
      continue;
    }
    if (i == 0) {
      if (fb->depth != nullptr) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, fb->depth->stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, 0, 0);
      }
      fb->depth = a.tex;
      if (a.tex) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, a.tex->stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, a.tex->id, 0);
      }
    }
    else {
      const int slot = i - 1;
      fb->color[slot] = a.tex;
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)slot, GL_TEXTURE_2D,
                             a.tex ? a.tex->id : 0, 0);
    }
  }
  fb->w = fb->h = 0;
  GPUTexture *size_tex = fb->depth;
  for (int i = 0; i < 8 && size_tex == nullptr; i++) {
    size_tex = fb->color[i];
  }
  if (size_tex) {
    fb->w = size_tex->w;
    fb->h = size_tex->h;
  }
  pg_fb_draw_buffers(fb);
  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    char b[64];
    std::snprintf(b, sizeof(b), "framebuffer incomplete 0x%x", status);
    set_error(b);
  }
  /* Restore the active binding. */
  glBindFramebuffer(GL_FRAMEBUFFER, g_active_fb ? g_active_fb->id : (GLuint)g_backbuffer);
}

void GPU_framebuffer_bind(GPUFrameBuffer *framebuffer)
{
  g_active_fb = framebuffer;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->id);
  pg_fb_draw_buffers(framebuffer);
  glViewport(0, 0, framebuffer->w, framebuffer->h);
  glScissor(0, 0, framebuffer->w, framebuffer->h);
}

GPUFrameBuffer *GPU_framebuffer_active_get()
{
  return g_active_fb;
}

void pg_gpu_framebuffer_set_backbuffer(int gl_fbo)
{
  g_backbuffer = gl_fbo;
  g_active_fb = nullptr;
}

/* The clears write through every write mask, as GLFrameBuffer::clear does. */
static void pg_clear_prepare(eGPUFrameBufferBits buffers)
{
  if (buffers & GPU_COLOR_BIT) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }
  if (buffers & GPU_DEPTH_BIT) {
    glDepthMask(GL_TRUE);
  }
  if (buffers & GPU_STENCIL_BIT) {
    glStencilMask(0xFF);
  }
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_RASTERIZER_DISCARD);
}

void GPU_framebuffer_clear(GPUFrameBuffer *fb,
                           eGPUFrameBufferBits buffers,
                           const float clear_col[4],
                           float clear_depth,
                           unsigned int clear_stencil)
{
  if (g_active_fb != fb) {
    GPU_framebuffer_bind(fb);
  }
  pg_clear_prepare(buffers);
  if (buffers & GPU_COLOR_BIT) {
    for (int i = 0; i < 8; i++) {
      if (fb->color[i]) {
        glClearBufferfv(GL_COLOR, i, clear_col);
      }
    }
  }
  if ((buffers & GPU_DEPTH_BIT) && (buffers & GPU_STENCIL_BIT)) {
    glClearBufferfi(GL_DEPTH_STENCIL, 0, clear_depth, (GLint)clear_stencil);
  }
  else if (buffers & GPU_DEPTH_BIT) {
    glClearBufferfv(GL_DEPTH, 0, &clear_depth);
  }
  else if (buffers & GPU_STENCIL_BIT) {
    const GLint s = (GLint)clear_stencil;
    glClearBufferiv(GL_STENCIL, 0, &s);
  }
}

void GPU_framebuffer_clear_color_depth(GPUFrameBuffer *fb, const float clear_col[4], float clear_depth)
{
  GPU_framebuffer_clear(fb, (eGPUFrameBufferBits)(GPU_COLOR_BIT | GPU_DEPTH_BIT), clear_col, clear_depth, 0);
}

void GPU_framebuffer_clear_depth_stencil(GPUFrameBuffer *fb, float clear_depth, unsigned int clear_stencil)
{
  GPU_framebuffer_clear(fb, (eGPUFrameBufferBits)(GPU_DEPTH_BIT | GPU_STENCIL_BIT), nullptr, clear_depth, clear_stencil);
}

void GPU_framebuffer_multi_clear(GPUFrameBuffer *fb, const float (*clear_colors)[4])
{
  if (g_active_fb != fb) {
    GPU_framebuffer_bind(fb);
  }
  pg_clear_prepare(GPU_COLOR_BIT);
  for (int i = 0; i < 8; i++) {
    if (fb->color[i]) {
      glClearBufferfv(GL_COLOR, i, clear_colors[i]);
    }
  }
}

void GPU_framebuffer_blit(GPUFrameBuffer *fb_read,
                          int read_slot,
                          GPUFrameBuffer *fb_write,
                          int write_slot,
                          eGPUFrameBufferBits blit_buffers)
{
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_read->id);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_write->id);
  GLbitfield mask = 0;
  if (blit_buffers & GPU_COLOR_BIT) {
    glReadBuffer(GL_COLOR_ATTACHMENT0 + (GLenum)read_slot);
    GLenum bufs[8] = {GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE};
    bufs[write_slot] = GL_COLOR_ATTACHMENT0 + (GLenum)write_slot;
    glDrawBuffers(write_slot + 1, bufs);
    mask |= GL_COLOR_BUFFER_BIT;
  }
  if (blit_buffers & GPU_DEPTH_BIT) {
    mask |= GL_DEPTH_BUFFER_BIT;
  }
  if (blit_buffers & GPU_STENCIL_BIT) {
    mask |= GL_STENCIL_BUFFER_BIT;
  }
  glBlitFramebuffer(0, 0, fb_read->w, fb_read->h, 0, 0, fb_write->w, fb_write->h, mask, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, g_active_fb ? g_active_fb->id : (GLuint)g_backbuffer);
  if (g_active_fb) {
    pg_fb_draw_buffers(g_active_fb);
  }
}

/** \} */
