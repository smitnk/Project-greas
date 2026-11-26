/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Port of Blender 3.6.23's undo system (blenkernel/intern/undo_system.cc, BKE_undo_system.h):
 * UndoStack / UndoStep / UndoType, BKE_undosys_step_push(_with_type / _init_with_type),
 * BKE_undosys_step_undo / _redo / load_data(_ex) (jump), BKE_undosys_stack_limit_steps_and_memory
 * with ED_undo_push's U.undosteps / U.undomemory order (editors/undo/ed_undo.cc), step names,
 * skip steps, the active step and step_init.
 *
 * Plus the memfile chunk idea of blenloader/intern/undofile.cc (BLO_memfile_chunk_add): a chunk
 * identical to the one of the reference (previous) memfile is shared by pointer instead of copied,
 * and only the chunks a step owns count in its data_size. Ownership is a shared_ptr refcount, which
 * does BLO_memfile_merge's ownership transfer when an older step is freed.
 *
 * Header-only (inline) so every target that links project_grease_gp_backend.cpp builds unchanged.
 * No Blender context: the context passed through is the caller's (the Backend::Impl).
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

/* U.undosteps (userdef_default.c); U.undomemory is 0 there (no limit), 256 MB on a phone. */
#define PG_UNDO_STEPS 32
#define PG_UNDO_MEMORY_MB 256

namespace pg_undo {

/* eUndoStepDir */
enum { STEP_REDO = 1, STEP_UNDO = -1, STEP_INVALID = 0 };

struct UndoStep;

/* UndoType: one per mode (memfile for object / pose / GP modes, armature for armature Edit mode). */
struct UndoType {
  const char *name;
  /* BKE_undosys_type_from_context: is this the type for the current mode? */
  bool (*poll)(void *C);
  bool (*step_encode)(void *C, UndoStep *us);
  void (*step_decode)(void *C, UndoStep *us, int dir, bool is_final);
  void (*step_free)(UndoStep *us);
  void (*step_encode_init)(void *C, UndoStep *us); /* optional */
};

struct UndoStep {
  std::string name;
  const UndoType *type = nullptr;
  size_t data_size = 0; /* bytes this step owns (shared memfile chunks are not counted) */
  bool skip = false;
  bool is_applied = false;
  std::shared_ptr<void> data; /* the type's payload */
};

struct UndoStack {
  std::vector<UndoStep *> steps;
  UndoStep *step_active = nullptr;
  UndoStep *step_init = nullptr;
  const std::vector<const UndoType *> *types = nullptr; /* by poll order; the last is the fallback */

  ~UndoStack() { clear(); delete step_init; }

  int index_of(const UndoStep *us) const
  {
    for (size_t i = 0; i < steps.size(); i++) if (steps[i] == us) return int(i);
    return -1;
  }
  UndoStep *prev(const UndoStep *us) const { int i = index_of(us); return i > 0 ? steps[i - 1] : nullptr; }
  UndoStep *next(const UndoStep *us) const
  {
    int i = index_of(us);
    return i >= 0 && i + 1 < int(steps.size()) ? steps[i + 1] : nullptr;
  }

  const UndoType *type_from_context(void *C) const
  {
    for (const UndoType *ut : *types) if (!ut->poll || ut->poll(C)) return ut;
    return nullptr;
  }

  void free_and_unlink(UndoStep *us)
  {
    us->type->step_free(us);
    int i = index_of(us);
    steps.erase(steps.begin() + i);
    if (step_active == us) step_active = nullptr;
    delete us;
  }

  /* BKE_undosys_stack_clear */
  void clear()
  {
    while (!steps.empty()) free_and_unlink(steps.back());
    step_active = nullptr;
  }

  /* undosys_stack_clear_all_first: free from the first step up to and including us. */
  void clear_all_first(UndoStep *us, UndoStep *us_exclude)
  {
    if (us && us == us_exclude) us = prev(us);
    if (!us) return;
    UndoStep *us_iter;
    do {
      us_iter = steps.front();
      if (us_iter == us_exclude) us_iter = steps[1];
      free_and_unlink(us_iter);
    } while (us != us_iter);
  }

  /* BKE_undosys_stack_limit_steps_and_memory (WITH_GLOBAL_UNDO_KEEP_ONE: memfile = types->back()) */
  void limit_steps_and_memory(int max_steps, size_t memory_limit)
  {
    if (max_steps == -1 && memory_limit == 0) return;
    UndoStep *us = steps.empty() ? nullptr : steps.back();
    UndoStep *us_exclude = nullptr;
    size_t data_size_all = 0;
    int us_count = 0;
    for (; us && prev(us); us = prev(us)) { /* keep at least two (original + other) */
      if (memory_limit) {
        data_size_all += us->data_size;
        if (data_size_all > memory_limit) break;
      }
      if (max_steps != -1) {
        if (us_count == max_steps) break;
        if (!us->skip) us_count += 1;
      }
    }
    if (us && prev(us)) {
      const UndoType *memfile = types->back();
      if (us->type != memfile) { /* keep at least one memfile step */
        us_exclude = prev(us);
        while (us_exclude && us_exclude->type != memfile) us_exclude = prev(us_exclude);
        if (us_exclude) us_exclude->skip = true;
      }
      clear_all_first(prev(us), us_exclude);
    }
  }

  /* BKE_undosys_step_push_init_with_type */
  UndoStep *push_init_with_type(void *C, const char *name, const UndoType *ut)
  {
    if (!ut->step_encode_init || step_init) return nullptr;
    if (step_active) while (steps.back() != step_active) free_and_unlink(steps.back());
    step_init = new UndoStep();
    if (name) step_init->name = name;
    step_init->type = ut;
    ut->step_encode_init(C, step_init);
    return step_init;
  }

  /* BKE_undosys_step_push_with_type */
  bool push_with_type(void *C, const char *name, const UndoType *ut)
  {
    /* Remove all undo-steps after (also when step_active == nullptr). */
    while (!steps.empty() && steps.back() != step_active) free_and_unlink(steps.back());
    UndoStep *us = step_init ? step_init : new UndoStep();
    step_init = nullptr;
    if (us->name.empty()) us->name = name ? name : "";
    us->type = ut;
    if (!ut->step_encode(C, us)) { delete us; return false; }
    for (UndoStep *s : steps) s->is_applied = false;
    us->is_applied = true;
    steps.push_back(us);
    step_active = us;
    return true;
  }

  /* ED_undo_push: limit to U.undosteps (only at the tip), push, then limit to U.undomemory (MB). */
  bool push(void *C, const char *name, int undosteps, int undomemory)
  {
    const UndoType *ut = step_init ? step_init->type : type_from_context(C);
    if (!ut) return false;
    if (undosteps <= 0 && step_init) undosteps = 1;
    if (undosteps <= 0) return false;
    if (step_active && step_active == steps.back()) limit_steps_and_memory(undosteps - 1, 0);
    const bool ok = push_with_type(C, name, ut);
    if (undomemory != 0) limit_steps_and_memory(-1, size_t(undomemory) * 1024 * 1024);
    return ok;
  }

  /* BKE_undosys_step_calc_direction */
  int calc_direction(const UndoStep *us_target, const UndoStep *us_reference) const
  {
    const int t = index_of(us_target), r = index_of(us_reference);
    if (t < 0 || r < 0) return STEP_INVALID;
    return t <= r ? STEP_UNDO : STEP_REDO;
  }

  /* BKE_undosys_step_load_data_ex: decode every step from the reference to the target, in order. */
  bool load_data_ex(void *C, UndoStep *us_target, UndoStep *us_reference, bool use_skip)
  {
    if (!us_target) return false;
    if (!us_reference) us_reference = step_active;
    if (!us_reference) return false;
    const int undo_dir = calc_direction(us_target, us_reference);
    if (undo_dir == STEP_INVALID) return false;
    UndoStep *us_target_active = us_target;
    if (use_skip) {
      while (us_target_active && us_target_active->skip)
        us_target_active = undo_dir == STEP_UNDO ? prev(us_target_active) : next(us_target_active);
      if (!us_target_active) return false; /* undo limit exceeded */
    }
    /* undosys_step_iter_first: the reference is the loaded state, skip it */
    for (UndoStep *us_iter = undo_dir == STEP_UNDO ? prev(us_reference) : next(us_reference); us_iter;
         us_iter = undo_dir == STEP_UNDO ? prev(us_iter) : next(us_iter)) {
      const bool is_final = us_iter == us_target_active;
      us_iter->type->step_decode(C, us_iter, undo_dir, is_final);
      step_active = us_iter;
      if (is_final) {
        for (UndoStep *s : steps) s->is_applied = index_of(s) <= index_of(step_active);
        return true;
      }
    }
    return false;
  }

  bool undo(void *C)
  {
    if (!step_active) return false;
    UndoStep *target = prev(step_active);
    return target && load_data_ex(C, target, step_active, true);
  }
  bool redo(void *C)
  {
    if (!step_active) return false;
    UndoStep *target = next(step_active);
    return target && load_data_ex(C, target, step_active, true);
  }
  /* BKE_undosys_step_load_from_index (Edit > Undo History) */
  bool load_from_index(void *C, int index)
  {
    if (index < 0 || index >= int(steps.size()) || steps[index]->skip) return false;
    if (steps[index] == step_active) return true;
    return load_data_ex(C, steps[index], nullptr, false);
  }

  bool can_undo() const
  {
    for (UndoStep *s = step_active ? prev(step_active) : nullptr; s; s = prev(s)) if (!s->skip) return true;
    return false;
  }
  bool can_redo() const
  {
    for (UndoStep *s = step_active ? next(step_active) : nullptr; s; s = next(s)) if (!s->skip) return true;
    return false;
  }
  size_t data_size_all() const
  {
    size_t n = 0;
    for (const UndoStep *s : steps) n += s->data_size;
    return n;
  }
};

} // namespace pg_undo
