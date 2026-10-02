/* Test-only stand-in for BLI_listbase.h. */
#pragma once
#include "DNA_listBase.h"
#define LISTBASE_FOREACH(type, var, list) \
  for (type var = (type)((list)->first); var != NULL; var = (type)(((Link *)(var))->next))
static inline void BLI_remlink(ListBase *listbase, void *vlink)
{
  Link *link = (Link *)vlink;
  if (link->next) link->next->prev = link->prev;
  if (link->prev) link->prev->next = link->next;
  if (listbase->last == link) listbase->last = link->prev;
  if (listbase->first == link) listbase->first = link->next;
}
void BLI_addtail(ListBase *listbase, void *vlink);
void *BLI_findlink(const ListBase *listbase, int number);
