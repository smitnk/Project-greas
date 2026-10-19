/* Host test for the Blender ports of GPENCIL_OT_extrude / duplicate / dissolve / stroke_split /
 * stroke_arrange / stroke_separate against the real pinned DNA and BKE; built by
 * tools/run_native_modifier_stack_tests.sh. */
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "MEM_guardedalloc.h"
#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit2.h"
#include "project_grease_blender_edit4.h"
#include "project_grease_blender_edit10.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void no_cache(bGPdata *) {}

static bGPDstroke *add_stroke(bGPDframe *f, int n, float x0, unsigned sel)
{
  bGPDstroke *gps = static_cast<bGPDstroke *>(MEM_callocN(sizeof(bGPDstroke), "s"));
  gps->totpoints = n;
  gps->thickness = 10;
  gps->points = static_cast<bGPDspoint *>(MEM_callocN(sizeof(bGPDspoint) * n, "p"));
  for (int i = 0; i < n; i++) {
    gps->points[i].x = x0 + 10.0f * i;
    gps->points[i].pressure = 1.0f;
    gps->points[i].strength = 1.0f;
    if (sel & (1u << i)) gps->points[i].flag |= GP_SPOINT_SELECT;
  }
  if (sel) gps->flag |= GP_STROKE_SELECT;
  BLI_addtail(&f->strokes, gps);
  return gps;
}
static unsigned mask(const bGPDstroke *gps)
{
  unsigned m = 0;
  for (int i = 0; i < gps->totpoints; i++) if (gps->points[i].flag & GP_SPOINT_SELECT) m |= 1u << i;
  return m;
}
static int count(const bGPDframe *f) { return BLI_listbase_count(&f->strokes); }
static bGPDstroke *nth(bGPDframe *f, int i) { return static_cast<bGPDstroke *>(BLI_findlink(&f->strokes, i)); }

struct Doc { bGPdata *gpd; bGPDlayer *gpl; bGPDframe *gpf; };
static Doc doc()
{
  Doc d;
  d.gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  d.gpd->onion_keytype = -1;
  d.gpl = BKE_gpencil_layer_addnew(d.gpd, "L", true, false);
  d.gpf = BKE_gpencil_frame_addnew(d.gpl, 1);
  d.gpl->actframe = d.gpf;
  return d;
}

int main()
{
  BKE_gpencil_batch_cache_dirty_tag_cb = no_cache;

  /* extrude an inner point: a new 2-point stroke branches from it, flipped so the extruded
   * (selected) point is last; the source keeps its points, inner point deselected, and the
   * source stroke (no end selected) is deselected. */
  {
    Doc d = doc();
    bGPDstroke *a = add_stroke(d.gpf, 5, 0, 1u << 2);
    CHECK(pg_gp_extrude(d.gpd, nullptr) == 1);
    CHECK(count(d.gpf) == 2);
    CHECK(a->totpoints == 5 && mask(a) == 0 && !(a->flag & GP_STROKE_SELECT));
    bGPDstroke *b = a->next;
    CHECK(b && b->totpoints == 2 && mask(b) == 2u && (b->flag & GP_STROKE_SELECT));
    CHECK(b->points[0].x == 20.0f && b->points[1].x == 20.0f);
    /* extrude again extends the branch from its selected end */
    CHECK(pg_gp_extrude(d.gpd, nullptr) == 1 && count(d.gpf) == 2 && b->totpoints == 3 && mask(b) == 4u);
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* extrude both ends plus an inner point; weights carried */
  {
    Doc d = doc();
    bGPDstroke *a = add_stroke(d.gpf, 4, 0, (1u << 0) | (1u << 1) | (1u << 3));
    a->dvert = static_cast<MDeformVert *>(MEM_callocN(sizeof(MDeformVert) * 4, "dv"));
    for (int i = 0; i < 4; i++) {
      a->dvert[i].totweight = 1;
      a->dvert[i].dw = static_cast<MDeformWeight *>(MEM_callocN(sizeof(MDeformWeight), "dw"));
      a->dvert[i].dw->weight = 0.1f * i;
    }
    CHECK(pg_gp_extrude(d.gpd, nullptr) == 1);
    CHECK(a->totpoints == 6 && mask(a) == ((1u << 0) | (1u << 5)) && (a->flag & GP_STROKE_SELECT));
    CHECK(a->dvert[0].dw != a->dvert[1].dw && std::fabs(a->dvert[5].dw->weight - 0.3f) < 1e-6f);
    bGPDstroke *b = a->next;
    CHECK(b && b->totpoints == 2 && b->points[1].x == 10.0f && mask(b) == 2u);
    CHECK(b->dvert && std::fabs(b->dvert[1].dw->weight - 0.1f) < 1e-6f);
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* duplicate: copies appended at the frame end, selected; original deselected; active frame
   * only even in multi-frame edit */
  {
    Doc d = doc();
    bGPDstroke *a = add_stroke(d.gpf, 5, 0, (1u << 0) | (1u << 1) | (1u << 3));
    bGPDstroke *u = add_stroke(d.gpf, 2, 100, 0);
    bGPDframe *f2 = BKE_gpencil_frame_addnew(d.gpl, 5);
    f2->flag |= GP_FRAME_SELECT;
    d.gpl->actframe = d.gpf;
    add_stroke(f2, 2, 0, 3);
    d.gpd->flag |= GP_DATA_STROKE_MULTIEDIT;
    CHECK(pg_gp_duplicate(d.gpd, nullptr) == 1);
    CHECK(count(d.gpf) == 4 && a->next == u && count(f2) == 1);
    CHECK(mask(a) == 0 && !(a->flag & GP_STROKE_SELECT));
    CHECK(nth(d.gpf, 2)->totpoints == 2 && mask(nth(d.gpf, 2)) == 3u && (nth(d.gpf, 2)->flag & GP_STROKE_SELECT));
    CHECK(nth(d.gpf, 3)->totpoints == 1 && nth(d.gpf, 3)->points[0].x == 30.0f);
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* dissolve: remaining points and the stroke end up deselected; UNSELECT with no selected point
   * deletes the selected stroke */
  {
    Doc d = doc();
    bGPDstroke *a = add_stroke(d.gpf, 5, 0, (1u << 0) | (1u << 3));
    CHECK(pg_gp_dissolve(d.gpd, nullptr, PG_DISSOLVE_BETWEEN) == 1);
    CHECK(a->totpoints == 3 && a->points[1].x == 30.0f && mask(a) == 0 && !(a->flag & GP_STROKE_SELECT));
    a->flag |= GP_STROKE_SELECT;
    CHECK(pg_gp_dissolve(d.gpd, nullptr, PG_DISSOLVE_UNSELECT) == 1 && count(d.gpf) == 0);
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* split: the selected run becomes a new stroke at the frame end with its points selected; the
   * original keeps the rest, unselected */
  {
    Doc d = doc();
    bGPDstroke *a = add_stroke(d.gpf, 5, 0, (1u << 1) | (1u << 2));
    add_stroke(d.gpf, 2, 100, 0);
    CHECK(pg_gp_split(d.gpd, nullptr) == 1);
    CHECK(count(d.gpf) == 4);
    bGPDstroke *s0 = nth(d.gpf, 0), *s1 = nth(d.gpf, 1), *last = nth(d.gpf, 3);
    (void)a;
    CHECK(s0->totpoints == 1 && mask(s0) == 0 && s1->totpoints == 2 && s1->points[0].x == 30.0f && mask(s1) == 0);
    CHECK(last->totpoints == 2 && last->points[0].x == 10.0f && mask(last) == 3u);
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* arrange: a selected stroke already on top locks, others move below it */
  {
    Doc d = doc();
    bGPDstroke *a = add_stroke(d.gpf, 2, 0, 3);
    bGPDstroke *b = add_stroke(d.gpf, 2, 0, 0);
    bGPDstroke *c = add_stroke(d.gpf, 2, 0, 3);
    CHECK(pg_gp_stroke_arrange(d.gpd, nullptr, PG_ARRANGE_UP) == 1); /* c locked on top, a moves */
    CHECK(nth(d.gpf, 0) == b && nth(d.gpf, 1) == a && nth(d.gpf, 2) == c);
    CHECK(pg_gp_stroke_arrange(d.gpd, nullptr, PG_ARRANGE_UP) == 0); /* a is right below the locked c */
    c->flag &= ~GP_STROKE_SELECT;
    CHECK(pg_gp_stroke_arrange(d.gpd, nullptr, PG_ARRANGE_UP) == 1 && nth(d.gpf, 2) == a);
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* separate selected points into a new layer named after the source */
  {
    Doc d = doc();
    bGPDstroke *a = add_stroke(d.gpf, 4, 0, (1u << 2) | (1u << 3));
    const float args[1] = {PG_SEPARATE_POINT};
    CHECK(pg_gp_edit10_dispatch(d.gpd, d.gpl, PG_EDIT10_CMD_SEPARATE, args, 1) == 1);
    (void)a;
    bGPDlayer *dst = static_cast<bGPDlayer *>(d.gpd->layers.last);
    CHECK(dst != d.gpl && std::strncmp(dst->info, "L", 1) == 0);
    bGPDframe *df = static_cast<bGPDframe *>(dst->frames.first);
    CHECK(df && df->framenum == 1 && count(df) == 1 && nth(df, 0)->totpoints == 2 && nth(df, 0)->points[0].x == 20.0f);
    CHECK(count(d.gpf) == 1 && nth(d.gpf, 0)->totpoints == 2 && mask(nth(d.gpf, 0)) == 0);
    CHECK(nth(d.gpf, 0)->flag & GP_STROKE_SELECT); /* source fragments keep the stroke flag, as in Blender */
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* Cutter: a lasso around the middle of a horizontal line crossed by a vertical one cuts the
   * middle away; the pieces left end at the cut, nothing remains inside the lasso. */
  {
    Doc d = doc();
    bGPDstroke *h = add_stroke(d.gpf, 11, 0.0f, 0); /* x 0..100, y 0 */
    bGPDstroke *v = add_stroke(d.gpf, 5, 0.0f, 0);
    for (int i = 0; i < 5; i++) { v->points[i].x = 55.0f; v->points[i].y = -20.0f + 10.0f * i; }
    (void)h;
    const int lasso[4][2] = {{42, -6}, {68, -6}, {68, 6}, {42, 6}};
    CHECK(pg_gp_cutter(d.gpd, lasso, 4, 0) == 1);
    int pieces = 0;
    LISTBASE_FOREACH (bGPDstroke *, s, &d.gpf->strokes) {
      bool horizontal = s->totpoints > 0 && std::fabs(s->points[0].y) < 1e-3f && std::fabs(s->points[s->totpoints - 1].y) < 1e-3f;
      if (!horizontal) continue;
      pieces++;
    }
    printf("  cutter: %d horizontal pieces\n", pieces);
    /* gpencil_cutter_lasso_select: the segment of the first point inside (x 50) runs from the free
     * start to the crossing at x 55, where a point is inserted; the rest of the line stays. */
    bGPDstroke *rest = nth(d.gpf, 0);
    CHECK(pieces == 1 && rest->totpoints == 6 && std::fabs(rest->points[0].x - 55.0f) < 1.0f &&
          std::fabs(rest->points[5].x - 100.0f) < 1e-3f);
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  /* Log 22: a stroke drawn bottom to top (y 100 -> 0, canvas y down) crossed in the middle; a lasso
   * around the upper end only must cut the upper piece (y 0..50), not the lower one. */
  {
    Doc d = doc();
    bGPDstroke *v = add_stroke(d.gpf, 11, 0.0f, 0);
    for (int i = 0; i < 11; i++) { v->points[i].x = 50.0f; v->points[i].y = 100.0f - 10.0f * i; }
    bGPDstroke *h = add_stroke(d.gpf, 11, 0.0f, 0);
    for (int i = 0; i < 11; i++) { h->points[i].x = 10.0f * i; h->points[i].y = 52.0f; }
    const int lasso[4][2] = {{40, -5}, {60, -5}, {60, 20}, {40, 20}};
    CHECK(pg_gp_cutter(d.gpd, lasso, 4, 0) == 1);
    float miny = 1e9f, maxy = -1e9f;
    LISTBASE_FOREACH (bGPDstroke *, s, &d.gpf->strokes) {
      if (s->totpoints < 1 || std::fabs(s->points[0].x - 50.0f) > 1e-3f) continue; /* the vertical one */
      for (int i = 0; i < s->totpoints; i++) { miny = std::min(miny, s->points[i].y); maxy = std::max(maxy, s->points[i].y); }
    }
    printf("  cutter upper lasso: vertical stroke left y %g..%g\n", miny, maxy);
    CHECK(maxy > 99.0f && miny > 45.0f); /* the lower piece (y 52..100) stays */
    BKE_gpencil_free_layers(&d.gpd->layers);
    MEM_freeN(d.gpd);
  }
  if (failures) {
    printf("%d edit10 failures\n", failures);
    return 1;
  }
  printf("edit10 tests passed\n");
  return 0;
}
