#include <GLES3/gl3.h>

#include "GPU_batch.h"
#include "GPU_index_buffer.h"
#include "GPU_vertex_buffer.h"
#include "GPU_vertex_format.h"

extern "C" int project_grease_android_gpu_configure_batch(GPUBatch *batch);

extern "C" int project_grease_android_gpu_buffer_backend_probe()
{
  if (glGetString(GL_VERSION) == nullptr) {
    return 0;
  }

  GPUVertFormat format = {};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  GPU_vertformat_attr_add(&format, "ma", GPU_COMP_I32, 4, GPU_FETCH_INT);
  GPU_vertformat_attr_add(&format, "uv", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format_ex(
      &format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(vbo, 3);

  struct ProbeVertex {
    float pos[4];
    int ma[4];
    float uv[4];
  } vertices[3] = {
      {{-0.5f, -0.5f, 0.0f, 1.0f}, {0, 0, 0, 0}, {0, 0, 0, 1}},
      {{ 0.5f, -0.5f, 0.0f, 1.0f}, {0, 0, 1, 0}, {1, 0, 0, 1}},
      {{ 0.0f,  0.5f, 0.0f, 1.0f}, {0, 0, 2, 0}, {0.5f, 1, 0, 1}},
  };

  GPU_vertbuf_vert_set(vbo, 0, &vertices[0]);
  GPU_vertbuf_vert_set(vbo, 1, &vertices[1]);
  GPU_vertbuf_vert_set(vbo, 2, &vertices[2]);

  GPUIndexBufBuilder ibo_builder;
  GPU_indexbuf_init(&ibo_builder, GPU_PRIM_TRIS, 1, 3);
  GPU_indexbuf_add_tri_verts(&ibo_builder, 0, 1, 2);
  GPUIndexBuf *ibo = GPU_indexbuf_build(&ibo_builder);

  GPUBatch *batch = GPU_batch_create(GPU_PRIM_TRIS, vbo, ibo);

  GPU_vertbuf_use(vbo);
  GPU_indexbuf_use(ibo);

  if (glGetError() != GL_NO_ERROR) {
    GPU_batch_discard(batch);
    return 0;
  }

  const int vao_ok = project_grease_android_gpu_configure_batch(batch);
  GPU_batch_discard(batch);
  GPU_indexbuf_discard(ibo);
  GPU_vertbuf_discard(vbo);
  return vao_ok;
}
