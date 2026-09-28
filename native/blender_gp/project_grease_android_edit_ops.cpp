#include "project_grease_android_edit_ops.h"

#include <cstring>

#include "MEM_guardedalloc.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"

static void copy_point(bGPDspoint *dst, const bGPDspoint *src)
{
  std::memcpy(static_cast<void *>(dst), static_cast<const void *>(src), sizeof(bGPDspoint));
  std::memset(static_cast<void *>(&dst->runtime), 0, sizeof(dst->runtime));
}

static void interpolate_point(
    bGPDspoint *out, const bGPDspoint *a, const bGPDspoint *b)
{
  copy_point(out, a);
  out->x = (a->x + b->x) * 0.5f;
  out->y = (a->y + b->y) * 0.5f;
  out->z = (a->z + b->z) * 0.5f;
  out->pressure = (a->pressure + b->pressure) * 0.5f;
  out->strength = (a->strength + b->strength) * 0.5f;
  out->time = (a->time + b->time) * 0.5f;
  out->uv_fac = (a->uv_fac + b->uv_fac) * 0.5f;
  out->uv_rot = (a->uv_rot + b->uv_rot) * 0.5f;
  for (int i = 0; i < 2; ++i) {
    out->uv_fill[i] = (a->uv_fill[i] + b->uv_fill[i]) * 0.5f;
  }
  for (int i = 0; i < 4; ++i) {
    out->vert_color[i] = (a->vert_color[i] + b->vert_color[i]) * 0.5f;
  }
  out->flag = a->flag & b->flag;
  std::memset(static_cast<void *>(&out->runtime), 0, sizeof(out->runtime));
}

static void invalidate_geometry(bGPDstroke *stroke)
{
  if (!stroke) {
    return;
  }

  /*
   * The Android GP closure deliberately does not depend on Blender's full
   * legacy BKE geometry-update API. The focused presentation path rebuilds
   * its geometry from the point array, so invalidate the owned per-stroke
   * geometry directly and clear runtime state.
   */
  MEM_SAFE_FREE(stroke->triangles);
  stroke->tot_triangles = 0;
  std::memset(static_cast<void *>(&stroke->runtime), 0, sizeof(stroke->runtime));
  stroke->_pad5 = nullptr;
}

static bool supported_edit_stroke(const bGPDstroke *stroke)
{
  /* Project Grease Android currently has no native vertex-weight or Bezier
   * edit-curve ownership path. Refuse destructive point-array edits until
   * those owned substructures are implemented rather than corrupting them. */
  return stroke && stroke->dvert == nullptr && stroke->editcurve == nullptr;
}

extern "C" bool project_grease_android_stroke_flip(bGPDstroke *stroke)
{
  if (!supported_edit_stroke(stroke) || stroke->totpoints < 2 || !stroke->points) {
    return false;
  }

  for (int i = 0, j = stroke->totpoints - 1; i < j; ++i, --j) {
    unsigned char tmp[sizeof(bGPDspoint)];
    std::memcpy(tmp, static_cast<const void *>(&stroke->points[i]), sizeof(tmp));
    std::memcpy(static_cast<void *>(&stroke->points[i]),
                static_cast<const void *>(&stroke->points[j]),
                sizeof(tmp));
    std::memcpy(static_cast<void *>(&stroke->points[j]), tmp, sizeof(tmp));
  }

  for (int i = 0; i < stroke->totpoints; ++i) {
    std::memset(static_cast<void *>(&stroke->points[i].runtime),
                0,
                sizeof(stroke->points[i].runtime));
  }

  invalidate_geometry(stroke);
  return true;
}

extern "C" bool project_grease_android_stroke_subdivide(bGPDstroke *stroke, int level)
{
  if (!supported_edit_stroke(stroke) || !stroke->points ||
      stroke->totpoints < 2 || level <= 0) {
    return false;
  }

  for (int pass = 0; pass < level; ++pass) {
    const int old_count = stroke->totpoints;
    const bool cyclic = (stroke->flag & GP_STROKE_CYCLIC) != 0;
    const int segment_count = cyclic ? old_count : old_count - 1;
    const int new_count = old_count + segment_count;

    bGPDspoint *new_points = static_cast<bGPDspoint *>(
        MEM_mallocN(sizeof(bGPDspoint) * new_count, "Project Grease subdivide"));
    if (!new_points) {
      return false;
    }

    int dst = 0;
    for (int i = 0; i < old_count; ++i) {
      copy_point(&new_points[dst++], &stroke->points[i]);
      if (i < old_count - 1 || cyclic) {
        const int next = (i + 1) % old_count;
        interpolate_point(
            &new_points[dst++], &stroke->points[i], &stroke->points[next]);
      }
    }

    MEM_freeN(stroke->points);
    stroke->points = new_points;
    stroke->totpoints = new_count;
  }

  invalidate_geometry(stroke);
  return true;
}

extern "C" bool project_grease_android_stroke_close(bGPDstroke *stroke)
{
  if (!supported_edit_stroke(stroke) || !stroke->points || stroke->totpoints < 3) {
    return false;
  }

  stroke->flag |= GP_STROKE_CYCLIC;
  invalidate_geometry(stroke);
  return true;
}

extern "C" bool project_grease_android_stroke_trim_points(
    bGPDstroke *stroke, int index_from, int index_to, bool keep_single_point)
{
  if (!supported_edit_stroke(stroke) || !stroke->points || index_from < 0 ||
      index_to < index_from || index_to >= stroke->totpoints) {
    return false;
  }

  const int new_count = index_to - index_from + 1;
  if (new_count == 1 && !keep_single_point) {
    return false;
  }

  bGPDspoint *new_points = static_cast<bGPDspoint *>(
      MEM_mallocN(sizeof(bGPDspoint) * new_count, "Project Grease trim"));
  if (!new_points) {
    return false;
  }

  for (int i = 0; i < new_count; ++i) {
    copy_point(&new_points[i], &stroke->points[index_from + i]);
  }

  MEM_freeN(stroke->points);
  stroke->points = new_points;
  stroke->totpoints = new_count;
  stroke->flag &= ~GP_STROKE_CYCLIC;
  invalidate_geometry(stroke);
  return true;
}

extern "C" bool project_grease_android_stroke_split(
    bGPDframe *frame, bGPDstroke *stroke, int before_index, bGPDstroke **remaining)
{
  if (!frame || !supported_edit_stroke(stroke) || !stroke->points || !remaining ||
      before_index <= 0 || before_index >= stroke->totpoints) {
    return false;
  }

  const int first_count = before_index;
  const int second_count = stroke->totpoints - before_index;

  bGPDstroke *tail = static_cast<bGPDstroke *>(
      MEM_callocN(sizeof(bGPDstroke), "Project Grease split stroke"));
  if (!tail) {
    return false;
  }

  std::memcpy(static_cast<void *>(tail),
              static_cast<const void *>(stroke),
              sizeof(bGPDstroke));
  tail->next = nullptr;
  tail->prev = nullptr;
  tail->points = nullptr;
  tail->triangles = nullptr;
  tail->tot_triangles = 0;
  tail->dvert = nullptr;
  tail->editcurve = nullptr;
  std::memset(static_cast<void *>(&tail->runtime), 0, sizeof(tail->runtime));
  tail->_pad5 = nullptr;
  tail->totpoints = second_count;
  tail->points = static_cast<bGPDspoint *>(
      MEM_mallocN(sizeof(bGPDspoint) * second_count, "Project Grease split points"));
  if (!tail->points) {
    MEM_freeN(tail);
    return false;
  }

  for (int i = 0; i < second_count; ++i) {
    copy_point(&tail->points[i], &stroke->points[before_index + i]);
  }

  bGPDspoint *head_points = static_cast<bGPDspoint *>(
      MEM_mallocN(sizeof(bGPDspoint) * first_count, "Project Grease split head"));
  if (!head_points) {
    MEM_freeN(tail->points);
    MEM_freeN(tail);
    return false;
  }

  for (int i = 0; i < first_count; ++i) {
    copy_point(&head_points[i], &stroke->points[i]);
  }

  MEM_freeN(stroke->points);
  stroke->points = head_points;
  stroke->totpoints = first_count;
  stroke->flag &= ~GP_STROKE_CYCLIC;
  invalidate_geometry(stroke);

  tail->flag &= ~GP_STROKE_CYCLIC;
  invalidate_geometry(tail);

  BLI_insertlinkafter(&frame->strokes, stroke, tail);
  *remaining = tail;
  return true;
}
