#include "android_gpu_backend.h"

#include <cstdint>
#include <cstring>

#include <GLES3/gl3.h>

#include "MEM_guardedalloc.h"
#include "BLI_assert.h"

#include "GPU_batch.h"
#include "GPU_index_buffer.h"
#include "GPU_vertex_buffer.h"

#include "gpu_batch_private.hh"
#include "gpu_index_buffer_private.hh"
#include "gpu_vertex_buffer_private.hh"

namespace blender::gpu {

class AndroidVertBuf final : public VertBuf {
 private:
  GLuint vbo_id_ = 0;

 public:
  ~AndroidVertBuf() override
  {
    clear();
  }

  void bind()
  {
    if (vbo_id_ == 0) {
      glGenBuffers(1, &vbo_id_);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo_id_);

    if (flag & GPU_VERTBUF_DATA_DIRTY) {
      const GLsizeiptr size = static_cast<GLsizeiptr>(size_used_get());
      glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_STATIC_DRAW);
      if (usage_ != GPU_USAGE_DEVICE_ONLY && data != nullptr && size > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
      }
      flag &= ~GPU_VERTBUF_DATA_DIRTY;
      flag |= GPU_VERTBUF_DATA_UPLOADED;
    }
  }

  GLuint gl_handle() const { return vbo_id_; }

 protected:
  void acquire_data() override
  {
    data = static_cast<uchar *>(MEM_mallocN(size_alloc_get(), "AndroidVertBuf data"));
  }

  void resize_data() override
  {
    data = static_cast<uchar *>(MEM_reallocN(data, size_alloc_get()));
  }

  void release_data() override
  {
    if (vbo_id_ != 0) {
      glDeleteBuffers(1, &vbo_id_);
      vbo_id_ = 0;
    }
    MEM_SAFE_FREE(data);
  }

  void upload_data() override
  {
    bind();
  }

  void duplicate_data(VertBuf *dst_) override
  {
    AndroidVertBuf *dst = static_cast<AndroidVertBuf *>(dst_);
    if (data != nullptr) {
      dst->data = static_cast<uchar *>(MEM_mallocN(size_alloc_get(), "AndroidVertBuf duplicate"));
      std::memcpy(dst->data, data, size_alloc_get());
    }
  }

  void bind_as_ssbo(uint) override {}
  void bind_as_texture(uint) override {}

  void wrap_handle(uint64_t handle) override
  {
    BLI_assert(vbo_id_ == 0);
    vbo_id_ = static_cast<GLuint>(handle);
    flag &= ~GPU_VERTBUF_DATA_DIRTY;
    flag |= GPU_VERTBUF_DATA_UPLOADED;
  }

 public:
  void update_sub(uint start, uint len, const void *src) override
  {
    bind();
    glBufferSubData(GL_ARRAY_BUFFER,
                    static_cast<GLintptr>(start),
                    static_cast<GLsizeiptr>(len),
                    src);
  }

  void read(void *dst) const override
  {
    if (dst != nullptr && data != nullptr) {
      std::memcpy(dst, data, size_used_get());
    }
  }
};

class AndroidIndexBuf final : public IndexBuf {
 private:
  GLuint ibo_id_ = 0;

 public:
  ~AndroidIndexBuf() override
  {
    if (ibo_id_ != 0) {
      glDeleteBuffers(1, &ibo_id_);
      ibo_id_ = 0;
    }
  }

  void bind()
  {
    if (is_subrange_) {
      static_cast<AndroidIndexBuf *>(src_)->bind();
      return;
    }

    const bool allocate = ibo_id_ == 0;
    if (allocate) {
      glGenBuffers(1, &ibo_id_);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id_);

    if (data_ != nullptr || allocate) {
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(size_get()),
                   data_,
                   GL_STATIC_DRAW);
      MEM_SAFE_FREE(data_);
    }
  }

  GLuint gl_handle() const { return ibo_id_; }

  GLenum gl_index_type() const
  {
    return index_type_ == GPU_INDEX_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  }

 protected:
  void upload_data() override { bind(); }
  void bind_as_ssbo(uint) override {}
  void read(uint32_t *) const override {}

  void update_sub(uint start, uint len, const void *src) override
  {
    bind();
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                    static_cast<GLintptr>(start),
                    static_cast<GLsizeiptr>(len),
                    src);
  }

  void strip_restart_indices() override {}
};

class AndroidBatch final : public Batch {
 private:
  GLuint vao_id_ = 0;

 public:
  ~AndroidBatch() override
  {
    if (vao_id_ != 0) {
      glDeleteVertexArrays(1, &vao_id_);
      vao_id_ = 0;
    }
  }

  void configure_vao()
  {
    BLI_assert(verts[0] != nullptr);
    BLI_assert(elem != nullptr);

    auto *vbo = static_cast<AndroidVertBuf *>(verts[0]);
    auto *ibo = static_cast<AndroidIndexBuf *>(elem);

    vbo->bind();
    ibo->bind();

    if (vao_id_ == 0) {
      glGenVertexArrays(1, &vao_id_);
    }
    glBindVertexArray(vao_id_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo->gl_handle());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo->gl_handle());

    const GPUVertFormat &format = vbo->format;
    for (uint attr_index = 0; attr_index < format.attr_len; ++attr_index) {
      const GPUVertAttr &attr = format.attrs[attr_index];
      const GLuint location = attr_index;
      const GLsizei stride = static_cast<GLsizei>(format.stride);
      const void *offset = reinterpret_cast<const void *>(
          static_cast<uintptr_t>(attr.offset));

      glEnableVertexAttribArray(location);

      if (attr.fetch_mode == GPU_FETCH_INT) {
        glVertexAttribIPointer(location,
                               static_cast<GLint>(attr.comp_len),
                               GL_INT,
                               stride,
                               offset);
      }
      else {
        GLenum type = GL_FLOAT;
        switch (static_cast<GPUVertCompType>(attr.comp_type)) {
          case GPU_COMP_F32: type = GL_FLOAT; break;
          case GPU_COMP_I32: type = GL_INT; break;
          case GPU_COMP_U32: type = GL_UNSIGNED_INT; break;
          default: BLI_assert(false); break;
        }
        glVertexAttribPointer(location,
                              static_cast<GLint>(attr.comp_len),
                              type,
                              GL_FALSE,
                              stride,
                              offset);
      }
    }
  }

  void draw(int v_first, int v_count, int i_first, int i_count) override
  {
    configure_vao();
    auto *ibo = static_cast<AndroidIndexBuf *>(elem);
    glBindVertexArray(vao_id_);

    const GLenum primitive = prim_type == GPU_PRIM_TRIS ? GL_TRIANGLES : GL_POINTS;
    const void *index_offset = ibo->offset_ptr(static_cast<uint>(v_first));

    glDrawElements(primitive,
                   static_cast<GLsizei>(v_count),
                   ibo->gl_index_type(),
                   index_offset);

    (void)i_first;
    (void)i_count;
  }

  void draw_indirect(GPUStorageBuf *, intptr_t) override {}
  void multi_draw_indirect(GPUStorageBuf *, int, intptr_t, intptr_t) override {}

  GLuint vao_handle() const { return vao_id_; }
};

Batch *AndroidBackend::batch_alloc()
{
  return new AndroidBatch();
}

IndexBuf *AndroidBackend::indexbuf_alloc()
{
  return new AndroidIndexBuf();
}

VertBuf *AndroidBackend::vertbuf_alloc()
{
  return new AndroidVertBuf();
}

AndroidBackend *android_backend_get()
{
  static AndroidBackend backend;
  return &backend;
}

GPUBackend *GPUBackend::get()
{
  return android_backend_get();
}

extern "C" int project_grease_android_gpu_configure_batch(GPUBatch *batch)
{
  if (batch == nullptr) {
    return 0;
  }
  auto *android_batch = static_cast<AndroidBatch *>(static_cast<Batch *>(batch));
  android_batch->configure_vao();
  return glGetError() == GL_NO_ERROR && android_batch->vao_handle() != 0 ? 1 : 0;
}

extern "C" int project_grease_android_gpu_draw_batch(GPUBatch *batch)
{
  if (batch == nullptr) {
    return 0;
  }
  auto *android_batch = static_cast<AndroidBatch *>(static_cast<Batch *>(batch));
  const int count = batch->elem ? static_cast<int>(android_batch->elem_()->index_len_get()) : 0;
  if (count <= 0) {
    return 0;
  }
  android_batch->draw(0, count, 0, 1);
  return glGetError() == GL_NO_ERROR ? 1 : 0;
}

}  // namespace blender::gpu
