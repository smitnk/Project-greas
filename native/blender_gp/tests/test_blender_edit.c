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
#include "DNA_meshdata_types.h"
#include "project_grease_blender_edit.h"
#include "project_grease_blender_edit2.h"
#include "project_grease_blender_edit3.h"
#include "BLI_rand.h"
#include "project_grease_blender_edit4.h"
#include "project_grease_blender_edit6.h"

int pg_test_mem_free_count = 0; /* see select_shim/MEM_guardedalloc.h */

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


/* stand-ins recording the BKE calls of the Length modifier */
typedef struct { int kind; float dist, overshoot; short mode; int extra; } LenCall;
static LenCall len_calls[8];
static int len_ncalls = 0;
bool BKE_gpencil_stroke_stretch(bGPDstroke *gps, float dist, float overshoot_fac, short mode,
                                bool follow_curvature, int extra_point_count, float segment_influence,
                                float max_angle, bool invert_curvature)
{
  (void)gps; (void)follow_curvature; (void)segment_influence; (void)max_angle; (void)invert_curvature;
  if (len_ncalls < 8) len_calls[len_ncalls++] = (LenCall){1, dist, overshoot_fac, mode, extra_point_count};
  return true;
}
bool BKE_gpencil_stroke_shrink(bGPDstroke *gps, float dist, short mode)
{
  (void)gps;
  if (len_ncalls < 8) len_calls[len_ncalls++] = (LenCall){2, dist, 0, mode, 0};
  return true;
}
float BKE_gpencil_stroke_length(const bGPDstroke *gps, bool use_3d)
{
  (void)use_3d;
  float l = 0;
  for (int i = 1; i < gps->totpoints; i++) l += hypotf(gps->points[i].x - gps->points[i - 1].x, gps->points[i].y - gps->points[i - 1].y);
  return l;
}


void BKE_gpencil_stroke_flip(bGPDstroke *gps)
{
  for (int i = 0, j = gps->totpoints - 1; i < j; i++, j--) {
    bGPDspoint t = gps->points[i]; gps->points[i] = gps->points[j]; gps->points[j] = t;
  }
}


bGPDstroke *BKE_gpencil_stroke_duplicate(bGPDstroke *src, bool dup_points, bool dup_curve)
{
  (void)dup_curve;
  bGPDstroke *d = malloc(sizeof(bGPDstroke));
  *d = *src;
  d->next = d->prev = NULL;
  if (dup_points) {
    d->points = malloc(sizeof(bGPDspoint) * (size_t)(src->totpoints > 0 ? src->totpoints : 1));
    memcpy(d->points, src->points, sizeof(bGPDspoint) * (size_t)src->totpoints);
  }
  return d;
}
static int join_calls = 0;
void BKE_gpencil_stroke_join(bGPDstroke *a, bGPDstroke *b, bool leave_gaps, bool fit_thickness, bool smooth, bool auto_flip)
{
  (void)leave_gaps; (void)fit_thickness; (void)smooth; (void)auto_flip;
  bGPDspoint *p = malloc(sizeof(bGPDspoint) * (size_t)(a->totpoints + b->totpoints));
  memcpy(p, a->points, sizeof(bGPDspoint) * (size_t)a->totpoints);
  memcpy(p + a->totpoints, b->points, sizeof(bGPDspoint) * (size_t)b->totpoints);
  free(a->points); a->points = p; a->totpoints += b->totpoints;
  join_calls++;
}

static int simplify_calls = 0, sample_calls = 0;
void BKE_gpencil_stroke_simplify_fixed(bGPdata *gpd, bGPDstroke *gps)
{
  (void)gpd; simplify_calls++;
  /* stand-in: keep every second point (Blender removes alternate points, keeping the ends) */
  int keep = 0;
  for (int i = 0; i < gps->totpoints; i++) if (i % 2 == 0 || i == gps->totpoints - 1) gps->points[keep++] = gps->points[i];
  gps->totpoints = keep;
}
bool BKE_gpencil_stroke_sample(bGPdata *gpd, bGPDstroke *gps, float dist, bool select, float sharp_threshold)
{
  (void)gpd; (void)gps; (void)select; (void)sharp_threshold;
  sample_calls++;
  return dist > 0.0f;
}

bGPDframe *BKE_gpencil_frame_addnew(bGPDlayer *gpl, int cframe)
{
  bGPDframe *n = calloc(1, sizeof(bGPDframe));
  n->framenum = cframe;
  bGPDframe *at = gpl->frames.first;
  while (at && at->framenum < cframe) at = at->next;
  if (at == NULL) {
    n->prev = gpl->frames.last; n->next = NULL;
    if (gpl->frames.last) ((bGPDframe *)gpl->frames.last)->next = n; else gpl->frames.first = n;
    gpl->frames.last = n;
  }
  else {
    n->next = at; n->prev = at->prev;
    if (at->prev) at->prev->next = n; else gpl->frames.first = n;
    at->prev = n;
  }
  return n;
}
bool BKE_gpencil_layer_frame_delete(bGPDlayer *gpl, bGPDframe *gpf)
{
  BLI_remlink(&gpl->frames, gpf);
  for (bGPDstroke *s = gpf->strokes.first, *n; s; s = n) { n = s->next; free(s->points); free(s); }
  free(gpf);
  return true;
}

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
static int merge_calls = 0;
void BKE_gpencil_stroke_merge_distance(bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, float threshold, bool use_unselected)
{
  (void)gpd; (void)gpf; (void)use_unselected; merge_calls++;
  int keep = 1;
  for (int i = 1; i < gps->totpoints; i++) {
    float dx = gps->points[i].x - gps->points[keep - 1].x, dy = gps->points[i].y - gps->points[keep - 1].y;
    if (dx * dx + dy * dy >= threshold * threshold) gps->points[keep++] = gps->points[i];
  }
  gps->totpoints = keep;
}
bGPDlayer *BKE_gpencil_layer_addnew(bGPdata *gpd, const char *name, bool setactive, bool add_to_header)
{
  (void)name; (void)setactive; (void)add_to_header;
  bGPDlayer *l = calloc(1, sizeof(bGPDlayer));
  l->opacity = 1.0f;
  l->prev = gpd->layers.last; l->next = NULL;
  if (gpd->layers.last) ((bGPDlayer *)gpd->layers.last)->next = l; else gpd->layers.first = l;
  gpd->layers.last = l;
  return l;
}

void BLI_addtail(ListBase *lb, void *vlink)
{
  Link *link = vlink;
  link->next = NULL; link->prev = lb->last;
  if (lb->last) ((Link *)lb->last)->next = link; else lb->first = link;
  lb->last = link;
}
void *BLI_findlink(const ListBase *lb, int number)
{
  if (number < 0) return NULL;
  Link *l = lb->first;
  for (int i = 0; l && i < number; i++) l = l->next;
  return l;
}

static void select_points(bGPdata *gpd, bGPDstroke *gps, unsigned mask)
{
  for (int i = 0; i < gps->totpoints; i++) {
    /* the mask covers the first 32 points; later points follow bit 31 (all set or all clear) */
    const unsigned bit = i < 32 ? (1u << i) : (1u << 31);
    if (mask & bit) gps->points[i].flag |= GP_SPOINT_SELECT; else gps->points[i].flag &= ~GP_SPOINT_SELECT;
  }
  BKE_gpencil_stroke_sync_selection(gpd, gps);
}
static unsigned sel_mask(const bGPDstroke *gps)
{
  unsigned m = 0;
  for (int i = 0; i < gps->totpoints && i < 32; i++) if (gps->points[i].flag & GP_SPOINT_SELECT) m |= 1u << i;
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


static void test_length_modifier(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 11, 0, 0, 0, 10, 0); /* length 100 */
  bGPDstroke *cyc = add_stroke(f, 4, 0, 0, 50, 10, 0);
  cyc->flag |= GP_STROKE_CYCLIC;
  select_points(gpd, a, 0x7FF); select_points(gpd, cyc, 15);
  cyc->flag |= GP_STROKE_CYCLIC;

  PGLengthParams p = {PG_LENGTH_RELATIVE, 0.1f, 0.2f, 0.1f, 0, 30.0f, 0.0f, 2.97f, 0};
  len_ncalls = 0; geometry_updates = 0;
  CHECK(pg_gp_mod_length(gpd, NULL, &p) == 1, "length modifier reports a change");
  CHECK(len_ncalls == 2, "two calls: start then end (cyclic stroke skipped)");
  CHECK(len_calls[0].kind == 1 && NEAR(len_calls[0].dist, 10.0f) && len_calls[0].mode == 1, "start: stretch by len*start_fac, mode 1");
  CHECK(len_calls[1].kind == 1 && NEAR(len_calls[1].dist, 20.0f) && len_calls[1].mode == 2, "end: stretch by len*end_fac, mode 2");
  CHECK(len_calls[0].extra == 3 && len_calls[1].extra == 6, "extra points = ceil(fac * point_density)");
  /* second_overshoot_fac = 0.1 * 9 / 9 * (1 - 0.1/10) = 0.099 */
  CHECK(NEAR(len_calls[1].overshoot, 0.099f), "second overshoot follows Blender's adjustment");
  CHECK(geometry_updates == 1, "geometry refreshed for the changed stroke");

  /* fractional density: 0.1 * 25 = 2.5 -> ceil 3 */
  p.point_density = 25.0f; len_ncalls = 0;
  pg_gp_mod_length(gpd, NULL, &p);
  CHECK(len_calls[0].extra == 3 && len_calls[1].extra == 5, "extra points round up (ceil)");
  p.point_density = 30.0f;

  /* negative start: shrink comes second after the swap */
  p.start_fac = -0.1f; p.end_fac = 0.3f; len_ncalls = 0;
  pg_gp_mod_length(gpd, NULL, &p);
  CHECK(len_calls[0].kind == 1 && len_calls[0].mode == 2 && NEAR(len_calls[0].dist, 30.0f), "swap: end stretch first");
  CHECK(len_calls[1].kind == 2 && len_calls[1].mode == 1 && NEAR(len_calls[1].dist, 10.0f), "then start shrink by |len*fac|");

  /* absolute mode uses len = 1 */
  p.mode = PG_LENGTH_ABSOLUTE; p.start_fac = 5.0f; p.end_fac = 0.0f; len_ncalls = 0;
  pg_gp_mod_length(gpd, NULL, &p);
  CHECK(len_ncalls == 1 && NEAR(len_calls[0].dist, 5.0f), "absolute: distance is the factor; zero end does nothing");

  select_points(gpd, a, 0); len_ncalls = 0;
  CHECK(pg_gp_mod_length(gpd, NULL, &p) == 0 && len_ncalls == 0, "unselected strokes are skipped");
  p.mode = 7;
  CHECK(pg_gp_mod_length(gpd, NULL, &p) == 0, "invalid mode rejected");
  const float args[9] = {0, 0.1f, 0.1f, 0.1f, 0, 30, 0, 2.97f, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_LENGTH, args, 8) == 0, "length needs 9 args");
}


static void test_tint_modifier(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 3, 0, 0, 0, 10, 0);
  bGPDstroke *b = add_stroke(f, 3, 0, 0, 20, 10, 0);
  MaterialGPencilStyle *st = gpd->mat[0]->gp_style;
  st->stroke_rgba[0] = 1; st->stroke_rgba[3] = 1;            /* red stroke material */
  st->fill_rgba[1] = 1; st->fill_rgba[3] = 1;                /* green fill material */
  for (int i = 0; i < 3; i++) a->points[i].strength = b->points[i].strength = 0.5f;
  select_points(gpd, a, 7);
  const float blue[3] = {0, 0, 1};

  CHECK(pg_gp_mod_tint(gpd, NULL, PG_PAINT_MODE_STROKE, 0.5f, blue) == 1, "tint stroke");
  CHECK(NEAR(a->points[1].vert_color[0], 0.5f) && NEAR(a->points[1].vert_color[2], 0.5f) &&
        NEAR(a->points[1].vert_color[3], 1.0f), "no vertex color: starts from the material color, mixes 50% blue");
  CHECK(a->vert_color_fill[3] == 0.0f, "stroke mode leaves the fill alone");
  CHECK(b->points[0].vert_color[3] == 0.0f, "unselected stroke untouched");

  pg_gp_mod_tint(gpd, NULL, PG_PAINT_MODE_FILL, 1.0f, blue);
  CHECK(NEAR(a->vert_color_fill[2], 1.0f) && NEAR(a->vert_color_fill[1], 0.0f) && NEAR(a->vert_color_fill[3], 1.0f),
        "fill mode: material fill green replaced by blue at factor 1");
  CHECK(NEAR(a->points[1].vert_color[0], 0.5f), "fill mode breaks before touching points");

  pg_gp_mod_tint(gpd, NULL, PG_PAINT_MODE_BOTH, 2.0f, blue);
  CHECK(NEAR(a->points[0].strength, 1.0f), "factor > 1 raises strength by factor-1 (clamped)");
  CHECK(NEAR(a->points[2].vert_color[2], 1.0f) && NEAR(a->points[2].vert_color[0], 0.0f), "color mix factor clamps to 1");
  CHECK(pg_gp_mod_tint(gpd, NULL, 5, 1.0f, blue) == 0, "invalid mode rejected");
  const float args[5] = {PG_PAINT_MODE_STROKE, 0.0f, 1, 0, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_TINT, args, 5) == 1, "dispatch tint");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_TINT, args, 4) == 0, "tint needs 5 args");
}


static void test_hsv_conversion(void)
{
  const float cases[][6] = {
      /* rgb -> expected hsv */
      {1, 0, 0, 0.0f, 1, 1},       {0, 1, 0, 1.0f / 3.0f, 1, 1}, {0, 0, 1, 2.0f / 3.0f, 1, 1},
      {1, 1, 0, 1.0f / 6.0f, 1, 1}, {0.5f, 0.5f, 0.5f, 0, 0, 0.5f}, {1, 0, 1, 5.0f / 6.0f, 1, 1},
  };
  for (int c = 0; c < 6; c++) {
    float hsv[3], rgb[3];
    pg_rgb_to_hsv(cases[c], hsv);
    CHECK(NEAR(hsv[0], cases[c][3]) && NEAR(hsv[1], cases[c][4]) && NEAR(hsv[2], cases[c][5]), "rgb_to_hsv known values");
    pg_hsv_to_rgb(hsv, rgb);
    CHECK(NEAR(rgb[0], cases[c][0]) && NEAR(rgb[1], cases[c][1]) && NEAR(rgb[2], cases[c][2]), "hsv_to_rgb round trip");
  }
}

static void test_color_modifier(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 2, 0, 0, 0, 10, 0);
  bGPDstroke *b = add_stroke(f, 2, 0, 0, 20, 10, 0);
  MaterialGPencilStyle *st = gpd->mat[0]->gp_style;
  st->stroke_rgba[0] = 1; st->stroke_rgba[3] = 1; /* red */
  st->fill_rgba[2] = 1; st->fill_rgba[3] = 1;     /* blue */
  select_points(gpd, a, 3);

  /* defaults (0.5, 1, 1) leave the color unchanged: hue + 0.5 + 0.5 wraps */
  const float identity[3] = {0.5f, 1.0f, 1.0f};
  CHECK(pg_gp_mod_color(gpd, NULL, PG_MODIFY_COLOR_BOTH, identity) == 1, "color modifier runs");
  CHECK(NEAR(a->points[0].vert_color[0], 1) && NEAR(a->points[0].vert_color[1], 0) && NEAR(a->points[0].vert_color[3], 1),
        "identity factors keep red (material color copied first)");
  CHECK(NEAR(a->vert_color_fill[2], 1) && NEAR(a->vert_color_fill[3], 1), "fill starts from the material fill color");

  /* hue +1/3 (factor 0.5 + 1/3): red -> green */
  const float to_green[3] = {0.5f + 1.0f / 3.0f, 1.0f, 1.0f};
  pg_gp_mod_color(gpd, NULL, PG_MODIFY_COLOR_STROKE, to_green);
  CHECK(NEAR(a->points[1].vert_color[1], 1) && NEAR(a->points[1].vert_color[0], 0), "hue shift red -> green");
  CHECK(NEAR(a->vert_color_fill[2], 1), "stroke mode leaves the fill");

  /* saturation 0 -> grey of value 1 = white; value 0.5 halves brightness */
  const float desat[3] = {0.5f, 0.0f, 0.5f};
  pg_gp_mod_color(gpd, NULL, PG_MODIFY_COLOR_FILL, desat);
  CHECK(NEAR(a->vert_color_fill[0], 0.5f) && NEAR(a->vert_color_fill[1], 0.5f) && NEAR(a->vert_color_fill[2], 0.5f),
        "fill desaturated and darkened");
  CHECK(NEAR(a->points[1].vert_color[1], 1), "fill mode leaves the points");
  CHECK(b->points[0].vert_color[3] == 0.0f, "unselected stroke untouched");
  CHECK(pg_gp_mod_color(gpd, NULL, PG_MODIFY_COLOR_HARDNESS, identity) == 0, "hardness is not a color mode here");
  const float args[4] = {PG_MODIFY_COLOR_BOTH, 0.5f, 1, 1};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_COLOR, args, 4) == 1, "dispatch color");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_MOD_COLOR, args, 3) == 0, "color needs 4 args");
}


static int order_of(const bGPDframe *f, const bGPDstroke *const *s, int n, int *out)
{
  int k = 0;
  for (const bGPDstroke *g = f->strokes.first; g; g = g->next)
    for (int i = 0; i < n; i++) if (s[i] == g) out[k++] = i;
  return k;
}

static void test_stroke_operators(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *s[4];
  for (int i = 0; i < 4; i++) s[i] = add_stroke(f, 3, 0, 0, i * 20.0f, 10, 0);
  int o[4];

  /* arrange: select 0 and 2 */
  select_points(gpd, s[0], 7); select_points(gpd, s[2], 7);
  CHECK(pg_gp_stroke_arrange(gpd, NULL, PG_ARRANGE_TOP) == 1, "arrange top");
  order_of(f, (const bGPDstroke *const *)s, 4, o);
  CHECK(o[0] == 1 && o[1] == 3 && o[2] == 0 && o[3] == 2, "top moves the selection to the end, keeping order");
  pg_gp_stroke_arrange(gpd, NULL, PG_ARRANGE_BOTTOM);
  order_of(f, (const bGPDstroke *const *)s, 4, o);
  CHECK(o[0] == 0 && o[1] == 2 && o[2] == 1 && o[3] == 3, "bottom moves the selection to the start, keeping order");
  pg_gp_stroke_arrange(gpd, NULL, PG_ARRANGE_UP);
  order_of(f, (const bGPDstroke *const *)s, 4, o);
  /* from the end: 2 steps over 1, then 0 steps over 1 (each selected stroke moves up one) */
  CHECK(o[0] == 1 && o[1] == 0 && o[2] == 2 && o[3] == 3, "up moves each selected stroke one step toward the top");
  pg_gp_stroke_arrange(gpd, NULL, PG_ARRANGE_DOWN);
  order_of(f, (const bGPDstroke *const *)s, 4, o);
  CHECK(o[0] == 0 && o[1] == 2 && o[2] == 1 && o[3] == 3, "down moves each selected stroke one step toward the bottom");
  CHECK(pg_gp_stroke_arrange(gpd, NULL, 9) == 0, "invalid direction");

  /* material: a locked target material makes the strokes non-editable afterwards */
  CHECK(pg_gp_stroke_set_material(gpd, NULL, 1) == 1 && pg_gp_stroke_flip(gpd, NULL) == 0, "strokes on a locked material are not edited");
  gpd->mat[1]->gp_style->flag = 0; /* unlock for the remaining checks */
  CHECK(pg_gp_stroke_set_material(gpd, NULL, 1) == 0 && s[0]->mat_nr == 1 && s[1]->mat_nr == 0, "assign material to selected");
  CHECK(pg_gp_stroke_set_material(gpd, NULL, 5) == 0, "material index out of range");

  /* reset vertex color */
  s[2]->points[0].vert_color[3] = 1; s[2]->vert_color_fill[3] = 1; s[1]->points[0].vert_color[3] = 1;
  pg_gp_stroke_reset_vertex_color(gpd, NULL, PG_PAINT_MODE_STROKE);
  CHECK(s[2]->points[0].vert_color[3] == 0 && s[2]->vert_color_fill[3] == 1, "stroke mode clears point colors only");
  pg_gp_stroke_reset_vertex_color(gpd, NULL, PG_PAINT_MODE_BOTH);
  CHECK(s[2]->vert_color_fill[3] == 0 && s[1]->points[0].vert_color[3] == 1, "both mode clears fill; unselected kept");

  /* flip */
  CHECK(pg_gp_stroke_flip(gpd, NULL) == 1 && NEAR(s[2]->points[0].x, 20) && NEAR(s[1]->points[0].x, 0), "flip reverses selected strokes");

  /* cyclic */
  geometry_updates = 0;
  CHECK(pg_gp_stroke_cyclical_set(gpd, NULL, PG_CYCLIC_CLOSE) == 1 && (s[2]->flag & GP_STROKE_CYCLIC) && !(s[1]->flag & GP_STROKE_CYCLIC), "close");
  CHECK(geometry_updates == 2, "geometry refreshed per changed stroke");
  CHECK(pg_gp_stroke_cyclical_set(gpd, NULL, PG_CYCLIC_CLOSE) == 0, "closing closed strokes is no change");
  pg_gp_stroke_cyclical_set(gpd, NULL, PG_CYCLIC_TOGGLE);
  CHECK(!(s[2]->flag & GP_STROKE_CYCLIC), "toggle opens");

  /* snap to grid: only selected points */
  s[2]->points[2].flag &= ~GP_SPOINT_SELECT; s[2]->points[2].x = 17; /* unselected point of a selected stroke */
  s[2]->points[1].x = 13; s[2]->points[1].y = 47;
  s[2]->points[1].flag |= GP_SPOINT_SELECT;
  s[1]->points[0].x = 13;
  CHECK(pg_gp_snap_to_grid(gpd, NULL, 10) == 1 && NEAR(s[2]->points[1].x, 10) && NEAR(s[2]->points[1].y, 50), "snap selected points to grid");
  CHECK(NEAR(s[1]->points[0].x, 13), "unselected points not snapped");
  CHECK(NEAR(s[2]->points[2].x, 17), "unselected point of a selected stroke not snapped");
  CHECK(pg_gp_snap_to_grid(gpd, NULL, 0) == 0, "zero grid rejected");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_FLIP, NULL, 0) == 1, "dispatch flip");
  const float a1[1] = {PG_ARRANGE_TOP};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_ARRANGE, a1, 1) == 1, "dispatch arrange");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_SNAP_GRID, NULL, 0) == 0, "snap needs a grid size");
}


static void test_stroke_operator_edges(void)
{
  bGPdata *gpd = make_gpd();
  bGPDframe *f = add_frame(add_layer(gpd, 0));
  bGPDstroke *s[3];
  for (int i = 0; i < 3; i++) s[i] = add_stroke(f, 3, 0, 0, i * 20.0f, 10, 0);
  select_points(gpd, s[1], 7); select_points(gpd, s[2], 7);
  int o[3];
  CHECK(pg_gp_stroke_arrange(gpd, NULL, PG_ARRANGE_UP) == 0, "selected strokes already at the top cannot move up");
  order_of(f, (const bGPDstroke *const *)s, 3, o);
  CHECK(o[0] == 0 && o[1] == 1 && o[2] == 2, "a selected stroke does not jump over another selected one");
  bGPDstroke *two = add_stroke(f, 2, 0, 0, 90, 10, 0);
  select_points(gpd, two, 3);
  pg_gp_stroke_cyclical_set(gpd, NULL, PG_CYCLIC_CLOSE);
  CHECK(!(two->flag & GP_STROKE_CYCLIC), "strokes with fewer than 3 points are not closed");
}


static void test_structure_operators(void)
{
  /* duplicate: two runs of selected points -> stand-in keeps them as one compacted copy */
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 5, 0, 0, 0, 10, 0);
  bGPDstroke *b = add_stroke(f, 3, 0, 0, 50, 10, 0);
  select_points(gpd, a, (1u << 1) | (1u << 2));
  CHECK(pg_gp_duplicate(gpd, NULL) == 1, "duplicate");
  CHECK(stroke_count(f) == 3 && a->next != b, "copy inserted right after the original");
  bGPDstroke *copy = a->next;
  CHECK(copy->totpoints == 2 && NEAR(copy->points[0].x, 10) && NEAR(copy->points[1].x, 20), "copy holds only the selected points");
  CHECK(sel_mask(copy) == 3 && (copy->flag & GP_STROKE_SELECT), "the copy is selected");
  CHECK(sel_mask(a) == 0 && !(a->flag & GP_STROKE_SELECT) && a->totpoints == 5, "original kept and deselected");
  CHECK(sel_mask(b) == 0, "unselected stroke untouched");

  /* dissolve points */
  bGPdata *g2 = make_gpd();
  bGPDframe *f2 = add_frame(add_layer(g2, 0));
  bGPDstroke *d = add_stroke(f2, 5, 0, 0, 0, 10, 0);
  select_points(g2, d, (1u << 1) | (1u << 3));
  geometry_updates = 0;
  CHECK(pg_gp_dissolve(g2, NULL, PG_DISSOLVE_POINTS) == 1 && d->totpoints == 3, "dissolve removes selected points");
  CHECK(NEAR(d->points[0].x, 0) && NEAR(d->points[1].x, 20) && NEAR(d->points[2].x, 40) && stroke_count(f2) == 1,
        "remaining points stay in one stroke (no split)");
  CHECK(geometry_updates == 1, "geometry refreshed");
  /* dissolve between: first and last selected kept, unselected inside removed */
  bGPDstroke *e = add_stroke(f2, 5, 0, 0, 20, 10, 0);
  select_points(g2, e, (1u << 0) | (1u << 3));
  pg_gp_dissolve(g2, NULL, PG_DISSOLVE_BETWEEN);
  CHECK(e->totpoints == 3 && NEAR(e->points[1].x, 30) && NEAR(e->points[2].x, 40), "between removes the unselected points in the range");
  /* dissolve unselected */
  select_points(g2, e, 1u << 1);
  pg_gp_dissolve(g2, NULL, PG_DISSOLVE_UNSELECT);
  CHECK(e->totpoints == 1 && NEAR(e->points[0].x, 30), "unselect keeps only selected points");
  /* everything selected and dissolved -> stroke deleted */
  select_points(g2, e, 1);
  pg_gp_dissolve(g2, NULL, PG_DISSOLVE_POINTS);
  CHECK(stroke_count(f2) == 1, "a stroke with no points left is deleted");
  CHECK(pg_gp_dissolve(g2, NULL, 7) == 0, "invalid dissolve type");

  /* split: selected points move to a new stroke, the original loses them */
  bGPdata *g3 = make_gpd();
  bGPDframe *f3 = add_frame(add_layer(g3, 0));
  bGPDstroke *s = add_stroke(f3, 4, 0, 0, 0, 10, 0);
  select_points(g3, s, (1u << 2) | (1u << 3));
  CHECK(pg_gp_split(g3, NULL) == 1 && stroke_count(f3) == 2, "split creates a stroke");
  CHECK(s->totpoints == 2 && NEAR(s->points[1].x, 10), "original keeps the unselected points");
  CHECK(s->next->totpoints == 2 && NEAR(s->next->points[0].x, 20) && sel_mask(s->next) == 3, "new stroke has the selected points, selected");
  select_points(g3, s, 3);
  select_points(g3, s->next, 0);
  CHECK(pg_gp_split(g3, NULL) == 0, "a fully selected stroke is not split");

  /* join */
  bGPdata *g4 = make_gpd();
  bGPDframe *f4 = add_frame(add_layer(g4, 0));
  bGPDstroke *j1 = add_stroke(f4, 2, 0, 0, 0, 10, 0);
  bGPDstroke *j2 = add_stroke(f4, 3, 0, 0, 20, 10, 0);
  bGPDstroke *j3 = add_stroke(f4, 2, 0, 0, 40, 10, 0);
  select_points(g4, j1, 3); select_points(g4, j3, 3);
  join_calls = 0;
  CHECK(pg_gp_join(g4, NULL, 0) == 1 && join_calls == 1, "join merges selected strokes");
  CHECK(stroke_count(f4) == 2 && f4->strokes.first == j1 && j1->next == j2 && j1->totpoints == 4, "into the first selected; unselected stays");
  CHECK(pg_gp_join(g4, NULL, 0) == 0, "a single selected stroke has nothing to join");

  const float dz[1] = {PG_DISSOLVE_POINTS};
  CHECK(pg_gp_edit_dispatch(g4, NULL, PG_EDIT_CMD_DISSOLVE, dz, 0) == 0, "dissolve needs a type");
}


static void test_dissolve_keeps_weights_aligned(void)
{
  bGPdata *gpd = make_gpd();
  bGPDframe *f = add_frame(add_layer(gpd, 0));
  bGPDstroke *d = add_stroke(f, 4, 0, 0, 0, 10, 0);
  d->dvert = calloc(4, sizeof(MDeformVert));
  for (int i = 0; i < 4; i++) {
    d->dvert[i].totweight = 1;
    d->dvert[i].dw = calloc(1, sizeof(MDeformWeight));
    d->dvert[i].dw->weight = (float)i / 10.0f; /* weight tags the original index */
  }
  select_points(gpd, d, (1u << 1) | (1u << 2));
  const int frees_before = pg_test_mem_free_count;
  CHECK(pg_gp_dissolve(gpd, NULL, PG_DISSOLVE_POINTS) == 1 && d->totpoints == 2, "weighted stroke is dissolved, not skipped");
  CHECK(pg_test_mem_free_count - frees_before == 2, "the weights of both removed points are freed (no leak)");
  CHECK(NEAR(d->dvert[0].dw->weight, 0.0f) && NEAR(d->dvert[1].dw->weight, 0.3f), "weights stay with their points");
  for (int i = 0; i < 2; i++) free(d->dvert[i].dw);
  free(d->dvert);
  d->dvert = NULL;
}

static void test_vertex_paint(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 5, 0, 0, 0, 10, 0); /* x = 0..40 */
  MaterialGPencilStyle *st = gpd->mat[0]->gp_style;
  st->stroke_rgba[0] = 1; st->stroke_rgba[3] = 1;   /* red material */
  st->fill_rgba[1] = 1; st->fill_rgba[3] = 1;       /* green fill */

  /* Draw: centre gets full influence, edge none; color starts from the material */
  PGVertexPaint vp = {PG_VPAINT_DRAW, 20, 0, 20, 1.0f, {0, 0, 1}, PG_PAINT_MODE_STROKE, 0, 0};
  CHECK(pg_gp_vertex_paint(gpd, NULL, &vp) == 1, "draw dab");
  CHECK(NEAR(a->points[2].vert_color[2], 1) && NEAR(a->points[2].vert_color[3], 1), "centre point fully blue");
  CHECK(a->points[1].vert_color[2] > 0.1f && a->points[1].vert_color[2] < 0.9f && a->points[1].vert_color[0] > 0.1f,
        "half-radius point is a red/blue mix (smooth falloff)");
  CHECK(a->points[0].vert_color[3] == 0.0f && a->points[4].vert_color[3] == 0.0f, "points at the radius are untouched");
  CHECK(a->vert_color_fill[3] == 0.0f, "stroke target leaves the fill");
  /* falloff value: t = 0.5 -> 3*0.25 - 2*0.125 = 0.5 */
  CHECK(NEAR(a->points[1].vert_color[3], 0.5f), "smooth falloff at half radius is 0.5");
  {
    bGPDstroke *q = add_stroke(f, 4, 0, 0, 300, 10, 0); /* x = 0..30 */
    PGVertexPaint qv = {PG_VPAINT_DRAW, 0, 300, 40, 1.0f, {0, 0, 1}, PG_PAINT_MODE_STROKE, 0, 0};
    pg_gp_vertex_paint(gpd, NULL, &qv);
    /* d = 30, t = 0.25 -> 3*0.0625 - 2*0.015625 = 0.15625 (a straight line would give 0.25) */
    CHECK(NEAR(q->points[3].vert_color[3], 0.15625f), "Blender's smooth curve, not linear, at a quarter radius");
  }

  /* Replace only touches points that already have vertex color */
  bGPDstroke *b = add_stroke(f, 3, 0, 0, 50, 10, 0);
  vp = (PGVertexPaint){PG_VPAINT_REPLACE, 10, 50, 15, 1.0f, {0, 1, 0}, PG_PAINT_MODE_STROKE, 0, 0};
  CHECK(pg_gp_vertex_paint(gpd, NULL, &vp) == 0 && b->points[1].vert_color[3] == 0.0f, "replace ignores unpainted points");
  vp = (PGVertexPaint){PG_VPAINT_REPLACE, 20, 0, 20, 1.0f, {0, 1, 0}, PG_PAINT_MODE_STROKE, 0, 0};
  pg_gp_vertex_paint(gpd, NULL, &vp);
  CHECK(NEAR(a->points[2].vert_color[1], 1) && NEAR(a->points[2].vert_color[2], 0), "replace recolors painted points");

  /* Average pulls painted points toward their mean */
  a->points[1].vert_color[0] = 1; a->points[1].vert_color[1] = 0; a->points[1].vert_color[2] = 0; a->points[1].vert_color[3] = 1;
  a->points[3].vert_color[0] = 0; a->points[3].vert_color[1] = 0; a->points[3].vert_color[2] = 1; a->points[3].vert_color[3] = 1;
  vp = (PGVertexPaint){PG_VPAINT_AVERAGE, 20, 0, 100, 1.0f, {0, 0, 0}, PG_PAINT_MODE_STROKE, 0, 0};
  CHECK(pg_gp_vertex_paint(gpd, NULL, &vp) == 1, "average dab");
  CHECK(a->points[1].vert_color[2] > 0.0f && a->points[3].vert_color[0] > 0.0f, "painted points move toward the mean");

  /* Blur mixes with neighbours */
  bGPDstroke *c = add_stroke(f, 3, 0, 0, 100, 10, 0);
  c->points[0].vert_color[0] = 1; c->points[0].vert_color[3] = 1;
  c->points[2].vert_color[2] = 1; c->points[2].vert_color[3] = 1;
  vp = (PGVertexPaint){PG_VPAINT_BLUR, 10, 100, 30, 1.0f, {0, 0, 0}, PG_PAINT_MODE_STROKE, 0, 0};
  pg_gp_vertex_paint(gpd, NULL, &vp);
  CHECK(c->points[1].vert_color[0] > 0.0f && c->points[1].vert_color[2] > 0.0f, "blur brings neighbour colors into the middle point");

  /* Smear drags color along the movement: moving +x pulls from the left neighbour */
  bGPDstroke *d = add_stroke(f, 3, 0, 0, 150, 10, 0);
  d->points[0].vert_color[1] = 1; d->points[0].vert_color[3] = 1;
  vp = (PGVertexPaint){PG_VPAINT_SMEAR, 10, 150, 30, 1.0f, {0, 0, 0}, PG_PAINT_MODE_STROKE, 5, 0};
  pg_gp_vertex_paint(gpd, NULL, &vp);
  CHECK(d->points[1].vert_color[1] > 0.5f, "smear moves the left color into the middle when dragging right");
  CHECK(d->points[2].vert_color[1] == 0.0f || d->points[2].vert_color[3] == 0.0f, "...but not past an unpainted source");
  vp.dx = 0; vp.dy = 0;
  CHECK(pg_gp_vertex_paint(gpd, NULL, &vp) == 0, "smear without movement does nothing");

  /* Fill target */
  vp = (PGVertexPaint){PG_VPAINT_DRAW, 20, 0, 20, 1.0f, {1, 1, 0}, PG_PAINT_MODE_FILL, 0, 0};
  float before = a->points[2].vert_color[0];
  pg_gp_vertex_paint(gpd, NULL, &vp);
  CHECK(NEAR(a->vert_color_fill[0], 1) && NEAR(a->vert_color_fill[1], 1) && NEAR(a->vert_color_fill[3], 1), "fill painted from the strongest influence");
  CHECK(NEAR(a->points[2].vert_color[0], before), "fill target leaves the points");

  vp.brush = 9;
  CHECK(pg_gp_vertex_paint(gpd, NULL, &vp) == 0, "invalid brush");
  const float args[11] = {PG_VPAINT_DRAW, 20, 0, 20, 1, 1, 0, 0, PG_PAINT_MODE_BOTH, 0, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_VERTEX_PAINT, args, 11) == 1, "dispatch vertex paint");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_VERTEX_PAINT, args, 10) == 0, "vertex paint needs 11 args");
}

static void test_easing(void)
{
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_LINEAR, PG_EASE_IN, 0.3f, 1.70158f), 0.3f), "linear");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_QUAD, PG_EASE_IN, 0.5f, 0), 0.25f), "quad in");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_QUAD, PG_EASE_OUT, 0.5f, 0), 0.75f), "quad out");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_CUBIC, PG_EASE_IN_OUT, 0.25f, 0), 0.0625f), "cubic in-out first half");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_CUBIC, PG_EASE_IN_OUT, 0.75f, 0), 0.9375f), "cubic in-out second half");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_SINE, PG_EASE_IN, 0.5f, 0), 1.0f - cosf((float)M_PI_4)), "sine in");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_EXPO, PG_EASE_IN, 0.0f, 0), 0.0f), "expo in at 0");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_CIRC, PG_EASE_IN, 0.6f, 0), 0.2f), "circ in: 1 - sqrt(1 - 0.36)");
  CHECK(pg_gp_interpolate_easing(PG_EASE_BACK, PG_EASE_IN, 0.2f, 1.70158f) < 0.0f, "back overshoots below 0 on ease-in");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_BOUNCE, PG_EASE_OUT, 1.0f, 0), 1.0f), "bounce ends at 1");
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_BOUNCE, PG_EASE_OUT, 0.5f, 0), 0.765625f), "bounce out at 0.5 (Penner)");
  for (int type = PG_EASE_LINEAR; type <= PG_EASE_BOUNCE; type++)
    for (int mode = 0; mode < 3; mode++) {
      CHECK(NEAR(pg_gp_interpolate_easing(type, mode, 0.0f, 1.70158f), 0.0f), "every curve starts at 0");
      CHECK(NEAR(pg_gp_interpolate_easing(type, mode, 1.0f, 1.70158f), 1.0f), "every curve ends at 1");
    }
  CHECK(NEAR(pg_gp_interpolate_easing(PG_EASE_QUAD, PG_EASE_IN, 2.0f, 0), 1.0f), "t is clamped");
}

static void test_mirror_copy(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 2, 0, 10, 20, 10, 0); /* (10,20) (20,20) */
  bGPDstroke *b = add_stroke(f, 2, 0, 0, 90, 10, 0);
  select_points(gpd, a, 3);
  CHECK(pg_gp_mirror_copy(gpd, NULL, 1, 0, 50, 0) == 1 && stroke_count(f) == 3, "mirror X adds one copy");
  CHECK(a->next != b && NEAR(a->next->points[0].x, 90) && NEAR(a->next->points[1].x, 80) && NEAR(a->next->points[0].y, 20),
        "copy reflected across x = 50, right after the original");
  CHECK(sel_mask(a->next) == 0 && sel_mask(a) == 3, "original stays selected, copy does not");
  CHECK(NEAR(a->points[0].x, 10), "original unchanged");
  pg_gp_mirror_copy(gpd, NULL, 1, 1, 50, 50);
  CHECK(stroke_count(f) == 6, "both axes add three copies (X, Y, XY)");
  CHECK(pg_gp_mirror_copy(gpd, NULL, 0, 0, 0, 0) == 0, "no axis, no copies");
}

static void test_weight_paint(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 5, 0, 0, 0, 10, 0); /* x = 0..40 */
  CHECK(pg_gp_weight_paint(gpd, NULL, 0, 20, 0, 20, 1.0f, 1.0f) == 1 && a->dvert != NULL, "weights allocated on first paint");
  CHECK(NEAR(a->dvert[2].dw[0].weight, 1.0f) && NEAR(a->dvert[1].dw[0].weight, 0.5f), "draw weight with smooth falloff");
  CHECK(a->dvert[0].totweight == 0 && a->dvert[4].totweight == 0, "points outside the brush get no weight entry");
  pg_gp_weight_paint(gpd, NULL, 3, 20, 0, 20, 1.0f, 0.25f);
  CHECK(a->dvert[2].totweight == 2 && a->dvert[2].dw[1].def_nr == 3 && NEAR(a->dvert[2].dw[1].weight, 0.25f), "second group added beside the first");
  pg_gp_weight_paint(gpd, NULL, 0, 20, 0, 20, 1.0f, 0.0f);
  CHECK(NEAR(a->dvert[2].dw[0].weight, 0.0f) && NEAR(a->dvert[2].dw[1].weight, 0.25f), "painting group 0 leaves group 3");
  CHECK(pg_gp_weight_paint(gpd, NULL, -1, 20, 0, 20, 1, 1) == 0, "invalid group");
  const float args[6] = {0, 20, 0, 20, 1, 1};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT_CMD_WEIGHT_PAINT, args, 6) == 1, "dispatch weight paint");
  for (int i = 0; i < a->totpoints; i++) free(a->dvert[i].dw);
  free(a->dvert); a->dvert = NULL;
}


static MDeformVert *weights(int n, const float *w)
{
  MDeformVert *dv = calloc((size_t)n, sizeof(MDeformVert));
  for (int i = 0; i < n; i++) {
    if (w[i] < 0.0f) continue; /* no entry for group 0 */
    dv[i].totweight = 1;
    dv[i].dw = calloc(1, sizeof(MDeformWeight));
    dv[i].dw->def_nr = 0;
    dv[i].dw->weight = w[i];
  }
  return dv;
}

static void test_point_weight(void)
{
  const float w[2] = {0.25f, -1.0f};
  MDeformVert *dv = weights(2, w);
  CHECK(NEAR(pg_gp_modifier_point_weight(NULL, 0, -1), 1.0f), "no group: weight 1");
  CHECK(NEAR(pg_gp_modifier_point_weight(&dv[0], 0, 0), 0.25f), "weight of the group");
  CHECK(pg_gp_modifier_point_weight(&dv[1], 0, 0) < 0.0f, "point not in the group is skipped");
  CHECK(pg_gp_modifier_point_weight(&dv[0], 1, 0) < 0.0f, "inverted: points in the group are skipped");
  CHECK(NEAR(pg_gp_modifier_point_weight(&dv[1], 1, 0), 1.0f), "inverted: points outside count fully");
  CHECK(pg_gp_modifier_point_weight(NULL, 0, 0) < 0.0f && NEAR(pg_gp_modifier_point_weight(NULL, 1, 0), 1.0f),
        "stroke without weights: skipped, or full when inverted");
  free(dv[0].dw); free(dv);
}

static void test_thickness_vgroup(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDstroke *a = add_stroke(add_frame(l), 3, 0, 0, 0, 10, 0);
  for (int i = 0; i < 3; i++) a->points[i].pressure = 1.0f;
  const float w[3] = {1.0f, 0.5f, -1.0f};
  a->dvert = weights(3, w);
  select_points(gpd, a, 7);
  CHECK(pg_gp_mod_thickness_vgroup(gpd, NULL, 0, 0, 0, 0, 2.0f) == 1, "weighted thickness");
  CHECK(NEAR(a->points[0].pressure, 2.0f) && NEAR(a->points[1].pressure, 1.5f) && NEAR(a->points[2].pressure, 1.0f),
        "full weight doubles, half weight goes halfway, ungrouped point untouched");
  pg_gp_mod_thickness_vgroup(gpd, NULL, 0, 1, 0, 0, 0.5f);
  CHECK(NEAR(a->points[2].pressure, 0.5f) && NEAR(a->points[0].pressure, 2.0f), "inverted group affects only the others");
  const float args[5] = {0, 0, 0, 0, 1.0f};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT2_CMD_MOD_THICKNESS_VGROUP, args, 5) == 0, "factor 1 changes nothing (routed through edit.c)");
  for (int i = 0; i < 2; i++) free(a->dvert[i].dw);
  free(a->dvert); a->dvert = NULL;
}

static void test_edit2_operators(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 5, 0, 0, 0, 10, 0);
  /* select vertex color: red-ish points */
  a->points[0].vert_color[0] = 1; a->points[0].vert_color[3] = 1;                                 /* red, hue 0 */
  a->points[1].vert_color[0] = 1; a->points[1].vert_color[2] = 0.2f; a->points[1].vert_color[3] = 1; /* hue ~0.97 */
  a->points[2].vert_color[1] = 1; a->points[2].vert_color[3] = 1;                                 /* green */
  const float red[3] = {1, 0, 0};
  CHECK(pg_gp_select_vertex_color(gpd, NULL, red, 0.05f, 0) == 1, "select vertex color");
  CHECK(sel_mask(a) == 3 && (a->flag & GP_STROKE_SELECT), "similar hues across the 0/1 wrap selected, green and unpainted not");

  /* normalize thickness/opacity on selected points */
  CHECK(pg_gp_stroke_normalize(gpd, NULL, 0, 0.3f) == 1 && NEAR(a->points[1].pressure, 0.3f) && NEAR(a->points[2].pressure, 0.0f),
        "normalize thickness of selected points only");
  pg_gp_stroke_normalize(gpd, NULL, 1, 2.0f);
  CHECK(NEAR(a->points[0].strength, 1.0f), "opacity clamps to 1");
  CHECK(pg_gp_stroke_normalize(gpd, NULL, 3, 1.0f) == 0, "invalid mode");

  /* simplify fixed and sample call Blender's functions on selected strokes */
  simplify_calls = 0;
  CHECK(pg_gp_stroke_simplify_fixed(gpd, NULL, 2) == 1 && simplify_calls == 2 && a->totpoints == 2, "simplify fixed twice");
  bGPDstroke *b = add_stroke(f, 6, 0, 0, 50, 10, 0);
  sample_calls = 0;
  CHECK(pg_gp_stroke_sample(gpd, NULL, 5.0f, 0.1f) == 1 && sample_calls == 1, "sample only the selected stroke");
  CHECK(pg_gp_stroke_sample(gpd, NULL, 0.0f, 0.1f) == 0, "zero length rejected");

  /* extrude both ends of a selected stroke */
  select_points(gpd, a, 0); select_points(gpd, b, (1u << 0) | (1u << 5));
  geometry_updates = 0;
  CHECK(pg_gp_extrude(gpd, NULL) == 1 && b->totpoints == 8, "extrude adds a point at each selected end");
  CHECK(sel_mask(b) == ((1u << 0) | (1u << 7)) && NEAR(b->points[7].x, 50) && NEAR(b->points[0].x, 0),
        "new end points are selected at the old end positions; old ends deselected");
  CHECK(geometry_updates == 1 && a->totpoints == 2, "only the changed stroke updated; unselected stroke untouched");
  b->flag |= GP_STROKE_CYCLIC;
  CHECK(pg_gp_extrude(gpd, NULL) == 0, "closed strokes have no ends to extrude");

  const float sv[5] = {1, 0, 0, 0.05f, 1};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT2_CMD_SELECT_VCOLOR, sv, 4) == 0, "select vcolor needs 5 args");
  CHECK(pg_gp_edit_dispatch(gpd, l, 66, NULL, 0) == 0, "unknown id beyond the range");
}

static void test_edit3(void)
{
  /* select random draws from the real pinned BLI_rng (rand.cc): same sequence as lrand48 */
  RNG *rng = BLI_rng_new(12345);
  CHECK(BLI_rng_get_int(rng) == 483889296 && BLI_rng_get_int(rng) == 1973930609 && BLI_rng_get_int(rng) == 444188209,
        "rng matches the drand48 sequence");
  BLI_rng_seed(rng, 7);
  for (int i = 0; i < 100; i++) { float f = BLI_rng_get_float(rng); if (f < 0.0f || f >= 1.0f) { CHECK(0, "rng float in [0,1)"); break; } }
  BLI_rng_free(rng);

  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  bGPDstroke *a = add_stroke(f, 200, 0, 0, 0, 1, 0);
  CHECK(pg_gp_select_random(gpd, NULL, 0.5f, 3, 1) == 1, "select random");
  int n = 0; for (int i = 0; i < 200; i++) n += (a->points[i].flag & GP_SPOINT_SELECT) != 0;
  CHECK(n > 70 && n < 130 && (a->flag & GP_STROKE_SELECT), "about half selected; stroke flag synced");
  unsigned m1 = sel_mask(a);
  select_points(gpd, a, 0); pg_gp_select_random(gpd, NULL, 0.5f, 3, 1);
  CHECK(sel_mask(a) == m1, "same seed, same selection");
  CHECK(pg_gp_select_random(gpd, NULL, 0.0f, 3, 1) == 0, "ratio 0 changes nothing");
  pg_gp_select_random(gpd, NULL, 1.0f, 1, 0);
  CHECK(sel_mask(a) == 0 && !(a->flag & GP_STROKE_SELECT), "ratio 1 deselect clears all");

  /* blank keyframe: shifts later frames when the current one is occupied */
  bGPDframe *f2 = BKE_gpencil_frame_addnew(l, 3);
  CHECK(pg_gp_blank_frame_add(gpd, l, 1) == 1, "insert blank at occupied frame 1");
  CHECK(f->framenum == 2 && f2->framenum == 4 && l->actframe->framenum == 1 && l->actframe->strokes.first == NULL,
        "frames at/after 1 moved one later; new empty active frame at 1");
  CHECK(pg_gp_blank_frame_add(gpd, l, 10) == 1 && f2->framenum == 4, "free frame: nothing shifts");

  /* fill color */
  const float rgba[4] = {0.2f, 0.4f, 0.6f, 2.0f};
  CHECK(pg_gp_material_fill_color(gpd, 0, rgba) == 1 && NEAR(gpd->mat[0]->gp_style->fill_rgba[1], 0.4f) &&
        NEAR(gpd->mat[0]->gp_style->fill_rgba[3], 1.0f), "fill color set separately (clamped)");
  CHECK(gpd->mat[0]->gp_style->stroke_rgba[0] == 0.0f, "stroke color untouched");
  CHECK(pg_gp_material_fill_color(gpd, 9, rgba) == 0, "bad material index");

  /* clean loose */
  bGPdata *g2 = make_gpd();
  bGPDlayer *l2 = add_layer(g2, 0);
  bGPDframe *f3 = add_frame(l2);
  add_stroke(f3, 1, 0, 0, 0, 0, 0);
  bGPDstroke *keep = add_stroke(f3, 3, 0, 0, 0, 1, 0);
  add_stroke(f3, 2, 0, 0, 0, 1, 0);
  CHECK(pg_gp_frame_clean_loose(g2, NULL, 2) == 1 && stroke_count(f3) == 1 && f3->strokes.first == keep,
        "strokes with <= limit points removed");

  /* clean duplicate frames */
  bGPDframe *d1 = BKE_gpencil_frame_addnew(l2, 5);
  bGPDframe *d2 = BKE_gpencil_frame_addnew(l2, 6);
  bGPDframe *d3 = BKE_gpencil_frame_addnew(l2, 7);
  add_stroke(d1, 2, 0, 5, 5, 1, 0); add_stroke(d2, 2, 0, 5, 5, 1, 0); add_stroke(d3, 2, 0, 9, 9, 1, 0);
  l2->actframe = d2;
  CHECK(pg_gp_frame_clean_duplicate(g2, l2) == 1, "clean duplicate frames");
  int frames = 0; for (bGPDframe *x = l2->frames.first; x; x = x->next) frames++;
  CHECK(frames == 3 && d1->next == d3 && l2->actframe == d1, "identical next frame removed, first kept, active moved");

  /* vertex color operators on selected points */
  bGPdata *g3 = make_gpd();
  bGPDlayer *l3 = add_layer(g3, 0);
  bGPDstroke *v = add_stroke(add_frame(l3), 3, 0, 0, 0, 10, 0);
  select_points(g3, v, (1u << 0) | (1u << 1));
  const float red[3] = {1, 0, 0};
  CHECK(pg_gp_vcolor_set(g3, NULL, PG_PAINT_MODE_STROKE, red, 1.0f) == 1 && NEAR(v->points[0].vert_color[0], 1) &&
        NEAR(v->points[0].vert_color[3], 1) && v->points[2].vert_color[3] == 0.0f, "set on selected points only");
  pg_gp_vcolor_invert(g3, NULL, PG_PAINT_MODE_STROKE);
  CHECK(NEAR(v->points[0].vert_color[0], 0) && NEAR(v->points[0].vert_color[1], 1), "invert");
  CHECK(v->points[2].vert_color[0] == 0.0f && v->points[2].vert_color[3] == 0.0f, "unpainted unselected point untouched");
  pg_gp_vcolor_set(g3, NULL, PG_PAINT_MODE_STROKE, red, 1.0f);
  pg_gp_vcolor_hsv(g3, NULL, PG_PAINT_MODE_STROKE, 0.5f + 1.0f / 3.0f, 1, 1);
  CHECK(NEAR(v->points[0].vert_color[1], 1) && NEAR(v->points[0].vert_color[0], 0), "hsv: hue +1/3 turns red green");
  pg_gp_vcolor_levels(g3, NULL, PG_PAINT_MODE_STROKE, 0.0f, 0.5f);
  CHECK(NEAR(v->points[0].vert_color[1], 0.5f), "levels gain halves");
  pg_gp_vcolor_brightness_contrast(g3, NULL, PG_PAINT_MODE_STROKE, 0.25f, 0.0f);
  CHECK(NEAR(v->points[0].vert_color[1], 0.75f) && NEAR(v->points[0].vert_color[0], 0.25f), "brightness +0.25 at zero contrast");
  pg_gp_vcolor_brightness_contrast(g3, NULL, PG_PAINT_MODE_STROKE, 0.0f, 0.5f);
  /* contrast 0.5: gain = 1/(1-0.5) = 2, offset = 2*(0-0.25) = -0.5 -> 0.75 -> 1.0, 0.25 -> 0.0 */
  CHECK(NEAR(v->points[0].vert_color[1], 1.0f) && NEAR(v->points[0].vert_color[0], 0.0f), "contrast stretches around 0.5");
  CHECK(pg_gp_vcolor_invert(g3, NULL, 5) == 0, "invalid mode");

  const float sr[3] = {0.5f, 3, 1};
  l->actframe = f; /* the blank-frame checks made an empty frame active */
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT3_CMD_SELECT_RANDOM, sr, 3) == 1, "routed edit.c -> edit2.c -> edit3.c");
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT3_CMD_VCOLOR_SET, sr, 3) == 0, "vcolor set needs 5 args");
  CHECK(pg_gp_edit_dispatch(gpd, l, 81, NULL, 0) == 0, "beyond the routed range");
}

static int frame_stroke_count(bGPDlayer *l) { return l->actframe ? stroke_count(l->actframe) : 0; }

static void test_edit4(void)
{
  bGPdata *gpd = make_gpd();
  bGPDlayer *l = add_layer(gpd, 0);
  bGPDframe *f = add_frame(l);
  /* dash: 10 points, dash 3 gap 2 -> runs [0..2] [5..7]; run starting at offset handled */
  bGPDstroke *a = add_stroke(f, 10, 0, 0, 0, 10, 0);
  select_points(gpd, a, 0x3FF);
  CHECK(pg_gp_dash(gpd, NULL, 3, 2, 0) == 1 && stroke_count(f) == 2, "dash 3/2 on 10 points -> 2 dashes");
  bGPDstroke *d1 = f->strokes.first;
  CHECK(d1->totpoints == 3 && NEAR(d1->points[0].x, 0) && d1->next->totpoints == 3 && NEAR(d1->next->points[0].x, 50),
        "dash pieces cover points 0-2 and 5-7");
  CHECK(pg_gp_dash(gpd, NULL, 0, 2, 0) == 0 && pg_gp_dash(gpd, NULL, 3, 0, 0) == 0, "invalid dash/gap");
  {
    bGPDframe *fd = add_frame(add_layer(gpd, 0));
    bGPDstroke *one = add_stroke(fd, 6, 0, 0, 0, 10, 0);
    select_points(gpd, one, 0x3F);
    CHECK(pg_gp_dash(gpd, fd->strokes.first ? NULL : NULL, 1, 1, 0) == 0 || stroke_count(fd) == 1,
          "1-point dashes draw nothing, so the stroke is kept");
    CHECK(stroke_count(fd) == 1 && ((bGPDstroke *)fd->strokes.first)->totpoints == 6, "stroke unchanged");
  }

  /* multiply: 2 copies at +d and -d along the normal of a horizontal stroke */
  bGPdata *g2 = make_gpd(); bGPDlayer *l2 = add_layer(g2, 0); bGPDframe *f2 = add_frame(l2);
  bGPDstroke *m = add_stroke(f2, 3, 0, 0, 0, 10, 0);
  select_points(g2, m, 7);
  CHECK(pg_gp_multiply(g2, NULL, 2, 5.0f) == 1 && stroke_count(f2) == 3, "multiply 2");
  CHECK(NEAR(m->next->points[1].y, 5.0f) && NEAR(m->next->next->points[1].y, -5.0f) && sel_mask(m->next) == 0,
        "copies offset +d and -d along the normal, unselected");
  CHECK(NEAR(m->points[1].y, 0.0f), "original unchanged");

  /* array */
  bGPdata *g3 = make_gpd(); bGPDlayer *l3 = add_layer(g3, 0); bGPDframe *f3 = add_frame(l3);
  bGPDstroke *r = add_stroke(f3, 2, 0, 0, 0, 10, 0);
  select_points(g3, r, 3);
  CHECK(pg_gp_array(g3, NULL, 3, 100, 0) == 1 && stroke_count(f3) == 3 && NEAR(r->next->next->points[0].x, 200), "array count 3 offset 100");
  CHECK(pg_gp_array(g3, NULL, 1, 100, 0) == 0, "count 1 makes nothing");

  /* merge by distance */
  bGPDstroke *mg = add_stroke(f3, 5, 0, 0, 50, 1, 0);
  select_points(g3, mg, 0x1F); select_points(g3, r, 0);
  merge_calls = 0;
  CHECK(pg_gp_merge_distance(g3, NULL, 2.5f, 0) == 1 && merge_calls >= 1 && mg->totpoints < 5, "merge by distance");

  /* caps */
  CHECK(pg_gp_caps_set(g3, NULL, PG_CAPS_TOGGLE_START) == 1 && mg->caps[0] == 1 && mg->caps[1] == 0, "toggle start cap to flat");
  pg_gp_caps_set(g3, NULL, PG_CAPS_DEFAULT);
  CHECK(mg->caps[0] == 0 && mg->caps[1] == 0, "default = round both");

  /* start set on a cyclic stroke */
  bGPDstroke *cy = add_stroke(f3, 4, 0, 0, 90, 10, 0);
  cy->flag |= GP_STROKE_CYCLIC;
  select_points(g3, mg, 0);
  select_points(g3, cy, 1u << 2);
  cy->flag |= GP_STROKE_CYCLIC;
  CHECK(pg_gp_start_set(g3, NULL) == 1 && NEAR(cy->points[0].x, 20) && NEAR(cy->points[1].x, 30) && NEAR(cy->points[2].x, 0),
        "start point rotated to the selected point, order kept");
  cy->flag &= ~GP_STROKE_CYCLIC;
  CHECK(pg_gp_start_set(g3, NULL) == 0, "open strokes are not rotated");

  /* separate to a new layer and move to layer */
  bGPdata *g4 = make_gpd(); bGPDlayer *src = add_layer(g4, 0); bGPDframe *sf = add_frame(src);
  bGPDstroke *s1 = add_stroke(sf, 2, 0, 0, 0, 10, 0);
  bGPDstroke *s2 = add_stroke(sf, 2, 0, 0, 20, 10, 0);
  select_points(g4, s1, 3);
  CHECK(pg_gp_separate_to_layer(g4, src) == 1 && stroke_count(sf) == 1 && sf->strokes.first == s2, "selected stroke left the source");
  bGPDlayer *sep = g4->layers.last;
  CHECK(sep != src && frame_stroke_count(sep) == 1 && sep->actframe->framenum == sf->framenum, "...into a new layer, same frame number");
  select_points(g4, s2, 3);
  CHECK(pg_gp_move_to_layer(g4, src, 1) == 1 && stroke_count(sf) == 0 && frame_stroke_count(sep) == 2, "move to layer index 1");
  CHECK(pg_gp_move_to_layer(g4, src, 7) == 0, "missing target layer");

  /* copy / paste */
  bGPdata *g5 = make_gpd(); bGPDlayer *l5 = add_layer(g5, 0); bGPDframe *f5 = add_frame(l5);
  bGPDstroke *c1 = add_stroke(f5, 3, 0, 0, 0, 10, 0);
  select_points(g5, c1, 7);
  pg_gp_copy(g5, NULL);
  CHECK(pg_gp_paste(g5, l5) == 1 && stroke_count(f5) == 2 && sel_mask(c1) == 0 && sel_mask(f5->strokes.last) == 7,
        "paste adds a selected copy and deselects the rest");
  select_points(g5, c1, 0); select_points(g5, f5->strokes.last, 0);
  pg_gp_copy(g5, NULL); /* nothing selected: clipboard kept */
  CHECK(pg_gp_paste(g5, l5) == 1 && stroke_count(f5) == 3, "empty copy keeps the previous clipboard");
  pg_gp_clipboard_free();
  CHECK(pg_gp_paste(g5, l5) == 0, "empty clipboard pastes nothing");

  const float args[3] = {3, 2, 0};
  CHECK(pg_gp_edit_dispatch(g5, l5, PG_EDIT4_CMD_DASH, args, 2) == 0, "dash needs 3 args");
  CHECK(pg_gp_edit_dispatch(g5, l5, 96, NULL, 0) == 0, "beyond the routed range");
}

/* edit6 stand-in (BKE_gpencil_stroke_uniform_subdivide is linked from Blender in the app). */
void BKE_gpencil_stroke_uniform_subdivide(bGPdata *gpd, bGPDstroke *gps, uint32_t target, bool select)
{
  (void)gpd; (void)select;
  /* stand-in: resample uniformly by index between the original points */
  int n = gps->totpoints;
  bGPDspoint *np = calloc(target, sizeof(bGPDspoint));
  for (unsigned int i = 0; i < target; i++) {
    float f = target > 1 ? (float)i * (n - 1) / (float)(target - 1) : 0;
    int a = (int)f; int b = a + 1 < n ? a + 1 : a; float t = f - a;
    np[i] = gps->points[a];
    np[i].x += (gps->points[b].x - gps->points[a].x) * t;
    np[i].y += (gps->points[b].y - gps->points[a].y) * t;
  }
  free(gps->points); gps->points = np; gps->totpoints = (int)target;
}

static void test_edit6(void)
{
  /* outline of a straight horizontal stroke, thickness 10 (radius 5), pressure 1 */
  bGPdata *gpd = make_gpd(); bGPDlayer *l = add_layer(gpd, 0); bGPDframe *f = add_frame(l);
  bGPDstroke *s = add_stroke(f, 3, 0, 0, 0, 10, 0);
  s->thickness = 10;
  for (int i = 0; i < 3; i++) s->points[i].pressure = 1.0f;
  select_points(gpd, s, 7);
  CHECK(pg_gp_outline(gpd, NULL, 2, 4) == 1, "outline");
  CHECK((s->flag & GP_STROKE_CYCLIC) && s->thickness == 2 && s->totpoints == 2 * 3 + 2 * 3, "closed perimeter with round caps");
  float miny = 1e9f, maxy = -1e9f, maxx = -1e9f;
  for (int i = 0; i < s->totpoints; i++) { miny = fminf(miny, s->points[i].y); maxy = fmaxf(maxy, s->points[i].y); maxx = fmaxf(maxx, s->points[i].x); }
  CHECK(NEAR(miny, -5) && NEAR(maxy, 5) && NEAR(maxx, 25), "perimeter at the stroke radius, cap reaches end + radius");
  CHECK(NEAR(s->points[0].y, 5) && NEAR(s->points[3 + 3 + 2].y, -5), "left side then right side");
  CHECK(pg_gp_outline(gpd, NULL, 2, 4) == 0, "outline of the (now closed, unselected) result does nothing");

  /* interpolation of unequal strokes */
  bGPDstroke *a = add_stroke(f, 2, 0, 0, 0, 10, 0);
  bGPDstroke *b = add_stroke(f, 3, 0, 0, 100, 10, 0);
  a->thickness = 4; b->thickness = 8;
  bGPDstroke *mid = pg_gp_interpolate_strokes(gpd, a, b, 0.5f);
  CHECK(mid != NULL && mid->totpoints == 3, "fewer points resampled to the larger count");
  CHECK(mid && NEAR(mid->points[2].x, 15) && NEAR(mid->points[2].y, 50) && mid->thickness == 6, "halfway positions and thickness");
  CHECK(a->totpoints == 2 && b->totpoints == 3, "inputs untouched");
  if (mid) { free(mid->points); free(mid); }
  bGPDstroke *rev = pg_gp_interpolate_strokes(gpd, b, a, 0.0f);
  CHECK(rev != NULL && rev->totpoints == 3 && NEAR(rev->points[2].x, 20) && NEAR(rev->points[2].y, 100),
        "from with more points: the 'to' copy is resampled, t=0 keeps 'from'");
  if (rev) { free(rev->points); free(rev); }

  /* onion ghosts */
  const int keys[5] = {1, 5, 10, 20, 30};
  int fr[8]; float al[8];
  int n = pg_onion_ghosts(PG_ONION_RELATIVE, keys, NULL, 5, 12, 1, 2, 0, 0.5f, fr, al, 8);
  CHECK(n == 3 && fr[0] == 5 && fr[1] == 20 && fr[2] == 30 && NEAR(al[0], 0.5f), "relative: 1 key before, 2 after the key on screen (10)");
  n = pg_onion_ghosts(PG_ONION_ABSOLUTE, keys, NULL, 5, 12, 7, 10, 0, 0.5f, fr, al, 8);
  CHECK(n == 2 && fr[0] == 5 && fr[1] == 20, "absolute: keys within 7 before / 10 after frame 12");
  n = pg_onion_ghosts(PG_ONION_RELATIVE, keys, NULL, 5, 10, 2, 0, 1, 1.0f, fr, al, 8);
  CHECK(n == 2 && NEAR(al[0], 0.5f) && NEAR(al[1], 1.0f), "fade: alpha factor/distance");
  const unsigned char sel[5] = {1, 0, 0, 1, 0};
  n = pg_onion_ghosts(PG_ONION_SELECTED, keys, sel, 5, 10, 0, 0, 0, 0.5f, fr, al, 8);
  CHECK(n == 2 && fr[0] == 1 && fr[1] == 20, "selected mode");

  /* fill extend lines */
  bGPDstroke *e = add_stroke(f, 3, 0, 0, 0, 10, 0);
  float seg[8];
  CHECK(pg_fill_extend_segments(e, 0.5f, seg) == 1 && NEAR(seg[2], -10) && NEAR(seg[6], 30), "ends extended outward by fac x length (20 x 0.5)");
  e->flag |= GP_STROKE_CYCLIC;
  CHECK(pg_fill_extend_segments(e, 0.5f, seg) == 0, "closed strokes have no ends");

  const float args[2] = {2, 4};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT6_CMD_OUTLINE, args, 1) == 0, "outline needs 2 args");
  /* onion style: Blender onion_mode + ghost colours with their switches */
  const float style[9] = {GP_ONION_MODE_SELECTED, 1, 0, 1, 0, 0, 0, 0, 1};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT6_CMD_ONION_STYLE, style, 9) == 1, "onion style");
  CHECK(gpd->onion_mode == GP_ONION_MODE_SELECTED && (gpd->onion_flag & GP_ONION_GHOST_PREVCOL) &&
        !(gpd->onion_flag & GP_ONION_GHOST_NEXTCOL) && NEAR(gpd->gcolor_prev[0], 1) && NEAR(gpd->gcolor_next[2], 1),
        "onion mode, colour switches and colours stored");
  const float bad[9] = {7, 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT6_CMD_ONION_STYLE, bad, 9) == 0, "unknown onion mode rejected");

  /* material texture settings */
  Material ma = {0}; MaterialGPencilStyle st = {0}; ma.gp_style = &st; Material *mats[1] = {&ma};
  gpd->mat = mats; gpd->totcol = 1;
  const float tex[10] = {0, 1, 1, 0.25f, 2, 3, 0.1f, 0.2f, 0.5f, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT6_CMD_MATERIAL_TEXTURE, tex, 10) == 1, "fill texture");
  CHECK(st.fill_style == GP_MATERIAL_FILL_STYLE_TEXTURE && NEAR(st.mix_factor, 0.25f) && NEAR(st.texture_scale[1], 3) &&
        NEAR(st.texture_offset[0], 0.1f) && NEAR(st.texture_angle, 0.5f) && st.stroke_style == 0, "fill texture fields");
  const float stex[10] = {0, 0, 1, 0.5f, 1, 1, 0, 0, 0, 50};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT6_CMD_MATERIAL_TEXTURE, stex, 10) == 1 &&
        st.stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE && NEAR(st.texture_pixsize, 50) && NEAR(st.mix_stroke_factor, 0.5f),
        "stroke texture fields");
  const float badslot[10] = {3, 0, 1, 0, 1, 1, 0, 0, 0, 0};
  CHECK(pg_gp_edit_dispatch(gpd, l, PG_EDIT6_CMD_MATERIAL_TEXTURE, badslot, 10) == 0, "missing slot rejected");
  gpd->mat = NULL; gpd->totcol = 0;
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
  test_length_modifier();
  test_tint_modifier();
  test_hsv_conversion();
  test_color_modifier();
  test_stroke_operators();
  test_stroke_operator_edges();
  test_structure_operators();
  test_dissolve_keeps_weights_aligned();
  test_vertex_paint();
  test_easing();
  test_mirror_copy();
  test_weight_paint();
  test_point_weight();
  test_thickness_vgroup();
  test_edit2_operators();
  test_edit3();
  test_edit4();
  test_edit6();
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
