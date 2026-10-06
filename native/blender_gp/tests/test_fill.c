/* Host tests for project_grease_blender_fill.c (Blender 3.6.23 gpencil_fill.c port), linked with the
 * real pinned BKE / BLI closure: closed square, gapped square closed by the leak size and by
 * Extend Lines (with and without the stroke collision check), dilate / contract, fill_factor. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BKE_gpencil_legacy.h"
#include "BLI_listbase.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"

#include "project_grease_blender_fill.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

enum { W = 64, H = 64 };

static void red(float *img, int x, int y)
{
  float *p = &img[(y * W + x) * 4];
  p[0] = 1.0f; p[1] = 0.0f; p[2] = 0.0f; p[3] = 1.0f;
}

/* Square outline from (a,a) to (b,b), 1 px wide, with an optional gap of `gap` px in the top edge. */
static float *square(int a, int b, int gap)
{
  float *img = calloc((size_t)W * H * 4, sizeof(float));
  const int mid = (a + b) / 2;
  for (int i = a; i <= b; i++) {
    red(img, i, a);
    if (gap <= 0 || i < mid || i >= mid + gap) red(img, i, b);
    red(img, a, i);
    red(img, b, i);
  }
  return img;
}

static void bbox(const float *xy, int n, float r[4])
{
  r[0] = r[1] = 1e9f; r[2] = r[3] = -1e9f;
  for (int i = 0; i < n; i++) {
    r[0] = fminf(r[0], xy[i * 2]); r[1] = fminf(r[1], xy[i * 2 + 1]);
    r[2] = fmaxf(r[2], xy[i * 2]); r[3] = fmaxf(r[3], xy[i * 2 + 1]);
  }
}

static int run(float *img, int leak, int dilate, float **xy, int *bc)
{
  return pg_fill_raster(img, W, H, 32, 32, 4.0f * (float)M_SQRT2, leak, dilate, xy, bc);
}

static void test_closed_square(void)
{
  float *img = square(12, 51, 0);
  float *xy = NULL; int bc = 0;
  const int n = run(img, 3, 0, &xy, &bc);
  CHECK(n > 100 && !bc);
  float b[4]; bbox(xy, n, b);
  /* No dilate: the outline runs over the first filled pixels inside the 1 px border. */
  CHECK(b[0] == 13.5f && b[1] == 13.5f && b[2] == 50.5f && b[3] == 50.5f);
  free(xy); free(img);
}

static void test_dilate_contract(void)
{
  float b[4];
  float *img = square(12, 51, 0); float *xy = NULL; int bc;
  int n = run(img, 3, 1, &xy, &bc);
  bbox(xy, n, b);
  CHECK(n > 0 && b[0] == 12.5f && b[2] == 51.5f); /* dilate 1 grows over the boundary line */
  free(xy); free(img);
  img = square(12, 51, 0); xy = NULL;
  n = run(img, 3, -2, &xy, &bc);
  bbox(xy, n, b);
  CHECK(n > 0 && b[0] == 15.5f && b[2] == 48.5f); /* contract 2 */
  free(xy); free(img);
}

static void test_gap_leak(void)
{
  /* 2 px gap: the default leak (3) treats it as closed; leak 1 floods out to the border. */
  float *img = square(12, 51, 2); float *xy = NULL; int bc = 0;
  int n = run(img, 3, 0, &xy, &bc);
  CHECK(n > 100 && !bc);
  free(xy); free(img);
  img = square(12, 51, 2); xy = NULL;
  n = run(img, 1, 0, &xy, &bc);
  CHECK(n == 0 && bc && xy == NULL);
  free(img);
  /* A 6 px gap leaks at the default leak and closes with fill_factor 2 (leak ceil(3 * 2) = 6...7). */
  img = square(12, 51, 6); xy = NULL;
  n = run(img, pg_fill_leak_from_factor(1.0f), 0, &xy, &bc);
  CHECK(n == 0 && bc);
  free(img);
  img = square(12, 51, 6); xy = NULL;
  n = run(img, pg_fill_leak_from_factor(2.5f), 0, &xy, &bc);
  CHECK(n > 100 && !bc);
  free(xy); free(img);
}

static void test_factor(void)
{
  CHECK(pg_fill_leak_from_factor(1.0f) == 3);
  CHECK(pg_fill_leak_from_factor(0.5f) == 2);
  CHECK(pg_fill_leak_from_factor(100.0f) == 24);
  CHECK(pg_fill_render_size(1000, 0.5f) == 500);
  CHECK(pg_fill_render_size(200, 0.1f) == 128);
  /* Downsampling keeps a 1 px line. */
  float *img = square(12, 51, 0);
  float *half = calloc((size_t)32 * 32 * 4, sizeof(float));
  pg_fill_resample_mask(img, W, H, half, 32, 32);
  CHECK(half[((6 * 32) + 16) * 4 + 0] == 1.0f && half[((16 * 32) + 16) * 4 + 3] == 0.0f);
  free(half); free(img);
}

enum { OX = 590, OY = 300 };

static bGPDstroke *line(bGPDframe *f, float x0, float y0, float x1, float y1)
{
  bGPDstroke *s = BKE_gpencil_stroke_add(f, 0, 2, 4, false);
  /* Placed near the canvas centre (the world origin): Blender only tests extension / stroke
   * collisions when the bounding boxes are within 1.1 BU, and a fresh extension's box is at 0. */
  s->points[0].x = x0 + OX; s->points[0].y = y0 + OY;
  s->points[1].x = x1 + OX; s->points[1].y = y1 + OY;
  for (int i = 0; i < 2; i++) { s->points[i].pressure = 1.0f; s->points[i].strength = 1.0f; }
  return s;
}

static void test_extend_lines(void)
{
  bGPdata *gpd = MEM_callocN(sizeof(bGPdata), "fill");
  MaterialGPencilStyle style = {0};
  Material mat = {{0}};
  mat.gp_style = &style;
  Material *mats[1] = {&mat};
  gpd->mat = mats;
  gpd->totcol = 1;
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "L", true, false);
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, 1);
  /* An open "U" (canvas units) and a horizontal stroke above it, 20 units over its gap. */
  line(gpf, 0, 0, 0, 100);
  line(gpf, 0, 0, 100, 0);
  line(gpf, 100, 0, 100, 100);
  line(gpf, -50, 120, 150, 120);
  const int strokes_before = BLI_listbase_count(&gpf->strokes);
  float seg[64];
  /* fac 0.3 -> 0.03 BU -> 3 canvas units: too short to reach. */
  int n = pg_fill_extend_lines(gpd, 1, 0.3f, 0, 1.0f, 0, 0, 640, 360, seg, 16);
  CHECK(n == 8);
  int reach = 0;
  for (int i = 0; i < n; i++) if (seg[i * 4 + 1] < OY + 110.0f && seg[i * 4 + 3] >= OY + 119.9f) reach++;
  CHECK(reach == 0);
  /* fac 3 -> 30 units: the two upward ends of the U pass the top stroke (no collide: full length). */
  n = pg_fill_extend_lines(gpd, 1, 3.0f, 0, 1.0f, 0, 0, 640, 360, seg, 16);
  reach = 0;
  for (int i = 0; i < n; i++) if (fabsf(seg[i * 4 + 3] - (OY + 130.0f)) < 1e-3f) reach++;
  CHECK(n == 8 && reach == 2);
  /* With the collision check only colliding extensions are drawn, cut at the top stroke. */
  n = pg_fill_extend_lines(gpd, 1, 3.0f, 1, 1.0f, 0, 0, 640, 360, seg, 16);
  CHECK(n == 2);
  for (int i = 0; i < n; i++) CHECK(fabsf(seg[i * 4 + 3] - (OY + 120.0f)) < 1e-2f);
  CHECK(BLI_listbase_count(&gpf->strokes) == strokes_before);
  BKE_gpencil_free_layers(&gpd->layers);
  gpd->mat = NULL;
  MEM_freeN(gpd);
}

int main(void)
{
  test_closed_square();
  test_dilate_contract();
  test_gap_leak();
  test_factor();
  test_extend_lines();
  if (failures) {
    fprintf(stderr, "fill tests: %d failure(s)\n", failures);
    return 1;
  }
  printf("fill tests passed\n");
  return 0;
}
