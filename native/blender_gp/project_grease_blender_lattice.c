/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Lattice warp for the lasso transform (Blender 3.6.23 lattice deform, verbatim).
 *
 * The selection's bounding box is a Lattice object (pntsu x pntsv x 1 points, flag 0 so the rest
 * points span -1..1 like BKE_lattice_resize without an object); moving its points deforms the
 * selected stroke points with BKE_lattice_deform_data_eval_co, as the Lattice modifier does.
 * The lookups the verbatim code makes into the object/ID system are renamed to the small helpers
 * below: a plain lattice object with no edit lattice, display list or vertex groups.
 */
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_simd.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_displist.h"

#include "project_grease_blender_lattice.h"

void key_curve_position_weights(float t, float data[4], int type);

static Lattice *pg_object_get_lattice(const Object *object)
{
  return (Lattice *)object->data;
}
static DispList *pg_displist_find(ListBase *UNUSED(lb), int UNUSED(type)) { return NULL; }
static const MDeformVert *pg_lattice_deform_verts_get(const Object *UNUSED(oblatt)) { return NULL; }
static int pg_defgroup_name_index(const ID *UNUSED(id), const char *UNUSED(name)) { return -1; }
static float pg_defvert_find_weight(const MDeformVert *UNUSED(dv), int UNUSED(i)) { return 0.0f; }
#define BKE_object_get_lattice pg_object_get_lattice
#define BKE_displist_find pg_displist_find
#define BKE_lattice_deform_verts_get pg_lattice_deform_verts_get
#define BKE_id_defgroup_name_index pg_defgroup_name_index
#define BKE_defvert_find_weight pg_defvert_find_weight

/* BEGIN VERBATIM source/blender/blenkernel/intern/lattice.c */
void calc_lat_fudu(int flag, int res, float *r_fu, float *r_du)
{
  if (res == 1) {
    *r_fu = 0.0;
    *r_du = 0.0;
  }
  else if (flag & LT_GRID) {
    *r_fu = -0.5f * (res - 1);
    *r_du = 1.0f;
  }
  else {
    *r_fu = -1.0f;
    *r_du = 2.0f / (res - 1);
  }
}
/* END VERBATIM */

/* BEGIN VERBATIM source/blender/blenkernel/intern/lattice_deform.c */
typedef struct LatticeDeformData {
  /* Convert from object space to deform space */
  float latmat[4][4];
  /* Cached reference to the lattice to use for evaluation. When in edit mode this attribute
   * is set to the edit mode lattice. */
  const Lattice *lt;
  /* Preprocessed lattice points (converted to deform space). */
  float *latticedata;
  /* Prefetched DeformWeights of the lattice. */
  float *lattice_weights;
} LatticeDeformData;

LatticeDeformData *BKE_lattice_deform_data_create(const Object *oblatt, const Object *ob)
{
  /* we make an array with all differences */
  Lattice *lt = BKE_object_get_lattice(oblatt);
  BPoint *bp;
  DispList *dl = oblatt->runtime.curve_cache ?
                     BKE_displist_find(&oblatt->runtime.curve_cache->disp, DL_VERTS) :
                     NULL;
  const float *co = dl ? dl->verts : NULL;
  float *fp, imat[4][4];
  float fu, fv, fw;
  int u, v, w;
  float *latticedata;
  float *lattice_weights = NULL;
  float latmat[4][4];
  LatticeDeformData *lattice_deform_data;

  bp = lt->def;

  const int32_t num_points = lt->pntsu * lt->pntsv * lt->pntsw;
  /* We allocate one additional float for SSE2 optimizations. Without this
   * the SSE2 instructions for the last item would read in unallocated memory. */
  fp = latticedata = MEM_mallocN(sizeof(float[3]) * num_points + sizeof(float), "latticedata");

  /* for example with a particle system: (ob == NULL) */
  if (ob == NULL) {
    /* In deform-space, calc matrix. */
    invert_m4_m4(latmat, oblatt->object_to_world);

    /* back: put in deform array */
    invert_m4_m4(imat, latmat);
  }
  else {
    /* In deform-space, calc matrix. */
    invert_m4_m4(imat, oblatt->object_to_world);
    mul_m4_m4m4(latmat, imat, ob->object_to_world);

    /* back: put in deform array. */
    invert_m4_m4(imat, latmat);
  }

  /* Prefetch lattice deform group weights. */
  int defgrp_index = -1;
  const MDeformVert *dvert = BKE_lattice_deform_verts_get(oblatt);
  if (lt->vgroup[0] && dvert) {
    defgrp_index = BKE_id_defgroup_name_index(&lt->id, lt->vgroup);

    if (defgrp_index != -1) {
      lattice_weights = MEM_malloc_arrayN(num_points, sizeof(float), "lattice_weights");
      for (int index = 0; index < num_points; index++) {
        lattice_weights[index] = BKE_defvert_find_weight(dvert + index, defgrp_index);
      }
    }
  }

  for (w = 0, fw = lt->fw; w < lt->pntsw; w++, fw += lt->dw) {
    for (v = 0, fv = lt->fv; v < lt->pntsv; v++, fv += lt->dv) {
      for (u = 0, fu = lt->fu; u < lt->pntsu; u++, bp++, co += 3, fp += 3, fu += lt->du) {
        if (dl) {
          fp[0] = co[0] - fu;
          fp[1] = co[1] - fv;
          fp[2] = co[2] - fw;
        }
        else {
          fp[0] = bp->vec[0] - fu;
          fp[1] = bp->vec[1] - fv;
          fp[2] = bp->vec[2] - fw;
        }

        mul_mat3_m4_v3(imat, fp);
      }
    }
  }

  lattice_deform_data = MEM_mallocN(sizeof(LatticeDeformData), "Lattice Deform Data");
  lattice_deform_data->latticedata = latticedata;
  lattice_deform_data->lattice_weights = lattice_weights;
  lattice_deform_data->lt = lt;
  copy_m4_m4(lattice_deform_data->latmat, latmat);

  return lattice_deform_data;
}

void BKE_lattice_deform_data_eval_co(LatticeDeformData *lattice_deform_data,
                                     float co[3],
                                     float weight)
{
  float *latticedata = lattice_deform_data->latticedata;
  float *lattice_weights = lattice_deform_data->lattice_weights;
  BLI_assert(latticedata);
  const Lattice *lt = lattice_deform_data->lt;
  float u, v, w, tu[4], tv[4], tw[4];
  float vec[3];
  int idx_w, idx_v, idx_u;
  int ui, vi, wi, uu, vv, ww;

  /* vgroup influence */
  float co_prev[4] = {0}, weight_blend = 0.0f;
  copy_v3_v3(co_prev, co);
#ifdef BLI_HAVE_SSE2
  __m128 co_vec = _mm_loadu_ps(co_prev);
#endif

  /* co is in local coords, treat with latmat */
  mul_v3_m4v3(vec, lattice_deform_data->latmat, co);

  /* u v w coords */

  if (lt->pntsu > 1) {
    u = (vec[0] - lt->fu) / lt->du;
    ui = (int)floor(u);
    u -= ui;
    key_curve_position_weights(u, tu, lt->typeu);
  }
  else {
    tu[0] = tu[2] = tu[3] = 0.0;
    tu[1] = 1.0;
    ui = 0;
  }

  if (lt->pntsv > 1) {
    v = (vec[1] - lt->fv) / lt->dv;
    vi = (int)floor(v);
    v -= vi;
    key_curve_position_weights(v, tv, lt->typev);
  }
  else {
    tv[0] = tv[2] = tv[3] = 0.0;
    tv[1] = 1.0;
    vi = 0;
  }

  if (lt->pntsw > 1) {
    w = (vec[2] - lt->fw) / lt->dw;
    wi = (int)floor(w);
    w -= wi;
    key_curve_position_weights(w, tw, lt->typew);
  }
  else {
    tw[0] = tw[2] = tw[3] = 0.0;
    tw[1] = 1.0;
    wi = 0;
  }

  const int w_stride = lt->pntsu * lt->pntsv;
  const int idx_w_max = (lt->pntsw - 1) * lt->pntsu * lt->pntsv;
  const int v_stride = lt->pntsu;
  const int idx_v_max = (lt->pntsv - 1) * lt->pntsu;
  const int idx_u_max = (lt->pntsu - 1);

  for (ww = wi - 1; ww <= wi + 2; ww++) {
    w = weight * tw[ww - wi + 1];
    idx_w = CLAMPIS(ww * w_stride, 0, idx_w_max);
    for (vv = vi - 1; vv <= vi + 2; vv++) {
      v = w * tv[vv - vi + 1];
      idx_v = CLAMPIS(vv * v_stride, 0, idx_v_max);
      for (uu = ui - 1; uu <= ui + 2; uu++) {
        u = v * tu[uu - ui + 1];
        idx_u = CLAMPIS(uu, 0, idx_u_max);
        const int idx = idx_w + idx_v + idx_u;
#ifdef BLI_HAVE_SSE2
        {
          __m128 weight_vec = _mm_set1_ps(u);
          /* We need to address special case for last item to avoid accessing invalid memory. */
          __m128 lattice_vec;
          if (idx * 3 == idx_w_max) {
            copy_v3_v3((float *)&lattice_vec, &latticedata[idx * 3]);
          }
          else {
            /* When not on last item, we can safely access one extra float, it will be ignored
             * anyway. */
            lattice_vec = _mm_loadu_ps(&latticedata[idx * 3]);
          }
          co_vec = _mm_add_ps(co_vec, _mm_mul_ps(lattice_vec, weight_vec));
        }
#else
        madd_v3_v3fl(co, &latticedata[idx * 3], u);
#endif
        if (lattice_weights) {
          weight_blend += (u * lattice_weights[idx]);
        }
      }
    }
  }
#ifdef BLI_HAVE_SSE2
  {
    copy_v3_v3(co, (float *)&co_vec);
  }
#endif

  if (lattice_weights) {
    interp_v3_v3v3(co, co_prev, co, weight_blend);
  }
}

void BKE_lattice_deform_data_destroy(LatticeDeformData *lattice_deform_data)
{
  if (lattice_deform_data->latticedata) {
    MEM_freeN(lattice_deform_data->latticedata);
  }

  MEM_freeN(lattice_deform_data);
}
/* END VERBATIM */

int pg_lattice_rest_point(int nu, int nv, int i, int j, float r_xy[2])
{
  float fu, du, fv, dv;
  if (nu < 2 || nv < 2 || i < 0 || j < 0 || i >= nu || j >= nv) return 0;
  calc_lat_fudu(0, nu, &fu, &du);
  calc_lat_fudu(0, nv, &fv, &dv);
  r_xy[0] = fu + du * i;
  r_xy[1] = fv + dv * j;
  return 1;
}

int pg_lattice_deform_points(float *xy, int n, const float bounds[4], int nu, int nv,
                             const float *ctrl, int interp)
{
  if (!xy || !bounds || !ctrl || n < 0 || nu < 2 || nv < 2 || nu > 16 || nv > 16) return 0;
  if (interp != KEY_LINEAR && interp != KEY_CARDINAL && interp != KEY_BSPLINE &&
      interp != KEY_CATMULL_ROM)
  {
    return 0;
  }
  /* the lattice object spans the box: lattice space -1..1 -> canvas (a flat box gets a unit half
   * size so the matrix stays invertible) */
  const float cx = 0.5f * (bounds[0] + bounds[2]), cy = 0.5f * (bounds[1] + bounds[3]);
  const float hx = fmaxf(0.5f * (bounds[2] - bounds[0]), 1.0f);
  const float hy = fmaxf(0.5f * (bounds[3] - bounds[1]), 1.0f);
  Lattice lt;
  memset(&lt, 0, sizeof(lt));
  lt.id.name[0] = 'L';
  lt.id.name[1] = 'T';
  lt.pntsu = (short)nu;
  lt.pntsv = (short)nv;
  lt.pntsw = 1;
  lt.typeu = lt.typev = lt.typew = (char)interp;
  lt.actbp = LT_ACTBP_NONE;
  calc_lat_fudu(lt.flag, nu, &lt.fu, &lt.du);
  calc_lat_fudu(lt.flag, nv, &lt.fv, &lt.dv);
  calc_lat_fudu(lt.flag, 1, &lt.fw, &lt.dw);
  lt.def = MEM_callocN(sizeof(BPoint) * nu * nv, "pg lattice bp");
  for (int k = 0; k < nu * nv; k++) {
    lt.def[k].vec[0] = (ctrl[2 * k] - cx) / hx;
    lt.def[k].vec[1] = (ctrl[2 * k + 1] - cy) / hy;
    lt.def[k].vec[2] = 0.0f;
  }
  Object *ob = MEM_callocN(sizeof(Object), "pg lattice ob");
  ob->type = OB_LATTICE;
  ob->data = &lt;
  unit_m4(ob->object_to_world);
  ob->object_to_world[0][0] = hx;
  ob->object_to_world[1][1] = hy;
  ob->object_to_world[3][0] = cx;
  ob->object_to_world[3][1] = cy;
  LatticeDeformData *ldd = BKE_lattice_deform_data_create(ob, NULL);
  for (int k = 0; k < n; k++) {
    float co[3] = {xy[2 * k], xy[2 * k + 1], 0.0f};
    BKE_lattice_deform_data_eval_co(ldd, co, 1.0f);
    xy[2 * k] = co[0];
    xy[2 * k + 1] = co[1];
  }
  BKE_lattice_deform_data_destroy(ldd);
  MEM_freeN(ob);
  MEM_freeN(lt.def);
  return 1;
}
