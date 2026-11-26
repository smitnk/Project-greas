/* SPDX-License-Identifier: GPL-2.0-or-later
 * Numeric input of a modal operator: Blender 3.6.23 editors/util/numinput.c (NumInput of
 * ED_numinput.h), in its default "fake edit" mode (USER_FLAG_NUMINPUT_ADVANCED off): digits and
 * '.' are typed, '-' negates, '/' inverts, '=' or '*' switches to full editing, where the string
 * is an expression. Python's BPY_run_string_as_number is replaced by pg_numinput_eval (numbers,
 * + - * /, parentheses, unary signs). Units: a value is typed in display units, which are the
 * internal value times unit_scale for B_UNIT_LENGTH (BKE_scene_unit_scale) and degrees for
 * B_UNIT_ROTATION (BKE_unit_apply_preferred_unit, unit_use_radians off). */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { PG_NUM_MAX_ELEMENTS = 3, PG_NUM_STR_REP_LEN = 64 };
/* NumInput.flag (public) */
enum { PG_NUM_AFFECT_ALL = (1 << 0) };
/* NumInput.val_flag[] (public) */
enum { PG_NUM_NULL_ONE = (1 << 0), PG_NUM_NO_NEGATIVE = (1 << 1), PG_NUM_NO_ZERO = (1 << 2), PG_NUM_NO_FRACTION = (1 << 3) };
/* B_UNIT_* (BKE_unit.h) */
enum { PG_UNIT_NONE = 0, PG_UNIT_LENGTH = 1, PG_UNIT_ROTATION = 5 };
/* keys besides ASCII characters (EVT_BACKSPACEKEY, EVT_DELKEY, EVT_TABKEY, Ctrl+Tab) */
enum { PG_NUM_KEY_BACKSPACE = 8, PG_NUM_KEY_TAB = 9, PG_NUM_KEY_DEL = 127, PG_NUM_KEY_CTRL_TAB = 0x109 };

typedef struct PGNumInput {
  short idx_max;
  int unit_type[PG_NUM_MAX_ELEMENTS];
  float unit_scale; /* display units per internal unit for PG_UNIT_LENGTH (canvas px per Blender unit) */
  bool unit_use_radians;
  short flag;
  short val_flag[PG_NUM_MAX_ELEMENTS];
  float val[PG_NUM_MAX_ELEMENTS];
  float val_org[PG_NUM_MAX_ELEMENTS];
  float val_inc[PG_NUM_MAX_ELEMENTS];
  short idx;
  char str[PG_NUM_STR_REP_LEN];
  int str_cur;
} PGNumInput;

void pg_numinput_init(PGNumInput *n);                       /* initNumInput */
bool pg_numinput_has(const PGNumInput *n);                  /* hasNumInput */
bool pg_numinput_apply(PGNumInput *n, float *vec);          /* applyNumInput */
bool pg_numinput_handle(PGNumInput *n, int key);            /* handleNumInput, one key */
/* outputNumInput: one line per value 0..idx_max, joined with ", ", at most len bytes. */
void pg_numinput_output(const PGNumInput *n, char *str, int len);
/* The expression evaluator in place of Python. 1 = ok. */
int pg_numinput_eval(const char *str, double *r_value);

#ifdef __cplusplus
}
#endif
