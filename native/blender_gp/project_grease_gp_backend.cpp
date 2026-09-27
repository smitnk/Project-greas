#include "project_grease_gp_backend.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#ifndef __ANDROID__
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"
#endif

#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"

#ifndef __ANDROID__
#include "GPU_context.h"
#include "GPU_init_exit.h"
#include "GHOST_C-api.h"
#endif

namespace project_grease::gp {

static void project_grease_gp_tag(bGPdata *gpd)
{
#ifdef __ANDROID__
  if (gpd) {
    gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  }
#else
  BKE_gpencil_tag(gpd);
#endif
}

struct Backend::Impl {
  std::string last_error;
#ifndef __ANDROID__
  Main *bmain = nullptr;
#endif
  bGPdata *gpd = nullptr;
  bGPDlayer *layer = nullptr;
  bGPDframe *frame = nullptr;
  bGPDstroke *stroke = nullptr;
  StrokeStyle stroke_style{};
  std::vector<StrokePoint> pending_points;

#ifndef __ANDROID__
  // Desktop proof path owns its temporary GHOST/GPU session.
  GHOST_SystemHandle ghost_system = nullptr;
  GHOST_ContextHandle ghost_context = nullptr;
  GPUContext *gpu_context = nullptr;
  bool gpu_frame_active = false;
#endif
  // Android uses the caller-owned EGL/GLES context directly. No Blender
  // GHOST or full GPU context is created for the legacy GP cache path.
  bool gpu_initialized = false;
  bool gpu_external_context = false;

  bool initialized = false;
  bool document_created = false;
  bool layer_created = false;
  bool frame_created = false;
  bool stroke_open = false;
};

Backend::Backend() : impl_(new Impl()) {}

Backend::~Backend()
{
  shutdown();
  delete impl_;
}

bool Backend::initialize()
{
  if (impl_->initialized) {
    return true;
  }

  // The full Blender draw module normally installs these callbacks during
  // startup. Project Grease embeds only the legacy GP closure, so install
  // the two callbacks directly and keep Android independent of Blender Main/ID
  // ownership.
  BKE_gpencil_batch_cache_dirty_tag_cb = DRW_gpencil_batch_cache_dirty_tag;
  BKE_gpencil_batch_cache_free_cb = DRW_gpencil_batch_cache_free;

  if (!BKE_gpencil_batch_cache_dirty_tag_cb ||
      !BKE_gpencil_batch_cache_free_cb) {
    impl_->last_error = "legacy GP draw callbacks were not installed";
    return false;
  }

#ifndef __ANDROID__
  BKE_idtype_init();
  impl_->bmain = BKE_main_new();
  if (!impl_->bmain) {
    impl_->last_error = "BKE_main_new() failed";
    return false;
  }
#endif

  impl_->initialized = true;
  return true;
}

void Backend::shutdown()
{
  if (!impl_) {
    return;
  }

  // Android does not create a Blender Main/ID database. Free the minimal
  // legacy GP containers directly after releasing their GPU cache.
#ifdef __ANDROID__
  if (impl_->gpd) {
    if (impl_->gpu_initialized) {
      DRW_gpencil_batch_cache_free(impl_->gpd);
    }
    for (bGPDlayer *layer = static_cast<bGPDlayer *>(impl_->gpd->layers.first);
         layer != nullptr;) {
      bGPDlayer *next_layer = layer->next;
      for (bGPDframe *frame = static_cast<bGPDframe *>(layer->frames.first);
           frame != nullptr;) {
        bGPDframe *next_frame = frame->next;
        for (bGPDstroke *stroke = static_cast<bGPDstroke *>(frame->strokes.first);
             stroke != nullptr;) {
          bGPDstroke *next_stroke = stroke->next;
          MEM_SAFE_FREE(stroke->points);
          MEM_SAFE_FREE(stroke->triangles);
          MEM_SAFE_FREE(stroke->dvert);
          MEM_SAFE_FREE(stroke->editcurve);
          MEM_freeN(stroke);
          stroke = next_stroke;
        }
        MEM_freeN(frame);
        frame = next_frame;
      }
      MEM_freeN(layer);
      layer = next_layer;
    }
    MEM_freeN(impl_->gpd);
    impl_->gpd = nullptr;
  }
#else
  // Legacy GP ID destruction can release GPU batches, so free Main while the
  // desktop proof context is still available.
  if (impl_->bmain) {
    BKE_main_free(impl_->bmain);
    impl_->bmain = nullptr;
  }
#endif

  impl_->stroke = nullptr;
  impl_->frame = nullptr;
  impl_->layer = nullptr;
  impl_->gpd = nullptr;
  impl_->stroke_open = false;
  impl_->frame_created = false;
  impl_->layer_created = false;
  impl_->document_created = false;
  impl_->pending_points.clear();

#ifndef __ANDROID__
  if (impl_->gpu_initialized) {
    if (impl_->gpu_frame_active) {
      GPU_context_end_frame(impl_->gpu_context);
      impl_->gpu_frame_active = false;
    }

    GPU_exit();
    GPU_context_discard(impl_->gpu_context);
    impl_->gpu_context = nullptr;

    if (!impl_->gpu_external_context) {
      GHOST_ReleaseOpenGLContext(impl_->ghost_context);
      GHOST_DisposeOpenGLContext(impl_->ghost_system, impl_->ghost_context);
      GHOST_DisposeSystem(impl_->ghost_system);
    }

    impl_->ghost_context = nullptr;
    impl_->ghost_system = nullptr;
    impl_->gpu_external_context = false;
    impl_->gpu_initialized = false;
  }
#else
  impl_->gpu_initialized = false;
  impl_->gpu_external_context = false;
#endif

  impl_->initialized = false;
}

bool Backend::create_document() {
  if (!impl_->initialized) {
    impl_->last_error = "backend is not initialized";
    return false;
  }
#ifdef __ANDROID__
  // Android deliberately avoids BKE_lib_id/BKE_main. This is still the real
  // Blender legacy bGPdata layout; only the ownership wrapper is minimal.
  impl_->gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "Project Grease Android GP"));
  if (!impl_->gpd) {
    impl_->last_error = "Android bGPdata allocation failed";
    return false;
  }
  impl_->gpd->flag = GP_DATA_DISPINFO | GP_DATA_EXPAND | GP_DATA_VIEWALIGN |
                     GP_DATA_SHOW_ONIONSKINS | GP_DATA_CURVE_ADAPTIVE_RESOLUTION;
  impl_->gpd->line_color[0] = 0.6f;
  impl_->gpd->line_color[1] = 0.6f;
  impl_->gpd->line_color[2] = 0.6f;
  impl_->gpd->line_color[3] = 0.5f;
  impl_->gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
  impl_->gpd->onion_keytype = -1;
  impl_->gpd->onion_flag = GP_ONION_GHOST_PREVCOL | GP_ONION_GHOST_NEXTCOL |
                           GP_ONION_FADE;
  impl_->gpd->onion_mode = GP_ONION_MODE_RELATIVE;
  impl_->gpd->onion_factor = 0.5f;
  impl_->gpd->gstep = 1;
  impl_->gpd->gstep_next = 1;
#else
  if (!impl_->bmain) {
    impl_->last_error = "backend is not initialized";
    return false;
  }
  impl_->gpd = BKE_gpencil_data_addnew(impl_->bmain, "Project Grease");
#endif
  if (!impl_->gpd) {
    impl_->last_error = "BKE_gpencil_data_addnew() failed";
    return false;
  }
  impl_->document_created = true;
  return true;
}

bool Backend::create_layer(const char *name) {
  if (!impl_->document_created || !impl_->gpd) {
    impl_->last_error = "document is not created"; return false;
  }
  const char *layer_name = (name && name[0]) ? name : "GP_Layer";
#ifdef __ANDROID__
  impl_->layer = static_cast<bGPDlayer *>(MEM_callocN(sizeof(bGPDlayer), "Project Grease Android GP layer"));
  if (impl_->layer) {
    std::strncpy(impl_->layer->info, layer_name, sizeof(impl_->layer->info) - 1);
    impl_->layer->info[sizeof(impl_->layer->info) - 1] = '\0';
    impl_->layer->opacity = 1.0f;
    impl_->layer->vertex_paint_opacity = 1.0f;
    impl_->layer->onion_flag |= GP_LAYER_ONIONSKIN;
    impl_->layer->scale[0] = 1.0f;
    impl_->layer->scale[1] = 1.0f;
    impl_->layer->scale[2] = 1.0f;
    BLI_addtail(&impl_->gpd->layers, impl_->layer);
    impl_->gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  }
#else
  impl_->layer = BKE_gpencil_layer_addnew(impl_->gpd, layer_name, true, false);
#endif
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
#ifdef __ANDROID__
  impl_->frame = static_cast<bGPDframe *>(MEM_callocN(sizeof(bGPDframe), "Project Grease Android GP frame"));
  if (impl_->frame) {
    impl_->frame->framenum = frame_number;
    BLI_addtail(&impl_->layer->frames, impl_->frame);
    impl_->layer->actframe = impl_->frame;
  }
#else
  impl_->frame = BKE_gpencil_frame_addnew(impl_->layer, frame_number);
#endif
  if (!impl_->frame) {
    impl_->last_error = "BKE_gpencil_frame_addnew() failed"; return false;
  }
  impl_->frame_created = true;
  return true;
}

bool Backend::select_layer(int index) {
  if (!impl_->document_created || !impl_->gpd || index < 0) {
    impl_->last_error = "invalid layer selection";
    return false;
  }

  int current = 0;
  for (bGPDlayer *layer = static_cast<bGPDlayer *>(impl_->gpd->layers.first);
       layer != nullptr;
       layer = layer->next, ++current) {
    if (current == index) {
      impl_->layer = layer;
      impl_->frame = nullptr;
      impl_->stroke = nullptr;
      impl_->layer_created = true;
      impl_->frame_created = false;
      impl_->last_error.clear();
      return true;
    }
  }

  impl_->last_error = "layer index out of range";
  return false;
}

int Backend::layer_count() const {
  if (!impl_->gpd) {
    return 0;
  }

  int count = 0;
  for (bGPDlayer *layer = static_cast<bGPDlayer *>(impl_->gpd->layers.first);
       layer != nullptr;
       layer = layer->next) {
    ++count;
  }
  return count;
}

bool Backend::select_frame(int frame_number) {
  if (!impl_->layer) {
    impl_->last_error = "layer is not selected";
    return false;
  }

  for (bGPDframe *frame =
           static_cast<bGPDframe *>(impl_->layer->frames.first);
       frame != nullptr;
       frame = frame->next) {
    if (frame->framenum == frame_number) {
      impl_->frame = frame;
      impl_->stroke = nullptr;
      impl_->frame_created = true;
      impl_->last_error.clear();
      return true;
    }
  }

  impl_->last_error = "frame number not found on selected layer";
  return false;
}

int Backend::frame_count() const {
  if (!impl_->layer) {
    return 0;
  }

  int count = 0;
  for (bGPDframe *frame =
           static_cast<bGPDframe *>(impl_->layer->frames.first);
       frame != nullptr;
       frame = frame->next) {
    ++count;
  }
  return count;
}

int Backend::stroke_count() const {
  if (!impl_->frame) {
    return 0;
  }

  int count = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    ++count;
  }
  return count;
}

int Backend::point_count() const {
  if (!impl_->frame) {
    return 0;
  }

  int count = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    count += stroke->totpoints;
  }
  return count;
}

bool Backend::select_stroke(int index) {
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid stroke selection";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current == index) {
      impl_->stroke = stroke;
      impl_->last_error.clear();
      return true;
    }
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::get_point(int stroke_index, int point_index, StrokePoint *out) const {
  if (!out || !impl_->frame || stroke_index < 0 || point_index < 0) {
    return false;
  }

  std::fprintf(stderr, "[GET] frame=%p first=%p selected=%p index=%d point=%d\\n",
               static_cast<void *>(impl_->frame),
               impl_->frame ? impl_->frame->strokes.first : nullptr,
               static_cast<void *>(impl_->stroke), stroke_index, point_index);
  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    std::fprintf(stderr, "[GET] stroke=%p current=%d\\n", static_cast<void *>(stroke), current);
    if (current != stroke_index) {
      continue;
    }
    std::fprintf(stderr, "[GET] totpoints=%d points=%p\\n", stroke->totpoints, static_cast<void *>(stroke->points));
    if (point_index >= stroke->totpoints || !stroke->points) {
      return false;
    }

    std::fprintf(stderr, "[GET] reading point\\n");
    const bGPDspoint &src = stroke->points[point_index];
    std::fprintf(stderr, "[GET] point read ok\\n");
    out->x = src.x;
    out->y = src.y;
    out->z = src.z;
    out->pressure = src.pressure;
    out->strength = src.strength;
    out->time = src.time;
    return true;
  }
  return false;
}

bool Backend::set_point(int stroke_index,
                        int point_index,
                        const StrokePoint &point) {
  if (!impl_->frame || stroke_index < 0 || point_index < 0) {
    impl_->last_error = "invalid stroke point";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != stroke_index) {
      continue;
    }
    if (point_index >= stroke->totpoints || !stroke->points) {
      impl_->last_error = "point index out of range";
      return false;
    }

    bGPDspoint &dst = stroke->points[point_index];
    dst.x = point.x;
    dst.y = point.y;
    dst.z = point.z;
    dst.pressure = point.pressure;
    dst.strength = point.strength;
    dst.time = point.time;
    std::fprintf(stderr, "[SET] point fields written\\n");

    std::fprintf(stderr, "[SET] before batch cache dirty\\n");
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    std::fprintf(stderr, "[SET] after batch cache dirty\\n");
    project_grease_gp_tag(impl_->gpd);
    std::fprintf(stderr, "[SET] after gp tag\\n");
    impl_->stroke = stroke;
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::delete_stroke(int index) {
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid stroke deletion";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    BLI_remlink(&impl_->frame->strokes, stroke);
    BKE_gpencil_free_stroke(stroke);
    impl_->stroke = nullptr;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::duplicate_stroke(int index) {
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid stroke duplication";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    bGPDstroke *duplicate = BKE_gpencil_stroke_duplicate(stroke, true, true);
    if (!duplicate) {
      impl_->last_error = "BKE_gpencil_stroke_duplicate() failed";
      return false;
    }

    BLI_addtail(&impl_->frame->strokes, duplicate);
    impl_->stroke = duplicate;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::translate_stroke(int index, float dx, float dy, float dz) {
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid stroke translation";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    if (stroke->totpoints <= 0 || !stroke->points) {
      impl_->last_error = "stroke has no points";
      return false;
    }

    for (int point_index = 0; point_index < stroke->totpoints; ++point_index) {
      bGPDspoint &point = stroke->points[point_index];
      point.x += dx;
      point.y += dy;
      point.z += dz;
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::flip_stroke(int index)
{
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid stroke flip";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    if (stroke->totpoints <= 0 || !stroke->points) {
      impl_->last_error = "stroke has no points";
      return false;
    }

    BKE_gpencil_stroke_flip(stroke);
    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::subdivide_stroke(int index, int level)
{
  if (!impl_->frame || index < 0 || level <= 0) {
    impl_->last_error = "invalid stroke subdivision";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    if (stroke->totpoints < 2 || !stroke->points) {
      impl_->last_error = "stroke needs at least two points";
      return false;
    }

    BKE_gpencil_stroke_subdivide(impl_->gpd, stroke, level, 0);
    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::close_stroke(int index)
{
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid stroke close";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    if (stroke->totpoints < 3 || !stroke->points) {
      impl_->last_error = "stroke needs at least three points";
      return false;
    }

    const int points_before_close = stroke->totpoints;
    const unsigned int flags_before_close = static_cast<unsigned int>(stroke->flag);
    const void *close_symbol = reinterpret_cast<const void *>(
        reinterpret_cast<uintptr_t>(&BKE_gpencil_stroke_close));
    std::fprintf(stderr,
                 "[CLOSE] before call stroke=%p points=%d flags=0x%x close_symbol=%p\\n",
                 static_cast<void *>(stroke),
                 points_before_close,
                 flags_before_close,
                 close_symbol);

    const bool close_result = BKE_gpencil_stroke_close(stroke);

    std::fprintf(stderr,
                 "[CLOSE] after call result=%d stroke=%p points=%d flags=0x%x cyclic=%d\\n",
                 close_result ? 1 : 0,
                 static_cast<void *>(stroke),
                 stroke->totpoints,
                 static_cast<unsigned int>(stroke->flag),
                 (stroke->flag & GP_STROKE_CYCLIC) != 0);

    if (!close_result) {
      impl_->last_error = "BKE_gpencil_stroke_close() failed";
      return false;
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::trim_stroke_points(int index,
                                 int index_from,
                                 int index_to,
                                 bool keep_single_point) {
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid stroke point trim";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    if (index_from < 0 || index_to < index_from ||
        index_to >= stroke->totpoints) {
      impl_->last_error = "invalid stroke point trim range";
      return false;
    }

    if (!BKE_gpencil_stroke_trim_points(
            stroke, index_from, index_to, keep_single_point)) {
      impl_->last_error = "BKE_gpencil_stroke_trim_points() failed";
      return false;
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::split_stroke(int index, int before_index)
{
  if (!impl_->frame || !impl_->gpd || index < 0 || before_index <= 0) {
    impl_->last_error = "invalid stroke split";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }

    if (before_index >= stroke->totpoints) {
      impl_->last_error = "stroke split index out of range";
      return false;
    }

    bGPDstroke *remaining = nullptr;
    if (!BKE_gpencil_stroke_split(
            impl_->gpd, impl_->frame, stroke, before_index, &remaining) ||
        !remaining) {
      impl_->last_error = "BKE_gpencil_stroke_split() failed";
      return false;
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
#ifndef __ANDROID__
    project_grease_gp_tag(impl_->gpd);
#endif
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::delete_last_stroke() {
  if (!impl_->frame || !impl_->frame->strokes.last) {
    impl_->last_error = "frame has no strokes";
    return false;
  }

  BKE_gpencil_frame_delete_laststroke(impl_->layer, impl_->frame);
  impl_->stroke = nullptr;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
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

#ifdef __ANDROID__
  impl_->stroke = static_cast<bGPDstroke *>(
      MEM_callocN(sizeof(bGPDstroke), "Project Grease Android GP stroke"));
  if (impl_->stroke) {
    impl_->stroke->thickness = thickness;
    impl_->stroke->fill_opacity_fac = 1.0f;
    impl_->stroke->hardeness = 1.0f;
    impl_->stroke->aspect_ratio[0] = 1.0f;
    impl_->stroke->aspect_ratio[1] = 1.0f;
    impl_->stroke->uv_scale = 1.0f;
    impl_->stroke->flag = GP_STROKE_3DSPACE;
    impl_->stroke->totpoints = point_count;
    impl_->stroke->points = static_cast<bGPDspoint *>(
        MEM_callocN(sizeof(bGPDspoint) * point_count, "Project Grease Android GP points"));
    impl_->stroke->mat_nr = material_index;
    if (impl_->stroke->points) {
      BLI_addtail(&impl_->frame->strokes, impl_->stroke);
    }
  }
#else
  impl_->stroke = BKE_gpencil_stroke_add(
      impl_->frame, material_index, point_count, thickness, false);
#endif
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
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::initialize_external_gpu_context()
{
  if (!impl_->initialized) {
    impl_->last_error = "backend is not initialized";
    return false;
  }

  if (impl_->gpu_initialized) {
    if (impl_->gpu_external_context) {
      return true;
    }
    impl_->last_error = "backend already owns a desktop GPU context";
    return false;
  }

#ifdef __ANDROID__
  // Android owns the EGL/GLES context. The legacy GP cache producer only
  // needs our Android GPU backend to be installed; creating Blender's generic
  // GPUContext would pull in the desktop backend/context dependency graph.
  impl_->gpu_external_context = true;
  impl_->gpu_initialized = true;
  impl_->last_error.clear();
  return true;
#else
  GPU_backend_type_selection_set(GPU_BACKEND_OPENGL);
  impl_->gpu_context = GPU_context_create(nullptr, nullptr);
  if (!impl_->gpu_context) {
    impl_->last_error = "GPU_context_create() failed for external GL context";
    return false;
  }

  GPU_init();
  impl_->gpu_external_context = true;
  impl_->gpu_initialized = true;
  impl_->last_error.clear();
  return true;
#endif
}

bool Backend::render_with_gpu_context()
{
  if (!impl_->gpu_initialized) {
    impl_->last_error = "Blender GP GPU backend is not initialized";
    return false;
  }

  Object *ob = nullptr;
#ifdef __ANDROID__
  Object android_ob = {};
  android_ob.type = OB_GPENCIL_LEGACY;
  android_ob.data = impl_->gpd;
  ob = &android_ob;
#else
  ob = BKE_object_add_only_object(
      impl_->bmain, OB_GPENCIL_LEGACY, "Project Grease Render");
  if (!ob) {
    impl_->last_error = "BKE_object_add_only_object() failed";
    return false;
  }
  ob->data = impl_->gpd;
#endif

  GPUBatch *batch = DRW_cache_gpencil_get(ob, impl_->frame->framenum);
  const bool cache_ready = batch != nullptr;

  DRW_gpencil_batch_cache_free(impl_->gpd);
#ifndef __ANDROID__
  ob->data = nullptr;
  BKE_id_free(impl_->bmain, &ob->id);
#endif

  impl_->last_error = cache_ready
      ? "real Blender GP draw-cache GPU batch built from current external GL context"
      : "DRW_cache_gpencil_get() returned null";
  return cache_ready;
}

bool Backend::render_external_context()
{
  if (!impl_->frame_created || !impl_->gpd || !impl_->frame) {
    impl_->last_error = "no native GP frame is ready to render";
    return false;
  }

  if (!impl_->gpu_initialized) {
    impl_->last_error = "external Blender GPU context is not initialized";
    return false;
  }

  if (!impl_->gpu_external_context) {
    impl_->last_error = "GPU context is not externally owned";
    return false;
  }

  return render_with_gpu_context();
}

bool Backend::render() {
#ifdef __ANDROID__
  // Android production owns the EGL/GLES context and never enters Blender's
  // desktop GHOST rendering path.
  return render_external_context();
#else
  if (!impl_->frame_created || !impl_->gpd || !impl_->frame) {
    impl_->last_error = "no native GP frame is ready to render";
    return false;
  }

  // Desktop/native proof path. This creates a temporary GHOST context so the
  // existing CI can continue to validate Blender's GP cache independently of
  // Android. Android will use initialize_external_gpu_context() instead.
  if (!impl_->gpu_initialized) {
    impl_->ghost_system = GHOST_CreateSystemBackground();
    if (!impl_->ghost_system) {
      impl_->last_error = "GHOST_CreateSystemBackground() failed";
      return false;
    }

    GPU_backend_type_selection_set(GPU_BACKEND_OPENGL);

    GHOST_GLSettings gl_settings = {};
    gl_settings.context_type = GHOST_kDrawingContextTypeOpenGL;
    impl_->ghost_context =
        GHOST_CreateOpenGLContext(impl_->ghost_system, gl_settings);
    if (!impl_->ghost_context) {
      GHOST_DisposeSystem(impl_->ghost_system);
      impl_->ghost_system = nullptr;
      impl_->last_error = "GHOST_CreateOpenGLContext() failed";
      return false;
    }

    if (GHOST_ActivateOpenGLContext(impl_->ghost_context) != GHOST_kSuccess) {
      GHOST_DisposeOpenGLContext(impl_->ghost_system, impl_->ghost_context);
      GHOST_DisposeSystem(impl_->ghost_system);
      impl_->ghost_context = nullptr;
      impl_->ghost_system = nullptr;
      impl_->last_error = "GHOST_ActivateOpenGLContext() failed";
      return false;
    }

    impl_->gpu_context = GPU_context_create(nullptr, impl_->ghost_context);
    if (!impl_->gpu_context) {
      GHOST_ReleaseOpenGLContext(impl_->ghost_context);
      GHOST_DisposeOpenGLContext(impl_->ghost_system, impl_->ghost_context);
      GHOST_DisposeSystem(impl_->ghost_system);
      impl_->ghost_context = nullptr;
      impl_->ghost_system = nullptr;
      impl_->last_error = "GPU_context_create() failed";
      return false;
    }

    GPU_init();
    impl_->gpu_external_context = false;
    impl_->gpu_initialized = true;
  }

  return render_with_gpu_context();
#endif
}

const char *Backend::last_error() const { return impl_->last_error.c_str(); }

}  // namespace project_grease::gp