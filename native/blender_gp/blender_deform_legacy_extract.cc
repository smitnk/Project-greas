/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Focused Blender 3.6.23 Legacy Grease Pencil deform helpers.
 *
 * These are the small BKE_deform primitives required by the selected Legacy
 * GP geometry closure. The Android target deliberately does not link
 * Blender's full object/deform subsystem.
 */
#include <cstring>

#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"
#include "DNA_meshdata_types.h"

extern "C" MDeformWeight *BKE_defvert_find_index(const MDeformVert *dvert,
                                                  const int defgroup)
{
  if (dvert && defgroup >= 0) {
    MDeformWeight *dw = dvert->dw;
    uint i;
    for (i = dvert->totweight; i != 0; i--, dw++) {
      if (dw->def_nr == defgroup) {
        return dw;
      }
    }
  }
  else {
    BLI_assert(0);
  }
  return nullptr;
}

extern "C" MDeformWeight *BKE_defvert_ensure_index(MDeformVert *dvert, const int defgroup)
{
  MDeformWeight *dw_new;
  if (!dvert || defgroup < 0) {
    BLI_assert(0);
    return nullptr;
  }

  dw_new = BKE_defvert_find_index(dvert, defgroup);
  if (dw_new) {
    return dw_new;
  }

  dw_new = static_cast<MDeformWeight *>(
      MEM_mallocN(sizeof(MDeformWeight) * (dvert->totweight + 1), "deformWeight"));
  if (dvert->dw) {
    memcpy(dw_new, dvert->dw, sizeof(MDeformWeight) * dvert->totweight);
    MEM_freeN(dvert->dw);
  }

  dvert->dw = dw_new;
  dw_new += dvert->totweight;
  dw_new->weight = 0.0f;
  dw_new->def_nr = defgroup;
  dvert->totweight++;
  return dw_new;
}

extern "C" void BKE_defvert_array_copy(MDeformVert *dst,
                                         const MDeformVert *src,
                                         const int totvert)
{
  if (!src || !dst) {
    return;
  }

  memcpy(dst, src, sizeof(MDeformVert) * totvert);
  for (int i = 0; i < totvert; i++) {
    if (src[i].dw) {
      dst[i].dw = static_cast<MDeformWeight *>(
          MEM_mallocN(sizeof(MDeformWeight) * src[i].totweight, "copy_deformWeight"));
      memcpy(dst[i].dw, src[i].dw, sizeof(MDeformWeight) * src[i].totweight);
    }
  }
}

extern "C" float BKE_defvert_find_weight(const MDeformVert *dvert, const int defgroup)
{
  MDeformWeight *dw = BKE_defvert_find_index(dvert, defgroup);
  return dw ? dw->weight : 0.0f;
}
