#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <GLES2/gl2.h>

#include "DNA_gpencil_legacy_types.h"

namespace {

struct Vertex {
  float x;
  float y;
};

GLuint g_program = 0;
GLuint g_vbo = 0;
GLint g_position = -1;
GLint g_color = -1;

const char *vertex_shader_source()
{
  return
      "attribute vec2 a_position;\n"
      "uniform vec4 u_color;\n"
      "varying vec4 v_color;\n"
      "void main() {\n"
      "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
      "  v_color = u_color;\n"
      "}\n";
}

const char *fragment_shader_source()
{
  return
      "precision mediump float;\n"
      "varying vec4 v_color;\n"
      "void main() {\n"
      "  gl_FragColor = v_color;\n"
      "}\n";
}

GLuint compile_shader(GLenum type, const char *source)
{
  const GLuint shader = glCreateShader(type);
  if (!shader) {
    return 0;
  }

  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_FALSE) {
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

bool ensure_program()
{
  if (g_program != 0) {
    return true;
  }

  const GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source());
  const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source());
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return false;
  }

  g_program = glCreateProgram();
  glAttachShader(g_program, vs);
  glAttachShader(g_program, fs);
  glBindAttribLocation(g_program, 0, "a_position");
  glLinkProgram(g_program);

  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint linked = GL_FALSE;
  glGetProgramiv(g_program, GL_LINK_STATUS, &linked);
  if (linked == GL_FALSE) {
    glDeleteProgram(g_program);
    g_program = 0;
    return false;
  }

  g_position = glGetAttribLocation(g_program, "a_position");
  g_color = glGetUniformLocation(g_program, "u_color");

  glGenBuffers(1, &g_vbo);
  return g_position >= 0 && g_color >= 0 && g_vbo != 0;
}

inline Vertex to_ndc(float x, float y, int width, int height)
{
  return {
      2.0f * (x / static_cast<float>(width)) - 1.0f,
      1.0f - 2.0f * (y / static_cast<float>(height)),
  };
}

void append_segment(std::vector<Vertex> &vertices,
                    const bGPDspoint &a,
                    const bGPDspoint &b,
                    float thickness,
                    int width,
                    int height)
{
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float length = std::sqrt(dx * dx + dy * dy);

  if (length < 0.001f) {
    return;
  }

  const float half = std::max(0.5f, thickness * 0.5f);
  const float nx = -dy / length * half;
  const float ny = dx / length * half;

  const Vertex p0 = to_ndc(a.x + nx, a.y + ny, width, height);
  const Vertex p1 = to_ndc(a.x - nx, a.y - ny, width, height);
  const Vertex p2 = to_ndc(b.x + nx, b.y + ny, width, height);
  const Vertex p3 = to_ndc(b.x - nx, b.y - ny, width, height);

  vertices.push_back(p0);
  vertices.push_back(p1);
  vertices.push_back(p2);
  vertices.push_back(p2);
  vertices.push_back(p1);
  vertices.push_back(p3);
}

void append_dot(std::vector<Vertex> &vertices,
                const bGPDspoint &point,
                float thickness,
                int width,
                int height)
{
  const float half = std::max(0.5f, thickness * 0.5f);
  const Vertex p0 = to_ndc(point.x - half, point.y - half, width, height);
  const Vertex p1 = to_ndc(point.x + half, point.y - half, width, height);
  const Vertex p2 = to_ndc(point.x - half, point.y + half, width, height);
  const Vertex p3 = to_ndc(point.x + half, point.y + half, width, height);

  vertices.push_back(p0);
  vertices.push_back(p1);
  vertices.push_back(p2);
  vertices.push_back(p2);
  vertices.push_back(p1);
  vertices.push_back(p3);
}

}  // namespace

extern "C" int project_grease_android_present_gp_frame(const bGPDframe *frame)
{
  if (!frame || !ensure_program()) {
    return 0;
  }

  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_VIEWPORT, viewport);
  const int width = viewport[2];
  const int height = viewport[3];
  if (width <= 0 || height <= 0) {
    return 0;
  }

  std::vector<Vertex> vertices;
  vertices.reserve(1024);

  for (const bGPDstroke *stroke =
           static_cast<const bGPDstroke *>(frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    if (!stroke->points || stroke->totpoints <= 0) {
      continue;
    }

    if (stroke->totpoints == 1) {
      const bGPDspoint &point = stroke->points[0];
      append_dot(vertices, point, static_cast<float>(stroke->thickness) *
                                      std::max(point.pressure, 0.01f),
                 width, height);
      continue;
    }

    for (int i = 0; i + 1 < stroke->totpoints; ++i) {
      const bGPDspoint &a = stroke->points[i];
      const bGPDspoint &b = stroke->points[i + 1];
      const float pressure =
          0.5f * (std::max(a.pressure, 0.01f) + std::max(b.pressure, 0.01f));
      append_segment(vertices, a, b, static_cast<float>(stroke->thickness) * pressure,
                     width, height);
    }

    if (stroke->flag & GP_STROKE_CYCLIC) {
      const bGPDspoint &a = stroke->points[stroke->totpoints - 1];
      const bGPDspoint &b = stroke->points[0];
      const float pressure =
          0.5f * (std::max(a.pressure, 0.01f) + std::max(b.pressure, 0.01f));
      append_segment(vertices, a, b, static_cast<float>(stroke->thickness) * pressure,
                     width, height);
    }
  }

  if (vertices.empty()) {
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return 1;
  }

  glUseProgram(g_program);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(),
               GL_DYNAMIC_DRAW);

  glEnableVertexAttribArray(static_cast<GLuint>(g_position));
  glVertexAttribPointer(
      static_cast<GLuint>(g_position), 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);

  glUniform4f(g_color, 1.0f, 1.0f, 1.0f, 1.0f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glDisable(GL_BLEND);

  glDisableVertexAttribArray(static_cast<GLuint>(g_position));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);

  return glGetError() == GL_NO_ERROR ? 1 : 0;
}
