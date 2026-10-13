/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * L8 part 2: the draw-manager calls Blender 3.6.23's Grease Pencil draw engine makes
 * (draw/engines/gpencil/*.c and draw/intern/draw_cache_impl_gpencil.cc, compiled UNMODIFIED), on
 * OpenGL ES 3.2, for Project Grease's 2D canvas. This is not Blender's draw manager: it records
 * passes, shading groups, uniforms and commands the way draw_manager_data.cc does and executes them
 * the way draw_manager_exec.c does, for the subset the GP engine uses.
 *
 * ADAPTED from draw_manager_exec.c (drw_state_set, draw_shgroup command loop, uniform order),
 * draw_manager_data.cc (DRW_shgroup_create_sub, DRW_shgroup_stencil_mask) and
 * gpu/opengl/gl_state.cc (GL state for a GPU state). Kept: the uniform application order
 * (newest first), sub-groups not inheriting uniforms (they rely on the GL program state), the
 * cached DRWState that skips redundant state changes, resource handle 0 = identity matrices.
 */
#include <GLES3/gl32.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_utildefines.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_layer_types.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_object.h"

#include "GPU_batch.h"
#include "GPU_framebuffer.h"
#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"
#include "GPU_vertex_buffer.h"

#include "DRW_render.h"
#include "draw_cache.h"
#include "draw_cache_impl.h"
#include "draw_shader_shared.h"
#include "draw_view_data.h"
#include "BLI_math_color.h"
#include "BKE_colorband.h"
#include "DNA_userdef_types.h"

#include "pg_gpu_gles.h"
void pg_gles_tex_buffer(GLenum target, GLenum format, GLuint buffer);
void pg_gles_draw_elements_base_vertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint base);
#include "project_grease_modifier_stack.h"

extern "C" DrawEngineType draw_engine_gpencil_type;

/* -------------------------------------------------------------------- */
/** \name Recorded passes (draw_manager_data.cc)
 * \{ */

namespace {

enum PGUniKind {
  UNI_INT,         /* copy, ivalue[length * arraysize] */
  UNI_FLOAT,       /* copy */
  UNI_INT_REF,     /* const int *, length 1 */
  UNI_FLOAT_REF,   /* const float *, length, arraysize */
  UNI_TEX,
  UNI_TEX_REF,
  UNI_BLOCK,
  UNI_BUFTEX,
};

struct PGUni {
  PGUniKind kind;
  int loc; /* uniform location, texture unit or block binding */
  int length = 1, arraysize = 1;
  int ivalue[16] = {};
  float fvalue[16] = {};
  const void *ptr = nullptr;
};

enum PGCmdKind { CMD_RANGE, CMD_PROCEDURAL, CMD_STATE, CMD_STENCIL, CMD_CLEAR };

struct PGCmd {
  PGCmdKind kind;
  GPUBatch *batch = nullptr;
  int handle = 0;
  uint first = 0, count = 0;
  DRWState enable = DRWState(0), disable = DRWState(0);
  uint write_mask = 0, ref = 0, comp_mask = 0;
  eGPUFrameBufferBits clear_bits = eGPUFrameBufferBits(0);
  float clear_col[4] = {}, clear_depth = 0;
  uint clear_stencil = 0;
};

}  // namespace

struct DRWShadingGroup {
  GPUShader *shader = nullptr;
  DRWPass *pass = nullptr;
  std::vector<PGUni> uniforms;
  std::vector<PGCmd> cmds;
};

struct DRWPass {
  char name[64] = {};
  DRWState state = DRWState(0);
  std::vector<DRWShadingGroup *> groups;
};

struct DRWView {
  int unused;
};

namespace {

struct PGFrame {
  std::vector<std::unique_ptr<DRWPass>> passes;
  std::vector<std::unique_ptr<DRWShadingGroup>> groups;
};

struct PGPoolTex {
  GPUTexture *tex;
  int w, h;
  eGPUTextureFormat format;
  bool used;
};

struct PGContext {
  bool initialized = false;
  /* Context state for the engine: the 2D canvas as Blender's viewport in Material Preview. */
  Scene scene;
  ViewLayer view_layer;
  View3D v3d;
  RegionView3D rv3d;
  ToolSettings toolsettings;
  Object ob;
  DRWContextState ctx;
  int cfra = 1;
  /* Viewport */
  float size[2] = {1, 1}, inv_size[2] = {1, 1};
  DefaultFramebufferList dfbl = {};
  DefaultTextureList dtxl = {};
  /* Engine data (DRWViewData / ViewportEngineData) */
  void *vedata_lists[5] = {};
  void *view_layer_engine_data = nullptr;
  void (*view_layer_engine_data_free)(void *) = nullptr;
  bool engine_initialized = false;
  /* Shared UBOs */
  GPUUniformBuf *view_ubo = nullptr, *matrices_ubo = nullptr, *infos_ubo = nullptr;
  float viewmat[4][4], viewinv[4][4], winmat[4][4], wininv[4][4], persmat[4][4], persinv[4][4];
  std::map<const Object *, int> handles;
  GLuint vao = 0;
  std::vector<GLuint> frame_buffer_textures; /* texture buffers of this frame */
  std::vector<PGPoolTex> pool;
  PGFrame frame;
  /* drw_state_set cache (DST.state) and the GPU mutable stencil state */
  DRWState state = DRWState(~0u);
  uint stencil_write_mask = 0, stencil_reference = 0, stencil_compare_mask = 0;
  eGPUStencilTest stencil_test = GPU_STENCIL_NONE;
  GLuint present_prog = 0;
  bool failed = false;
};

PGContext *G_pg = nullptr;

PGContext &pgc()
{
  if (G_pg == nullptr) {
    G_pg = new PGContext();
  }
  return *G_pg;
}

}  // namespace

static int uniform_loc(DRWShadingGroup *shgroup, const char *name)
{
  return pg_gpu_shader_uniform(shgroup->shader, name);
}

static void add_uni(DRWShadingGroup *shgroup, const PGUni &u)
{
  if (u.loc < 0) {
    return; /* as drw_shgroup_uniform_create_ex: unknown uniforms are ignored */
  }
  shgroup->uniforms.push_back(u);
}

DRWPass *DRW_pass_create(const char *name, DRWState state)
{
  PGContext &c = pgc();
  c.frame.passes.emplace_back(new DRWPass());
  DRWPass *pass = c.frame.passes.back().get();
  std::snprintf(pass->name, sizeof(pass->name), "%s", name);
  pass->state = state;
  return pass;
}

DRWShadingGroup *DRW_shgroup_create(GPUShader *shader, DRWPass *pass)
{
  PGContext &c = pgc();
  c.frame.groups.emplace_back(new DRWShadingGroup());
  DRWShadingGroup *g = c.frame.groups.back().get();
  g->shader = shader;
  g->pass = pass;
  pass->groups.push_back(g);
  return g;
}

DRWShadingGroup *DRW_shgroup_create_sub(DRWShadingGroup *shgroup)
{
  /* draw_manager_data.cc: a copy with no uniforms and no commands, inserted after the parent. */
  PGContext &c = pgc();
  c.frame.groups.emplace_back(new DRWShadingGroup());
  DRWShadingGroup *g = c.frame.groups.back().get();
  g->shader = shgroup->shader;
  g->pass = shgroup->pass;
  auto &v = shgroup->pass->groups;
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] == shgroup) {
      v.insert(v.begin() + (long)i + 1, g);
      return g;
    }
  }
  v.push_back(g);
  return g;
}

static void uni_int(DRWShadingGroup *g, const char *name, const int *v, int len, int arraysize)
{
  PGUni u{UNI_INT, uniform_loc(g, name)};
  u.length = len;
  u.arraysize = arraysize;
  std::memcpy(u.ivalue, v, sizeof(int) * (size_t)(len * arraysize));
  add_uni(g, u);
}

static void uni_float(DRWShadingGroup *g, const char *name, const float *v, int len)
{
  PGUni u{UNI_FLOAT, uniform_loc(g, name)};
  u.length = len;
  std::memcpy(u.fvalue, v, sizeof(float) * (size_t)len);
  add_uni(g, u);
}

void DRW_shgroup_uniform_int_copy(DRWShadingGroup *shgroup, const char *name, int value)
{
  uni_int(shgroup, name, &value, 1, 1);
}

void DRW_shgroup_uniform_bool_copy(DRWShadingGroup *shgroup, const char *name, bool value)
{
  const int ival = value;
  uni_int(shgroup, name, &ival, 1, 1);
}

void DRW_shgroup_uniform_float_copy(DRWShadingGroup *shgroup, const char *name, float value)
{
  uni_float(shgroup, name, &value, 1);
}

void DRW_shgroup_uniform_vec2_copy(DRWShadingGroup *shgroup, const char *name, const float *value)
{
  uni_float(shgroup, name, value, 2);
}

void DRW_shgroup_uniform_vec3_copy(DRWShadingGroup *shgroup, const char *name, const float *value)
{
  uni_float(shgroup, name, value, 3);
}

void DRW_shgroup_uniform_vec4_copy(DRWShadingGroup *shgroup, const char *name, const float *value)
{
  uni_float(shgroup, name, value, 4);
}

void DRW_shgroup_uniform_vec4(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
  PGUni u{UNI_FLOAT_REF, uniform_loc(shgroup, name)};
  u.length = 4;
  u.arraysize = arraysize;
  u.ptr = value;
  add_uni(shgroup, u);
}

void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize)
{
  PGUni u{UNI_INT_REF, uniform_loc(shgroup, name)};
  u.arraysize = arraysize;
  u.ptr = value;
  add_uni(shgroup, u);
}

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex)
{
  PGUni u{UNI_TEX, pg_gpu_shader_sampler_unit(shgroup->shader, name)};
  u.ptr = tex;
  add_uni(shgroup, u);
}

void DRW_shgroup_uniform_texture_ref(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex)
{
  PGUni u{UNI_TEX_REF, pg_gpu_shader_sampler_unit(shgroup->shader, name)};
  u.ptr = tex;
  add_uni(shgroup, u);
}

void DRW_shgroup_uniform_block_ex(DRWShadingGroup *shgroup, const char *name, const GPUUniformBuf *ubo)
{
  PGUni u{UNI_BLOCK, pg_gpu_shader_block_binding(shgroup->shader, name)};
  u.ptr = ubo;
  add_uni(shgroup, u);
}

void DRW_shgroup_buffer_texture(DRWShadingGroup *shgroup, const char *name, GPUVertBuf *vertex_buffer)
{
  PGUni u{UNI_BUFTEX, pg_gpu_shader_sampler_unit(shgroup->shader, name)};
  u.ptr = vertex_buffer;
  add_uni(shgroup, u);
}

static int resource_handle(const Object *ob)
{
  if (ob == nullptr) {
    return 0;
  }
  PGContext &c = pgc();
  auto it = c.handles.find(ob);
  return it == c.handles.end() ? 0 : it->second;
}

void DRW_shgroup_call_range(DRWShadingGroup *shgroup, Object *ob, GPUBatch *geom, uint v_sta, uint v_num)
{
  PGCmd cmd{CMD_RANGE};
  cmd.batch = geom;
  cmd.handle = resource_handle(ob);
  cmd.first = v_sta;
  cmd.count = v_num;
  shgroup->cmds.push_back(cmd);
}

void DRW_shgroup_call_procedural_triangles(DRWShadingGroup *shgroup, Object *ob, uint tri_count)
{
  PGCmd cmd{CMD_PROCEDURAL};
  cmd.handle = resource_handle(ob);
  cmd.count = tri_count * 3;
  shgroup->cmds.push_back(cmd);
}

void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state)
{
  PGCmd cmd{CMD_STATE};
  cmd.enable = state;
  shgroup->cmds.push_back(cmd);
}

void DRW_shgroup_state_disable(DRWShadingGroup *shgroup, DRWState state)
{
  PGCmd cmd{CMD_STATE};
  cmd.disable = state;
  shgroup->cmds.push_back(cmd);
}

void DRW_shgroup_stencil_mask(DRWShadingGroup *shgroup, uint mask)
{
  /* draw_manager_data.cc: drw_command_set_stencil_mask(shgroup, 0xFF, mask, 0xFF) */
  PGCmd cmd{CMD_STENCIL};
  cmd.write_mask = 0xFF;
  cmd.ref = mask;
  cmd.comp_mask = 0xFF;
  shgroup->cmds.push_back(cmd);
}

void DRW_shgroup_clear_framebuffer(DRWShadingGroup *shgroup,
                                   eGPUFrameBufferBits channels,
                                   uchar r,
                                   uchar g,
                                   uchar b,
                                   uchar a,
                                   float depth,
                                   uchar stencil)
{
  PGCmd cmd{CMD_CLEAR};
  cmd.clear_bits = channels;
  cmd.clear_col[0] = r / 255.0f;
  cmd.clear_col[1] = g / 255.0f;
  cmd.clear_col[2] = b / 255.0f;
  cmd.clear_col[3] = a / 255.0f;
  cmd.clear_depth = depth;
  cmd.clear_stencil = stencil;
  shgroup->cmds.push_back(cmd);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Execution (draw_manager_exec.c, gl_state.cc)
 * \{ */

static int trace_level()
{
  static int t = -1;
  if (t < 0) {
    const char *e = getenv("PG_DRW_TRACE");
    t = (e && e[0] >= '1' && e[0] <= '9') ? e[0] - '0' : 0;
  }
  return t;
}
/* PG_DRW_TRACE=1: passes; 2: also groups, uniforms and vertex buffers. */
static bool trace_enabled()
{
  return trace_level() >= 1;
}
static bool trace_detail()
{
  return trace_level() >= 2;
}

static void gl_blend(eGPUBlend value)
{
  /* GLStateManager::set_blend (gl_state.cc) */
  GLenum src_rgb, src_alpha, dst_rgb, dst_alpha;
  switch (value) {
    default:
    case GPU_BLEND_ALPHA:
      src_rgb = GL_SRC_ALPHA; dst_rgb = GL_ONE_MINUS_SRC_ALPHA; src_alpha = GL_ONE; dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    case GPU_BLEND_ALPHA_PREMULT:
      src_rgb = GL_ONE; dst_rgb = GL_ONE_MINUS_SRC_ALPHA; src_alpha = GL_ONE; dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    case GPU_BLEND_ADDITIVE:
      src_rgb = GL_SRC_ALPHA; dst_rgb = GL_ONE; src_alpha = GL_ZERO; dst_alpha = GL_ONE;
      break;
    case GPU_BLEND_SUBTRACT:
    case GPU_BLEND_ADDITIVE_PREMULT:
      src_rgb = GL_ONE; dst_rgb = GL_ONE; src_alpha = GL_ONE; dst_alpha = GL_ONE;
      break;
    case GPU_BLEND_MULTIPLY:
      src_rgb = GL_DST_COLOR; dst_rgb = GL_ZERO; src_alpha = GL_DST_ALPHA; dst_alpha = GL_ZERO;
      break;
    case GPU_BLEND_INVERT:
      src_rgb = GL_ONE_MINUS_DST_COLOR; dst_rgb = GL_ZERO; src_alpha = GL_ZERO; dst_alpha = GL_ONE;
      break;
    case GPU_BLEND_OIT:
      src_rgb = GL_ONE; dst_rgb = GL_ONE; src_alpha = GL_ZERO; dst_alpha = GL_ONE_MINUS_SRC_ALPHA;
      break;
    case GPU_BLEND_BACKGROUND:
      src_rgb = GL_ONE_MINUS_DST_ALPHA; dst_rgb = GL_SRC_ALPHA; src_alpha = GL_ZERO; dst_alpha = GL_SRC_ALPHA;
      break;
    case GPU_BLEND_ALPHA_UNDER_PREMUL:
      src_rgb = GL_ONE_MINUS_DST_ALPHA; dst_rgb = GL_ONE; src_alpha = GL_ONE_MINUS_DST_ALPHA; dst_alpha = GL_ONE;
      break;
    case GPU_BLEND_CUSTOM:
      /* shortcut: GLES 3.2 has no dual-source blending (GL_SRC1_COLOR needs
       * EXT_blend_func_extended); the GP engine never uses DRW_STATE_BLEND_CUSTOM. */
      src_rgb = GL_ONE; dst_rgb = GL_ONE; src_alpha = GL_ONE; dst_alpha = GL_ONE;
      break;
  }
  glBlendEquation(value == GPU_BLEND_SUBTRACT ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
  glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
  if (value != GPU_BLEND_NONE) {
    glEnable(GL_BLEND);
  }
  else {
    glDisable(GL_BLEND);
  }
}

static void gl_apply_stencil_mask()
{
  /* GLStateManager::set_stencil_mask */
  PGContext &c = pgc();
  GLenum func;
  switch (c.stencil_test) {
    case GPU_STENCIL_NEQUAL: func = GL_NOTEQUAL; break;
    case GPU_STENCIL_EQUAL: func = GL_EQUAL; break;
    case GPU_STENCIL_ALWAYS: func = GL_ALWAYS; break;
    case GPU_STENCIL_NONE:
    default:
      glStencilMask(0x00);
      glStencilFunc(GL_ALWAYS, 0x00, 0x00);
      return;
  }
  glStencilMask(c.stencil_write_mask);
  glStencilFunc(func, (GLint)c.stencil_reference, c.stencil_compare_mask);
}

static void drw_state_set(DRWState state)
{
  PGContext &c = pgc();
  if (c.state == state) {
    return;
  }
  /* write mask (GLStateManager::set_write_mask) */
  const bool wdepth = (state & DRW_STATE_WRITE_DEPTH) != 0;
  const bool wcolor = (state & DRW_STATE_WRITE_COLOR) != 0;
  const bool wstencil = (state & DRW_STATE_WRITE_STENCIL_ENABLED) != 0;
  glDepthMask(wdepth);
  glColorMask(wcolor, wcolor, wcolor, wcolor);
  if (!wdepth && !wcolor && !wstencil) {
    glEnable(GL_RASTERIZER_DISCARD);
  }
  else {
    glDisable(GL_RASTERIZER_DISCARD);
  }
  /* culling */
  switch (state & (DRW_STATE_CULL_BACK | DRW_STATE_CULL_FRONT)) {
    case DRW_STATE_CULL_BACK: glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
    case DRW_STATE_CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
    default: glDisable(GL_CULL_FACE); break;
  }
  /* depth test */
  GLenum depth_func = GL_ALWAYS;
  bool depth_test = true;
  switch (state & DRW_STATE_DEPTH_TEST_ENABLED) {
    case DRW_STATE_DEPTH_LESS: depth_func = GL_LESS; break;
    case DRW_STATE_DEPTH_LESS_EQUAL: depth_func = GL_LEQUAL; break;
    case DRW_STATE_DEPTH_EQUAL: depth_func = GL_EQUAL; break;
    case DRW_STATE_DEPTH_GREATER: depth_func = GL_GREATER; break;
    case DRW_STATE_DEPTH_GREATER_EQUAL: depth_func = GL_GEQUAL; break;
    case DRW_STATE_DEPTH_ALWAYS: depth_func = GL_ALWAYS; break;
    default: depth_test = false; break;
  }
  if (depth_test) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(depth_func);
  }
  else {
    glDisable(GL_DEPTH_TEST);
  }
  /* stencil op (+ GPU_stencil_write_mask_set) */
  switch (state & DRW_STATE_WRITE_STENCIL_ENABLED) {
    case DRW_STATE_WRITE_STENCIL:
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      c.stencil_write_mask = 0xFF;
      break;
    case DRW_STATE_WRITE_STENCIL_SHADOW_PASS:
      glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
      glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
      c.stencil_write_mask = 0xFF;
      break;
    case DRW_STATE_WRITE_STENCIL_SHADOW_FAIL:
      glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
      glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
      c.stencil_write_mask = 0xFF;
      break;
    default:
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      c.stencil_write_mask = 0x00;
      break;
  }
  switch (state & DRW_STATE_STENCIL_TEST_ENABLED) {
    case DRW_STATE_STENCIL_ALWAYS: c.stencil_test = GPU_STENCIL_ALWAYS; break;
    case DRW_STATE_STENCIL_EQUAL: c.stencil_test = GPU_STENCIL_EQUAL; break;
    case DRW_STATE_STENCIL_NEQUAL: c.stencil_test = GPU_STENCIL_NEQUAL; break;
    default: c.stencil_test = GPU_STENCIL_NONE; break;
  }
  if (c.stencil_test != GPU_STENCIL_NONE) {
    glEnable(GL_STENCIL_TEST);
  }
  else {
    glDisable(GL_STENCIL_TEST);
  }
  gl_apply_stencil_mask();
  /* blend */
  eGPUBlend blend;
  switch (state & DRW_STATE_BLEND_ENABLED) {
    case DRW_STATE_BLEND_ADD: blend = GPU_BLEND_ADDITIVE; break;
    case DRW_STATE_BLEND_ADD_FULL: blend = GPU_BLEND_ADDITIVE_PREMULT; break;
    case DRW_STATE_BLEND_ALPHA: blend = GPU_BLEND_ALPHA; break;
    case DRW_STATE_BLEND_ALPHA_PREMUL: blend = GPU_BLEND_ALPHA_PREMULT; break;
    case DRW_STATE_BLEND_BACKGROUND: blend = GPU_BLEND_BACKGROUND; break;
    case DRW_STATE_BLEND_OIT: blend = GPU_BLEND_OIT; break;
    case DRW_STATE_BLEND_MUL: blend = GPU_BLEND_MULTIPLY; break;
    case DRW_STATE_BLEND_SUB: blend = GPU_BLEND_SUBTRACT; break;
    case DRW_STATE_BLEND_CUSTOM: blend = GPU_BLEND_CUSTOM; break;
    case DRW_STATE_LOGIC_INVERT: blend = GPU_BLEND_INVERT; break;
    case DRW_STATE_BLEND_ALPHA_UNDER_PREMUL: blend = GPU_BLEND_ALPHA_UNDER_PREMUL; break;
    default: blend = GPU_BLEND_NONE; break;
  }
  gl_blend(blend);
  c.state = state;
}

static void apply_uniforms(DRWShadingGroup *g)
{
  PGContext &c = pgc();
  /* draw_update_uniforms: in reverse order. */
  for (auto it = g->uniforms.rbegin(); it != g->uniforms.rend(); ++it) {
    const PGUni &u = *it;
    switch (u.kind) {
      case UNI_INT:
        switch (u.length) {
          case 1: glUniform1iv(u.loc, u.arraysize, u.ivalue); break;
          case 2: glUniform2iv(u.loc, u.arraysize, u.ivalue); break;
          case 3: glUniform3iv(u.loc, u.arraysize, u.ivalue); break;
          default: glUniform4iv(u.loc, u.arraysize, u.ivalue); break;
        }
        break;
      case UNI_FLOAT:
        switch (u.length) {
          case 1: glUniform1fv(u.loc, 1, u.fvalue); break;
          case 2: glUniform2fv(u.loc, 1, u.fvalue); break;
          case 3: glUniform3fv(u.loc, 1, u.fvalue); break;
          default: glUniform4fv(u.loc, 1, u.fvalue); break;
        }
        break;
      case UNI_INT_REF:
        glUniform1iv(u.loc, u.arraysize, (const GLint *)u.ptr);
        break;
      case UNI_FLOAT_REF:
        glUniform4fv(u.loc, u.arraysize, (const GLfloat *)u.ptr);
        break;
      case UNI_TEX:
        pg_gpu_texture_bind((const GPUTexture *)u.ptr, u.loc);
        break;
      case UNI_TEX_REF:
        pg_gpu_texture_bind(*(GPUTexture *const *)u.ptr, u.loc);
        break;
      case UNI_BLOCK:
        pg_gpu_uniformbuf_bind((const GPUUniformBuf *)u.ptr, u.loc);
        break;
      case UNI_BUFTEX: {
        /* GPU_vertbuf_bind_as_texture: the GP buffers are 4 x 32-bit per attribute (RGBA32F). */
        GLuint buf = pg_android_vertbuf_gl((GPUVertBuf *)u.ptr);
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glActiveTexture(GL_TEXTURE0 + (GLenum)u.loc);
        glBindTexture(GL_TEXTURE_BUFFER, tex);
        pg_gles_tex_buffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buf);
        c.frame_buffer_textures.push_back(tex);
        break;
      }
    }
  }
}

static void draw_command_once(DRWShadingGroup *g, const PGCmd &cmd);

static void draw_command(DRWShadingGroup *g, const PGCmd &cmd)
{
  PGContext &c = pgc();
  const int dual = pg_gpu_shader_uniform(g->shader, "pgDualSourcePass");
  if (dual >= 0 && (c.state & DRW_STATE_BLEND_ENABLED) == DRW_STATE_BLEND_CUSTOM) {
    /* GPU_BLEND_CUSTOM (ONE, SRC1_COLOR / ONE, SRC1_ALPHA) without dual-source blending: the
     * generated shader outputs SRC_1 then SRC_0 (tools/gles_shaders, gles_dual_source). */
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glUniform1i(dual, 1);
    glBlendFuncSeparate(GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_SRC_ALPHA); /* dst *= SRC_1 */
    draw_command_once(g, cmd);
    glUniform1i(dual, 0);
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE); /* dst += SRC_0 */
    draw_command_once(g, cmd);
    c.state = DRWState(~0u); /* blend function changed outside drw_state_set */
    return;
  }
  draw_command_once(g, cmd);
}

static void draw_command_once(DRWShadingGroup *g, const PGCmd &cmd)
{
  PGContext &c = pgc();
  const GPUShader *sh = g->shader;
  const int rid = pg_gpu_shader_uniform(sh, "drw_ResourceID");
  if (rid >= 0) {
    glUniform1i(rid, cmd.handle);
  }
  const int chunk = pg_gpu_shader_uniform(sh, "drw_resourceChunk");
  if (chunk >= 0) {
    glUniform1i(chunk, 0);
  }
  /* draw_call_single_do draws with the resource index as first instance; without base-instance
   * support the GL backend passes it as gpu_BaseInstance (resource_id = gpu_InstanceIndex). */
  const int base = pg_gpu_shader_uniform(sh, "gpu_BaseInstance");
  if (base >= 0) {
    glUniform1i(base, cmd.handle);
  }
  glBindVertexArray(c.vao);
  if (cmd.kind == CMD_PROCEDURAL) {
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)cmd.count);
    return;
  }
  unsigned int ibo, type, start, base_index, len;
  if (!pg_android_batch_elem(cmd.batch, &ibo, &type, &start, &base_index, &len)) {
    return;
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  /* GLBatch::draw: offset (v_first + index_start) * index size, base vertex = index_base. */
  const size_t isize = type == GL_UNSIGNED_SHORT ? 2 : 4;
  const void *ofs = (const void *)(uintptr_t)((cmd.first + start) * isize);
  pg_gles_draw_elements_base_vertex(GL_TRIANGLES, (GLsizei)cmd.count, type, ofs, (GLint)base_index);
}

static void draw_shgroup(DRWShadingGroup *g, DRWState pass_state)
{
  PGContext &c = pgc();
  const GLuint prog = pg_gpu_shader_program(g->shader);
  if (prog == 0) {
    c.failed = true;
    return;
  }
  glUseProgram(prog);
  /* The draw manager's own blocks: view, object matrices, object infos. */
  pg_gpu_uniformbuf_bind(c.view_ubo, pg_gpu_shader_block_binding(g->shader, "drw_view_"));
  pg_gpu_uniformbuf_bind(c.matrices_ubo, pg_gpu_shader_block_binding(g->shader, "drw_matrices"));
  pg_gpu_uniformbuf_bind(c.infos_ubo, pg_gpu_shader_block_binding(g->shader, "drw_infos"));
  apply_uniforms(g);
  if (trace_detail()) {
    std::printf("[drw]   group prog %u uniforms %zu cmds %zu\n", prog, g->uniforms.size(), g->cmds.size());
    for (const PGCmd &cmd : g->cmds) {
      if (cmd.kind == CMD_RANGE) {
        std::printf("[drw]     range first %u count %u handle %d\n", cmd.first, cmd.count, cmd.handle);
      }
    }
  }
  drw_state_set(pass_state);
  if (trace_detail()) {
    static const char *names[] = {"gpThicknessScale", "gpThicknessWorldScale", "gpThicknessOffset",
                                  "viewportSize", "gpLayerOpacity", "gpVertexColorOpacity", "gpStrokeIndexOffset"};
    for (const char *n : names) {
      const int loc = pg_gpu_shader_uniform(g->shader, n);
      if (loc >= 0) {
        float v[4] = {};
        glGetUniformfv(prog, loc, v);
        std::printf("[drw]     %s = %g %g\n", n, v[0], v[1]);
      }
    }
    for (const PGUni &u : g->uniforms) {
      if (u.kind == UNI_BUFTEX) {
        GPUVertBuf *vb = (GPUVertBuf *)u.ptr;
        const float *d = (const float *)GPU_vertbuf_get_data(vb);
        const uint n = GPU_vertbuf_get_vertex_len(vb);
        std::printf("[drw]     buffer texture unit %d: %u verts, stride %u\n", u.loc, n,
                    GPU_vertbuf_get_format(vb)->stride);
        for (uint i = 0; d && i < n && i < 5; i++) {
          const float *r = d + i * (GPU_vertbuf_get_format(vb)->stride / 4);
          std::printf("[drw]       v%u: %g %g %g %g | %08x %08x %08x %08x\n", i, r[0], r[1], r[2], r[3],
                      ((const uint *)r)[4], ((const uint *)r)[5], ((const uint *)r)[6], ((const uint *)r)[7]);
        }
      }
    }
  }
  DRWState enabled = DRWState(0), disabled = DRWState(0);
  for (const PGCmd &cmd : g->cmds) {
    switch (cmd.kind) {
      case CMD_CLEAR:
        GPU_framebuffer_clear(GPU_framebuffer_active_get(), cmd.clear_bits, cmd.clear_col, cmd.clear_depth,
                              cmd.clear_stencil);
        c.state = DRWState(~0u); /* the clear changed the masks */
        drw_state_set((pass_state & ~disabled) | enabled);
        break;
      case CMD_STATE:
        enabled = enabled | cmd.enable;
        disabled = disabled | cmd.disable;
        drw_state_set((pass_state & ~disabled) | enabled);
        break;
      case CMD_STENCIL:
        c.stencil_write_mask = cmd.write_mask;
        c.stencil_reference = cmd.ref;
        c.stencil_compare_mask = cmd.comp_mask;
        gl_apply_stencil_mask();
        break;
      case CMD_RANGE:
      case CMD_PROCEDURAL:
        draw_command(g, cmd);
        break;
    }
  }
}

void DRW_draw_pass(DRWPass *pass)
{
  PGContext &c = pgc();
  if (trace_enabled() && pass) {
    int draws = 0;
    for (DRWShadingGroup *g : pass->groups) {
      for (const PGCmd &cmd : g->cmds) {
        draws += (cmd.kind == CMD_RANGE || cmd.kind == CMD_PROCEDURAL) ? 1 : 0;
      }
    }
    GLint fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    std::printf("[drw] pass %-22s state 0x%llx groups %zu draws %d fbo %d prog %u\n", pass->name,
                (unsigned long long)pass->state, pass->groups.size(), draws, fbo,
                pass->groups.empty() ? 0u : pg_gpu_shader_program(pass->groups[0]->shader));
  }
  if (pass == nullptr || pass->groups.empty()) {
    return;
  }
  /* GPU_framebuffer_clear calls can change the state outside the DRW module. */
  c.state = DRWState(~0u);
  drw_state_set(pass->state);
  for (DRWShadingGroup *g : pass->groups) {
    draw_shgroup(g, pass->state);
  }
  if (trace_enabled()) {
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
      std::printf("[drw]   GL error 0x%x after %s\n", err, pass->name);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context, viewport, view
 * \{ */

extern "C" int pg_drw_cfra(void)
{
  return pgc().cfra;
}

const DRWContextState *DRW_context_state_get(void)
{
  return &pgc().ctx;
}

bool DRW_state_is_fbo(void)
{
  return true;
}

bool DRW_state_is_image_render(void)
{
  return false;
}

void DRW_stats_group_start(const char * /*name*/) {}
void DRW_stats_group_end(void) {}

const float *DRW_viewport_size_get(void)
{
  return pgc().size;
}

const float *DRW_viewport_invert_size_get(void)
{
  return pgc().inv_size;
}

DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void)
{
  return &pgc().dfbl;
}

DefaultTextureList *DRW_viewport_texture_list_get(void)
{
  return &pgc().dtxl;
}

void DRW_view_viewmat_get(const DRWView * /*view*/, float mat[4][4], bool inverse)
{
  copy_m4_m4(mat, inverse ? pgc().viewinv : pgc().viewmat);
}

void DRW_view_winmat_get(const DRWView * /*view*/, float mat[4][4], bool inverse)
{
  copy_m4_m4(mat, inverse ? pgc().wininv : pgc().winmat);
}

void DRW_view_persmat_get(const DRWView * /*view*/, float mat[4][4], bool inverse)
{
  copy_m4_m4(mat, inverse ? pgc().persinv : pgc().persmat);
}

bool DRW_view_is_persp_get(const DRWView * /*view*/)
{
  return false;
}

int DRW_object_visibility_in_active_context(const Object * /*ob*/)
{
  return OB_VISIBLE_SELF;
}

void **DRW_view_layer_engine_data_ensure(DrawEngineType * /*engine_type*/, void (*callback)(void *storage))
{
  PGContext &c = pgc();
  c.view_layer_engine_data_free = callback;
  return &c.view_layer_engine_data;
}

void DRW_shader_free(GPUShader *shader)
{
  GPU_shader_free(shader);
}

GPUBatch *DRW_gpencil_dummy_buffer_get(void)
{
  /* Only the sbuffer (stroke being drawn) batches use it; pg_gp_engine_draw draws with no sbuffer. */
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Textures (draw_manager_texture.c, draw_texture_pool.cc)
 * \{ */

static bool is_filterable_color(eGPUTextureFormat f)
{
  return !ELEM(f, GPU_DEPTH24_STENCIL8, GPU_DEPTH_COMPONENT24, GPU_DEPTH32F_STENCIL8, GPU_RGBA32F, GPU_R32F);
}

static void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags)
{
  GPU_texture_filter_mode(tex, (flags & DRW_TEX_FILTER) != 0);
  pg_gpu_texture_wrap_mode(tex, (flags & DRW_TEX_WRAP) != 0);
}

GPUTexture *DRW_texture_create_2d(int w, int h, eGPUTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
  GPUTexture *tex = GPU_texture_create_2d(__func__, w, h, 1, format, GPU_TEXTURE_USAGE_GENERAL, fpixels);
  drw_texture_set_parameters(tex, flags);
  return tex;
}

void DRW_texture_ensure_2d(GPUTexture **tex, int w, int h, eGPUTextureFormat format, DRWTextureFlag flags)
{
  if (*tex == nullptr) {
    *tex = DRW_texture_create_2d(w, h, format, flags, nullptr);
  }
}

void DRW_texture_free(GPUTexture *tex)
{
  GPU_texture_free(tex);
}

GPUTexture *DRW_texture_pool_query_2d_ex(
    int w, int h, eGPUTextureFormat format, eGPUTextureUsage /*usage*/, DrawEngineType * /*engine_type*/)
{
  PGContext &c = pgc();
  for (PGPoolTex &p : c.pool) {
    if (!p.used && p.w == w && p.h == h && p.format == format) {
      p.used = true;
      return p.tex;
    }
  }
  GPUTexture *tex = GPU_texture_create_2d("pool", w, h, 1, format, GPU_TEXTURE_USAGE_GENERAL, nullptr);
  /* Pool textures are filtered for colour formats (DRW_texture_pool_query_2d contract). */
  GPU_texture_filter_mode(tex, is_filterable_color(format));
  c.pool.push_back({tex, w, h, format, true});
  return tex;
}

GPUTexture *DRW_texture_pool_query_2d(int w, int h, eGPUTextureFormat format, DrawEngineType *engine_type)
{
  return DRW_texture_pool_query_2d_ex(w, h, format, GPU_TEXTURE_USAGE_GENERAL, engine_type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Project Grease entry
 * \{ */

static void context_init(PGContext &c)
{
  std::memset(&c.scene, 0, sizeof(c.scene));
  std::memcpy(&c.scene, DNA_struct_default_get(Scene), sizeof(Scene));
  std::memset(&c.toolsettings, 0, sizeof(c.toolsettings));
  c.scene.toolsettings = &c.toolsettings;
  c.scene.world = nullptr;
  std::memset(&c.view_layer, 0, sizeof(c.view_layer));
  std::snprintf(c.view_layer.name, sizeof(c.view_layer.name), "ViewLayer");
  std::memcpy(&c.v3d, DNA_struct_default_get(View3D), sizeof(View3D));
  /* Material Preview: no solid-mode colour overrides, materials and textures as authored. */
  c.v3d.shading.type = OB_MATERIAL;
  c.v3d.shading.flag &= ~(V3D_SHADING_SCENE_LIGHTS | V3D_SHADING_SCENE_WORLD);
  std::memset(&c.rv3d, 0, sizeof(c.rv3d));
  c.rv3d.persp = RV3D_ORTHO;
  std::memset(&c.ob, 0, sizeof(c.ob));
  c.ob.type = OB_GPENCIL_LEGACY;
  c.ob.dt = OB_TEXTURE;
  std::snprintf(c.ob.id.name, sizeof(c.ob.id.name), "OBProject Grease");
  unit_m4(c.ob.object_to_world);
  unit_m4(c.ob.world_to_object);
  std::memset(&c.ctx, 0, sizeof(c.ctx));
  c.ctx.scene = &c.scene;
  c.ctx.view_layer = &c.view_layer;
  c.ctx.v3d = &c.v3d;
  c.ctx.rv3d = &c.rv3d;
  c.ctx.obact = &c.ob;
  c.ctx.object_mode = OB_MODE_OBJECT;
  c.ctx.depsgraph = nullptr;
  const DrawEngineDataSize *sz = draw_engine_gpencil_type.vedata_size;
  const int lens[4] = {sz->fbl_len, sz->txl_len, sz->psl_len, sz->stl_len};
  c.vedata_lists[0] = &draw_engine_gpencil_type;
  for (int i = 0; i < 4; i++) {
    c.vedata_lists[i + 1] = MEM_callocN(sizeof(void *) * (size_t)(lens[i] > 0 ? lens[i] : 1), "pg vedata list");
  }
  /* Shared UBOs: resource 0 is the identity (calls without an object), 1 the GP object. */
  ViewMatrices vm = {};
  c.view_ubo = GPU_uniformbuf_create_ex(sizeof(ViewMatrices), &vm, "drw_view");
  std::vector<ObjectMatrices> mats(DRW_RESOURCE_CHUNK_LEN);
  for (ObjectMatrices &m : mats) {
    unit_m4((float(*)[4])&m.model);
    unit_m4((float(*)[4])&m.model_inverse);
  }
  std::memcpy(&mats[1].model, c.ob.object_to_world, sizeof(float[4][4]));
  std::memcpy(&mats[1].model_inverse, c.ob.world_to_object, sizeof(float[4][4]));
  c.matrices_ubo = GPU_uniformbuf_create_ex(sizeof(ObjectMatrices) * mats.size(), mats.data(), "drw_matrices");
  std::vector<ObjectInfos> infos(DRW_RESOURCE_CHUNK_LEN);
  std::memset(infos.data(), 0, sizeof(ObjectInfos) * infos.size());
  c.infos_ubo = GPU_uniformbuf_create_ex(sizeof(ObjectInfos) * infos.size(), infos.data(), "drw_infos");
  c.handles[&c.ob] = 1;
  /* As the draw manager registers them (DRW_engines_register). */
  BKE_gpencil_batch_cache_dirty_tag_cb = DRW_gpencil_batch_cache_dirty_tag;
  BKE_gpencil_batch_cache_free_cb = DRW_gpencil_batch_cache_free;
  glGenVertexArrays(1, &c.vao);
  c.initialized = true;
}

static void viewport_ensure(PGContext &c, int w, int h)
{
  if (c.dtxl.color && GPU_texture_width(c.dtxl.color) == w && GPU_texture_height(c.dtxl.color) == h) {
    return;
  }
  GPU_FRAMEBUFFER_FREE_SAFE(c.dfbl.default_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(c.dfbl.depth_only_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(c.dfbl.color_only_fb);
  DRW_TEXTURE_FREE_SAFE(c.dtxl.color);
  DRW_TEXTURE_FREE_SAFE(c.dtxl.depth);
  /* DRWViewport: colour RGBA16F (scene linear), depth 24 + stencil 8. */
  c.dtxl.color = GPU_texture_create_2d("viewport_color", w, h, 1, GPU_RGBA16F, GPU_TEXTURE_USAGE_GENERAL, nullptr);
  c.dtxl.depth = GPU_texture_create_2d("viewport_depth", w, h, 1, GPU_DEPTH24_STENCIL8, GPU_TEXTURE_USAGE_GENERAL, nullptr);
  GPU_framebuffer_ensure_config(&c.dfbl.default_fb,
                                {GPU_ATTACHMENT_TEXTURE(c.dtxl.depth), GPU_ATTACHMENT_TEXTURE(c.dtxl.color)});
  GPU_framebuffer_ensure_config(&c.dfbl.depth_only_fb, {GPU_ATTACHMENT_TEXTURE(c.dtxl.depth), GPU_ATTACHMENT_NONE});
  GPU_framebuffer_ensure_config(&c.dfbl.color_only_fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(c.dtxl.color)});
}

static void view_set(PGContext &c, const PGGpEngineView *v)
{
  /* World = canvas pixels, y down. View: y flipped to Blender's y up. Window: orthographic, the
   * canvas at origin / scale in the viewport (as the Android presenter maps it). */
  const float W = (float)v->viewport_w, H = (float)v->viewport_h;
  /* Two-finger view rotation: a turn of the canvas about the canvas point at the viewport centre
   * (screen y down, as the presenter), in the view matrix so the window matrix (stroke thickness,
   * ProjectionMatrix[1][1]) is unchanged. */
  const float cs = std::cos(v->rotation), sn = std::sin(v->rotation);
  const float cx = (0.5f * W - v->origin_x) / v->scale, cy = (0.5f * H - v->origin_y) / v->scale;
  unit_m4(c.viewmat);
  c.viewmat[0][0] = cs;
  c.viewmat[0][1] = -sn; /* row 1 is negated: the y flip to Blender's y up */
  c.viewmat[1][0] = -sn;
  c.viewmat[1][1] = -cs;
  c.viewmat[3][0] = cx - (cs * cx - sn * cy);
  c.viewmat[3][1] = -(cy - (sn * cx + cs * cy));
  invert_m4_m4(c.viewinv, c.viewmat);
  unit_m4(c.winmat);
  c.winmat[0][0] = 2.0f * v->scale / W;
  c.winmat[1][1] = 2.0f * v->scale / H;
  c.winmat[2][2] = -0.001f; /* clip -1000 .. 1000 px */
  c.winmat[3][0] = 2.0f * v->origin_x / W - 1.0f;
  c.winmat[3][1] = 1.0f - 2.0f * v->origin_y / H;
  invert_m4_m4(c.wininv, c.winmat);
  mul_m4_m4m4(c.persmat, c.winmat, c.viewmat);
  invert_m4_m4(c.persinv, c.persmat);
  ViewMatrices vm;
  std::memcpy(&vm.viewmat, c.viewmat, sizeof(float[4][4]));
  std::memcpy(&vm.viewinv, c.viewinv, sizeof(float[4][4]));
  std::memcpy(&vm.winmat, c.winmat, sizeof(float[4][4]));
  std::memcpy(&vm.wininv, c.wininv, sizeof(float[4][4]));
  GPU_uniformbuf_update(c.view_ubo, &vm);
  c.size[0] = W;
  c.size[1] = H;
  c.inv_size[0] = 1.0f / W;
  c.inv_size[1] = 1.0f / H;
}

static void frame_end(PGContext &c)
{
  if (!c.frame_buffer_textures.empty()) {
    glDeleteTextures((GLsizei)c.frame_buffer_textures.size(), c.frame_buffer_textures.data());
    c.frame_buffer_textures.clear();
  }
  glBindVertexArray(0);
  glUseProgram(0);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_RASTERIZER_DISCARD);
  glDepthMask(GL_TRUE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilMask(0xFF);
  c.state = DRWState(~0u);
}


/* ---- Weight Paint view: Blender's overlay (overlay_gpencil.cc) edit lines and points in weight
 * colours (doWeightColor), drawn over the engine image with Blender's overlay_edit_gpencil shaders. */

/* BEGIN VERBATIM source/blender/draw/intern/draw_common.c (the default ramp branch) */
static void DRW_evaluate_weight_to_color(const float weight, float result[4])
{
  if (U.flag & USER_CUSTOM_RANGE) {
    BKE_colorband_evaluate(&U.coba_weight, weight, result);
  }
  else {
    /* Use gamma correction to even out the color bands:
     * increasing widens yellow/cyan vs red/green/blue.
     * Gamma 1.0 produces the original 2.79 color ramp. */
    const float gamma = 1.5f;
    const float hsv[3] = {(2.0f / 3.0f) * (1.0f - weight), 1.0f, pow(0.5f + 0.5f * weight, gamma)};

    hsv_to_rgb_v(hsv, result);

    for (int i = 0; i < 3; i++) {
      result[i] = pow(result[i], 1.0f / gamma);
    }
  }
}
/* END VERBATIM */

static void draw_weight_overlay(PGContext &c, bGPdata *gpd, int cfra, int group)
{
  static GPUTexture *ramp = nullptr;
  static GPUUniformBuf *globals = nullptr;
  if (ramp == nullptr) {
    /* ADAPTED: DRW_create_weight_colorramp_texture is GPU_SRGB8_A8 1D: here the decoded (linear)
     * values in a 256x1 2D texture, sampled the same. */
    float pixels[256][4];
    for (int i = 0; i < 256; i++) {
      DRW_evaluate_weight_to_color(i / 255.0f, pixels[i]);
      for (int k = 0; k < 3; k++) pixels[i][k] = srgb_to_linearrgb(pixels[i][k]);
      pixels[i][3] = 1.0f;
    }
    ramp = GPU_texture_create_2d("weight_color_ramp", 256, 1, 1, GPU_RGBA16F, GPU_TEXTURE_USAGE_SHADER_READ, pixels[0]);
    /* ADAPTED: DRW_globals_update without the theme API: the default theme's gp_vertex (black),
     * gp_vertex_select (0xff8500) and gp_vertex_size 3, at U.pixelsize 2 for a touch screen. */
    GlobalsUboStorage gb;
    std::memset(&gb, 0, sizeof(gb));
    const float vert[4] = {0, 0, 0, 1}, sel[4] = {1.0f, 0x85 / 255.0f, 0, 1};
    std::memcpy(&gb.color_gpencil_vertex, vert, sizeof(vert));
    std::memcpy(&gb.color_gpencil_vertex_select, sel, sizeof(sel));
    gb.size_vertex_gpencil = 2.0f * 3.0f;
    globals = GPU_uniformbuf_create_ex(sizeof(gb), &gb, "globalsBlock");
  }
  const short active = gpd->vertex_group_active_index;
  gpd->vertex_group_active_index = (short)(group + 1);
  GPUBatch *lines = DRW_cache_gpencil_edit_lines_get(&c.ob, cfra);
  GPUBatch *points = DRW_cache_gpencil_edit_points_get(&c.ob, cfra);
  gpd->vertex_group_active_index = active;
  GpencilBatchCache *cache = static_cast<GpencilBatchCache *>(gpd->runtime.gpencil_cache);
  (void)cache;
  if (lines == nullptr || points == nullptr || lines->verts[0] == nullptr || lines->verts[1] == nullptr) return;
  const int vert_len = (int)GPU_vertbuf_get_vertex_len(lines->verts[0]);
  if (vert_len <= 0) return;
  GPU_framebuffer_bind(c.dfbl.default_fb);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  static GLuint vao = 0;
  if (!vao) glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  /* gpencil_stroke_format: pos vec4, ma ivec4, uv vec4; gpencil_edit_stroke_format: vflag, weight */
  glBindBuffer(GL_ARRAY_BUFFER, pg_android_vertbuf_gl(lines->verts[0]));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 48, (const void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribIPointer(1, 1, GL_INT, 48, (const void *)16);
  glBindBuffer(GL_ARRAY_BUFFER, pg_android_vertbuf_gl(lines->verts[1]));
  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 8, (const void *)0);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 8, (const void *)4);
  const bool show_lines = (c.v3d.gp_flag & V3D_GP_SHOW_EDIT_LINES) != 0;
  for (int pass = show_lines ? 0 : 1; pass < 2; pass++) {
    GPUShader *sh = GPU_shader_create_from_info_name(pass == 0 ? "overlay_edit_gpencil_wire" : "overlay_edit_gpencil_point");
    if (sh == nullptr) { c.failed = true; break; }
    glUseProgram(pg_gpu_shader_program(sh));
    pg_gpu_uniformbuf_bind(c.view_ubo, pg_gpu_shader_block_binding(sh, "drw_view_"));
    pg_gpu_uniformbuf_bind(c.matrices_ubo, pg_gpu_shader_block_binding(sh, "drw_matrices"));
    pg_gpu_uniformbuf_bind(c.infos_ubo, pg_gpu_shader_block_binding(sh, "drw_infos"));
    pg_gpu_uniformbuf_bind(globals, pg_gpu_shader_block_binding(sh, "globalsBlock"));
    pg_gpu_texture_bind(ramp, pg_gpu_shader_sampler_unit(sh, "weightTex"));
    auto i1 = [&](const char *n, int v) { const int l = pg_gpu_shader_uniform(sh, n); if (l >= 0) glUniform1i(l, v); };
    auto f1 = [&](const char *n, float v) { const int l = pg_gpu_shader_uniform(sh, n); if (l >= 0) glUniform1f(l, v); };
    i1("doMultiframe", 0);
    i1("doStrokeEndpoints", 0);
    i1("hideSelect", 0);
    i1("doWeightColor", 1);
    i1("drw_ResourceID", 0);
    i1("drw_resourceChunk", 0);
    i1("gpu_BaseInstance", 0);
    f1("gpEditOpacity", c.v3d.vertex_opacity);
    const int col = pg_gpu_shader_uniform(sh, "gpEditColor");
    if (col >= 0) glUniform4fv(col, 1, gpd->line_color);
    glDrawArrays(pass == 0 ? GL_LINE_STRIP : GL_POINTS, 0, vert_len);
  }
  for (int a = 0; a < 4; a++) glDisableVertexAttribArray(a);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);
  c.state = DRWState(~0u);
}

int pg_gp_engine_draw(bGPdata *gpd, int cfra, const PGGpEngineView *view)
{
  PGContext &c = pgc();
  if (gpd == nullptr || view == nullptr || view->viewport_w <= 0 || view->viewport_h <= 0) {
    return 0;
  }
  GLint prev_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
  pg_gpu_framebuffer_set_backbuffer(prev_fbo);
  if (!c.initialized) {
    context_init(c);
  }
  c.failed = false;
  c.cfra = cfra;
  c.frame.passes.clear();
  c.frame.groups.clear();
  for (PGPoolTex &p : c.pool) {
    p.used = false;
  }
  viewport_ensure(c, view->viewport_w, view->viewport_h);
  view_set(c, view);
  /* Overlay "Onion Skins" (V3D_GP_SHOW_ONION_SKIN) follows the document's onion switch; the engine
   * draws the ghosts itself (BKE_gpencil_visible_stroke_advanced_iter with do_onion). */
  c.v3d.shading.type = view->rendered ? OB_RENDER : OB_MATERIAL;
  c.v3d.overlay.gpencil_vertex_paint_opacity = view->vertex_opacity;
  c.ob.shader_fx = view->shader_fx ? *view->shader_fx : ListBase{nullptr, nullptr};
  if (gpd->flag & GP_DATA_SHOW_ONIONSKINS) {
    c.v3d.gp_flag |= V3D_GP_SHOW_ONION_SKIN;
  }
  else {
    c.v3d.gp_flag &= ~V3D_GP_SHOW_ONION_SKIN;
  }
  c.ob.data = gpd;
  c.ob.totcol = (short)gpd->totcol;
  /* Update tagging: the caller says when the strokes changed (document revision, frame, evaluated
   * frames); the batch cache and the bound box are rebuilt only then, as Blender's depsgraph tags
   * them (GP_DATA_CACHE_IS_DIRTY). */
  if (view->data_changed) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
    MEM_SAFE_FREE(c.ob.runtime.bb);
  }
  /* The background pass of the viewport: paper colour, depth 1. */
  GPU_framebuffer_bind(c.dfbl.default_fb);
  GPU_framebuffer_clear(c.dfbl.default_fb, (eGPUFrameBufferBits)(GPU_COLOR_BIT | GPU_DEPTH_BIT | GPU_STENCIL_BIT),
                        view->background, 1.0f, 0);
  /* shortcut: the stroke being drawn (sbuffer) stays on the Android presenter; switch it to the
   * engine (DRW_cache_gpencil_sbuffer_*) when the tool session fills gpd->runtime.sbuffer. */
  const int sbuffer_used = gpd->runtime.sbuffer_used;
  gpd->runtime.sbuffer_used = 0;
  /* ADAPTED: BKE_gpencil_frame_active_set (gpencil_modifier_legacy.c), the depsgraph step that
   * makes each layer's active frame the one shown at the scene frame. Restored afterwards: the
   * document's actframe is the editing frame. */
  std::vector<bGPDframe *> actframes;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    actframes.push_back(gpl->actframe);
    gpl->actframe = BKE_gpencil_layer_frame_get(gpl, cfra, GP_GETFRAME_USE_PREV);
  }
  /* ADAPTED: world units here are canvas pixels, PG_MOD_PIXELS_PER_UNIT per Blender unit. A stroke's
   * width in world units is thickness * pixfactor / GPENCIL_PIXEL_FACTOR (gpencil_layer_cache_add),
   * defined for Blender units, so pixfactor is scaled by the unit for the duration of the draw: the
   * same image as converting every point to units (the 1-pixel minimum of
   * gpencil_stroke_thickness_modulate applies to thickness * object scale, which stays 1).
   * shortcut: lengths the engine reads from the data (stroke uv_fac for texture repeats) stay in
   * pixels; convert them when texture materials reach the engine. */
  const float pixfactor = gpd->pixfactor;
  gpd->pixfactor = pixfactor * PG_MOD_PIXELS_PER_UNIT;
  void *vedata = c.vedata_lists;
  DrawEngineType &e = draw_engine_gpencil_type;
  e.engine_init(vedata);
  e.cache_init(vedata);
  e.cache_populate(vedata, &c.ob);
  e.cache_finish(vedata);
  GPU_framebuffer_bind(c.dfbl.default_fb);
  e.draw_scene(vedata);
  if (view->weight_group >= 0) {
    draw_weight_overlay(c, gpd, cfra, view->weight_group);
  }
  gpd->runtime.sbuffer_used = sbuffer_used;
  gpd->pixfactor = pixfactor;
  size_t li = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpl->actframe = actframes[li++];
  }
  frame_end(c);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
  pg_gpu_framebuffer_set_backbuffer(prev_fbo);
  if (c.failed) {
    /* A shader failed part way: hand the context back clean, the Android presenter draws next. */
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glViewport(0, 0, view->viewport_w, view->viewport_h);
  }
  return c.failed ? 0 : 1;
}

unsigned int pg_gp_engine_result_texture(void)
{
  return G_pg ? pg_gpu_texture_gl(G_pg->dtxl.color) : 0;
}

static bool g_present_over = false;
/* As pg_gp_engine_present, blended (premultiplied) over what the frame-buffer holds. */
int pg_gp_engine_present_over(int target_w, int target_h)
{
  g_present_over = true;
  const int ok = pg_gp_engine_present(target_w, target_h);
  g_present_over = false;
  return ok;
}

int pg_gp_engine_present(int target_w, int target_h)
{
  PGContext &c = pgc();
  if (!c.initialized || c.dtxl.color == nullptr) {
    return 0;
  }
  if (c.present_prog == 0) {
    /* Blender's "Standard" view transform: scene linear -> sRGB (IMB colormanagement, OCIO). */
    static const char *vs =
        "#version 300 es\n"
        "out vec2 uv;\n"
        "void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));"
        "uv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}\n";
    static const char *fs =
        "#version 300 es\nprecision highp float;\n"
        "in vec2 uv; uniform sampler2D img; out vec4 col;\n"
        "float s(float c){return c<0.0031308?c*12.92:1.055*pow(c,1.0/2.4)-0.055;}\n"
        "void main(){vec4 c=texture(img,uv);c.rgb=max(c.rgb,0.0);"
        "if(c.a>0.0)c.rgb/=c.a;c.rgb=vec3(s(c.r),s(c.g),s(c.b));col=vec4(c.rgb*c.a,c.a);}\n";
    GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &vs, nullptr);
    glCompileShader(v);
    glShaderSource(f, 1, &fs, nullptr);
    glCompileShader(f);
    c.present_prog = glCreateProgram();
    glAttachShader(c.present_prog, v);
    glAttachShader(c.present_prog, f);
    glLinkProgram(c.present_prog);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(c.present_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
      glDeleteProgram(c.present_prog);
      c.present_prog = 0;
      return 0;
    }
  }
  glViewport(0, 0, target_w, target_h);
  if (g_present_over) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }
  else {
    glDisable(GL_BLEND);
  }
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glUseProgram(c.present_prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, pg_gpu_texture_gl(c.dtxl.color));
  glUniform1i(glGetUniformLocation(c.present_prog, "img"), 0);
  glBindVertexArray(c.vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  glUseProgram(0);
  return 1;
}

void pg_gp_engine_reset(void)
{
  /* shortcut: after a context loss the GL names are gone; the CPU side objects are leaked once
   * per loss rather than freed with a dead context. Free them properly when the presenter's
   * reset path grows a "context still alive" flag. */
  if (G_pg != nullptr) {
    G_pg = nullptr;
  }
}

/** \} */
