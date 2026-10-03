/* Host test for project_grease_blender_edit8.c (document-state commands and the document query)
 * against the real pinned DNA; built by tools/run_native_modifier_stack_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstring>

#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "MEM_guardedalloc.h"
#include "project_grease_blender_edit8.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static Material *material()
{
  Material *ma = static_cast<Material *>(MEM_callocN(sizeof(Material), "ma"));
  ma->gp_style = static_cast<MaterialGPencilStyle *>(MEM_callocN(sizeof(MaterialGPencilStyle), "st"));
  return ma;
}

static void no_cache(bGPdata *) {}

int main()
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache; /* the draw module sets it on Android */
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  gpd->onion_keytype = -1;
  bGPDlayer *a = BKE_gpencil_layer_addnew(gpd, "A", true, false);
  bGPDlayer *b = BKE_gpencil_layer_addnew(gpd, "B", true, false);
  BKE_gpencil_frame_addnew(a, 1);
  BKE_gpencil_frame_addnew(a, 5);
  BKE_gpencil_frame_addnew(b, 5);
  /* keyframe type on the active layer only, then on all layers */
  const float kt[3] = {5, BEZT_KEYTYPE_BREAKDOWN, 0};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_FRAME_KEYTYPE, kt, 3) == 1);
  float out[64];
  const float la[1] = {0};
  int n = pg_gp_doc_query(gpd, a, PG_DOC_Q_FRAMES, la, 1, out, 64);
  CHECK(n == 6 && out[0] == 1 && out[1] == 0 && out[3] == 5 && out[4] == BEZT_KEYTYPE_BREAKDOWN);
  const float lb[1] = {1};
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_FRAMES, lb, 1, out, 64);
  CHECK(n == 3 && out[1] == BEZT_KEYTYPE_KEYFRAME);
  const float kt2[3] = {5, BEZT_KEYTYPE_JITTER, 1};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_FRAME_KEYTYPE, kt2, 3) == 1);
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_FRAMES, lb, 1, out, 64);
  CHECK(out[1] == BEZT_KEYTYPE_JITTER);
  const float bad[2] = {5, 9};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_FRAME_KEYTYPE, bad, 2) == 0);
  /* frame selection: set, toggle, add; deselect */
  const float s1[2] = {1, 0};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_FRAME_SELECT, s1, 2) == 1);
  const float s5[2] = {5, 2};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_FRAME_SELECT, s5, 2) == 1);
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_FRAMES, la, 1, out, 64);
  CHECK(out[2] == 1 && out[5] == 1);
  const float t1[2] = {1, 1};
  pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_FRAME_SELECT, t1, 2);
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_FRAMES, la, 1, out, 64);
  CHECK(out[2] == 0 && out[5] == 1);
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_FRAME_DESELECT, nullptr, 0) == 1);
  /* layer blend / tint / line change / pass */
  const float bl[2] = {1, eGplBlendMode_Multiply};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_LAYER_BLEND, bl, 2) == 1 && b->blend_mode == eGplBlendMode_Multiply);
  const float tint[5] = {-1, 1, 0, 0, 0.5f};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_LAYER_TINT, tint, 5) == 1 && a->tintcolor[0] == 1 && a->tintcolor[3] == 0.5f);
  const float lc[2] = {0, 12};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_LAYER_LINE, lc, 2) == 1 && a->line_change == 12);
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_LAYER, la, 1, out, 64);
  CHECK(n == 7 && out[1] == 1 && out[4] == 0.5f && out[5] == 12);
  /* materials: move remaps strokes, flags, isolate, mode */
  gpd->totcol = 3;
  gpd->mat = static_cast<Material **>(MEM_callocN(sizeof(Material *) * 3, "mats"));
  for (int i = 0; i < 3; i++) gpd->mat[i] = material();
  Material *m0 = gpd->mat[0], *m2 = gpd->mat[2];
  bGPDstroke *s0 = BKE_gpencil_stroke_add(static_cast<bGPDframe *>(a->frames.first), 0, 2, 3, false);
  bGPDstroke *s2 = BKE_gpencil_stroke_add(static_cast<bGPDframe *>(a->frames.first), 2, 2, 3, false);
  const float mv[2] = {0, 2};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_MATERIAL_MOVE, mv, 2) == 1);
  CHECK(gpd->mat[2] == m0 && gpd->mat[1] == m2 && s0->mat_nr == 2 && s2->mat_nr == 1);
  const float fl[3] = {1, 1, 0};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_MATERIAL_FLAGS, fl, 3) == 1 && (m2->gp_style->flag & GP_MATERIAL_LOCKED));
  const float solo[1] = {0};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_MATERIAL_SOLO, solo, 1) == 1);
  CHECK((gpd->mat[1]->gp_style->flag & GP_MATERIAL_HIDE) && (gpd->mat[2]->gp_style->flag & GP_MATERIAL_HIDE));
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_MATERIAL_SOLO, solo, 1) == 1 && !(gpd->mat[1]->gp_style->flag & GP_MATERIAL_HIDE));
  const float md[4] = {1, GP_MATERIAL_MODE_DOT, GP_MATERIAL_FOLLOW_FIXED, 0.5f};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_MATERIAL_MODE, md, 4) == 1);
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_MATERIAL, la + 0, 0, out, 64);
  CHECK(n == -1);
  const float q1[1] = {1};
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_MATERIAL, q1, 1, out, 64);
  CHECK(n == 6 && out[0] == GP_MATERIAL_MODE_DOT && out[1] == GP_MATERIAL_FOLLOW_FIXED && out[2] == 0.5f && out[3] == 1);
  /* onion filter + loop */
  const float on[2] = {BEZT_KEYTYPE_BREAKDOWN, 1};
  CHECK(pg_gp_edit8_dispatch(gpd, a, PG_EDIT8_CMD_ONION_FILTER, on, 2) == 1);
  n = pg_gp_doc_query(gpd, a, PG_DOC_Q_ONION, nullptr, 0, out, 64);
  CHECK(n == 3 && out[0] == BEZT_KEYTYPE_BREAKDOWN && out[1] == 1);
  CHECK(pg_gp_doc_query(gpd, a, PG_DOC_Q_ONION, nullptr, 0, nullptr, 0) == 3); /* count only */
  printf(failures ? "%d FAILURES\n" : "edit8 tests passed\n", failures);
  return failures ? 1 : 0;
}
