/* Test-only stand-in for the Blender macros the selection code uses. */
#pragma once
#include <stdbool.h>
#include <assert.h>
#define PG_ELEM_PICK(_1, _2, _3, _4, NAME, ...) NAME
#define PG_ELEM_2(v, a) ((v) == (a))
#define PG_ELEM_3(v, a, b) (((v) == (a)) || ((v) == (b)))
#define PG_ELEM_4(v, a, b, c) (((v) == (a)) || ((v) == (b)) || ((v) == (c)))
#define ELEM(...) PG_ELEM_PICK(__VA_ARGS__, PG_ELEM_4, PG_ELEM_3, PG_ELEM_2)(__VA_ARGS__)
#define SET_FLAG_FROM_TEST(value, test, flag) \
  { \
    if (test) { \
      (value) |= (flag); \
    } \
    else { \
      (value) &= ~(flag); \
    } \
  } \
  ((void)0)
#define BLI_assert_msg(a, msg) assert((a) && (msg))
#include <stdint.h>
#define POINTER_FROM_INT(i) ((void *)(intptr_t)(i))
