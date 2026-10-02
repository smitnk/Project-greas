/* Test-only stand-in for BLI_lasso_2d.h (implemented in the test). */
#pragma once
#include <stdbool.h>
#include "DNA_vec_types.h"
void BLI_lasso_boundbox(rcti *rect, const int mcoords[][2], unsigned int mcoords_len);
bool BLI_lasso_is_point_inside(
    const int mcoords[][2], unsigned int mcoords_len, int sx, int sy, int error_value);
