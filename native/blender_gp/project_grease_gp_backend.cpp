#include "project_grease_gp_backend.h"
#if defined(__has_include)
#if __has_include(<android/log.h>)
#include <android/log.h>
#define PG_HAVE_ANDROID_LOG 1
#endif
#endif
#include "project_grease_blender_edit6.h"
#include "project_grease_blender_edit7.h"
#include "project_grease_annotations.h"
#include "project_grease_legacy_fill.h"
#include "project_grease_blender_fill.h"
#include <cstdlib>
#include "project_grease_legacy_primitive.h"
#include "project_grease_legacy_eraser.h"
#include "project_grease_blender_edit.h"
#include "project_grease_document_state.h"
#include "project_grease_modifier_stack.h"
#include "project_grease_shader_fx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_lasso_2d.h"
#include "BLI_math_geom.h"

#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BLI_string_utils.h"
#include "DNA_gpencil_modifier_types.h"
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
#include "ED_gpencil_legacy.h"

extern "C" bool project_grease_legacy_build_apply(bGPdata *gpd, bGPDframe *gpf, BuildGpencilModifierData *mmd, float factor);

#ifdef __ANDROID__
extern "C" int project_grease_android_present_gp_document(const bGPdata *gpd, int frame_number);
extern "C" int project_grease_android_present_annotations(const bGPdata *annot, int frame_number);
extern "C" int project_grease_android_present_gp_fill_mask(const bGPdata *gpd, int frame_number);
typedef const bGPDframe *(*ProjectGreaseGPFrameEvaluator)(void *user, const bGPDlayer *layer,
                                                          const bGPDframe *current, int frame_number);
extern "C" void project_grease_android_set_frame_evaluator(ProjectGreaseGPFrameEvaluator fn, void *user);
typedef int (*ProjectGreaseGPFxProvider)(void *user, const bGPDlayer *layer, const PGFxEntry **entries);
extern "C" void project_grease_android_set_fx_provider(ProjectGreaseGPFxProvider fn, void *user);
extern "C" int project_grease_android_present_pending_stroke(
    const project_grease::gp::StrokePoint *points, int count, float thickness);
#endif

#ifndef __ANDROID__
#include "GPU_context.h"
#include "GPU_init_exit.h"
#include "GHOST_C-api.h"
#endif

namespace project_grease::gp {

/* Bumped by every document edit (all edits go through project_grease_gp_tag). The evaluated
 * modifier-stack frames are keyed by it, so any edit invalidates them. A process-wide counter can
 * only over-invalidate when several backends exist. */
static uint64_t g_gp_data_revision = 1;

/* Installed as BKE_gpencil_batch_cache_dirty_tag_cb: the Blender-side edit code (edit commands,
 * BKE_gpencil_tag) reports every stroke change through it, so those changes invalidate the
 * evaluated modifier frames too, then the draw cache is dirtied as before. */
static void project_grease_gp_dirty_tag_callback(bGPdata *gpd)
{
  g_gp_data_revision++;
  DRW_gpencil_batch_cache_dirty_tag(gpd);
}

static void project_grease_gp_tag(bGPdata *gpd)
{
  g_gp_data_revision++;
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
    // Default outline is dark so it remains visible on the light canvas.
    style->stroke_rgba[0] = 0.05f;
    style->stroke_rgba[1] = 0.05f;
    style->stroke_rgba[2] = 0.05f;
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
    style->flag = GP_MATERIAL_STROKE_SHOW;
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

/* Deep copy of a material slot array: undo snapshots own their materials (Blender's memfile undo
 * restores Material datablocks too), so a colour/fill/line-type edit is undone and a slot freed by
 * Delete Material never leaves a snapshot pointing at freed memory. */
static Material **gp_materials_duplicate(Material *const *source, int count)
{
  if (!source || count <= 0) {
    return nullptr;
  }
  Material **out = static_cast<Material **>(MEM_callocN(sizeof(Material *) * count, "PG history materials"));
  if (!out) {
    return nullptr;
  }
  for (int i = 0; i < count; ++i) {
    if (!source[i]) {
      continue;
    }
    Material *ma = static_cast<Material *>(MEM_dupallocN(source[i]));
    if (!ma) {
      continue;
    }
    ma->gp_style = source[i]->gp_style ?
                       static_cast<MaterialGPencilStyle *>(MEM_dupallocN(source[i]->gp_style)) :
                       nullptr;
    out[i] = ma;
  }
  return out;
}

using ModifierStacks = std::vector<std::vector<PGModEntry>>;
using FxStacks = std::vector<std::vector<PGFxEntry>>;

struct HistorySnapshot {
  bGPdata *data = nullptr;
  ModifierStacks stacks; /* the live modifier stack of every layer, by layer index */
  FxStacks fx_stacks;    /* the shader effect list of every layer, by layer index */
};

/* One evaluated copy of a layer's current frame (see pg_mod_eval_frame). */
struct EvalCacheEntry {
  const bGPDlayer *layer = nullptr;
  const bGPDframe *source = nullptr;
  int cfra = 0;
  uint64_t revision = 0;
  uint64_t stack_revision = 0;
  bGPDframe frame{};
};

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
  LegacyPaintSettings paint_settings{};
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

  std::vector<HistorySnapshot *> undo_history;
  float fill_map_scale = 1.0f, fill_map_origin_x = 0.0f, fill_map_origin_y = 0.0f;
  // Brush fill_factor ("Precision"), Blender default 1.
  float fill_factor = 1.0f;
  std::vector<HistorySnapshot *> redo_history;

  // Live modifier stacks: stacks[i] belongs to the i-th layer of gpd->layers. They are document
  // state (saved, in undo snapshots); strokes never carry modifiers.
  ModifierStacks stacks;
  // Shader effect lists (2D post-pass, see project_grease_shader_fx.h), same indexing and lifetime.
  FxStacks fx_stacks;
  uint64_t stack_revision = 1;
  uint64_t eval_count = 0;
  // Annotation data (project_grease_annotations.h): a second bGPdata drawn on top of the
  // drawing, not part of the drawing's layers, undo snapshots or modifier evaluation.
  bGPdata *annotations = nullptr;
  bool annotations_visible = true;
  std::vector<std::unique_ptr<EvalCacheEntry>> eval_cache;
};

static void eval_cache_clear(Backend::Impl *impl)
{
  for (auto &entry : impl->eval_cache) {
    pg_mod_eval_free(&entry->frame);
  }
  impl->eval_cache.clear();
}

static int layer_index_of(const bGPdata *gpd, const bGPDlayer *layer)
{
  int i = 0;
  for (const bGPDlayer *l = static_cast<const bGPDlayer *>(gpd->layers.first); l; l = l->next, ++i) {
    if (l == layer) {
      return i;
    }
  }
  return -1;
}

static int layer_total(const bGPdata *gpd)
{
  return gpd ? BLI_listbase_count(&gpd->layers) : 0;
}

/* Keeps stacks[] the same length as the layer list (new layers start empty). */
static std::vector<PGModEntry> *layer_stack(Backend::Impl *impl, int layer_index)
{
  if (!impl->gpd || layer_index < 0 || layer_index >= layer_total(impl->gpd)) {
    return nullptr;
  }
  impl->stacks.resize(static_cast<size_t>(layer_total(impl->gpd)));
  return &impl->stacks[static_cast<size_t>(layer_index)];
}

static std::vector<PGFxEntry> *layer_fx(Backend::Impl *impl, int layer_index)
{
  if (!impl->gpd || layer_index < 0 || layer_index >= layer_total(impl->gpd)) {
    return nullptr;
  }
  impl->fx_stacks.resize(static_cast<size_t>(layer_total(impl->gpd)));
  return &impl->fx_stacks[static_cast<size_t>(layer_index)];
}

/* Presenter callback (see Backend::evaluated_frame). */
[[maybe_unused]] static const bGPDframe *eval_frame_trampoline(void *user, const bGPDlayer *layer,
                                                              const bGPDframe *current,
                                                              int frame_number)
{
  return static_cast<Backend *>(user)->evaluated_frame(layer, current, frame_number);
}

/* The evaluated copy of `current` for `layer`, or `current` itself when the layer has no enabled
 * modifier. The copy is cached until an edit, a frame change or a stack change. */
const bGPDframe *Backend::evaluated_frame(const bGPDlayer *layer, const bGPDframe *current,
                                          int frame_number)
{
  Impl *impl = impl_;
  if (!impl || !impl->gpd || !layer || !current) {
    return current;
  }
  const int index = layer_index_of(impl->gpd, layer);
  if (index < 0 || static_cast<size_t>(index) >= impl->stacks.size()) {
    return current;
  }
  const std::vector<PGModEntry> &stack = impl->stacks[static_cast<size_t>(index)];
  bool any_enabled = false;
  bool time_offset = false;
  for (const PGModEntry &e : stack) {
    any_enabled |= e.enabled != 0 && pg_mod_valid_type(e.type) != 0;
    time_offset |= e.enabled != 0 && e.type == PG_MOD_TIME;
  }
  if (!any_enabled) {
    return current;
  }
  // Time Offset (MOD_gpencil_legacy_time.c): the layer shows the keyframe at the remapped frame
  // number; before the first keyframe nothing is shown (an empty evaluated frame).
  bool empty_source = false;
  if (time_offset) {
    const int f = pg_mod_time_frame(stack.data(), static_cast<int>(stack.size()), frame_number);
    const bGPDframe *found = nullptr;
    for (const bGPDframe *gpf = static_cast<const bGPDframe *>(layer->frames.first); gpf; gpf = gpf->next) {
      if (gpf->framenum <= f) found = gpf;
    }
    if (found) current = found;
    else empty_source = true;
  }
  EvalCacheEntry *target = nullptr;
  for (auto &entry : impl->eval_cache) {
    if (entry->layer == layer) {
      target = entry.get();
      break;
    }
  }
  if (target != nullptr && target->source == current && target->cfra == frame_number &&
      target->revision == g_gp_data_revision && target->stack_revision == impl->stack_revision)
  {
    return &target->frame;
  }
  if (target == nullptr) {
    impl->eval_cache.push_back(std::make_unique<EvalCacheEntry>());
    target = impl->eval_cache.back().get();
    target->layer = layer;
  }
  else {
    pg_mod_eval_free(&target->frame);
  }
  target->source = current;
  target->cfra = frame_number;
  target->revision = g_gp_data_revision;
  target->stack_revision = impl->stack_revision;
  if (empty_source) {
    std::memset(&target->frame, 0, sizeof(target->frame));
    target->source = nullptr;
  }
  else {
    pg_mod_eval_frame(impl->gpd, const_cast<bGPDlayer *>(layer), const_cast<bGPDframe *>(current),
                      stack.data(), static_cast<int>(stack.size()), frame_number, &target->frame);
  }
  impl->eval_count++;
  return &target->frame;
}

/* Presenter callback: the shader effect list of `layer` (see android_gp_presentation.cpp). Effects
 * never change stroke data. */
[[maybe_unused]] static int fx_provider_trampoline(void *user, const bGPDlayer *layer,
                                                    const PGFxEntry **entries)
{
  return static_cast<const Backend *>(user)->fx_for_layer(layer, entries);
}

static void history_snapshot_free(HistorySnapshot *snapshot)
{
  if (!snapshot) {
    return;
  }
  if (snapshot->data) {
    BKE_gpencil_free_layers(&snapshot->data->layers);
    BLI_freelistN(&snapshot->data->vertex_group_names);
    gp_materials_free(snapshot->data);
    MEM_freeN(snapshot->data);
  }
  delete snapshot;
}

static bGPdata *history_gp_duplicate(const bGPdata *source)
{
  if (!source) {
    return nullptr;
  }

  // This mirrors Blender 3.6.23's internal_copy path in
  // BKE_gpencil_data_duplicate(), without entering BKE_id_copy(). The Android
  // document is not an ID database, so the ID-copy branch is intentionally
  // outside this focused closure.
  bGPdata *destination = static_cast<bGPdata *>(
      MEM_dupallocN(source));
  if (!destination) {
    return nullptr;
  }

  // Runtime/cache state belongs to the live owner, never to the snapshot.
  std::memset(&destination->runtime, 0, sizeof(destination->runtime));

  // Vertex group names are a list of nodes: copy them, do not alias the source's nodes.
  BLI_listbase_clear(&destination->vertex_group_names);
  BKE_defgroup_copy_list(&destination->vertex_group_names, &source->vertex_group_names);

  destination->mat = gp_materials_duplicate(source->mat, source->totcol);
  if (!destination->mat) {
    destination->totcol = 0;
  }

  // Do not shallow-copy the source list nodes.
  BLI_listbase_clear(&destination->layers);
  LISTBASE_FOREACH (bGPDlayer *, source_layer, &source->layers) {
    bGPDlayer *destination_layer =
        BKE_gpencil_layer_duplicate(source_layer, true, true);
    if (!destination_layer) {
      BLI_freelistN(&destination->vertex_group_names);
      BKE_gpencil_free_layers(&destination->layers);
      gp_materials_free(destination);
      MEM_freeN(destination);
      return nullptr;
    }
    BLI_addtail(&destination->layers, destination_layer);
  }

  return destination;
}

static HistorySnapshot *history_snapshot_create(const bGPdata *source,
                                                const ModifierStacks &stacks,
                                                const FxStacks &fx_stacks)
{
  if (!source) {
    return nullptr;
  }

  HistorySnapshot *snapshot = new HistorySnapshot();
  snapshot->data = history_gp_duplicate(source);
  if (!snapshot->data) {
    delete snapshot;
    return nullptr;
  }

  snapshot->data->flag |= GP_DATA_CACHE_IS_DIRTY;
  snapshot->stacks = stacks;
  snapshot->fx_stacks = fx_stacks;
  return snapshot;
}

static void history_snapshot_clear(std::vector<HistorySnapshot *> &history)
{
  for (HistorySnapshot *snapshot : history) {
    history_snapshot_free(snapshot);
  }
  history.clear();
}

static void history_copy_settings(const bGPdata *source, bGPdata *destination)
{
  destination->flag = source->flag | GP_DATA_CACHE_IS_DIRTY;
  destination->pixfactor = source->pixfactor;
  std::memcpy(destination->line_color, source->line_color, sizeof(source->line_color));
  destination->onion_factor = source->onion_factor;
  destination->onion_mode = source->onion_mode;
  destination->onion_flag = source->onion_flag;
  destination->gstep = source->gstep;
  destination->gstep_next = source->gstep_next;
  std::memcpy(destination->gcolor_prev, source->gcolor_prev, sizeof(source->gcolor_prev));
  std::memcpy(destination->gcolor_next, source->gcolor_next, sizeof(source->gcolor_next));
  destination->zdepth_offset = source->zdepth_offset;
  destination->totcol = source->totcol;
  destination->draw_mode = source->draw_mode;
  destination->onion_keytype = source->onion_keytype;
  destination->select_last_index = source->select_last_index;
  destination->grid = source->grid;
}

static bool history_restore_snapshot(Backend::Impl *impl, const HistorySnapshot *snapshot)
{
  if (!impl || !impl->gpd || !snapshot || !snapshot->data) {
    return false;
  }

  if (impl->gpu_initialized) {
    DRW_gpencil_batch_cache_free(impl->gpd);
  }

  // Duplicate first so an allocation failure leaves the live document intact.
  bGPdata *restored = history_gp_duplicate(snapshot->data);
  if (!restored) {
    return false;
  }
  std::memset(&restored->runtime, 0, sizeof(restored->runtime));

  BKE_gpencil_free_layers(&impl->gpd->layers);
  gp_materials_free(impl->gpd);
  BLI_listbase_clear(&impl->gpd->layers);

  history_copy_settings(snapshot->data, impl->gpd);
  impl->gpd->totcol = restored->mat ? restored->totcol : 0;
  BLI_freelistN(&impl->gpd->vertex_group_names);
  impl->gpd->vertex_group_names = restored->vertex_group_names;
  BLI_listbase_clear(&restored->vertex_group_names);
  impl->gpd->vertex_group_active_index = snapshot->data->vertex_group_active_index;
  impl->gpd->layers = restored->layers;
  impl->gpd->mat = restored->mat;
  BLI_listbase_clear(&restored->layers);
  restored->mat = nullptr;
  MEM_freeN(restored);
  BKE_gpencil_stats_update(impl->gpd);

  impl->layer = nullptr;
  for (bGPDlayer *layer = static_cast<bGPDlayer *>(impl->gpd->layers.first);
       layer != nullptr;
       layer = layer->next) {
    if (layer->flag & GP_LAYER_ACTIVE) {
      impl->layer = layer;
      break;
    }
  }
  if (!impl->layer) {
    impl->layer = static_cast<bGPDlayer *>(impl->gpd->layers.first);
  }
  impl->frame = impl->layer ? impl->layer->actframe : nullptr;
  impl->stroke = nullptr;
  impl->layer_created = impl->layer != nullptr;
  impl->frame_created = impl->frame != nullptr;
  impl->stroke_open = false;
  impl->stacks = snapshot->stacks;
  impl->fx_stacks = snapshot->fx_stacks;
  impl->stack_revision++;
  eval_cache_clear(impl);
  project_grease_gp_tag(impl->gpd);
  return true;
}


Backend::Backend() : impl_(new Impl()) {}

Backend::~Backend()
{
  shutdown();
  delete impl_;
}

bool Backend::history_reset()
{
  if (!impl_->gpd) {
    impl_->last_error = "document is not created";
    return false;
  }
  history_snapshot_clear(impl_->undo_history);
  history_snapshot_clear(impl_->redo_history);
  return history_record();
}

bool Backend::history_record()
{
  if (!impl_->gpd) {
    impl_->last_error = "document is not created";
    return false;
  }

  HistorySnapshot *snapshot = history_snapshot_create(impl_->gpd, impl_->stacks, impl_->fx_stacks);
  if (!snapshot) {
    impl_->last_error = "Legacy GP history snapshot allocation failed";
    return false;
  }

  history_snapshot_clear(impl_->redo_history);
  impl_->undo_history.push_back(snapshot);

  constexpr size_t kMaxHistory = 64;
  if (impl_->undo_history.size() > kMaxHistory) {
    history_snapshot_free(impl_->undo_history.front());
    impl_->undo_history.erase(impl_->undo_history.begin());
  }

  impl_->last_error.clear();
  return true;
}

bool Backend::history_undo()
{
  if (impl_->undo_history.size() < 2) {
    return false;
  }

  HistorySnapshot *current = impl_->undo_history.back();
  HistorySnapshot *target = impl_->undo_history[impl_->undo_history.size() - 2];
  if (!history_restore_snapshot(impl_, target)) {
    impl_->last_error = "Legacy GP undo restore failed";
    return false;
  }

  impl_->undo_history.pop_back();
  impl_->redo_history.push_back(current);
  impl_->last_error.clear();
  return true;
}

bool Backend::history_redo()
{
  if (impl_->redo_history.empty()) {
    return false;
  }

  HistorySnapshot *target = impl_->redo_history.back();
  if (!history_restore_snapshot(impl_, target)) {
    impl_->last_error = "Legacy GP redo restore failed";
    return false;
  }

  impl_->redo_history.pop_back();
  impl_->undo_history.push_back(target);
  impl_->last_error.clear();
  return true;
}

bool Backend::history_can_undo() const
{
  return impl_->undo_history.size() >= 2;
}

bool Backend::history_can_redo() const
{
  return !impl_->redo_history.empty();
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
  BKE_gpencil_batch_cache_dirty_tag_cb = project_grease_gp_dirty_tag_callback;
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
  pg_annot_free(impl_->annotations);
  impl_->annotations = nullptr;

  eval_cache_clear(impl_);
  impl_->stacks.clear();
  impl_->fx_stacks.clear();

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
      BKE_gpencil_free_layer_masks(layer);
      MEM_freeN(layer);
      layer = next_layer;
    }
    gp_materials_free(impl_->gpd);
    BLI_freelistN(&impl_->gpd->vertex_group_names);
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
  history_snapshot_clear(impl_->undo_history);
  history_snapshot_clear(impl_->redo_history);

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

bool Backend::reset_document()
{
  if (!impl_->initialized) {
    impl_->last_error = "backend is not initialized";
    return false;
  }

  if (impl_->stroke_open) {
    cancel_stroke();
  }
  eval_cache_clear(impl_);
  impl_->stacks.clear();
  impl_->fx_stacks.clear();
  impl_->stack_revision++;
  pg_annot_free(impl_->annotations); /* a new document starts without annotations */
  impl_->annotations = nullptr;
  if (impl_->gpd) {
    if (impl_->gpu_initialized) {
      DRW_gpencil_batch_cache_free(impl_->gpd);
    }
#ifdef __ANDROID__
    for (bGPDlayer *layer = static_cast<bGPDlayer *>(impl_->gpd->layers.first);
         layer;) {
      bGPDlayer *next_layer = layer->next;
      for (bGPDframe *frame = static_cast<bGPDframe *>(layer->frames.first);
           frame;) {
        bGPDframe *next_frame = frame->next;
        for (bGPDstroke *stroke = static_cast<bGPDstroke *>(frame->strokes.first);
             stroke;) {
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
      BKE_gpencil_free_layer_masks(layer);
      MEM_freeN(layer);
      layer = next_layer;
    }
    gp_materials_free(impl_->gpd);
    BLI_freelistN(&impl_->gpd->vertex_group_names);
    MEM_freeN(impl_->gpd);
#else
    if (impl_->bmain) {
      /* The desktop proof path owns the GP ID through Main. */
      BKE_gpencil_data_free(impl_->gpd);
    }
#endif
    impl_->gpd = nullptr;
  }

  impl_->layer = nullptr;
  impl_->frame = nullptr;
  impl_->stroke = nullptr;
  impl_->document_created = false;
  impl_->layer_created = false;
  impl_->frame_created = false;
  impl_->stroke_open = false;
  impl_->pending_points.clear();
  history_snapshot_clear(impl_->undo_history);
  history_snapshot_clear(impl_->redo_history);

  if (!create_document() || !create_layer("Layer 1") || !create_frame(1)) {
    return false;
  }
  impl_->last_error.clear();
  return true;
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
  // Onion skinning starts off (the editor's onion switch); GP_DATA_SHOW_ONIONSKINS is the overlay
  // toggle, GP_LAYER_ONIONSKIN (on for new layers, BKE_gpencil_layer_addnew) the per-layer one.
  impl_->gpd->flag = GP_DATA_DISPINFO | GP_DATA_EXPAND | GP_DATA_VIEWALIGN |
                     GP_DATA_CURVE_ADAPTIVE_RESOLUTION;
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
  if (!impl_->annotations) {
    impl_->annotations = pg_annot_create();
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
  impl_->stacks.resize(static_cast<size_t>(layer_total(impl_->gpd))); /* the new layer's stack is empty */
  impl_->fx_stacks.resize(static_cast<size_t>(layer_total(impl_->gpd)));
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
      // The active layer is document state (GP_LAYER_ACTIVE): undo/redo restore it from the flag.
      BKE_gpencil_layer_active_set(impl_->gpd, layer);
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
  if (layer_stack(impl_, from_index) != nullptr) {
    std::vector<PGModEntry> moved = std::move(impl_->stacks[static_cast<size_t>(from_index)]);
    impl_->stacks.erase(impl_->stacks.begin() + from_index);
    impl_->stacks.insert(impl_->stacks.begin() + to_index, std::move(moved));
  }
  if (layer_fx(impl_, from_index) != nullptr) {
    std::vector<PGFxEntry> moved = std::move(impl_->fx_stacks[static_cast<size_t>(from_index)]);
    impl_->fx_stacks.erase(impl_->fx_stacks.begin() + from_index);
    impl_->fx_stacks.insert(impl_->fx_stacks.begin() + to_index, std::move(moved));
  }
  impl_->stack_revision++;
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
  const std::vector<PGModEntry> source_stack =
      layer_stack(impl_, index) ? impl_->stacks[static_cast<size_t>(index)] : std::vector<PGModEntry>();
  const std::vector<PGFxEntry> source_fx =
      layer_fx(impl_, index) ? impl_->fx_stacks[static_cast<size_t>(index)] : std::vector<PGFxEntry>();
  BLI_addtail(&impl_->gpd->layers, copy);
  impl_->stacks.resize(static_cast<size_t>(layer_total(impl_->gpd)) - 1);
  impl_->stacks.push_back(source_stack); /* a duplicated layer keeps its modifiers */
  impl_->fx_stacks.resize(static_cast<size_t>(layer_total(impl_->gpd)) - 1);
  impl_->fx_stacks.push_back(source_fx);  /* ... and its effects */
  impl_->stack_revision++;
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
  if (layer_stack(impl_, index) != nullptr) {
    impl_->stacks.erase(impl_->stacks.begin() + index);
  }
  if (layer_fx(impl_, index) != nullptr) {
    impl_->fx_stacks.erase(impl_->fx_stacks.begin() + index);
  }
  impl_->stack_revision++;
  BKE_gpencil_layer_delete(impl_->gpd, layer);
  impl_->layer = static_cast<bGPDlayer *>(impl_->gpd->layers.first);
  impl_->frame = impl_->layer ? impl_->layer->actframe : nullptr;
  impl_->layer_created = impl_->layer != nullptr;
  impl_->frame_created = impl_->frame != nullptr;
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::merge_layer_down()
{
  if (!impl_->gpd || !impl_->layer) { impl_->last_error = "no active layer"; return false; }
  int index = 0;
  for (bGPDlayer *l = static_cast<bGPDlayer *>(impl_->gpd->layers.first); l && l != impl_->layer; l = l->next) index++;
  if (index == 0) { impl_->last_error = "no layer below"; return false; }
  bGPDlayer *below = nullptr;
  if (!pg_gp_layer_merge_down(impl_->gpd, impl_->layer, &below) || below == nullptr) {
    impl_->last_error = "layer merge failed";
    return false;
  }
  /* the merged layer is gone: drop its modifier / effect lists (Blender merges the strokes only) */
  if (layer_stack(impl_, index) != nullptr) impl_->stacks.erase(impl_->stacks.begin() + index);
  if (layer_fx(impl_, index) != nullptr) impl_->fx_stacks.erase(impl_->fx_stacks.begin() + index);
  impl_->stack_revision++;
  eval_cache_clear(impl_);
  BKE_gpencil_layer_active_set(impl_->gpd, below);
  impl_->layer = below;
  impl_->frame = below->actframe;
  impl_->stroke = nullptr;
  impl_->layer_created = true;
  impl_->frame_created = impl_->frame != nullptr;
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::rename_layer(int index, const char* name)
{
  bGPDlayer *layer = layer_at(impl_->gpd, index);
  if (!layer || !name || !name[0]) { impl_->last_error = "invalid layer rename"; return false; }
  char old_name[sizeof(layer->info)];
  std::memcpy(old_name, layer->info, sizeof(old_name));
  std::strncpy(layer->info, name, sizeof(layer->info) - 1);
  layer->info[sizeof(layer->info) - 1] = '\0';
  /* Masks refer to layers by name: follow the rename. */
  LISTBASE_FOREACH (bGPDlayer *, other, &impl_->gpd->layers) {
    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &other->mask_layers) {
      if (std::strncmp(mask->name, old_name, sizeof(mask->name)) == 0) {
        std::strncpy(mask->name, layer->info, sizeof(mask->name) - 1);
        mask->name[sizeof(mask->name) - 1] = '\0';
      }
    }
  }
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

int Backend::frame_numbers(int *out_frames, int capacity) const
{
  if (!impl_->layer || !out_frames || capacity <= 0) return 0;
  int count = 0;
  for (bGPDframe *frame = static_cast<bGPDframe *>(impl_->layer->frames.first);
       frame != nullptr && count < capacity;
       frame = frame->next) {
    out_frames[count++] = frame->framenum;
  }
  return count;
}

bool Backend::interpolate_frame(int source_frame, int target_frame, int result_frame, float factor,
                                int easing_type, int easing_mode)
{
  if (!impl_->layer || source_frame < 1 || target_frame < 1 || result_frame < 1 ||
      result_frame == source_frame || result_frame == target_frame) {
    impl_->last_error = "invalid interpolation frame parameters";
    return false;
  }
  bGPDframe *source = BKE_gpencil_layer_frame_find(impl_->layer, source_frame);
  bGPDframe *target = BKE_gpencil_layer_frame_find(impl_->layer, target_frame);
  if (!source || !target) {
    impl_->last_error = "interpolation source/target frame not found";
    return false;
  }
  if (BKE_gpencil_layer_frame_find(impl_->layer, result_frame)) {
    impl_->last_error = "interpolation result frame already exists";
    return false;
  }
  if (factor < 0.0f || factor > 1.0f) {
    impl_->last_error = "interpolation factor must be in range 0..1";
    return false;
  }

  /* Blender's default "back" overshoot (BLI_easing_back_ease_*: 1.70158). */
  factor = pg_gp_interpolate_easing(easing_type, easing_mode, factor, 1.70158f);

  bGPDframe *result = BKE_gpencil_frame_duplicate(source, true);
  if (!result) {
    impl_->last_error = "BKE_gpencil_frame_duplicate() failed for interpolation";
    return false;
  }
  result->framenum = result_frame;

  /* Strokes are paired by index (gpencil_interpolate.c pairs gps_from/gps_to the same way); a pair
   * with different point counts is resampled to the larger count (pg_gp_interpolate_strokes, which
   * uses BKE_gpencil_stroke_uniform_subdivide) instead of failing the whole frame. Strokes without a
   * partner are not interpolated (dropped from the in-between, as Blender skips them). */
  bGPDstroke *rs = static_cast<bGPDstroke *>(result->strokes.first);
  bGPDstroke *ts = static_cast<bGPDstroke *>(target->strokes.first);
  for (; rs && ts; ts = ts->next) {
    if (rs->totpoints != ts->totpoints) {
      bGPDstroke *mixed = pg_gp_interpolate_strokes(impl_->gpd, rs, ts, factor);
      if (!mixed) {
        BKE_gpencil_free_strokes(result);
        MEM_freeN(result);
        impl_->last_error = "interpolation of strokes with different point counts failed";
        return false;
      }
      BLI_insertlinkafter(&result->strokes, rs, mixed);
      BLI_remlink(&result->strokes, rs);
      BKE_gpencil_free_stroke(rs);
      rs = mixed->next;
      continue;
    }
    for (int i = 0; i < rs->totpoints; ++i) {
      const bGPDspoint &a = rs->points[i];
      const bGPDspoint &b = ts->points[i];
      bGPDspoint &p = rs->points[i];
      p.x = a.x + (b.x - a.x) * factor;
      p.y = a.y + (b.y - a.y) * factor;
      p.z = a.z + (b.z - a.z) * factor;
      p.pressure = a.pressure + (b.pressure - a.pressure) * factor;
      p.strength = a.strength + (b.strength - a.strength) * factor;
      p.time = a.time + (b.time - a.time) * factor;
      p.uv_fac = a.uv_fac + (b.uv_fac - a.uv_fac) * factor;
      p.uv_rot = a.uv_rot + (b.uv_rot - a.uv_rot) * factor;
    }
    rs = rs->next;
  }
  while (rs) { /* source strokes without a partner in the target frame */
    bGPDstroke *next = rs->next;
    BLI_remlink(&result->strokes, rs);
    BKE_gpencil_free_stroke(rs);
    rs = next;
  }
  BLI_addtail(&impl_->layer->frames, result);
  BKE_gpencil_layer_frames_sort(impl_->layer, nullptr);
  impl_->layer->actframe = result;
  impl_->frame = result;
  impl_->frame_created = true;
  impl_->stroke = nullptr;
  project_grease_gp_tag(impl_->gpd);
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
    if (impl_->layer) {
      for (bGPDframe *frame = static_cast<bGPDframe *>(impl_->layer->frames.first); frame; frame = frame->next) {
        frame->flag |= GP_FRAME_SELECT;
      }
    }
  }
  else {
    impl_->gpd->flag &= ~GP_DATA_STROKE_MULTIEDIT;
    if (impl_->layer) {
      for (bGPDframe *frame = static_cast<bGPDframe *>(impl_->layer->frames.first); frame; frame = frame->next) {
        frame->flag &= ~GP_FRAME_SELECT;
      }
    }
  }
  project_grease_gp_tag(impl_->gpd);
  return true;
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
  // BKE_gpencil_layer_frame_delete() returns whether strokes were freed, not whether the frame
  // went away: an empty keyframe is deleted too, so its result is not a failure.
  BKE_gpencil_layer_frame_delete(impl_->layer, target);
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

int Backend::frame_selected_point_count() const {
  if (!impl_->frame) {
    return 0;
  }
  int count = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first); stroke != nullptr;
       stroke = stroke->next) {
    for (int i = 0; i < stroke->totpoints; i++) {
      if (stroke->points[i].flag & GP_SPOINT_SELECT) count++;
    }
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
    // Zero vert_color (alpha 0) means "no vertex color": the renderer mixes it with the
    // material color by alpha, as Blender does, so the material color stays authoritative.
    dst.vert_color[0] = 0.0f;
    dst.vert_color[1] = 0.0f;
    dst.vert_color[2] = 0.0f;
    dst.vert_color[3] = 0.0f;
  }

  if (cyclic) {
    stroke->flag |= GP_STROKE_CYCLIC;
    BKE_gpencil_stroke_fill_triangulate(stroke);
  }

  // New primitive strokes behave like Blender Edit Mode stroke selection:
  // select the whole generated stroke so it can immediately be transformed.
  clear_selection();
  stroke->flag |= GP_STROKE_SELECT;
  for (int i = 0; i < stroke->totpoints; ++i) {
    stroke->points[i].flag |= GP_SPOINT_SELECT;
  }
  impl_->stroke = stroke;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::erase_at(float x, float y, float radius)
{
  if (!impl_->frame || radius <= 0.0f ||
      !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius)) {
    impl_->last_error = "invalid eraser";
    return false;
  }

  legacy_gp_eraser::Settings settings{};
  settings.draw_strength = 1.0f;
  settings.pointer_pressure = 1.0f;
  settings.soft = false;
  settings.stroke_eraser = false;

  bool changed = false;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;) {
    bGPDstroke *next = stroke->next;
    changed |= legacy_gp_eraser::process_stroke(
        impl_->gpd, impl_->frame, stroke, x, y, static_cast<int>(radius), settings);
    stroke = next;
  }

  if (!changed) {
    impl_->last_error = "eraser did not hit a stroke";
    return false;
  }

  impl_->stroke = nullptr;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::soft_erase_at(float x, float y, float radius, float strength)
{
  if (!impl_->frame || radius <= 0.0f || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(radius) || !std::isfinite(strength)) {
    impl_->last_error = "invalid soft eraser";
    return false;
  }

  legacy_gp_eraser::Settings settings{};
  settings.draw_strength = 1.0f;
  settings.pointer_pressure = 1.0f;
  settings.soft = true;
  settings.soft_strength = std::clamp(strength, 0.0f, 1.0f);
  settings.soft_thickness = std::clamp(strength, 0.0f, 1.0f);
  settings.stroke_eraser = false;

  bool changed = false;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;) {
    bGPDstroke *next = stroke->next;
    changed |= legacy_gp_eraser::process_stroke(
        impl_->gpd, impl_->frame, stroke, x, y, static_cast<int>(radius), settings);
    stroke = next;
  }

  if (!changed) {
    impl_->last_error = "soft eraser did not hit a stroke";
    return false;
  }

  impl_->stroke = nullptr;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
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



bool Backend::select_all(int mode)
{
  if (!impl_->frame || mode < 0 || mode > 3) {
    impl_->last_error = "invalid selection mode";
    return false;
  }

  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    const bool select = (mode == 0 || mode == 1);
    if (mode == 2) {
      stroke->flag ^= GP_STROKE_SELECT;
    }
    else if (mode == 3) {
      stroke->flag &= ~GP_STROKE_SELECT;
    }
    else if (select) {
      stroke->flag |= GP_STROKE_SELECT;
    }

    for (int i = 0; i < stroke->totpoints; ++i) {
      if (mode == 2) {
        stroke->points[i].flag ^= GP_SPOINT_SELECT;
      }
      else if (mode == 3) {
        stroke->points[i].flag &= ~GP_SPOINT_SELECT;
      }
      else {
        stroke->points[i].flag |= GP_SPOINT_SELECT;
      }
    }
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

int Backend::select_circle(float x, float y, float radius, int mode)
{
  if (!impl_->frame || radius <= 0.0f || mode < 0 || mode > 2) {
    impl_->last_error = "invalid circle selection";
    return 0;
  }

  if (mode == 0) {
    clear_selection();
  }

  const float radius_sq = radius * radius;
  int selected = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    bool hit = false;
    for (int i = 0; i < stroke->totpoints; ++i) {
      const float dx = stroke->points[i].x - x;
      const float dy = stroke->points[i].y - y;
      if ((dx * dx + dy * dy) <= radius_sq) {
        hit = true;
        if (mode == 2) {
          stroke->points[i].flag &= ~GP_SPOINT_SELECT;
        }
        else {
          stroke->points[i].flag |= GP_SPOINT_SELECT;
        }
      }
    }

    bool any_point_selected = false;
    for (int i = 0; i < stroke->totpoints; ++i) {
      if (stroke->points[i].flag & GP_SPOINT_SELECT) {
        any_point_selected = true;
        break;
      }
    }

    if (hit) {
      if (mode == 2) {
        if (!any_point_selected) {
          stroke->flag &= ~GP_STROKE_SELECT;
        }
      }
      else {
        stroke->flag |= GP_STROKE_SELECT;
      }
      ++selected;
    }
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return selected;
}

bool Backend::reverse_selected_strokes()
{
  if (!impl_->frame) {
    impl_->last_error = "no active frame";
    return false;
  }

  bool changed = false;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    if (!(stroke->flag & GP_STROKE_SELECT) || stroke->totpoints < 2 || !stroke->points) {
      continue;
    }

    for (int left = 0, right = stroke->totpoints - 1; left < right; ++left, --right) {
      char temporary[sizeof(bGPDspoint)];
      std::memcpy(temporary, &stroke->points[left], sizeof(bGPDspoint));
      std::memcpy(&stroke->points[left], &stroke->points[right], sizeof(bGPDspoint));
      std::memcpy(&stroke->points[right], temporary, sizeof(bGPDspoint));
    }
    changed = true;
  }

  if (!changed) {
    impl_->last_error = "no selected stroke to reverse";
    return false;
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::dissolve_selected_points()
{
  if (!impl_->frame || !impl_->gpd) {
    impl_->last_error = "no active frame";
    return false;
  }
  // GPENCIL_OT_dissolve(type=POINTS): the port in project_grease_blender_edit.c (weights kept
  // aligned, emptied strokes freed, selection cleared as in Blender).
  if (!pg_gp_dissolve(impl_->gpd, impl_->layer, PG_DISSOLVE_POINTS)) {
    impl_->last_error = "no selected strokes to dissolve";
    return false;
  }
  impl_->stroke = nullptr;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::merge_selected_points(float threshold)
{
  if (!impl_->frame || !impl_->gpd || !std::isfinite(threshold) || threshold <= 0.0f) {
    impl_->last_error = "invalid merge threshold";
    return false;
  }

  bool changed = false;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    if (!(stroke->flag & GP_STROKE_SELECT) || stroke->totpoints < 2) {
      continue;
    }
    // Match the Legacy GP merge-distance rule used by the 3.6 API:
    // keep the first and last point and collapse selected interior points
    // whose distance from the previous kept point is below the threshold.
    const float threshold_sq = threshold * threshold;
    int write_index = 0;
    for (int read_index = 0; read_index < stroke->totpoints; ++read_index) {
      const bGPDspoint &point = stroke->points[read_index];
      const bool keep_endpoint =
          (read_index == 0 || read_index == stroke->totpoints - 1);
      bool merge = false;
      if (!keep_endpoint && write_index > 0) {
        const bGPDspoint &previous = stroke->points[write_index - 1];
        const float dx = point.x - previous.x;
        const float dy = point.y - previous.y;
        const float dz = point.z - previous.z;
        merge = (dx * dx + dy * dy + dz * dz) < threshold_sq &&
                (point.flag & GP_SPOINT_SELECT);
      }
      if (!merge) {
        if (write_index != read_index) {
          std::memcpy(&stroke->points[write_index], &point, sizeof(bGPDspoint));
        }
        ++write_index;
      }
    }
    if (write_index != stroke->totpoints) {
      bGPDspoint *points = static_cast<bGPDspoint *>(
          MEM_mallocN(sizeof(bGPDspoint) * static_cast<size_t>(write_index),
                       "Project Grease merge points"));
      if (!points) {
        impl_->last_error = "merge point allocation failed";
        return false;
      }
      std::memcpy(points,
                  stroke->points,
                  sizeof(bGPDspoint) * static_cast<size_t>(write_index));
      MEM_freeN(stroke->points);
      stroke->points = points;
      stroke->totpoints = write_index;
      MEM_SAFE_FREE(stroke->triangles);
      stroke->tot_triangles = 0;
      changed = true;
    }
  }

  if (!changed) {
    impl_->last_error = "no selected stroke to merge";
    return false;
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::join_selected_strokes()
{
  if (!impl_->frame || !impl_->gpd) {
    impl_->last_error = "no active frame";
    return false;
  }

  std::vector<bGPDstroke *> selected;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    if ((stroke->flag & GP_STROKE_SELECT) && stroke->totpoints > 0 && stroke->points) {
      selected.push_back(stroke);
    }
  }
  if (selected.size() < 2) {
    impl_->last_error = "at least two selected strokes are required";
    return false;
  }

  bGPDstroke *destination = selected.front();
  for (size_t i = 1; i < selected.size(); ++i) {
    bGPDstroke *source = selected[i];
    const int destination_points = destination->totpoints;
    const int source_points = source->totpoints;
    bGPDspoint *joined_points = static_cast<bGPDspoint *>(
        MEM_mallocN(sizeof(bGPDspoint) *
                        static_cast<size_t>(destination_points + source_points),
                    "Project Grease joined stroke points"));
    if (!joined_points) {
      impl_->last_error = "joined stroke point allocation failed";
      return false;
    }

    std::memcpy(joined_points,
                destination->points,
                sizeof(bGPDspoint) * static_cast<size_t>(destination_points));
    std::memcpy(joined_points + destination_points,
                source->points,
                sizeof(bGPDspoint) * static_cast<size_t>(source_points));

    MEM_freeN(destination->points);
    destination->points = joined_points;
    destination->totpoints = destination_points + source_points;
    destination->flag &= ~GP_STROKE_CYCLIC;
    MEM_SAFE_FREE(destination->triangles);
    destination->tot_triangles = 0;
    MEM_SAFE_FREE(destination->dvert);
    MEM_SAFE_FREE(destination->editcurve);

    BLI_remlink(&impl_->frame->strokes, source);
    BKE_gpencil_free_stroke(source);
  }

  destination->flag |= GP_STROKE_SELECT;
  for (int i = 0; i < destination->totpoints; ++i) {
    destination->points[i].flag |= GP_SPOINT_SELECT;
  }
  impl_->stroke = destination;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::select_first_points(bool only_selected_strokes, bool extend)
{
  if (!impl_->frame || !impl_->gpd) {
    impl_->last_error = "no active frame";
    return false;
  }

  std::vector<bGPDstroke *> eligible;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    if (!stroke->points || stroke->totpoints <= 0) continue;
    bool has_selected_point = false;
    for (int i = 0; i < stroke->totpoints; ++i) {
      if (stroke->points[i].flag & GP_SPOINT_SELECT) {
        has_selected_point = true;
        break;
      }
    }
    if (!only_selected_strokes ||
        (stroke->flag & GP_STROKE_SELECT) || has_selected_point) {
      eligible.push_back(stroke);
    }
  }

  if (!extend) clear_selection();

  bool changed = false;
  for (bGPDstroke *stroke : eligible) {
    stroke->points[0].flag |= GP_SPOINT_SELECT;
    stroke->flag |= GP_STROKE_SELECT;
    changed = true;
  }
  if (!changed) {
    impl_->last_error = "no eligible stroke for first-point selection";
    return false;
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::select_grouped(int type)
{
  if (!impl_->frame || !impl_->gpd || (type != 0 && type != 1)) {
    impl_->last_error = "invalid grouped selection";
    return false;
  }

  int target_material = -1;
  if (type == 1) {
    for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
         stroke != nullptr;
         stroke = stroke->next) {
      if (stroke->flag & GP_STROKE_SELECT) {
        target_material = stroke->mat_nr;
        break;
      }
    }
    if (target_material < 0) {
      impl_->last_error = "select a stroke before material grouping";
      return false;
    }
  }

  bool changed = false;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next) {
    if (type == 1 && stroke->mat_nr != target_material) continue;
    stroke->flag |= GP_STROKE_SELECT;
    for (int i = 0; i < stroke->totpoints; ++i) {
      stroke->points[i].flag |= GP_SPOINT_SELECT;
    }
    changed = true;
  }
  if (!changed) {
    impl_->last_error = "no matching strokes for grouped selection";
    return false;
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::reorder_selected_strokes(int direction)
{
  if (!impl_->frame || direction < 0 || direction > 3) {
    impl_->last_error = "invalid stroke reorder direction";
    return false;
  }

  bool changed = false;
  if (direction == 0 || direction == 3) {
    std::vector<bGPDstroke *> selected;
    for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
         stroke != nullptr;
         stroke = stroke->next) {
      if (stroke->flag & GP_STROKE_SELECT) {
        selected.push_back(stroke);
      }
    }
    for (bGPDstroke *stroke : selected) {
      BLI_remlink(&impl_->frame->strokes, stroke);
      if (direction == 0) {
        BLI_addhead(&impl_->frame->strokes, stroke);
      }
      else {
        BLI_addtail(&impl_->frame->strokes, stroke);
      }
      changed = true;
    }
  }
  else {
    for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
         stroke != nullptr;) {
      bGPDstroke *next = stroke->next;
      if (!(stroke->flag & GP_STROKE_SELECT)) {
        stroke = next;
        continue;
      }

      if (direction == 1) {
        bGPDstroke *prev = stroke->prev;
        if (prev && !(prev->flag & GP_STROKE_SELECT)) {
          BLI_remlink(&impl_->frame->strokes, stroke);
          stroke->prev = prev->prev;
          stroke->next = prev;
          if (prev->prev) prev->prev->next = stroke;
          else impl_->frame->strokes.first = stroke;
          prev->prev = stroke;
          changed = true;
        }
      }
      else {
        bGPDstroke *next_unselected = next;
        while (next_unselected && (next_unselected->flag & GP_STROKE_SELECT)) {
          next_unselected = next_unselected->next;
        }
        if (next_unselected) {
          bGPDstroke *after = next_unselected->next;
          BLI_remlink(&impl_->frame->strokes, stroke);
          stroke->prev = next_unselected;
          stroke->next = after;
          next_unselected->next = stroke;
          if (after) after->prev = stroke;
          else impl_->frame->strokes.last = stroke;
          changed = true;
        }
      }
      stroke = next;
    }
  }

  if (!changed) {
    impl_->last_error = "no selected stroke to reorder";
    return false;
  }

  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
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
    out->r = src.vert_color[0];
    out->g = src.vert_color[1];
    out->b = src.vert_color[2];
    out->a = src.vert_color[3];
    out->uv_fac = src.uv_fac;
    out->uv_rot = src.uv_rot;
    return true;
  }
  return false;
}

bool Backend::get_point_group_weight(int index, int point_index, int group_index, float *out) const
{
  if (!out || !impl_->frame || index < 0 || point_index < 0 || group_index < 0) {
    return false;
  }

  bGPDstroke *stroke = nullptr;
  int current = 0;
  for (bGPDstroke *candidate = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       candidate; candidate = candidate->next, ++current) {
    if (current == index) {
      stroke = candidate;
      break;
    }
  }
  if (!stroke || point_index >= stroke->totpoints || !stroke->dvert) {
    return false;
  }

  const MDeformVert &dv = stroke->dvert[point_index];
  for (int i = 0; i < dv.totweight; ++i) {
    if (dv.dw[i].def_nr == group_index) {
      *out = dv.dw[i].weight;
      return true;
    }
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
    // Sculpt point writes intentionally preserve Blender-owned vertex-color
    // and UV state. Kotlin only owns the sculpt attributes it can read/write
    // through the focused Android contract: position, pressure, strength, time.
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
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
  // Reserved index -1: median of every selected point (pivot of selection-wide edits).
  if (index == -1 && x && y && impl_->gpd) {
    return pg_gp_edit_selection_pivot(impl_->gpd, impl_->layer, x, y) != 0;
  }
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
  if (impl_->gpd && (impl_->gpd->flag & GP_DATA_STROKE_MULTIEDIT) && impl_->layer) {
    bool changed = false;
    for (bGPDframe *edit_frame = static_cast<bGPDframe *>(impl_->layer->frames.first);
         edit_frame; edit_frame = edit_frame->next) {
      if (!(edit_frame->flag & GP_FRAME_SELECT)) continue;
      int edit_index = 0;
      for (bGPDstroke *edit_stroke = static_cast<bGPDstroke *>(edit_frame->strokes.first);
           edit_stroke; edit_stroke = edit_stroke->next, ++edit_index) {
        if (edit_index != index || edit_stroke->totpoints <= 0) continue;
        changed = true;
        for (int point_index=0; point_index<edit_stroke->totpoints; ++point_index) {
          bGPDspoint &point=edit_stroke->points[point_index];
          if (point.flag & GP_SPOINT_SELECT) { point.x+=dx; point.y+=dy; point.z+=dz; }
        }
      }
    }
    if (!changed) { impl_->last_error = "no selected stroke in multiframe edit set"; return false; }
    impl_->stroke = impl_->frame ? static_cast<bGPDstroke *>(impl_->frame->strokes.first) : nullptr;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }

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
  if (impl_->gpd && (impl_->gpd->flag & GP_DATA_STROKE_MULTIEDIT) && impl_->layer) {
    bool changed = false;
    for (bGPDframe *edit_frame = static_cast<bGPDframe *>(impl_->layer->frames.first);
         edit_frame; edit_frame = edit_frame->next) {
      if (!(edit_frame->flag & GP_FRAME_SELECT)) continue;
      int edit_index = 0;
      for (bGPDstroke *edit_stroke = static_cast<bGPDstroke *>(edit_frame->strokes.first);
           edit_stroke; edit_stroke = edit_stroke->next, ++edit_index) {
        if (edit_index != index || selected_point_count(edit_stroke) == 0) continue;
        changed = true;
        const float c=std::cos(radians), s=std::sin(radians);
        for (int point_index=0; point_index<edit_stroke->totpoints; ++point_index) {
          bGPDspoint &p=edit_stroke->points[point_index];
          if (!(p.flag & GP_SPOINT_SELECT)) continue;
          const float x=p.x-center_x, y=p.y-center_y;
          p.x=center_x+x*c-y*s; p.y=center_y+x*s+y*c;
        }
      }
    }
    if (!changed) { impl_->last_error = "no selected stroke in multiframe edit set"; return false; }
    impl_->stroke = impl_->frame ? static_cast<bGPDstroke *>(impl_->frame->strokes.first) : nullptr;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }

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
  if (impl_->gpd && (impl_->gpd->flag & GP_DATA_STROKE_MULTIEDIT) && impl_->layer) {
    bool changed = false;
    for (bGPDframe *edit_frame = static_cast<bGPDframe *>(impl_->layer->frames.first);
         edit_frame; edit_frame = edit_frame->next) {
      if (!(edit_frame->flag & GP_FRAME_SELECT)) continue;
      int edit_index = 0;
      for (bGPDstroke *edit_stroke = static_cast<bGPDstroke *>(edit_frame->strokes.first);
           edit_stroke; edit_stroke = edit_stroke->next, ++edit_index) {
        if (edit_index != index || selected_point_count(edit_stroke) == 0) continue;
        changed = true;
        for (int point_index=0; point_index<edit_stroke->totpoints; ++point_index) {
          bGPDspoint &p=edit_stroke->points[point_index];
          if (!(p.flag & GP_SPOINT_SELECT)) continue;
          p.x=center_x+(p.x-center_x)*scale_x; p.y=center_y+(p.y-center_y)*scale_y;
        }
      }
    }
    if (!changed) { impl_->last_error = "no selected stroke in multiframe edit set"; return false; }
    impl_->stroke = impl_->frame ? static_cast<bGPDstroke *>(impl_->frame->strokes.first) : nullptr;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }

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
  if (impl_->gpd && (impl_->gpd->flag & GP_DATA_STROKE_MULTIEDIT) && impl_->layer) {
    bool changed = false;
    for (bGPDframe *edit_frame = static_cast<bGPDframe *>(impl_->layer->frames.first);
         edit_frame; edit_frame = edit_frame->next) {
      if (!(edit_frame->flag & GP_FRAME_SELECT)) continue;
      int edit_index = 0;
      for (bGPDstroke *edit_stroke = static_cast<bGPDstroke *>(edit_frame->strokes.first);
           edit_stroke; edit_stroke = edit_stroke->next, ++edit_index) {
        if (edit_index != index || selected_point_count(edit_stroke) == 0) continue;
        changed = true;
        for (int point_index=0; point_index<edit_stroke->totpoints; ++point_index) {
          bGPDspoint &p=edit_stroke->points[point_index];
          if (!(p.flag & GP_SPOINT_SELECT)) continue;
          if (mirror_x) p.x=2.0f*center_x-p.x;
          if (mirror_y) p.y=2.0f*center_y-p.y;
        }
      }
    }
    if (!changed) { impl_->last_error = "no selected stroke in multiframe edit set"; return false; }
    impl_->stroke = impl_->frame ? static_cast<bGPDstroke *>(impl_->frame->strokes.first) : nullptr;
    BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
    project_grease_gp_tag(impl_->gpd);
    impl_->last_error.clear();
    return true;
  }

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
    if (!BKE_gpencil_stroke_trim(impl_->gpd, stroke)) {
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


bool Backend::set_legacy_paint_settings(const LegacyPaintSettings &settings)
{
  if (settings.draw_smooth_level < 0 ||
      settings.draw_smooth_factor < 0.0f ||
      settings.draw_smooth_factor > 1.0f ||
      settings.input_samples < 0) {
    impl_->last_error = "invalid Legacy GP paint settings";
    return false;
  }

  impl_->paint_settings = settings;
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

  /*
   * Enter Blender 3.6.23's actual editor stroke-cache representation.
   * tGPspoint/sbuffer is the transient point format used by the upstream
   * Legacy GP paint operator. Android remains only the input host; it does
   * not introduce a second stroke-buffer format.
   */
  impl_->gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      static_cast<tGPspoint *>(impl_->gpd->runtime.sbuffer),
      &impl_->gpd->runtime.sbuffer_size,
      &impl_->gpd->runtime.sbuffer_used,
      true);
  if (!impl_->gpd->runtime.sbuffer) {
    impl_->last_error = "Blender Legacy GP stroke buffer allocation failed";
    return false;
  }

  impl_->gpd->runtime.sbuffer_sflag = 0;
  impl_->stroke_style = style;
  impl_->pending_points.clear();
  impl_->stroke = nullptr;
  impl_->stroke_open = true;
  return true;
}

bool Backend::add_point(const StrokePoint &point) {
  if (!impl_->stroke_open) {
    impl_->last_error = "stroke is not open";
    return false;
  }

  impl_->gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      static_cast<tGPspoint *>(impl_->gpd->runtime.sbuffer),
      &impl_->gpd->runtime.sbuffer_size,
      &impl_->gpd->runtime.sbuffer_used,
      false);
  if (!impl_->gpd->runtime.sbuffer) {
    impl_->last_error = "Blender Legacy GP stroke buffer allocation failed";
    return false;
  }

  tGPspoint *buffer = static_cast<tGPspoint *>(impl_->gpd->runtime.sbuffer);
  tGPspoint &dst = buffer[impl_->gpd->runtime.sbuffer_used++];
  dst.m_xy[0] = point.x;
  dst.m_xy[1] = point.y;
  dst.pressure = point.pressure;
  dst.strength = point.strength;
  dst.time = point.time;
  dst.uv_fac = 0.0f;
  dst.uv_rot = 0.0f;
  dst.rnd[0] = dst.rnd[1] = dst.rnd[2] = 0.0f;
  dst.rnd_dirty = false;
  // Zero alpha = no vertex color (Blender's default); see create_polyline().
  dst.vert_color[0] = 0.0f;
  dst.vert_color[1] = 0.0f;
  dst.vert_color[2] = 0.0f;
  dst.vert_color[3] = 0.0f;

  // Kept only for the existing Android preview adapter. The authoritative
  // input representation is now Blender's sbuffer above.
  impl_->pending_points.push_back(point);
  return true;
}

int Backend::stroke_buffer_count() const
{
  if (!impl_->gpd) {
    return 0;
  }
  return impl_->gpd->runtime.sbuffer_used;
}

bool Backend::end_stroke() {
  if (!impl_->stroke_open) {
    impl_->last_error = "stroke is not open";
    return false;
  }
  if (impl_->gpd->runtime.sbuffer_used <= 0 ||
      impl_->gpd->runtime.sbuffer == nullptr) {
    impl_->last_error = "stroke has no points";
    impl_->stroke_open = false;
    return false;
  }

  const int point_count = impl_->gpd->runtime.sbuffer_used;
  const int material_index = impl_->stroke_style.material_index;
  const short thickness = static_cast<short>(
      impl_->stroke_style.thickness < 1.0f ? 1.0f : impl_->stroke_style.thickness);

  /*
   * Commit the real Blender Legacy GP paint buffer into the real
   * bGPDstroke data structure. The temporary sbuffer is not retained as the
   * document; Blender's frame stroke remains the authoritative drawing.
   */
  impl_->stroke = BKE_gpencil_stroke_add(
      impl_->frame, material_index, point_count, thickness, false);
  if (!impl_->stroke) {
    impl_->last_error = "BKE_gpencil_stroke_add() failed";
    impl_->stroke_open = false;
    return false;
  }

  const tGPspoint *buffer = static_cast<const tGPspoint *>(impl_->gpd->runtime.sbuffer);
  for (int i = 0; i < point_count; ++i) {
    const tGPspoint &src = buffer[i];
    bGPDspoint &dst = impl_->stroke->points[i];
    dst.x = src.m_xy[0];
    dst.y = src.m_xy[1];
    dst.z = 0.0f;
    dst.pressure = src.pressure;
    dst.strength = src.strength;
    dst.time = src.time;
    dst.uv_fac = src.uv_fac;
    dst.uv_rot = src.uv_rot;
    std::memcpy(dst.vert_color, src.vert_color, sizeof(dst.vert_color));
  }

  /*
   * Blender 3.6.23's Legacy paint commit performs brush-driven smoothing on
   * the freshly created stroke before the editor finishes the stroke. Keep
   * that stage Blender-backed: every geometry operation below is a real BKE
   * Legacy GP callback.
   */
  /*
   * Match Blender 3.6.23 gpencil_stroke_newfrombuffer() paint post-process:
   * one stroke-wide smooth callback using draw_smoothfac/draw_smoothlvl,
   * followed by per-point input-sample smoothing when depth projection is
   * active. Project Grease does not invent a second smoothing algorithm.
   */
  const int smooth_level = std::max(0, impl_->paint_settings.draw_smooth_level);
  if (smooth_level > 0 &&
      (impl_->paint_settings.smooth_position || impl_->paint_settings.smooth_strength)) {
    BKE_gpencil_stroke_smooth(impl_->stroke,
                              std::max(0.0f, impl_->paint_settings.draw_smooth_factor),
                              smooth_level,
                              impl_->paint_settings.smooth_position,
                              impl_->paint_settings.smooth_strength,
                              false,
                              false,
                              true,
                              nullptr);
  }

  if (impl_->paint_settings.input_samples > 0) {
    const float ifac = static_cast<float>(impl_->paint_settings.input_samples) / 10.0f;
    const float sfac = 1.0f + (0.2f - 1.0f) * std::min(ifac, 1.0f);
    for (int i = 0; i < impl_->stroke->totpoints; ++i) {
      if (impl_->paint_settings.smooth_position) {
        BKE_gpencil_stroke_smooth_point(
            impl_->stroke, i, sfac, 2, false, true, impl_->stroke);
      }
      if (impl_->paint_settings.smooth_strength) {
        BKE_gpencil_stroke_smooth_strength(impl_->stroke, i, sfac, 2, impl_->stroke);
      }
    }
  }

  BKE_gpencil_stroke_geometry_update(impl_->gpd, impl_->stroke);

  impl_->stroke_open = false;
  impl_->pending_points.clear();

  // Clear Blender's temporary paint buffer through the same public editor
  // utility used by the Legacy GP paint session.
  impl_->gpd->runtime.sbuffer = ED_gpencil_sbuffer_ensure(
      static_cast<tGPspoint *>(impl_->gpd->runtime.sbuffer),
      &impl_->gpd->runtime.sbuffer_size,
      &impl_->gpd->runtime.sbuffer_used,
      true);

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

#ifdef __ANDROID__
  // While a stroke is open (sbuffer in use) nothing committed changes, and the Android presenter
  // never draws from this batch: rebuilding and freeing Blender's whole-frame batch cache on every
  // input sample made drawing over a full frame cost ~150 ms per sample on the emulator.
  const bool stroke_open = impl_->gpd->runtime.sbuffer && impl_->gpd->runtime.sbuffer_used > 0;
  GPUBatch *batch = stroke_open ? nullptr : DRW_cache_gpencil_get(ob, impl_->frame->framenum);
#else
  GPUBatch *batch = DRW_cache_gpencil_get(ob, impl_->frame->framenum);
#endif
  const bool cache_ready = batch != nullptr;
#ifdef __ANDROID__
  // Android presentation is the focused Project Grease renderer. It must not
  // depend on DRW cache availability while a Legacy GP stroke is still open:
  // Blender's sbuffer is the authoritative live input and there may be no
  // committed bGPDstroke/batch yet. The previous cache_ready gate caused
  // live drawing to remain invisible until ACTION_UP committed the stroke.
  //
  // Keep building the real Blender GP cache when possible, but never make
  // Android's live presentation depend on that cache.
  // The presenter draws the evaluated copies of layers with live modifiers; the originals stay
  // untouched. Dropping copies the last render left behind keeps the cache to what is drawn.
  for (size_t i = impl_->eval_cache.size(); i-- > 0;) {
    EvalCacheEntry &entry = *impl_->eval_cache[i];
    if (entry.revision != g_gp_data_revision || entry.stack_revision != impl_->stack_revision) {
      pg_mod_eval_free(&entry.frame);
      impl_->eval_cache.erase(impl_->eval_cache.begin() + static_cast<std::ptrdiff_t>(i));
    }
  }
  project_grease_android_set_frame_evaluator(eval_frame_trampoline, this);
  project_grease_android_set_fx_provider(fx_provider_trampoline, this);
  bool presented =
      project_grease_android_present_gp_document(impl_->gpd, impl_->frame->framenum) != 0;
  // Annotations are drawn over every layer, in screen-space thickness.
  if (impl_->annotations && impl_->annotations_visible) {
    if (!presented) {
#ifdef PG_HAVE_ANDROID_LOG
      __android_log_print(ANDROID_LOG_WARN, "ProjectGrease", "annotpass skipped: document pass failed");
#endif
    }
    else {
      presented = project_grease_android_present_annotations(impl_->annotations, impl_->frame->framenum) != 0;
    }
  }
  project_grease_android_set_frame_evaluator(nullptr, nullptr);
  project_grease_android_set_fx_provider(nullptr, nullptr);
#else
  const bool presented = cache_ready;
#endif

  DRW_gpencil_batch_cache_free(impl_->gpd);
#ifndef __ANDROID__
  ob->data = nullptr;
  BKE_id_free(impl_->bmain, &ob->id);
#endif

#ifdef __ANDROID__
  impl_->last_error = !presented
      ? "Blender Legacy GP Android presentation failed"
      : (cache_ready
          ? "Blender Legacy GP data/cache accepted and presented through Android GLES"
          : "Blender Legacy GP data presented through focused Android GLES while DRW cache was unavailable");
#else
  impl_->last_error = !cache_ready
      ? "DRW_cache_gpencil_get() returned null"
      : !presented
          ? "Blender Legacy GP cache built, but Android presentation failed"
          : "Blender Legacy GP data/cache accepted and presented through Android GLES";
#endif
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

bool Backend::material_fill_enabled(int index) const
{
  const Material *ma = gp_material_at(impl_->gpd, index);
  return ma && ma->gp_style && (ma->gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0;
}

bool Backend::get_stroke_info(int stroke_index, PGStrokeInfo *out) const
{
  if (!out || !impl_->frame || stroke_index < 0) {
    return false;
  }
  int current = 0;
  for (bGPDstroke *stroke = static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke != nullptr;
       stroke = stroke->next, ++current) {
    if (current == stroke_index) {
      return pg_doc_stroke_info_get(stroke, out) != 0;
    }
  }
  return false;
}

bool Backend::add_stroke(const StrokePoint *points, int count, const PGStrokeInfo &info)
{
  if (!impl_->frame || !impl_->frame_created) {
    impl_->last_error = "frame is not created";
    return false;
  }
  if (!points || count < 1) {
    impl_->last_error = "stroke needs at least one point";
    return false;
  }
  const int material_index = info.material_index < 0 ? 0 : info.material_index;
  if (!gp_material_ensure_slot(impl_->gpd, material_index)) {
    impl_->last_error = "Legacy GP material index is unavailable";
    return false;
  }

  const short thickness = static_cast<short>(
      std::max(1.0f, std::min(info.thickness, 32767.0f)));
  bGPDstroke *stroke = BKE_gpencil_stroke_add(
      impl_->frame, material_index, count, thickness, false);
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
    // Restore the saved vertex color; a zero default is "no vertex color" (see create_polyline()).
    const float rgba[4] = {src.r, src.g, src.b, src.a};
    pg_doc_point_color_apply(&dst, rgba);
  }

  pg_doc_stroke_info_apply(stroke, &info);
  if (stroke->flag & GP_STROKE_CYCLIC) {
    BKE_gpencil_stroke_fill_triangulate(stroke);
  }
  impl_->stroke = stroke;
  impl_->gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::get_layer_info(int index, PGLayerInfo *out) const
{
  const bGPDlayer *layer = layer_at(impl_->gpd, index);
  if (!layer || !out) {
    return false;
  }
  return pg_doc_layer_info_get(layer, out) != 0;
}

uint64_t Backend::modifier_eval_count() const { return impl_->eval_count; }

int Backend::fx_for_layer(const bGPDlayer *layer, const PGFxEntry **entries) const
{
  *entries = nullptr;
  if (!impl_->gpd || !layer) {
    return 0;
  }
  const int index = layer_index_of(impl_->gpd, layer);
  if (index < 0 || static_cast<size_t>(index) >= impl_->fx_stacks.size()) {
    return 0;
  }
  const std::vector<PGFxEntry> &list = impl_->fx_stacks[static_cast<size_t>(index)];
  *entries = list.data();
  return static_cast<int>(list.size());
}

int Backend::fx_count(int layer_index) const
{
  if (!impl_->gpd || layer_index < 0 || layer_index >= layer_total(impl_->gpd)) {
    return 0;
  }
  return static_cast<size_t>(layer_index) < impl_->fx_stacks.size()
             ? static_cast<int>(impl_->fx_stacks[static_cast<size_t>(layer_index)].size())
             : 0;
}

int Backend::fx_add(int layer_index, int type)
{
  std::vector<PGFxEntry> *list = layer_fx(impl_, layer_index);
  if (!list) {
    impl_->last_error = "layer index out of range";
    return -1;
  }
  if (!pg_fx_valid_type(type)) {
    impl_->last_error = "unknown effect type";
    return -1;
  }
  if (list->size() >= static_cast<size_t>(PG_FX_MAX_STACK)) {
    impl_->last_error = "effect list is full";
    return -1;
  }
  PGFxEntry entry;
  pg_fx_entry_init(&entry, type);
  list->push_back(entry);
  impl_->last_error.clear();
  return static_cast<int>(list->size()) - 1;
}

bool Backend::fx_remove(int layer_index, int fx_index)
{
  std::vector<PGFxEntry> *list = layer_fx(impl_, layer_index);
  if (!list || fx_index < 0 || static_cast<size_t>(fx_index) >= list->size()) {
    impl_->last_error = "effect index out of range";
    return false;
  }
  list->erase(list->begin() + fx_index);
  impl_->last_error.clear();
  return true;
}

bool Backend::fx_move(int layer_index, int from_index, int to_index)
{
  std::vector<PGFxEntry> *list = layer_fx(impl_, layer_index);
  const int n = list ? static_cast<int>(list->size()) : 0;
  if (!list || from_index < 0 || to_index < 0 || from_index >= n || to_index >= n) {
    impl_->last_error = "effect index out of range";
    return false;
  }
  if (from_index != to_index) {
    PGFxEntry moved = (*list)[static_cast<size_t>(from_index)];
    list->erase(list->begin() + from_index);
    list->insert(list->begin() + to_index, moved);
  }
  impl_->last_error.clear();
  return true;
}

bool Backend::fx_set_enabled(int layer_index, int fx_index, bool enabled)
{
  std::vector<PGFxEntry> *list = layer_fx(impl_, layer_index);
  if (!list || fx_index < 0 || static_cast<size_t>(fx_index) >= list->size()) {
    impl_->last_error = "effect index out of range";
    return false;
  }
  (*list)[static_cast<size_t>(fx_index)].enabled = enabled ? 1 : 0;
  impl_->last_error.clear();
  return true;
}

bool Backend::fx_set_target(int layer_index, int fx_index, int target)
{
  std::vector<PGFxEntry> *list = layer_fx(impl_, layer_index);
  if (!list || fx_index < 0 || static_cast<size_t>(fx_index) >= list->size() ||
      target < PG_FX_TARGET_LAYER || target > PG_FX_TARGET_FILLS) {
    impl_->last_error = "invalid effect target";
    return false;
  }
  (*list)[static_cast<size_t>(fx_index)].target = target;
  impl_->last_error.clear();
  return true;
}

int Backend::fx_target(int layer_index, int fx_index) const
{
  std::vector<PGFxEntry> *list = layer_fx(impl_, layer_index);
  if (!list || fx_index < 0 || static_cast<size_t>(fx_index) >= list->size()) return -1;
  return (*list)[static_cast<size_t>(fx_index)].target;
}

bool Backend::fx_set_params(int layer_index, int fx_index, const float *params, int count)
{
  std::vector<PGFxEntry> *list = layer_fx(impl_, layer_index);
  if (!list || fx_index < 0 || static_cast<size_t>(fx_index) >= list->size() || !params ||
      count < 0)
  {
    impl_->last_error = "invalid effect parameters";
    return false;
  }
  PGFxEntry &entry = (*list)[static_cast<size_t>(fx_index)];
  const int n = std::min(count, pg_fx_param_count(entry.type));
  for (int i = 0; i < n; ++i) {
    entry.params[i] = params[i];
  }
  pg_fx_sanitize(entry.type, entry.params);
  impl_->last_error.clear();
  return true;
}

int Backend::fx_get(int layer_index, int fx_index, int *type, int *enabled, float *params,
                    int capacity) const
{
  if (!impl_->gpd || layer_index < 0 || layer_index >= layer_total(impl_->gpd) ||
      static_cast<size_t>(layer_index) >= impl_->fx_stacks.size())
  {
    return -1;
  }
  const std::vector<PGFxEntry> &list = impl_->fx_stacks[static_cast<size_t>(layer_index)];
  if (fx_index < 0 || static_cast<size_t>(fx_index) >= list.size()) {
    return -1;
  }
  const PGFxEntry &entry = list[static_cast<size_t>(fx_index)];
  const int n = pg_fx_param_count(entry.type);
  if (type) *type = entry.type;
  if (enabled) *enabled = entry.enabled;
  if (params) {
    for (int i = 0; i < n && i < capacity; ++i) {
      params[i] = entry.params[i];
    }
  }
  return n;
}

int Backend::modifier_count(int layer_index) const
{
  if (!impl_->gpd || layer_index < 0 || layer_index >= layer_total(impl_->gpd)) {
    return 0;
  }
  return static_cast<size_t>(layer_index) < impl_->stacks.size()
             ? static_cast<int>(impl_->stacks[static_cast<size_t>(layer_index)].size())
             : 0;
}

int Backend::modifier_add(int layer_index, int type)
{
  std::vector<PGModEntry> *stack = layer_stack(impl_, layer_index);
  if (!stack) {
    impl_->last_error = "layer index out of range";
    return -1;
  }
  if (!pg_mod_valid_type(type)) {
    impl_->last_error = "unknown modifier type";
    return -1;
  }
  if (stack->size() >= static_cast<size_t>(PG_MOD_MAX_STACK)) {
    impl_->last_error = "modifier stack is full";
    return -1;
  }
  PGModEntry entry;
  pg_mod_entry_init(&entry, type);
  stack->push_back(entry);
  impl_->stack_revision++;
  impl_->last_error.clear();
  return static_cast<int>(stack->size()) - 1;
}

bool Backend::modifier_remove(int layer_index, int modifier_index)
{
  std::vector<PGModEntry> *stack = layer_stack(impl_, layer_index);
  if (!stack || modifier_index < 0 || static_cast<size_t>(modifier_index) >= stack->size()) {
    impl_->last_error = "modifier index out of range";
    return false;
  }
  stack->erase(stack->begin() + modifier_index);
  impl_->stack_revision++;
  impl_->last_error.clear();
  return true;
}

bool Backend::modifier_move(int layer_index, int from_index, int to_index)
{
  std::vector<PGModEntry> *stack = layer_stack(impl_, layer_index);
  const int n = stack ? static_cast<int>(stack->size()) : 0;
  if (!stack || from_index < 0 || to_index < 0 || from_index >= n || to_index >= n) {
    impl_->last_error = "modifier index out of range";
    return false;
  }
  if (from_index != to_index) {
    PGModEntry moved = (*stack)[static_cast<size_t>(from_index)];
    stack->erase(stack->begin() + from_index);
    stack->insert(stack->begin() + to_index, moved);
    impl_->stack_revision++;
  }
  impl_->last_error.clear();
  return true;
}

bool Backend::modifier_set_enabled(int layer_index, int modifier_index, bool enabled)
{
  std::vector<PGModEntry> *stack = layer_stack(impl_, layer_index);
  if (!stack || modifier_index < 0 || static_cast<size_t>(modifier_index) >= stack->size()) {
    impl_->last_error = "modifier index out of range";
    return false;
  }
  (*stack)[static_cast<size_t>(modifier_index)].enabled = enabled ? 1 : 0;
  impl_->stack_revision++;
  impl_->last_error.clear();
  return true;
}

bool Backend::modifier_set_params(int layer_index, int modifier_index, const float *params, int count)
{
  std::vector<PGModEntry> *stack = layer_stack(impl_, layer_index);
  if (!stack || modifier_index < 0 || static_cast<size_t>(modifier_index) >= stack->size() ||
      !params || count < 0)
  {
    impl_->last_error = "invalid modifier parameters";
    return false;
  }
  PGModEntry &entry = (*stack)[static_cast<size_t>(modifier_index)];
  const int n = std::min(count, pg_mod_param_count(entry.type));
  for (int i = 0; i < n; ++i) {
    entry.params[i] = params[i];
  }
  pg_mod_sanitize(entry.type, entry.params);
  impl_->stack_revision++;
  impl_->last_error.clear();
  return true;
}

int Backend::modifier_get(int layer_index, int modifier_index, int *type, int *enabled,
                          float *params, int capacity) const
{
  if (!impl_->gpd || layer_index < 0 || layer_index >= layer_total(impl_->gpd) ||
      static_cast<size_t>(layer_index) >= impl_->stacks.size())
  {
    return -1;
  }
  const std::vector<PGModEntry> &stack = impl_->stacks[static_cast<size_t>(layer_index)];
  if (modifier_index < 0 || static_cast<size_t>(modifier_index) >= stack.size()) {
    return -1;
  }
  const PGModEntry &entry = stack[static_cast<size_t>(modifier_index)];
  const int n = pg_mod_param_count(entry.type);
  if (type) *type = entry.type;
  if (enabled) *enabled = entry.enabled;
  if (params) {
    for (int i = 0; i < n && i < capacity; ++i) {
      params[i] = entry.params[i];
    }
  }
  return n;
}

bool Backend::modifier_apply(int layer_index, int modifier_index)
{
  std::vector<PGModEntry> *stack = layer_stack(impl_, layer_index);
  if (!stack || modifier_index < 0 || static_cast<size_t>(modifier_index) >= stack->size()) {
    impl_->last_error = "modifier index out of range";
    return false;
  }
  bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  if (!layer) {
    impl_->last_error = "layer index out of range";
    return false;
  }
  const PGModEntry entry = (*stack)[static_cast<size_t>(modifier_index)];
  const int cfra = impl_->frame ? impl_->frame->framenum : 1;
  /* Bake into the originals. Simplify/Merge may free strokes, so drop the open-stroke pointer. */
  impl_->stroke = nullptr;
  pg_mod_apply(impl_->gpd, layer, &entry, cfra);
  stack->erase(stack->begin() + modifier_index);
  impl_->stack_revision++;
  BKE_gpencil_stats_update(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::layer_use_mask(int layer_index) const
{
  const bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  return layer != nullptr && (layer->flag & GP_LAYER_USE_MASK) != 0;
}

bool Backend::set_layer_use_mask(int layer_index, bool enabled)
{
  bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  if (!layer) { impl_->last_error = "layer index out of range"; return false; }
  if (enabled) layer->flag |= GP_LAYER_USE_MASK;
  else layer->flag &= ~GP_LAYER_USE_MASK;
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

int Backend::mask_count(int layer_index) const
{
  const bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  return layer ? BLI_listbase_count(&layer->mask_layers) : 0;
}

bool Backend::mask_add(int layer_index, int mask_layer_index)
{
  bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  bGPDlayer *mask_layer = layer_at(impl_->gpd, mask_layer_index);
  if (!layer || !mask_layer || layer == mask_layer) {
    impl_->last_error = "invalid mask layer";
    return false;
  }
  if (BKE_gpencil_layer_mask_named_get(layer, mask_layer->info) != nullptr) {
    impl_->last_error = "layer is already a mask";
    return false;
  }
  BKE_gpencil_layer_mask_add(layer, mask_layer->info);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

static bGPDlayer_Mask *mask_at(bGPDlayer *layer, int index)
{
  return layer && index >= 0
             ? static_cast<bGPDlayer_Mask *>(BLI_findlink(&layer->mask_layers, index))
             : nullptr;
}

bool Backend::mask_remove(int layer_index, int mask_index)
{
  bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  bGPDlayer_Mask *mask = mask_at(layer, mask_index);
  if (!mask) { impl_->last_error = "mask index out of range"; return false; }
  BKE_gpencil_layer_mask_remove(layer, mask);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::mask_get(int layer_index, int mask_index, char *name, int name_capacity, int *flags) const
{
  bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  const bGPDlayer_Mask *mask = mask_at(layer, mask_index);
  if (!mask) return false;
  if (name && name_capacity > 0) {
    std::strncpy(name, mask->name, static_cast<size_t>(name_capacity) - 1);
    name[name_capacity - 1] = '\0';
  }
  if (flags) *flags = ((mask->flag & GP_MASK_HIDE) ? 1 : 0) | ((mask->flag & GP_MASK_INVERT) ? 2 : 0);
  return true;
}

bool Backend::mask_set_flags(int layer_index, int mask_index, int flags)
{
  bGPDlayer *layer = layer_at(impl_->gpd, layer_index);
  bGPDlayer_Mask *mask = mask_at(layer, mask_index);
  if (!mask) { impl_->last_error = "mask index out of range"; return false; }
  mask->flag = static_cast<short>(((flags & 1) ? GP_MASK_HIDE : 0) | ((flags & 2) ? GP_MASK_INVERT : 0));
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

int Backend::vertex_group_count() const
{
  return impl_->gpd ? BLI_listbase_count(&impl_->gpd->vertex_group_names) : 0;
}

static bDeformGroup *vgroup_at(const bGPdata *gpd, int group)
{
  return gpd && group >= 0 ? static_cast<bDeformGroup *>(BLI_findlink(&gpd->vertex_group_names, group))
                           : nullptr;
}

bool Backend::vertex_group_name(int group, char *name, int name_capacity) const
{
  const bDeformGroup *dg = vgroup_at(impl_->gpd, group);
  if (!dg || !name || name_capacity <= 0) return false;
  std::strncpy(name, dg->name, static_cast<size_t>(name_capacity) - 1);
  name[name_capacity - 1] = '\0';
  return true;
}

int Backend::vertex_group_add(const char *name)
{
  if (!impl_->gpd) { impl_->last_error = "document is not created"; return -1; }
  if (vertex_group_count() >= 255) { impl_->last_error = "too many vertex groups"; return -1; }
  bDeformGroup *dg = static_cast<bDeformGroup *>(MEM_callocN(sizeof(bDeformGroup), "Project Grease vertex group"));
  if (!dg) return -1;
  std::strncpy(dg->name, (name && name[0]) ? name : "Group", sizeof(dg->name) - 1);
  BLI_addtail(&impl_->gpd->vertex_group_names, dg);
  BLI_uniquename(&impl_->gpd->vertex_group_names, dg, "Group", '.', offsetof(bDeformGroup, name), sizeof(dg->name));
  const int index = vertex_group_count() - 1;
  if (impl_->gpd->vertex_group_active_index <= 0) impl_->gpd->vertex_group_active_index = index + 1;
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return index;
}

bool Backend::vertex_group_remove(int group)
{
  bDeformGroup *dg = vgroup_at(impl_->gpd, group);
  if (!dg) { impl_->last_error = "vertex group index out of range"; return false; }
  /* Drop the weights of this group and shift the higher group numbers down, in every frame. */
  LISTBASE_FOREACH (bGPDlayer *, layer, &impl_->gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, frame, &layer->frames) {
      LISTBASE_FOREACH (bGPDstroke *, stroke, &frame->strokes) {
        if (!stroke->dvert) continue;
        for (int i = 0; i < stroke->totpoints; ++i) {
          MDeformVert *dv = &stroke->dvert[i];
          if (MDeformWeight *dw = BKE_defvert_find_index(dv, group)) {
            BKE_defvert_remove_group(dv, dw);
          }
          for (int k = 0; k < dv->totweight; ++k) {
            if (static_cast<int>(dv->dw[k].def_nr) > group) dv->dw[k].def_nr--;
          }
        }
      }
    }
  }
  BLI_freelinkN(&impl_->gpd->vertex_group_names, dg);
  const int count = vertex_group_count();
  int &active = impl_->gpd->vertex_group_active_index; /* 1-based, 0 = none */
  if (active > group + 1) active--;
  else if (active == group + 1) active = count > 0 ? std::min(group + 1, count) : 0;
  if (active > count) active = count;
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::vertex_group_rename(int group, const char *name)
{
  bDeformGroup *dg = vgroup_at(impl_->gpd, group);
  if (!dg || !name || !name[0]) { impl_->last_error = "invalid vertex group rename"; return false; }
  std::strncpy(dg->name, name, sizeof(dg->name) - 1);
  dg->name[sizeof(dg->name) - 1] = '\0';
  BLI_uniquename(&impl_->gpd->vertex_group_names, dg, "Group", '.', offsetof(bDeformGroup, name), sizeof(dg->name));
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

int Backend::vertex_group_active() const
{
  if (!impl_->gpd) return -1;
  const int active = impl_->gpd->vertex_group_active_index - 1;
  return active >= 0 && active < vertex_group_count() ? active : -1;
}

bool Backend::set_vertex_group_active(int group)
{
  if (!vgroup_at(impl_->gpd, group)) { impl_->last_error = "vertex group index out of range"; return false; }
  impl_->gpd->vertex_group_active_index = group + 1;
  impl_->last_error.clear();
  return true;
}

static bGPDstroke *frame_stroke_at(bGPDframe *frame, int index)
{
  return frame && index >= 0 ? static_cast<bGPDstroke *>(BLI_findlink(&frame->strokes, index)) : nullptr;
}

int Backend::point_weight_count(int stroke_index, int point_index) const
{
  const bGPDstroke *stroke = frame_stroke_at(impl_->frame, stroke_index);
  if (!stroke || !stroke->dvert || point_index < 0 || point_index >= stroke->totpoints) return 0;
  return stroke->dvert[point_index].totweight;
}

bool Backend::point_weight_at(int stroke_index, int point_index, int k, int *group, float *weight) const
{
  const bGPDstroke *stroke = frame_stroke_at(impl_->frame, stroke_index);
  if (!stroke || !stroke->dvert || point_index < 0 || point_index >= stroke->totpoints) return false;
  const MDeformVert &dv = stroke->dvert[point_index];
  if (k < 0 || k >= dv.totweight) return false;
  if (group) *group = static_cast<int>(dv.dw[k].def_nr);
  if (weight) *weight = dv.dw[k].weight;
  return true;
}

bool Backend::set_point_weight(int stroke_index, int point_index, int group, float weight)
{
  bGPDstroke *stroke = frame_stroke_at(impl_->frame, stroke_index);
  if (!stroke || point_index < 0 || point_index >= stroke->totpoints || group < 0 ||
      !std::isfinite(weight))
  {
    impl_->last_error = "invalid point weight target";
    return false;
  }
  if (!stroke->dvert) {
    stroke->dvert = static_cast<MDeformVert *>(
        MEM_callocN(sizeof(MDeformVert) * static_cast<size_t>(stroke->totpoints), "gp_stroke_weights"));
    if (!stroke->dvert) return false;
  }
  MDeformWeight *dw = BKE_defvert_ensure_index(&stroke->dvert[point_index], group);
  if (!dw) return false;
  dw->weight = std::min(1.0f, std::max(0.0f, weight));
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::set_layer_opacity(int index, float opacity)
{
  bGPDlayer *layer = layer_at(impl_->gpd, index);
  if (!layer || !std::isfinite(opacity)) {
    impl_->last_error = "invalid layer opacity";
    return false;
  }
  layer->opacity = std::max(0.0f, std::min(opacity, 1.0f));
  project_grease_gp_tag(impl_->gpd);
  return true;
}

bool Backend::get_material_info(int index, PGMaterialInfo *out) const
{
  const Material *ma = gp_material_at(impl_->gpd, index);
  if (!ma || !out) {
    return false;
  }
  return pg_doc_material_info_get(ma, out) != 0;
}



bool Backend::apply_legacy_geometry_batch(int stroke_index,
                                          const LegacyGeometryOp *operations,
                                          int operation_count)
{
  if (!impl_->gpd || !impl_->frame || !operations || operation_count <= 0) {
    impl_->last_error = "invalid Legacy GP geometry batch";
    return false;
  }

  bGPDstroke *stroke = nullptr;
  int current = 0;
  for (bGPDstroke *candidate =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       candidate != nullptr;
       candidate = candidate->next, ++current) {
    if (current == stroke_index) {
      stroke = candidate;
      break;
    }
  }

  if (!stroke) {
    impl_->last_error = "Legacy GP geometry batch stroke index out of range";
    return false;
  }

  for (int i = 0; i < operation_count; ++i) {
    const LegacyGeometryOp &op = operations[i];

    switch (op.type) {
      case LegacyGeometryOpType::SimplifyAdaptive:
        BKE_gpencil_stroke_simplify_adaptive(impl_->gpd, stroke, op.value0);
        break;

      case LegacyGeometryOpType::SimplifyFixed:
        BKE_gpencil_stroke_simplify_fixed(impl_->gpd, stroke);
        break;

      case LegacyGeometryOpType::Subdivide:
        BKE_gpencil_stroke_subdivide(
            impl_->gpd, stroke, std::max(1, op.int0), op.int1);
        break;

      case LegacyGeometryOpType::TrimIntersection:
        if (!BKE_gpencil_stroke_trim(impl_->gpd, stroke)) {
          impl_->last_error = "Blender Legacy GP trim found no intersection";
          return false;
        }
        break;

      case LegacyGeometryOpType::TrimPoints:
        if (!BKE_gpencil_stroke_trim_points(
                stroke, op.int0, op.int1, op.flag0)) {
          impl_->last_error = "Blender Legacy GP point trim failed";
          return false;
        }
        break;

      case LegacyGeometryOpType::MergeDistance:
        BKE_gpencil_stroke_merge_distance(
            impl_->gpd, impl_->frame, stroke, op.value0, op.flag0);
        break;

      case LegacyGeometryOpType::Sample:
        if (!BKE_gpencil_stroke_sample(
                impl_->gpd, stroke, op.value0, op.flag0, op.value1)) {
          impl_->last_error = "Blender Legacy GP resample failed";
          return false;
        }
        break;

      case LegacyGeometryOpType::SmoothStrength:
        for (int point = 0; point < stroke->totpoints; ++point) {
          BKE_gpencil_stroke_smooth_strength(stroke, point, op.value0, 1, stroke);
        }
        break;

      case LegacyGeometryOpType::SmoothThickness:
        for (int point = 0; point < stroke->totpoints; ++point) {
          BKE_gpencil_stroke_smooth_thickness(stroke, point, op.value0, 1, stroke);
        }
        break;

      case LegacyGeometryOpType::SmoothUV:
        for (int point = 0; point < stroke->totpoints; ++point) {
          BKE_gpencil_stroke_smooth_uv(stroke, point, op.value0, 1, stroke);
        }
        break;

      case LegacyGeometryOpType::Stretch:
        if (!BKE_gpencil_stroke_stretch(stroke,
                                        op.value0,
                                        op.value1,
                                        static_cast<short>(op.int0),
                                        op.flag0,
                                        std::max(0, op.int1),
                                        op.value2,
                                        op.value1,
                                        op.flag1)) {
          impl_->last_error = "Blender Legacy GP stroke stretch failed";
          return false;
        }
        break;

      case LegacyGeometryOpType::Close:
        if (!BKE_gpencil_stroke_close(stroke)) {
          impl_->last_error = "Blender Legacy GP stroke close failed";
          return false;
        }
        break;

      case LegacyGeometryOpType::Dissolve:
        BKE_gpencil_dissolve_points(
            impl_->gpd, impl_->frame, stroke, static_cast<short>(op.int0));
        break;

      case LegacyGeometryOpType::FillTriangulate:
        BKE_gpencil_stroke_fill_triangulate(stroke);
        break;
      
      case LegacyGeometryOpType::Reverse:
        BKE_gpencil_stroke_flip(stroke);
        break;

      case LegacyGeometryOpType::UniformSubdivide:
        BKE_gpencil_stroke_uniform_subdivide(
            impl_->gpd, stroke, std::max<uint32_t>(2u, static_cast<uint32_t>(op.int0)), op.flag0);
        break;

      case LegacyGeometryOpType::Shrink:
        if (!BKE_gpencil_stroke_shrink(
                stroke, std::max(0.0f, op.value0), static_cast<short>(op.int0))) {
          impl_->last_error = "Blender Legacy GP stroke shrink failed";
          return false;
        }
        break;

      case LegacyGeometryOpType::RandomColor:
        BKE_gpencil_stroke_set_random_color(stroke);
        break;
    }
  }

  impl_->stroke = stroke;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::apply_blender_modifier(int index, int modifier_type, float factor, int iterations)
{
  if (!impl_->frame || !impl_->gpd || index < 0) {
    impl_->last_error = "invalid Legacy GP geometry target";
    return false;
  }

  /*
   * This one-shot path does not evaluate Blender's object modifier stack: only operations with a
   * direct Blender 3.6.23 Legacy GP BKE implementation are exposed here. The live, non-destructive
   * per-layer stack is a separate Project Grease structure (project_grease_modifier_stack.c,
   * Backend::modifier_*) that calls the same BKE functions and the verbatim deformStroke() bodies;
   * it needs no Depsgraph or Object.
   */
  const float amount = std::max(0.0f, factor);
  switch (modifier_type) {
    case eGpencilModifierType_Smooth:
      return smooth_stroke(index, std::min(amount, 1.0f), std::max(1, iterations));

    case eGpencilModifierType_Simplify: {
      LegacyGeometryOp op{};
      op.type = LegacyGeometryOpType::SimplifyAdaptive;
      op.value0 = amount;
      return apply_legacy_geometry_batch(index, &op, 1);
    }

    case eGpencilModifierType_Subdiv: {
      LegacyGeometryOp op{};
      op.type = LegacyGeometryOpType::Subdivide;
      op.int0 = std::max(1, iterations);
      return apply_legacy_geometry_batch(index, &op, 1);
    }

    default:
      impl_->last_error =
          "This Legacy GP modifier requires Blender's real object-level modifier stack";
      return false;
  }
}

bool Backend::apply_blender_generator(int modifier_type, float factor, int iterations)
{
  (void)modifier_type;
  (void)factor;
  (void)iterations;
  impl_->last_error = "Legacy GP generator is outside the focused Android closure";
  return false;
}

bool Backend::apply_blender_modifier_stack(int index,
                                           const int *modifier_types,
                                           int modifier_count,
                                           float factor,
                                           int iterations)
{
  if (!modifier_types || modifier_count <= 0) {
    impl_->last_error = "invalid Legacy GP modifier stack target";
    return false;
  }

  for (int i = 0; i < modifier_count; ++i) {
    if (!apply_blender_modifier(index, modifier_types[i], factor, iterations)) {
      return false;
    }
  }

  impl_->last_error.clear();
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

bool Backend::sculpt_at(int tool, float x, float y, float radius, float influence)
{
  if (!impl_->frame) {
    impl_->last_error = "no active frame";
    return false;
  }
  return sculpt_update(tool, x, y, x, y, 1.0f, radius, influence, false);
}

bool Backend::sculpt_begin(int tool, float x, float y, float pressure,
                           float radius, float strength, bool invert)
{
  impl_->stroke_style.thickness = radius;
  impl_->last_error.clear();
  if (!impl_->frame) {
    impl_->last_error = "no active frame";
    return false;
  }
  return sculpt_update(tool, x, y, x, y, pressure, radius, strength, invert);
}

bool Backend::sculpt_update(int tool, float x, float y, float prev_x, float prev_y,
                            float pressure, float radius, float strength, bool invert)
{
  if (!impl_->gpd || !impl_->frame || radius <= 0.0f ||
      !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(prev_x) || !std::isfinite(prev_y) ||
      !std::isfinite(pressure) || !std::isfinite(strength)) {
    impl_->last_error = "invalid Legacy GP sculpt stroke";
    return false;
  }

  legacy_gp_sculpt::Tool sculpt_tool;
  switch (tool) {
    case 0: sculpt_tool = legacy_gp_sculpt::Smooth; break;
    case 1: sculpt_tool = legacy_gp_sculpt::Thickness; break;
    case 2: sculpt_tool = legacy_gp_sculpt::Strength; break;
    case 3: sculpt_tool = legacy_gp_sculpt::Grab; break;
    case 4: sculpt_tool = legacy_gp_sculpt::Push; break;
    case 5: sculpt_tool = legacy_gp_sculpt::Pinch; break;
    case 6: sculpt_tool = legacy_gp_sculpt::Twist; break;
    case 7: sculpt_tool = legacy_gp_sculpt::Randomize; break;
    default:
      impl_->last_error = "unknown Legacy GP sculpt brush";
      return false;
  }

  legacy_gp_sculpt::Context context{};
  context.mouse_x = x;
  context.mouse_y = y;
  context.prev_x = prev_x;
  context.prev_y = prev_y;
  context.delta_x = x - prev_x;
  context.delta_y = y - prev_y;

  legacy_gp_sculpt::Settings settings{};
  settings.brush_alpha = std::clamp(strength, 0.0f, 1.0f);
  settings.pressure = std::clamp(pressure, 0.0f, 1.0f);
  settings.radius = radius;
  settings.invert = invert;
  settings.apply_position = sculpt_tool == legacy_gp_sculpt::Smooth ||
                             sculpt_tool == legacy_gp_sculpt::Grab ||
                             sculpt_tool == legacy_gp_sculpt::Push ||
                             sculpt_tool == legacy_gp_sculpt::Pinch ||
                             sculpt_tool == legacy_gp_sculpt::Twist ||
                             sculpt_tool == legacy_gp_sculpt::Randomize;
  settings.apply_strength = sculpt_tool == legacy_gp_sculpt::Smooth ||
                             sculpt_tool == legacy_gp_sculpt::Strength ||
                             sculpt_tool == legacy_gp_sculpt::Randomize;
  settings.apply_thickness = sculpt_tool == legacy_gp_sculpt::Smooth ||
                              sculpt_tool == legacy_gp_sculpt::Thickness ||
                              sculpt_tool == legacy_gp_sculpt::Randomize;
  settings.apply_uv = sculpt_tool == legacy_gp_sculpt::Smooth ||
                       sculpt_tool == legacy_gp_sculpt::Randomize;

  bool changed = false;
  for (bGPDstroke *stroke =
           static_cast<bGPDstroke *>(impl_->frame->strokes.first);
       stroke; stroke = stroke->next) {
    changed |= legacy_gp_sculpt::apply(
        impl_->gpd, impl_->frame, stroke, sculpt_tool, context, settings, 2);
  }

  if (!changed) {
    impl_->last_error = "Legacy GP sculpt brush hit no editable points";
    return false;
  }

  impl_->stroke = nullptr;
  BKE_gpencil_batch_cache_dirty_tag(impl_->gpd);
  project_grease_gp_tag(impl_->gpd);
  impl_->last_error.clear();
  return true;
}

bool Backend::sculpt_end()
{
  impl_->last_error.clear();
  return true;
}

bool Backend::set_onion_skin(bool enabled, int before, int after, float opacity)
{
  if (!impl_->layer) {
    impl_->last_error = "layer is not selected";
    return false;
  }
  // Overlay switch and keyframe counts of the data (Blender: overlay "Onion Skins",
  // bGPdata.gstep / gstep_next); each layer keeps its own GP_LAYER_ONIONSKIN.
  if (enabled) impl_->gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
  else impl_->gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
  impl_->gpd->gstep = static_cast<short>(std::max(0, std::min(before, 100)));
  impl_->gpd->gstep_next = static_cast<short>(std::max(0, std::min(after, 100)));
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


void Backend::set_fill_screen_map(float scale, float origin_x, float origin_y)
{
  impl_->fill_map_scale = scale;
  impl_->fill_map_origin_x = origin_x;
  impl_->fill_map_origin_y = origin_y;
}

void Backend::set_fill_factor(float factor)
{
  impl_->fill_factor = pg_fill_factor_clamp(factor);
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
  // presentation layer supplies that mask (red = boundary, lower-left origin).
  legacy_gp_fill::normalize_to_legacy_mask(image, 0.5f);

  // gpencil_render_offscreen() renders at region size * fill_factor (min 128 px). The presenter
  // renders at view size, so the mask is resampled on the CPU to that size
  // (project_grease_blender_fill.c documents the adaptation).
  const float factor = impl_->fill_factor;
  const int sx = pg_fill_render_size(width, factor);
  const int sy = pg_fill_render_size(height, factor);
  std::vector<float> scaled;
  float *mask = image.rgba().data();
  if (sx != width || sy != height) {
    scaled.resize(static_cast<size_t>(sx) * static_cast<size_t>(sy) * 4u);
    pg_fill_resample_mask(mask, width, height, scaled.data(), sx, sy);
    mask = scaled.data();
  }
  // GLES readback has its origin at the lower-left; Android touch coordinates are top-left based.
  const int raster_seed_y = height - 1 - seed_y;
  const int img_seed_x = std::min(sx - 1, static_cast<int>(static_cast<float>(seed_x) * sx / width));
  const int img_seed_y =
      std::min(sy - 1, static_cast<int>(static_cast<float>(raster_seed_y) * sy / height));
  // tgpf->fill_leak = ceil(FILL_LEAK * fill_factor); a positive fill_leak overrides it.
  const int leak = fill_leak > 0 ? fill_leak : pg_fill_leak_from_factor(factor);
  // draw_mouse_position(): point of 4 * fill_factor * sqrt(2) px.
  const float point_size = 4.0f * factor * 1.41421356f;
  float *xy = nullptr;
  int border_contact = 0;
  const int total = pg_fill_raster(mask, sx, sy, img_seed_x, img_seed_y, point_size, leak,
                                   dilate_pixels, &xy, &border_contact);
  if (total < 3) {
    std::free(xy);
    impl_->last_error = border_contact
        ? "Legacy GP fill reached the render boundary"
        : "Legacy GP fill found no closed area";
    return false;
  }

  // The outline is in pixels of the fill image: back to view pixels and canvas units, as Blender's
  // fill unprojects its 2D boundary (gpencil_stroke_from_buffer -> gpencil_stroke_convertcoords_tpoint).
  const float map_scale = impl_->fill_map_scale > 1e-6f ? impl_->fill_map_scale : 1.0f;
  const float kx = static_cast<float>(width) / static_cast<float>(sx);
  const float ky = static_cast<float>(height) / static_cast<float>(sy);
  std::vector<StrokePoint> points;
  points.reserve(static_cast<size_t>(total));
  for (int i = 0; i < total; i++) {
    const float px = xy[i * 2] * kx;
    const float py = xy[i * 2 + 1] * ky;
    points.push_back({(px - impl_->fill_map_origin_x) / map_scale,
                      (static_cast<float>(height) - py - impl_->fill_map_origin_y) / map_scale,
                      0.0f,
                      1.0f,
                      1.0f,
                      0.0f});
  }
  std::free(xy);

  if (!create_polyline(points.data(),
                       static_cast<int>(points.size()),
                       style,
                       true)) {
    return false;
  }

  // gpencil_stroke_from_buffer(): per-point smooth in place, fill_simplylvl (brush default 1)
  // passes of BKE_gpencil_stroke_simplify_fixed, then the geometry update.
  if (impl_->stroke) {
    bGPDstroke *gps = impl_->stroke;
    for (int i = 0; i < gps->totpoints; i++) {
      BKE_gpencil_stroke_smooth_point(gps, i, 1.0f, 2, false, true, gps);
    }
    BKE_gpencil_stroke_simplify_fixed(impl_->gpd, gps);
    BKE_gpencil_stroke_geometry_update(impl_->gpd, gps);
  }

  impl_->last_error.clear();
  return true;
}

const char *Backend::last_error() const { return impl_->last_error.c_str(); }

bGPdata *Backend::document_data() const { return impl_->gpd; }

bGPDlayer *Backend::active_layer_data() const { return impl_->layer; }

bGPdata *Backend::annotation_data() const { return impl_->annotations; }
void Backend::set_annotations_visible(bool visible) { impl_->annotations_visible = visible; }
bool Backend::annotations_visible() const { return impl_->annotations_visible; }
int Backend::current_frame_number() const { return impl_->frame ? impl_->frame->framenum : 1; }

void Backend::sync_active_frame()
{
  impl_->frame = impl_->layer ? impl_->layer->actframe : nullptr;
  impl_->frame_created = impl_->frame != nullptr;
  project_grease_gp_tag(impl_->gpd);
}

}  // namespace project_grease::gp