/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * L8 part 2: the Blender kernel / editor functions the GP draw engine calls that are not part of
 * Project Grease's closure, for its one Object (pg_drw_gles.cc) whose data is the document and
 * whose materials are bGPdata::mat[] (no Main, no Material ID system).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_material.h"
#include "BKE_object.h"

struct Depsgraph;
struct Image;
struct ImageUser;
struct ImBuf;
struct GPUTexture;
struct Camera;
struct bContext;
struct wmWindowManager;
struct View3D;
struct RenderEngine;
struct RenderLayer;
struct rcti;
struct GPENCIL_Data;

int pg_drw_cfra(void);

/* DEG_get_ctime(): the frame pg_gp_engine_draw was asked to draw. */
float DEG_get_ctime(const struct Depsgraph *graph)
{
  (void)graph;
  return (float)pg_drw_cfra();
}

/* DEG_get_original_id(): Project Grease has no copy-on-write evaluated IDs; an ID is its own
 * original (BKE_gpencil_boundbox_get then skips the "update the original" step). */
struct ID *DEG_get_original_id(struct ID *id)
{
  return id;
}

/* BEGIN VERBATIM source/blender/blenkernel/intern/material.cc */
void BKE_gpencil_material_attr_init(Material *ma)
{
  if ((ma) && (ma->gp_style == NULL)) {
    ma->gp_style = (MaterialGPencilStyle *)(
        MEM_callocN(sizeof(MaterialGPencilStyle), "Grease Pencil Material Settings"));

    MaterialGPencilStyle *gp_style = ma->gp_style;
    /* set basic settings */
    gp_style->stroke_rgba[3] = 1.0f;
    gp_style->fill_rgba[3] = 1.0f;
    ARRAY_SET_ITEMS(gp_style->mix_rgba, 1.0f, 1.0f, 1.0f, 1.0f);
    ARRAY_SET_ITEMS(gp_style->texture_scale, 1.0f, 1.0f);
    gp_style->texture_offset[0] = -0.5f;
    gp_style->texture_pixsize = 100.0f;
    gp_style->mix_factor = 0.5f;

    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
  }
}
/* END VERBATIM */

/* material.cc default_material_gpencil (material_default_gpencil_init). */
static Material pg_default_material_gpencil;
Material *BKE_material_default_gpencil(void)
{
  if (pg_default_material_gpencil.gp_style == NULL) {
    strcpy(pg_default_material_gpencil.id.name, "MADefault GPencil");
    BKE_gpencil_material_attr_init(&pg_default_material_gpencil);
    add_v3_fl(&pg_default_material_gpencil.gp_style->stroke_rgba[0], 0.6f);
  }
  return &pg_default_material_gpencil;
}

/* ADAPTED: BKE_object_material_get(ob, act) for a GP object whose slots are all data slots
 * (ob->matbits zero): the data's material act - 1, or NULL. */
Material *BKE_gpencil_material(Object *ob, short act)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  if (gpd == NULL || act < 1 || act > gpd->totcol || gpd->mat == NULL) {
    return NULL;
  }
  return gpd->mat[act - 1];
}

/* BEGIN VERBATIM source/blender/blenkernel/intern/material.cc */
MaterialGPencilStyle *BKE_gpencil_material_settings(Object *ob, short act)
{
  Material *ma = BKE_gpencil_material(ob, act);
  if (ma != NULL) {
    if (ma->gp_style == NULL) {
      BKE_gpencil_material_attr_init(ma);
    }

    return ma->gp_style;
  }

  return BKE_material_default_gpencil()->gp_style;
}
/* END VERBATIM */

/* ADAPTED: BKE_object_material_count_eval for the GP object: the data's slot count. */
int BKE_object_material_count_eval(Object *ob)
{
  const bGPdata *gpd = (const bGPdata *)ob->data;
  return gpd ? gpd->totcol : 0;
}

/* ADAPTED: BKE_object_boundbox_get, case OB_GPENCIL_LEGACY. */
const BoundBox *BKE_object_boundbox_get(Object *ob)
{
  return BKE_gpencil_boundbox_get(ob);
}

/* BEGIN VERBATIM source/blender/blenkernel/intern/object.cc */
void BKE_boundbox_calc_center_aabb(const BoundBox *bb, float r_cent[3])
{
  r_cent[0] = 0.5f * (bb->vec[0][0] + bb->vec[4][0]);
  r_cent[1] = 0.5f * (bb->vec[0][1] + bb->vec[2][1]);
  r_cent[2] = 0.5f * (bb->vec[0][2] + bb->vec[1][2]);
}

void BKE_boundbox_calc_size_aabb(const BoundBox *bb, float r_size[3])
{
  r_size[0] = 0.5f * fabsf(bb->vec[0][0] - bb->vec[4][0]);
  r_size[1] = 0.5f * fabsf(bb->vec[0][1] - bb->vec[2][1]);
  r_size[2] = 0.5f * fabsf(bb->vec[0][2] - bb->vec[1][2]);
}
/* END VERBATIM */

/* shortcut: no Image datablocks on Android (material textures are uploaded per slot by the
 * presenter), so texture materials draw with their colour like a missing image in Blender. Return
 * the slot's GPU texture here when the material texture upload moves to the engine. */
struct ImBuf *BKE_image_acquire_ibuf(struct Image *ima, struct ImageUser *iuser, void **r_lock)
{
  (void)ima;
  (void)iuser;
  *r_lock = NULL;
  return NULL;
}

void BKE_image_release_ibuf(struct Image *ima, struct ImBuf *ibuf, void *lock)
{
  (void)ima;
  (void)ibuf;
  (void)lock;
}

struct GPUTexture *BKE_image_get_gpu_texture(struct Image *image, struct ImageUser *iuser, struct ImBuf *ibuf)
{
  (void)image;
  (void)iuser;
  (void)ibuf;
  return NULL;
}

/* Reached only through paths pg_gp_engine_draw never takes: a camera with depth of field
 * (pd->camera is NULL in the 2D view), a bContext (evil_C is NULL), final renders
 * (DRW_state_is_image_render is false), the sbuffer stroke (sbuffer_used is 0 while drawing) and
 * the "fade 3D objects" overlay (off). They abort rather than return a made-up value. */
#define PG_UNREACHABLE(fn) \
  do { \
    fprintf(stderr, "%s: not reachable from pg_gp_engine_draw\n", fn); \
    abort(); \
  } while (0)

float BKE_camera_sensor_size(int sensor_fit, float sensor_x, float sensor_y)
{
  (void)sensor_fit;
  (void)sensor_x;
  (void)sensor_y;
  PG_UNREACHABLE(__func__);
}

float BKE_camera_object_dof_distance(const Object *ob)
{
  (void)ob;
  PG_UNREACHABLE(__func__);
}

struct wmWindowManager *CTX_wm_manager(const struct bContext *C)
{
  (void)C;
  PG_UNREACHABLE(__func__);
}

struct bScreen *ED_screen_animation_playing(const struct wmWindowManager *wm)
{
  (void)wm;
  PG_UNREACHABLE(__func__);
}

void ED_view3d_background_color_get(const struct Scene *scene, const struct View3D *v3d, float r_color[3])
{
  (void)scene;
  (void)v3d;
  (void)r_color;
  PG_UNREACHABLE(__func__);
}

bool BKE_gpencil_has_time_modifiers(Object *ob)
{
  (void)ob;
  PG_UNREACHABLE(__func__);
}

struct bGPDframe *BKE_gpencil_frame_retime_get(struct Depsgraph *depsgraph,
                                               struct Scene *scene,
                                               struct Object *ob,
                                               struct bGPDlayer *gpl)
{
  (void)depsgraph;
  (void)scene;
  (void)ob;
  (void)gpl;
  PG_UNREACHABLE(__func__);
}

void GPENCIL_render_to_image(void *vedata,
                             struct RenderEngine *engine,
                             struct RenderLayer *render_layer,
                             const struct rcti *rect)
{
  (void)vedata;
  (void)engine;
  (void)render_layer;
  (void)rect;
  PG_UNREACHABLE(__func__);
}

void ED_gpencil_drawing_reference_get(const struct Scene *scene,
                                      const struct Object *ob,
                                      char align_flag,
                                      float r_vec[3])
{
  (void)scene;
  (void)ob;
  (void)align_flag;
  (void)r_vec;
  PG_UNREACHABLE(__func__);
}

void ED_gpencil_tpoint_to_point(struct ARegion *region,
                                float origin[3],
                                const struct tGPspoint *tpt,
                                struct bGPDspoint *pt)
{
  (void)region;
  (void)origin;
  (void)tpt;
  (void)pt;
  PG_UNREACHABLE(__func__);
}
