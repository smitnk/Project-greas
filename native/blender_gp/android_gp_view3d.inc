/* 3D viewport of the Android presenter (included by android_gp_presentation.cpp, before
 * engine_draw_canvas). The Camera view is the 2D canvas path, unchanged; the 3D view draws Blender's
 * grid floor (overlay_grid.cc + overlay_grid_vert/frag.glsl), the GP draw engine with the 3D view
 * matrices (project_grease_view3d.c), the canvas frame, empties, object origins and the 3D cursor.
 *
 * World: Blender units. The canvas lies on the Front (X-Z) plane at the GP object's location,
 * PG_MOD_PIXELS_PER_UNIT canvas pixels per unit, centred, canvas y down = -Z, canvas z = +Y. */
#include "project_grease_view3d.h"
#include "BLI_math_vector.h"

namespace {
enum { PG_OB_GPENCIL = 0, PG_OB_ARMATURE = 1, PG_OB_EMPTY = 2 };
struct PGV3DObject { int type; float loc[3]; int selected; };
struct PGV3DState {
  int active = 0;
  bool inited = false;
  PGView3D v{};
  int lock_axis = PG_LOCKAXIS_Y;     /* DNA_scene_defaults.h gp_sculpt.lock_axis */
  int align = PG_PROJECT_VIEWSPACE;  /* gpencil_v3d_align default: Origin */
  float cursor[3] = {0, 0, 0};
  std::vector<PGV3DObject> obs{{PG_OB_GPENCIL, {0, 0, 0}, 1}};
  int active_ob = 0;
  /* the 2D canvas map of the last 3D frame: the open stroke is drawn (and sampled) with it */
  float map_ox = 0, map_oy = 0, map_scale = 1, map_rot = 0;
  int cw = 1280, ch = 720;
  double last_ms = 0, sum_ms = 0, max_ms = 0;
  long frames = 0;
  GLuint grid_prog = 0, grid_vbo = 0;
};
PGV3DState g_v3d;

void v3d_ensure(int w, int h)
{
  if (!g_v3d.inited) {
    pg_view3d_init(&g_v3d.v);
    g_v3d.inited = true;
  }
  if (w > 0 && h > 0 && (w != g_v3d.v.winx || h != g_v3d.v.winy)) pg_view3d_update(&g_v3d.v, w, h);
}
/* canvas pixels -> world */
void v3d_canvas_to_world_m4(float m[4][4])
{
  const float u = 1.0f / PG_MOD_PIXELS_PER_UNIT;
  const float *l = g_v3d.obs[0].loc;
  zero_m4(m);
  m[0][0] = u;
  m[1][2] = -u;
  m[2][1] = u;
  m[3][0] = -0.5f * float(g_v3d.cw) * u + l[0];
  m[3][1] = l[1];
  m[3][2] = 0.5f * float(g_v3d.ch) * u + l[2];
  m[3][3] = 1.0f;
}
void v3d_world_to_canvas(const float co[3], float r[3])
{
  const float U = PG_MOD_PIXELS_PER_UNIT;
  const float *l = g_v3d.obs[0].loc;
  r[0] = (co[0] - l[0]) * U + 0.5f * float(g_v3d.cw);
  r[1] = 0.5f * float(g_v3d.ch) - (co[2] - l[2]) * U;
  r[2] = (co[1] - l[1]) * U;
}
/* The engine's world is canvas pixels: view = S(U) * viewmat * C, window = winmat * S(1 / U), so view
 * depth is in pixels like the stroke widths (gpThicknessWorldScale) and the image is Blender's. */
void view3d_engine_matrices(PGGpEngineView &view)
{
  if (!g_v3d.active || g_cfg->export_mode) return;
  const float U = PG_MOD_PIXELS_PER_UNIT;
  float c[4][4];
  v3d_canvas_to_world_m4(c);
  mul_m4_m4m4(view.viewmat, g_v3d.v.viewmat, c);
  for (int col = 0; col < 4; col++)
    for (int row = 0; row < 3; row++) view.viewmat[col][row] *= U;
  copy_m4_m4(view.winmat, g_v3d.v.winmat);
  for (int col = 0; col < 3; col++)
    for (int row = 0; row < 4; row++) view.winmat[col][row] /= U;
  view.use_matrices = 1;
  view.rotation = 0.0f;
}

/* overlay_grid_vert.glsl / overlay_grid_frag.glsl, GLSL ES 3.00 (the depth test against the scene
 * is dropped: the grid is drawn first, the strokes over it). */
const char *v3d_grid_vs =
    "#version 300 es\n"
    "in vec2 pos; uniform mat4 viewmat, winmat; uniform vec3 cameraPos, plane_axes, grid_size; uniform int grid_flag;\n"
    "out vec3 local_pos;\n"
    "#define flag_test(f,b) (((f)&(b))!=0)\n"
    "void main(){vec3 vert_pos;\n"
    " if(flag_test(grid_flag,16))vert_pos=vec3(pos.x,pos.y,0.0);\n"
    " else if(flag_test(grid_flag,32))vert_pos=vec3(pos.x,0.0,pos.y);\n"
    " else vert_pos=vec3(0.0,pos.x,pos.y);\n"
    " local_pos=vert_pos; vec3 real_pos=cameraPos*plane_axes+vert_pos*grid_size;\n"
    " gl_Position=winmat*(viewmat*vec4(real_pos,1.0));}\n";
const char *v3d_grid_fs =
    "#version 300 es\n"
    "precision highp float; in vec3 local_pos; out vec4 out_color;\n"
    "uniform mat4 viewmat, winmat, viewinv; uniform vec3 cameraPos, plane_axes, grid_size; uniform int grid_flag;\n"
    "uniform float grid_distance, line_size; uniform float steps[8];\n"
    "uniform vec4 colorGrid, colorGridEmphasis, colorGridAxisX, colorGridAxisY, colorGridAxisZ;\n"
    "#define flag_test(f,b) (((f)&(b))!=0)\n"
    "#define M_1_SQRTPI 0.5641895835477563\n"
    "#define DISC_RADIUS (M_1_SQRTPI * 1.05)\n"
    "#define GRID_LINE_SMOOTH_START (0.5 + DISC_RADIUS)\n"
    "#define GRID_LINE_SMOOTH_END (0.5 - DISC_RADIUS)\n"
    "#define GRID_LINE_STEP(dist) smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist)\n"
    "#define linearstep(p0, p1, v) (clamp(((v) - (p0)) / abs((p1) - (p0)), 0.0, 1.0))\n"
    "float get_grid(vec2 co, vec2 fwidthCos, vec2 grid_scale){vec2 half_size=grid_scale/2.0;\n"
    " vec2 grid_domain=abs(mod(co+half_size,grid_scale)-half_size); grid_domain/=fwidthCos;\n"
    " float line_dist=min(grid_domain.x,grid_domain.y); return GRID_LINE_STEP(line_dist-line_size);}\n"
    "vec3 get_axes(vec3 co, vec3 fwidthCos, float ls){vec3 axes_domain=abs(co); axes_domain/=fwidthCos;\n"
    " return GRID_LINE_STEP(axes_domain-(ls+line_size));}\n"
    "void main(){vec3 P=local_pos*grid_size; vec3 dFdxPos=dFdx(P); vec3 dFdyPos=dFdy(P);\n"
    " vec3 fwidthPos=abs(dFdxPos)+abs(dFdyPos); P+=cameraPos*plane_axes; float dist,fade;\n"
    " bool is_persp=winmat[3][3]==0.0;\n"
    " if(is_persp){vec3 V=cameraPos-P; dist=length(V); V/=dist; float angle;\n"
    "  if(flag_test(grid_flag,32))angle=V.y; else if(flag_test(grid_flag,64))angle=V.x; else angle=V.z;\n"
    "  angle=1.0-abs(angle); angle*=angle; fade=1.0-angle*angle;\n"
    "  fade*=1.0-smoothstep(0.0,grid_distance,dist-grid_distance);}\n"
    " else{dist=gl_FragCoord.z*2.0-1.0; dist=abs(dist); fade=1.0-smoothstep(0.0,0.5,dist-0.5); dist=1.0;\n"
    "  if(flag_test(grid_flag,16)){float angle=1.0-abs(viewinv[2].z); dist=1.0+angle*2.0; angle*=angle; fade*=1.0-angle*angle;}}\n"
    " if(flag_test(grid_flag,8)){float grid_res=dot(dFdxPos,viewinv[0].xyz); grid_res*=4.0;\n"
    "  int step_id_x=7; int step_id_y=7;\n"
    "  for(int i=6;i>=0;--i){step_id_x=(grid_res<steps[i])?i:step_id_x; step_id_y=(grid_res<steps[i])?i:step_id_y;}\n"
    "  float scale0x=step_id_x>0?steps[step_id_x-1]:0.0; float scaleAx=steps[step_id_x];\n"
    "  float scaleBx=steps[min(step_id_x+1,7)]; float scaleCx=steps[min(step_id_x+2,7)];\n"
    "  float scale0y=step_id_y>0?steps[step_id_y-1]:0.0; float scaleAy=steps[step_id_y];\n"
    "  float scaleBy=steps[min(step_id_y+1,7)]; float scaleCy=steps[min(step_id_y+2,7)];\n"
    "  float blend=1.0-linearstep(scale0x+scale0y,scaleAx+scaleAy,grid_res+grid_res); blend=blend*blend*blend;\n"
    "  vec2 grid_pos,grid_fwidth;\n"
    "  if(flag_test(grid_flag,32)){grid_pos=P.xz;grid_fwidth=fwidthPos.xz;}\n"
    "  else if(flag_test(grid_flag,64)){grid_pos=P.yz;grid_fwidth=fwidthPos.yz;}\n"
    "  else{grid_pos=P.xy;grid_fwidth=fwidthPos.xy;}\n"
    "  float gridA=get_grid(grid_pos,grid_fwidth,vec2(scaleAx,scaleAy));\n"
    "  float gridB=get_grid(grid_pos,grid_fwidth,vec2(scaleBx,scaleBy));\n"
    "  float gridC=get_grid(grid_pos,grid_fwidth,vec2(scaleCx,scaleCy));\n"
    "  out_color=colorGrid; out_color.a*=gridA*blend;\n"
    "  out_color=mix(out_color,mix(colorGrid,colorGridEmphasis,blend),gridB);\n"
    "  out_color=mix(out_color,colorGridEmphasis,gridC);}\n"
    " else out_color=vec4(colorGrid.rgb,0.0);\n"
    " if(flag_test(grid_flag,7)){vec3 axes_dist=vec3(0.0),axes_fwidth=vec3(1.0);\n"
    "  if(flag_test(grid_flag,1)){axes_dist.x=dot(P.yz,plane_axes.yz);axes_fwidth.x=dot(fwidthPos.yz,plane_axes.yz);}\n"
    "  if(flag_test(grid_flag,2)){axes_dist.y=dot(P.xz,plane_axes.xz);axes_fwidth.y=dot(fwidthPos.xz,plane_axes.xz);}\n"
    "  if(flag_test(grid_flag,4)){axes_dist.z=dot(P.xy,plane_axes.xy);axes_fwidth.z=dot(fwidthPos.xy,plane_axes.xy);}\n"
    "  vec3 axes=get_axes(axes_dist,axes_fwidth,0.1);\n"
    "  if(flag_test(grid_flag,1)){out_color.a=max(out_color.a,axes.x);out_color.rgb=(axes.x<1e-8)?out_color.rgb:colorGridAxisX.rgb;}\n"
    "  if(flag_test(grid_flag,2)){out_color.a=max(out_color.a,axes.y);out_color.rgb=(axes.y<1e-8)?out_color.rgb:colorGridAxisY.rgb;}\n"
    "  if(flag_test(grid_flag,4)){out_color.a=max(out_color.a,axes.z);out_color.rgb=(axes.z<1e-8)?out_color.rgb:colorGridAxisZ.rgb;}}\n"
    " out_color.a*=fade;}\n";

/* Theme colours (userdef_default_theme.c: view3d grid 0x54545480, back 0x3d3d3d, axes
 * 0xff3352 / 0x8bdc00 / 0x2890ff) shaded as DRW_globals_update: grid +10, emphasis +20, axes the
 * grid / axis blend 0.5 shaded -10. */
constexpr float k255 = 1.0f / 255.0f;
const float V3D_BACK[3] = {0x3d * k255, 0x3d * k255, 0x3d * k255};
const float V3D_GRID[4] = {94 * k255, 94 * k255, 94 * k255, 128 * k255};
const float V3D_GRID_EMPH[4] = {104 * k255, 104 * k255, 104 * k255, 128 * k255};
const float V3D_AXIS_X[4] = {159 * k255, 57 * k255, 73 * k255, 191 * k255};
const float V3D_AXIS_Y[4] = {101 * k255, 142 * k255, 32 * k255, 191 * k255};
const float V3D_AXIS_Z[4] = {52 * k255, 104 * k255, 159 * k255, 191 * k255};
const float V3D_SELECT[4] = {0xed * k255, 0x57 * k255, 0x00, 1.0f};  /* TH_SELECT */
const float V3D_ACTIVE[4] = {0xff * k255, 0xa0 * k255, 0x28 * k255, 1.0f}; /* TH_ACTIVE */

bool v3d_grid_program()
{
  if (g_v3d.grid_prog) return true;
  GLuint vs = compile_shader(GL_VERTEX_SHADER, v3d_grid_vs), fs = compile_shader(GL_FRAGMENT_SHADER, v3d_grid_fs);
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return false;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glBindAttribLocation(p, 0, "pos");
  glLinkProgram(p);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = GL_FALSE;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (ok == GL_FALSE) {
    glDeleteProgram(p);
    return false;
  }
  g_v3d.grid_prog = p;
  /* DRW_cache_grid_get: 8 x 8 quads over -1 .. 1 */
  std::vector<float> pos;
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++) {
      const float x0 = i / 4.0f - 1, x1 = (i + 1) / 4.0f - 1, y0 = j / 4.0f - 1, y1 = (j + 1) / 4.0f - 1;
      pos.insert(pos.end(), {x0, y0, x1, y0, x0, y1, x1, y0, x1, y1, x0, y1});
    }
  glGenBuffers(1, &g_v3d.grid_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_v3d.grid_vbo);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(pos.size() * sizeof(float)), pos.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return true;
}

/* OVERLAY_grid_init (SPACE_VIEW3D, gridflag X | Y | FLOOR | ORTHO_GRID, no unit system:
 * view3d_grid_steps_ex) + OVERLAY_grid_cache_init (the main grid pass; the Z axis is off). */
void v3d_draw_grid()
{
  if (!v3d_grid_program()) return;
  const PGView3D &v = g_v3d.v;
  int flag = 0;
  const bool user = v.view == PG_RV3D_VIEW_USER;
  if (v.winmat[3][3] == 0.0f || user) flag = 16 /*PLANE_XY*/ | 1 | 2 | 8;
  else if (v.view == PG_RV3D_VIEW_RIGHT || v.view == PG_RV3D_VIEW_LEFT) flag = 64 | 2 | 4 | 8;
  else if (v.view == PG_RV3D_VIEW_TOP || v.view == PG_RV3D_VIEW_BOTTOM) flag = 16 | 1 | 2 | 8;
  else flag = 32 | 1 | 4 | 8;
  const float axes[3] = {float((flag & (32 | 16)) != 0), float((flag & (64 | 16)) != 0), float((flag & (64 | 32)) != 0)};
  const float dist = v.clip_end;
  float size;
  if (v.winmat[3][3] == 0.0f) size = dist;
  else size = (1.0f / std::min(std::fabs(v.winmat[0][0]), std::fabs(v.winmat[1][1]))) * dist;
  float steps[8];
  float grid_scale = v.grid;
  if (!user) grid_scale /= std::pow(float(v.gridsubdiv), 3.0f);
  int subdiv = 1;
  for (int i = 0; i < 8; i++) {
    steps[i] = grid_scale * float(subdiv);
    subdiv *= v.gridsubdiv;
  }
  const GLuint p = g_v3d.grid_prog;
  glUseProgram(p);
  glUniformMatrix4fv(glGetUniformLocation(p, "viewmat"), 1, GL_FALSE, &v.viewmat[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(p, "winmat"), 1, GL_FALSE, &v.winmat[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(p, "viewinv"), 1, GL_FALSE, &v.viewinv[0][0]);
  glUniform3fv(glGetUniformLocation(p, "cameraPos"), 1, v.viewinv[3]);
  glUniform3fv(glGetUniformLocation(p, "plane_axes"), 1, axes);
  glUniform3f(glGetUniformLocation(p, "grid_size"), size, size, size);
  glUniform1i(glGetUniformLocation(p, "grid_flag"), flag);
  glUniform1f(glGetUniformLocation(p, "grid_distance"), dist / 2.0f);
  glUniform1f(glGetUniformLocation(p, "line_size"), 0.0f); /* max(0, U.pixelsize - 1) * 0.5 */
  glUniform1fv(glGetUniformLocation(p, "steps"), 8, steps);
  glUniform4fv(glGetUniformLocation(p, "colorGrid"), 1, V3D_GRID);
  glUniform4fv(glGetUniformLocation(p, "colorGridEmphasis"), 1, V3D_GRID_EMPH);
  glUniform4fv(glGetUniformLocation(p, "colorGridAxisX"), 1, V3D_AXIS_X);
  glUniform4fv(glGetUniformLocation(p, "colorGridAxisY"), 1, V3D_AXIS_Y);
  glUniform4fv(glGetUniformLocation(p, "colorGridAxisZ"), 1, V3D_AXIS_Z);
  glBindBuffer(GL_ARRAY_BUFFER, g_v3d.grid_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnable(GL_BLEND); /* DRW_STATE_BLEND_ALPHA */
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES, 0, 8 * 8 * 6);
  glDisable(GL_BLEND);
  glDisableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);
}

/* Screen-space overlay shapes (window coordinates, y up) as triangles for draw_vertices. */
Vertex v3d_ndc(float x, float y) { return {2.0f * x / float(g_v3d.v.winx) - 1.0f, 2.0f * y / float(g_v3d.v.winy) - 1.0f}; }
void v3d_seg(std::vector<Vertex> &out, const float a[2], const float b[2], float width)
{
  float d[2] = {b[0] - a[0], b[1] - a[1]};
  const float len = std::hypot(d[0], d[1]);
  if (len < 1e-6f) return;
  const float nx = -d[1] / len * width * 0.5f, ny = d[0] / len * width * 0.5f;
  out.insert(out.end(), {v3d_ndc(a[0] + nx, a[1] + ny), v3d_ndc(b[0] + nx, b[1] + ny), v3d_ndc(a[0] - nx, a[1] - ny),
                         v3d_ndc(b[0] + nx, b[1] + ny), v3d_ndc(b[0] - nx, b[1] - ny), v3d_ndc(a[0] - nx, a[1] - ny)});
}
void v3d_world_seg(std::vector<Vertex> &out, const float a[3], const float b[3], float width)
{
  float pa[2], pb[2];
  if (pg_view3d_project(&g_v3d.v, a, pa) && pg_view3d_project(&g_v3d.v, b, pb)) v3d_seg(out, pa, pb, width);
}
void v3d_disc(std::vector<Vertex> &out, const float c[2], float r)
{
  for (int i = 0; i < 16; i++) {
    const float a0 = 6.2831853f * i / 16, a1 = 6.2831853f * (i + 1) / 16;
    out.insert(out.end(), {v3d_ndc(c[0], c[1]), v3d_ndc(c[0] + r * std::cos(a0), c[1] + r * std::sin(a0)),
                           v3d_ndc(c[0] + r * std::cos(a1), c[1] + r * std::sin(a1))});
  }
}

void v3d_draw_overlays(float px)
{
  const float black[4] = {0, 0, 0, 1}, white[4] = {1, 1, 1, 1}, red[4] = {1, 0.2f, 0.2f, 1};
  /* The canvas frame (the camera border of the 2D view) on the drawing plane. */
  {
    std::vector<Vertex> frame;
    float c[4][4];
    v3d_canvas_to_world_m4(c);
    float corners[4][3] = {{0, 0, 0}, {float(g_v3d.cw), 0, 0}, {float(g_v3d.cw), float(g_v3d.ch), 0}, {0, float(g_v3d.ch), 0}};
    for (auto &k : corners) mul_m4_v3(c, k);
    for (int i = 0; i < 4; i++) v3d_world_seg(frame, corners[i], corners[(i + 1) % 4], 1.0f * px);
    draw_vertices(frame, black, true);
  }
  /* Empties (plain axes, empty_display_size 1) and origins (U.obcenter_dia 6 px, outlined). */
  std::vector<Vertex> wires, wires_sel;
  for (const PGV3DObject &ob : g_v3d.obs) {
    if (ob.type != PG_OB_EMPTY) continue;
    for (int a = 0; a < 3; a++) {
      float p0[3], p1[3];
      copy_v3_v3(p0, ob.loc);
      copy_v3_v3(p1, ob.loc);
      p0[a] -= 1.0f;
      p1[a] += 1.0f;
      v3d_world_seg(ob.selected ? wires_sel : wires, p0, p1, 1.0f * px);
    }
  }
  draw_vertices(wires, black, true);
  draw_vertices(wires_sel, V3D_SELECT, true);
  for (int i = 0; i < int(g_v3d.obs.size()); i++) {
    const PGV3DObject &ob = g_v3d.obs[size_t(i)];
    float s[2];
    if (!pg_view3d_project(&g_v3d.v, ob.loc, s)) continue;
    std::vector<Vertex> outline, dot;
    v3d_disc(outline, s, 4.0f * px);
    v3d_disc(dot, s, 3.0f * px);
    draw_vertices(outline, black, true);
    const float grey[4] = {0.55f, 0.55f, 0.55f, 1};
    draw_vertices(dot, !ob.selected ? grey : (i == g_v3d.active_ob ? V3D_ACTIVE : V3D_SELECT), true);
  }
  /* 3D cursor (view3d_draw.cc drawcursor: red / white dashed circle of U.widget_unit, crosshair). */
  float s[2];
  if (pg_view3d_project(&g_v3d.v, g_v3d.cursor, s)) {
    const float r = 10.0f * px;
    std::vector<Vertex> rw[2];
    for (int i = 0; i < 16; i++) {
      const float a0 = 6.2831853f * i / 16, a1 = 6.2831853f * (i + 1) / 16;
      const float p0[2] = {s[0] + r * std::cos(a0), s[1] + r * std::sin(a0)}, p1[2] = {s[0] + r * std::cos(a1), s[1] + r * std::sin(a1)};
      v3d_seg(rw[i & 1], p0, p1, 2.0f * px);
    }
    draw_vertices(rw[0], red, true);
    draw_vertices(rw[1], white, true);
    std::vector<Vertex> cross;
    for (int k = 0; k < 4; k++) {
      const float dx = k == 0 ? 1.f : k == 1 ? -1.f : 0.f, dy = k == 2 ? 1.f : k == 3 ? -1.f : 0.f;
      const float a[2] = {s[0] + dx * r * 0.5f, s[1] + dy * r * 0.5f}, b[2] = {s[0] + dx * r * 1.4f, s[1] + dy * r * 1.4f};
      v3d_seg(cross, a, b, 1.0f * px);
    }
    draw_vertices(cross, black, true);
  }
}
}  // namespace

bool engine_wanted(const bGPdata *gpd);
static void arm_overlay(const bGPdata *gpd, int w, int h, bool front, bool view3d); /* android_gp_armature_overlay.inc */
bool engine_draw_canvas(const bGPdata *gpd, int frame_number, int w, int h);

/* The 3D frame; false leaves the frame to the 2D path (Camera view, exports, no GLES 3 engine). */
static bool view3d_present(const bGPdata *gpd, int frame_number, int w, int h)
{
  if (!g_v3d.active || g_cfg->export_mode) return false;
  /* shortcut: the 3D view draws strokes with the GLES 3 engine only; on a GLES 2 device the
   * Camera view is shown instead, until the presenter gets a 3D path. */
  if (!engine_wanted(gpd)) return false;
  const auto t0 = std::chrono::steady_clock::now();
  v3d_ensure(w, h);
  g_v3d.map_ox = g_cfg->map_origin_x;
  g_v3d.map_oy = g_cfg->map_origin_y;
  g_v3d.map_scale = g_cfg->map_scale;
  g_v3d.map_rot = g_cfg->view_rotation;
  g_v3d.cw = g_cfg->canvas_width;
  g_v3d.ch = g_cfg->canvas_height;
  glClearColor(V3D_BACK[0], V3D_BACK[1], V3D_BACK[2], 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  v3d_draw_grid();
  arm_overlay(gpd, w, h, false, true);
  const bool ok = engine_draw_canvas(gpd, frame_number, w, h);
  glViewport(0, 0, w, h);
  v3d_draw_overlays(std::max(1.0f, float(std::min(w, h)) / 400.0f));
  arm_overlay(gpd, w, h, true, true);
  draw_sbuffer(gpd, 1.0f, w, h);
  gl_check("3D viewport");
  g_v3d.last_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  g_v3d.sum_ms += g_v3d.last_ms;
  g_v3d.max_ms = std::max(g_v3d.max_ms, g_v3d.last_ms);
  if (++g_v3d.frames % 60 == 0) {
#ifdef PG_HAVE_ANDROID_LOG
    __android_log_print(ANDROID_LOG_INFO, "ProjectGrease", "view3d frames=%ld frame_ms_avg=%.2f frame_ms_max=%.2f",
                        g_v3d.frames, g_v3d.sum_ms / 60.0, g_v3d.max_ms);
#endif
    g_v3d.sum_ms = g_v3d.max_ms = 0;
  }
  return ok;
}

/* ---- the controls (JNI: project_grease_view3d_jni.cpp; touch coordinates: view pixels, y down) ---- */
static void v3d_mval(float sx, float sy, float r[2])
{
  r[0] = sx;
  r[1] = float(g_v3d.v.winy) - sy;
}
extern "C" void project_grease_view3d_set_active(int active)
{
  v3d_ensure(0, 0);
  g_v3d.active = active ? 1 : 0;
}
extern "C" int project_grease_view3d_active() { return g_v3d.active; }
extern "C" void project_grease_view3d_orbit(float dx, float dy, float ui_scale)
{
  v3d_ensure(0, 0);
  pg_view3d_orbit(&g_v3d.v, dx, -dy, ui_scale);
}
extern "C" void project_grease_view3d_pan(float dx, float dy)
{
  v3d_ensure(0, 0);
  pg_view3d_pan(&g_v3d.v, dx, -dy);
}
extern "C" void project_grease_view3d_zoom(float dfac, float sx, float sy)
{
  v3d_ensure(0, 0);
  float m[2];
  v3d_mval(sx, sy, m);
  pg_view3d_zoom(&g_v3d.v, dfac, m);
}
extern "C" void project_grease_view3d_set_axis_view(int view)
{
  v3d_ensure(0, 0);
  pg_view3d_set_axis_view(&g_v3d.v, view);
}
extern "C" void project_grease_view3d_toggle_persp()
{
  v3d_ensure(0, 0);
  pg_view3d_toggle_persp(&g_v3d.v);
}
/* out[11]: persp, view, dist, viewquat[4], ofs[3], last frame ms */
extern "C" void project_grease_view3d_state(float *out)
{
  v3d_ensure(0, 0);
  const PGView3D &v = g_v3d.v;
  const float s[11] = {float(v.persp), float(v.view), v.dist, v.viewquat[0], v.viewquat[1], v.viewquat[2], v.viewquat[3],
                       v.ofs[0], v.ofs[1], v.ofs[2], float(g_v3d.last_ms)};
  std::memcpy(out, s, sizeof s);
}
/* align: gpencil_v3d_align flags (GP_PROJECT_*), lock_axis: GP_LOCKAXIS_* */
extern "C" void project_grease_view3d_set_placement(int align, int lock_axis)
{
  g_v3d.align = align;
  g_v3d.lock_axis = std::clamp(lock_axis, 0, 4);
}
/* Cursor placement (view3d_cursor3d: no surface here, the view plane through the cursor). */
extern "C" void project_grease_view3d_cursor_at(float sx, float sy)
{
  v3d_ensure(0, 0);
  float m[2], ref[3], out[3];
  v3d_mval(sx, sy, m);
  copy_v3_v3(ref, g_v3d.cursor);
  pg_gpencil_project_point(&g_v3d.v, m, ref, PG_LOCKAXIS_VIEW, ref, out);
  copy_v3_v3(g_v3d.cursor, out);
}
/* Object mode click select by origin (ED_view3d_select_dist_px = 75 px * U.pixelsize); a click on
 * nothing deselects all (deselect_all); extend < 0 only tests. Returns the picked object or -1. */
extern "C" int project_grease_view3d_select_at(float sx, float sy, int extend, float ui_scale)
{
  v3d_ensure(0, 0);
  float m[2];
  v3d_mval(sx, sy, m);
  int best = -1;
  float best_d = 75.0f * std::max(ui_scale, 1.0f);
  for (int i = 0; i < int(g_v3d.obs.size()); i++) {
    float s[2];
    if (!pg_view3d_project(&g_v3d.v, g_v3d.obs[size_t(i)].loc, s)) continue;
    const float d = std::hypot(s[0] - m[0], s[1] - m[1]);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  if (extend < 0) return best; /* hit test only */
  if (!extend)
    for (PGV3DObject &ob : g_v3d.obs) ob.selected = 0;
  if (best >= 0) {
    g_v3d.obs[size_t(best)].selected = 1;
    g_v3d.active_ob = best;
  }
  return best;
}
/* Translate (transform G) of the selected objects in the view plane of the active object. */
extern "C" void project_grease_view3d_move_selected(float dx, float dy)
{
  v3d_ensure(0, 0);
  if (g_v3d.obs.empty()) return;
  const float delta[2] = {dx, -dy};
  float dvec[3];
  const float zfac = pg_view3d_calc_zfac(&g_v3d.v, g_v3d.obs[size_t(std::clamp(g_v3d.active_ob, 0, int(g_v3d.obs.size()) - 1))].loc);
  pg_view3d_win_to_delta(&g_v3d.v, delta, zfac, dvec);
  for (PGV3DObject &ob : g_v3d.obs)
    if (ob.selected) add_v3_v3(ob.loc, dvec);
}
/* Adds an empty (2) or the armature object slot (1) at the 3D cursor, selected and active. */
extern "C" int project_grease_view3d_add_object(int type)
{
  if (type != PG_OB_EMPTY && type != PG_OB_ARMATURE) return -1;
  for (PGV3DObject &ob : g_v3d.obs) ob.selected = 0;
  g_v3d.obs.push_back({type, {g_v3d.cursor[0], g_v3d.cursor[1], g_v3d.cursor[2]}, 1});
  g_v3d.active_ob = int(g_v3d.obs.size()) - 1;
  return g_v3d.active_ob;
}
extern "C" int project_grease_view3d_object_count() { return int(g_v3d.obs.size()); }
/* out[5]: type, location, selected */
extern "C" int project_grease_view3d_object_get(int i, float *out)
{
  if (i < 0 || i >= int(g_v3d.obs.size())) return 0;
  const PGV3DObject &ob = g_v3d.obs[size_t(i)];
  out[0] = float(ob.type);
  out[1] = ob.loc[0];
  out[2] = ob.loc[1];
  out[3] = ob.loc[2];
  out[4] = float(ob.selected);
  return 1;
}
/* A stroke drawn in the 3D view was sampled as screen positions through the 2D canvas map (that is
 * how the open stroke is drawn under the finger): place it now, as gpencil_stroke_convertcoords +
 * gpencil_reproject_toplane do at the end of a Blender stroke. */
extern "C" void project_grease_view3d_stroke_done(bGPdata *gpd, bGPDstroke *gps)
{
  if (!g_v3d.active || !gpd || !gps || !g_v3d.inited) return;
  const bool to_plane = (g_v3d.align & PG_PROJECT_VIEWSPACE) && !(g_v3d.align & PG_PROJECT_DEPTH_VIEW);
  const int lock = to_plane ? g_v3d.lock_axis : PG_LOCKAXIS_VIEW;
  float ref[3];
  pg_gpencil_drawing_reference_get(g_v3d.align, g_v3d.obs[0].loc, g_v3d.cursor, ref);
  const float c = std::cos(g_v3d.map_rot), sn = std::sin(g_v3d.map_rot);
  const float W = float(g_v3d.v.winx), H = float(g_v3d.v.winy);
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint &pt = gps->points[i];
    float sx = g_v3d.map_ox + pt.x * g_v3d.map_scale, sy = g_v3d.map_oy + pt.y * g_v3d.map_scale;
    if (g_v3d.map_rot != 0.0f) { /* as ndc() */
      const float dx = sx - 0.5f * W, dy = sy - 0.5f * H;
      sx = 0.5f * W + c * dx - sn * dy;
      sy = 0.5f * H + sn * dx + c * dy;
    }
    float m[2], out[3], cv[3];
    v3d_mval(sx, sy, m);
    pg_gpencil_project_point(&g_v3d.v, m, ref, lock, ref, out);
    v3d_world_to_canvas(out, cv);
    pt.x = cv[0];
    pt.y = cv[1];
    pt.z = cv[2];
  }
  BKE_gpencil_stroke_geometry_update(gpd, gps); /* triangles and bound box of the placed stroke */
}
extern "C" void project_grease_view3d_reset()
{
  g_v3d.active = 0;
  g_v3d.inited = false;
  g_v3d.obs.assign(1, PGV3DObject{PG_OB_GPENCIL, {0, 0, 0}, 1});
  g_v3d.active_ob = 0;
  zero_v3(g_v3d.cursor);
}
/* Where a canvas point (pixels, z off the canvas plane) shows in the 3D view (view pixels, y down);
 * 0 behind the view. */
extern "C" int project_grease_view3d_canvas_to_screen(float x, float y, float z, float *out)
{
  v3d_ensure(0, 0);
  float c[4][4], co[3] = {x, y, z}, s[2];
  v3d_canvas_to_world_m4(c);
  mul_m4_v3(c, co);
  if (!pg_view3d_project(&g_v3d.v, co, s)) return 0;
  out[0] = s[0];
  out[1] = float(g_v3d.v.winy) - s[1];
  return 1;
}
