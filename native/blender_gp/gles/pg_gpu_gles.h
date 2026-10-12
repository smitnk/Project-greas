/* SPDX-License-Identifier: GPL-2.0-or-later
 * L8: GLES 3.2 GPU-module subset (pg_gpu_gles.cc) and the draw-manager adapter (pg_drw_gles.cc). */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUShader;
struct GPUTexture;
struct GPUUniformBuf;
struct GPUVertBuf;
struct GPUBatch;
struct bGPdata;
struct ListBase;

/* Last error of the GLES layer (shader compile / link, incomplete frame-buffer), or "". */
const char *pg_gpu_gles_last_error(void);
/* Generated GLES 3.2 source of a create-info (fragment != 0: fragment stage), or NULL. */
const char *pg_gles_shader_source(const char *info_name, int fragment);

unsigned int pg_gpu_shader_program(const struct GPUShader *sh);
int pg_gpu_shader_uniform(const struct GPUShader *sh, const char *name);
int pg_gpu_shader_block_binding(const struct GPUShader *sh, const char *name);
int pg_gpu_shader_sampler_unit(const struct GPUShader *sh, const char *name);
void pg_gpu_uniformbuf_bind(const struct GPUUniformBuf *ubo, int binding);
void pg_gpu_texture_bind(const struct GPUTexture *tex, int unit);
void pg_gpu_texture_wrap_mode(struct GPUTexture *tex, int repeat);
unsigned int pg_gpu_texture_gl(const struct GPUTexture *tex);
/* GL frame-buffer that "no frame-buffer bound" means (the window, or the caller's FBO). */
void pg_gpu_framebuffer_set_backbuffer(int gl_fbo);

/* android_gpu_backend.cpp */
unsigned int pg_android_vertbuf_gl(struct GPUVertBuf *vbo);
int pg_android_batch_elem(struct GPUBatch *batch,
                          unsigned int *r_ibo,
                          unsigned int *r_gl_type,
                          unsigned int *r_start,
                          unsigned int *r_base,
                          unsigned int *r_len);

/* The 2D canvas view Blender's GP draw engine renders: canvas pixels are world units (y down),
 * mapped to the viewport like the Android presenter (screen = origin + canvas * scale). */
typedef struct PGGpEngineView {
  int viewport_w, viewport_h; /* render target size in pixels */
  float origin_x, origin_y;   /* screen position of canvas (0, 0), pixels from the top-left */
  float scale;                /* screen pixels per canvas pixel */
  float background[4];        /* linear, premultiplied clear colour of the viewport (the paper) */
  int data_changed;           /* the strokes changed since the last draw: rebuild the batch cache */
  float rotation;             /* view rotation (radians, clockwise on screen) about the viewport centre */
  /* The drawing's shader effects (ShaderFxData list, Blender's object effect stack) and Rendered
   * viewport shading: Blender shows effects only in the Rendered view (simplify_fx otherwise), and
   * there onion skin only with "Always" (GP_ONION_GHOST_ALWAYS). */
  const struct ListBase *shader_fx;
  int rendered;
  /* Weight Paint view: the vertex group whose weights the overlay shows, or -1. */
  int weight_group;
} PGGpEngineView;

/* Runs Blender 3.6.23's GP draw engine (draw/engines/gpencil, unmodified) on \a gpd at frame
 * \a cfra into the engine's viewport colour (linear, RGBA16F) and depth. Returns 1 on success, 0
 * when a shader failed (pg_gpu_gles_last_error). Needs a current GLES 3.2 context. */
int pg_gp_engine_draw(struct bGPdata *gpd, int cfra, const PGGpEngineView *view);
/* GL texture holding the last pg_gp_engine_draw result (scene-linear, premultiplied alpha). */
unsigned int pg_gp_engine_result_texture(void);
/* Draws the last result over the bound frame-buffer as Blender's "Standard" view transform
 * (linear -> sRGB), replacing its colour. */
int pg_gp_engine_present(int target_w, int target_h);
/* The same, blended (premultiplied) over the bound frame-buffer (a reference image under the strokes). */
int pg_gp_engine_present_over(int target_w, int target_h);
/* Frees every GL object of the engine (context loss). */
void pg_gp_engine_reset(void);

#ifdef __cplusplus
}
#endif
