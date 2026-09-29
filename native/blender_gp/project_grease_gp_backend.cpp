#include "project_grease_gp_backend.h"
#include "project_grease_legacy_fill.h"
#include "project_grease_legacy_primitive.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_geom.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#ifndef __ANDROID__
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"
#endif

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"

#ifdef __ANDROID__
extern "C" int project_grease_android_present_gp_document(const bGPdata *gpd, int frame_number);
extern "C" int project_grease_android_present_gp_fill_mask(const bGPdata *gpd, int frame_number);
extern "C" int project_grease_android_present_pending_stroke(
    const project_grease::gp::StrokePoint *points, int count, float thickness);
#endif

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

static Material *gp_material_at(bGPdata *gpd, int index)
{
  if (!gpd || index < 0 || index >= gpd->totcol || !gpd->mat) {
    return nullptr;
  }
  return gpd->mat[index];
}

static bool gp_material_ensure_slot(bGPdata *gpd, int index)
{
  if (!gpd || index < 0 || index > 32766) {
    return false;
  }
  if (index >= gpd->totcol) {
    const int old_count = gpd->totcol;
    const int new_count = index + 1;
    gpd->mat = static_cast<Material **>(
        MEM_recallocN(gpd->mat, sizeof(Material *) * new_count));
    if (!gpd->mat) {
      gpd->totcol = 0;
      return false;
    }
    gpd->totcol = new_count;
    for (int i = old_count; i < new_count; ++i) {
      gpd->mat[i] = nullptr;
    }
  }
  if (!gpd->mat[index]) {
    Material *ma = static_cast<Material *>(
        MEM_callocN(sizeof(Material), "Project Grease Legacy GP Material"));
    MaterialGPencilStyle *style = static_cast<MaterialGPencilStyle *>(
        MEM_callocN(sizeof(MaterialGPencilStyle), "Project Grease Legacy GP Material Style"));
    if (!ma || !style) {
      MEM_SAFE_FREE(style);
      MEM_SAFE_FREE(ma);
      return false;
    }
    ma->gp_style = style;
    style->stroke_rgba[0] = 1.0f;
    style->stroke_rgba[1] = 1.0f;
    style->stroke_rgba[2] = 1.0f;
    style->stroke_rgba[3] = 1.0f;
    style->fill_rgba[0] = 1.0f;
    style->fill_rgba[1] = 1.0f;
    style->fill_rgba[2] = 1.0f;
    style->fill_rgba[3] = 1.0f;
    style->texture_scale[0] = 1.0f;
    style->texture_scale[1] = 1.0f;
    style->texture_offset[0] = -0.5f;
    style->texture_offset[1] = -0.5f;
    style->texture_pixsize = 100.0f;
    style->mix_factor = 0.5f;
    style->stroke_style = GP_MATERIAL_STROKE_STYLE_SOLID;
    style->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
    style->flag = GP_MATERIAL_STROKE_SHOW | GP_MATERIAL_FILL_SHOW;
    gpd->mat[index] = ma;
  }
  return true;
}

static void gp_materials_free(bGPdata *gpd)
{
  if (!gpd || !gpd->mat) {
    return;
  }
  for (int i = 0; i < gpd->totcol; ++i) {
    Material *ma = gpd->mat[i];
    if (!ma) {
      continue;
    }
    MEM_SAFE_FREE(ma->gp_style);
    MEM_freeN(ma);
  }
  MEM_freeN(gpd->mat);
  gpd->mat = nullptr;
  gpd->totcol = 0;
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
    gp_materials_free(impl_->gpd);
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
  if (!gp_material_ensure_slot(impl_->gpd, 0)) {
    impl_->last_error = "Legacy GP material slot allocation failed";
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
  /* Use Blender 3.6.23's actual Legacy GP layer constructor on Android too.
   * Android is only the host; it does not recreate layer semantics. */
  impl_->layer = BKE_gpencil_layer_addnew(impl_->gpd, layer_name, true, false);
  if (!impl_->layer) {
    impl_->last_error = "BKE_gpencil_layer_addnew() failed"; return false;
  }
  impl_->layer_created = true;
  return true;
}

bool Backend::create_frame(int frame_number) {
  if (!impl_->layer_created || !impl_->layer || frame_number < 1) {
    impl_->last_error = "invalid layer/frame";
    return false;
  }
  if (BKE_gpencil_layer_frame_find(impl_->layer, frame_number)) {
    impl_->last_error = "frame already exists";
    return false;
  }
  impl_->frame = BKE_gpencil_frame_addnew(impl_->layer, frame_number);
  if (!impl_->frame) {
    impl_->last_error = "BKE_gpencil_frame_addnew() failed";
    return false;
  }
  impl_->layer->actframe = impl_->frame;
  impl_->frame_created = true;
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
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

static bGPDlayer *layer_at(bGPdata *gpd, int index)
{
  if (!gpd || index < 0) return nullptr;
  int i = 0;
  for (bGPDlayer *layer = static_cast<bGPDlayer *>(gpd->layers.first);
       layer; layer = layer->next, ++i) {
    if (i == index) return layer;
  }
  return nullptr;
}

bool Backend::set_layer_visibility(int index, bool visible)
{
  bGPDlayer *layer = layer_at(impl_->gpd, index);
  if (!layer) { impl_->last_error = "layer index out of range"; return false; }
  if (visible) layer->flag &= ~GP_LAYER_HIDE;
  else layer->flag |= GP_LAYER_HIDE;
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::set_layer_locked(int index, bool locked)
{
  bGPDlayer *layer = layer_at(impl_->gpd, index);
  if (!layer) { impl_->last_error = "layer index out of range"; return false; }
  if (locked) layer->flag |= GP_LAYER_LOCKED;
  else layer->flag &= ~GP_LAYER_LOCKED;
  return true;
}

bool Backend::move_layer(int from_index, int to_index)
{
  if (!impl_->gpd || from_index < 0 || to_index < 0 ||
      from_index >= layer_count() || to_index >= layer_count()) {
    impl_->last_error = "layer move index out of range"; return false;
  }
  if (!BLI_listbase_move_index(&impl_->gpd->layers, from_index, to_index)) return false;
  impl_->layer = layer_at(impl_->gpd, to_index);
  impl_->frame = impl_->layer ? impl_->layer->actframe : nullptr;
  impl_->frame_created = impl_->frame != nullptr;
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::duplicate_layer(int index)
{
  bGPDlayer *source = layer_at(impl_->gpd, index);
  if (!source) { impl_->last_error = "layer index out of range"; return false; }
  bGPDlayer *copy = BKE_gpencil_layer_duplicate(source, true, true);
  if (!copy) { impl_->last_error = "BKE_gpencil_layer_duplicate() failed"; return false; }
  BLI_addtail(&impl_->gpd->layers, copy);
  impl_->layer = copy;
  impl_->frame = copy->actframe;
  impl_->layer_created = true;
  impl_->frame_created = impl_->frame != nullptr;
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::delete_layer(int index)
{
  bGPDlayer *layer = layer_at(impl_->gpd, index);
  if (!layer) { impl_->last_error = "layer index out of range"; return false; }
  if (layer_count() <= 1) { impl_->last_error = "cannot delete final layer"; return false; }
  BKE_gpencil_layer_delete(impl_->gpd, layer);
  impl_->layer = static_cast<bGPDlayer *>(impl_->gpd->layers.first);
  impl_->frame = impl_->layer ? impl_->layer->actframe : nullptr;
  impl_->layer_created = impl_->layer != nullptr;
  impl_->frame_created = impl_->frame != nullptr;
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::rename_layer(int index, const char* name)
{
  bGPDlayer *layer = layer_at(impl_->gpd, index);
  if (!layer || !name || !name[0]) { impl_->last_error = "invalid layer rename"; return false; }
  std::strncpy(layer->info, name, sizeof(layer->info) - 1);
  layer->info[sizeof(layer->info) - 1] = '\0';
  project_grease_gp_tag(impl_->gpd);
  return true;
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

bool Backend::select_frame_or_hold(int frame_number)
{
  if (!impl_->layer || frame_number < 1) {
    impl_->last_error = "invalid playback frame";
    return false;
  }
  bGPDframe *frame = BKE_gpencil_layer_frame_get(
      impl_->layer, frame_number, GP_GETFRAME_USE_PREV);
  if (!frame) {
    impl_->last_error = "no preceding frame";
    return false;
  }
  impl_->frame = frame;
  impl_->layer->actframe = frame;
  impl_->stroke = nullptr;
  impl_->frame_created = true;
  impl_->last_error.clear();
  return true;
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

int Backend::frame_end() const
{
  if (!impl_->layer) return 1;
  int end_frame = 1;
  for (bGPDframe *frame = static_cast<bGPDframe *>(impl_->layer->frames.first);
       frame; frame = frame->next) {
    end_frame = std::max(end_frame, frame->framenum);
  }
  return end_frame;
}

bool Backend::duplicate_frame(int source_frame, int target_frame)
{
  if (!impl_->layer || source_frame < 1 || target_frame < 1) {
    impl_->last_error = "invalid frame duplication";
    return false;
  }
  bGPDframe *source = BKE_gpencil_layer_frame_find(impl_->layer, source_frame);
  if (!source) {
    impl_->last_error = "source frame not found";
    return false;
  }
  if (BKE_gpencil_layer_frame_find(impl_->layer, target_frame)) {
    impl_->last_error = "target frame already exists";
    return false;
  }
  bGPDframe *copy = BKE_gpencil_frame_duplicate(source, true);
  if (!copy) {
    impl_->last_error = "BKE_gpencil_frame_duplicate() failed";
    return false;
  }
  copy->framenum = target_frame;
  BLI_addtail(&impl_->layer->frames, copy);
  BKE_gpencil_layer_frames_sort(impl_->layer, nullptr);
  impl_->layer->actframe = copy;
  impl_->frame = copy;
  impl_->frame_created = true;
  impl_->stroke = nullptr;
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}


bool Backend::delete_frame(int frame_number)
{
  if (!impl_->layer) {
    impl_->last_error = "layer is not selected";
    return false;
  }
  bGPDframe *target = BKE_gpencil_layer_frame_find(impl_->layer, frame_number);
  if (!target) {
    impl_->last_error = "frame number not found";
    return false;
  }
  if (!BKE_gpencil_layer_frame_delete(impl_->layer, target)) {
    impl_->last_error = "BKE_gpencil_layer_frame_delete() failed";
    return false;
  }
  impl_->frame = static_cast<bGPDframe *>(impl_->layer->frames.first);
  impl_->layer->actframe = impl_->frame;
  impl_->frame_created = impl_->frame != nullptr;
  impl_->stroke = nullptr;
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
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
      clear_selection();
      stroke->flag |= GP_STROKE_SELECT;
      if (stroke->points) {
        for (int i = 0; i < stroke->totpoints; ++i) {
          stroke->points[i].flag |= GP_SPOINT_SELECT;
        }
      }
      impl_->stroke = stroke;
      impl_->last_error.clear();
      return true;
    }
  }

  impl_->last_error = "stroke index out of range";
  return false;
}

int Backend::hit_test_stroke(float x, float y, float radius) const
{
  if (!impl_->frame || radius < 0.0f) {
    return -1;
  }

  const float radius_sq = radius * radius;
  float best_distance_sq = radius_sq;
  int best_index = -1;
  int current = 0;

  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (!stroke->points || stroke->totpoints <= 0) {
      continue;
    }

    for (int i = 0; i < stroke->totpoints; ++i) {
      const float px = stroke->points[i].x;
      const float py = stroke->points[i].y;
      const float dx = x - px;
      const float dy = y - py;
      const float point_distance_sq = dx * dx + dy * dy;
      if (point_distance_sq <= best_distance_sq) {
        best_distance_sq = point_distance_sq;
        best_index = current;
      }
    }

    if (stroke->totpoints >= 2) {
      const bool cyclic = (stroke->flag & GP_STROKE_CYCLIC) != 0;
      const int segment_count = cyclic ? stroke->totpoints : stroke->totpoints - 1;
      for (int i = 0; i < segment_count; ++i) {
        const bGPDspoint &a = stroke->points[i];
        const bGPDspoint &b = stroke->points[(i + 1) % stroke->totpoints];
        const float vx = b.x - a.x;
        const float vy = b.y - a.y;
        const float len_sq = vx * vx + vy * vy;

        float t = 0.0f;
        if (len_sq > 1.0e-12f) {
          t = ((x - a.x) * vx + (y - a.y) * vy) / len_sq;
          t = std::fmax(0.0f, std::fmin(1.0f, t));
        }

        const float cx = a.x + t * vx;
        const float cy = a.y + t * vy;
        const float dx = x - cx;
        const float dy = y - cy;
        const float distance_sq = dx * dx + dy * dy;
        if (distance_sq <= best_distance_sq) {
          best_distance_sq = distance_sq;
          best_index = current;
        }
      }
    }
  }

  return best_index;
}


bool Backend::create_primitive(int type,
                               float x0,
                               float y0,
                               float x1,
                               float y1,
                               float start_angle,
                               float end_angle,
                               int segments,
                               const StrokeStyle &style)
{
  if (!impl_->frame || !impl_->frame_created) {
    impl_->last_error = "frame is not created";
    return false;
  }
  if (type < 0 || type > 3) {
    impl_->last_error = "invalid primitive type";
    return false;
  }

  const legacy_gp_primitive::Point start{x0, y0};
  const legacy_gp_primitive::Point end{x1, y1};

  // Keep final geometry on the same native Blender-3.6.23-derived
  // primitive generator used by Android preview. The modal UI is not copied;
  // only the primitive geometry core is shared.
  const std::vector<legacy_gp_primitive::Point> geometry =
      legacy_gp_primitive::generate(type, start, end, start_angle, end_angle, segments);
  bool cyclic = (type == 1 || type == 2);
  if (geometry.empty()) {
    impl_->last_error = "invalid primitive type";
    return false;
  }

  if (geometry.size() < 2) {
    impl_->last_error = "primitive geometry contains too few points";
    return false;
  }

  std::vector<StrokePoint> points;
  points.reserve(geometry.size());
  for (size_t i = 0; i < geometry.size(); ++i) {
    points.push_back({geometry[i].x,
                      geometry[i].y,
                      0.0f,
                      1.0f,
                      1.0f,
                      static_cast<float>(i)});
  }

  return create_polyline(points.data(), static_cast<int>(points.size()), style, cyclic);
}

bool Backend::create_polyline(const StrokePoint *points,
                              int count,
                              const StrokeStyle &style,
                              bool cyclic)
{
  if (!impl_->frame || !impl_->frame_created) {
    impl_->last_error = "frame is not created";
    return false;
  }
  if (!points || count < 2) {
    impl_->last_error = "polyline needs at least two points";
    return false;
  }

  const short thickness = static_cast<short>(
      std::max(1.0f, std::min(style.thickness, 32767.0f)));
  bGPDstroke *stroke = BKE_gpencil_stroke_add(
      impl_->frame, style.material_index, count, thickness, false);
  if (!stroke) {
    impl_->last_error = "BKE_gpencil_stroke_add() failed";
    return false;
  }

  for (int i = 0; i < count; ++i) {
    const StrokePoint &src = points[i];
    bGPDspoint &dst = stroke->points[i];
    dst.x = src.x;
    dst.y = src.y;
    dst.z = src.z;
    dst.pressure = std::max(0.0f, src.pressure);
    dst.strength = std::max(0.0f, std::min(src.strength, 1.0f));
    dst.time = src.time;
  }

  if (cyclic) {
    stroke->flag |= GP_STROKE_CYCLIC;
    BKE_gpencil_stroke_fill_triangulate(stroke);
  }

  impl_->stroke = stroke;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

static float project_grease_legacy_eraser_influence(
    float x, float y, float radius, float point_x, float point_y)
{
  if (radius <= 0.0f) {
    return 0.0f;
  }

  const float dx = x - point_x;
  const float dy = y - point_y;
  const float distance = std::sqrt(dx * dx + dy * dy);
  const float clamped = std::min(distance, radius);

  /*
   * Blender 3.6.23 Legacy GP hard-eraser influence:
   * fac = 1 - distance / radius, multiplied by draw strength and pen
   * pressure. Project Grease currently exposes a full-strength hard eraser,
   * so the remaining factor is 1.0 here.
   */
  return 1.0f - (clamped / radius);
}

bool Backend::erase_at(float x, float y, float radius)
{
  if (!impl_->frame || radius <= 0.0f ||
      !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius)) {
    impl_->last_error = "invalid eraser";
    return false;
  }

  bool removed = false;

  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;) {
    bGPDstroke *next = stroke->next;
    bool stroke_removed = false;

    if (!stroke->points || stroke->totpoints <= 0) {
      stroke = next;
      continue;
    }

    if (stroke->totpoints == 1) {
      bGPDspoint &point = stroke->points[0];
      const float influence =
          project_grease_legacy_eraser_influence(x, y, radius, point.x, point.y);
      if (influence > 0.0f) {
        point.pressure = 0.0f;
        point.flag |= GP_SPOINT_TAG;
        removed = true;
        stroke_removed = true;
      }
    }
    else {
      for (int i = 0; i + 1 < stroke->totpoints; ++i) {
        bGPDspoint *pt0 = (i > 0) ? &stroke->points[i - 1] : nullptr;
        bGPDspoint *pt1 = &stroke->points[i];
        bGPDspoint *pt2 = &stroke->points[i + 1];

        const float p0x = pt0 ? pt0->x : pt1->x;
        const float p0y = pt0 ? pt0->y : pt1->y;

        /*
         * Blender's Legacy GP eraser first performs an edge-vs-circle
         * collision test using the segment pt0 -> pt2.
         */
        const float edge_a[2] = {p0x, p0y};
        const float edge_b[2] = {pt2->x, pt2->y};
        const float center[2] = {x, y};
        if (dist_squared_to_line_segment_v2(center, edge_a, edge_b) >=
            radius * radius) {
          continue;
        }

        const float inf1 =
            project_grease_legacy_eraser_influence(x, y, radius, pt1->x, pt1->y);
        const float inf2 =
            project_grease_legacy_eraser_influence(x, y, radius, pt2->x, pt2->y);

        if (inf1 > 0.0f) {
          pt1->pressure = 0.0f;
          pt1->flag |= GP_SPOINT_TAG;
          removed = true;
          stroke_removed = true;
        }
        if (inf2 > 0.0f) {
          pt2->pressure = 0.0f;
          pt2->flag |= GP_SPOINT_TAG;
          removed = true;
          stroke_removed = true;
        }
      }
    }

    if (stroke_removed) {
      BKE_gpencil_stroke_delete_tagged_points(
          impl_->gpd, impl_->frame, stroke, next, GP_SPOINT_TAG, false, false, 0);
    }

    stroke = next;
  }

  if (removed) {
    impl_->stroke = nullptr;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "eraser did not hit a stroke";
  return false;
}


void Backend::clear_selection()
{
  if (!impl_->frame) {
    return;
  }
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    stroke->flag &= ~GP_STROKE_SELECT;
    if (stroke->points) {
      for (int i = 0; i < stroke->totpoints; ++i) {
        stroke->points[i].flag &= ~GP_SPOINT_SELECT;
      }
    }
  }
  impl_->stroke = nullptr;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
}

static bool project_grease_point_in_polygon(float x, float y, const float *xy, int count)
{
  if (!xy || count < 3) {
    return false;
  }

  /*
   * Blender 3.6.23 Legacy GP lasso selection uses BLI_lasso_is_point_inside()
   * after converting GP points to the selection space. Project Grease owns
   * the Android canvas coordinate space, so the Android lasso coordinates and
   * GP point coordinates are already in the same 2D space here.
   */
  std::vector<int> lasso_coords(static_cast<size_t>(count) * 2u);
  for (int i = 0; i < count; ++i) {
    lasso_coords[static_cast<size_t>(i) * 2u] = static_cast<int>(std::lround(xy[i * 2]));
    lasso_coords[static_cast<size_t>(i) * 2u + 1u] = static_cast<int>(std::lround(xy[i * 2 + 1]));
  }

  return BLI_lasso_is_point_inside(reinterpret_cast<const int (*)[2]>(lasso_coords.data()),
                                    count,
                                    static_cast<int>(std::lround(x)),
                                    static_cast<int>(std::lround(y)),
                                    INT_MAX);
}

int Backend::lasso_select(const float *xy, int count, bool additive)
{
  if (!impl_->frame || !xy || count < 3) {
    impl_->last_error = "invalid lasso";
    return 0;
  }

  /*
   * Blender 3.6.23 GPENCIL_OT_select_lasso defaults to SET selection.
   * Keep that behavior for the Project Grease lasso tool; additive is the
   * Android-side equivalent of extending the selection.
   */
  if (!additive) {
    clear_selection();
  }

  int selected = 0;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    if (!stroke->points || stroke->totpoints <= 0) {
      continue;
    }

    bool stroke_hit = false;
    int inside_points = 0;
    const int original_points = stroke->totpoints;

    for (int i = 0; i < original_points; ++i) {
      bGPDspoint &point = stroke->points[i];
      if (project_grease_point_in_polygon(point.x, point.y, xy, count)) {
        point.flag |= GP_SPOINT_SELECT;
        stroke_hit = true;
        ++inside_points;
      }
    }

    if (stroke_hit) {
      /*
       * Point-selection mode is the Legacy GP default used by this adapter:
       * points inside the lasso are selected and the containing stroke becomes
       * selected/synchronized. If the lasso contains every point, the result
       * is therefore a fully selected stroke, matching Legacy GP's point-mode
       * behavior.
       */
      stroke->flag |= GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_set(impl_->gpd, stroke);
      ++selected;
      (void)inside_points;
    }
    else if (!additive) {
      BKE_gpencil_stroke_sync_selection(impl_->gpd, stroke);
    }
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return selected;
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


static int selected_point_count(const bGPDstroke *stroke)
{
  if (!stroke || !stroke->points || stroke->totpoints <= 0) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < stroke->totpoints; ++i) {
    if (stroke->points[i].flag & GP_SPOINT_SELECT) {
      ++count;
    }
  }
  return count;
}

bool Backend::stroke_center(int index, float *x, float *y) const
{
  if (!impl_->frame || index < 0 || !x || !y) {
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
      return false;
    }

    /*
     * Match Blender 3.6.23 Legacy GP transform conversion:
     * the transform pivot is the median of selected points. This matters
     * for lasso/point selection; falling back to the whole stroke would
     * move the pivot away from the actual Blender selection.
     */
    double sum_x = 0.0;
    double sum_y = 0.0;
    int selected = 0;
    for (int i = 0; i < stroke->totpoints; ++i) {
      const bGPDspoint &point = stroke->points[i];
      if (point.flag & GP_SPOINT_SELECT) {
        sum_x += point.x;
        sum_y += point.y;
        ++selected;
      }
    }

    if (selected == 0) {
      return false;
    }

    *x = static_cast<float>(sum_x / selected);
    *y = static_cast<float>(sum_y / selected);
    return true;
  }
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

    if (selected_point_count(stroke) == 0) {
      impl_->last_error = "stroke has no selected points";
      return false;
    }

    /* Blender's Legacy GP transform path operates on selected points. */
    for (int point_index = 0; point_index < stroke->totpoints; ++point_index) {
      bGPDspoint &point = stroke->points[point_index];
      if (!(point.flag & GP_SPOINT_SELECT)) {
        continue;
      }
      point.x += dx;
      point.y += dy;
      point.z += dz;
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
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



bool Backend::rotate_stroke(int index, float radians)
{
  float cx = 0.0f, cy = 0.0f;
  if (!stroke_center(index, &cx, &cy)) {
    impl_->last_error = "invalid stroke rotation";
    return false;
  }
  return rotate_stroke_about(index, radians, cx, cy);
}

bool Backend::rotate_stroke_about(int index, float radians, float center_x, float center_y)
{
  if (!impl_->frame || index < 0 || !std::isfinite(radians) ||
      !std::isfinite(center_x) || !std::isfinite(center_y)) {
    impl_->last_error = "invalid stroke rotation";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }
    if (stroke->totpoints <= 0 || !stroke->points) {
      impl_->last_error = "stroke has no points";
      return false;
    }
    if (selected_point_count(stroke) == 0) {
      impl_->last_error = "stroke has no selected points";
      return false;
    }

    const float c = std::cos(radians);
    const float ss = std::sin(radians);
    for (int i = 0; i < stroke->totpoints; ++i) {
      bGPDspoint &p = stroke->points[i];
      if (!(p.flag & GP_SPOINT_SELECT)) {
        continue;
      }
      const float x = p.x - center_x;
      const float y = p.y - center_y;
      p.x = center_x + x * c - y * ss;
      p.y = center_y + x * ss + y * c;
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}


bool Backend::scale_stroke(int index, float scale_x, float scale_y)
{
  float cx = 0.0f, cy = 0.0f;
  if (!stroke_center(index, &cx, &cy)) {
    impl_->last_error = "invalid stroke scale";
    return false;
  }
  return scale_stroke_about(index, scale_x, scale_y, cx, cy);
}

bool Backend::scale_stroke_about(
    int index, float scale_x, float scale_y, float center_x, float center_y)
{
  if (!impl_->frame || index < 0 || !std::isfinite(scale_x) || !std::isfinite(scale_y) ||
      scale_x == 0.0f || scale_y == 0.0f ||
      !std::isfinite(center_x) || !std::isfinite(center_y)) {
    impl_->last_error = "invalid stroke scale";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }
    if (stroke->totpoints <= 0 || !stroke->points) {
      impl_->last_error = "stroke has no points";
      return false;
    }
    if (selected_point_count(stroke) == 0) {
      impl_->last_error = "stroke has no selected points";
      return false;
    }

    for (int i = 0; i < stroke->totpoints; ++i) {
      bGPDspoint &p = stroke->points[i];
      if (!(p.flag & GP_SPOINT_SELECT)) {
        continue;
      }
      p.x = center_x + (p.x - center_x) * scale_x;
      p.y = center_y + (p.y - center_y) * scale_y;
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }

  impl_->last_error = "stroke index out of range";
  return false;
}


bool Backend::mirror_stroke(int index, bool mirror_x, bool mirror_y)
{
  float cx = 0.0f, cy = 0.0f;
  if (!stroke_center(index, &cx, &cy)) {
    impl_->last_error = "invalid stroke mirror";
    return false;
  }
  return mirror_stroke_about(index, mirror_x, mirror_y, cx, cy);
}

bool Backend::mirror_stroke_about(
    int index, bool mirror_x, bool mirror_y, float center_x, float center_y)
{
  if (!impl_->frame || index < 0 || (!mirror_x && !mirror_y) ||
      !std::isfinite(center_x) || !std::isfinite(center_y)) {
    impl_->last_error = "invalid stroke mirror";
    return false;
  }

  int current = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }
    if (stroke->totpoints <= 0 || !stroke->points) {
      impl_->last_error = "stroke has no points";
      return false;
    }
    if (selected_point_count(stroke) == 0) {
      impl_->last_error = "stroke has no selected points";
      return false;
    }

    for (int i = 0; i < stroke->totpoints; ++i) {
      bGPDspoint &p = stroke->points[i];
      if (!(p.flag & GP_SPOINT_SELECT)) {
        continue;
      }
      if (mirror_x) {
        p.x = 2.0f * center_x - p.x;
      }
      if (mirror_y) {
        p.y = 2.0f * center_y - p.y;
      }
    }

    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
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
    std::fprintf(stderr,
                 "[CLOSE] before call stroke=%p points=%d flags=0x%x\\n",
                 static_cast<void *>(stroke),
                 points_before_close,
                 flags_before_close);

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
    BKE_gpencil_stroke_fill_triangulate(stroke);
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

bool Backend::trim_stroke(int index) {
  if (!impl_->frame || !impl_->gpd || index < 0) {
    impl_->last_error = "invalid stroke trim";
    return false;
  }
  int current = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr; stroke = stroke->next, ++current) {
    if (current != index) continue;
    if (stroke->totpoints < 4 || !stroke->points) {
      impl_->last_error = "stroke needs at least four points for Legacy GP trim";
      return false;
    }
    if (!BKE_gpencil_stroke_trim(stroke)) {
      impl_->last_error = "BKE_gpencil_stroke_trim() found no intersection";
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
        const bool split_result = BKE_gpencil_stroke_split(
        impl_->gpd, impl_->frame, stroke, before_index, &remaining);
    if (!split_result || !remaining) {
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
  if (!gp_material_ensure_slot(impl_->gpd, style.material_index)) {
    impl_->last_error = "Legacy GP material index is unavailable";
    return false;
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

  /* Exact Blender Legacy GP stroke allocation path. */
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
  if (impl_->stroke->flag & GP_STROKE_CYCLIC) {
    BKE_gpencil_stroke_fill_triangulate(impl_->stroke);
  }
  impl_->gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::cancel_stroke()
{
  if (!impl_->stroke_open) {
    impl_->pending_points.clear();
    impl_->stroke = nullptr;
    return true;
  }

  // The stroke is only allocated in end_stroke(). During input collection
  // Android holds points in pending_points, so cancellation must discard the
  // pending input without creating a Blender bGPDstroke.
  impl_->pending_points.clear();
  impl_->stroke = nullptr;
  impl_->stroke_open = false;
  impl_->last_error.clear();
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
#ifdef __ANDROID__
  // The Legacy GP cache is the Blender geometry/data proof layer. Android
  // presents the same real bGPDframe through a focused GLES adapter because
  // the desktop GP shader stack depends on buffer-texture/material
  // infrastructure that is intentionally outside Project Grease scope.
  bool presented = cache_ready &&
      project_grease_android_present_gp_document(impl_->gpd, impl_->frame->framenum) != 0;
  if (presented && impl_->stroke_open && !impl_->pending_points.empty()) {
    presented = project_grease_android_present_pending_stroke(
        impl_->pending_points.data(),
        static_cast<int>(impl_->pending_points.size()),
        impl_->stroke_style.thickness) != 0;
  }
#else
  const bool presented = cache_ready;
#endif

  DRW_gpencil_batch_cache_free(impl_->gpd);
#ifndef __ANDROID__
  ob->data = nullptr;
  BKE_id_free(impl_->bmain, &ob->id);
#endif

  impl_->last_error = !cache_ready
      ? "DRW_cache_gpencil_get() returned null"
      : !presented
          ? "Blender Legacy GP cache built, but Android presentation failed"
          : "Blender Legacy GP data/cache accepted and presented through Android GLES";
  return presented;
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


bool Backend::render_fill_mask()
{
#ifdef __ANDROID__
  if (!impl_->gpd || !impl_->frame_created || !impl_->frame) {
    impl_->last_error = "no native GP frame is ready for fill mask";
    return false;
  }
  if (!impl_->gpu_initialized || !impl_->gpu_external_context) {
    impl_->last_error = "external Android GPU context is not ready for fill mask";
    return false;
  }
  if (!project_grease_android_present_gp_fill_mask(
          impl_->gpd, impl_->frame->framenum)) {
    impl_->last_error = "Legacy GP fill mask presentation failed";
    return false;
  }
  impl_->last_error.clear();
  return true;
#else
  impl_->last_error = "Legacy GP fill mask is an Android integration operation";
  return false;
#endif
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


int Backend::material_count() const
{
  return impl_->gpd ? impl_->gpd->totcol : 0;
}

bool Backend::create_material()
{
  if (!impl_->gpd || !gp_material_ensure_slot(impl_->gpd, impl_->gpd->totcol)) {
    impl_->last_error = "Legacy GP material creation failed";
    return false;
  }
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::set_material_colors(int index, const float stroke_rgba[4], const float fill_rgba[4])
{
  Material *ma = gp_material_at(impl_->gpd, index);
  if (!ma || !ma->gp_style || !stroke_rgba || !fill_rgba) {
    impl_->last_error = "invalid Legacy GP material";
    return false;
  }
  std::memcpy(ma->gp_style->stroke_rgba, stroke_rgba, sizeof(float) * 4);
  std::memcpy(ma->gp_style->fill_rgba, fill_rgba, sizeof(float) * 4);
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::set_material_visibility(int index, bool visible)
{
  Material *ma = gp_material_at(impl_->gpd, index);
  if (!ma || !ma->gp_style) {
    impl_->last_error = "invalid Legacy GP material";
    return false;
  }
  if (visible) {
    ma->gp_style->flag &= ~GP_MATERIAL_HIDE;
  }
  else {
    ma->gp_style->flag |= GP_MATERIAL_HIDE;
  }
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::set_material_fill_enabled(int index, bool enabled)
{
  Material *ma = gp_material_at(impl_->gpd, index);
  if (!ma || !ma->gp_style) {
    impl_->last_error = "invalid Legacy GP material";
    return false;
  }
  if (enabled) {
    ma->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }
  else {
    ma->gp_style->flag &= ~GP_MATERIAL_FILL_SHOW;
  }
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::smooth_stroke(int index, float influence, int iterations)
{
  if (!impl_->frame || index < 0 || influence <= 0.0f || iterations <= 0) {
    impl_->last_error = "invalid Legacy GP smooth parameters";
    return false;
  }
  int current = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke; stroke = stroke->next, ++current) {
    if (current != index) {
      continue;
    }
    BKE_gpencil_stroke_smooth(stroke,
                              std::min(influence, 1.0f),
                              iterations,
                              true,
                              false,
                              false,
                              false,
                              true,
                              nullptr);
    impl_->stroke = stroke;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }
  impl_->last_error = "stroke index out of range";
  return false;
}

bool Backend::set_onion_skin(bool enabled, int before, int after, float opacity)
{
  if (!impl_->layer) {
    impl_->last_error = "layer is not selected";
    return false;
  }
  impl_->layer->onion_flag = enabled ? (impl_->layer->onion_flag | GP_LAYER_ONIONSKIN) :
                                       (impl_->layer->onion_flag & ~GP_LAYER_ONIONSKIN);
  impl_->layer->gstep = static_cast<short>(std::max(0, std::min(before, 100)));
  impl_->layer->gstep_next = static_cast<short>(std::max(0, std::min(after, 100)));
  impl_->gpd->onion_factor = std::max(0.0f, std::min(opacity, 1.0f));
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::fill_stroke(int index)
{
  if (!impl_->frame || index < 0) {
    impl_->last_error = "invalid fill stroke";
    return false;
  }

  int current = 0;
  bGPDstroke *stroke = nullptr;
  for (bGPDstroke *candidate = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       candidate != nullptr;
       candidate = candidate->next, ++current) {
    if (current == index) {
      stroke = candidate;
      break;
    }
  }

  if (!stroke || !stroke->points || stroke->totpoints < 3) {
    impl_->last_error = "fill requires a stroke with at least three points";
    return false;
  }

  if ((stroke->flag & GP_STROKE_CYCLIC) == 0) {
    impl_->last_error = "fill requires a closed Legacy GP stroke";
    return false;
  }

  // This is Blender 3.6.23 Legacy GP's real fill-geometry operation.
  // Do not replace this with a Project Grease triangulation algorithm.
  BKE_gpencil_stroke_fill_triangulate(stroke);
  if (!stroke->triangles || stroke->tot_triangles <= 0) {
    impl_->last_error = "Legacy GP fill triangulation produced no triangles";
    return false;
  }

  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}


bool Backend::fill_at_screen(const float* rgba,
                             int width,
                             int height,
                             int seed_x,
                             int seed_y,
                             int fill_leak,
                             int dilate_pixels,
                             const StrokeStyle& style)
{
  if (!impl_->frame || !impl_->frame_created || !rgba ||
      width < 3 || height < 3 ||
      seed_x < 0 || seed_x >= width ||
      seed_y < 0 || seed_y >= height) {
    impl_->last_error = "invalid Legacy GP fill raster or seed";
    return false;
  }

  legacy_gp_fill::Image image(width, height);
  const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  std::copy(rgba, rgba + count, image.rgba().begin());

  // Blender's fill operator works on a stroke-only render mask. The Android
  // presentation layer supplies that mask; this call performs Blender 3.6.23's
  // boundary-fill + Moore-neighborhood outline extraction.
  legacy_gp_fill::normalize_to_legacy_mask(image, 0.5f);

  // GLES readback has its origin at the lower-left; Android touch coordinates
  // are top-left based.
  const int raster_seed_y = height - 1 - seed_y;
  legacy_gp_fill::Result result =
      legacy_gp_fill::run(image, seed_x, raster_seed_y, fill_leak, dilate_pixels);

  if (!result.valid || result.outline.size() < 3) {
    impl_->last_error = result.border_contact
        ? "Legacy GP fill reached the render boundary"
        : "Legacy GP fill found no closed area";
    return false;
  }

  std::vector<StrokePoint> points;
  points.reserve(result.outline.size());
  for (const legacy_gp_fill::Point& p : result.outline) {
    points.push_back({p.x,
                      static_cast<float>(height) - p.y,
                      0.0f,
                      1.0f,
                      1.0f,
                      0.0f});
  }

  if (!create_polyline(points.data(),
                       static_cast<int>(points.size()),
                       style,
                       true)) {
    return false;
  }

  // Use Blender's real Legacy GP geometry update/smoothing path on the new
  // stroke rather than a Project Grease replacement.
  if (impl_->stroke) {
    BKE_gpencil_stroke_smooth(impl_->stroke,
                              1.0f,
                              2,
                              true,
                              false,
                              false,
                              false,
                              true,
                              nullptr);
    BKE_gpencil_stroke_geometry_update(impl_->gpd, impl_->stroke);
  }

  impl_->last_error.clear();
  return true;
}

bool Backend::set_multiframe_editing(bool enabled)
{
  if (!impl_->gpd) {
    impl_->last_error = "document is not created";
    return false;
  }
  if (enabled) {
    impl_->gpd->flag |= GP_DATA_STROKE_MULTIEDIT;
  }
  else {
    impl_->gpd->flag &= ~GP_DATA_STROKE_MULTIEDIT;
  }
  project_grease_gp_tag(impl_->gpd);
  return true;
}

const char *Backend::last_error() const { return impl_->last_error.c_str(); }

}  // namespace project_grease::gp