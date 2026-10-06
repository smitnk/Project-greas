#include <algorithm>
#include <map>
#include <cmath>
#include <vector>

#include <GLES2/gl2.h>
#if defined(__has_include)
#if __has_include(<android/log.h>)
#include <android/log.h>
#define PG_HAVE_ANDROID_LOG 1
#endif
#endif

#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "ED_gpencil_legacy.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "project_grease_gp_backend.h"
#include "project_grease_gp_color.h"
#include "project_grease_shader_fx.h"
#include "project_grease_stroke_outline.h"
#include "project_grease_blender_edit5.h"
#include "project_grease_blender_edit6.h"
#include "project_grease_blender_fill.h"
#include "project_grease_blender_mod2.h"

namespace {
struct Vertex { float x; float y; };

GLuint g_program=0, g_vbo=0;
GLint g_position=-1, g_color=-1;
GLuint g_vc_program=0, g_vc_mask_program=0;
// Material textures (MaterialGPencilStyle stroke_style / fill_style TEXTURE). Android has no Blender
// Image datablock: the picked image is uploaded per material slot, key = slot * 2 + (0 stroke, 1 fill).
struct MatTex { GLuint id=0; int w=0, h=0; };
std::map<int,MatTex> g_mat_tex;
GLuint g_tex_program=0;
GLuint g_grad_program=0;
// Per-stroke single coverage: each stroke draws with its own stencil reference and a fragment the
// stroke already wrote is rejected, so a stroke crossing itself is blended once (Blender draws a
// stroke's triangles depth-tested against themselves for the same result). -1 forces a clear.
int g_stencil_ref=0;
GLint g_stencil_fbo=-1;
// Layer masks: strokes of a masked layer are multiplied by an offscreen mask (see render_layer_mask).
GLuint g_mask_program=0;
GLint g_mask_color=-1, g_mask_sampler=-1, g_mask_size=-1;
GLuint g_mask_fbo=0, g_mask_tex=0;
int g_mask_w=0, g_mask_h=0;
GLuint g_active_mask_tex=0;   // non-zero while a masked layer is drawn
int g_active_w=1, g_active_h=1;
// DRAW_REVEALAGE multiplies the target by (1 - alpha) (mask buffer, Blender's "revealage" buffer);
// DRAW_INVERT replaces the target by 1 - target.
enum DrawMode { DRAW_NORMAL, DRAW_REVEALAGE, DRAW_INVERT, DRAW_PREMULT };
DrawMode g_draw_mode=DRAW_NORMAL;
// Weight Paint view: >= 0 draws strokes tinted by the weight of this vertex group (blue 0 .. red 1).
int g_weight_group=-1;
// Fill boundary source (brush fill_draw_mode): 0 GP_FILL_DMODE_BOTH, 1 STROKE, 2 CONTROL.
int g_fill_draw_mode=0;
// Fill "Extend Lines" (brush fill_extend_fac, Blender default 0): Blender's line extensions
// (pg_fill_extend_lines: fill_extend_fac * 0.1 BU, cut where they meet) are drawn in the mask.
float g_fill_extend=0.0f;
// GP_BRUSH_FILL_STROKE_COLLIDE: only extensions that hit something are drawn.
int g_fill_collide=0;
// Offscreen export (PNG): 0 off, 1 canvas background, 2 transparent background. Annotations and the
// open sbuffer are not part of an export.
int g_export_mode=0;
/* Background of opaque exports (export mode 1): the project settings' background colour. */
float g_export_background[4]={0.96f,0.96f,0.96f,1.0f};
const float g_paper_color[4]={0.96f,0.96f,0.96f,1.0f};
// Edit-mode overlay: the points of the editable strokes (Blender's edit-mode vertices), selected
// points in the theme's vertex-select orange.
int g_selection_overlay=0;
// Which parts draw_frame draws: 1 strokes, 2 fills (effect targets render them into separate passes).
int g_draw_parts=3;
// Onion ghost tint (gpencil_layer_final_tint_and_alpha_get: a custom ghost colour replaces the rgb).
bool g_ghost_tint=false;
float g_ghost_rgb[3]={0,0,0};
// Layer state applied while a layer is drawn (gpencil_vert.glsl): layer tint mixed over the colour
// (gpLayerTint), thickness offset (gpThicknessOffset = line_change); the opacity override is used by
// the blend-mode pass, which applies the layer opacity once at composite time (blendOpacity).
float g_layer_tint[4]={0,0,0,0};
float g_line_change=0.0f;
float g_force_layer_opacity=-1.0f;
// Drawing guide overlay (GP_GUIDE_*; -1 off), drawn over the canvas, never exported.
int g_guide_type=-1;
float g_guide_cx=0,g_guide_cy=0,g_guide_angle=0,g_guide_spacing=0;
float g_stroke_color[4]={0.05f,0.05f,0.05f,1.0f};
int g_canvas_width=1280;
int g_canvas_height=720;
float g_map_origin_x=0.0f;
float g_map_origin_y=0.0f;
float g_map_scale=1.0f;
float g_view_zoom=1.0f;
float g_view_pan_x=0.0f;
float g_view_pan_y=0.0f;
void update_canvas_map(int w,int h){
  const float sx=float(w)/float(std::max(1,g_canvas_width));
  const float sy=float(h)/float(std::max(1,g_canvas_height));
  g_map_scale=std::min(sx,sy)*0.92f*g_view_zoom;
  const float cw=float(g_canvas_width)*g_map_scale;
  const float ch=float(g_canvas_height)*g_map_scale;
  g_map_origin_x=(float(w)-cw)*0.5f+g_view_pan_x;
  g_map_origin_y=(float(h)-ch)*0.5f+g_view_pan_y;
}


const char *vs_src(){
  return "attribute vec2 a_position; uniform vec4 u_color; varying vec4 v_color; "
         "void main(){gl_Position=vec4(a_position,0.0,1.0);v_color=u_color;}";
}
// Weight Paint view: per-vertex colour on the stroke geometry (Blender's weight overlay interpolates the
// weight colour across the stroke, not one flat colour per segment).
const char *vc_vs_src(){
  return "attribute vec2 a_position; attribute vec4 a_color; varying vec4 v_color; "
         "void main(){gl_Position=vec4(a_position,0.0,1.0);v_color=a_color;}";
}
const char *fs_src(){
  return "precision mediump float; varying vec4 v_color; void main(){gl_FragColor=v_color;}";
}
const char *mask_fs_src(){
  return "precision mediump float; varying vec4 v_color; uniform sampler2D u_mask; uniform vec2 u_size; "
         "void main(){float m=texture2D(u_mask,gl_FragCoord.xy/u_size).r;gl_FragColor=vec4(v_color.rgb,v_color.a*m);}";
}
GLuint compile_shader(GLenum type,const char *src){
  GLuint s=glCreateShader(type); if(!s) return 0;
  glShaderSource(s,1,&src,nullptr); glCompileShader(s);
  GLint ok=GL_FALSE; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(ok==GL_FALSE){glDeleteShader(s);return 0;} return s;
}
bool ensure_program(){
  if(g_program) return true;
  GLuint vs=compile_shader(GL_VERTEX_SHADER,vs_src()), fs=compile_shader(GL_FRAGMENT_SHADER,fs_src());
  if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
  g_program=glCreateProgram(); glAttachShader(g_program,vs); glAttachShader(g_program,fs);
  glBindAttribLocation(g_program,0,"a_position"); glLinkProgram(g_program);
  glDeleteShader(vs);glDeleteShader(fs);
  GLint ok=GL_FALSE;glGetProgramiv(g_program,GL_LINK_STATUS,&ok);
  if(ok==GL_FALSE){glDeleteProgram(g_program);g_program=0;return false;}
  g_position=glGetAttribLocation(g_program,"a_position"); g_color=glGetUniformLocation(g_program,"u_color");
  glGenBuffers(1,&g_vbo);
  return g_position>=0&&g_color>=0&&g_vbo!=0;
}
GLuint link_vc(const char* fs_text){
  GLuint vs=compile_shader(GL_VERTEX_SHADER,vc_vs_src()), fs=compile_shader(GL_FRAGMENT_SHADER,fs_text);
  if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 0;}
  GLuint p=glCreateProgram(); glAttachShader(p,vs); glAttachShader(p,fs);
  glBindAttribLocation(p,0,"a_position"); glBindAttribLocation(p,1,"a_color");
  glLinkProgram(p);
  glDeleteShader(vs);glDeleteShader(fs);
  GLint ok=GL_FALSE;glGetProgramiv(p,GL_LINK_STATUS,&ok);
  if(ok==GL_FALSE){glDeleteProgram(p);return 0;}
  return p;
}
// Plain and layer-masked (same mask texture as draw_vertices) per-vertex-colour programs.
bool ensure_vc_program(){
  if(!g_vc_program)g_vc_program=link_vc(fs_src());
  if(!g_vc_mask_program)g_vc_mask_program=link_vc(mask_fs_src());
  return g_vc_program!=0&&g_vc_mask_program!=0;
}
// gpencil_frag.glsl: col = texture * (1 - mix) + material colour * mix (stroke_texture_mix /
// fill_texture_mix are 1 - mix_stroke_factor / 1 - mix_factor). fract() repeats the image (GLES2 has
// no REPEAT wrap for non-power-of-two textures); u_mask multiplies like the layer-mask program.
const char *tex_vs_src(){
  return "attribute vec2 a_position; attribute vec2 a_uv; varying vec2 v_uv; "
         "void main(){gl_Position=vec4(a_position,0.0,1.0);v_uv=a_uv;}";
}
const char *tex_fs_src(){
  return "precision mediump float; varying vec2 v_uv; uniform sampler2D u_tex; uniform vec4 u_color; "
         "uniform float u_mix; uniform sampler2D u_mask; uniform vec2 u_size; uniform float u_use_mask; "
         "void main(){vec4 t=texture2D(u_tex,fract(v_uv));"
         "vec4 c=vec4(mix(t.rgb,u_color.rgb,u_mix),mix(t.a,1.0,u_mix)*u_color.a);"
         "if(u_use_mask>0.5)c.a*=texture2D(u_mask,gl_FragCoord.xy/u_size).r;gl_FragColor=c;}";
}
bool ensure_tex_program(){
  if(g_tex_program)return true;
  GLuint vs=compile_shader(GL_VERTEX_SHADER,tex_vs_src()), fs=compile_shader(GL_FRAGMENT_SHADER,tex_fs_src());
  if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
  g_tex_program=glCreateProgram();glAttachShader(g_tex_program,vs);glAttachShader(g_tex_program,fs);
  glBindAttribLocation(g_tex_program,0,"a_position");glBindAttribLocation(g_tex_program,1,"a_uv");
  glLinkProgram(g_tex_program);glDeleteShader(vs);glDeleteShader(fs);
  GLint ok=GL_FALSE;glGetProgramiv(g_tex_program,GL_LINK_STATUS,&ok);
  if(ok==GL_FALSE){glDeleteProgram(g_tex_program);g_tex_program=0;return false;}
  return true;
}
// Gradient fill (gpencil_frag.glsl GP_FILL_GRADIENT_USE): fac = radial ? length(uv * 2 - 1) : uv.x,
// col = mix(fill_color, fill_mix_color, fac); gpencil_color_output() then mixes it with the
// (vertex-colour mixed) fill colour by fill_texture_mix = 1 - mix_factor. Straight-alpha form of
// out = col * color_mul + col.a * color_add.
const char *grad_fs_src(){
  return "precision mediump float; varying vec2 v_uv; uniform vec4 u_c1; uniform vec4 u_c2; uniform vec4 u_color; "
         "uniform float u_mix; uniform float u_radial; uniform sampler2D u_mask; uniform vec2 u_size; uniform float u_use_mask; "
         "void main(){float f=clamp(u_radial>0.5?length(v_uv*2.0-1.0):v_uv.x,0.0,1.0);vec4 g=mix(u_c1,u_c2,f);"
         "vec4 c=vec4(g.rgb*u_mix+u_color.rgb*(1.0-u_mix),g.a*u_color.a);"
         "if(u_use_mask>0.5)c.a*=texture2D(u_mask,gl_FragCoord.xy/u_size).r;gl_FragColor=c;}";
}
bool ensure_grad_program(){
  if(g_grad_program)return true;
  GLuint vs=compile_shader(GL_VERTEX_SHADER,tex_vs_src()), fs=compile_shader(GL_FRAGMENT_SHADER,grad_fs_src());
  if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
  g_grad_program=glCreateProgram();glAttachShader(g_grad_program,vs);glAttachShader(g_grad_program,fs);
  glBindAttribLocation(g_grad_program,0,"a_position");glBindAttribLocation(g_grad_program,1,"a_uv");
  glLinkProgram(g_grad_program);glDeleteShader(vs);glDeleteShader(fs);
  GLint ok=GL_FALSE;glGetProgramiv(g_grad_program,GL_LINK_STATUS,&ok);
  if(ok==GL_FALSE){glDeleteProgram(g_grad_program);g_grad_program=0;return false;}
  return true;
}
const MatTex* material_texture(int slot,int fill){
  auto it=g_mat_tex.find(slot*2+(fill?1:0));
  return it!=g_mat_tex.end()&&it->second.id?&it->second:nullptr;
}
struct UVVertex{float x,y,u,v;};
void draw_textured(const std::vector<UVVertex>&v,const MatTex&tex,const float color[4],float mix){
  if(v.empty()||!ensure_tex_program())return;
  const bool masked=g_active_mask_tex!=0&&g_draw_mode==DRAW_NORMAL;
  glUseProgram(g_tex_program);glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
  glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(v.size()*sizeof(UVVertex)),v.data(),GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(UVVertex),nullptr);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(UVVertex),reinterpret_cast<const void*>(2*sizeof(float)));
  glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,tex.id);
  glUniform1i(glGetUniformLocation(g_tex_program,"u_tex"),1);
  glActiveTexture(GL_TEXTURE0);
  if(masked)glBindTexture(GL_TEXTURE_2D,g_active_mask_tex);
  glUniform1i(glGetUniformLocation(g_tex_program,"u_mask"),0);
  glUniform2f(glGetUniformLocation(g_tex_program,"u_size"),float(g_active_w),float(g_active_h));
  glUniform1f(glGetUniformLocation(g_tex_program,"u_use_mask"),masked?1.0f:0.0f);
  glUniform4f(glGetUniformLocation(g_tex_program,"u_color"),color[0],color[1],color[2],color[3]);
  glUniform1f(glGetUniformLocation(g_tex_program,"u_mix"),std::clamp(mix,0.0f,1.0f));
  glEnable(GL_BLEND);
  if(g_draw_mode==DRAW_REVEALAGE)glBlendFunc(GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);
  else glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES,0,(GLsizei)v.size());
  glDisable(GL_BLEND);
  glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
  if(masked)glBindTexture(GL_TEXTURE_2D,0);
  glDisableVertexAttribArray(1);glDisableVertexAttribArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
}
void draw_gradient(const std::vector<UVVertex>&v,const float c1[4],const float c2[4],const float color[4],float mix_tex,bool radial){
  if(v.empty()||!ensure_grad_program())return;
  const bool masked=g_active_mask_tex!=0&&g_draw_mode==DRAW_NORMAL;
  glUseProgram(g_grad_program);glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
  glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(v.size()*sizeof(UVVertex)),v.data(),GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(UVVertex),nullptr);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(UVVertex),reinterpret_cast<const void*>(2*sizeof(float)));
  glActiveTexture(GL_TEXTURE0);
  if(masked)glBindTexture(GL_TEXTURE_2D,g_active_mask_tex);
  glUniform1i(glGetUniformLocation(g_grad_program,"u_mask"),0);
  glUniform2f(glGetUniformLocation(g_grad_program,"u_size"),float(g_active_w),float(g_active_h));
  glUniform1f(glGetUniformLocation(g_grad_program,"u_use_mask"),masked?1.0f:0.0f);
  glUniform4f(glGetUniformLocation(g_grad_program,"u_c1"),c1[0],c1[1],c1[2],c1[3]);
  glUniform4f(glGetUniformLocation(g_grad_program,"u_c2"),c2[0],c2[1],c2[2],c2[3]);
  glUniform4f(glGetUniformLocation(g_grad_program,"u_color"),color[0],color[1],color[2],color[3]);
  glUniform1f(glGetUniformLocation(g_grad_program,"u_mix"),std::clamp(mix_tex,0.0f,1.0f));
  glUniform1f(glGetUniformLocation(g_grad_program,"u_radial"),radial?1.0f:0.0f);
  glEnable(GL_BLEND);
  if(g_draw_mode==DRAW_REVEALAGE)glBlendFunc(GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);
  else glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES,0,(GLsizei)v.size());
  glDisable(GL_BLEND);
  if(masked)glBindTexture(GL_TEXTURE_2D,0);
  glDisableVertexAttribArray(1);glDisableVertexAttribArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
}
bool coverage_begin(){
  GLint bits=0;glGetIntegerv(GL_STENCIL_BITS,&bits);
  if(bits<=0)return false;
  GLint fbo=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);
  glStencilMask(0xFF);
  if(fbo!=g_stencil_fbo||g_stencil_ref>=255){
    glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);g_stencil_ref=0;g_stencil_fbo=fbo;
  }
  ++g_stencil_ref;
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_NOTEQUAL,g_stencil_ref,0xFF);
  glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
  return true;
}
void coverage_end(bool on){if(on)glDisable(GL_STENCIL_TEST);}
bool ensure_mask_program(){
  if(g_mask_program) return true;
  GLuint vs=compile_shader(GL_VERTEX_SHADER,vs_src()), fs=compile_shader(GL_FRAGMENT_SHADER,mask_fs_src());
  if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
  g_mask_program=glCreateProgram(); glAttachShader(g_mask_program,vs); glAttachShader(g_mask_program,fs);
  glBindAttribLocation(g_mask_program,0,"a_position"); glLinkProgram(g_mask_program);
  glDeleteShader(vs);glDeleteShader(fs);
  GLint ok=GL_FALSE;glGetProgramiv(g_mask_program,GL_LINK_STATUS,&ok);
  if(ok==GL_FALSE){glDeleteProgram(g_mask_program);g_mask_program=0;return false;}
  g_mask_color=glGetUniformLocation(g_mask_program,"u_color");
  g_mask_sampler=glGetUniformLocation(g_mask_program,"u_mask");
  g_mask_size=glGetUniformLocation(g_mask_program,"u_size");
  return g_mask_color>=0&&g_mask_sampler>=0&&g_mask_size>=0;
}
bool ensure_mask_target(int w,int h){
  if(g_mask_fbo&&g_mask_w==w&&g_mask_h==h) return true;
  if(g_mask_tex){glDeleteTextures(1,&g_mask_tex);g_mask_tex=0;}
  if(g_mask_fbo){glDeleteFramebuffers(1,&g_mask_fbo);g_mask_fbo=0;}
  glGenTextures(1,&g_mask_tex);glBindTexture(GL_TEXTURE_2D,g_mask_tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1,&g_mask_fbo);
  GLint prev=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&prev);
  glBindFramebuffer(GL_FRAMEBUFFER,g_mask_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,g_mask_tex,0);
  const bool complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)prev);glBindTexture(GL_TEXTURE_2D,0);
  g_mask_w=w;g_mask_h=h;
  return complete;
}
inline Vertex ndc(float x,float y,int w,int h){(void)w;(void)h;const float sx=g_map_origin_x+x*g_map_scale;const float sy=g_map_origin_y+y*g_map_scale;return {2.0f*sx/float(std::max(1,w))-1.0f,1.0f-2.0f*sy/float(std::max(1,h))};}
// A whole stroke as one band with miter / bevel+round joins and round caps
// (project_grease_stroke_outline.h, after Blender's gpencil_vertex()); thickness is per point.
void append_outline(std::vector<Vertex>&v,const std::vector<PGOutlinePoint>&pts,int flags,int w,int h){
  if(pts.empty())return;
  float max_r=0.0f;
  for(const PGOutlinePoint&p:pts)max_r=std::max(max_r,p.radius);
  const int steps=std::clamp(int(max_r*g_map_scale*0.5f),4,32);
  static std::vector<float> buf;
  const int max_tris=pg_stroke_outline_max_triangles(int(pts.size()),steps);
  buf.resize(size_t(max_tris)*6u);
  const int n=pg_stroke_outline(pts.data(),int(pts.size()),flags,steps,buf.data(),max_tris);
  v.reserve(v.size()+size_t(n)*3u);
  for(int i=0;i<n*3;i++)v.push_back(ndc(buf[size_t(i)*2],buf[size_t(i)*2+1],w,h));
}
inline PGOutlinePoint outline_point(float x,float y,float thickness){
  return PGOutlinePoint{x,y,std::max(0.5f,thickness*0.5f)};
}
void append_stroke_outline(std::vector<Vertex>&v,const bGPDstroke*s,float thickness,int w,int h){
  std::vector<PGOutlinePoint> pts; pts.reserve(size_t(s->totpoints));
  // gpencil_stroke_thickness_modulate(): max(1, thickness * scale + gpThicknessOffset)
  for(int i=0;i<s->totpoints;i++)
    pts.push_back(outline_point(s->points[i].x,s->points[i].y,
                                std::max(g_line_change!=0.0f?1.0f:0.0f,thickness*std::max(s->points[i].pressure,0.01f)+g_line_change)));
  int flags=0;
  if(s->flag&GP_STROKE_CYCLIC)flags|=PG_OUTLINE_CYCLIC;
  if(s->caps[0]==GP_STROKE_CAP_FLAT)flags|=PG_OUTLINE_FLAT_START;
  if(s->caps[1]==GP_STROKE_CAP_FLAT)flags|=PG_OUTLINE_FLAT_END;
  append_outline(v,pts,flags,w,h);
}
// Line types Dots / Squares (MaterialGPencilStyle mode GP_MATERIAL_MODE_DOT / SQUARE): every point is
// a quad of the point's thickness (gpencil_vertex() is_dot path); the x axis follows the stroke
// (GP_MATERIAL_FOLLOW_PATH: the segment to the next point, the previous one at the end), the canvas
// x axis (FOLLOW_OBJ) or the screen x axis (FOLLOW_FIXED), turned by alignment_rotation. Dots are
// round (gpencil_frag.glsl discards outside the circle), squares fill the quad.
void append_dots(std::vector<Vertex>&v,const bGPDstroke*s,const MaterialGPencilStyle*st,float thickness,int w,int h){
  const bool squares=st->mode==GP_MATERIAL_MODE_SQUARE;
  const float ca=std::cos(st->alignment_rotation),sa=std::sin(st->alignment_rotation);
  for(int i=0;i<s->totpoints;i++){
    const bGPDspoint&p=s->points[i];
    const float r=0.5f*std::max(g_line_change!=0.0f?1.0f:0.0f,thickness*std::max(p.pressure,0.01f)+g_line_change);
    if(!squares){
      const int seg=std::clamp(int(r*g_map_scale*0.5f),8,32);
      for(int k=0;k<seg;k++){
        const float a0=6.2831853f*k/seg,a1=6.2831853f*(k+1)/seg;
        v.push_back(ndc(p.x,p.y,w,h));
        v.push_back(ndc(p.x+r*std::cos(a0),p.y+r*std::sin(a0),w,h));
        v.push_back(ndc(p.x+r*std::cos(a1),p.y+r*std::sin(a1),w,h));
      }
      continue;
    }
    float ax=1.0f,ay=0.0f;
    if(st->alignment_mode==GP_MATERIAL_FOLLOW_PATH&&s->totpoints>1){
      const bGPDspoint&a=i+1<s->totpoints?s->points[i]:s->points[i-1];
      const bGPDspoint&b=i+1<s->totpoints?s->points[i+1]:s->points[i];
      const float l=std::hypot(b.x-a.x,b.y-a.y);
      if(l>1e-6f){ax=(b.x-a.x)/l;ay=(b.y-a.y)/l;}
    }
    const float xx=ax*ca-ay*sa,xy=ax*sa+ay*ca; // rotated x axis
    const float yx=-xy,yy=xx;                  // 90 deg
    const float c[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    Vertex q[4];
    for(int k=0;k<4;k++)q[k]=ndc(p.x+r*(c[k][0]*xx+c[k][1]*yx),p.y+r*(c[k][0]*xy+c[k][1]*yy),w,h);
    v.insert(v.end(),{q[0],q[1],q[2],q[0],q[2],q[3]});
  }
}
void append_fill(std::vector<Vertex>&v,const bGPDstroke*s,int w,int h){
  if(!s->triangles||s->tot_triangles<=0)return;
  for(int i=0;i<s->tot_triangles;i++){
    v.push_back(ndc(s->points[s->triangles[i].verts[0]].x,s->points[s->triangles[i].verts[0]].y,w,h));
    v.push_back(ndc(s->points[s->triangles[i].verts[1]].x,s->points[s->triangles[i].verts[1]].y,w,h));
    v.push_back(ndc(s->points[s->triangles[i].verts[2]].x,s->points[s->triangles[i].verts[2]].y,w,h));
  }
}
void draw_vertices(const std::vector<Vertex>&v,const float color[4],bool blend=true){
  if(v.empty())return;
  const bool masked=g_active_mask_tex!=0&&g_draw_mode==DRAW_NORMAL&&g_mask_program!=0;
  const GLuint program=masked?g_mask_program:g_program;
  const GLint color_loc=masked?g_mask_color:g_color;
  glUseProgram(program);glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
  if(masked){
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,g_active_mask_tex);
    glUniform1i(g_mask_sampler,0);glUniform2f(g_mask_size,float(g_active_w),float(g_active_h));
  }
  glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(v.size()*sizeof(Vertex)),v.data(),GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray((GLuint)g_position);
  glVertexAttribPointer((GLuint)g_position,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
  glUniform4f(color_loc,color[0],color[1],color[2],color[3]);
  if(g_draw_mode==DRAW_REVEALAGE){glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_ONE_MINUS_SRC_ALPHA);}
  else if(g_draw_mode==DRAW_INVERT){glEnable(GL_BLEND);glBlendFunc(GL_ONE_MINUS_DST_COLOR,GL_ZERO);}
  else if(g_draw_mode==DRAW_PREMULT){glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);}
  else if(blend){glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);}
  glDrawArrays(GL_TRIANGLES,0,(GLsizei)v.size());
  if(blend||g_draw_mode!=DRAW_NORMAL)glDisable(GL_BLEND);
  if(masked){glBindTexture(GL_TEXTURE_2D,0);}
  glDisableVertexAttribArray((GLuint)g_position);glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
}
// One stroke's triangles, blended once per pixel.
void draw_stroke_once(const std::vector<Vertex>&v,const float color[4]){
  if(v.empty())return;
  const bool on=coverage_begin();
  draw_vertices(v,color);
  coverage_end(on);
}
// Holdout materials (gpencil_frag.glsl: revealColor = fragColor.aaaa) cut through what is below.
// Offscreen (premultiplied) and transparent export targets are erased; the direct path shows the
// canvas paper (or the export background), which is what lies under every layer there.
void draw_holdout(const std::vector<Vertex>&v,float alpha,bool overlap){
  if(v.empty()||g_draw_mode==DRAW_REVEALAGE||g_draw_mode==DRAW_INVERT)return;
  const float a=std::clamp(alpha,0.0f,1.0f);
  if(g_draw_mode==DRAW_PREMULT||g_export_mode==2){
    const DrawMode saved=g_draw_mode;g_draw_mode=DRAW_REVEALAGE;
    const float c[4]={0,0,0,a};
    const bool on=overlap?false:coverage_begin();draw_vertices(v,c);if(!overlap)coverage_end(on);
    g_draw_mode=saved;return;
  }
  const float*bg=g_export_mode==1?g_export_background:g_paper_color;
  const float c[4]={bg[0],bg[1],bg[2],a};
  const bool on=overlap?false:coverage_begin();draw_vertices(v,c);if(!overlap)coverage_end(on);
}
void draw_sbuffer(const bGPdata *gpd, float thickness, int w, int h)
{
  if (!gpd || !gpd->runtime.sbuffer || gpd->runtime.sbuffer_used <= 0) return;
  const tGPspoint *points = static_cast<const tGPspoint *>(gpd->runtime.sbuffer);
  std::vector<Vertex> stroke;
  std::vector<PGOutlinePoint> pts;
  pts.reserve(static_cast<size_t>(gpd->runtime.sbuffer_used));
  for (int i = 0; i < gpd->runtime.sbuffer_used; ++i) {
    pts.push_back(outline_point(points[i].m_xy[0], points[i].m_xy[1],
                                thickness * std::max(points[i].pressure, 0.01f)));
  }
  append_outline(stroke, pts, 0, w, h);
  // g_stroke_color carries the material colour and opacity only; the point strength of the open
  // buffer is applied here once, as draw_frame() applies it to committed strokes.
  float strength = 0.0f;
  for (int i = 0; i < gpd->runtime.sbuffer_used; ++i) strength += std::clamp(points[i].strength, 0.0f, 1.0f);
  strength /= float(gpd->runtime.sbuffer_used);
  const float color[4] = {g_stroke_color[0], g_stroke_color[1], g_stroke_color[2], g_stroke_color[3] * strength};
  draw_stroke_once(stroke, color);
}

// Position of (x,y) on the stroke's centre line: length from the start and signed distance (left +).
void centerline_param(const bGPDstroke*s,float x,float y,float*along,float*side){
  const bool cyclic=(s->flag&GP_STROKE_CYCLIC)!=0;
  const int segs=s->totpoints<2?0:(cyclic?s->totpoints:s->totpoints-1);
  float best=1e30f,acc=0.0f;*along=0.0f;*side=0.0f;
  for(int i=0;i<segs;i++){
    const int j=(i+1)%s->totpoints;
    const float ax=s->points[i].x,ay=s->points[i].y,dx=s->points[j].x-ax,dy=s->points[j].y-ay;
    const float l2=dx*dx+dy*dy,len=std::sqrt(l2);
    float t=l2>1e-12f?((x-ax)*dx+(y-ay)*dy)/l2:0.0f;t=std::clamp(t,0.0f,1.0f);
    const float px=ax+dx*t-x,py=ay+dy*t-y,d=px*px+py*py;
    if(d<best){best=d;*along=acc+len*t;*side=len>1e-6f?((x-ax)*(-dy)+(y-ay)*dx)/len:0.0f;}
    acc+=len;
  }
}
// Stroke texture: U along the stroke in texture_pixsize units of the stroke width (Blender's
// stroke_u_scale = 500 / texture_pixsize over the point uv_fac distance), V across the width.
void append_textured_stroke(std::vector<UVVertex>&out,const bGPDstroke*s,float pixsize,int w,int h){
  std::vector<Vertex> tri;
  append_stroke_outline(tri,s,float(s->thickness),w,h);
  const float width=std::max(1.0f,float(s->thickness));
  const float u_scale=(100.0f/std::max(pixsize,1e-3f))/width;
  out.reserve(out.size()+tri.size());
  for(const Vertex&v:tri){
    // back from NDC to canvas units
    const float sx=(v.x+1.0f)*0.5f*float(w),sy=(1.0f-v.y)*0.5f*float(h);
    const float cx=(sx-g_map_origin_x)/g_map_scale,cy=(sy-g_map_origin_y)/g_map_scale;
    float along,side;centerline_param(s,cx,cy,&along,&side);
    out.push_back({v.x,v.y,along*u_scale,0.5f+side/width});
  }
}
// Fill texture: UV from the stroke's bounding square, then texture_scale, texture_angle and
// texture_offset (the material's fill_uv_transform).
void append_textured_fill(std::vector<UVVertex>&out,const bGPDstroke*s,const MaterialGPencilStyle*st,int w,int h){
  if(!s->triangles||s->tot_triangles<=0)return;
  float mn[2]={1e30f,1e30f},mx[2]={-1e30f,-1e30f};
  for(int i=0;i<s->totpoints;i++){mn[0]=std::min(mn[0],s->points[i].x);mn[1]=std::min(mn[1],s->points[i].y);
    mx[0]=std::max(mx[0],s->points[i].x);mx[1]=std::max(mx[1],s->points[i].y);}
  const float size=std::max({mx[0]-mn[0],mx[1]-mn[1],1e-6f});
  const float ca=std::cos(st->texture_angle),sa=std::sin(st->texture_angle);
  for(int i=0;i<s->tot_triangles;i++)for(int k=0;k<3;k++){
    const bGPDspoint&p=s->points[s->triangles[i].verts[k]];
    float u0=(p.x-mn[0])/size-0.5f+s->uv_translation[0],v0=(p.y-mn[1])/size-0.5f+s->uv_translation[1];
    // the stroke's own fill uv transform (Texture Mapping modifier / gpencil_calc_stroke_fill_uv)
    if(s->uv_rotation!=0.0f){const float rs=std::sin(s->uv_rotation),rc=std::cos(s->uv_rotation);
      const float x=u0*rc-v0*rs,y=u0*rs+v0*rc;u0=x;v0=y;}
    if(s->uv_scale!=0.0f&&s->uv_scale!=1.0f){u0/=s->uv_scale;v0/=s->uv_scale;}
    const float u1=(ca*u0-sa*v0)*st->texture_scale[0],v1=(sa*u0+ca*v0)*st->texture_scale[1];
    const Vertex n=ndc(p.x,p.y,w,h);
    out.push_back({n.x,n.y,u1+0.5f+st->texture_offset[0],v1+0.5f+st->texture_offset[1]});
  }
}
// gpencil_uv_transform_get (gpencil_draw_data.c): T(0.5) * S(1 / scale) * R(-angle) * T(offset).
void gradient_uv_transform(const MaterialGPencilStyle*st,float m[2][2],float off[2]){
  const float sx=st->texture_scale[0]!=0.0f?1.0f/st->texture_scale[0]:0.0f,sy=st->texture_scale[1]!=0.0f?1.0f/st->texture_scale[1]:0.0f;
  const float c=std::cos(-st->texture_angle),s=std::sin(-st->texture_angle);
  // columns of the 2x2 part: S * R
  m[0][0]=sx*c;m[0][1]=sy*s;m[1][0]=-sx*s;m[1][1]=sy*c;
  off[0]=m[0][0]*st->texture_offset[0]+m[1][0]*st->texture_offset[1]+0.5f;
  off[1]=m[0][1]*st->texture_offset[0]+m[1][1]*st->texture_offset[1]+0.5f;
}
// The fill uv follows gpencil_calc_stroke_fill_uv (uv_translation, uv_rotation about 0.5, then
// 1 / uv_scale) but over the stroke's bounding square, centred: Blender normalises by a fixed -1..1 box in
// world units, and the canvas has no world unit (a literal port repeats every 2 canvas units).
void append_gradient_fill(std::vector<UVVertex>&out,const bGPDstroke*s,const MaterialGPencilStyle*st,int w,int h){
  if(!s->triangles||s->tot_triangles<=0)return;
  float mn[2]={1e30f,1e30f},mx[2]={-1e30f,-1e30f};
  for(int i=0;i<s->totpoints;i++){mn[0]=std::min(mn[0],s->points[i].x);mn[1]=std::min(mn[1],s->points[i].y);
    mx[0]=std::max(mx[0],s->points[i].x);mx[1]=std::max(mx[1],s->points[i].y);}
  const float size=std::max({mx[0]-mn[0],mx[1]-mn[1],1e-6f});
  const float rs=std::sin(s->uv_rotation),rc=std::cos(s->uv_rotation);
  float m[2][2],off[2];gradient_uv_transform(st,m,off);
  for(int i=0;i<s->tot_triangles;i++)for(int k=0;k<3;k++){
    const int vi=s->triangles[i].verts[k];
    if(vi<0||vi>=s->totpoints)continue;
    const bGPDspoint&p=s->points[vi];
    float u=(p.x-0.5f*(mn[0]+mx[0]))/size+s->uv_translation[0],v=(p.y-0.5f*(mn[1]+mx[1]))/size+s->uv_translation[1];
    // centred on the box: the material transform adds the 0.5 back (gpencil_uv_transform_get)
    float x=u*rc-v*rs,y=u*rs+v*rc;
    if(s->uv_scale!=0.0f){x/=s->uv_scale;y/=s->uv_scale;}
    const Vertex n=ndc(p.x,p.y,w,h);
    out.push_back({n.x,n.y,m[0][0]*x+m[1][0]*y+off[0],m[0][1]*x+m[1][1]*y+off[1]});
  }
}
void draw_frame(const bGPdata*gpd,const bGPDlayer*layer,const bGPDframe*frame,int w,int h,float alpha){
  if(!frame||!layer||!gpd)return;
  std::vector<Vertex> fill,stroke; fill.reserve(512);stroke.reserve(1024);
  for(const bGPDstroke*s=static_cast<const bGPDstroke*>(frame->strokes.first);s;s=s->next){
    if(!s->points||s->totpoints<=0)continue;
    const Material *ma=(s->mat_nr>=0&&s->mat_nr<gpd->totcol&&gpd->mat)?gpd->mat[s->mat_nr]:nullptr;
    const MaterialGPencilStyle *style=ma?ma->gp_style:nullptr;
    if(style&&(style->flag&GP_MATERIAL_HIDE))continue;
    float avg_strength = 0.0f;
    for (int i = 0; i < s->totpoints; ++i) {
      avg_strength += std::max(0.0f, std::min(s->points[i].strength, 1.0f));
    }
    avg_strength /= float(std::max(1, s->totpoints));
    // Vertex colors follow Blender 3.6.23 gpencil_color_output(): rgb is
    // mix(material.rgb, vert.rgb, vert.a), vertex alpha is not multiplied into the result alpha,
    // and zero alpha (the default) means "no vertex color". Stroke color mixes the point
    // vert_color, fill color mixes the stroke's vert_color_fill. One color per draw call, so the
    // stroke gets the mean of its per-point mixes (see project_grease_gp_color.h).
    const float stroke_base_rgb[3] = {
        style ? style->stroke_rgba[0] : g_stroke_color[0],
        style ? style->stroke_rgba[1] : g_stroke_color[1],
        style ? style->stroke_rgba[2] : g_stroke_color[2]};
    const float fill_base_rgb[3] = {
        style ? style->fill_rgba[0] : g_stroke_color[0],
        style ? style->fill_rgba[1] : g_stroke_color[1],
        style ? style->fill_rgba[2] : g_stroke_color[2]};
    float stroke_rgb[3], fill_rgb[3];
    pg_gp_stroke_mean_mix(stroke_base_rgb, s->points[0].vert_color, sizeof(bGPDspoint),
                          s->totpoints, PG_GP_VERTEX_COLOR_OPACITY, stroke_rgb);
    pg_gp_mix_vertex_color(fill_base_rgb, s->vert_color_fill, PG_GP_VERTEX_COLOR_OPACITY, fill_rgb);
    const float stroke_base_alpha = style ? style->stroke_rgba[3] : g_stroke_color[3];
    const float fill_base_alpha = style ? style->fill_rgba[3] : g_stroke_color[3];
    const float layer_opacity = g_force_layer_opacity>=0.0f?g_force_layer_opacity:layer->opacity;
    const float alpha_scale = alpha * layer_opacity * avg_strength;
    // gpencil_vert.glsl: mixed_col.rgb = mix(mixed_col.rgb, gpLayerTint.rgb, gpLayerTint.a)
    if(g_layer_tint[3]>0.0f){
      for(int c=0;c<3;c++){
        stroke_rgb[c]+= (g_layer_tint[c]-stroke_rgb[c])*g_layer_tint[3];
        fill_rgb[c]+= (g_layer_tint[c]-fill_rgb[c])*g_layer_tint[3];
      }
    }
    float color[4]={stroke_rgb[0],stroke_rgb[1],stroke_rgb[2],stroke_base_alpha*alpha_scale};
    float fill_color[4]={fill_rgb[0],fill_rgb[1],fill_rgb[2],fill_base_alpha*alpha_scale};
    if(g_ghost_tint){for(int c=0;c<3;c++){color[c]=g_ghost_rgb[c];fill_color[c]=g_ghost_rgb[c];}}
    const MatTex*stroke_tex=style&&style->stroke_style==GP_MATERIAL_STROKE_STYLE_TEXTURE?material_texture(s->mat_nr,0):nullptr;
    const MatTex*fill_tex=style&&style->fill_style==GP_MATERIAL_FILL_STYLE_TEXTURE?material_texture(s->mat_nr,1):nullptr;
    if((g_draw_parts&1)&&(style==nullptr||((style->flag&GP_MATERIAL_STROKE_SHOW)!=0))){
      const bool overlap=style&&(style->flag&GP_MATERIAL_DISABLE_STENCIL);
      if(style&&(style->flag&GP_MATERIAL_IS_STROKE_HOLDOUT)&&!g_ghost_tint){
        if(style->mode!=GP_MATERIAL_MODE_LINE)append_dots(stroke,s,style,float(s->thickness),w,h);
        else append_stroke_outline(stroke,s,float(s->thickness),w,h);
        draw_holdout(stroke,color[3],overlap); stroke.clear();
      }
      else if(stroke_tex&&!g_ghost_tint&&g_draw_mode!=DRAW_REVEALAGE){
        std::vector<UVVertex> tv;append_textured_stroke(tv,s,style->texture_pixsize,w,h);
        const bool on=coverage_begin();draw_textured(tv,*stroke_tex,color,style->mix_stroke_factor);coverage_end(on);
      }
      else if(style&&style->mode!=GP_MATERIAL_MODE_LINE){
        append_dots(stroke,s,style,float(s->thickness),w,h);
        if(overlap)draw_vertices(stroke,color);else draw_stroke_once(stroke,color);
        stroke.clear();
      }
      else{
        append_stroke_outline(stroke,s,float(s->thickness),w,h);
        // Self Overlap (GP_MATERIAL_DISABLE_STENCIL): every triangle blends, crossings darken
        if(overlap)draw_vertices(stroke,color);else draw_stroke_once(stroke,color);
        stroke.clear();
      }
    }
    if(g_draw_parts&2){
      if(style&&(style->flag&GP_MATERIAL_IS_FILL_HOLDOUT)&&(style->flag&GP_MATERIAL_FILL_SHOW)&&!g_ghost_tint){
        append_fill(fill,s,w,h);draw_holdout(fill,fill_color[3],true);fill.clear();
      }
      else if(fill_tex&&(style->flag&GP_MATERIAL_FILL_SHOW)&&!g_ghost_tint&&g_draw_mode!=DRAW_REVEALAGE){
        std::vector<UVVertex> tv;append_textured_fill(tv,s,style,w,h);draw_textured(tv,*fill_tex,fill_color,style->mix_factor);
      }
      else if(style&&style->fill_style==GP_MATERIAL_FILL_STYLE_GRADIENT&&(style->flag&GP_MATERIAL_FILL_SHOW)&&!g_ghost_tint&&g_draw_mode!=DRAW_REVEALAGE){
        float c1[4],c2[4];
        for(int c=0;c<4;c++){c1[c]=style->fill_rgba[c];c2[c]=style->mix_rgba[c];}
        if(style->flag&GP_MATERIAL_FLIP_FILL)for(int c=0;c<4;c++)std::swap(c1[c],c2[c]);
        // gpencil_vert.glsl: gradient mode does not modulate alpha by the material (fill_col.a = 1)
        float base[3];pg_gp_mix_vertex_color(c1,s->vert_color_fill,PG_GP_VERTEX_COLOR_OPACITY,base);
        if(g_layer_tint[3]>0.0f)for(int c=0;c<3;c++)base[c]+=(g_layer_tint[c]-base[c])*g_layer_tint[3];
        const float gcol[4]={base[0],base[1],base[2],alpha_scale};
        std::vector<UVVertex> tv;append_gradient_fill(tv,s,style,w,h);
        draw_gradient(tv,c1,c2,gcol,1.0f-style->mix_factor,style->gradient_type==GP_MATERIAL_GRADIENT_RADIAL);
      }
      else{
        if(style && (style->flag&GP_MATERIAL_FILL_SHOW)) append_fill(fill,s,w,h);
        if(!style) append_fill(fill,s,w,h);
        if(!fill.empty()){draw_vertices(fill,fill_color);fill.clear();}
      }
    }
  }
}
} // namespace

// Optional live-modifier hook (see project_grease_modifier_stack.h): maps a layer's current frame
// to the evaluated copy that is drawn instead. Onion-skin frames are always drawn unmodified.
typedef const bGPDframe*(*FrameEvaluator)(void*,const bGPDlayer*,const bGPDframe*,int);
// Shader effects of a layer (project_grease_shader_fx.h): the provider returns the layer's effect list.
typedef int(*FxProvider)(void*,const bGPDlayer*,const PGFxEntry**);
extern "C" int project_grease_fx_pass_count(const PGFxEntry*,int,const PGFxView*);
extern "C" int project_grease_fx_begin_layer(int,int);
extern "C" int project_grease_fx_end_layer(const PGFxEntry*,int,const PGFxView*,unsigned char*,unsigned char*);
extern "C" void project_grease_fx_reset();
extern "C" int project_grease_fx_end_layer_blend(const PGFxEntry*,int,const PGFxView*,int,float);
namespace {
FxProvider g_fx_provider=nullptr;
void* g_fx_provider_user=nullptr;
FrameEvaluator g_frame_evaluator=nullptr;
void* g_frame_evaluator_user=nullptr;
} // namespace

extern "C" void project_grease_android_set_fx_provider(FxProvider fn,void* user){g_fx_provider=fn;g_fx_provider_user=user;}
extern "C" void project_grease_android_set_frame_evaluator(FrameEvaluator fn,void* user){
  g_frame_evaluator=fn;g_frame_evaluator_user=user;
}

namespace {
float point_group_weight(const bGPDstroke* s,int i,int group){
  if(!s->dvert)return 0.0f;
  const MDeformVert& dv=s->dvert[i];
  for(int k=0;k<dv.totweight;k++)if(int(dv.dw[k].def_nr)==group)return dv.dw[k].weight;
  return 0.0f;
}
// Weight Paint display: the stroke's normal outline geometry (same joins and caps as the colour
// view), every outline vertex coloured by the weight interpolated at its nearest centre-line
// position through Blender's weight ramp (BKE_defvert_weight_to_rgb), blue 0 .. red 1.
float weight_at(const bGPDstroke* s,float x,float y){
  if(s->totpoints==1)return point_group_weight(s,0,g_weight_group);
  const bool cyclic=(s->flag&GP_STROKE_CYCLIC)!=0;
  const int segs=cyclic?s->totpoints:s->totpoints-1;
  float best_d=1e30f,best_w=0.0f;
  for(int i=0;i<segs;i++){
    const int j=(i+1)%s->totpoints;
    const float ax=s->points[i].x,ay=s->points[i].y,bx=s->points[j].x,by=s->points[j].y;
    const float dx=bx-ax,dy=by-ay,l2=dx*dx+dy*dy;
    float t=l2>1e-12f?((x-ax)*dx+(y-ay)*dy)/l2:0.0f;
    t=std::clamp(t,0.0f,1.0f);
    const float px=ax+dx*t-x,py=ay+dy*t-y,d=px*px+py*py;
    if(d<best_d){
      best_d=d;
      const float w0=point_group_weight(s,i,g_weight_group),w1=point_group_weight(s,j,g_weight_group);
      best_w=w0+(w1-w0)*t;
    }
  }
  return best_w;
}
void draw_frame_weights(const bGPdata* gpd,const bGPDframe* frame,int w,int h){
  if(!frame||!gpd||!ensure_vc_program())return;
  struct VC{float x,y,r,g,b,a;};
  std::vector<VC> verts;
  std::vector<PGOutlinePoint> pts;
  static std::vector<float> buf;
  for(const bGPDstroke* s=static_cast<const bGPDstroke*>(frame->strokes.first);s;s=s->next){
    if(!s->points||s->totpoints<=0)continue;
    const float base=float(std::max<short>(s->thickness,1));
    // At least 4 px wide on screen so the weights stay readable on thin strokes.
    const float min_r=2.0f/std::max(g_map_scale,1e-6f);
    // Centre line resampled about every 6 px so the colour ramp (not a linear blue..red blend)
    // shows between the original points; extra points are collinear and do not change the shape.
    const bool cyclic=(s->flag&GP_STROKE_CYCLIC)!=0;
    pts.clear();
    for(int i=0;i<s->totpoints;i++){
      const bGPDspoint&a=s->points[i];
      PGOutlinePoint p=outline_point(a.x,a.y,base*std::max(a.pressure,0.01f));
      p.radius=std::max(p.radius,min_r);
      pts.push_back(p);
      if(i+1>=s->totpoints&&!cyclic)break;
      const bGPDspoint&b=s->points[(i+1)%s->totpoints];
      const float len=std::hypot(b.x-a.x,b.y-a.y)*g_map_scale;
      const int pieces=std::clamp(int(std::ceil(len/6.0f)),1,64);
      const float rb=std::max(base*std::max(b.pressure,0.01f)*0.5f,min_r);
      for(int k=1;k<pieces;k++){
        const float t=float(k)/float(pieces);
        pts.push_back(PGOutlinePoint{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,p.radius+(rb-p.radius)*t});
      }
    }
    int flags=0;
    if(cyclic)flags|=PG_OUTLINE_CYCLIC;
    float max_r=0.0f;for(const PGOutlinePoint&p:pts)max_r=std::max(max_r,p.radius);
    const int steps=std::clamp(int(max_r*g_map_scale*0.5f),4,32);
    const int max_tris=pg_stroke_outline_max_triangles(int(pts.size()),steps);
    buf.resize(size_t(max_tris)*6u);
    const int n=pg_stroke_outline(pts.data(),int(pts.size()),flags,steps,buf.data(),max_tris);
    verts.clear();verts.reserve(size_t(n)*3u);
    for(int i=0;i<n*3;i++){
      const float x=buf[size_t(i)*2],y=buf[size_t(i)*2+1];
      float rgb[3];BKE_defvert_weight_to_rgb(rgb,weight_at(s,x,y));
      const Vertex v=ndc(x,y,w,h);
      verts.push_back({v.x,v.y,rgb[0],rgb[1],rgb[2],1.0f});
    }
    if(verts.empty())continue;
    const bool on=coverage_begin();
    const bool masked=g_active_mask_tex!=0;
    const GLuint prog=masked?g_vc_mask_program:g_vc_program;
    glUseProgram(prog);glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
    if(masked){
      glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,g_active_mask_tex);
      glUniform1i(glGetUniformLocation(prog,"u_mask"),0);
      glUniform2f(glGetUniformLocation(prog,"u_size"),float(g_active_w),float(g_active_h));
    }
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(verts.size()*sizeof(VC)),verts.data(),GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(VC),nullptr);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(VC),reinterpret_cast<const void*>(2*sizeof(float)));
    glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)verts.size());
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(1);glDisableVertexAttribArray(0);
    if(masked)glBindTexture(GL_TEXTURE_2D,0);
    glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
    coverage_end(on);
  }
}
// A layer is masked when it has GP_LAYER_USE_MASK and at least one valid mask entry: a layer other
// than itself that is visible, with an entry that is not hidden (gpencil_cache_utils.c).
const bGPDlayer_Mask* valid_mask_entry(const bGPdata* gpd,const bGPDlayer* layer,const bGPDlayer* mask_layer){
  if(!mask_layer||mask_layer==layer||(mask_layer->flag&GP_LAYER_HIDE))return nullptr;
  const bGPDlayer_Mask* entry=BKE_gpencil_layer_mask_named_get(const_cast<bGPDlayer*>(layer),mask_layer->info);
  (void)gpd;
  if(!entry||(entry->flag&GP_MASK_HIDE))return nullptr;
  return entry;
}
bool layer_is_masked(const bGPdata* gpd,const bGPDlayer* layer){
  if(!(layer->flag&GP_LAYER_USE_MASK)||layer->mask_layers.first==nullptr)return false;
  for(const bGPDlayer* m=static_cast<const bGPDlayer*>(gpd->layers.first);m;m=m->next){
    if(valid_mask_entry(gpd,layer,m))return true;
  }
  return false;
}
void draw_invert_pass(int w,int h){
  (void)w;(void)h;
  const std::vector<Vertex> quad={{-1,-1},{1,-1},{-1,1},{-1,1},{1,-1},{1,1}};
  const float white[4]={1,1,1,1};
  g_draw_mode=DRAW_INVERT;draw_vertices(quad,white,false);g_draw_mode=DRAW_NORMAL;
}
// Renders the opacity mask of `layer` into the offscreen target and returns its texture (0 when
// there is nothing to mask with). gpencil_engine.c gpencil_draw_mask(): the buffer starts at 1
// ("revealage"), every mask layer multiplies it by (1 - alpha), an inverted entry flips the buffer
// before it is drawn, and the result is flipped back to an opacity mask, so masks combine as a union.
GLuint render_layer_mask(const bGPdata* gpd,const bGPDlayer* layer,int frame_number,int w,int h){
  if(!ensure_mask_program()||!ensure_mask_target(w,h))return 0;
  GLint prev_fbo=0,vp[4]={0,0,0,0};
  glGetIntegerv(GL_FRAMEBUFFER_BINDING,&prev_fbo);glGetIntegerv(GL_VIEWPORT,vp);
  glBindFramebuffer(GL_FRAMEBUFFER,g_mask_fbo);glViewport(0,0,w,h);
  bool cleared=false,inverted=false;
  for(const bGPDlayer* m=static_cast<const bGPDlayer*>(gpd->layers.first);m;m=m->next){
    const bGPDlayer_Mask* entry=valid_mask_entry(gpd,layer,m);
    if(!entry)continue;
    const bool invert=(entry->flag&GP_MASK_INVERT)!=0;
    if(invert!=inverted){if(cleared)draw_invert_pass(w,h);inverted=!inverted;}
    if(!cleared){cleared=true;glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT);}
    const bGPDframe* frame=BKE_gpencil_layer_frame_get(const_cast<bGPDlayer*>(m),frame_number,GP_GETFRAME_USE_PREV);
    if(!frame)continue;
    const bGPDframe* shown=g_frame_evaluator?g_frame_evaluator(g_frame_evaluator_user,m,frame,frame_number):frame;
    g_draw_mode=DRAW_REVEALAGE;
    draw_frame(gpd,m,shown?shown:frame,w,h,1.0f);
    g_draw_mode=DRAW_NORMAL;
  }
  if(cleared&&!inverted)draw_invert_pass(w,h);
  glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)prev_fbo);glViewport(vp[0],vp[1],vp[2],vp[3]);
  return cleared?g_mask_tex:0;
}
} // namespace

namespace {
// Selected points 5 px, unselected 3 px (screen pixels, independent of zoom).
void draw_selection_overlay(const bGPDframe*frame,int w,int h){
  if(!frame)return;
  const float px=1.0f/std::max(g_map_scale,1e-6f);
  std::vector<Vertex> sel,unsel;
  for(const bGPDstroke*s=static_cast<const bGPDstroke*>(frame->strokes.first);s;s=s->next){
    if(!s->points)continue;
    for(int i=0;i<s->totpoints;i++){
      const PGOutlinePoint p=outline_point(s->points[i].x,s->points[i].y,(s->points[i].flag&GP_SPOINT_SELECT)?5.0f*px:3.0f*px);
      append_outline((s->points[i].flag&GP_SPOINT_SELECT)?sel:unsel,std::vector<PGOutlinePoint>{p},0,w,h);
    }
  }
  const float dark[4]={0.0f,0.0f,0.0f,0.85f};
  const float orange[4]={1.0f,0.522f,0.0f,1.0f};
  draw_vertices(unsel,dark);
  draw_vertices(sel,orange);
}
// Open-stroke cache: while a stroke is being drawn (the sbuffer is open) nothing else in the document
// changes, so the committed drawing of the first frame of the gesture is kept in a texture and every
// later frame of the gesture redraws only that texture and the open stroke. Blender's draw engine
// does the same with its cached batches; without it each input sample re-tessellated and redrew
// every stroke of the frame.
struct OpenStrokeCache {
  GLuint tex=0, program=0, vbo=0;
  int w=0, h=0, frame=0;
  const bGPdata* gpd=nullptr;
  float scale=0, ox=0, oy=0;
  bool valid=false;
} g_open_cache;
long g_open_cache_stores=0, g_open_cache_reuses=0;
bool open_cache_matches(const bGPdata* gpd,int frame,int w,int h){
  const OpenStrokeCache& c=g_open_cache;
  return c.valid&&c.gpd==gpd&&c.frame==frame&&c.w==w&&c.h==h&&c.scale==g_map_scale&&c.ox==g_map_origin_x&&c.oy==g_map_origin_y;
}
bool open_cache_program(){
  OpenStrokeCache& c=g_open_cache;
  if(c.program)return true;
  GLuint vs=compile_shader(GL_VERTEX_SHADER,"attribute vec2 a_position; varying vec2 v_uv; "
      "void main(){v_uv=a_position*0.5+0.5;gl_Position=vec4(a_position,0.0,1.0);}");
  GLuint fs=compile_shader(GL_FRAGMENT_SHADER,"precision mediump float; varying vec2 v_uv; uniform sampler2D u_tex; "
      "void main(){gl_FragColor=vec4(texture2D(u_tex,v_uv).rgb,1.0);}");
  if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
  GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);
  glBindAttribLocation(p,0,"a_position");glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);
  GLint ok=GL_FALSE;glGetProgramiv(p,GL_LINK_STATUS,&ok);
  if(ok==GL_FALSE){glDeleteProgram(p);return false;}
  c.program=p;glGenBuffers(1,&c.vbo);
  return c.vbo!=0;
}
// Clears GL's sticky error flags, so a check after a call reports that call only: an error left
// by an earlier, unrelated draw call otherwise marked every store / reuse as failed and each input
// sample of the open stroke redrew the whole frame.
void clear_gl_errors(){for(int i=0;i<16&&glGetError()!=GL_NO_ERROR;i++){}}
// Copies the framebuffer as drawn so far (canvas and committed strokes) into the cache texture.
void open_cache_store(const bGPdata* gpd,int frame,int w,int h){
  OpenStrokeCache& c=g_open_cache;
  clear_gl_errors();
  if(!c.tex)glGenTextures(1,&c.tex);
  glBindTexture(GL_TEXTURE_2D,c.tex);
  if(c.w!=w||c.h!=h){
    // GL_RGB: a subset of both RGB and RGBA surfaces, as glCopyTexSubImage2D requires.
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  }
  glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,w,h);
  glBindTexture(GL_TEXTURE_2D,0);
  c.valid=glGetError()==GL_NO_ERROR;
  if(c.valid)g_open_cache_stores++;
  c.gpd=gpd;c.frame=frame;c.w=w;c.h=h;c.scale=g_map_scale;c.ox=g_map_origin_x;c.oy=g_map_origin_y;
}
bool open_cache_draw(){
  OpenStrokeCache& c=g_open_cache;
  if(!open_cache_program())return false;
  clear_gl_errors();
  const float quad[12]={-1,-1, 1,-1, -1,1, -1,1, 1,-1, 1,1};
  glUseProgram(c.program);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,c.tex);
  glUniform1i(glGetUniformLocation(c.program,"u_tex"),0);
  glBindBuffer(GL_ARRAY_BUFFER,c.vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STREAM_DRAW);
  glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
  glDisable(GL_BLEND);
  glDrawArrays(GL_TRIANGLES,0,6);
  glDisableVertexAttribArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glBindTexture(GL_TEXTURE_2D,0);glUseProgram(0);
  return glGetError()==GL_NO_ERROR;
}
} // namespace

extern "C" void project_grease_android_present_cache_stats(long* stores,long* reuses){
  if(stores)*stores=g_open_cache_stores;
  if(reuses)*reuses=g_open_cache_reuses;
}

extern "C" int project_grease_android_present_gp_document(const bGPdata* gpd,int frame_number){
  if(!gpd||!ensure_program())return 0;
  GLint vp[4]={0,0,0,0};glGetIntegerv(GL_VIEWPORT,vp);int w=vp[2],h=vp[3];if(w<=0||h<=0)return 0;
  if(g_export_mode==2)glClearColor(0.0f,0.0f,0.0f,0.0f);
  else glClearColor(0.08f,0.08f,0.08f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  g_stencil_fbo=-1;
  update_canvas_map(w,h);
  const bool drawing=!g_export_mode&&gpd->runtime.sbuffer&&gpd->runtime.sbuffer_used>0;
  if(!drawing)g_open_cache.valid=false;
  const bool reuse=drawing&&open_cache_matches(gpd,frame_number,w,h)&&open_cache_draw();
  if(reuse)g_open_cache_reuses++;
  if(!reuse){
  std::vector<Vertex> canvas;
  const float x0=g_map_origin_x, y0=g_map_origin_y;
  const float x1=x0+float(g_canvas_width)*g_map_scale, y1=y0+float(g_canvas_height)*g_map_scale;
  canvas.insert(canvas.end(),{
      ndc(0,0,w,h), ndc(g_canvas_width,0,w,h), ndc(0,g_canvas_height,w,h),
      ndc(g_canvas_width,g_canvas_height,w,h), ndc(0,g_canvas_height,w,h), ndc(g_canvas_width,0,w,h)
  });
  if(g_export_mode!=2)draw_vertices(canvas,g_export_mode==1?g_export_background:g_paper_color,false);
  (void)x0; (void)y0; (void)x1; (void)y1;
  for(const bGPDlayer*layer=static_cast<const bGPDlayer*>(gpd->layers.first);layer;layer=layer->next){
    if(layer->flag&GP_LAYER_HIDE)continue;
    bGPDframe*current=BKE_gpencil_layer_frame_get(const_cast<bGPDlayer*>(layer),frame_number,GP_GETFRAME_USE_PREV);
    if(!current)continue;
    // Layers without masks keep the direct path; a masked layer is multiplied by its mask texture.
    if(layer_is_masked(gpd,layer)){
      g_active_mask_tex=render_layer_mask(gpd,layer,frame_number,w,h);
      g_active_w=w;g_active_h=h;
    }
    // Shader effects: a layer with working effects is drawn offscreen (premultiplied), the passes run on
    // it and the result is composited; a layer without effects keeps the direct path.
    const PGFxEntry* fx_entries=nullptr;
    const int fx_count=g_fx_provider?g_fx_provider(g_fx_provider_user,layer,&fx_entries):0;
    PGFxView fx_view{w,h,g_map_scale,g_map_origin_x,g_map_origin_y,g_canvas_width,g_canvas_height};
    // Effects aimed at strokes only / fills only: the layer is drawn as a fills pass and a strokes
    // pass, each through the effects that target it or the whole layer.
    bool split_fx=false;
    for(int i=0;i<fx_count;i++)if(fx_entries[i].enabled&&fx_entries[i].target!=PG_FX_TARGET_LAYER)split_fx=true;
    // Layer blend mode (eGplBlendMode_*): the layer is drawn offscreen at full opacity and blended
    // onto the frame with gpencil_layer_blend_frag.glsl's blend_mode_output (fx pipeline buffer).
    const int blend_mode=(!split_fx&&layer->blend_mode!=eGplBlendMode_Regular)?layer->blend_mode:eGplBlendMode_Regular;
    const bool want_fx=!split_fx&&fx_count>0&&project_grease_fx_pass_count(fx_entries,fx_count,&fx_view)>0;
    const bool use_blend=blend_mode!=eGplBlendMode_Regular&&g_weight_group<0&&project_grease_fx_begin_layer(w,h)!=0;
    const bool use_fx=!use_blend&&want_fx&&project_grease_fx_begin_layer(w,h)!=0;
    if(use_fx||use_blend)g_draw_mode=DRAW_PREMULT;
    g_layer_tint[0]=layer->tintcolor[0];g_layer_tint[1]=layer->tintcolor[1];g_layer_tint[2]=layer->tintcolor[2];g_layer_tint[3]=layer->tintcolor[3];
    g_line_change=float(layer->line_change);
    // Onion skin: the ghost keyframes of the layer by bGPdata.onion_mode (pg_onion_ghosts: Relative =
    // gstep keyframes before / gstep_next after, Absolute = keyframes within that many frames,
    // Selected = selected keyframes), shown when the overlay switch (GP_DATA_SHOW_ONIONSKINS) and the
    // layer's GP_LAYER_ONIONSKIN are on. onion_id is Blender's signed delta
    // (BKE_gpencil_visible_stroke_advanced_iter); opacity from gpencil_layer_final_tint_and_alpha_get
    // (pg_gp_onion_alpha), custom ghost colours gcolor_prev / gcolor_next when switched on.
    if((gpd->flag&GP_DATA_SHOW_ONIONSKINS)&&(layer->onion_flag&GP_LAYER_ONIONSKIN)){
      const bool fade=(gpd->onion_flag&GP_ONION_FADE)!=0;
      std::vector<int> keys; std::vector<unsigned char> sel; std::vector<const bGPDframe*> frames;
      for(const bGPDframe*f=static_cast<const bGPDframe*>(layer->frames.first);f;f=f->next){
        keys.push_back(f->framenum); sel.push_back((f->flag&GP_FRAME_SELECT)?1:0); frames.push_back(f);
      }
      const int mode=gpd->onion_mode==GP_ONION_MODE_ABSOLUTE?PG_ONION_ABSOLUTE:
                     gpd->onion_mode==GP_ONION_MODE_SELECTED?PG_ONION_SELECTED:PG_ONION_RELATIVE;
      std::vector<int> ghost(keys.size()); std::vector<float> ghost_alpha(keys.size());
      const int n=keys.empty()?0:pg_onion_ghosts(mode,keys.data(),sel.data(),int(keys.size()),current->framenum,
          std::max(0,(int)gpd->gstep),std::max(0,(int)gpd->gstep_next),fade,gpd->onion_factor,
          ghost.data(),ghost_alpha.data(),int(keys.size()));
      int cur_index=0;
      for(size_t i=0;i<frames.size();i++)if(frames[i]==current)cur_index=int(i);
      // Keyframe-type filter (gpd->onion_keytype, -1 = all; pg_onion_keytype_filter) and Loop
      // (GP_ONION_LOOP): a frame out of range is tested again with its delta wrapped by the last
      // frame (BKE_gpencil_visible_stroke_advanced_iter), so the first keys follow the last.
      std::vector<int> types(frames.size());
      for(size_t i=0;i<frames.size();i++)types[i]=frames[i]->key_type;
      std::vector<unsigned char> keep(frames.size(),1);
      if(!frames.empty())pg_onion_keytype_filter(types.data(),int(types.size()),gpd->onion_keytype,keep.data());
      std::vector<unsigned char> drawn(frames.size(),0);
      auto ghost_draw=[&](size_t i,int delta){
        const bool before=delta<0;
        g_ghost_tint=(gpd->onion_flag&(before?GP_ONION_GHOST_PREVCOL:GP_ONION_GHOST_NEXTCOL))!=0;
        const float*col=before?gpd->gcolor_prev:gpd->gcolor_next;
        for(int c=0;c<3;c++)g_ghost_rgb[c]=col[c];
        draw_frame(gpd,layer,frames[i],w,h,pg_gp_onion_alpha(delta,fade,gpd->onion_factor));
        g_ghost_tint=false;
        drawn[i]=1;
      };
      for(int g=0;g<n;g++){
        size_t i=0; while(i<keys.size()&&keys[i]!=ghost[g])i++;
        if(i>=keys.size()||frames[i]==current||!keep[i])continue;
        const int delta=mode==PG_ONION_ABSOLUTE?keys[i]-current->framenum:int(i)-cur_index;
        if(delta==0)continue;
        ghost_draw(i,delta);
      }
      if((gpd->onion_flag&GP_ONION_LOOP)&&mode!=PG_ONION_SELECTED&&!frames.empty()){
        const int shift=mode==PG_ONION_ABSOLUTE?keys.back():int(frames.size())-1;
        for(size_t i=0;i<frames.size();i++){
          if(drawn[i]||frames[i]==current||!keep[i])continue;
          int delta=mode==PG_ONION_ABSOLUTE?keys[i]-current->framenum:int(i)-cur_index;
          if(-delta<=gpd->gstep&&delta<=gpd->gstep_next)continue; /* in range: already handled */
          delta+=(delta<0)?(shift+1):-(shift+1);
          if(delta!=0&&-delta<=gpd->gstep&&delta<=gpd->gstep_next)ghost_draw(i,delta);
        }
      }
    }
    const bGPDframe*shown=g_frame_evaluator?g_frame_evaluator(g_frame_evaluator_user,layer,current,frame_number):current;
    if(use_blend)g_force_layer_opacity=1.0f; // applied once by the blend composite (blendOpacity)
    if(g_weight_group>=0)draw_frame_weights(gpd,shown?shown:current,w,h);
    else if(split_fx){
      for(int part:{2,1}){ // fills under strokes
        std::vector<PGFxEntry> sub;
        for(int i=0;i<fx_count;i++)
          if(fx_entries[i].target==PG_FX_TARGET_LAYER||fx_entries[i].target==(part==1?PG_FX_TARGET_STROKES:PG_FX_TARGET_FILLS))
            sub.push_back(fx_entries[i]);
        const bool pass=!sub.empty()&&project_grease_fx_pass_count(sub.data(),int(sub.size()),&fx_view)>0&&
                        project_grease_fx_begin_layer(w,h)!=0;
        if(pass)g_draw_mode=DRAW_PREMULT;
        g_draw_parts=part;
        draw_frame(gpd,layer,shown?shown:current,w,h,1.0f);
        g_draw_parts=3;
        if(pass){g_draw_mode=DRAW_NORMAL;project_grease_fx_end_layer(sub.data(),int(sub.size()),&fx_view,nullptr,nullptr);}
      }
    }
    else draw_frame(gpd,layer,shown?shown:current,w,h,1.0f);
    if(use_fx){g_draw_mode=DRAW_NORMAL;project_grease_fx_end_layer(fx_entries,fx_count,&fx_view,nullptr,nullptr);}
    if(use_blend){
      g_draw_mode=DRAW_NORMAL;
      project_grease_fx_end_layer_blend(want_fx?fx_entries:nullptr,want_fx?fx_count:0,&fx_view,blend_mode,layer->opacity);
    }
    g_force_layer_opacity=-1.0f;
    g_layer_tint[3]=0.0f;
    g_line_change=0.0f;
    g_active_mask_tex=0;
    if(g_selection_overlay&&!g_export_mode&&!(layer->flag&GP_LAYER_LOCKED))draw_selection_overlay(current,w,h);
  }
  if(drawing)open_cache_store(gpd,frame_number,w,h);
  }
  // Drawing guide (gpencil_draw_guide / the guide overlay of the Draw tool): reference lines of the
  // active guide in the theme's guide colour, over the canvas, not exported.
  if(!g_export_mode&&g_guide_type>=0){
    std::vector<Vertex> lines;
    const float px=1.0f/std::max(g_map_scale,1e-6f);
    const float diag=std::hypot(float(g_canvas_width),float(g_canvas_height));
    auto seg=[&](float x0,float y0,float x1,float y1){
      append_outline(lines,{PGOutlinePoint{x0,y0,0.75f*px},PGOutlinePoint{x1,y1,0.75f*px}},PG_OUTLINE_FLAT_START|PG_OUTLINE_FLAT_END,w,h);
    };
    const float sp=std::max(g_guide_spacing,4.0f);
    const int count=std::min(200,int(diag/sp)+1);
    if(g_guide_type==0){ // circular: rings around the centre
      for(int k=1;k<=count;k++){
        const float r=sp*k; const int n=std::clamp(int(r*g_map_scale*0.25f),24,180);
        for(int j=0;j<n;j++){const float a0=6.2831853f*j/n,a1=6.2831853f*(j+1)/n;
          seg(g_guide_cx+r*std::cos(a0),g_guide_cy+r*std::sin(a0),g_guide_cx+r*std::cos(a1),g_guide_cy+r*std::sin(a1));}
      }
    }
    else if(g_guide_type==1){ // radial: rays through the centre
      for(int k=0;k<24;k++){const float a=6.2831853f*k/24;seg(g_guide_cx,g_guide_cy,g_guide_cx+diag*std::cos(a),g_guide_cy+diag*std::sin(a));}
    }
    else{ // parallel / grid / isometric: families of lines through the canvas
      std::vector<float> angles;
      if(g_guide_type==2)angles={g_guide_angle};
      else if(g_guide_type==3)angles={0.0f,1.5707963f};
      else angles={0.5235988f,1.5707963f,2.6179939f};
      for(float a:angles){
        const float dx=std::cos(a),dy=std::sin(a),nx=-dy,ny=dx;
        for(int k=-count;k<=count;k++){
          const float ox=g_guide_cx+nx*sp*k,oy=g_guide_cy+ny*sp*k;
          seg(ox-dx*diag,oy-dy*diag,ox+dx*diag,oy+dy*diag);
        }
      }
    }
    const float guide_color[4]={0.25f,0.55f,0.95f,0.45f};
    draw_vertices(lines,guide_color);
  }
  // Present Blender 3.6.23 Legacy GP tGPspoint sbuffer while the stroke is open.
  if(!g_export_mode)draw_sbuffer(gpd, 1.0f, w, h);
  return glGetError()==GL_NO_ERROR?1:0;
}


extern "C" int project_grease_android_present_gp_fill_mask(const bGPdata* gpd, int frame_number)
{
  if (!gpd || !ensure_program()) return 0;
  GLint vp[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, vp);
  const int w = vp[2], h = vp[3];
  if (w <= 0 || h <= 0) return 0;

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  const float mask_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  for (const bGPDlayer* layer =
           static_cast<const bGPDlayer*>(gpd->layers.first);
       layer;
       layer = layer->next) {
    if (layer->flag & GP_LAYER_HIDE) continue;

    bGPDframe* current = BKE_gpencil_layer_frame_get(
        const_cast<bGPDlayer*>(layer), frame_number, GP_GETFRAME_USE_PREV);
    if (!current) continue;

    std::vector<Vertex> strokes;
    for (const bGPDstroke* stroke =
             static_cast<const bGPDstroke*>(current->strokes.first);
         stroke;
         stroke = stroke->next) {
      if (!stroke->points || stroke->totpoints <= 0) continue;

      const Material* ma =
          (stroke->mat_nr >= 0 && stroke->mat_nr < gpd->totcol && gpd->mat)
              ? gpd->mat[stroke->mat_nr]
              : nullptr;
      const MaterialGPencilStyle* style = ma ? ma->gp_style : nullptr;
      if (style && (style->flag & GP_MATERIAL_HIDE)) continue;

      // gpencil_fill.c gpencil_draw_datablock(): STROKE / BOTH draw the strokes as they look,
      // CONTROL ("Edit Lines") only the thin basic lines (gpencil_draw_basic_stroke, 1 px).
      if (g_fill_draw_mode == 2) {
        append_stroke_outline(strokes, stroke, 1.0f / std::max(g_map_scale, 1e-6f), w, h);
      }
      else {
        append_stroke_outline(strokes, stroke, float(stroke->thickness), w, h);
      }
    }
    draw_vertices(strokes, mask_color, false);
  }

  // Extend Lines: gpencil_draw_datablock() draws the extension strokes with
  // gpencil_draw_basic_stroke(thickness 1) -> line width 2 px.
  if (g_fill_extend > 0.0f) {
    update_canvas_map(w, h);
    std::vector<float> ext(4096 * 4);
    const int n = pg_fill_extend_lines(const_cast<bGPdata*>(gpd), frame_number, g_fill_extend,
                                       g_fill_collide, g_map_scale, g_map_origin_x, g_map_origin_y,
                                       0.5f * float(g_canvas_width), 0.5f * float(g_canvas_height),
                                       ext.data(), 4096);
    const float r = 1.0f / std::max(g_map_scale, 1e-6f);
    std::vector<Vertex> lines;
    for (int e = 0; e < n; e++) {
      const std::vector<PGOutlinePoint> seg = {PGOutlinePoint{ext[e * 4], ext[e * 4 + 1], r},
                                               PGOutlinePoint{ext[e * 4 + 2], ext[e * 4 + 3], r}};
      append_outline(lines, seg, PG_OUTLINE_FLAT_START | PG_OUTLINE_FLAT_END, w, h);
    }
    draw_vertices(lines, mask_color, false);
  }

  return glGetError() == GL_NO_ERROR ? 1 : 0;
}

// Annotations (project_grease_annotations.h): the frame shown at frame_number of every visible
// annotation layer, in the layer color with a fixed screen-space thickness (layer->thickness px,
// independent of zoom), drawn over the document like Blender's annotation overlay.
extern "C" const bGPDframe *pg_annot_frame_at(const bGPdata *annot, int frame);
#ifdef PG_HAVE_ANDROID_LOG
static void annot_early(const char*why,int frame_number){
  static const char*last=nullptr;
  if(why!=last){last=why;__android_log_print(ANDROID_LOG_INFO,"ProjectGrease","annotpass early-return %s frame=%d",why,frame_number);}
}
#else
static void annot_early(const char*,int){}
#endif
extern "C" int project_grease_android_present_annotations(const bGPdata* annot,int frame_number){
  if(g_export_mode){annot_early("export-mode",frame_number);return 1;}
  if(!annot||!ensure_program()){annot_early("no-annot-or-program",frame_number);return 0;}
  GLint vp[4]={0,0,0,0};glGetIntegerv(GL_VIEWPORT,vp);int w=vp[2],h=vp[3];if(w<=0||h<=0){annot_early("viewport",frame_number);return 0;}
  update_canvas_map(w,h);
  const bGPDlayer*layer=static_cast<const bGPDlayer*>(annot->layers.first);
  if(!layer||(layer->flag&GP_LAYER_HIDE)){annot_early("layer-hidden",frame_number);return 1;}
  const bGPDframe*frame=pg_annot_frame_at(annot,frame_number);
  if(!frame){annot_early("no-frame",frame_number);return 1;}
  annot_early("drawing",frame_number);
  const float px=std::max(1.0f,float(layer->thickness))/std::max(g_map_scale,1e-6f);
  std::vector<Vertex> v; v.reserve(1024);
  for(const bGPDstroke*s=static_cast<const bGPDstroke*>(frame->strokes.first);s;s=s->next){
    if(!s->points||s->totpoints<=0)continue;
    std::vector<PGOutlinePoint> pts; pts.reserve(size_t(s->totpoints));
    for(int i=0;i<s->totpoints;i++)pts.push_back(outline_point(s->points[i].x,s->points[i].y,px));
    append_outline(v,pts,0,w,h);
  }
  clear_gl_errors(); // an earlier pass's error is not this draw's
  draw_vertices(v,layer->color);
  const GLenum err=glGetError();
#ifdef PG_HAVE_ANDROID_LOG
  // Evidence for the emulator sweep: what the annotation pass drew, whenever that changes.
  int strokes=0; long points=0;
  for(const bGPDstroke*s=static_cast<const bGPDstroke*>(frame->strokes.first);s;s=s->next){strokes++;points+=s->totpoints;}
  static long last_key=-1;
  const long key=((long(frame_number)*1000+strokes)*1000000+points)*7+long(v.size()%7)+long(v.size())*1000003L;
  if(key!=last_key){
    last_key=key;
    const bGPDstroke*last=static_cast<const bGPDstroke*>(frame->strokes.last);
    const int np=last?last->totpoints:0;
    const float fx=np?last->points[0].x:0, fy=np?last->points[0].y:0, lx=np?last->points[np-1].x:0, ly=np?last->points[np-1].y:0;
    __android_log_print(ANDROID_LOG_INFO,"ProjectGrease","annotpass frame=%d/%d strokes=%d points=%ld last_stroke_points=%d first=(%.1f,%.1f) last=(%.1f,%.1f) verts=%zu px=%.2f scale=%.3f vp=%dx%d rgba=%.2f,%.2f,%.2f,%.2f err=0x%x",
        frame_number,frame->framenum,strokes,points,np,fx,fy,lx,ly,v.size(),px,g_map_scale,w,h,layer->color[0],layer->color[1],layer->color[2],layer->color[3],err);
  }
#endif
  return err==GL_NO_ERROR?1:0;
}

extern "C" int project_grease_android_present_gp_frame(const bGPDframe*frame){
  (void)frame;
  return 0;
}

extern "C" int project_grease_android_present_pending_stroke(const project_grease::gp::StrokePoint*points,int count,float thickness){
  if(!points||count<=0||!ensure_program())return 0;
  GLint vp[4]={0,0,0,0};glGetIntegerv(GL_VIEWPORT,vp);int w=vp[2],h=vp[3];if(w<=0||h<=0)return 0;
  std::vector<Vertex>v;
  std::vector<PGOutlinePoint> pts; pts.reserve(size_t(count));
  for(int i=0;i<count;i++)pts.push_back(outline_point(points[i].x,points[i].y,thickness*std::max(points[i].pressure,0.01f)));
  append_outline(v,pts,0,w,h);
  // Live preview must use the active Legacy GP material color. The old
  // presenter hard-coded white, which made shape previews disagree with the
  // committed stroke and obscured whether the tool was actually connected.
  draw_vertices(v,g_stroke_color);
  return glGetError()==GL_NO_ERROR?1:0;
}
extern "C" void project_grease_android_present_set_canvas_size(int width,int height){
  g_canvas_width=std::max(1,width);
  g_canvas_height=std::max(1,height);
}
extern "C" void project_grease_android_present_set_weight_view(int group){g_weight_group=group;}
extern "C" void project_grease_android_present_set_guide(int type,float cx,float cy,float angle,float spacing){
  g_guide_type=(type>=0&&type<=4)?type:-1;g_guide_cx=cx;g_guide_cy=cy;g_guide_angle=angle;g_guide_spacing=spacing;}
extern "C" void project_grease_android_present_set_fill_draw_mode(int mode){g_fill_draw_mode=std::clamp(mode,0,2);}
extern "C" int project_grease_android_present_set_material_texture(int slot,int fill,const unsigned char*rgba,int w,int h){
  const int key=slot*2+(fill?1:0);
  auto it=g_mat_tex.find(key);
  if(it!=g_mat_tex.end()){if(it->second.id)glDeleteTextures(1,&it->second.id);g_mat_tex.erase(it);}
  if(!rgba||w<=0||h<=0||slot<0)return 1;
  MatTex t;glGenTextures(1,&t.id);glBindTexture(GL_TEXTURE_2D,t.id);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D,0);
  t.w=w;t.h=h;g_mat_tex[key]=t;
  return glGetError()==GL_NO_ERROR?1:0;
}
extern "C" void project_grease_android_present_set_fill_collide(int collide){g_fill_collide=collide?1:0;}
extern "C" void project_grease_android_present_set_fill_extend(float factor){g_fill_extend=std::isfinite(factor)?std::clamp(factor,0.0f,10.0f):0.0f;}
extern "C" void project_grease_android_present_set_export_mode(int mode){g_export_mode=std::clamp(mode,0,2);}
extern "C" void project_grease_android_present_get_canvas_map(int w,int h,float*scale,float*ox,float*oy){
  update_canvas_map(w,h);
  if(scale)*scale=g_map_scale;
  if(ox)*ox=g_map_origin_x;
  if(oy)*oy=g_map_origin_y;
}
extern "C" void project_grease_android_present_set_export_background(float r,float g,float b){
  g_export_background[0]=std::clamp(r,0.0f,1.0f);g_export_background[1]=std::clamp(g,0.0f,1.0f);
  g_export_background[2]=std::clamp(b,0.0f,1.0f);g_export_background[3]=1.0f;
}
extern "C" void project_grease_android_present_set_selection_overlay(int enabled){g_selection_overlay=enabled!=0;}
extern "C" void project_grease_android_present_get_view_transform(float*zoom,float*pan_x,float*pan_y){
  if(zoom)*zoom=g_view_zoom;if(pan_x)*pan_x=g_view_pan_x;if(pan_y)*pan_y=g_view_pan_y;}
extern "C" void project_grease_android_present_set_view_transform(float zoom,float pan_x,float pan_y){
  g_view_zoom=std::clamp(zoom,0.1f,8.0f);
  g_view_pan_x=pan_x;
  g_view_pan_y=pan_y;
}
extern "C" void project_grease_android_present_set_color(float r,float g,float b,float a){g_stroke_color[0]=std::clamp(r,0.0f,1.0f);g_stroke_color[1]=std::clamp(g,0.0f,1.0f);g_stroke_color[2]=std::clamp(b,0.0f,1.0f);g_stroke_color[3]=std::clamp(a,0.0f,1.0f);}
extern "C" void project_grease_android_present_reset(){if(g_vbo)glDeleteBuffers(1,&g_vbo);if(g_program)glDeleteProgram(g_program);g_vbo=0;g_program=0;g_position=-1;g_color=-1;
  if(g_mask_program)glDeleteProgram(g_mask_program);if(g_mask_tex)glDeleteTextures(1,&g_mask_tex);if(g_mask_fbo)glDeleteFramebuffers(1,&g_mask_fbo);
  g_mask_program=0;g_mask_tex=0;g_mask_fbo=0;g_mask_w=g_mask_h=0;g_active_mask_tex=0;
  for(auto&kv:g_mat_tex)if(kv.second.id)glDeleteTextures(1,&kv.second.id);g_mat_tex.clear();if(g_tex_program)glDeleteProgram(g_tex_program);g_tex_program=0;if(g_grad_program)glDeleteProgram(g_grad_program);g_grad_program=0;
  if(g_vc_program)glDeleteProgram(g_vc_program);if(g_vc_mask_program)glDeleteProgram(g_vc_mask_program);g_vc_program=0;g_vc_mask_program=0;g_stencil_ref=0;g_stencil_fbo=-1;
  // The open-stroke cache's texture / program / buffer belong to the context being torn down: a
  // stale id in the next context has no storage, every later store failed and each input sample
  // redrew the whole frame (sweep: 104 ms per sample over 200 strokes, cache stored once per run).
  {OpenStrokeCache&c=g_open_cache;if(c.tex)glDeleteTextures(1,&c.tex);if(c.program)glDeleteProgram(c.program);if(c.vbo)glDeleteBuffers(1,&c.vbo);c=OpenStrokeCache{};}
  project_grease_fx_reset();}
