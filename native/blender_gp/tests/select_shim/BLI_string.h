/* Test-only stand-in for BLI_string.h. */
#pragma once
#include <string.h>
#define STRNCPY(dst, src) (strncpy((dst), (src), sizeof(dst) - 1), (dst)[sizeof(dst) - 1] = 0)
