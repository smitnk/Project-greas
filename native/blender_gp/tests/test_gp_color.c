/* Host test for project_grease_gp_color.h (presenter vertex-color mixing). */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "project_grease_gp_color.h"

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static bool near3(const float *a, const float *b)
{
  return fabsf(a[0] - b[0]) < 1e-6f && fabsf(a[1] - b[1]) < 1e-6f && fabsf(a[2] - b[2]) < 1e-6f;
}

typedef struct FakePoint {
  float x, y, z;
  float vert_color[4];
  int flag;
} FakePoint;

static void test_mix(void)
{
  const float base[3] = {1.0f, 0.0f, 0.0f}; /* red material */
  float out[3];

  /* alpha 0 = no vertex color: the material color, whatever the vertex rgb says */
  const float none[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  pg_gp_mix_vertex_color(base, none, 1.0f, out);
  CHECK(near3(out, base));

  /* alpha 1 replaces the material color */
  const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  const float blue_rgb[3] = {0.0f, 0.0f, 1.0f};
  pg_gp_mix_vertex_color(base, blue, 1.0f, out);
  CHECK(near3(out, blue_rgb));

  /* half way is a mix (purple), NOT the product (which would be black) */
  const float half[4] = {0.0f, 0.0f, 1.0f, 0.5f};
  const float purple[3] = {0.5f, 0.0f, 0.5f};
  pg_gp_mix_vertex_color(base, half, 1.0f, out);
  CHECK(near3(out, purple));

  /* the vertex-color opacity scales the factor */
  pg_gp_mix_vertex_color(base, blue, 0.5f, out);
  CHECK(near3(out, purple));
  pg_gp_mix_vertex_color(base, blue, 0.0f, out);
  CHECK(near3(out, base));

  /* the old multiplicative result for a tint is NOT produced: red * blue would be black */
  const float black[3] = {0.0f, 0.0f, 0.0f};
  pg_gp_mix_vertex_color(base, blue, 1.0f, out);
  CHECK(!near3(out, black));

  /* out-of-range and NaN input is clamped, never propagated */
  const float wild[4] = {5.0f, -2.0f, NAN, 9.0f};
  const float expect_wild[3] = {1.0f, 0.0f, 0.0f};
  pg_gp_mix_vertex_color(base, wild, 1.0f, out);
  CHECK(near3(out, expect_wild));
}

static void test_mean_mix(void)
{
  const float base[3] = {1.0f, 0.0f, 0.0f};
  FakePoint pts[2];
  memset(pts, 0, sizeof(pts));
  float out[3];

  /* all points untinted: the material color */
  pg_gp_stroke_mean_mix(base, pts[0].vert_color, sizeof(FakePoint), 2, 1.0f, out);
  CHECK(near3(out, base));

  /* one point fully blue, one untinted: the mean of the per-point mixes */
  const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  memcpy(pts[1].vert_color, blue, sizeof(blue));
  const float mean[3] = {0.5f, 0.0f, 0.5f};
  pg_gp_stroke_mean_mix(base, pts[0].vert_color, sizeof(FakePoint), 2, 1.0f, out);
  CHECK(near3(out, mean));

  /* that is not mix(base, mean(vert rgb), mean(alpha)): averaging first gives a different color */
  float avg_rgba[4] = {0.0f, 0.0f, 0.5f, 0.5f};
  float averaged_first[3];
  pg_gp_mix_vertex_color(base, avg_rgba, 1.0f, averaged_first);
  CHECK(!near3(averaged_first, mean));

  /* no points / no data: the base color */
  pg_gp_stroke_mean_mix(base, NULL, sizeof(FakePoint), 2, 1.0f, out);
  CHECK(near3(out, base));
  pg_gp_stroke_mean_mix(base, pts[0].vert_color, sizeof(FakePoint), 0, 1.0f, out);
  CHECK(near3(out, base));
}

/* Fill uses the same mix with the stroke's vert_color_fill against the material fill color. */
static void test_fill(void)
{
  const float fill_base[3] = {0.2f, 0.4f, 0.6f};
  const float vcf[4] = {1.0f, 1.0f, 0.0f, 0.25f};
  float out[3];
  pg_gp_mix_vertex_color(fill_base, vcf, 1.0f, out);
  const float expect[3] = {0.2f * 0.75f + 0.25f, 0.4f * 0.75f + 0.25f, 0.6f * 0.75f};
  CHECK(near3(out, expect));
}

int main(void)
{
  test_mix();
  test_mean_mix();
  test_fill();
  if (failures) {
    printf("%d FAILED\n", failures);
    return 1;
  }
  printf("ALL PASSED\n");
  return 0;
}
