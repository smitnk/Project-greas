/* L6 parity test: Project Grease's evaluated frame (pg_mod_eval_frame) must carry the same
 * evaluated-to-original links as Blender 3.6.23's copy-on-write (frame copy, then
 * BKE_gpencil_frame_original_pointers_update), with no modifier and after a deform modifier.
 * Blender's draw cache dims every point whose runtime.pt_orig is NULL (GP_EDIT_POINT_DIMMED). */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "DNA_gpencil_legacy_types.h"
#include "MEM_guardedalloc.h"
#include "project_grease_modifier_stack.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); failures++; } } while (0)

static void add_stroke(bGPDframe *gpf, int n, float y)
{
  bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, 0, n, 10, false);
  for (int i = 0; i < n; i++) {
    gps->points[i].x = 12.0f * i;
    gps->points[i].y = y + 5.0f * std::sin(0.7f * i);
    gps->points[i].pressure = 0.6f;
    gps->points[i].strength = 1.0f;
  }
}

/* Every evaluated stroke / point links to the same original as Blender's reference copy. */
static void compare(const char *what, const bGPDframe *ref, const bGPDframe *pg)
{
  CHECK(pg->runtime.gpf_orig == ref->runtime.gpf_orig, "%s: gpf_orig", what);
  const bGPDstroke *a = (const bGPDstroke *)ref->strokes.first;
  const bGPDstroke *b = (const bGPDstroke *)pg->strokes.first;
  int s = 0;
  for (; a && b; a = a->next, b = b->next, s++) {
    CHECK(b->runtime.gps_orig == a->runtime.gps_orig && b->runtime.gps_orig, "%s: stroke %d gps_orig", what, s);
    CHECK(b->totpoints == a->totpoints, "%s: stroke %d points", what, s);
    for (int i = 0; i < a->totpoints && i < b->totpoints; i++) {
      CHECK(b->points[i].runtime.pt_orig == a->points[i].runtime.pt_orig && b->points[i].runtime.pt_orig,
            "%s: stroke %d point %d pt_orig", what, s, i);
      CHECK(b->points[i].runtime.idx_orig == a->points[i].runtime.idx_orig, "%s: stroke %d point %d idx_orig", what, s, i);
    }
  }
  CHECK(!a && !b, "%s: stroke count", what);
}

int main()
{
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "Layer", true, false);
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, 1);
  add_stroke(gpf, 9, 10.0f);
  add_stroke(gpf, 5, 60.0f);
  add_stroke(gpf, 2, 90.0f);

  /* Blender: COW copy of the frame, then the original-pointer pass. */
  bGPDframe *ref = BKE_gpencil_frame_duplicate(gpf, true);
  ref->runtime.gpf_orig = gpf; /* BKE_gpencil_layer_original_pointers_update */
  BKE_gpencil_frame_original_pointers_update(gpf, ref);

  bGPDframe ev;
  pg_mod_eval_frame(gpd, gpl, gpf, nullptr, 0, 1, &ev);
  compare("no modifier", ref, &ev);
  pg_mod_eval_free(&ev);

  /* A deform modifier keeps the point arrays, so the links survive it in Blender too. */
  for (int type : {PG_MOD_NOISE, PG_MOD_SMOOTH, PG_MOD_THICKNESS, PG_MOD_OFFSET}) {
    PGModEntry e;
    std::memset(&e, 0, sizeof(e));
    pg_mod_entry_init(&e, type);
    pg_mod_eval_frame(gpd, gpl, gpf, &e, 1, 1, &ev);
    char what[48];
    std::snprintf(what, sizeof(what), "deform type %d", type);
    compare(what, ref, &ev);
    pg_mod_eval_free(&ev);
  }

  BKE_gpencil_free_strokes(ref);
  MEM_freeN(ref);
  BKE_gpencil_free_layers(&gpd->layers);
  MEM_freeN(gpd);
  std::printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
