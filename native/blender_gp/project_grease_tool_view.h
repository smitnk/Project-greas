/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Internal header of the native tool session brush ports (project_grease_tool_*.c): the canvas
 * view adapter standing in for the 3D viewport, and the gpencil_utils.c helpers they share.
 * See project_grease_tool_util.c.
 */
#pragma once

#include <stdbool.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_vec_types.h"
#include "DNA_view2d_types.h"
#include "DNA_view3d_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Brush;
struct BoundBox;
struct MaterialGPencilStyle;
struct bGPDspoint;
struct bGPDstroke;
struct bGPdata;
struct bGPDlayer;

/* gpencil_intern.h GP_SpaceConversion (same members). */
typedef struct GP_SpaceConversion {
  struct Scene *scene;
  struct Object *ob;
  struct bGPdata *gpd;
  struct bGPDlayer *gpl;

  struct ScrArea *area;
  struct ARegion *region;
  struct View2D *v2d;

  rctf *subrect; /* for using the camera rect within the 3d view */
  rctf subrect_data;

  float mat[4][4]; /* transform matrix on the strokes (introduced in [b770964]) */
} GP_SpaceConversion;

/* The "viewport": canvas units scaled by the on-screen pixels per canvas unit. */
typedef struct PGToolView {
  float px_per_unit;
  ARegion region; /* regiondata -> this view */
  RegionView3D rv3d;
  ScrArea area;
  Scene scene;
  ToolSettings ts;
  Object ob;
} PGToolView;

void pg_tool_view_init(PGToolView *view, GP_SpaceConversion *gsc, float px_per_unit);

/* ED_view3d.h results / tests used by the ports. */
typedef enum eV3DProjStatus { V3D_PROJ_RET_OK = 0, V3D_PROJ_RET_NOP = 1 } eV3DProjStatus;
typedef enum eV3DProjTest { V3D_PROJ_TEST_NOP = 0 } eV3DProjTest;

eV3DProjStatus ED_view3d_project_int_global(const ARegion *region, const float co[3], int r_co[2],
                                            const eV3DProjTest flag);
eV3DProjStatus ED_view3d_project_float_global(const ARegion *region, const float co[3],
                                              float r_co[2], const eV3DProjTest flag);
float ED_view3d_calc_zfac(const RegionView3D *rv3d, const float co[3]);
float ED_view3d_calc_zfac_ex(const RegionView3D *rv3d, const float co[3], bool *r_flip);
void ED_view3d_win_to_delta(const ARegion *region, const float xy_delta[2], float zfac,
                            float r_out[3]);
void UI_view2d_view_to_region_clip(const View2D *v2d, float x, float y, int *r_region_x,
                                   int *r_region_y);
void UI_view2d_region_to_view(const View2D *v2d, float x, float y, float *r_view_x,
                              float *r_view_y);
#ifndef V2D_IS_CLIPPED
#  define V2D_IS_CLIPPED 12000 /* UI_view2d.h */
#endif

struct MaterialGPencilStyle *pg_tool_material_style(const struct bGPdata *gpd, short act);

/* Verbatim helpers (project_grease_tool_util.c). */
bool edge_inside_circle(const float cent[2], float radius, const float screen_co_a[2],
                        const float screen_co_b[2]);
float BKE_brush_curve_strength(const struct Brush *br, float p, const float len);
void BKE_boundbox_init_from_minmax(struct BoundBox *bb, const float min[3], const float max[3]);
bool gpencil_stroke_inside_circle(const float mval[2], int rad, int x0, int y0, int x1, int y1);
void gpencil_point_to_world_space(const struct bGPDspoint *pt, const float diff_mat[4][4],
                                  struct bGPDspoint *r_pt);
void gpencil_point_to_xy(const GP_SpaceConversion *gsc, const struct bGPDstroke *gps,
                         const struct bGPDspoint *pt, int *r_x, int *r_y);
void gpencil_point_3d_to_xy(const GP_SpaceConversion *gsc, const short flag, const float pt[3],
                            float xy[2]);
void gpencil_point_to_xy_fl(const GP_SpaceConversion *gsc, const struct bGPDstroke *gps,
                            const struct bGPDspoint *pt, float *r_x, float *r_y);
void ED_gpencil_projected_2d_bound_box(const GP_SpaceConversion *gsc, const struct bGPDstroke *gps,
                                       const float diff_mat[4][4], float r_min[2], float r_max[2]);
bool ED_gpencil_stroke_check_collision(const GP_SpaceConversion *gsc, struct bGPDstroke *gps,
                                       const float mval[2], const int radius,
                                       const float diff_mat[4][4]);
bool ED_gpencil_stroke_point_is_inside(const struct bGPDstroke *gps, const GP_SpaceConversion *gsc,
                                       const int mval[2], const float diff_mat[4][4]);

/* Original points carry no evaluated runtime links; the ported loops read pt->runtime.pt_orig and
 * idx_orig (Blender works on evaluated copies). These link every point of the editable active
 * frames to itself before a sample and clear the links afterwards. */
void pg_tool_link_runtime(struct bGPdata *gpd, bool link);

#ifdef __cplusplus
}
#endif
