// Shader effects of a layer on GLES2 (see project_grease_shader_fx.h for the design). The layer is
// drawn into an offscreen buffer by the presenter, the passes built by pg_fx_build_passes() run
// here as fragment shaders in Blender's colour/revealage representation, and the result is
// composited onto the frame as frame = frame * R + C (gpencil_vfx_frag.glsl COMPOSITE).
//
// GLES2 has no multiple render targets, so every pass runs twice (once for the colour buffer, once
// for the revealage buffer) and the GL blend state of Blender's passes is evaluated in the shader
// from a copy of the buffer it blends onto. The shader math is the same as the C executor of
// project_grease_shader_fx.c, which the host tests compare against.
#include <GLES2/gl2.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "project_grease_shader_fx.h"

namespace {

// rb: stencil renderbuffer of the layer plane (strokes are drawn into it with per-stroke coverage).
struct Plane { GLuint tex = 0, fbo = 0, rb = 0; };
struct Buffer { Plane color, reveal; };

int g_w = 0, g_h = 0;
Plane g_layer;            // the layer as drawn: premultiplied rgba
Buffer g_bufs[3];
GLuint g_vbo = 0;
GLint g_prev_fbo = 0;
GLint g_prev_viewport[4] = {0, 0, 0, 0};

const char* vs_src() {
  return "attribute vec2 a_position; void main(){gl_Position=vec4(a_position,0.0,1.0);}";
}

// Everything the passes need is declared once; unused uniforms are optimised away.
const char* fs_header() {
  return
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\nprecision highp int;\n"
    "#else\nprecision mediump float;\nprecision mediump int;\n#endif\n"
    "uniform sampler2D colorBuf; uniform sampler2D revealBuf; uniform sampler2D dstColor; uniform sampler2D dstReveal;\n"
    "uniform vec2 uSize; uniform int uOut; uniform int uBlend;\n"
    "uniform vec2 offset; uniform int sampCount;\n"
    "uniform vec3 lowColor; uniform vec3 highColor; uniform float factor; uniform int mode;\n"
    "uniform vec2 axisFlip; uniform vec2 waveDir; uniform vec2 waveOffset; uniform float wavePhase;\n"
    "uniform float swirlRadius; uniform vec2 swirlCenter; uniform float swirlAngle;\n"
    "uniform vec2 targetPixelSize; uniform vec2 targetPixelOffset; uniform vec2 accumOffset;\n"
    "uniform vec4 threshold; uniform vec4 glowColor; uniform int glowUnder; uniform int firstPass; uniform int blendMode;\n"
    "uniform vec2 blurDir; uniform vec2 uvOffset; uniform vec2 uvRotX; uniform vec2 uvRotY;\n"
    "uniform vec3 maskColor; uniform vec3 rimColor; uniform vec4 shadowColor;\n"
    "vec4 fragColor; vec4 fragRevealage;\n"
    "float gaussian_weight(float x){return exp(-x * x / (2.0 * 0.35 * 0.35));}\n"
    "vec4 doBlend(vec4 s, vec4 d){\n"
    "  if (uBlend == 1) return s + d * (1.0 - s.a);\n"
    "  if (uBlend == 2) return s + d;\n"
    "  if (uBlend == 3) return d - s;\n"
    "  if (uBlend == 4) return s * d;\n"
    "  return s;\n}\n"
    // blend_mode_output() of gpencil_common_lib.glsl
    "void blend_mode_output(int bm, vec4 color, float opacity, out vec4 fc, out vec4 fr){\n"
    "  fc = vec4(0.0); fr = vec4(0.0);\n"
    "  if (bm == 0) { color *= opacity; fc = color; fr = vec4(0.0, 0.0, 0.0, color.a); }\n"
    "  else if (bm == 4) { color.a *= opacity; fr = fc = (1.0 - color.a) + color.a * color; }\n"
    "  else if (bm == 5) { color.a *= opacity; fr = fc = clamp(1.0 / max(vec4(1e-6), 1.0 - color * color.a), 0.0, 1e18); }\n"
    "  else if (bm == 1) { color = mix(vec4(0.5), color, color.a * opacity); vec4 s = step(-0.5, -color);\n"
    "    fr = fc = 2.0 * s + 2.0 * color * (1.0 - s * 2.0); fr = max(vec4(0.0), fr); }\n"
    "  else if (bm == 999) { color = mix(vec4(0.5), color, color.a * opacity);\n"
    "    fr = fc = (-1.0 + 2.0 * color) * step(-0.5, -color); fr = max(vec4(0.0), fr); }\n"
    "  else if (bm == 2 || bm == 3) { fc = color * color.a * opacity; fr = vec4(0.0); }\n"
    "}\n";
}

const char* kind_body(int kind) {
  switch (kind) {
    case PGFX_PASS_COLORIZE:
      return
        "void main(){ vec2 uv = gl_FragCoord.xy / uSize;\n"
        "  fragColor = texture2D(colorBuf, uv); fragRevealage = texture2D(revealBuf, uv);\n"
        "  float luma = dot(fragColor.rgb, vec3(0.2126, 0.7152, 0.723));\n"
        "  mat3 sepia = mat3(vec3(0.393, 0.349, 0.272), vec3(0.769, 0.686, 0.534), vec3(0.189, 0.168, 0.131));\n"
        "  if (mode == 0) fragColor.rgb = mix(fragColor.rgb, vec3(luma), factor);\n"
        "  else if (mode == 1) fragColor.rgb = mix(fragColor.rgb, sepia * fragColor.rgb, factor);\n"
        "  else if (mode == 2) fragColor.rgb = luma * ((luma <= factor) ? lowColor : highColor);\n"
        "  else if (mode == 3) fragColor.rgb = mix(fragColor.rgb, luma * lowColor, factor);\n"
        "  else { fragColor.rgb *= factor; fragRevealage.rgb = mix(vec3(1.0), fragRevealage.rgb, factor); }\n";
    case PGFX_PASS_BLUR:
      return
        "void main(){ vec2 uv0 = gl_FragCoord.xy / uSize; vec2 pixel_size = 1.0 / uSize; vec2 ofs = offset * pixel_size;\n"
        "  fragColor = vec4(0.0); fragRevealage = vec4(0.0); float weight_accum = 0.0;\n"
        "  for (int i = -32; i <= 32; i++) { if (i < -sampCount || i > sampCount) continue;\n"
        "    float x = float(i) / float(sampCount); float weight = gaussian_weight(x); weight_accum += weight;\n"
        "    vec2 uv = uv0 + ofs * x;\n"
        "    fragColor.rgb += texture2D(colorBuf, uv).rgb * weight; fragRevealage.rgb += texture2D(revealBuf, uv).rgb * weight; }\n"
        "  fragColor /= weight_accum; fragRevealage /= weight_accum;\n";
    case PGFX_PASS_TRANSFORM:
      return
        "void main(){ vec2 uv0 = gl_FragCoord.xy / uSize;\n"
        "  vec2 uv = (uv0 - 0.5) * axisFlip + 0.5;\n"
        "  float wave_time = dot(uv, waveDir.xy); uv += sin(wave_time + wavePhase) * waveOffset;\n"
        "  if (swirlRadius > 0.0) { vec2 tex_size = uSize; vec2 pix_coord = uv * tex_size - swirlCenter;\n"
        "    float dist = length(pix_coord); float percent = clamp((swirlRadius - dist) / swirlRadius, 0.0, 1.0);\n"
        "    float theta = percent * percent * swirlAngle; float s = sin(theta); float c = cos(theta);\n"
        "    mat2 rot = mat2(vec2(c, -s), vec2(s, c)); uv = (rot * pix_coord + swirlCenter) / tex_size; }\n"
        "  fragColor = texture2D(colorBuf, uv); fragRevealage = texture2D(revealBuf, uv);\n";
    case PGFX_PASS_PIXELIZE:
      return
        "void main(){ vec2 uv0 = gl_FragCoord.xy / uSize;\n"
        "  vec2 pixel = floor((uv0 - targetPixelOffset) / targetPixelSize);\n"
        "  vec2 uv = (pixel + 0.5) * targetPixelSize + targetPixelOffset;\n"
        "  fragColor = vec4(0.0); fragRevealage = vec4(0.0);\n"
        "  for (int i = -2; i <= 2; i++) { if (i < -sampCount || i > sampCount) continue;\n"
        "    float x = float(i) / float(sampCount + 1); vec2 uv_ofs = uv + accumOffset * 0.5 * x;\n"
        "    fragColor += texture2D(colorBuf, uv_ofs); fragRevealage += texture2D(revealBuf, uv_ofs); }\n"
        "  fragColor /= float(sampCount) * 2.0 + 1.0; fragRevealage /= float(sampCount) * 2.0 + 1.0;\n";
    case PGFX_PASS_GLOW:
      return
        "void main(){ vec2 uv0 = gl_FragCoord.xy / uSize; vec2 pixel_size = 1.0 / uSize; vec2 ofs = offset * pixel_size;\n"
        "  fragColor = vec4(0.0); fragRevealage = vec4(0.0); float weight_accum = 0.0;\n"
        "  for (int i = -32; i <= 32; i++) { if (i < -sampCount || i > sampCount) continue;\n"
        "    float x = float(i) / float(sampCount); float weight = gaussian_weight(x); weight_accum += weight;\n"
        "    vec2 uv = uv0 + ofs * x; vec3 col = texture2D(colorBuf, uv).rgb; vec3 rev = texture2D(revealBuf, uv).rgb;\n"
        "    if (threshold.x > -1.0) {\n"
        "      if (threshold.y > -1.0) { if (any(greaterThan(abs(col - vec3(threshold)), vec3(threshold.w)))) weight = 0.0; }\n"
        "      else { if (dot(col, vec3(1.0 / 3.0)) < threshold.x) weight = 0.0; } }\n"
        "    fragColor.rgb += col * weight; fragRevealage.rgb += (1.0 - rev) * weight; }\n"
        "  if (weight_accum > 0.0) { fragColor *= glowColor.rgbb / weight_accum; fragRevealage = fragRevealage / weight_accum; }\n"
        "  fragRevealage = 1.0 - fragRevealage;\n"
        "  if (glowUnder != 0) {\n"
        "    if (firstPass != 0) { vec3 original_revealage = texture2D(revealBuf, uv0).rgb;\n"
        "      fragRevealage.a = clamp(dot(original_revealage.rgb, vec3(0.333334)), 0.0, 1.0); }\n"
        "    else { fragRevealage.a = texture2D(revealBuf, uv0).a; } }\n"
        "  if (firstPass == 0) { fragColor.a = clamp(1.0 - dot(fragRevealage.rgb, vec3(0.333334)), 0.0, 1.0);\n"
        "    fragRevealage.a *= glowColor.a; blend_mode_output(blendMode, fragColor, fragRevealage.a, fragColor, fragRevealage); }\n";
    case PGFX_PASS_RIM:
      return
        "void main(){ vec2 uv0 = gl_FragCoord.xy / uSize;\n"
        "  fragRevealage = vec4(0.0); fragColor = vec4(0.0); float weight_accum = 0.0;\n"
        "  for (int i = -32; i <= 32; i++) { if (i < -sampCount || i > sampCount) continue;\n"
        "    float x = float(i) / float(sampCount); float weight = gaussian_weight(x); weight_accum += weight;\n"
        "    vec2 uv = uv0 + blurDir * x + uvOffset; vec3 col = texture2D(revealBuf, uv).rgb;\n"
        "    if (any(notEqual(vec2(0.0), floor(uv)))) col = vec3(0.0);\n"
        "    fragRevealage.rgb += col * weight; }\n"
        "  fragRevealage /= weight_accum;\n"
        "  if (firstPass != 0) { fragColor = texture2D(revealBuf, uv0); vec3 col = texture2D(colorBuf, uv0).rgb;\n"
        "    if (all(lessThan(abs(col - maskColor), vec3(0.05)))) fragColor = vec4(1.0); }\n"
        "  else { float mask = 1.0 - clamp(dot(vec3(0.333334), texture2D(colorBuf, uv0).rgb), 0.0, 1.0);\n"
        "    float rim = clamp(dot(vec3(0.333334), fragRevealage.rgb), 0.0, 1.0);\n"
        "    vec4 color = vec4(rimColor, 1.0); blend_mode_output(blendMode, color, rim * mask, fragColor, fragRevealage); }\n";
    case PGFX_PASS_SHADOW:
      return
        "void main(){ vec2 uv0 = gl_FragCoord.xy / uSize;\n"
        "  fragRevealage = vec4(0.0); fragColor = vec4(0.0); float weight_accum = 0.0;\n"
        "  for (int i = -32; i <= 32; i++) { if (i < -sampCount || i > sampCount) continue;\n"
        "    float x = float(i) / float(sampCount); float weight = gaussian_weight(x); weight_accum += weight;\n"
        "    vec2 uv = uv0.x * uvRotX + uv0.y * uvRotY + uvOffset; uv += blurDir * x;\n"
        "    float wave_time = dot(uv, waveDir.xy); uv += sin(wave_time + wavePhase) * waveOffset;\n"
        "    vec3 col = texture2D(revealBuf, uv).rgb;\n"
        "    if (any(notEqual(vec2(0.0), floor(uv)))) col = vec3(1.0);\n"
        "    fragRevealage.rgb += col * weight; }\n"
        "  fragRevealage /= weight_accum;\n"
        "  if (firstPass != 0) { fragColor = texture2D(revealBuf, uv0); }\n"
        "  else { float shadow_fac = 1.0 - clamp(dot(vec3(0.333334), fragRevealage.rgb), 0.0, 1.0);\n"
        "    vec3 original_revealage = texture2D(colorBuf, uv0).rgb;\n"
        "    shadow_fac *= clamp(dot(vec3(0.333334), original_revealage), 0.0, 1.0); shadow_fac *= shadowColor.a;\n"
        "    fragColor.rgb = mix(vec3(0.0), shadowColor.rgb, shadow_fac); fragColor.a = shadow_fac;\n"
        "    fragRevealage.rgb = original_revealage * (1.0 - shadow_fac); fragRevealage.a = 1.0; }\n";
    default:
      return "void main(){ gl_FragColor = vec4(0.0);\n";
  }
}

const char* fs_tail() {
  return
    "  vec4 o = (uOut == 0) ? fragColor : fragRevealage;\n"
    "  if (uBlend != 0) { vec4 d = (uOut == 0) ? texture2D(dstColor, uv0Final()) : texture2D(dstReveal, uv0Final()); o = doBlend(o, d); }\n"
    "  gl_FragColor = o; }\n";
}

struct Program {
  GLuint id = 0;
  GLint u_size = -1, u_out = -1, u_blend = -1;
};
Program g_prog[8];       // by pass kind
Program g_convert, g_composite;

GLuint compile(GLenum type, const std::string& src) {
  GLuint s = glCreateShader(type);
  const char* c = src.c_str();
  glShaderSource(s, 1, &c, nullptr);
  glCompileShader(s);
  GLint ok = GL_FALSE;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) { glDeleteShader(s); return 0; }
  return s;
}

bool link(Program& p, const std::string& fs) {
  GLuint vs = compile(GL_VERTEX_SHADER, vs_src());
  GLuint f = compile(GL_FRAGMENT_SHADER, fs);
  if (!vs || !f) { if (vs) glDeleteShader(vs); if (f) glDeleteShader(f); return false; }
  p.id = glCreateProgram();
  glAttachShader(p.id, vs); glAttachShader(p.id, f);
  glBindAttribLocation(p.id, 0, "a_position");
  glLinkProgram(p.id);
  glDeleteShader(vs); glDeleteShader(f);
  GLint ok = GL_FALSE;
  glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
  if (!ok) { glDeleteProgram(p.id); p.id = 0; return false; }
  p.u_size = glGetUniformLocation(p.id, "uSize");
  p.u_out = glGetUniformLocation(p.id, "uOut");
  p.u_blend = glGetUniformLocation(p.id, "uBlend");
  return true;
}

Program* program_for(int kind) {
  if (kind < 0 || kind > 7) return nullptr;
  Program& p = g_prog[kind];
  if (!p.id) {
    std::string fs = std::string(fs_header()) + kind_body(kind);
    // the body ends inside main(): close it with the shared tail; uv0 is its first local
    fs += "  vec2 uvF = gl_FragCoord.xy / uSize;\n";
    std::string tail = fs_tail();
    // uv0Final() is replaced by the pixel coordinate computed above
    size_t at;
    while ((at = tail.find("uv0Final()")) != std::string::npos) tail.replace(at, 10, "uvF");
    fs += tail;
    if (!link(p, fs)) return nullptr;
  }
  return &p;
}

bool ensure_convert_composite() {
  if (!g_convert.id) {
    const std::string fs = std::string(fs_header()) +
      "void main(){ vec2 uv = gl_FragCoord.xy / uSize; vec4 f = texture2D(colorBuf, uv);\n"
      "  if (uOut == 0) gl_FragColor = f; else { float r = 1.0 - f.a; gl_FragColor = vec4(r, r, r, 1.0); } }\n";
    if (!link(g_convert, fs)) return false;
  }
  if (!g_composite.id) {
    const std::string fs = std::string(fs_header()) +
      "void main(){ vec2 uv = gl_FragCoord.xy / uSize;\n"
      "  if (uOut == 0) gl_FragColor = vec4(texture2D(revealBuf, uv).rgb, 1.0);\n"
      "  else gl_FragColor = vec4(texture2D(colorBuf, uv).rgb, 0.0); }\n";
    if (!link(g_composite, fs)) return false;
  }
  if (!g_vbo) {
    glGenBuffers(1, &g_vbo);
    const float tri[6] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
  return g_vbo != 0;
}

bool make_plane(Plane& p, int w, int h, bool stencil = false) {
  if (p.tex) glDeleteTextures(1, &p.tex);
  if (p.fbo) glDeleteFramebuffers(1, &p.fbo);
  if (p.rb) glDeleteRenderbuffers(1, &p.rb);
  p.rb = 0;
  glGenTextures(1, &p.tex);
  glBindTexture(GL_TEXTURE_2D, p.tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &p.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, p.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p.tex, 0);
  if (stencil) {
    glGenRenderbuffers(1, &p.rb);
    glBindRenderbuffer(GL_RENDERBUFFER, p.rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, p.rb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      /* No stencil-only attachment on this driver: keep the colour-only plane. */
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
      glDeleteRenderbuffers(1, &p.rb);
      p.rb = 0;
    }
  }
  return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

bool ensure_targets(int w, int h) {
  if (g_layer.fbo && g_w == w && g_h == h) return true;
  bool ok = make_plane(g_layer, w, h, true);
  for (Buffer& b : g_bufs) ok = make_plane(b.color, w, h) && make_plane(b.reveal, w, h) && ok;
  glBindTexture(GL_TEXTURE_2D, 0);
  g_w = w; g_h = h;
  return ok;
}

void bind_tex(int unit, GLuint tex, const Program& p, const char* name) {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(p.id, name), unit);
}

void set2(GLuint prog, const char* n, const float* v) { glUniform2f(glGetUniformLocation(prog, n), v[0], v[1]); }
void set3(GLuint prog, const char* n, const float* v) { glUniform3f(glGetUniformLocation(prog, n), v[0], v[1], v[2]); }
void set4(GLuint prog, const char* n, const float* v) { glUniform4f(glGetUniformLocation(prog, n), v[0], v[1], v[2], v[3]); }
void seti(GLuint prog, const char* n, int v) { glUniform1i(glGetUniformLocation(prog, n), v); }
void setf(GLuint prog, const char* n, float v) { glUniform1f(glGetUniformLocation(prog, n), v); }

void draw_triangle() {
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glDisableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void set_pass_uniforms(const Program& pr, const PGFxPass& p) {
  const GLuint g = pr.id;
  set2(g, "offset", p.offset); seti(g, "sampCount", p.samp_count);
  set3(g, "lowColor", p.low_color); set3(g, "highColor", p.high_color); setf(g, "factor", p.factor); seti(g, "mode", p.mode);
  set2(g, "axisFlip", p.axis_flip); set2(g, "waveDir", p.wave_dir); set2(g, "waveOffset", p.wave_offset);
  setf(g, "wavePhase", p.wave_phase);
  setf(g, "swirlRadius", p.swirl_radius); set2(g, "swirlCenter", p.swirl_center); setf(g, "swirlAngle", p.swirl_angle);
  set2(g, "targetPixelSize", p.target_pixel_size); set2(g, "targetPixelOffset", p.target_pixel_offset);
  set2(g, "accumOffset", p.accum_offset);
  set4(g, "threshold", p.threshold); set4(g, "glowColor", p.glow_color);
  seti(g, "glowUnder", p.glow_under); seti(g, "firstPass", p.first_pass); seti(g, "blendMode", p.blend_mode);
  set2(g, "blurDir", p.blur_dir); set2(g, "uvOffset", p.uv_offset); set2(g, "uvRotX", p.uv_rot_x); set2(g, "uvRotY", p.uv_rot_y);
  set3(g, "maskColor", p.mask_color); set3(g, "rimColor", p.rim_color); set4(g, "shadowColor", p.shadow_color);
}

int count_passes(const PGFxEntry* entries, int count, const PGFxView& view) {
  int total = 0;
  for (int i = 0; i < count; i++) {
    PGFxPass passes[PG_FX_MAX_PASSES];
    total += pg_fx_build_passes(&entries[i], &view, passes);
  }
  return total;
}

}  // namespace

// Number of passes the effects would run (0: nothing to do, draw the layer directly).
extern "C" int project_grease_fx_pass_count(const PGFxEntry* entries, int count, const PGFxView* view) {
  return (entries && view) ? count_passes(entries, count, *view) : 0;
}

// Starts a layer: binds the offscreen layer buffer cleared to transparent black. The caller draws
// the layer into it with premultiplied-alpha blending and then calls project_grease_fx_end_layer.
extern "C" int project_grease_fx_begin_layer(int w, int h) {
  if (w <= 0 || h <= 0) return 0;
  // the framebuffer to composite onto is the one bound now (creating the buffers rebinds FBOs)
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &g_prev_fbo);
  glGetIntegerv(GL_VIEWPORT, g_prev_viewport);
  if (!ensure_convert_composite() || !ensure_targets(w, h)) {
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)g_prev_fbo);
    return 0;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, g_layer.fbo);
  glViewport(0, 0, w, h);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);
  return 1;
}

// Runs the effects on the layer buffer and composites the result onto the framebuffer that was
// bound at begin. `out_color`/`out_reveal` (optional, w*h*4 bytes) receive the final buffers for tests.
extern "C" int project_grease_fx_end_layer(const PGFxEntry* entries, int count, const PGFxView* view,
                                           unsigned char* out_color, unsigned char* out_reveal) {
  const int w = g_w, h = g_h;
  glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER, g_bufs[0].color.fbo);
  // layer (premultiplied) -> C0, R0
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  for (int out = 0; out < 2; out++) {
    glBindFramebuffer(GL_FRAMEBUFFER, out == 0 ? g_bufs[0].color.fbo : g_bufs[0].reveal.fbo);
    glViewport(0, 0, w, h);
    glUseProgram(g_convert.id);
    bind_tex(0, g_layer.tex, g_convert, "colorBuf");
    glUniform2f(g_convert.u_size, float(w), float(h));
    glUniform1i(g_convert.u_out, out);
    glUniform1i(g_convert.u_blend, 0);
    draw_triangle();
  }
  int cur = 0;
  for (int e = 0; e < count; e++) {
    PGFxPass passes[PG_FX_MAX_PASSES];
    const int n = pg_fx_build_passes(&entries[e], view, passes);
    if (n == 0) continue;
    const int start = cur;
    int prev_src = cur;
    for (int k = 0; k < n; k++) {
      const PGFxPass& p = passes[k];
      Program* pr = program_for(p.kind);
      if (!pr) continue;
      const int src = (p.src == PGFX_SRC_PREV_SRC) ? prev_src : cur;
      const int dst = (p.dst == PGFX_DST_START) ? start : (p.dst == PGFX_DST_PREV ? cur : -1);
      int target = 0;
      while (target == src || target == dst) target++;
      glUseProgram(pr->id);
      bind_tex(0, g_bufs[src].color.tex, *pr, "colorBuf");
      bind_tex(1, g_bufs[src].reveal.tex, *pr, "revealBuf");
      bind_tex(2, g_bufs[dst >= 0 ? dst : src].color.tex, *pr, "dstColor");
      bind_tex(3, g_bufs[dst >= 0 ? dst : src].reveal.tex, *pr, "dstReveal");
      glUniform2f(pr->u_size, float(w), float(h));
      glUniform1i(pr->u_blend, dst >= 0 ? p.blend : 0);
      set_pass_uniforms(*pr, p);
      for (int out = 0; out < 2; out++) {
        glBindFramebuffer(GL_FRAMEBUFFER, out == 0 ? g_bufs[target].color.fbo : g_bufs[target].reveal.fbo);
        glViewport(0, 0, w, h);
        glUniform1i(pr->u_out, out);
        draw_triangle();
      }
      prev_src = cur;
      cur = target;
    }
  }
  if (out_color || out_reveal) {
    glFinish();
    if (out_color) {
      glBindFramebuffer(GL_FRAMEBUFFER, g_bufs[cur].color.fbo);
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out_color);
    }
    if (out_reveal) {
      glBindFramebuffer(GL_FRAMEBUFFER, g_bufs[cur].reveal.fbo);
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out_reveal);
    }
  }
  // composite: frame = frame * R, then frame += C (COMPOSITE of gpencil_vfx_frag.glsl)
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)g_prev_fbo);
  glViewport(g_prev_viewport[0], g_prev_viewport[1], g_prev_viewport[2], g_prev_viewport[3]);
  glUseProgram(g_composite.id);
  bind_tex(0, g_bufs[cur].color.tex, g_composite, "colorBuf");
  bind_tex(1, g_bufs[cur].reveal.tex, g_composite, "revealBuf");
  glUniform2f(g_composite.u_size, float(g_prev_viewport[2]), float(g_prev_viewport[3]));
  glEnable(GL_BLEND);
  glUniform1i(g_composite.u_out, 0);
  glBlendFunc(GL_ZERO, GL_SRC_COLOR);
  draw_triangle();
  glUniform1i(g_composite.u_out, 1);
  glBlendFunc(GL_ONE, GL_ONE);
  draw_triangle();
  glDisable(GL_BLEND);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
  return 1;
}

extern "C" void project_grease_fx_reset() {
  for (Program& p : g_prog) { if (p.id) glDeleteProgram(p.id); p = Program(); }
  if (g_convert.id) glDeleteProgram(g_convert.id);
  if (g_composite.id) glDeleteProgram(g_composite.id);
  g_convert = Program(); g_composite = Program();
  if (g_layer.tex) glDeleteTextures(1, &g_layer.tex);
  if (g_layer.fbo) glDeleteFramebuffers(1, &g_layer.fbo);
  if (g_layer.rb) glDeleteRenderbuffers(1, &g_layer.rb);
  g_layer = Plane();
  for (Buffer& b : g_bufs) {
    for (Plane* pl : {&b.color, &b.reveal}) {
      if (pl->tex) glDeleteTextures(1, &pl->tex);
      if (pl->fbo) glDeleteFramebuffers(1, &pl->fbo);
      *pl = Plane();
    }
  }
  if (g_vbo) glDeleteBuffers(1, &g_vbo);
  g_vbo = 0; g_w = g_h = 0;
}
