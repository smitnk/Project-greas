#include <algorithm>
#include <cmath>
#include <vector>

#include <GLES2/gl2.h>

#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "ED_gpencil_legacy.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "project_grease_gp_backend.h"
#include "project_grease_gp_color.h"

namespace {
struct Vertex { float x; float y; };

GLuint g_program=0, g_vbo=0;
GLint g_position=-1, g_color=-1;
// Layer masks: strokes of a masked layer are multiplied by an offscreen mask (see render_layer_mask).
GLuint g_mask_program=0;
GLint g_mask_color=-1, g_mask_sampler=-1, g_mask_size=-1;
GLuint g_mask_fbo=0, g_mask_tex=0;
int g_mask_w=0, g_mask_h=0;
GLuint g_active_mask_tex=0;   // non-zero while a masked layer is drawn
int g_active_w=1, g_active_h=1;
// DRAW_REVEALAGE multiplies the target by (1 - alpha) (mask buffer, Blender's "revealage" buffer);
// DRAW_INVERT replaces the target by 1 - target.
enum DrawMode { DRAW_NORMAL, DRAW_REVEALAGE, DRAW_INVERT };
DrawMode g_draw_mode=DRAW_NORMAL;
// Weight Paint view: >= 0 draws strokes tinted by the weight of this vertex group (blue 0 .. red 1).
int g_weight_group=-1;
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
void append_segment(std::vector<Vertex>&v,const bGPDspoint&a,const bGPDspoint&b,float thickness,int w,int h,float alpha){
  (void)alpha;
  float dx=b.x-a.x,dy=b.y-a.y,len=std::sqrt(dx*dx+dy*dy); if(len<0.001f)return;
  float half=std::max(0.5f,thickness*0.5f)*g_map_scale,nx=-dy/len*half,ny=dx/len*half;
  Vertex p0=ndc(a.x+nx,a.y+ny,w,h),p1=ndc(a.x-nx,a.y-ny,w,h),p2=ndc(b.x+nx,b.y+ny,w,h),p3=ndc(b.x-nx,b.y-ny,w,h);
  v.insert(v.end(),{p0,p1,p2,p2,p1,p3});
}
void append_dot(std::vector<Vertex>&v,const bGPDspoint&p,float thickness,int w,int h){
  float half=std::max(0.5f,thickness*0.5f)*g_map_scale;
  Vertex p0=ndc(p.x-half,p.y-half,w,h),p1=ndc(p.x+half,p.y-half,w,h),p2=ndc(p.x-half,p.y+half,w,h),p3=ndc(p.x+half,p.y+half,w,h);
  v.insert(v.end(),{p0,p1,p2,p2,p1,p3});
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
  else if(blend){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
  glDrawArrays(GL_TRIANGLES,0,(GLsizei)v.size());
  if(blend||g_draw_mode!=DRAW_NORMAL)glDisable(GL_BLEND);
  if(masked){glBindTexture(GL_TEXTURE_2D,0);}
  glDisableVertexAttribArray((GLuint)g_position);glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
}
void draw_sbuffer(const bGPdata *gpd, float thickness, int w, int h)
{
  if (!gpd || !gpd->runtime.sbuffer || gpd->runtime.sbuffer_used <= 0) return;
  const tGPspoint *points = static_cast<const tGPspoint *>(gpd->runtime.sbuffer);
  std::vector<Vertex> stroke;
  stroke.reserve(static_cast<size_t>(std::max(1, gpd->runtime.sbuffer_used - 1)) * 6u);
  if (gpd->runtime.sbuffer_used == 1) {
    bGPDspoint p{};
    p.x = points[0].m_xy[0]; p.y = points[0].m_xy[1];
    p.pressure = std::max(points[0].pressure, 0.01f);
    append_dot(stroke, p, thickness * p.pressure, w, h);
  } else {
    for (int i=0; i+1<gpd->runtime.sbuffer_used; ++i) {
      bGPDspoint a{}, b{};
      a.x=points[i].m_xy[0]; a.y=points[i].m_xy[1];
      b.x=points[i+1].m_xy[0]; b.y=points[i+1].m_xy[1];
      a.pressure=std::max(points[i].pressure,0.01f);
      b.pressure=std::max(points[i+1].pressure,0.01f);
      append_segment(stroke,a,b,thickness*0.5f*(a.pressure+b.pressure),w,h,1.0f);
    }
  }
  draw_vertices(stroke,g_stroke_color);
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
    const float alpha_scale = alpha * layer->opacity * avg_strength;
    float color[4]={stroke_rgb[0],stroke_rgb[1],stroke_rgb[2],stroke_base_alpha*alpha_scale};
    float fill_color[4]={fill_rgb[0],fill_rgb[1],fill_rgb[2],fill_base_alpha*alpha_scale};
    if(style==nullptr||((style->flag&GP_MATERIAL_STROKE_SHOW)!=0)){
      if(s->totpoints==1) append_dot(stroke,s[0].points[0],float(s->thickness)*std::max(s->points[0].pressure,0.01f),w,h);
      else {
        for(int i=0;i+1<s->totpoints;i++){
          float pressure=0.5f*(std::max(s->points[i].pressure,0.01f)+std::max(s->points[i+1].pressure,0.01f));
          append_segment(stroke,s->points[i],s->points[i+1],float(s->thickness)*pressure,w,h,alpha);
        }
        if(s->flag&GP_STROKE_CYCLIC){
          float pressure=0.5f*(std::max(s->points[s->totpoints-1].pressure,0.01f)+std::max(s->points[0].pressure,0.01f));
          append_segment(stroke,s->points[s->totpoints-1],s->points[0],float(s->thickness)*pressure,w,h,alpha);
        }
      }
      draw_vertices(stroke,color); stroke.clear();
    }
    if(style && (style->flag&GP_MATERIAL_FILL_SHOW)) append_fill(fill,s,w,h);
    if(!style) append_fill(fill,s,w,h);
    if(!fill.empty()){draw_vertices(fill,fill_color);fill.clear();}
  }
}
} // namespace

// Optional live-modifier hook (see project_grease_modifier_stack.h): maps a layer's current frame
// to the evaluated copy that is drawn instead. Onion-skin frames are always drawn unmodified.
typedef const bGPDframe*(*FrameEvaluator)(void*,const bGPDlayer*,const bGPDframe*,int);
namespace {
FrameEvaluator g_frame_evaluator=nullptr;
void* g_frame_evaluator_user=nullptr;
} // namespace

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
// Weight Paint display: every segment gets the mean weight of its end points through Blender's
// weight colour ramp (BKE_defvert_weight_to_rgb), every point a dot of its own weight.
void draw_frame_weights(const bGPdata* gpd,const bGPDframe* frame,int w,int h){
  if(!frame||!gpd)return;
  for(const bGPDstroke* s=static_cast<const bGPDstroke*>(frame->strokes.first);s;s=s->next){
    if(!s->points||s->totpoints<=0)continue;
    const float base=float(std::max<short>(s->thickness,1));
    for(int i=0;i+1<s->totpoints;i++){
      // Split the segment so the weight ramp is visible along it (about one piece per 6 px).
      const float w0=point_group_weight(s,i,g_weight_group),w1=point_group_weight(s,i+1,g_weight_group);
      const float len=std::hypot(s->points[i+1].x-s->points[i].x,s->points[i+1].y-s->points[i].y)*g_map_scale;
      const int pieces=std::clamp(int(std::ceil(len/6.0f)),1,48);
      for(int k=0;k<pieces;k++){
        const float t0=float(k)/float(pieces),t1=float(k+1)/float(pieces);
        bGPDspoint a{},b{};
        a.x=s->points[i].x+(s->points[i+1].x-s->points[i].x)*t0;a.y=s->points[i].y+(s->points[i+1].y-s->points[i].y)*t0;
        b.x=s->points[i].x+(s->points[i+1].x-s->points[i].x)*t1;b.y=s->points[i].y+(s->points[i+1].y-s->points[i].y)*t1;
        float rgb[3];BKE_defvert_weight_to_rgb(rgb,w0+(w1-w0)*(0.5f*(t0+t1)));
        const float pressure=0.5f*(std::max(s->points[i].pressure,0.01f)+std::max(s->points[i+1].pressure,0.01f));
        std::vector<Vertex> seg;append_segment(seg,a,b,base*pressure,w,h,1.0f);
        const float color[4]={rgb[0],rgb[1],rgb[2],1.0f};draw_vertices(seg,color);
      }
    }
    for(int i=0;i<s->totpoints;i++){
      float rgb[3];BKE_defvert_weight_to_rgb(rgb,point_group_weight(s,i,g_weight_group));
      std::vector<Vertex> dot;append_dot(dot,s->points[i],std::max(base*std::max(s->points[i].pressure,0.01f),4.0f/std::max(g_map_scale,0.01f)),w,h);
      const float color[4]={rgb[0],rgb[1],rgb[2],1.0f};draw_vertices(dot,color);
    }
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

extern "C" int project_grease_android_present_gp_document(const bGPdata* gpd,int frame_number){
  if(!gpd||!ensure_program())return 0;
  GLint vp[4]={0,0,0,0};glGetIntegerv(GL_VIEWPORT,vp);int w=vp[2],h=vp[3];if(w<=0||h<=0)return 0;
  glClearColor(0.08f,0.08f,0.08f,1.0f);glClear(GL_COLOR_BUFFER_BIT);
  update_canvas_map(w,h);
  std::vector<Vertex> canvas;
  const float x0=g_map_origin_x, y0=g_map_origin_y;
  const float x1=x0+float(g_canvas_width)*g_map_scale, y1=y0+float(g_canvas_height)*g_map_scale;
  canvas.insert(canvas.end(),{
      ndc(0,0,w,h), ndc(g_canvas_width,0,w,h), ndc(0,g_canvas_height,w,h),
      ndc(g_canvas_width,g_canvas_height,w,h), ndc(0,g_canvas_height,w,h), ndc(g_canvas_width,0,w,h)
  });
  const float canvas_color[4]={0.96f,0.96f,0.96f,1.0f};
  draw_vertices(canvas,canvas_color,false);
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
    if(layer->onion_flag&GP_LAYER_ONIONSKIN){
      const float base=std::max(0.0f,std::min(gpd->onion_factor,1.0f));
      int steps=std::max(0,(int)layer->gstep);
      for(bGPDframe*f=current->prev;f&&current->framenum-f->framenum<=steps;f=f->prev) {
        float fac=base*(1.0f-float(current->framenum-f->framenum)/float(steps+1));
        draw_frame(gpd,layer,f,w,h,fac);
      }
      steps=std::max(0,(int)layer->gstep_next);
      for(bGPDframe*f=current->next;f&&f->framenum-current->framenum<=steps;f=f->next) {
        float fac=base*(1.0f-float(f->framenum-current->framenum)/float(steps+1));
        draw_frame(gpd,layer,f,w,h,fac);
      }
    }
    const bGPDframe*shown=g_frame_evaluator?g_frame_evaluator(g_frame_evaluator_user,layer,current,frame_number):current;
    if(g_weight_group>=0)draw_frame_weights(gpd,shown?shown:current,w,h);
    else draw_frame(gpd,layer,shown?shown:current,w,h,1.0f);
    g_active_mask_tex=0;
  }
  // Present Blender 3.6.23 Legacy GP tGPspoint sbuffer while the stroke is open.
  draw_sbuffer(gpd, 1.0f, w, h);
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

      if (stroke->totpoints == 1) {
        append_dot(strokes,
                   stroke->points[0],
                   float(stroke->thickness) *
                       std::max(stroke->points[0].pressure, 0.01f),
                   w,
                   h);
      }
      else {
        for (int i = 0; i + 1 < stroke->totpoints; ++i) {
          const float pressure =
              0.5f * (std::max(stroke->points[i].pressure, 0.01f) +
                      std::max(stroke->points[i + 1].pressure, 0.01f));
          append_segment(strokes,
                         stroke->points[i],
                         stroke->points[i + 1],
                         float(stroke->thickness) * pressure,
                         w,
                         h,
                         1.0f);
        }
        if (stroke->flag & GP_STROKE_CYCLIC) {
          const float pressure =
              0.5f *
              (std::max(stroke->points[stroke->totpoints - 1].pressure, 0.01f) +
               std::max(stroke->points[0].pressure, 0.01f));
          append_segment(strokes,
                         stroke->points[stroke->totpoints - 1],
                         stroke->points[0],
                         float(stroke->thickness) * pressure,
                         w,
                         h,
                         1.0f);
        }
      }
    }
    draw_vertices(strokes, mask_color, false);
  }

  return glGetError() == GL_NO_ERROR ? 1 : 0;
}

extern "C" int project_grease_android_present_gp_frame(const bGPDframe*frame){
  (void)frame;
  return 0;
}

extern "C" int project_grease_android_present_pending_stroke(const project_grease::gp::StrokePoint*points,int count,float thickness){
  if(!points||count<=0||!ensure_program())return 0;
  GLint vp[4]={0,0,0,0};glGetIntegerv(GL_VIEWPORT,vp);int w=vp[2],h=vp[3];if(w<=0||h<=0)return 0;
  std::vector<Vertex>v;v.reserve((size_t)std::max(1,count-1)*6);
  if(count==1){bGPDspoint p={};p.x=points[0].x;p.y=points[0].y;p.pressure=std::max(points[0].pressure,0.01f);append_dot(v,p,thickness*p.pressure,w,h);}
  else for(int i=0;i+1<count;i++){bGPDspoint a={},b={};a.x=points[i].x;a.y=points[i].y;a.pressure=std::max(points[i].pressure,0.01f);b.x=points[i+1].x;b.y=points[i+1].y;b.pressure=std::max(points[i+1].pressure,0.01f);append_segment(v,a,b,thickness*0.5f*(a.pressure+b.pressure),w,h,1.0f);}
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
extern "C" void project_grease_android_present_set_view_transform(float zoom,float pan_x,float pan_y){
  g_view_zoom=std::clamp(zoom,0.1f,8.0f);
  g_view_pan_x=pan_x;
  g_view_pan_y=pan_y;
}
extern "C" void project_grease_android_present_set_color(float r,float g,float b,float a){g_stroke_color[0]=std::clamp(r,0.0f,1.0f);g_stroke_color[1]=std::clamp(g,0.0f,1.0f);g_stroke_color[2]=std::clamp(b,0.0f,1.0f);g_stroke_color[3]=std::clamp(a,0.0f,1.0f);}
extern "C" void project_grease_android_present_reset(){if(g_vbo)glDeleteBuffers(1,&g_vbo);if(g_program)glDeleteProgram(g_program);g_vbo=0;g_program=0;g_position=-1;g_color=-1;
  if(g_mask_program)glDeleteProgram(g_mask_program);if(g_mask_tex)glDeleteTextures(1,&g_mask_tex);if(g_mask_fbo)glDeleteFramebuffers(1,&g_mask_fbo);
  g_mask_program=0;g_mask_tex=0;g_mask_fbo=0;g_mask_w=g_mask_h=0;g_active_mask_tex=0;}
