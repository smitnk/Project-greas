#include <algorithm>
#include <cmath>
#include <vector>

#include <GLES2/gl2.h>

#include "BKE_gpencil_legacy.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"

#include "project_grease_gp_backend.h"

namespace {
struct Vertex { float x; float y; };

GLuint g_program=0, g_vbo=0;
GLint g_position=-1, g_color=-1;
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
  glUseProgram(g_program);glBindBuffer(GL_ARRAY_BUFFER,g_vbo);
  glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(v.size()*sizeof(Vertex)),v.data(),GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray((GLuint)g_position);
  glVertexAttribPointer((GLuint)g_position,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
  glUniform4f(g_color,color[0],color[1],color[2],color[3]);
  if(blend){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
  glDrawArrays(GL_TRIANGLES,0,(GLsizei)v.size());
  if(blend)glDisable(GL_BLEND);
  glDisableVertexAttribArray((GLuint)g_position);glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
}
void draw_frame(const bGPdata*gpd,const bGPDlayer*layer,const bGPDframe*frame,int w,int h,float alpha){
  if(!frame||!layer||!gpd)return;
  std::vector<Vertex> fill,stroke; fill.reserve(512);stroke.reserve(1024);
  for(const bGPDstroke*s=static_cast<const bGPDstroke*>(frame->strokes.first);s;s=s->next){
    if(!s->points||s->totpoints<=0)continue;
    const Material *ma=(s->mat_nr>=0&&s->mat_nr<gpd->totcol&&gpd->mat)?gpd->mat[s->mat_nr]:nullptr;
    const MaterialGPencilStyle *style=ma?ma->gp_style:nullptr;
    if(style&&(style->flag&GP_MATERIAL_HIDE))continue;
    float avg_strength = 1.0f;
    for (int i = 0; i < s->totpoints; ++i) {
      avg_strength += std::max(0.0f, std::min(s->points[i].strength, 1.0f));
    }
    avg_strength /= float(std::max(1, s->totpoints));
    float color[4]={g_stroke_color[0],g_stroke_color[1],g_stroke_color[2],
                    g_stroke_color[3]*alpha*layer->opacity*avg_strength};
    float fill_color[4]={color[0],color[1],color[2],color[3]};
    if(style){
      color[0]=style->stroke_rgba[0];color[1]=style->stroke_rgba[1];color[2]=style->stroke_rgba[2];color[3]=style->stroke_rgba[3]*alpha*layer->opacity*avg_strength;
      fill_color[0]=style->fill_rgba[0];fill_color[1]=style->fill_rgba[1];fill_color[2]=style->fill_rgba[2];fill_color[3]=style->fill_rgba[3]*alpha*layer->opacity*avg_strength;
    }
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
    draw_frame(gpd,layer,current,w,h,1.0f);
  }
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
extern "C" void project_grease_android_present_set_view_transform(float zoom,float pan_x,float pan_y){
  g_view_zoom=std::clamp(zoom,0.1f,8.0f);
  g_view_pan_x=pan_x;
  g_view_pan_y=pan_y;
}
extern "C" void project_grease_android_present_set_color(float r,float g,float b,float a){g_stroke_color[0]=std::clamp(r,0.0f,1.0f);g_stroke_color[1]=std::clamp(g,0.0f,1.0f);g_stroke_color[2]=std::clamp(b,0.0f,1.0f);g_stroke_color[3]=std::clamp(a,0.0f,1.0f);}
extern "C" void project_grease_android_present_reset(){if(g_vbo)glDeleteBuffers(1,&g_vbo);if(g_program)glDeleteProgram(g_program);g_vbo=0;g_program=0;g_position=-1;g_color=-1;}
