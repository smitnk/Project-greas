#include "project_grease_gp_backend.h"

#include <cstdio>
#include <string>
#include <vector>

#include "BKE_gpencil_legacy.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"

#include "GPU_context.h"
#include "GPU_init_exit.h"
#include "GHOST_C-api.h"

namespace project_grease::gp {

struct Backend::Impl {
  std::string last_error;
  Main *bmain = nullptr;
  bGPdata *gpd = nullptr;
  bGPDlayer *layer = nullptr;
  bGPDframe *frame = nullptr;
  bGPDstroke *stroke = nullptr;
  StrokeStyle stroke_style{};
  std::vector<StrokePoint> pending_points;
  bool initialized = false;
  bool document_created = false;
  bool layer_created = false;
  bool frame_created = false;
  bool stroke_open = false;
};

Backend::Backend() : impl_(new Impl()) {}
Backend::~Backend() { shutdown(); delete impl_; }

bool Backend::initialize() {
  if (impl_->initialized) return true;
  BKE_idtype_init();
  impl_->bmain = BKE_main_new();
  if (!impl_->bmain) { impl_->last_error = "BKE_main_new() failed"; return false; }
  impl_->initialized = true;
  return true;
}

void Backend::shutdown() {
  if (!impl_) return;
  impl_->stroke = nullptr;
  impl_->frame = nullptr;
  impl_->layer = nullptr;
  impl_->gpd = nullptr;
  impl_->stroke_open = false;
  impl_->frame_created = false;
  impl_->layer_created = false;
  impl_->document_created = false;
  impl_->pending_points.clear();
  if (impl_->bmain) {
    BKE_main_free(impl_->bmain);
    impl_->bmain = nullptr;
  }
  impl_->initialized = false;
}

bool Backend::create_document() {
  if (!impl_->initialized || !impl_->bmain) {
    impl_->last_error = "backend is not initialized"; return false;
  }
  impl_->gpd = BKE_gpencil_data_addnew(impl_->bmain, "Project Grease");
  if (!impl_->gpd) {
    impl_->last_error = "BKE_gpencil_data_addnew() failed"; return false;
  }
  impl_->document_created = true;
  return true;
}

bool Backend::create_layer(const char *name) {
  if (!impl_->document_created || !impl_->gpd) {
    impl_->last_error = "document is not created"; return false;
  }
  const char *layer_name = (name && name[0]) ? name : "GP_Layer";
  impl_->layer = BKE_gpencil_layer_addnew(impl_->gpd, layer_name, true, false);
  if (!impl_->layer) {
    impl_->last_error = "BKE_gpencil_layer_addnew() failed"; return false;
  }
  impl_->layer_created = true;
  return true;
}

bool Backend::create_frame(int frame_number) {
  if (!impl_->layer_created || !impl_->layer) {
    impl_->last_error = "layer is not created"; return false;
  }
  impl_->frame = BKE_gpencil_frame_addnew(impl_->layer, frame_number);
  if (!impl_->frame) {
    impl_->last_error = "BKE_gpencil_frame_addnew() failed"; return false;
  }
  impl_->frame_created = true;
  return true;
}

bool Backend::begin_stroke(const StrokeStyle &style) {
  if (!impl_->frame_created || !impl_->frame) {
    impl_->last_error = "frame is not created"; return false;
  }
  if (impl_->stroke_open) {
    impl_->last_error = "a stroke is already open"; return false;
  }
  impl_->stroke_style = style;
  impl_->pending_points.clear();
  impl_->stroke = nullptr;
  impl_->stroke_open = true;
  return true;
}

bool Backend::add_point(const StrokePoint &point) {
  if (!impl_->stroke_open) {
    impl_->last_error = "stroke is not open"; return false;
  }
  impl_->pending_points.push_back(point);
  return true;
}

bool Backend::end_stroke() {
  if (!impl_->stroke_open) {
    impl_->last_error = "stroke is not open";
    return false;
  }
  if (impl_->pending_points.empty()) {
    impl_->last_error = "stroke has no points";
    impl_->stroke_open = false;
    return false;
  }

  const int point_count = static_cast<int>(impl_->pending_points.size());
  const int material_index = impl_->stroke_style.material_index;
  const short thickness = static_cast<short>(
      impl_->stroke_style.thickness < 1.0f ? 1.0f : impl_->stroke_style.thickness);

  impl_->stroke = BKE_gpencil_stroke_add(
      impl_->frame, material_index, point_count, thickness, false);
  if (!impl_->stroke) {
    impl_->last_error = "BKE_gpencil_stroke_add() failed";
    impl_->stroke_open = false;
    return false;
  }

  for (int i = 0; i < point_count; ++i) {
    const StrokePoint &src = impl_->pending_points[i];
    bGPDspoint &dst = impl_->stroke->points[i];
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.pressure = src.pressure;
    dst.strength = src.strength;
    dst.time = src.time;
  }

  impl_->stroke_open = false;
  impl_->pending_points.clear();
  impl_->gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  BKE_gpencil_tag(impl_->gpd);
  return true;
}

bool Backend::render() {
  if (!impl_->frame_created || !impl_->gpd || !impl_->stroke) {
    impl_->last_error = "no native GP stroke is ready to render";
    return false;
  }

  std::fprintf(stderr, "[PG42] before GHOST_CreateSystemBackground\n");
  std::fflush(stderr);
  GHOST_SystemHandle ghost_system = GHOST_CreateSystemBackground();
  std::fprintf(stderr, "[PG42] after GHOST_CreateSystemBackground: %p\n", ghost_system);
  std::fflush(stderr);
  if (!ghost_system) {
    impl_->last_error = "GHOST_CreateSystemBackground() failed";
    return false;
  }

  // Blender v3.6 GPU tests select the backend before creating the
  // GHOST context. v3.6.23 does not provide GPU_backend_ghost_system_set().
  std::fprintf(stderr, "[PG42] before GPU_backend_type_selection_set\n");
  std::fflush(stderr);
  GPU_backend_type_selection_set(GPU_BACKEND_OPENGL);
  std::fprintf(stderr, "[PG42] after GPU_backend_type_selection_set\n");
  std::fflush(stderr);

  GHOST_GLSettings gl_settings = {};
  gl_settings.context_type = GHOST_kDrawingContextTypeOpenGL;

  std::fprintf(stderr, "[PG42] before GHOST_CreateOpenGLContext\n");
  std::fflush(stderr);
  GHOST_ContextHandle ghost_context =
      GHOST_CreateOpenGLContext(ghost_system, gl_settings);
  std::fprintf(stderr, "[PG42] after GHOST_CreateOpenGLContext: %p\n", ghost_context);
  std::fflush(stderr);
  if (!ghost_context) {
    GHOST_DisposeSystem(ghost_system);
    impl_->last_error = "GHOST_CreateOpenGLContext() failed";
    return false;
  }

  std::fprintf(stderr, "[PG42] before GHOST_ActivateOpenGLContext\n");
  std::fflush(stderr);
  if (GHOST_ActivateOpenGLContext(ghost_context) != GHOST_kSuccess) {
    GHOST_DisposeOpenGLContext(ghost_system, ghost_context);
    GHOST_DisposeSystem(ghost_system);
    impl_->last_error = "GHOST_ActivateOpenGLContext() failed";
    return false;
  }
  std::fprintf(stderr, "[PG42] after GHOST_ActivateOpenGLContext\n");
  std::fflush(stderr);

  std::fprintf(stderr, "[PG42] before GPU_context_create\n");
  std::fflush(stderr);
  GPUContext *gpu_context = GPU_context_create(nullptr, ghost_context);
  std::fprintf(stderr, "[PG42] after GPU_context_create: %p\n", gpu_context);
  std::fflush(stderr);
  if (!gpu_context) {
    GHOST_ReleaseOpenGLContext(ghost_context);
    GHOST_DisposeOpenGLContext(ghost_system, ghost_context);
    GHOST_DisposeSystem(ghost_system);
    impl_->last_error = "GPU_context_create() failed";
    return false;
  }

  std::fprintf(stderr, "[PG42] before GPU_init\n");
  std::fflush(stderr);
  GPU_init();
  std::fprintf(stderr, "[PG42] after GPU_init\n");
  std::fflush(stderr);

  std::fprintf(stderr, "[PG42] before GPU_context_begin_frame\n");
  std::fflush(stderr);
  GPU_context_begin_frame(gpu_context);
  std::fprintf(stderr, "[PG42] after GPU_context_begin_frame\n");
  std::fflush(stderr);

  std::fprintf(stderr, "[PG42] before BKE_object_add_only_object\n");
  std::fflush(stderr);
  Object *ob = BKE_object_add_only_object(
      impl_->bmain, OB_GPENCIL_LEGACY, "Project Grease Render");
  std::fprintf(stderr, "[PG42] after BKE_object_add_only_object: %p\n", ob);
  std::fflush(stderr);
  if (!ob) {
    GPU_context_end_frame(gpu_context);
    GPU_exit();
    GPU_context_discard(gpu_context);
    GHOST_ReleaseOpenGLContext(ghost_context);
    GHOST_DisposeOpenGLContext(ghost_system, ghost_context);
    GHOST_DisposeSystem(ghost_system);
    impl_->last_error = "BKE_object_add_only_object() failed";
    return false;
  }

  std::fprintf(stderr, "[PG42] before assigning ob->data\n");
  std::fflush(stderr);
  ob->data = impl_->gpd;
  std::fprintf(stderr, "[PG42] after assigning ob->data: %p\n", ob->data);
  std::fflush(stderr);

  std::fprintf(stderr, "[PG42] before DRW_cache_gpencil_get\n");
  std::fflush(stderr);
  GPUBatch *batch = DRW_cache_gpencil_get(ob, 1);
  std::fprintf(stderr, "[PG42] after DRW_cache_gpencil_get: %p\n", batch);
  std::fflush(stderr);

  const bool cache_ready = batch != nullptr;
  impl_->last_error = cache_ready
      ? "real Blender GP draw-cache GPU batch built from native bGPDstroke"
      : "DRW_cache_gpencil_get() returned null";

  DRW_gpencil_batch_cache_free(impl_->gpd);
  ob->data = nullptr;
  BKE_id_free(impl_->bmain, &ob->id);

  std::fprintf(stderr, "[PG42] before teardown\n");
  std::fflush(stderr);
  GPU_context_end_frame(gpu_context);
  GPU_exit();
  GPU_context_discard(gpu_context);
  GHOST_ReleaseOpenGLContext(ghost_context);
  GHOST_DisposeOpenGLContext(ghost_system, ghost_context);
  GHOST_DisposeSystem(ghost_system);

  return cache_ready;
}

const char *Backend::last_error() const { return impl_->last_error.c_str(); }

}  // namespace project_grease::gp
