/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Focused extraction from Blender 3.6.23
 * e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca.
 *
 * Only the exact helpers required by BKE_gpencil_layer_addnew are included.
 * This is not a replacement Blender string subsystem.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

size_t BLI_strnlen(const char *s, const size_t maxlen)
{
  size_t len;
  for (len = 0; len < maxlen; len++, s++) {
    if (!*s) {
      break;
    }
  }
  return len;
}

char *BLI_strncpy(char *__restrict dst,
                  const char *__restrict src,
                  const size_t dst_maxncpy)
{
  BLI_string_debug_size(dst, dst_maxncpy);
  BLI_assert(dst_maxncpy != 0);
  size_t srclen = BLI_strnlen(src, dst_maxncpy - 1);
  memcpy(dst, src, srclen);
  dst[srclen] = '\0';
  return dst;
}

size_t BLI_vsnprintf_rlen(char *__restrict dst,
                          size_t dst_maxncpy,
                          const char *__restrict format,
                          va_list arg)
{
  BLI_string_debug_size(dst, dst_maxncpy);
  size_t n;
  BLI_assert(dst != NULL);
  BLI_assert(dst_maxncpy > 0);
  BLI_assert(format != NULL);
  n = (size_t)vsnprintf(dst, dst_maxncpy, format, arg);
  if (n != (size_t)-1 && n < dst_maxncpy) {
    /* pass */
  }
  else {
    n = dst_maxncpy - 1;
  }
  dst[n] = '\0';
  return n;
}

size_t BLI_snprintf_rlen(char *__restrict dst,
                         size_t dst_maxncpy,
                         const char *__restrict format,
                         ...)
{
  BLI_string_debug_size(dst, dst_maxncpy);
  size_t n;
  va_list arg;
  va_start(arg, format);
  n = BLI_vsnprintf_rlen(dst, dst_maxncpy, format, arg);
  va_end(arg);
  return n;
}

int BLI_snprintf(char *__restrict dst,
                 size_t dst_maxncpy,
                 const char *__restrict format,
                 ...)
{
  va_list arg;
  va_start(arg, format);
  int result = vsnprintf(dst, dst_maxncpy, format, arg);
  va_end(arg);
  return result;
}

size_t BLI_string_split_name_number(const char *name,
                                    const char delim,
                                    char *r_name_left,
                                    int *r_number)
{
  const size_t name_len = strlen(name);
  *r_number = 0;
  memcpy(r_name_left, name, (name_len + 1) * sizeof(char));

  if ((name_len > 1 && name[name_len - 1] == delim) == 0) {
    size_t a = name_len;
    while (a--) {
      if (name[a] == delim) {
        r_name_left[a] = '\0';
        *r_number = atol(name + a + 1);
        if (*r_number < 0) {
          *r_number = 0;
        }
        return a;
      }
      if (isdigit((unsigned char)name[a]) == 0) {
        break;
      }
    }
  }
  return name_len;
}

static int utf8_char_compute_skip(const char c)
{
  if (UNLIKELY(c >= 192)) {
    if ((c & 0xe0) == 0xc0) {
      return 2;
    }
    if ((c & 0xf0) == 0xe0) {
      return 3;
    }
    if ((c & 0xf8) == 0xf0) {
      return 4;
    }
    if ((c & 0xfc) == 0xf8) {
      return 5;
    }
    if ((c & 0xfe) == 0xfc) {
      return 6;
    }
  }
  return 1;
}

static char *str_utf8_copy_max_bytes_impl(char *dst,
                                          const char *src,
                                          size_t dst_maxncpy)
{
  size_t utf8_size;
  while ((utf8_size = (size_t)utf8_char_compute_skip(*src)) < dst_maxncpy) {
    dst_maxncpy -= utf8_size;
    switch (utf8_size) {
      case 6: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 5: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 4: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 3: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 2: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++; ATTR_FALLTHROUGH;
      case 1: if (UNLIKELY(!(*dst = *src++))) { return dst; } dst++;
    }
  }
  *dst = '\0';
  return dst;
}

size_t BLI_strncpy_utf8_rlen(char *__restrict dst,
                             const char *__restrict src,
                             size_t dst_maxncpy)
{
  BLI_assert(dst_maxncpy != 0);
  BLI_string_debug_size(dst, dst_maxncpy);
  char *r_dst = dst;
  dst = str_utf8_copy_max_bytes_impl(dst, src, dst_maxncpy);
  return (size_t)(dst - r_dst);
}

static bool uniquename_find_dupe(ListBase *list,
                                 void *vlink,
                                 const char *name,
                                 int name_offset)
{
  Link *link;
  for (link = list->first; link; link = link->next) {
    if (link != vlink) {
      if (STREQ(POINTER_OFFSET((const char *)link, name_offset), name)) {
        return true;
      }
    }
  }
  return false;
}

static bool uniquename_unique_check(void *arg, const char *name)
{
  struct {
    ListBase *lb;
    void *vlink;
    int name_offset;
  } *data = arg;
  return uniquename_find_dupe(data->lb, data->vlink, name, data->name_offset);
}

bool BLI_uniquename_cb(UniquenameCheckCallback unique_check,
                       void *arg,
                       const char *defname,
                       char delim,
                       char *name,
                       size_t name_maxncpy)
{
  BLI_string_debug_size_after_nil(name, name_maxncpy);

  if (name[0] == '\0') {
    BLI_strncpy(name, defname, name_maxncpy);
  }

  if (unique_check(arg, name)) {
    char numstr[16];
    char *tempname = alloca(name_maxncpy);
    char *left = alloca(name_maxncpy);
    int number;
    size_t len = BLI_string_split_name_number(name, delim, left, &number);
    do {
      const size_t numlen =
          (size_t)BLI_snprintf(numstr, sizeof(numstr), "%c%03d", delim, ++number) + 1;
      if ((len == 0) || (numlen >= name_maxncpy)) {
        BLI_strncpy(tempname, numstr, name_maxncpy);
      }
      else {
        char *tempname_buf;
        tempname_buf =
            tempname + BLI_strncpy_utf8_rlen(tempname, left, name_maxncpy - numlen);
        memcpy(tempname_buf, numstr, numlen);
      }
    } while (unique_check(arg, tempname));

    BLI_strncpy(name, tempname, name_maxncpy);
    return true;
  }

  return false;
}

bool BLI_uniquename(ListBase *list,
                    void *vlink,
                    const char *defname,
                    char delim,
                    int name_offset,
                    size_t name_maxncpy)
{
  struct {
    ListBase *lb;
    void *vlink;
    int name_offset;
  } data;
  data.lb = list;
  data.vlink = vlink;
  data.name_offset = name_offset;

  BLI_assert(name_maxncpy > 1);

  if (ELEM(NULL, vlink)) {
    return false;
  }

  return BLI_uniquename_cb(uniquename_unique_check,
                           &data,
                           defname,
                           delim,
                           POINTER_OFFSET(vlink, name_offset),
                           name_maxncpy);
}
