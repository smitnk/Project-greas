/* Host tests for project_grease_blender_edit5.c, linked with the real pinned BKE / BLI closure:
 * segment select (ED_gpencil_select_stroke_segment), material slot removal, onion alpha. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BKE_gpencil_legacy.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "BLI_listbase.h"

#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit5.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static bGPDstroke *line(bGPDframe *f, int mat, float x0, float y0, float x1, float y1, int n)
{
  bGPDstroke *s = BKE_gpencil_stroke_add(f, mat, n, 4, false);
  for (int i = 0; i < n; i++) {
    const float t = (float)i / (float)(n - 1);
    s->points[i].x = x0 + (x1 - x0) * t;
    s->points[i].y = y0 + (y1 - y0) * t;
    s->points[i].pressure = 1.0f;
    s->points[i].strength = 1.0f;
  }
  return s;
}
static int selected_mask(const bGPDstroke *s, char *out)
{
  int n = 0;
  for (int i = 0; i < s->totpoints; i++) {
    out[i] = (s->points[i].flag & GP_SPOINT_SELECT) ? '1' : '0';
    n += out[i] == '1';
  }
  out[s->totpoints] = 0;
  return n;
}
static bGPdata *new_doc(int materials)
{
  bGPdata *gpd = MEM_callocN(sizeof(bGPdata), "gpd");
  gpd->totcol = materials;
  gpd->mat = MEM_callocN(sizeof(Material *) * (size_t)materials, "mat");
  for (int i = 0; i < materials; i++) {
    gpd->mat[i] = MEM_callocN(sizeof(Material), "ma");
    gpd->mat[i]->gp_style = MEM_callocN(sizeof(MaterialGPencilStyle), "style");
    gpd->mat[i]->gp_style->stroke_rgba[0] = (float)i;
  }
  return gpd;
}
static void free_doc(bGPdata *gpd)
{
  BKE_gpencil_free_layers(&gpd->layers);
  for (int i = 0; i < gpd->totcol; i++) {
    MEM_freeN(gpd->mat[i]->gp_style);
    MEM_freeN(gpd->mat[i]);
  }
  MEM_SAFE_FREE(gpd->mat);
  MEM_freeN(gpd);
}

static void test_segment()
{
  bGPdata *gpd = new_doc(1);
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, 1);
  gpl->actframe = gpf;
  /* Horizontal stroke, 11 points at x = 0, 10, ..., 100, crossed by two verticals at x = 25 and 75. */
  bGPDstroke *h = line(gpf, 0, 0, 50, 100, 50, 11);
  line(gpf, 0, 25, 0, 25, 100, 2);
  line(gpf, 0, 75, 0, 75, 100, 2);
  char mask[32];
  /* Tap on the point at x = 50: only the run between the crossings (x = 30..70) is selected. */
  CHECK(pg_gp_select_segment_pick(gpd, gpl, 50, 50, 8, PG_PICK_DESELECT_ALL) == 1);
  CHECK(selected_mask(h, mask) == 5);
  CHECK(strcmp(mask, "00011111000") == 0);
  CHECK(h->flag & GP_STROKE_SELECT);
  /* Tap at x = 10: from the start up to the first crossing (x = 0..20). */
  CHECK(pg_gp_select_segment_pick(gpd, gpl, 10, 50, 8, PG_PICK_DESELECT_ALL) == 1);
  CHECK(strcmp((selected_mask(h, mask), mask), "11100000000") == 0);
  /* Tap at x = 90: from the last crossing to the end. */
  CHECK(pg_gp_select_segment_pick(gpd, gpl, 90, 50, 8, PG_PICK_DESELECT_ALL) == 1);
  CHECK(strcmp((selected_mask(h, mask), mask), "00000000111") == 0);
  /* Extend keeps the earlier segment. */
  CHECK(pg_gp_select_segment_pick(gpd, gpl, 10, 50, 8, PG_PICK_EXTEND) == 1);
  CHECK(strcmp((selected_mask(h, mask), mask), "11100000111") == 0);
  /* A stroke with no crossings: just the tapped point (Blender restores the point-pick result). */
  bGPDstroke *lone = line(gpf, 0, 0, 200, 100, 200, 6);
  CHECK(pg_gp_select_segment_pick(gpd, gpl, 40, 200, 8, PG_PICK_DESELECT_ALL) == 1);
  selected_mask(lone, mask);
  CHECK(strcmp(mask, "001000") == 0);
  CHECK(selected_mask(h, mask) == 0);
  /* Empty space with deselect-all clears everything. */
  CHECK(pg_gp_select_segment_pick(gpd, gpl, 500, 500, 8, PG_PICK_DESELECT_ALL) == 1);
  CHECK(selected_mask(lone, mask) == 0);
  free_doc(gpd);
}

static void test_material_remove()
{
  bGPdata *gpd = new_doc(3);
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *f1 = BKE_gpencil_frame_addnew(gpl, 1);
  bGPDframe *f2 = BKE_gpencil_frame_addnew(gpl, 5);
  line(f1, 0, 0, 0, 10, 0, 2);
  line(f1, 1, 0, 0, 10, 0, 2);
  line(f1, 2, 0, 0, 10, 0, 2);
  line(f2, 1, 0, 0, 10, 0, 2);
  line(f2, 2, 0, 0, 10, 0, 2);
  Material *third = gpd->mat[2];
  CHECK(pg_gp_material_slot_remove(gpd, 1) == 3); /* two strokes used slot 1 */
  CHECK(gpd->totcol == 2);
  CHECK(gpd->mat[1] == third);
  CHECK(BLI_listbase_count(&f1->strokes) == 2 && BLI_listbase_count(&f2->strokes) == 1);
  const bGPDstroke *a = f1->strokes.first, *b = a->next, *c = f2->strokes.first;
  CHECK(a->mat_nr == 0 && b->mat_nr == 1 && c->mat_nr == 1);
  CHECK(pg_gp_material_slot_remove(gpd, 5) == 0);
  CHECK(pg_gp_material_slot_remove(gpd, 0) == 2);
  CHECK(gpd->totcol == 1 && ((const bGPDstroke *)f1->strokes.first)->mat_nr == 0);
  CHECK(pg_gp_material_slot_remove(gpd, 0) == 0); /* the last slot stays */
  free_doc(gpd);
}

static void test_onion()
{
  /* gpencil_layer_final_tint_and_alpha_get(): fade 1/|id| x factor, clamped to [0.1, 1]. */
  CHECK(fabsf(pg_gp_onion_alpha(-1, 1, 0.5f) - 0.5f) < 1e-6f);
  CHECK(fabsf(pg_gp_onion_alpha(2, 1, 0.5f) - 0.25f) < 1e-6f);
  CHECK(fabsf(pg_gp_onion_alpha(-10, 1, 0.5f) - 0.1f) < 1e-6f);
  CHECK(fabsf(pg_gp_onion_alpha(3, 0, 0.5f) - 0.25f) < 1e-6f);
  CHECK(fabsf(pg_gp_onion_alpha(1, 0, 0.0f) - 0.01f) < 1e-6f);
  bGPdata *gpd = new_doc(1);
  BKE_gpencil_layer_addnew(gpd, "A", true, false);
  bGPDlayer *b = BKE_gpencil_layer_addnew(gpd, "B", true, false);
  CHECK(b->onion_flag & GP_LAYER_ONIONSKIN); /* on for new layers */
  const float off[2] = {1, 0}, fade_off[1] = {0};
  CHECK(pg_gp_edit5_dispatch(gpd, b, PG_EDIT5_CMD_ONION_LAYER, off, 2) == 1);
  CHECK(!(b->onion_flag & GP_LAYER_ONIONSKIN));
  CHECK(pg_gp_edit5_dispatch(gpd, b, PG_EDIT5_CMD_ONION_LAYER, off, 2) == 0);
  gpd->onion_flag = GP_ONION_FADE;
  CHECK(pg_gp_edit5_dispatch(gpd, b, PG_EDIT5_CMD_ONION_FADE, fade_off, 1) == 1);
  CHECK(!(gpd->onion_flag & GP_ONION_FADE));
  free_doc(gpd);
}

static void no_cache(bGPdata *gpd) { (void)gpd; }

int main(void)
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache;
  test_segment();
  test_material_remove();
  test_onion();
  if (failures) { fprintf(stderr, "%d edit5 check(s) failed\n", failures); return 1; }
  printf("edit5 tests passed\n");
  return 0;
}
