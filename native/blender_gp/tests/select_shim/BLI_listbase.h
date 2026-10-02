/* Test-only stand-in for BLI_listbase.h. */
#pragma once
#include "DNA_listBase.h"
#define LISTBASE_FOREACH(type, var, list) \
  for (type var = (type)((list)->first); var != NULL; var = (type)(((Link *)(var))->next))
