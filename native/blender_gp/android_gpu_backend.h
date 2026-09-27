#pragma once

#include "gpu_backend.hh"

struct GPUBatch;

namespace blender::gpu {

class AndroidBackend final : public GPUBackend {
 public:
  ~AndroidBackend() override = default;

  void delete_resources() override {}
  void samplers_update() override {}
  void compute_dispatch(int, int, int) override {}
  void compute_dispatch_indirect(StorageBuf *) override {}

  Context *context_alloc(void *, void *) override { return nullptr; }

  Batch *batch_alloc() override;
  DrawList *drawlist_alloc(int) override { return nullptr; }
  Fence *fence_alloc() override { return nullptr; }
  FrameBuffer *framebuffer_alloc(const char *) override { return nullptr; }
  IndexBuf *indexbuf_alloc() override;
  PixelBuffer *pixelbuf_alloc(uint) override { return nullptr; }
  QueryPool *querypool_alloc() override { return nullptr; }
  Shader *shader_alloc(const char *) override { return nullptr; }
  Texture *texture_alloc(const char *) override { return nullptr; }
  UniformBuf *uniformbuf_alloc(int, const char *) override { return nullptr; }
  StorageBuf *storagebuf_alloc(int, GPUUsageType, const char *) override { return nullptr; }
  VertBuf *vertbuf_alloc() override;

  void render_begin() override {}
  void render_end() override {}
  void render_step() override {}
};

AndroidBackend *android_backend_get();

extern "C" int project_grease_android_gpu_configure_batch(GPUBatch *batch);
extern "C" int project_grease_android_gpu_draw_batch(GPUBatch *batch);

}  // namespace blender::gpu
