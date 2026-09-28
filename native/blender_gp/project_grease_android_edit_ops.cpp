#include "project_grease_android_edit_ops.h"

#include <cstring>

#include "MEM_guardedalloc.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"

static bGPDspoint interpolate_point(const bGPDspoint &a, const bGPDspoint &b)
{
  bGPDspoint p{};
  std::memcpy(&p, &a, sizeof(p));
  p.x = (a.x + b.x) * 0.5f;
  p.y = (a.y + b.y) * 0.5f;
  p.z = (a.z + b.z) * 0.5f;
  p.pressure = (a.pressure + b.pressure) * 0.5f;
  p.strength = (a.strength + b.strength) * 0.5f;
  p.time = (a.time + b.time) * 0.5f;
  p.flag = a.flag & b.flag;
  p.runtime.pt_orig = nullptr;
  p.runtime.idx_orig = -1;
  return p;
}

extern "C" void project_grease_android_stroke_flip(bGPDstroke *stroke)
{
  if (!stroke || stroke->totpoints < 2 || !stroke->points) {
    return;
  }
  for (int i = 0, j = stroke->totpoints - 1; i < j; ++i, --j) {
    bGPDspoint tmp{};
    std::memcpy(&tmp, &stroke->points[i], sizeof(tmp));
    std::memcpy(&stroke->points[i], &stroke->points[j], sizeof(tmp));
    std::memcpy(&stroke->points[j], &tmp, sizeof(tmp));
  }
}

extern "C" bool project_grease_android_stroke_subdivide(bGPDstroke *stroke, int level)
{
  if (!stroke || !stroke->points || stroke->totpoints < 2 || level <= 0) {
    return false;
  }

  for (int pass = 0; pass < level; ++pass) {
    const int old_count = stroke->totpoints;
    const bool cyclic = (stroke->flag & GP_STROKE_CYCLIC) != 0;
    const int segment_count = cyclic ? old_count : old_count - 1;
    const int new_count = old_count + segment_count;
    bGPDspoint *new_points = static_cast<bGPDspoint *>(
        MEM_mallocN(sizeof(bGPDspoint) * new_count, "Project Grease subdivide"));

    int dst = 0;
    for (int i = 0; i < old_count; ++i) {
      std::memcpy(&new_points[dst++], &stroke->points[i], sizeof(bGPDspoint));
      if (i < old_count - 1 || cyclic) {
        const int next = (i + 1) % old_count;
        const bGPDspoint midpoint =
            interpolate_point(stroke->points[i], stroke->points[next]);
        std::memcpy(&new_points[dst++], &midpoint, sizeof(midpoint));
      }
    }

    MEM_freeN(stroke->points);
    stroke->points = new_points;
    stroke->totpoints = new_count;
  }
  return true;
}

extern "C" bool project_grease_android_stroke_close(bGPDstroke *stroke)
{
  if (!stroke || !stroke->points || stroke->totpoints < 3) {
    return false;
  }
  stroke->flag |= GP_STROKE_CYCLIC;
  return true;
}

extern "C" bool project_grease_android_stroke_trim_points(
    bGPDstroke *stroke, int index_from, int index_to, bool keep_single_point)
{
  if (!stroke || !stroke->points || index_from < 0 ||
      index_to < index_from || index_to >= stroke->totpoints) {
    return false;
  }

  const int new_count = index_to - index_from + 1;
  if (new_count == 1 && !keep_single_point) {
    return false;
  }

  bGPDspoint *new_points = static_cast<bGPDspoint *>(
      MEM_mallocN(sizeof(bGPDspoint) * new_count, "Project Grease trim"));
  std::memcpy(new_points,
              stroke->points + index_from,
              sizeof(bGPDspoint) * new_count);
  MEM_freeN(stroke->points);
  stroke->points = new_points;
  stroke->totpoints = new_count;
  stroke->flag &= ~GP_STROKE_CYCLIC;
  return true;
}

extern "C" bool project_grease_android_stroke_split(
    bGPDframe *frame, bGPDstroke *stroke, int before_index, bGPDstroke **remaining)
{
  if (!frame || !stroke || !stroke->points || !remaining ||
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

  std::memcpy(tail, stroke, sizeof(bGPDstroke));
  tail->next = nullptr;
  tail->prev = nullptr;
  tail->points = nullptr;
  tail->triangles = nullptr;
  tail->dvert = nullptr;
  tail->editcurve = nullptr;
  tail->runtime.gps_orig = nullptr;
  tail->_pad5 = nullptr;
  tail->totpoints = second_count;
  tail->points = static_cast<bGPDspoint *>(
      MEM_mallocN(sizeof(bGPDspoint) * second_count, "Project Grease split points"));
  if (!tail->points) {
    MEM_freeN(tail);
    return false;
  }

  std::memcpy(tail->points,
              stroke->points + before_index,
              sizeof(bGPDspoint) * second_count);

  bGPDspoint *head_points = static_cast<bGPDspoint *>(
      MEM_mallocN(sizeof(bGPDspoint) * first_count, "Project Grease split head"));
  if (!head_points) {
    MEM_freeN(tail->points);
    MEM_freeN(tail);
    return false;
  }
  std::memcpy(head_points, stroke->points, sizeof(bGPDspoint) * first_count);

  MEM_freeN(stroke->points);
  stroke->points = head_points;
  stroke->totpoints = first_count;
  stroke->flag &= ~GP_STROKE_CYCLIC;
  tail->flag &= ~GP_STROKE_CYCLIC;

  BLI_insertlinkafter(&frame->strokes, stroke, tail);
  *remaining = tail;
  return true;
}
