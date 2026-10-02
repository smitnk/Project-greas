/* Host test for the Blender 3.6.23 random-number closure used by the Offset and Noise modifiers:
 * rand.cc (BLI_rng_*, BLI_halton_*), noise.c (the hash table rand.cc links against) and the
 * header-only BLI_hash.h. The real pinned sources are compiled in (tools/run_native_rng_tests.sh).
 *
 * The expected values are golden numbers produced by an independent Python reference written from
 * the pinned source (tests/gen_rng_golden.py): the 48-bit drand48-style generator of
 * BLI_rand.hh, the Jenkins lookup3 hash of BLI_hash.h and halton_ex() of rand.cc. Seed 0 also
 * reproduces the classic srand48(0)/lrand48() sequence (366850414, ...). */
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "BLI_hash.h"
#include "BLI_rand.h"
}

/* guardedalloc stand-ins: rand.cc only needs plain allocation. In Blender 3.6 the MEM_* entry
 * points are function-pointer variables (MEM_guardedalloc.h), so they are defined as such. */
static void *plain_malloc(size_t len, const char *) { return malloc(len); }
static void *plain_malloc_aligned(size_t len, size_t, const char *) { return malloc(len); }
static void plain_free(void *p) { free(p); }
extern "C" {
void *(*MEM_mallocN)(size_t, const char *) = plain_malloc;
void *(*MEM_mallocN_aligned)(size_t, size_t, const char *) = plain_malloc_aligned;
void (*MEM_freeN)(void *) = plain_free;
}

static int failures = 0;
#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

static void test_rng_ints()
{
  struct Case {
    unsigned seed;
    int expect[6];
  } cases[] = {
      {0, {366850414, 1610402240, 206956554, 1869309841, 1239749840, 1687491058}},
      {1, {89400484, 976015093, 1792756325, 721524505, 1214379247, 3794415}},
      {12345, {483889296, 1973930609, 444188209, 1556452597, 1572385691, 1946656043}},
  };
  for (const Case &c : cases) {
    RNG *rng = BLI_rng_new(c.seed);
    for (int i = 0; i < 6; i++) {
      CHECK(BLI_rng_get_int(rng) == c.expect[i]);
    }
    BLI_rng_free(rng);
  }
}

static void test_rng_floats_and_copy()
{
  RNG *rng = BLI_rng_new(12345);
  const float expect[4] = {0.2253285050392151f, 0.919183075428009f, 0.20684126019477844f,
                           0.7247797250747681f};
  RNG *copy = BLI_rng_copy(rng); /* a copy continues the same sequence */
  for (int i = 0; i < 4; i++) {
    const float v = BLI_rng_get_float(rng);
    CHECK(v == expect[i]);
    CHECK(BLI_rng_get_float(copy) == expect[i]);
  }
  /* re-seeding restarts it; skipping equals drawing */
  BLI_rng_seed(rng, 12345);
  BLI_rng_skip(rng, 2);
  CHECK(BLI_rng_get_float(rng) == expect[2]);
  BLI_rng_free(rng);
  BLI_rng_free(copy);
}

static void test_hash()
{
  CHECK(BLI_hash_int_2d(0, 0) == 3695015161u);
  CHECK(BLI_hash_int_2d(1, 0) == 1267069554u);
  CHECK(BLI_hash_int_2d(7, 3) == 436444241u);
  CHECK(BLI_hash_int_2d(12345, 99) == 1077337676u);
  CHECK(BLI_hash_int_2d(4294967295u, 1) == 2128903035u);

  CHECK(BLI_hash_int_01(0) == 0.8603127598762512f);
  CHECK(BLI_hash_int_01(1) == 0.29501262307167053f);
  CHECK(BLI_hash_int_01(2) == 0.2331811934709549f);
  CHECK(BLI_hash_int_01(12345) == 0.700317919254303f);

  CHECK(BLI_hash_string("") == 0u);
  CHECK(BLI_hash_string("Noise") == 151955142u);
  CHECK(BLI_hash_string("Offset") == 1379697623u);
  CHECK(BLI_hash_string("GPencil") == 3225499486u);
}

static void test_halton()
{
  const unsigned primes[3] = {2, 3, 7};
  struct Case {
    int n;
    double expect[3];
  } cases[] = {
      {1, {0.5, 0.3333333333333333, 0.14285714285714285}},
      {2, {0.25, 0.6666666666666666, 0.2857142857142857}},
      {5, {0.625, 0.7777777777777777, 0.7142857142857142}},
      {9, {0.5625, 0.03703703703703676, 0.3061224489795916}},
  };
  for (const Case &c : cases) {
    double offset[3] = {0.0, 0.0, 0.0}, r[3];
    BLI_halton_3d(primes, offset, c.n, r);
    for (int i = 0; i < 3; i++) {
      CHECK(r[i] == c.expect[i]);
    }
  }
  /* n = 0 yields zeros, as Offset's first stroke relies on */
  double offset[3] = {0.0, 0.0, 0.0}, r[3] = {9.0, 9.0, 9.0};
  BLI_halton_3d(primes, offset, 0, r);
  CHECK(r[0] == 0.0 && r[1] == 0.0 && r[2] == 0.0);
}

int main()
{
  test_rng_ints();
  test_rng_floats_and_copy();
  test_hash();
  test_halton();
  if (failures) {
    printf("%d FAILED\n", failures);
    return 1;
  }
  printf("ALL PASSED\n");
  return 0;
}
