/* Test-only stand-in: the bGPdata layout subset used by the selection code. Production
 * builds use Blender 3.6.23's real DNA_gpencil_legacy_types.h. */
#pragma once
#include "DNA_listBase.h"
#include "DNA_material_types.h"

#define GP_SPOINT_SELECT (1 << 0)
#define GP_SPOINT_TAG (1 << 1)
#define GP_STROKE_SELECT (1 << 1)
#define GP_STROKE_CYCLIC (1 << 7)
#define GP_STROKE_NEEDS_CURVE_UPDATE (1 << 12)
#define GP_FRAME_SELECT (1 << 1)
#define GP_LAYER_HIDE (1 << 0)
#define GP_LAYER_LOCKED (1 << 1)
#define GP_LAYER_UNLOCK_COLOR (1 << 12)
#define GP_DATA_STROKE_MULTIEDIT (1 << 9)
#define GP_DATA_CACHE_IS_DIRTY (1 << 22)

typedef struct bGPDspoint_Runtime { struct bGPDspoint *pt_orig; } bGPDspoint_Runtime;
typedef struct bGPDspoint {
  float x, y, z;
  float pressure, strength, time;
  int flag;
  float vert_color[4];
  bGPDspoint_Runtime runtime;
} bGPDspoint;

typedef struct bGPDstroke_Runtime { struct bGPDstroke *gps_orig; } bGPDstroke_Runtime;
typedef struct bGPDstroke {
  struct bGPDstroke *next, *prev;
  bGPDspoint *points;
  int totpoints;
  struct MDeformVert *dvert;
  int flag;
  int mat_nr;
  int select_index;
  short thickness;
  float hardeness;
  float fill_opacity_fac;
  float vert_color_fill[4];
  void *editcurve;
  bGPDstroke_Runtime runtime;
} bGPDstroke;

typedef struct bGPDframe {
  struct bGPDframe *next, *prev;
  ListBase strokes;
  int framenum;
  int flag;
} bGPDframe;

typedef struct bGPDlayer {
  struct bGPDlayer *next, *prev;
  ListBase frames;
  bGPDframe *actframe;
  int flag;
  char info[128];
  float opacity;
} bGPDlayer;

typedef struct bGPdata {
  ListBase layers;
  int flag;
  int select_last_index;
  Material **mat;
  short totcol;
} bGPdata;
