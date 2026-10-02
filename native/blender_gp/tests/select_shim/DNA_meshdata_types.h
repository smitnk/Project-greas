/* Test-only stand-in for vertex weights. */
#pragma once
typedef struct MDeformWeight { unsigned int def_nr; float weight; } MDeformWeight;
typedef struct MDeformVert { MDeformWeight *dw; int totweight; int flag; } MDeformVert;
