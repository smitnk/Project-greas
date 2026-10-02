/* Host tests for project_grease_blender_edit.c. The BKE_* functions below are stand-ins:
 * BKE_gpencil_stroke_delete_tagged_points() here only compacts points (the real one also splits
 * strokes), so those tests cover this module's glue, not Blender's splitting. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "project_grease_blender_edit.h"

/* ---- stand-ins ---------------------------------------------------------------- */
static int geometry_updates = 0;
static int dirty_tags = 0;
void BKE_gpencil_stroke_select_index_set(bGPdata *gpd, bGPDstroke *gps) { gps->select_index = ++gpd->select_last_index; }
void BKE_gpencil_stroke_select_index_reset(bGPDstroke *gps) { gps->select_index = 0; }
bool BKE_gpencil_stroke_select_check(const bGPDstroke *gps)
{
  for (int i = 0; i < gps->totpoints; i++) if (gps->points[i].flag & GP_SPOINT_SELECT) return true;
  return false;
}
void BKE_gpencil_stroke_sync_selection(bGPdata *gpd, bGPDstroke *gps)
{
  gps->flag &= ~GP_STROKE_SELECT;
  BKE_gpencil_stroke_select_index_reset(gps);
  for (int i = 0; i < gps->totpoints; i++) {
    if (gps->points[i].flag & GP_SPOINT_SELECT) {
      gps->flag |= GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_set(gpd, gps);
      break;
    }
  }
}
bool BKE_gpencil_layer_is_editable(const bGPDlayer *gpl)
{
  return (gpl->flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) == 0 && gpl->opacity > 0.001f;
}
void BKE_gpencil_batch_cache_dirty_tag(bGPdata *gpd) { (void)gpd; dirty_tags++; }
void BKE_gpencil_stroke_geometry_update(bGPdata *gpd, bGPDstroke *gps) { (void)gpd; (void)gps; geometry_updates++; }
void BKE_gpencil_free_stroke(bGPDstroke *gps)
{
  free(gps->points);
  free(gps);
}
bGPDstroke *BKE_gpencil_stroke_delete_tagged_points(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps,
                                                    bGPDstroke *next_stroke, int tag_flags, bool select,
                                                    bool flat_cap, int limit)
{
  (void)gpd; (void)next_stroke; (void)select; (void)flat_cap; (void)limit;
  int keep = 0;
  for (int i = 0; i < gps->totpoints; i++) {
    if (!(gps->points[i].flag & tag_flags)) gps->points[keep++] = gps->points[i];
  }
  gps->totpoints = keep;
  if (keep == 0) {
    BLI_remlink(&gpf->strokes, gps);
    BKE_gpencil_free_stroke(gps);
    return NULL;
  }
  return gps;
}
void BLI_lasso_boundbox(rcti *r, const int m[][2], unsigned int n) { (void)r; (void)m; (void)n; }
bool BLI_lasso_is_point_inside(const int m[][2], unsigned int n, int x, int y, int e) { (void)m; (void)n; (void)x; (void)y; (void)e; return false; }

/* ---- fixtures ------------------------------------------------------------------ */
static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-3f)

static void list_add(ListBase *lb, void *vlink)
{
  Link *link = vlink;
  link->next = NULL;
  link->prev = lb->last;
  if (lb->last) ((Link *)lb->last)->next = link; else lb->first = link;
  lb->last = link;
}
static bGPdata *make_gpd(void)
{
  bGPdata *gpd = calloc(1, sizeof(bGPdata));
  gpd->totcol = 2;
  gpd->mat = calloc(2, sizeof(Material *));
  for (int i = 0; i < 2; i++) {
    gpd->mat[i] = calloc(1, sizeof(Material));
    gpd->mat[i]->gp_style = calloc(1, sizeof(MaterialGPencilStyle));
  }
  gpd->mat[1]->gp_style->flag = GP_MATERIAL_LOCKED;
  return gpd;
}
static bGPDlayer *add_layer(bGPdata *gpd, int flag)
{
  bGPDlayer *gpl = calloc(1, sizeof(bGPDlayer));
  gpl->opacity = 1.0f; gpl->flag = flag;
  list_add(&gpd->layers, gpl);
  return gpl;
}
static bGPDframe *add_frame(bGPDlayer *gpl)
{
  bGPDframe *gpf = calloc(1, sizeof(bGPDframe));
  gpf->framenum = 1;
  list_add(&gpl->frames, gpf);
  gpl->actframe = gpf;
  return gpf;
}
static bGPDstroke *add_stroke(bGPDframe *gpf, int n, int mat, float x0, float y0, float dx, float dy)
{
  bGPDstroke *gps = calloc(1, sizeof(bGPDstroke));
  gps->totpoints = n; gps->mat_nr = mat;
  gps->points = calloc((size_t)n, sizeof(bGPDspoint));
  for (int i = 0; i < n; i++) { gps->points[i].x = x0 + dx * i; gps->points[i].y = y0 + dy * i; }
  list_add(&gpf->strokes, gps);
  return gps;
}
static void select_points(bGPdata *gpd, bGPDstroke *gps, unsigned mask)
{
  for (int i = 0; i < gps->totpoints; i++) {
    if (mask & (1u << i)) gps->points[i].flag |= GP_SPOINT_SELECT; else gps->points[i].flag &= ~GP_SPOINT_SELECT;
  }
  BKE_gpencil_stroke_sync_selection(gpd, gps);
}
static unsigned sel_mask(const bGPDstroke *gps)
{
  unsigned m = 0;
  for (int i = 0; i < gps->totpoints; i++) if (gps->points[i].flag & GP_SPOINT_SELECT) m |= 1u << i;
  return m;
}
static int stroke_count(const bGPDframe *gpf)
{
  int n = 0;
  for (const bGPDstroke *s = gpf->strokes.first; s; s = s->next) n++;
  return n;
}

/* ---- pick ------------------------------------------------------------------------ */
static void test_pick(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 5, 0, 0, 0, 20, 0);   /* x = 0..80, y = 0 */
  bGPDstroke *b = add_stroke(f, 5, 0, 0, 100, 20, 0); /* y = 100 */

  CHECK(pg_gp_edit_pick(gpd, NULL, 41, 3, 64, 0, 0) == 1, "pick reports a change");
  CHECK(sel_mask(a) == (1u << 2) && (a->flag & GP_STROKE_SELECT), "pick selects the nearest point (x=40) only");
  CHECK(sel_mask(b) == 0, "other strokes untouched");

  /* replacing the selection (no extend) */
  pg_gp_edit_pick(gpd, NULL, 60, 100, 64, 0, 0);
  CHECK(sel_mask(a) == 0 && !(a->flag & GP_STROKE_SELECT), "a click without extend replaces the selection");
  CHECK(sel_mask(b) == (1u << 3), "the new point is selected");

  /* extend adds */
  pg_gp_edit_pick(gpd, NULL, 20, 0, 64, PG_PICK_EXTEND, 0);
  CHECK(sel_mask(a) == (1u << 1) && sel_mask(b) == (1u << 3), "extend keeps the old selection");

  /* toggle deselects a selected point; the stroke flag follows */
  pg_gp_edit_pick(gpd, NULL, 20, 0, 64, PG_PICK_TOGGLE | PG_PICK_EXTEND, 0);
  CHECK(sel_mask(a) == 0 && !(a->flag & GP_STROKE_SELECT), "toggle deselects an already selected point");

  /* entire strokes */
  pg_gp_edit_pick(gpd, NULL, 40, 0, 64, PG_PICK_ENTIRE, 0);
  CHECK(sel_mask(a) == 31 && sel_mask(b) == 0, "entire_strokes selects the whole stroke");
  pg_gp_edit_pick(gpd, NULL, 40, 3, 64, 1 * PG_PICK_DESELECT | PG_PICK_EXTEND | PG_PICK_ENTIRE, 0);
  CHECK(sel_mask(a) == 0 && !(a->flag & GP_STROKE_SELECT), "deselect + entire clears the stroke");

  /* stroke select mode behaves like entire strokes */
  pg_gp_edit_pick(gpd, NULL, 40, 0, 64, 0, 1);
  CHECK(sel_mask(a) == 31, "stroke select mode picks the whole stroke");

  /* empty click: only deselects with deselect_all */
  CHECK(pg_gp_edit_pick(gpd, NULL, 500, 500, 64, 0, 0) == 0 && sel_mask(a) == 31, "a miss changes nothing by default");
  CHECK(pg_gp_edit_pick(gpd, NULL, 500, 500, 64, PG_PICK_DESELECT_ALL, 0) == 1 && sel_mask(a) == 0,
        "a miss with deselect_all clears the selection");

  /* passthrough keeps an existing selection when clicking part of it */
  select_points(gpd, a, 31);
  select_points(gpd, b, 31);
  CHECK(pg_gp_edit_pick(gpd, NULL, 40, 0, 64, PG_PICK_PASSTHROUGH | PG_PICK_ENTIRE, 0) == 0, "passthrough: no change");
  CHECK(sel_mask(a) == 31 && sel_mask(b) == 31, "passthrough keeps the whole selection");
  select_points(gpd, b, 0);
  CHECK(pg_gp_edit_pick(gpd, NULL, 40, 100, 64, PG_PICK_PASSTHROUGH | PG_PICK_ENTIRE, 0) == 1, "passthrough picks unselected things");
  CHECK(sel_mask(a) == 0 && sel_mask(b) == 31, "...and replaces the selection with them");

  /* the nearest point wins, Manhattan distance, inclusive radius_squared */
  pg_gp_edit_pick(gpd, NULL, 28, 0, 64, 0, 0);
  CHECK(sel_mask(a) == (1u << 1), "x=28 is nearer to x=20 than x=40");
}


static void test_pick_radius_is_strict(void)
{
  /* Blender starts hit_distance at radius_squared, so a point at exactly that Manhattan distance
   * never wins (strict '<'), while one unit closer does. */
  bGPdata *gpd = make_gpd();
  bGPDstroke *a = add_stroke(add_frame(add_layer(gpd, 0)), 1, 0, 40, 0, 0, 0);
  CHECK(pg_gp_edit_pick(gpd, NULL, 40 + 60, 4, 64, 0, 0) == 0 && sel_mask(a) == 0, "distance == radius_squared misses");
  CHECK(pg_gp_edit_pick(gpd, NULL, 40 + 59, 4, 64, 0, 0) == 1 && sel_mask(a) == 1u, "distance < radius_squared hits");
}

static void test_pick_respects_editability(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *hidden = add_layer(gpd, GP_LAYER_HIDE);
  bGPDstroke *h = add_stroke(add_frame(hidden), 3, 0, 0, 0, 10, 0);
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *locked = add_stroke(f, 3, 1, 0, 50, 10, 0);
  CHECK(pg_gp_edit_pick(gpd, NULL, 10, 0, 64, 0, 0) == 0 && sel_mask(h) == 0, "hidden layers cannot be picked");
  CHECK(pg_gp_edit_pick(gpd, NULL, 10, 50, 64, 0, 0) == 0 && sel_mask(locked) == 0, "locked-material strokes cannot be picked");
}

/* ---- transforms --------------------------------------------------------------------- */
static void test_pivot_and_transforms(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 3, 0, 0, 0, 10, 0);   /* (0,0) (10,0) (20,0) */
  bGPDstroke *b = add_stroke(f, 3, 0, 0, 20, 10, 0);  /* (0,20) (10,20) (20,20) */
  bGPDstroke *c = add_stroke(f, 2, 0, 500, 500, 10, 0);
  float px = 0, py = 0;
  CHECK(!pg_gp_edit_selection_pivot(gpd, NULL, &px, &py), "no selection -> no pivot");

  select_points(gpd, a, 7);
  select_points(gpd, b, 2);                            /* only (10,20) of stroke b */
  CHECK(pg_gp_edit_selection_pivot(gpd, NULL, &px, &py), "pivot exists");
  CHECK(NEAR(px, (0 + 10 + 20 + 10) / 4.0f) && NEAR(py, 20.0f / 4.0f), "pivot is the median of ALL selected points");

  geometry_updates = 0;
  CHECK(pg_gp_edit_translate(gpd, NULL, 5, -3) == 1, "translate reports a change");
  CHECK(NEAR(a->points[1].x, 15) && NEAR(a->points[1].y, -3), "selected stroke moves");
  CHECK(NEAR(b->points[1].x, 15) && NEAR(b->points[1].y, 17), "selected point of the second stroke moves");
  CHECK(NEAR(b->points[0].x, 0) && NEAR(b->points[0].y, 20), "unselected points do not move");
  CHECK(NEAR(c->points[0].x, 500), "unselected strokes do not move");
  CHECK(geometry_updates == 2, "geometry (triangulation, bounds) is refreshed for every moved stroke");

  /* rotation by 90 degrees about an explicit pivot (canvas formula: x' = cx + x*c - y*s) */
  select_points(gpd, a, 0); select_points(gpd, b, 0); select_points(gpd, c, 3);
  const float pivot[2] = {500, 500};
  pg_gp_edit_rotate(gpd, NULL, (float)M_PI_2, pivot);
  CHECK(NEAR(c->points[1].x, 500) && NEAR(c->points[1].y, 510), "rotate about the explicit pivot");
  /* scale about the median when no pivot is given: two points (500,500) and (500,510) -> median (500,505) */
  pg_gp_edit_scale(gpd, NULL, 2.0f, 2.0f, NULL);
  CHECK(NEAR(c->points[0].y, 495) && NEAR(c->points[1].y, 515), "scale about the selection median");
  /* mirror across the vertical line through the pivot */
  c->points[0].x = 480; c->points[0].y = 495; c->points[1].x = 520; c->points[1].y = 515;
  pg_gp_edit_mirror(gpd, NULL, 1, 0, NULL);
  CHECK(NEAR(c->points[0].x, 520) && NEAR(c->points[1].x, 480), "mirror X reflects about the median");
  CHECK(pg_gp_edit_translate(gpd, NULL, 0, 0) == 0, "a zero move changes nothing");
  CHECK(pg_gp_edit_scale(gpd, NULL, 0.0f, 1.0f, NULL) == 0, "degenerate scale is rejected");
}

static void test_transform_skips_locked_and_other_layers(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *la = add_layer(gpd, 0);
  bGPDlayer *lb = add_layer(gpd, 0);
  bGPDstroke *sa = add_stroke(add_frame(la), 2, 0, 0, 0, 10, 0);
  bGPDstroke *sb = add_stroke(add_frame(lb), 2, 0, 0, 50, 10, 0);
  bGPDstroke *locked = add_stroke(la->actframe, 2, 1, 0, 90, 10, 0);
  select_points(gpd, sa, 3); select_points(gpd, sb, 3); select_points(gpd, locked, 3);
  pg_gp_edit_translate(gpd, la, 100, 0);
  CHECK(NEAR(sa->points[0].x, 100), "scoped layer moves");
  CHECK(NEAR(sb->points[0].x, 0), "other layer is outside the scope");
  CHECK(NEAR(locked->points[0].x, 0), "locked-material strokes are never transformed");
}

/* ---- delete ------------------------------------------------------------------------------ */
static void test_delete(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 4, 0, 0, 0, 10, 0);
  bGPDstroke *b = add_stroke(f, 4, 0, 0, 20, 10, 0);
  bGPDstroke *c = add_stroke(f, 4, 0, 0, 40, 10, 0);
  select_points(gpd, a, 15);   /* whole stroke selected */
  select_points(gpd, c, 3);    /* partly selected */
  CHECK(pg_gp_edit_delete_strokes(gpd, NULL) == 1, "delete strokes reports a change");
  CHECK(stroke_count(f) == 1 && f->strokes.first == b, "every stroke with the selected flag is removed, others stay");
  CHECK(pg_gp_edit_delete_strokes(gpd, NULL) == 0, "nothing selected -> nothing to do");

  bGPdata *g2 = make_gpd();
  bGPDframe *f2 = add_frame(add_layer(g2, 0));
  bGPDstroke *d = add_stroke(f2, 5, 0, 0, 0, 10, 0);
  bGPDstroke *e = add_stroke(f2, 3, 0, 0, 20, 10, 0);
  select_points(g2, d, (1u << 1) | (1u << 2));
  select_points(g2, e, 7);
  CHECK(pg_gp_edit_delete_points(g2, NULL) == 1, "delete points reports a change");
  CHECK(d->totpoints == 3 && NEAR(d->points[0].x, 0) && NEAR(d->points[1].x, 30), "selected points are removed");
  CHECK(stroke_count(f2) == 1, "a stroke with every point selected disappears");
}

static void test_dispatch(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDstroke *a = add_stroke(add_frame(l), 3, 0, 0, 0, 10, 0);
  const float pick[] = {10, 0, 64, PG_PICK_ENTIRE, 0};
  gpd->flag &= ~GP_DATA_CACHE_IS_DIRTY; dirty_tags = 0;
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_PICK, pick, 5) == 1, "dispatch pick");
  CHECK(sel_mask(a) == 7 && (gpd->flag & GP_DATA_CACHE_IS_DIRTY) && dirty_tags > 0, "pick selected and tagged the cache");
  const float mv[] = {4, 6};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_TRANSLATE, mv, 2) == 1 && NEAR(a->points[0].x, 4), "dispatch translate");
  const float rot[] = {(float)M_PI, 0, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_ROTATE, rot, 3) == 1, "dispatch rotate with pivot");
  const float sc[] = {1.5f, 1.5f};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_SCALE, sc, 2) == 1, "dispatch scale about the median");
  const float mi[] = {0, 1};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MIRROR, mi, 2) == 1, "dispatch mirror");
  CHECK(pg_gp_edit_dispatch(gpd, l, 9999, NULL, 0) == 0, "unknown command");
  const float bad[] = {NAN, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_TRANSLATE, bad, 2) == 0, "NaN rejected");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_PICK, pick, 3) == 0, "pick needs 5 args");
  CHECK(pg_gp_edit_dispatch(NULL, l, PG_EDIT_CMD_TRANSLATE, mv, 2) == 0, "null document");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_DELETE_STROKES, NULL, 0) == 1, "dispatch delete strokes");
}


static void test_modifiers(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 3, 0, 0, 0, 10, 0);
  bGPDstroke *b = add_stroke(f, 3, 0, 0, 20, 10, 0);
  for (int i = 0; i < 3; i++) { a->points[i].pressure = b->points[i].pressure = 0.8f; a->points[i].strength = b->points[i].strength = 0.6f; }
  a->thickness = 20; a->hardeness = 0.5f; a->fill_opacity_fac = 1.0f; b->fill_opacity_fac = 1.0f;
  select_points(gpd, a, 7); /* only stroke a is selected */

  CHECK(pg_gp_mod_thickness(gpd, NULL, 0, 0, 1.5f) == 1, "thickness factor changes pressure");
  CHECK(NEAR(a->points[0].pressure, 1.2f) && NEAR(b->points[0].pressure, 0.8f), "factor multiplies pressure of selected strokes only");
  pg_gp_mod_thickness(gpd, NULL, 1, 10, 1.0f);
  CHECK(NEAR(a->points[1].pressure, 0.5f), "normalized: pressure = thickness / stroke thickness (10/20)");
  pg_gp_mod_thickness(gpd, NULL, 0, 0, -2.0f);
  CHECK(a->points[2].pressure == 0.0f, "pressure clamps at 0");

  CHECK(pg_gp_mod_opacity(gpd, NULL, PG_MODIFY_COLOR_STROKE, 0.5f, 0, 1.0f) == 1, "opacity stroke");
  CHECK(NEAR(a->points[0].strength, 0.1f) && NEAR(a->fill_opacity_fac, 1.0f), "non-normalized adds factor-1; fill untouched in stroke mode");
  pg_gp_mod_opacity(gpd, NULL, PG_MODIFY_COLOR_BOTH, 0.7f, 1, 1.0f);
  CHECK(NEAR(a->points[0].strength, 0.7f) && NEAR(a->fill_opacity_fac, 0.7f), "normalized sets strength; both mode sets fill factor");
  pg_gp_mod_opacity(gpd, NULL, PG_MODIFY_COLOR_FILL, 3.0f, 0, 1.0f);
  CHECK(NEAR(a->fill_opacity_fac, 1.0f) && NEAR(a->points[0].strength, 0.7f), "fill mode clamps fill, leaves strength");
  pg_gp_mod_opacity(gpd, NULL, PG_MODIFY_COLOR_HARDNESS, 1.0f, 0, 0.5f);
  CHECK(NEAR(a->hardeness, 0.25f), "hardness multiplies stroke hardness");
  CHECK(NEAR(b->points[0].strength, 0.6f) && NEAR(b->fill_opacity_fac, 1.0f), "unselected stroke untouched");
  CHECK(pg_gp_mod_opacity(gpd, NULL, 9, 1.0f, 0, 1.0f) == 0, "invalid mode rejected");

  const float th[] = {0, 0, 2.0f};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_THICKNESS, th, 3) == 0, "pressure 0 stays 0 -> no change on that point only");
  const float op[] = {PG_MODIFY_COLOR_STROKE, 0.9f, 1, 1.0f};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_OPACITY, op, 4) == 1 && NEAR(a->points[1].strength, 0.9f), "dispatch opacity");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_OPACITY, op, 2) == 0, "opacity needs 4 args");
}

int main(void)
{
  test_pick();
  test_pick_radius_is_strict();
  test_pick_respects_editability();
  test_pivot_and_transforms();
  test_transform_skips_locked_and_other_layers();
  test_delete();
  test_dispatch();
  test_modifiers();
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
