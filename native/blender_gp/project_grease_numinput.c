/* SPDX-License-Identifier: GPL-2.0-or-later
 * See project_grease_numinput.h. Ported from Blender 3.6.23 source/blender/editors/util/numinput.c
 * (initNumInput, outputNumInput, hasNumInput, applyNumInput, value_to_editstr,
 * editstr_insert_at_cursor, user_string_to_number, editstr_is_simple_numinput, handleNumInput),
 * USE_FAKE_EDIT defined as there. Not ported: Ctrl+C / Ctrl+V, Ctrl+arrows / delimiter jumps,
 * wheel increments (NUM_MODAL_INCREMENT_*), unit names typed in the string. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "project_grease_numinput.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { NUM_EDIT_FULL = (1 << 9), NUM_FAKE_EDITED = (1 << 10) };                                       /* flag */
enum { NUM_EDITED = (1 << 9), NUM_INVALID = (1 << 10), NUM_NEGATE = (1 << 11), NUM_INVERSE = (1 << 12) }; /* val_flag */

void pg_numinput_init(PGNumInput *n)
{
  n->idx_max = 0;
  for (int i = 0; i < PG_NUM_MAX_ELEMENTS; i++) {
    n->unit_type[i] = PG_UNIT_NONE;
    n->val_flag[i] = 0;
    n->val[i] = n->val_org[i] = 0.0f;
    n->val_inc[i] = 1.0f;
  }
  n->unit_scale = 1.0f;
  n->unit_use_radians = false;
  n->flag = 0;
  n->idx = 0;
  n->str[0] = '\0';
  n->str_cur = 0;
}

/* BKE_scene_unit_scale / the degrees of B_UNIT_ROTATION: internal -> display */
static double display_of(const PGNumInput *n, int idx, double v)
{
  if (n->unit_type[idx] == PG_UNIT_ROTATION && !n->unit_use_radians) return v * 180.0 / M_PI;
  if (n->unit_type[idx] == PG_UNIT_LENGTH) return v * n->unit_scale;
  return v;
}

static const char *unit_suffix(const PGNumInput *n, int idx)
{
  return n->unit_type[idx] == PG_UNIT_LENGTH ? " px" :
         n->unit_type[idx] == PG_UNIT_ROTATION ? (n->unit_use_radians ? "r" : "\xc2\xb0") : "";
}

void pg_numinput_output(const PGNumInput *n, char *out, int len)
{
  if (len <= 0) return;
  out[0] = '\0';
  int used = 0;
  for (int j = 0; j <= n->idx_max && used < len; j++) {
    const int i = (n->flag & PG_NUM_AFFECT_ALL && n->idx != j && !(n->val_flag[j] & NUM_EDITED)) ? 0 : j;
    char s[PG_NUM_STR_REP_LEN * 2];
    /* shortcut: %g with 4 digits stands in for UI_calc_float_precision + BKE_unit_value_as_string_adaptive */
    if (n->val_flag[i] & NUM_EDITED) {
      if (i == n->idx) {
        const char *head = "", *tail = "";
        if (n->val_flag[i] & NUM_NEGATE) { head = (n->val_flag[i] & NUM_INVERSE) ? "-1/(" : "-("; tail = ")"; }
        else if (n->val_flag[i] & NUM_INVERSE) { head = "1/("; tail = ")"; }
        char val[32];
        if (n->val_flag[i] & NUM_INVALID) snprintf(val, sizeof(val), "Invalid");
        else snprintf(val, sizeof(val), "%.4g%s", display_of(n, i, n->val[i]), unit_suffix(n, i));
        snprintf(s, sizeof(s), "[%s%.*s|%s%s] = %s", head, n->str_cur, n->str, &n->str[n->str_cur], tail, val);
      }
      else {
        snprintf(s, sizeof(s), "%.4g%s", display_of(n, i, n->val[i]), unit_suffix(n, i));
      }
    }
    else {
      const char *cur = (i == n->idx) ? "|" : "";
      snprintf(s, sizeof(s), "%sNONE%s", cur, cur);
    }
    used += snprintf(out + used, (size_t)(len - used), "%s%s", j ? ", " : "", s);
  }
}

bool pg_numinput_has(const PGNumInput *n)
{
  if (n->flag & NUM_FAKE_EDITED) return true;
  for (int i = 0; i <= n->idx_max; i++) if (n->val_flag[i] & NUM_EDITED) return true;
  return false;
}

bool pg_numinput_apply(PGNumInput *n, float *vec)
{
  if (pg_numinput_has(n)) {
    for (int j = 0; j <= n->idx_max; j++) {
      float val;
      if (n->flag & NUM_FAKE_EDITED) {
        val = n->val[j];
      }
      else {
        const int i = (n->flag & PG_NUM_AFFECT_ALL && n->idx != j && !(n->val_flag[j] & NUM_EDITED)) ? 0 : j;
        val = (!(n->val_flag[i] & NUM_EDITED) && n->val_flag[i] & PG_NUM_NULL_ONE) ? 1.0f : n->val[i];
        if (n->val_flag[i] & PG_NUM_NO_NEGATIVE && val < 0.0f) val = 0.0f;
        if (n->val_flag[i] & PG_NUM_NO_FRACTION && val != floorf(val)) {
          val = floorf(val + 0.5f);
          if (n->val_flag[i] & PG_NUM_NO_ZERO && val == 0.0f) val = 1.0f;
        }
        else if (n->val_flag[i] & PG_NUM_NO_ZERO && val == 0.0f) {
          val = 0.0001f;
        }
      }
      vec[j] = val;
    }
    n->flag &= ~NUM_FAKE_EDITED;
    return true;
  }
  for (int j = 0; j <= n->idx_max; j++) n->val[j] = n->val_org[j] = vec[j];
  return false;
}

static void value_to_editstr(PGNumInput *n, int idx)
{
  n->str_cur = snprintf(n->str, PG_NUM_STR_REP_LEN, "%.6g", display_of(n, idx, n->val[idx]));
}

static bool editstr_insert_at_cursor(PGNumInput *n, const char *buf, const int buf_len)
{
  const int cur = n->str_cur;
  const int len = (int)strlen(&n->str[cur]) + 1;
  const int n_cur = cur + buf_len;
  if (n_cur + len >= PG_NUM_STR_REP_LEN) return false;
  memmove(&n->str[n_cur], &n->str[cur], (size_t)len);
  memcpy(&n->str[cur], buf, (size_t)buf_len);
  n->str_cur = n_cur;
  return true;
}

/* ---- the expression evaluator (in place of BPY_run_string_as_number) ---- */
typedef struct { const char *p; int ok; } Ev;
static void ev_ws(Ev *e) { while (*e->p == ' ') e->p++; }
static double ev_expr(Ev *e);
static double ev_factor(Ev *e)
{
  ev_ws(e);
  if (*e->p == '-' || *e->p == '+') { const char c = *e->p++; const double v = ev_factor(e); return c == '-' ? -v : v; }
  if (*e->p == '(') {
    e->p++;
    const double v = ev_expr(e);
    ev_ws(e);
    if (*e->p++ != ')') e->ok = 0;
    return v;
  }
  if (!((*e->p >= '0' && *e->p <= '9') || *e->p == '.')) { e->ok = 0; return 0; }
  char *end = NULL;
  const double v = strtod(e->p, &end);
  if (end == e->p) e->ok = 0;
  e->p = end;
  return v;
}
static double ev_term(Ev *e)
{
  double v = ev_factor(e);
  for (ev_ws(e); e->ok && (*e->p == '*' || *e->p == '/'); ev_ws(e)) {
    const char c = *e->p++;
    const double w = ev_factor(e);
    v = c == '*' ? v * w : v / w;
  }
  return v;
}
static double ev_expr(Ev *e)
{
  double v = ev_term(e);
  for (ev_ws(e); e->ok && (*e->p == '+' || *e->p == '-'); ev_ws(e)) {
    const char c = *e->p++;
    const double w = ev_term(e);
    v = c == '+' ? v + w : v - w;
  }
  return v;
}
int pg_numinput_eval(const char *str, double *r_value)
{
  Ev e = {str, 1};
  const double v = ev_expr(&e);
  ev_ws(&e);
  if (!e.ok || *e.p != '\0') return 0;
  *r_value = v;
  return 1;
}

/* user_string_to_number (WITH_PYTHON path, no unit in the string) */
static bool user_string_to_number(const PGNumInput *n, int idx, const char *str, double *r_value)
{
  const int success = pg_numinput_eval(str, r_value);
  if (n->unit_type[idx] == PG_UNIT_ROTATION && !n->unit_use_radians) *r_value *= M_PI / 180.0; /* BKE_unit_apply_preferred_unit */
  if (n->unit_type[idx] == PG_UNIT_LENGTH && n->unit_scale != 0.0f) *r_value /= n->unit_scale;
  return success;
}

static bool editstr_is_simple_numinput(const char ascii) { return (ascii >= '0' && ascii <= '9') || ascii == '.'; }

bool pg_numinput_handle(PGNumInput *n, int key)
{
  const char event_ascii = (key > 32 && key < 127) ? (char)key : '\0';
  char ascii[2] = {'\0', '\0'};
  const char *utf8_buf = NULL;
  bool updated = false;
  short idx = n->idx;
  const short idx_max = n->idx_max;
  int dir_prev = 0;

  /* XXX Hack around keyboards without direct access to '=' nor '*'... */
  if ((event_ascii == '=' || event_ascii == '*') && !(n->flag & NUM_EDIT_FULL)) {
    n->flag |= NUM_EDIT_FULL;
    n->val_flag[idx] |= NUM_EDITED;
    return true;
  }

  switch (key) {
    case PG_NUM_KEY_BACKSPACE:
      if (!(n->val_flag[idx] & NUM_EDITED)) {
        for (int i = 0; i < PG_NUM_MAX_ELEMENTS; i++) { n->val[i] = n->val_org[i]; n->val_flag[i] &= ~NUM_EDITED; }
        n->flag |= NUM_FAKE_EDITED;
        updated = true;
        break;
      }
      else if (!n->str[0]) {
        n->val[idx] = n->val_org[idx];
        n->val_flag[idx] &= ~NUM_EDITED;
        n->str[0] = '\0';
        n->str_cur = 0;
        updated = true;
        break;
      }
      dir_prev = 1;
      /* fall-through: common behavior with DELKEY */
    case PG_NUM_KEY_DEL:
      if ((n->val_flag[idx] & NUM_EDITED) && n->str[0]) {
        /* one ASCII character before / after the cursor (BLI_str_cursor_step_utf8, STRCUR_JUMP_NONE) */
        int cur = n->str_cur, t_cur = dir_prev ? cur - 1 : cur + 1;
        if (t_cur >= 0 && t_cur <= (int)strlen(n->str)) {
          if (t_cur < cur) { const int s = t_cur; t_cur = cur; cur = s; n->str_cur = cur; }
          memmove(&n->str[cur], &n->str[t_cur], strlen(&n->str[t_cur]) + 1);
          updated = true;
        }
        if (!n->str[0]) n->val[idx] = n->val_org[idx];
      }
      else {
        return false;
      }
      break;
    case PG_NUM_KEY_TAB:
    case PG_NUM_KEY_CTRL_TAB:
      n->val_flag[idx] &= ~(NUM_NEGATE | NUM_INVERSE);
      idx = (short)((idx + idx_max + (key == PG_NUM_KEY_CTRL_TAB ? 0 : 2)) % (idx_max + 1));
      n->idx = idx;
      if (n->val_flag[idx] & NUM_EDITED) value_to_editstr(n, idx);
      else { n->str[0] = '\0'; n->str_cur = 0; }
      return true;
    case '-':
      if (!(n->flag & NUM_EDIT_FULL)) { n->val_flag[idx] ^= NUM_NEGATE; updated = true; }
      break;
    case '/':
      if (!(n->flag & NUM_EDIT_FULL)) { n->val_flag[idx] ^= NUM_INVERSE; updated = true; }
      break;
    default:
      break;
  }

  if (!updated && event_ascii) { ascii[0] = event_ascii; utf8_buf = ascii; }

  if (utf8_buf && utf8_buf[0]) {
    if (!(n->flag & NUM_EDIT_FULL) && !editstr_is_simple_numinput(utf8_buf[0])) return false;
    if (!editstr_insert_at_cursor(n, utf8_buf, 1)) return false;
    n->val_flag[idx] |= NUM_EDITED;
  }
  else if (!updated) {
    return false;
  }

  if (n->str[0]) {
    const float val_prev = n->val[idx];
    double val;
    if (user_string_to_number(n, idx, n->str, &val)) {
      n->val[idx] = (float)val;
      n->val_flag[idx] &= ~NUM_INVALID;
    }
    else {
      n->val_flag[idx] |= NUM_INVALID;
    }
    if (n->val_flag[idx] & NUM_NEGATE) n->val[idx] = -n->val[idx];
    if (n->val_flag[idx] & NUM_INVERSE) {
      val = n->val[idx];
      if (!n->unit_use_radians && n->unit_type[idx] == PG_UNIT_ROTATION) val = val * 180.0 / M_PI;
      val = 1.0 / val;
      if (!n->unit_use_radians && n->unit_type[idx] == PG_UNIT_ROTATION) val = val * M_PI / 180.0;
      n->val[idx] = (float)val;
    }
    if (!isfinite(n->val[idx])) {
      n->val[idx] = val_prev;
      n->val_flag[idx] |= NUM_INVALID;
    }
  }
  return true;
}
