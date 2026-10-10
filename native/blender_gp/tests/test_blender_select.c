/* Host tests for project_grease_blender_select.c.
 *
 * Expected values come from reading Blender 3.6.23 gpencil_select.c,
 * ED_gpencil_select_toggle_all() (gpencil_utils.c) and select_utils.c.
 *
 * The BKE_* functions at the top are test stand-ins for the real
 * blenkernel helpers (production links the real ones). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BKE_gpencil_legacy.h"
#include "BLI_lasso_2d.h"
#include "project_grease_blender_select.h"

/* ---- stand-ins for the real blenkernel / blenlib helpers ------------------ */
void BKE_gpencil_stroke_select_index_set(bGPdata *gpd, bGPDstroke *gps)
{
  gpd->select_last_index++;
  gps->select_index = gpd->select_last_index;
}
void BKE_gpencil_stroke_select_index_reset(bGPDstroke *gps)
{
  gps->select_index = 0;
}
bool BKE_gpencil_stroke_select_check(const bGPDstroke *gps)
{
  for (int i = 0; i < gps->totpoints; i++) {
    if (gps->points[i].flag & GP_SPOINT_SELECT) {
      return true;
    }
  }
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
  if ((gpl->flag & (GP_LAYER_HIDE | GP_LAYER_LOCKED)) == 0) {
    return gpl->opacity > 0.001f;
  }
  return false;
}
static int dirty_tags = 0;
void BKE_gpencil_batch_cache_dirty_tag(bGPdata *gpd)
{
  (void)gpd;
  dirty_tags++;
}
void BLI_lasso_boundbox(rcti *rect, const int mcoords[][2], unsigned int len)
{
  rect->xmin = rect->xmax = mcoords[0][0];
  rect->ymin = rect->ymax = mcoords[0][1];
  for (unsigned int i = 1; i < len; i++) {
    if (mcoords[i][0] < rect->xmin) rect->xmin = mcoords[i][0];
    if (mcoords[i][0] > rect->xmax) rect->xmax = mcoords[i][0];
    if (mcoords[i][1] < rect->ymin) rect->ymin = mcoords[i][1];
    if (mcoords[i][1] > rect->ymax) rect->ymax = mcoords[i][1];
  }
}
bool BLI_lasso_is_point_inside(
    const int mcoords[][2], unsigned int len, int sx, int sy, int error_value)
{
  if (len == 0 || sx == error_value) {
    return false;
  }
  bool inside = false;
  for (unsigned int i = 0, j = len - 1; i < len; j = i++) {
    const float xi = (float)mcoords[i][0], yi = (float)mcoords[i][1];
    const float xj = (float)mcoords[j][0], yj = (float)mcoords[j][1];
    if (((yi > (float)sy) != (yj > (float)sy)) &&
        ((float)sx < (xj - xi) * ((float)sy - yi) / (yj - yi) + xi))
    {
      inside = !inside;
    }
  }
  return inside;
}

/* ---- fixture helpers ------------------------------------------------------ */
static int failures = 0;
#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      printf("FAIL: %s (line %d)\n", msg, __LINE__); \
      failures++; \
    } \
  } while (0)

static bGPdata *make_gpd(void)
{
  bGPdata *gpd = calloc(1, sizeof(bGPdata));
  gpd->totcol = 4;
  gpd->mat = calloc(4, sizeof(Material *));
  for (int i = 0; i < 4; i++) {
    gpd->mat[i] = calloc(1, sizeof(Material));
    gpd->mat[i]->gp_style = calloc(1, sizeof(MaterialGPencilStyle));
  }
  gpd->mat[1]->gp_style->flag = GP_MATERIAL_LOCKED;
  gpd->mat[2]->gp_style->flag = GP_MATERIAL_FILL_SHOW;
  gpd->mat[3]->gp_style->flag = GP_MATERIAL_HIDE;
  return gpd;
}
static void list_add(ListBase *lb, void *vlink)
{
  Link *link = vlink;
  link->next = NULL;
  link->prev = lb->last;
  if (lb->last) {
    ((Link *)lb->last)->next = link;
  }
  else {
    lb->first = link;
  }
  lb->last = link;
}
static bGPDlayer *add_layer(bGPdata *gpd, int flag)
{
  bGPDlayer *gpl = calloc(1, sizeof(bGPDlayer));
  gpl->opacity = 1.0f;
  gpl->flag = flag;
  list_add(&gpd->layers, gpl);
  return gpl;
}
static bGPDframe *add_frame(bGPDlayer *gpl, int framenum, bool active)
{
  bGPDframe *gpf = calloc(1, sizeof(bGPDframe));
  gpf->framenum = framenum;
  list_add(&gpl->frames, gpf);
  if (active) {
    gpl->actframe = gpf;
  }
  return gpf;
}
/* n points on a line starting at (x0,y0) with step (dx,dy) */
static bGPDstroke *add_stroke(
    bGPDframe *gpf, int n, int mat, float x0, float y0, float dx, float dy)
{
  bGPDstroke *gps = calloc(1, sizeof(bGPDstroke));
  gps->totpoints = n;
  gps->mat_nr = mat;
  gps->points = calloc((size_t)n, sizeof(bGPDspoint));
  for (int i = 0; i < n; i++) {
    gps->points[i].x = x0 + dx * (float)i;
    gps->points[i].y = y0 + dy * (float)i;
  }
  list_add(&gpf->strokes, gps);
  return gps;
}
static void set_points(bGPdata *gpd, bGPDstroke *gps, unsigned mask)
{
  for (int i = 0; i < gps->totpoints; i++) {
    if (mask & (1u << i)) {
      gps->points[i].flag |= GP_SPOINT_SELECT;
    }
    else {
      gps->points[i].flag &= ~GP_SPOINT_SELECT;
    }
  }
  BKE_gpencil_stroke_sync_selection(gpd, gps);
}
static unsigned point_mask(const bGPDstroke *gps)
{
  unsigned m = 0;
  for (int i = 0; i < gps->totpoints; i++) {
    if (gps->points[i].flag & GP_SPOINT_SELECT) {
      m |= 1u << i;
    }
  }
  return m;
}
static bool stroke_sel(const bGPDstroke *gps)
{
  return (gps->flag & GP_STROKE_SELECT) != 0;
}

/* ---- eSelectOp helpers ------------------------------------------------------ */
static void test_select_op_tables(void)
{
  /* index = is_select * 2 + is_inside */
  static const int action[6][4] = {
      {0, 0, 0, 0},
      /* ADD */ {-1, 1, -1, -1},
      /* SUB */ {-1, -1, -1, 0},
      /* SET */ {0, 1, 0, 1},
      /* AND */ {-1, -1, 0, -1},
      /* XOR */ {-1, 1, -1, 0},
  };
  static const int deselected[6][4] = {
      {0, 0, 0, 0},
      /* ADD */ {-1, 1, -1, -1},
      /* SUB */ {-1, -1, -1, 0},
      /* SET */ {-1, 1, -1, 1},
      /* AND */ {-1, -1, 0, -1},
      /* XOR */ {-1, 1, -1, 0},
  };
  for (int op = PG_SEL_OP_ADD; op <= PG_SEL_OP_XOR; op++) {
    for (int s = 0; s < 2; s++) {
      for (int i = 0; i < 2; i++) {
        CHECK(pg_select_op_action(op, s, i) == action[op][s * 2 + i], "ED_select_op_action");
        CHECK(pg_select_op_action_deselected(op, s, i) == deselected[op][s * 2 + i],
              "ED_select_op_action_deselected");
      }
    }
  }
  CHECK(pg_select_op_modal(PG_SEL_OP_SET, 1) == PG_SEL_OP_SET, "modal: first dab keeps SET");
  CHECK(pg_select_op_modal(PG_SEL_OP_SET, 0) == PG_SEL_OP_ADD, "modal: later dabs extend");
  CHECK(pg_select_op_modal(PG_SEL_OP_SUB, 0) == PG_SEL_OP_SUB, "modal: SUB unchanged");
}

/* ---- select all / deselect / invert / toggle -------------------------------- */
static void test_select_all(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *a = add_layer(gpd, 0);
  bGPDframe *a1 = add_frame(a, 1, false);
  bGPDframe *a2 = add_frame(a, 2, true);
  bGPDstroke *s1 = add_stroke(a1, 3, 0, 0, 0, 10, 0);
  bGPDstroke *s3 = add_stroke(a2, 3, 0, 0, 20, 10, 0);
  bGPDstroke *locked_mat = add_stroke(a2, 2, 1, 0, 40, 10, 0);
  bGPDlayer *hidden = add_layer(gpd, GP_LAYER_HIDE);
  bGPDstroke *sh = add_stroke(add_frame(hidden, 1, true), 2, 0, 0, 60, 10, 0);

  CHECK(pg_gp_select_all(gpd, PG_SEL_SELECT, NULL) == 1, "select reports a change");
  CHECK(point_mask(s3) == 7 && stroke_sel(s3) && s3->select_index > 0,
        "select: active frame stroke fully selected with a select index");
  CHECK(point_mask(s1) == 0 && !stroke_sel(s1), "select: other frames are untouched");
  CHECK(point_mask(locked_mat) == 0, "select: strokes with locked material are not editable");
  CHECK(point_mask(sh) == 0, "select: hidden layers are untouched");
  CHECK(pg_gp_select_all(gpd, PG_SEL_SELECT, NULL) == 0, "selecting again changes nothing");

  /* deselect works across every frame of editable layers, including locked-material strokes */
  set_points(gpd, s1, 7);
  set_points(gpd, locked_mat, 3);
  set_points(gpd, sh, 3);
  gpd->select_last_index = 5;
  CHECK(pg_gp_select_all(gpd, PG_SEL_DESELECT, NULL) == 1, "deselect reports a change");
  CHECK(point_mask(s1) == 0 && !stroke_sel(s1) && s1->select_index == 0,
        "deselect: strokes on non-active frames are cleared");
  CHECK(point_mask(s3) == 0 && !stroke_sel(s3), "deselect: active frame stroke cleared");
  CHECK(point_mask(locked_mat) == 0 && !stroke_sel(locked_mat),
        "deselect ignores material locks (it uses editable layers only)");
  CHECK(point_mask(sh) == 3 && stroke_sel(sh), "deselect: hidden layer is still untouched");
  CHECK(gpd->select_last_index == 0, "deselect resets the selection index counter");

  /* invert keeps the stroke flag consistent with its points */
  set_points(gpd, s3, 2);
  CHECK(pg_gp_select_all(gpd, PG_SEL_INVERT, NULL) == 1, "invert reports a change");
  CHECK(point_mask(s3) == 5 && stroke_sel(s3), "invert: partial selection flips per point");
  set_points(gpd, s3, 7);
  pg_gp_select_all(gpd, PG_SEL_INVERT, NULL);
  CHECK(point_mask(s3) == 0 && !stroke_sel(s3) && s3->select_index == 0,
        "invert: fully selected stroke becomes fully deselected (flag cleared)");

  /* toggle selects when nothing is selected, deselects otherwise */
  set_points(gpd, s3, 0);
  set_points(gpd, s1, 0);
  pg_gp_select_all(gpd, PG_SEL_TOGGLE, NULL);
  CHECK(point_mask(s3) == 7, "toggle with empty selection selects");
  pg_gp_select_all(gpd, PG_SEL_TOGGLE, NULL);
  CHECK(point_mask(s3) == 0, "toggle with a selected stroke deselects");
  CHECK(pg_gp_select_all(gpd, 9, NULL) == 0, "invalid action is rejected");
  CHECK(pg_gp_select_all(NULL, PG_SEL_SELECT, NULL) == 0, "null document is rejected");
  (void)sh;
}

static void test_layer_scope(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *a = add_layer(gpd, 0);
  bGPDlayer *b = add_layer(gpd, 0);
  bGPDstroke *sa = add_stroke(add_frame(a, 1, true), 3, 0, 0, 0, 10, 0);
  bGPDstroke *sb = add_stroke(add_frame(b, 1, true), 3, 0, 0, 20, 10, 0);
  pg_gp_select_all(gpd, PG_SEL_SELECT, a);
  CHECK(point_mask(sa) == 7 && point_mask(sb) == 0, "scoped to the active layer only");
  pg_gp_select_all(gpd, PG_SEL_DESELECT, NULL);
  pg_gp_select_all(gpd, PG_SEL_SELECT, NULL);
  CHECK(point_mask(sa) == 7 && point_mask(sb) == 7, "unscoped covers every editable layer");
}

/* ---- linked / alternate / more / less / first / last -------------------------- */
static void test_linked_alternate(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *a = add_layer(gpd, 0);
  bGPDframe *f = add_frame(a, 1, true);
  bGPDstroke *s1 = add_stroke(f, 5, 0, 0, 0, 10, 0);
  bGPDstroke *s2 = add_stroke(f, 5, 0, 0, 20, 10, 0);
  set_points(gpd, s1, 1u << 2);
  CHECK(pg_gp_select_linked(gpd, NULL) == 1, "linked reports a change");
  CHECK(point_mask(s1) == 31, "linked: whole selected stroke");
  CHECK(point_mask(s2) == 0, "linked: unselected strokes untouched");
  CHECK(pg_gp_select_linked(gpd, NULL) == 0, "linked is idempotent");

  /* alternate: every second point starting with the first (row 0 selected) */
  CHECK(pg_gp_select_alternate(gpd, 0, NULL) == 1, "alternate reports a change");
  CHECK(point_mask(s1) == (1u | 4u | 16u), "alternate: points 0,2,4");
  set_points(gpd, s1, 31);
  pg_gp_select_alternate(gpd, 1, NULL);
  CHECK(point_mask(s1) == ((1u << 1) | (1u << 3)),
        "alternate with unselect_ends: pattern starts at point 1 and both ends are removed");
  CHECK(point_mask(s2) == 0, "alternate: unselected strokes untouched");
  /* single point strokes are skipped */
  bGPDstroke *one = add_stroke(f, 1, 0, 0, 50, 0, 0);
  set_points(gpd, one, 1);
  pg_gp_select_alternate(gpd, 0, NULL);
  CHECK(point_mask(one) == 1, "alternate: stroke with one point is left alone");
}

static void test_more_less(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *a = add_layer(gpd, 0);
  bGPDframe *f = add_frame(a, 1, true);
  bGPDstroke *s = add_stroke(f, 7, 0, 0, 0, 10, 0);
  bGPDstroke *other = add_stroke(f, 7, 0, 0, 20, 10, 0);
  set_points(gpd, s, (1u << 2) | (1u << 3));
  CHECK(pg_gp_select_more(gpd, NULL) == 1, "more reports a change");
  CHECK(point_mask(s) == (0x1Eu), "more: island {2,3} grows to {1,2,3,4}");
  CHECK(point_mask(other) == 0, "more: unselected strokes untouched");
  pg_gp_select_more(gpd, NULL);
  CHECK(point_mask(s) == 0x3Fu, "more twice reaches points 0 and 5");
  set_points(gpd, s, 0x1Eu);
  CHECK(pg_gp_select_less(gpd, NULL) == 1, "less reports a change");
  CHECK(point_mask(s) == ((1u << 2) | (1u << 3)), "less: island {1..4} shrinks to {2,3}");
  set_points(gpd, s, 1u << 3);
  pg_gp_select_less(gpd, NULL);
  CHECK(point_mask(s) == 0, "less: a single selected point disappears");
  CHECK(!stroke_sel(s) || point_mask(s) == 0, "less: no points left");
}

static void test_first_last(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *a = add_layer(gpd, 0);
  bGPDframe *f = add_frame(a, 1, true);
  bGPDstroke *s1 = add_stroke(f, 4, 0, 0, 0, 10, 0);
  bGPDstroke *s2 = add_stroke(f, 4, 0, 0, 20, 10, 0);
  set_points(gpd, s1, 0xE);

  CHECK(pg_gp_select_first(gpd, 1, 0, NULL) == 1, "first (selected strokes only) reports a change");
  CHECK(point_mask(s1) == 1u && stroke_sel(s1), "first: only the first point stays selected");
  CHECK(point_mask(s2) == 0 && !stroke_sel(s2), "first: only_selected skips unselected strokes");

  set_points(gpd, s1, 0xE);
  pg_gp_select_first(gpd, 0, 1, NULL);
  CHECK(point_mask(s1) == 0xF, "first with extend keeps other points and adds the first");
  CHECK(point_mask(s2) == 1u && stroke_sel(s2), "first: every stroke is eligible by default");

  pg_gp_select_all(gpd, PG_SEL_DESELECT, NULL);
  pg_gp_select_last(gpd, 0, 0, NULL);
  CHECK(point_mask(s1) == 8u && point_mask(s2) == 8u, "last: only the last point of every stroke");
  set_points(gpd, s1, 1);
  pg_gp_select_last(gpd, 1, 1, NULL);
  CHECK(point_mask(s1) == 9u, "last with extend adds to existing selection");
  CHECK(point_mask(s2) == 8u, "last: only_selected leaves strokes with selected flag only");
}

/* ---- grouped -------------------------------------------------------------------- */
static void test_grouped(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *a = add_layer(gpd, 0);
  bGPDlayer *b = add_layer(gpd, 0);
  bGPDlayer *hidden = add_layer(gpd, GP_LAYER_HIDE);
  bGPDframe *fa = add_frame(a, 1, true);
  bGPDframe *fb = add_frame(b, 1, true);
  bGPDstroke *a1 = add_stroke(fa, 2, 0, 0, 0, 10, 0);
  bGPDstroke *a2 = add_stroke(fa, 2, 0, 0, 20, 10, 0);
  bGPDstroke *b1 = add_stroke(fb, 2, 0, 0, 40, 10, 0);
  bGPDstroke *h1 = add_stroke(add_frame(hidden, 1, true), 2, 0, 0, 60, 10, 0);

  /* nothing selected: nothing happens (Blender requires a selected stroke on the layer) */
  CHECK(pg_gp_select_grouped(gpd, PG_SELECT_GROUPED_LAYER, NULL) == 0, "layer: needs a selection");

  set_points(gpd, a1, 1);
  CHECK(pg_gp_select_grouped(gpd, PG_SELECT_GROUPED_LAYER, NULL) == 1, "layer: reports a change");
  CHECK(point_mask(a1) == 3 && point_mask(a2) == 3, "layer: every stroke on the layer");
  CHECK(point_mask(b1) == 0, "layer: layers without a selected stroke stay untouched");
  CHECK(point_mask(h1) == 0, "layer: hidden layers are not editable");

  /* material: set of materials of all selected strokes, across layers */
  pg_gp_select_all(gpd, PG_SEL_DESELECT, NULL);
  bGPDstroke *m0a = add_stroke(fa, 2, 0, 0, 80, 10, 0);
  bGPDstroke *m2a = add_stroke(fa, 2, 2, 0, 100, 10, 0);
  bGPDstroke *m2b = add_stroke(fb, 2, 2, 0, 120, 10, 0);
  bGPDstroke *mlock = add_stroke(fb, 2, 1, 0, 140, 10, 0);
  set_points(gpd, m2a, 3);
  CHECK(pg_gp_select_grouped(gpd, PG_SELECT_GROUPED_MATERIAL, NULL) == 1, "material: change");
  CHECK(point_mask(m2b) == 3, "material: same material on another layer");
  CHECK(point_mask(m0a) == 0, "material: different material is untouched");
  CHECK(point_mask(mlock) == 0, "material: locked-material strokes are not editable");
  set_points(gpd, m0a, 3);
  pg_gp_select_grouped(gpd, PG_SELECT_GROUPED_MATERIAL, NULL);
  CHECK(point_mask(a1) == 3 && point_mask(a2) == 3, "material: uses every selected stroke's material");
  CHECK(pg_gp_select_grouped(gpd, 7, NULL) == 0, "invalid group type is rejected");
}

/* ---- lasso / box / circle ----------------------------------------------------- */
static const int RECT_LASSO[4][2] = {{25, -10}, {65, -10}, {65, 10}, {25, 10}};

static void build_two_strokes(bGPdata **gpd, bGPDstroke **s1, bGPDstroke **s2)
{
  *gpd = make_gpd();
  bGPDlayer *a = add_layer(*gpd, 0);
  bGPDframe *f = add_frame(a, 1, true);
  *s1 = add_stroke(f, 10, 0, 0, 0, 10, 0);   /* x = 0..90, y = 0 */
  *s2 = add_stroke(f, 10, 0, 0, 50, 10, 0);  /* x = 0..90, y = 50 */
}

static void test_lasso_ops(void)
{
  bGPdata *gpd;
  bGPDstroke *s1, *s2;
  build_two_strokes(&gpd, &s1, &s2);
  const unsigned inside = (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6);

  set_points(gpd, s2, 1);
  CHECK(pg_gp_select_lasso(gpd, RECT_LASSO, 4, PG_SEL_OP_SET, PG_SELECTMODE_POINT, NULL) == 1,
        "lasso SET reports a change");
  CHECK(point_mask(s1) == inside && stroke_sel(s1) && s1->select_index > 0,
        "lasso SET: points inside the noose are selected and the stroke flag follows");
  CHECK(point_mask(s2) == 0 && !stroke_sel(s2), "lasso SET: previous selection is replaced");

  set_points(gpd, s2, 1);
  pg_gp_select_lasso(gpd, RECT_LASSO, 4, PG_SEL_OP_ADD, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s1) == inside && point_mask(s2) == 1, "lasso ADD keeps the old selection");

  set_points(gpd, s1, 0x3FF);
  pg_gp_select_lasso(gpd, RECT_LASSO, 4, PG_SEL_OP_SUB, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s1) == (0x3FFu & ~inside) && stroke_sel(s1),
        "lasso SUB deselects only the points inside; stroke stays selected");

  set_points(gpd, s1, (1u << 4) | 1u);
  pg_gp_select_lasso(gpd, RECT_LASSO, 4, PG_SEL_OP_XOR, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s1) == (1u | (1u << 3) | (1u << 5) | (1u << 6)),
        "lasso XOR toggles points inside the noose");

  set_points(gpd, s1, 0x3FF);
  set_points(gpd, s2, 0x3FF);
  pg_gp_select_lasso(gpd, RECT_LASSO, 4, PG_SEL_OP_AND, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s1) == inside, "lasso AND keeps only selected points inside");
  CHECK(point_mask(s2) == 0 && !stroke_sel(s2), "lasso AND clears strokes entirely outside");

  CHECK(pg_gp_select_lasso(gpd, RECT_LASSO, 4, 99, PG_SELECTMODE_POINT, NULL) == 0, "bad op rejected");
  CHECK(pg_gp_select_lasso(gpd, RECT_LASSO, 2, PG_SEL_OP_SET, PG_SELECTMODE_POINT, NULL) == 0,
        "lasso needs at least three points");
}

static void test_lasso_stroke_mode_and_fill(void)
{
  bGPdata *gpd;
  bGPDstroke *s1, *s2;
  build_two_strokes(&gpd, &s1, &s2);

  pg_gp_select_lasso(gpd, RECT_LASSO, 4, PG_SEL_OP_SET, PG_SELECTMODE_STROKE, NULL);
  CHECK(point_mask(s1) == 0x3FF && stroke_sel(s1), "stroke mode: one point inside selects the stroke");
  CHECK(point_mask(s2) == 0, "stroke mode: other strokes untouched");
  pg_gp_select_lasso(gpd, RECT_LASSO, 4, PG_SEL_OP_SUB, PG_SELECTMODE_STROKE, NULL);
  CHECK(point_mask(s1) == 0 && !stroke_sel(s1), "stroke mode SUB: deselects the whole stroke");

  /* A filled stroke whose points are all outside, but whose area contains the centre of the noose. */
  bGPDlayer *a = (bGPDlayer *)gpd->layers.first;
  bGPDframe *f = a->actframe;
  bGPDstroke *fill = add_stroke(f, 4, 2, 0, 0, 0, 0);
  const float sq[4][2] = {{0, 100}, {100, 100}, {100, 200}, {0, 200}};
  for (int i = 0; i < 4; i++) {
    fill->points[i].x = sq[i][0];
    fill->points[i].y = sq[i][1];
  }
  bGPDstroke *outline = add_stroke(f, 4, 0, 0, 0, 0, 0);
  for (int i = 0; i < 4; i++) {
    outline->points[i].x = sq[i][0] + 300.0f;
    outline->points[i].y = sq[i][1];
  }
  const int small[3][2] = {{45, 145}, {55, 145}, {50, 155}};
  pg_gp_select_all(gpd, PG_SEL_DESELECT, NULL);
  pg_gp_select_lasso(gpd, small, 3, PG_SEL_OP_SET, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(fill) == 0xF && stroke_sel(fill),
        "lasso inside the area of a filled stroke selects the whole stroke");
  const int small2[3][2] = {{345, 145}, {355, 145}, {350, 155}};
  pg_gp_select_lasso(gpd, small2, 3, PG_SEL_OP_SET, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(outline) == 0, "stroke without a visible fill is not selected by area");
}

static void test_box(void)
{
  bGPdata *gpd;
  bGPDstroke *s1, *s2;
  build_two_strokes(&gpd, &s1, &s2);
  CHECK(pg_gp_select_box(gpd, 25, -10, 65, 60, PG_SEL_OP_SET, PG_SELECTMODE_POINT, NULL) == 1,
        "box reports a change");
  const unsigned inside = (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6);
  CHECK(point_mask(s1) == inside && point_mask(s2) == inside, "box selects points of both strokes");
  CHECK(pg_gp_select_box(gpd, 25, -10, 65, 60, PG_SEL_OP_SUB, PG_SELECTMODE_POINT, NULL) == 1,
        "box SUB reports a change");
  CHECK(point_mask(s1) == 0 && point_mask(s2) == 0 && !stroke_sel(s1), "box SUB clears them");
  pg_gp_select_box(gpd, 60, -5, 60, 5, PG_SEL_OP_SET, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s1) == (1u << 6), "box edges are inclusive");
}

static void test_circle(void)
{
  bGPdata *gpd;
  bGPDstroke *s1, *s2;
  build_two_strokes(&gpd, &s1, &s2);
  CHECK(pg_gp_select_circle(gpd, 30, 0, 15, PG_SEL_OP_SET, 1, PG_SELECTMODE_POINT, NULL) == 1,
        "circle reports a change");
  CHECK(point_mask(s1) == ((1u << 2) | (1u << 3) | (1u << 4)) && stroke_sel(s1),
        "circle selects points within the radius");
  CHECK(point_mask(s2) == 0, "circle leaves far strokes alone");

  /* radius is inclusive (<= radius*radius) */
  pg_gp_select_all(gpd, PG_SEL_DESELECT, NULL);
  pg_gp_select_circle(gpd, 30, 0, 10, PG_SEL_OP_SET, 1, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s1) == ((1u << 2) | (1u << 3) | (1u << 4)), "circle radius is inclusive");

  /* subtract from a fully selected stroke */
  set_points(gpd, s1, 0x3FF);
  pg_gp_select_circle(gpd, 30, 0, 15, PG_SEL_OP_SUB, 1, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s1) == (0x3FFu & ~((1u << 2) | (1u << 3) | (1u << 4))) && stroke_sel(s1),
        "circle SUB removes points but the stroke stays selected");

  /* modal behaviour: only the first dab of a SET gesture replaces the selection */
  set_points(gpd, s2, 1);
  set_points(gpd, s1, 0);
  pg_gp_select_circle(gpd, 30, 0, 5, PG_SEL_OP_SET, 0, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s2) == 1, "circle SET after the first dab extends instead of replacing");
  pg_gp_select_circle(gpd, 30, 0, 5, PG_SEL_OP_SET, 1, PG_SELECTMODE_POINT, NULL);
  CHECK(point_mask(s2) == 0, "first dab of a SET gesture replaces the selection");

  /* stroke mode */
  pg_gp_select_circle(gpd, 30, 0, 5, PG_SEL_OP_SET, 1, PG_SELECTMODE_STROKE, NULL);
  CHECK(point_mask(s1) == 0x3FF, "circle in stroke mode selects the whole stroke on a hit");
  pg_gp_select_circle(gpd, 30, 0, 5, PG_SEL_OP_SUB, 1, PG_SELECTMODE_STROKE, NULL);
  CHECK(point_mask(s1) == 0 && !stroke_sel(s1), "circle SUB in stroke mode deselects the stroke");
}

static void test_dispatch(void)
{
  bGPdata *gpd;
  bGPDstroke *s1, *s2;
  build_two_strokes(&gpd, &s1, &s2);
  bGPDlayer *layer = (bGPDlayer *)gpd->layers.first;
  /* Area selection with a NULL scope must include editable strokes on other visible layers. */
  bGPDlayer *other_layer = add_layer(gpd, 0);
  bGPDframe *other_frame = add_frame(other_layer, 1, true);
  bGPDstroke *other_stroke = add_stroke(other_frame, 5, 0, 30, 0, 8, 0);

  const float lasso[] = {PG_SEL_OP_SET, PG_SELECTMODE_POINT, 25, -10, 65, -10, 65, 10, 25, 10};
  dirty_tags = 0;
  gpd->flag &= ~GP_DATA_CACHE_IS_DIRTY;
  CHECK(pg_gp_select_dispatch(gpd, NULL, PG_SELECT_CMD_LASSO, lasso, 10) == 1, "dispatch lasso");
  CHECK(point_mask(s1) == ((1u << 3) | (1u << 4) | (1u << 5) | (1u << 6)), "dispatch lasso selection");
  CHECK(point_mask(other_stroke) == 0x1Fu, "dispatch lasso selects another visible editable layer");
  CHECK((gpd->flag & GP_DATA_CACHE_IS_DIRTY) != 0 && dirty_tags > 0, "a change tags the GP cache dirty");

  dirty_tags = 0;
  CHECK(pg_gp_select_dispatch(gpd, NULL, PG_SELECT_CMD_LASSO, lasso, 10) == 0, "no change -> 0");
  CHECK(dirty_tags == 0, "no change leaves the cache alone");

  /* Blender segment selection is bounded by intersections with other strokes. */
  (void)add_stroke(layer->actframe, 3, 0, 15, -10, 0, 10);
  (void)add_stroke(layer->actframe, 3, 0, 45, -10, 0, 10);
  const float segment_lasso[] = {PG_SEL_OP_SET, PG_SELECTMODE_SEGMENT, 29, -5, 31, -5, 31, 5, 29, 5};
  CHECK(pg_gp_select_dispatch(gpd, NULL, PG_SELECT_CMD_LASSO, segment_lasso, 10) == 1,
        "dispatch segment lasso");
  CHECK((point_mask(s1) & (1u << 3)) != 0, "segment lasso selects hit point");
  CHECK((point_mask(s1) & ((1u << 2) | (1u << 4))) != 0,
        "segment lasso expands to adjacent segment point");

  const float box[] = {PG_SEL_OP_SET, PG_SELECTMODE_POINT, 0, 40, 100, 60};
  CHECK(pg_gp_select_dispatch(gpd, NULL, PG_SELECT_CMD_BOX, box, 6) == 1, "dispatch box");
  CHECK(point_mask(s2) == 0x3FF && point_mask(s1) == 0, "dispatch box selection");

  const float circle[] = {PG_SEL_OP_SET, PG_SELECTMODE_POINT, 0, 0, 5, 1};
  CHECK(pg_gp_select_dispatch(gpd, NULL, PG_SELECT_CMD_CIRCLE, circle, 6) == 1, "dispatch circle");
  CHECK(point_mask(s1) == 1u && point_mask(s2) == 0, "dispatch circle selection");

  const float all[] = {PG_SEL_SELECT};
  CHECK(pg_gp_select_dispatch(gpd, NULL, PG_SELECT_CMD_ALL, all, 1) == 1, "dispatch select all");
  CHECK(point_mask(s1) == 0x3FF && point_mask(s2) == 0x3FF, "dispatch select all result");
  const float first[] = {0, 0};
  pg_gp_select_dispatch(gpd, layer, PG_SELECT_CMD_FIRST, first, 2);
  CHECK(point_mask(s1) == 1u, "dispatch first");

  CHECK(pg_gp_select_dispatch(gpd, layer, 9999, NULL, 0) == 0, "unknown command");
  CHECK(pg_gp_select_dispatch(gpd, layer, PG_SELECT_CMD_LASSO, lasso, 3) == 0, "lasso needs 3 points");
  CHECK(pg_gp_select_dispatch(gpd, layer, PG_SELECT_CMD_BOX, box, 4) == 0, "box needs 6 args");
  const float bad[] = {PG_SEL_OP_SET, PG_SELECTMODE_POINT, NAN, 0, 5, 1};
  CHECK(pg_gp_select_dispatch(gpd, layer, PG_SELECT_CMD_CIRCLE, bad, 6) == 0, "NaN rejected");
  CHECK(pg_gp_select_dispatch(NULL, layer, PG_SELECT_CMD_ALL, all, 1) == 0, "null document rejected");
}

int main(void)
{
  test_select_op_tables();
  test_select_all();
  test_layer_scope();
  test_linked_alternate();
  test_more_less();
  test_first_last();
  test_grouped();
  test_lasso_ops();
  test_lasso_stroke_mode_and_fill();
  test_box();
  test_circle();
  test_dispatch();
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
