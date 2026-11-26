/* Armature overlay of the Android presenter (included by android_gp_presentation.cpp after
 * android_gp_view3d.inc): Blender 3.6.23's bone drawing in the 2D Camera view and the 3D view.
 *
 *   draw/engines/overlay/overlay_armature.cc   draw_bone_octahedral, draw_bone_line, draw_points,
 *                                              draw_bone_box (B-Bone), draw_bone_wire (Wire),
 *                                              draw_bone_update_disp_matrix_bbone,
 *                                              get_bone_solid / wire / hint colour, set_pchan_color,
 *                                              draw_bone_update_disp_matrix_default
 *   draw/intern/draw_cache.c                   DRW_cache_bone_octahedral_get (+ _wire_get adjacency),
 *                                              DRW_cache_bone_box_get (+ _wire_get adjacency),
 *                                              DRW_cache_bone_stick_get
 *   draw/intern/draw_common.c                  DRW_globals_update (bone colours, default theme)
 *   overlay_armature_shape_solid_vert/frag, overlay_armature_shape_outline_vert_no_geom +
 *   overlay_armature_wire_frag, overlay_armature_stick_vert/frag   (GLSL ES 3.00 below)
 *
 * Without the depth buffer of Blender's overlay pass, In Front is draw order: off, the bones go
 * under the strokes (drawn before the GP engine image); on, over them. Head / tail spheres of the
 * octahedral display are screen circles (sphere solid + outline seen face on).
 * shortcut: needs a GLES 3 context (ES 3.00 shaders); on GLES 2 no bones are drawn. */
#include <array>

constexpr int ARM_MAX_BBONE_SUBDIV = 32; /* BKE_armature.h MAX_BBONE_SUBDIV */
#include "DNA_armature_types.h"
#include "project_grease_armature.h"

namespace {

/* DRW_globals_update with userdef_default_theme.c space_view3d (sRGB, as the viewport shows them
 * after the display transform): bone_solid 0xb2b2b2, bone_pose 0x50c8ff, bone_pose_active
 * 0x8cffff, wire / wire_edit / vertex 0x000000, vertex_select 0xff7a00, edge_select 0xffa000. */
constexpr float kc = 1.0f / 255.0f;
const float ARM_BONE_SOLID[4] = {0xb2 * kc, 0xb2 * kc, 0xb2 * kc, 1};
const float ARM_BONE_POSE[4] = {0x50 * kc, 0xc8 * kc, 0xff * kc, 1};
const float ARM_BONE_POSE_ACTIVE[4] = {0x8c * kc, 0xff * kc, 0xff * kc, 1};
const float ARM_BONE_POSE_ACTIVE_UNSEL[4] = {12 * kc, 30 * kc, 38 * kc, 1}; /* blend wire / bone_pose 0.15 */
const float ARM_BONE_ACTIVE[4] = {255 * kc, 220 * kc, 60 * kc, 1};         /* edge_select shade +60 */
const float ARM_BONE_SELECT[4] = {235 * kc, 140 * kc, 0, 1};               /* edge_select shade -20 */
const float ARM_BONE_ACTIVE_UNSEL[4] = {38 * kc, 24 * kc, 0, 1};           /* blend wire_edit / edge_select 0.15 */
const float ARM_WIRE[4] = {0, 0, 0, 1};
const float ARM_WIRE_EDIT[4] = {0, 0, 0, 1};
const float ARM_VERTEX[4] = {0, 0, 0, 1};
const float ARM_VERTEX_SELECT[4] = {1, 0x7a * kc, 0, 1};
constexpr int BONE_DRAW_ACTIVE_FLAG = 1 << 30; /* overlay_armature.cc BONE_DRAW_ACTIVE */

const char *arm_solid_vs =
    "#version 300 es\n"
    "in vec3 pos; in vec3 nor; uniform mat4 model_mat, viewmat, winmat; uniform vec4 bone_color, state_color;\n"
    "out vec4 finalColor; flat out int inverted;\n"
    "void main(){mat3 normal_mat=transpose(inverse(mat3(model_mat)));\n"
    " vec3 normal=normalize(mat3(viewmat)*(normal_mat*nor));\n"
    " inverted=int(dot(cross(model_mat[0].xyz,model_mat[1].xyz),model_mat[2].xyz)<0.0);\n"
    " const vec3 light=vec3(0.1,0.1,0.8); float n=dot(normal,light);\n"
    " const float s=0.2; float fac=clamp((n*(1.0-s))+s,0.0,1.0);\n"
    " finalColor.rgb=mix(state_color.rgb,bone_color.rgb,fac*fac); finalColor.a=1.0;\n"
    " gl_Position=winmat*(viewmat*(model_mat*vec4(pos,1.0)));}\n";
const char *arm_solid_fs =
    "#version 300 es\n"
    "precision highp float; in vec4 finalColor; flat in int inverted; uniform float alpha; out vec4 fragColor;\n"
    "void main(){if((inverted==1)==gl_FrontFacing)discard; fragColor=vec4(finalColor.rgb,alpha);}\n";
/* overlay_armature_shape_outline_vert_no_geom.glsl: each vertex carries its line's adjacency (the 4
 * vertex fetches); `corner` picks the line end (x) and, so the wire gets its thickness without the
 * line AA pass, the side of a quad across the outline (y). */
const char *arm_outline_vs =
    "#version 300 es\n"
    "in vec3 pos0, pos1, pos2, pos3, snor0, snor1, snor2, snor3; in vec2 corner;\n"
    "uniform mat4 model_mat, viewmat, winmat; uniform vec4 sizeViewport; uniform vec4 color; uniform float thickness;\n"
    "flat out vec4 finalColor;\n"
    "vec2 proj(vec4 p){return (0.5*(p.xy/p.w)+0.5)*sizeViewport.xy;}\n"
    "void vs(vec3 in_pos,out vec4 pPos,out vec3 vPos,out vec2 ssPos){vec4 v=viewmat*(model_mat*vec4(in_pos,1.0));\n"
    " vPos=v.xyz; pPos=winmat*v; ssPos=proj(pPos);}\n"
    "void main(){vec4 pPos[4]; vec3 vPos[4]; vec2 ssPos[4];\n"
    " vs(pos0,pPos[0],vPos[0],ssPos[0]); vs(pos1,pPos[1],vPos[1],ssPos[1]);\n"
    " vs(pos2,pPos[2],vPos[2],ssPos[2]); vs(pos3,pPos[3],vPos[3],ssPos[3]);\n"
    " int inverted=int(dot(cross(model_mat[0].xyz,model_mat[1].xyz),model_mat[2].xyz)<0.0);\n"
    " finalColor=vec4(color.rgb,1.0); gl_Position=vec4(0.0);\n"
    " bool is_persp=(winmat[3][3]==0.0);\n"
    " vec3 view_vec=(is_persp)?normalize(vPos[1]):vec3(0.0,0.0,-1.0);\n"
    " vec3 v10=vPos[0]-vPos[1]; vec3 v12=vPos[2]-vPos[1]; vec3 v13=vPos[3]-vPos[1];\n"
    " vec3 n0=cross(v12,v10); vec3 n3=cross(v13,v12);\n"
    " float fac0=dot(view_vec,n0); float fac3=dot(view_vec,n3);\n"
    " if(abs(fac0)>1e-5&&abs(fac3)>1e-5){if(sign(fac0)==sign(fac3))return;}\n"
    " n0=(inverted==1)?-n0:n0; if(dot(n0,v13)>0.0001)return;\n"
    " vec2 perp=normalize(ssPos[2]-ssPos[1]); vec2 edge_dir=vec2(-perp.y,perp.x);\n"
    " vec2 hidden_point;\n"
    " if(vPos[0].z<vPos[3].z)hidden_point=(abs(fac0)>1e-5)?ssPos[0]:ssPos[3];\n"
    " else hidden_point=(abs(fac3)>1e-5)?ssPos[3]:ssPos[0];\n"
    " vec2 hidden_dir=normalize(hidden_point-ssPos[1]);\n"
    " float fac=dot(-hidden_dir,edge_dir); edge_dir*=(fac<0.0)?-1.0:1.0;\n"
    " float side=1.0+(corner.y-0.5)*thickness;\n"
    " if(corner.x<0.5){gl_Position=pPos[1]; gl_Position.xy+=(edge_dir*side-perp)*sizeViewport.zw*2.0*gl_Position.w;}\n"
    " else{gl_Position=pPos[2]; gl_Position.xy+=(edge_dir*side+perp)*sizeViewport.zw*2.0*gl_Position.w;}\n"
    " gl_Position.z-=(is_persp)?1e-4:1e-6;}\n";
const char *arm_flat_fs =
    "#version 300 es\n"
    "precision highp float; flat in vec4 finalColor; uniform float alpha; out vec4 fragColor;\n"
    "void main(){fragColor=vec4(finalColor.rgb,finalColor.a*alpha);}\n";
/* overlay_armature_stick_vert/frag.glsl (flag as a float attribute: no integer attributes through
 * the GLES 2 entry points; noperspective colorFac is smooth, ES 3.00 has no noperspective). */
const char *arm_stick_vs =
    "#version 300 es\n"
    "in vec2 pos; in float flag_f; uniform mat4 viewmat, winmat; uniform vec4 sizeViewport; uniform float sizePixel;\n"
    "uniform vec3 boneStart, boneEnd; uniform vec4 wireColor, boneColor, headColor, tailColor;\n"
    "out float colorFac; flat out vec4 finalWireColor; flat out vec4 finalInnerColor;\n"
    "#define COL_WIRE 1u\n#define COL_HEAD 2u\n#define COL_TAIL 4u\n#define COL_BONE 8u\n#define POS_HEAD 16u\n#define POS_BONE 64u\n"
    "vec2 proj(vec4 p){return (0.5*(p.xy/p.w)+0.5)*sizeViewport.xy;}\n"
    "void main(){uint flag=uint(flag_f+0.5); bool is_head=(flag&POS_HEAD)!=0u; bool is_bone=(flag&POS_BONE)!=0u;\n"
    " bool do_wire=wireColor.a>0.0;\n"
    " finalInnerColor=((flag&COL_HEAD)!=0u)?headColor:tailColor;\n"
    " finalInnerColor=((flag&COL_BONE)!=0u)?boneColor:finalInnerColor;\n"
    " finalWireColor=(do_wire)?wireColor:finalInnerColor;\n"
    " colorFac=((flag&COL_WIRE)==0u)?(((flag&COL_BONE)!=0u)?1.0:2.0):0.0;\n"
    " vec4 v0=viewmat*vec4(boneStart,1.0); vec4 v1=viewmat*vec4(boneEnd,1.0);\n"
    " float clip_dist=(winmat[3][3]==0.0)?-1e-7:1e20;\n"
    " vec3 bvec=v1.xyz-v0.xyz; vec3 clip_pt=v0.xyz+bvec*((v0.z-clip_dist)/-bvec.z);\n"
    " if(v0.z>clip_dist)v0.xyz=clip_pt; else if(v1.z>clip_dist)v1.xyz=clip_pt;\n"
    " vec4 p0=winmat*v0; vec4 p1=winmat*v1; float h=(is_head)?p0.w:p1.w;\n"
    " vec2 x_screen_vec=normalize(proj(p1)-proj(p0)+1e-8); vec2 y_screen_vec=vec2(x_screen_vec.y,-x_screen_vec.x);\n"
    " vec2 vpos=pos.x*x_screen_vec+pos.y*y_screen_vec;\n"
    " vpos*=(winmat[3][3]==0.0)?h:1.0; vpos*=(do_wire)?1.0:0.5;\n"
    " if(finalInnerColor.a>0.0){float stick_size=sizePixel*5.0; gl_Position=(is_head)?p0:p1;\n"
    "  gl_Position.xy+=stick_size*(vpos*sizeViewport.zw*2.0); gl_Position.z+=(is_bone)?0.0:1e-6;}\n"
    " else gl_Position=vec4(0.0);}\n";
const char *arm_stick_fs =
    "#version 300 es\n"
    "precision highp float; in float colorFac; flat in vec4 finalWireColor; flat in vec4 finalInnerColor; uniform float alpha;\n"
    "out vec4 fragColor;\n"
    "void main(){float fac=smoothstep(1.0,0.2,colorFac); fragColor.rgb=mix(finalInnerColor.rgb,finalWireColor.rgb,fac); fragColor.a=alpha;}\n";

struct ArmGL {
  bool tried = false, ok = false;
  GLuint solid = 0, outline = 0, stick = 0;
  GLuint solid_vbo = 0, outline_vbo = 0, stick_vbo = 0, box_solid_vbo = 0, box_outline_vbo = 0;
  int outline_verts = 0, stick_verts = 0;
};
ArmGL g_arm;

GLuint arm_link(const char *vsrc, const char *fsrc, std::initializer_list<const char *> attrs)
{
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc), fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return 0;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  GLuint i = 0;
  for (const char *a : attrs) glBindAttribLocation(p, i++, a);
  glLinkProgram(p);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = GL_FALSE;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (ok == GL_FALSE) {
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

/* draw_cache.c: bone_octahedral_verts / _smooth_normals / _solid_tris / _solid_normals /
 * _wire_lines_adjacency, DRW_cache_bone_stick_get. */
const float OCT_VERTS[6][3] = {{0, 0, 0}, {0.1f, 0.1f, 0.1f}, {0.1f, 0.1f, -0.1f}, {-0.1f, 0.1f, -0.1f}, {-0.1f, 0.1f, 0.1f}, {0, 1, 0}};
const float OCT_SNOR[6][3] = {{0, -1, 0}, {0.70710678f, 0, 0.70710678f}, {0.70710678f, 0, -0.70710678f},
                              {-0.70710678f, 0, -0.70710678f}, {-0.70710678f, 0, 0.70710678f}, {0, 1, 0}};
const unsigned OCT_TRIS[8][3] = {{2, 1, 0}, {3, 2, 0}, {4, 3, 0}, {1, 4, 0}, {5, 1, 2}, {5, 2, 3}, {5, 3, 4}, {5, 4, 1}};
const float OCT_TRI_NOR[8][3] = {{0.70710678f, -0.70710678f, 0}, {0, -0.70710678f, -0.70710678f}, {-0.70710678f, -0.70710678f, 0},
                                 {0, -0.70710678f, 0.70710678f}, {0.99388373f, 0.11043154f, 0}, {0, 0.11043154f, -0.99388373f},
                                 {-0.99388373f, 0.11043154f, 0}, {0, 0.11043154f, 0.99388373f}};
const unsigned OCT_ADJ[12][4] = {{0, 1, 2, 6}, {0, 12, 1, 6}, {0, 3, 12, 6}, {0, 2, 3, 6}, {1, 6, 2, 3}, {1, 12, 6, 3},
                                 {1, 0, 12, 3}, {1, 2, 0, 3}, {2, 0, 1, 12}, {2, 3, 0, 12}, {2, 6, 3, 12}, {2, 1, 6, 12}};
/* draw_cache.c: bone_box_verts / _smooth_normals / _solid_tris / _solid_normals /
 * _wire_lines_adjacency */
constexpr float SQ3 = 0.57735027f; /* M_SQRT3 of draw_cache.c is 1 / sqrt(3) */
const float BOX_VERTS[8][3] = {{1, 0, 1}, {1, 0, -1}, {-1, 0, -1}, {-1, 0, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}, {-1, 1, 1}};
const float BOX_SNOR[8][3] = {{SQ3, -SQ3, SQ3}, {SQ3, -SQ3, -SQ3}, {-SQ3, -SQ3, -SQ3}, {-SQ3, -SQ3, SQ3},
                              {SQ3, SQ3, SQ3},  {SQ3, SQ3, -SQ3},  {-SQ3, SQ3, -SQ3},  {-SQ3, SQ3, SQ3}};
const unsigned BOX_TRIS[12][3] = {{0, 2, 1}, {0, 3, 2}, {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
                                  {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}, {4, 5, 6}, {4, 6, 7}};
const float BOX_TRI_NOR[12][3] = {{0, -1, 0}, {0, -1, 0}, {1, 0, 0},  {1, 0, 0},  {0, 0, -1}, {0, 0, -1},
                                  {-1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, 1}, {0, 1, 0},  {0, 1, 0}};
const unsigned BOX_ADJ[12][4] = {{4, 2, 0, 11},   {0, 1, 2, 8},    {2, 4, 1, 14},  {1, 0, 4, 20},
                                 {0, 8, 11, 14},  {2, 14, 8, 20},  {1, 20, 14, 11}, {4, 11, 20, 8},
                                 {20, 0, 11, 2},  {11, 2, 8, 1},   {8, 1, 14, 4},   {14, 4, 20, 0}};
enum { STK_COL_WIRE = 1, STK_COL_HEAD = 2, STK_COL_TAIL = 4, STK_COL_BONE = 8, STK_POS_HEAD = 16, STK_POS_TAIL = 32, STK_POS_BONE = 64 };

bool arm_gl()
{
  if (g_arm.tried) return g_arm.ok;
  g_arm.tried = true;
  g_arm.solid = arm_link(arm_solid_vs, arm_solid_fs, {"pos", "nor"});
  g_arm.outline = arm_link(arm_outline_vs, arm_flat_fs, {"pos0", "pos1", "pos2", "pos3", "snor0", "snor1", "snor2", "snor3", "corner"});
  g_arm.stick = arm_link(arm_stick_vs, arm_stick_fs, {"pos", "flag_f"});
  if (!g_arm.solid || !g_arm.outline || !g_arm.stick) return false;
  /* the solid vertices: pos, nor (snor of the outline comes with the adjacency below); outline:
   * per adjacency line 6 vertices (a quad), each 4 x (pos, snor) + corner */
  auto shape = [](int ntri, const unsigned (*tris)[3], const float (*verts)[3], const float (*tri_nor)[3], const float (*snor)[3],
                  const unsigned (*adj)[4], std::vector<float> &solid, std::vector<float> &outline) {
    std::vector<float> snor_pos;
    for (int i = 0; i < ntri; i++)
      for (int j = 0; j < 3; j++) {
        const unsigned v = tris[i][j];
        solid.insert(solid.end(), {verts[v][0], verts[v][1], verts[v][2], tri_nor[i][0], tri_nor[i][1], tri_nor[i][2]});
        snor_pos.insert(snor_pos.end(), {verts[v][0], verts[v][1], verts[v][2], snor[v][0], snor[v][1], snor[v][2]});
      }
    const float corners[6][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
    for (int li = 0; li < 12; li++)
      for (const auto &c : corners) {
        const unsigned *l = adj[li];
        for (int k = 0; k < 4; k++) outline.insert(outline.end(), &snor_pos[l[k] * 6], &snor_pos[l[k] * 6] + 3);
        for (int k = 0; k < 4; k++) outline.insert(outline.end(), &snor_pos[l[k] * 6 + 3], &snor_pos[l[k] * 6 + 3] + 3);
        outline.insert(outline.end(), {c[0], c[1]});
      }
  };
  std::vector<float> solid, outline, box_solid, box_outline;
  shape(8, OCT_TRIS, OCT_VERTS, OCT_TRI_NOR, OCT_SNOR, OCT_ADJ, solid, outline);
  shape(12, BOX_TRIS, BOX_VERTS, BOX_TRI_NOR, BOX_SNOR, BOX_ADJ, box_solid, box_outline);
  g_arm.outline_verts = 12 * 6; /* both shapes have 12 outline edges */
  /* stick: the triangle fans (head circle, tail circle, bone rectangle) as triangles */
  std::vector<float> fan_v;
  auto vert = [&](float x, float y, unsigned f) { fan_v.insert(fan_v.end(), {x, y, float(f)}); };
  std::vector<float> stick;
  auto tri = [&](int a, int b, int c) {
    for (int k : {a, b, c}) stick.insert(stick.end(), &fan_v[size_t(k) * 3], &fan_v[size_t(k) * 3] + 3);
  };
  constexpr int RES = 12;
  for (int i = 0; i < 2; i++) {
    const int base = int(fan_v.size() / 3);
    unsigned flag = (i == 0 ? STK_POS_HEAD : STK_POS_TAIL) | (i == 0 ? STK_COL_HEAD : STK_COL_TAIL);
    vert(0, 0, flag);
    flag |= STK_COL_WIRE;
    for (int a = 0; a < RES; a++) vert(2.0f * std::sin(6.2831853f * a / RES), 2.0f * std::cos(6.2831853f * a / RES), flag);
    for (int a = 0; a < RES; a++) tri(base, base + 1 + a, base + 1 + (a + 1) % RES);
  }
  const int rb = int(fan_v.size() / 3);
  for (int i = 0; i < 6; i++) {
    const float y = (i == 0 || i == 3) ? 0.0f : (i < 3 ? 1.0f : -1.0f);
    vert(0, y, ((i < 2 || i > 4) ? STK_POS_HEAD : STK_POS_TAIL) | ((i == 0 || i == 3) ? 0 : STK_COL_WIRE) | STK_COL_BONE | STK_POS_BONE);
  }
  for (int i = 1; i < 5; i++) tri(rb, rb + i, rb + i + 1);
  g_arm.stick_verts = int(stick.size() / 3);
  GLuint b[5];
  glGenBuffers(5, b);
  g_arm.box_solid_vbo = b[3];
  g_arm.box_outline_vbo = b[4];
  glBindBuffer(GL_ARRAY_BUFFER, b[3]);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(box_solid.size() * sizeof(float)), box_solid.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, b[4]);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(box_outline.size() * sizeof(float)), box_outline.data(), GL_STATIC_DRAW);
  g_arm.solid_vbo = b[0];
  g_arm.outline_vbo = b[1];
  g_arm.stick_vbo = b[2];
  glBindBuffer(GL_ARRAY_BUFFER, b[0]);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(solid.size() * sizeof(float)), solid.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, b[1]);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(outline.size() * sizeof(float)), outline.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, b[2]);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(stick.size() * sizeof(float)), stick.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  g_arm.ok = true;
  return true;
}

void arm_attribs(int count, const int *sizes)
{
  int stride = 0;
  for (int i = 0; i < count; i++) stride += sizes[i];
  int off = 0;
  for (int i = 0; i < count; i++) {
    glEnableVertexAttribArray(GLuint(i));
    glVertexAttribPointer(GLuint(i), sizes[i], GL_FLOAT, GL_FALSE, stride * 4, reinterpret_cast<const void *>(intptr_t(off * 4)));
    off += sizes[i];
  }
}
void arm_attribs_off(int count)
{
  for (int i = 0; i < count; i++) glDisableVertexAttribArray(GLuint(i));
}

void bone_hint_color_shade(float hint[4], const float c[4])
{
  for (int i = 0; i < 3; i++) hint[i] = c[i] * c[i] * 0.1f;
  hint[3] = 1.0f;
}

struct ArmBoneDraw {
  float disp_mat[4][4];
  float head[3], tail[3];
  int flag; /* EditBone flag | BONE_DRAW_ACTIVE */
  bool head_shown;
  std::vector<std::array<float, 16>> segs; /* world display matrices: disp_mat, or the B-Bone segments */
};

/* get_bone_wire_color (no bone colour sets, no constraints) */
void arm_wire_color(int mode, int boneflag, float r[4])
{
  const bool sel = boneflag & BONE_SELECTED, act = boneflag & BONE_DRAW_ACTIVE_FLAG;
  const float *c;
  if (mode == PG_ARM_MODE_EDIT) c = sel ? (act ? ARM_BONE_ACTIVE : ARM_BONE_SELECT) : (act ? ARM_BONE_ACTIVE_UNSEL : ARM_WIRE_EDIT);
  else if (mode == PG_ARM_MODE_POSE) /* set_pchan_color PCHAN_COLOR_NORMAL */
    c = (act && sel) ? ARM_BONE_POSE_ACTIVE : act ? ARM_BONE_POSE_ACTIVE_UNSEL : sel ? ARM_BONE_POSE : ARM_WIRE;
  else c = ARM_VERTEX;
  copy_v3_v3(r, c);
  r[3] = (boneflag & (BONE_DRAW_ACTIVE_FLAG | BONE_SELECTED)) ? 2.0f : 1.0f; /* get_bone_wire_thickness */
}

/* A screen circle (window coordinates, y up) as triangles in NDC. */
void arm_disc(std::vector<Vertex> &out, const float c[2], float r, int w, int h)
{
  auto n = [&](float x, float y) { return Vertex{2.0f * x / float(w) - 1.0f, 2.0f * y / float(h) - 1.0f}; };
  const int seg = std::clamp(int(r * 0.5f), 12, 48);
  for (int i = 0; i < seg; i++) {
    const float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
    out.insert(out.end(), {n(c[0], c[1]), n(c[0] + r * std::cos(a0), c[1] + r * std::sin(a0)), n(c[0] + r * std::cos(a1), c[1] + r * std::sin(a1))});
  }
}

/* draw_points: head (when not connected) and tail spheres, radius PT_DEFAULT_RAD of the
 * length, solid colour with the wire colour (vertex_select for a selected edit joint) around */
void arm_draw_points(const std::vector<ArmBoneDraw> &bones, int mode, const float viewmat[4][4], const float winmat[4][4], int w,
                     int h, float px)
{
  const float vscale = len_v3(viewmat[0]);
  for (const ArmBoneDraw &b : bones) {
    float wire[4];
    arm_wire_color(mode, b.flag, wire);
    for (int end = 0; end < 2; end++) {
      if (end == 0 && !b.head_shown) continue;
      float col_wire[4];
      if (mode == PG_ARM_MODE_POSE) copy_v4_v4(col_wire, wire);
      else {
        copy_v3_v3(col_wire, ARM_VERTEX);
        col_wire[3] = wire[3];
        if (mode == PG_ARM_MODE_EDIT && (b.flag & (end == 0 ? BONE_ROOTSEL : BONE_TIPSEL))) copy_v3_v3(col_wire, ARM_VERTEX_SELECT);
      }
      const float *c = end == 0 ? b.head : b.tail;
      float v[4] = {c[0], c[1], c[2], 1.0f}, vv[4];
      mul_v4_m4v4(vv, viewmat, v);
      const float rad = 0.05f * len_v3(b.disp_mat[1]) * vscale; /* world length of the bone */
      float q[4] = {vv[0] + rad, vv[1], vv[2], 1.0f}, pc[4], pq[4];
      mul_v4_m4v4(pc, winmat, vv);
      mul_v4_m4v4(pq, winmat, q);
      if (pc[3] <= 0.0f) continue;
      const float s[2] = {(pc[0] / pc[3] * 0.5f + 0.5f) * float(w), (pc[1] / pc[3] * 0.5f + 0.5f) * float(h)};
      const float r = std::fabs((pq[0] / pq[3] - pc[0] / pc[3]) * 0.5f * float(w));
      std::vector<Vertex> ring, dot;
      arm_disc(ring, s, r + col_wire[3] * px, w, h);
      arm_disc(dot, s, r, w, h);
      col_wire[3] = 1.0f;
      draw_vertices(ring, col_wire, false);
      draw_vertices(dot, ARM_BONE_SOLID, false);
    }
  }
}

/* The bones of `a` with obmat (armature space -> world, the rig object's object_to_world), viewmat
 * and winmat, in a w x h window; px is U.pixelsize. */
void arm_draw(const PGArmature *a, const float obmat[4][4], const float viewmat[4][4], const float winmat[4][4], int w, int h,
              float px)
{
  const int nb = pg_arm_bone_count(a);
  if (nb <= 0 || !arm_gl()) return;
  const int mode = pg_arm_mode(a), active = pg_arm_active_bone(a);
  const int drawtype = pg_arm_display_type(a);
  const bool stick = drawtype == ARM_LINE;
  /* Envelope is not ported: drawn as Octahedral */
  const bool bbone = drawtype == ARM_B_BONE, wire_draw = drawtype == eArmature_Drawtype::ARM_WIRE; /* ARM_WIRE is also the wire colour here */
  std::vector<ArmBoneDraw> bones;
  bones.reserve(size_t(nb));
  float info[PG_ARM_BONE_INFO];
  for (int i = 0; i < nb; i++) {
    if (!pg_arm_bone_info(a, i, info)) continue;
    ArmBoneDraw b;
    /* draw_bone_update_disp_matrix_default: edit bones their rest matrix, else pose_mat; scaled by
     * the length, the tail at local y = 1 */
    float m[4][4];
    std::memcpy(m, mode == PG_ARM_MODE_EDIT ? info + 10 : info + 32, sizeof m);
    for (int c = 0; c < 3; c++) mul_v3_fl(m[c], info[9]);
    /* drw_shgroup_bone_*: ob->object_to_world * bone_mat */
    mul_m4_m4m4(b.disp_mat, obmat, m);
    mul_v3_m4v3(b.head, obmat, m[3]);
    add_v3_v3v3(b.tail, m[3], m[1]);
    mul_m4_v3(obmat, b.tail);
    b.flag = int(info[8]) | (i == active ? BONE_DRAW_ACTIVE_FLAG : 0);
    b.head_shown = !(info[7] >= 0.0f && (b.flag & BONE_CONNECTED));
    if (bbone || wire_draw) {
      float segm[ARM_MAX_BBONE_SUBDIV][4][4];
      const int ns = pg_arm_bone_bbone_matrices(a, i, segm, ARM_MAX_BBONE_SUBDIV);
      for (int k = 0; k < ns; k++) {
        std::array<float, 16> o;
        mul_m4_m4m4(reinterpret_cast<float(*)[4]>(o.data()), obmat, segm[k]);
        b.segs.push_back(o);
      }
    }
    else {
      std::array<float, 16> o;
      std::memcpy(o.data(), b.disp_mat, sizeof b.disp_mat);
      b.segs.push_back(o);
    }
    bones.push_back(b);
  }
  const float vp[4] = {float(w), float(h), 1.0f / float(w), 1.0f / float(h)};
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  if (stick) {
    /* draw_bone_line */
    const GLuint p = g_arm.stick;
    glUseProgram(p);
    glUniformMatrix4fv(glGetUniformLocation(p, "viewmat"), 1, GL_FALSE, &viewmat[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(p, "winmat"), 1, GL_FALSE, &winmat[0][0]);
    glUniform4fv(glGetUniformLocation(p, "sizeViewport"), 1, vp);
    glUniform1f(glGetUniformLocation(p, "sizePixel"), px);
    glUniform1f(glGetUniformLocation(p, "alpha"), 1.0f);
    glBindBuffer(GL_ARRAY_BUFFER, g_arm.stick_vbo);
    const int sz[2] = {2, 1};
    arm_attribs(2, sz);
    const float no_display[4] = {0, 0, 0, 0};
    for (const ArmBoneDraw &b : bones) {
      float wire[4];
      arm_wire_color(mode, b.flag, wire);
      const float *col_bone = ARM_BONE_SOLID, *col_head = no_display, *col_tail = col_bone;
      const float *col_wire = wire;
      if (mode == PG_ARM_MODE_EDIT) {
        if (b.flag & BONE_TIPSEL) col_tail = ARM_VERTEX_SELECT;
        if (b.flag & BONE_SELECTED) col_bone = ARM_BONE_ACTIVE;
        col_wire = ARM_WIRE;
      }
      if (b.head_shown) col_head = (mode == PG_ARM_MODE_EDIT && (b.flag & BONE_ROOTSEL)) ? ARM_VERTEX_SELECT : col_bone;
      glUniform3fv(glGetUniformLocation(p, "boneStart"), 1, b.head);
      glUniform3fv(glGetUniformLocation(p, "boneEnd"), 1, b.tail);
      glUniform4fv(glGetUniformLocation(p, "wireColor"), 1, col_wire);
      glUniform4fv(glGetUniformLocation(p, "boneColor"), 1, col_bone);
      glUniform4fv(glGetUniformLocation(p, "headColor"), 1, col_head);
      glUniform4fv(glGetUniformLocation(p, "tailColor"), 1, col_tail);
      glDrawArrays(GL_TRIANGLES, 0, g_arm.stick_verts);
    }
    arm_attribs_off(2);
  }
  else if (wire_draw) {
    /* draw_bone_wire: a line per segment (drw_shgroup_bone_wire, head = mat[3], tail = head +
     * mat[1]) in the wire colour, line width get_bone_wire_thickness * pixelsize; draw_points in
     * edit mode only */
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    float persmat[4][4];
    mul_m4_m4m4(persmat, winmat, viewmat);
    for (const ArmBoneDraw &b : bones) {
      float wire[4];
      arm_wire_color(mode, b.flag, wire);
      std::vector<Vertex> quads;
      for (const auto &m : b.segs) {
        const float(*mm)[4] = reinterpret_cast<const float(*)[4]>(m.data());
        float p0[4] = {mm[3][0], mm[3][1], mm[3][2], 1.0f}, p1[4] = {mm[3][0] + mm[1][0], mm[3][1] + mm[1][1], mm[3][2] + mm[1][2], 1.0f};
        mul_m4_v4(persmat, p0);
        mul_m4_v4(persmat, p1);
        if (p0[3] <= 0.0f || p1[3] <= 0.0f) continue;
        const float s0[2] = {(p0[0] / p0[3] * 0.5f + 0.5f) * float(w), (p0[1] / p0[3] * 0.5f + 0.5f) * float(h)};
        const float s1[2] = {(p1[0] / p1[3] * 0.5f + 0.5f) * float(w), (p1[1] / p1[3] * 0.5f + 0.5f) * float(h)};
        float d[2] = {s1[0] - s0[0], s1[1] - s0[1]};
        const float l = std::max(1e-6f, std::hypot(d[0], d[1]));
        const float hw = 0.5f * wire[3] * px;
        const float nx = -d[1] / l * hw, ny = d[0] / l * hw;
        auto n = [&](float x, float y) { return Vertex{2.0f * x / float(w) - 1.0f, 2.0f * y / float(h) - 1.0f}; };
        quads.insert(quads.end(), {n(s0[0] + nx, s0[1] + ny), n(s1[0] + nx, s1[1] + ny), n(s1[0] - nx, s1[1] - ny),
                                   n(s0[0] + nx, s0[1] + ny), n(s1[0] - nx, s1[1] - ny), n(s0[0] - nx, s0[1] - ny)});
      }
      float c[4] = {wire[0], wire[1], wire[2], 1.0f};
      draw_vertices(quads, c, false);
    }
    glDisable(GL_BLEND);
    if (mode != PG_ARM_MODE_EDIT) return;
    arm_draw_points(bones, mode, viewmat, winmat, w, h, px);
    return;
  }
  else {
    /* draw_bone_octahedral / draw_bone_box: solid (hint / solid colour), then the outlines (wire colour) */
    GLuint p = g_arm.solid;
    glUseProgram(p);
    glUniformMatrix4fv(glGetUniformLocation(p, "viewmat"), 1, GL_FALSE, &viewmat[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(p, "winmat"), 1, GL_FALSE, &winmat[0][0]);
    glUniform1f(glGetUniformLocation(p, "alpha"), 1.0f);
    glBindBuffer(GL_ARRAY_BUFFER, bbone ? g_arm.box_solid_vbo : g_arm.solid_vbo);
    const int sz_s[2] = {3, 3};
    arm_attribs(2, sz_s);
    for (const ArmBoneDraw &b : bones) {
      float wire[4], hint[4];
      arm_wire_color(mode, b.flag, wire);
      bone_hint_color_shade(hint, wire);
      glUniform4fv(glGetUniformLocation(p, "bone_color"), 1, ARM_BONE_SOLID);
      glUniform4fv(glGetUniformLocation(p, "state_color"), 1, hint);
      for (const auto &m : b.segs) {
        glUniformMatrix4fv(glGetUniformLocation(p, "model_mat"), 1, GL_FALSE, m.data());
        glDrawArrays(GL_TRIANGLES, 0, bbone ? 36 : 24);
      }
    }
    arm_attribs_off(2);
    p = g_arm.outline;
    glUseProgram(p);
    glUniformMatrix4fv(glGetUniformLocation(p, "viewmat"), 1, GL_FALSE, &viewmat[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(p, "winmat"), 1, GL_FALSE, &winmat[0][0]);
    glUniform4fv(glGetUniformLocation(p, "sizeViewport"), 1, vp);
    glUniform1f(glGetUniformLocation(p, "alpha"), 1.0f);
    glBindBuffer(GL_ARRAY_BUFFER, bbone ? g_arm.box_outline_vbo : g_arm.outline_vbo);
    const int sz_o[9] = {3, 3, 3, 3, 3, 3, 3, 3, 2};
    arm_attribs(9, sz_o);
    for (const ArmBoneDraw &b : bones) {
      float wire[4];
      arm_wire_color(mode, b.flag, wire);
      glUniform4fv(glGetUniformLocation(p, "color"), 1, wire);
      glUniform1f(glGetUniformLocation(p, "thickness"), wire[3] * px);
      for (const auto &m : b.segs) {
        glUniformMatrix4fv(glGetUniformLocation(p, "model_mat"), 1, GL_FALSE, m.data());
        glDrawArrays(GL_TRIANGLES, 0, g_arm.outline_verts);
      }
    }
    arm_attribs_off(9);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
    /* draw_bone_box draws the points in edit mode only */
    if (!bbone || mode == PG_ARM_MODE_EDIT) arm_draw_points(bones, mode, viewmat, winmat, w, h, px);
    return;
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);
  glDisable(GL_BLEND);
}

/* The 2D Camera view: armature space -> window pixels (y up) of the canvas map (ndc()). */
void arm_matrices_2d(int w, int h, float obmat[4][4], float viewmat[4][4], float winmat[4][4])
{
  const float k = PG_ARM_PIXELS_PER_UNIT * g_cfg->map_scale;
  unit_m4(obmat); /* the map is the view: armature space -> window px, y up */
  unit_m4(viewmat);
  viewmat[0][0] = viewmat[1][1] = viewmat[2][2] = k;
  viewmat[3][0] = g_cfg->map_origin_x;
  viewmat[3][1] = float(h) - g_cfg->map_origin_y;
  if (g_cfg->view_rotation != 0.0f && !g_cfg->export_mode) {
    /* ndc() turns screen (y down) by +rotation about the centre: -rotation with y up */
    float r[4][4], t[4][4];
    unit_m4(r);
    const float c = std::cos(g_cfg->view_rotation), sn = std::sin(g_cfg->view_rotation);
    r[0][0] = c, r[0][1] = -sn, r[1][0] = sn, r[1][1] = c;
    r[3][0] = 0.5f * float(w) - (c * 0.5f * float(w) + sn * 0.5f * float(h));
    r[3][1] = 0.5f * float(h) - (-sn * 0.5f * float(w) + c * 0.5f * float(h));
    mul_m4_m4m4(t, r, viewmat);
    copy_m4_m4(viewmat, t);
  }
  unit_m4(winmat);
  winmat[0][0] = 2.0f / float(w);
  winmat[1][1] = 2.0f / float(h);
  winmat[2][2] = -1e-6f; /* ortho, depth unused */
  winmat[3][0] = winmat[3][1] = -1.0f;
}
/* The 3D view: armature -> canvas px -> world (v3d_canvas_to_world_m4) -> view. */
void arm_matrices_3d(float obmat[4][4], float viewmat[4][4], float winmat[4][4])
{
  float a2c[4][4], c2w[4][4];
  unit_m4(a2c);
  a2c[0][0] = PG_ARM_PIXELS_PER_UNIT;
  a2c[1][1] = -PG_ARM_PIXELS_PER_UNIT;
  a2c[2][2] = PG_ARM_PIXELS_PER_UNIT;
  v3d_canvas_to_world_m4(c2w);
  mul_m4_m4m4(obmat, c2w, a2c);
  copy_m4_m4(viewmat, g_v3d.v.viewmat);
  copy_m4_m4(winmat, g_v3d.v.winmat);
}

}  // namespace

/* The overlay pass of one frame: `front` is the pass after the strokes (In Front bones), the other
 * one goes under them. No rig, no work. */
static void arm_overlay(const bGPdata *gpd, int w, int h, bool front, bool view3d)
{
  if (g_cfg->export_mode) return;
  const PGArmature *a = pg_arm_for_gpd(gpd);
  if (!a || pg_arm_show_in_front(a) != front || pg_arm_bone_count(a) <= 0) return;
  float obmat[4][4], viewmat[4][4], winmat[4][4];
  if (view3d) arm_matrices_3d(obmat, viewmat, winmat);
  else arm_matrices_2d(w, h, obmat, viewmat, winmat);
  glViewport(0, 0, w, h);
  arm_draw(a, obmat, viewmat, winmat, w, h, std::max(1.0f, float(std::min(w, h)) / 400.0f));
  gl_check("armature overlay");
}

/* Canvas px -> window px (y down) of the 3D view, column major, for PG_ARM_CMD_PICK. */
extern "C" void project_grease_view3d_canvas_matrix(float *out)
{
  v3d_ensure(0, 0);
  float c2w[4][4], m[4][4], flip[4][4];
  v3d_canvas_to_world_m4(c2w);
  mul_m4_m4m4(m, g_v3d.v.persmat, c2w);
  /* NDC -> window, y down */
  unit_m4(flip);
  flip[0][0] = 0.5f * float(g_v3d.v.winx);
  flip[3][0] = 0.5f * float(g_v3d.v.winx);
  flip[1][1] = -0.5f * float(g_v3d.v.winy);
  flip[3][1] = 0.5f * float(g_v3d.v.winy);
  float r[4][4];
  mul_m4_m4m4(r, flip, m);
  std::memcpy(out, r, sizeof r);
}
