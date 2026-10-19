/* Test-only stand-in for BLI_listbase.h. */
#pragma once
#include <stdbool.h>
#include "DNA_listBase.h"
#define LISTBASE_FOREACH(type, var, list) \
  for (type var = (type)((list)->first); var != NULL; var = (type)(((Link *)(var))->next))
#define LISTBASE_FOREACH_MUTABLE(type, var, list) \
  for (type var = (type)((list)->first), *var##_iter_next; \
       ((var != NULL) ? ((void)(var##_iter_next = (type)(((Link *)(var))->next)), 1) : 0); \
       var = var##_iter_next)
static inline void BLI_remlink(ListBase *listbase, void *vlink)
{
  Link *link = (Link *)vlink;
  if (link->next) link->next->prev = link->prev;
  if (link->prev) link->prev->next = link->next;
  if (listbase->last == link) listbase->last = link->prev;
  if (listbase->first == link) listbase->first = link->next;
}
void BLI_addtail(ListBase *listbase, void *vlink);
void BLI_addhead(ListBase *listbase, void *vlink);
void *BLI_findstring(const ListBase *listbase, const char *id, int offset);
static inline bool BLI_listbase_is_empty(const ListBase *lb) { return lb->first == 0; }
void *BLI_findlink(const ListBase *listbase, int number);
static inline int BLI_listbase_count(const ListBase *lb)
{
  int n = 0;
  for (const Link *l = (const Link *)lb->first; l; l = l->next) n++;
  return n;
}
