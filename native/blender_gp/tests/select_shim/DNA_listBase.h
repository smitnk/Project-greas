/* Test-only stand-in for Blender's DNA_listBase.h. */
#pragma once
typedef struct Link { struct Link *next, *prev; } Link;
typedef struct ListBase { void *first, *last; } ListBase;
