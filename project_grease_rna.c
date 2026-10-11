/* L4: lookup over the generated Blender 3.6.23 RNA table, the generated-setter clamp, and Blender's
 * custom range/set functions for the 2D GP modifier properties (PLATFORM REPLACE of RNA: the
 * functions take the DNA struct directly instead of a PointerRNA). */
#include <string.h>

#include "BLI_utildefines.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_scene_types.h" /* MINFRAME, MAXFRAME */

#include "project_grease_rna.h"

const PGRnaProperty *pg_rna_find(const char *srna, const char *identifier)
{
  for (int i = 0; i < pg_rna_properties_len; i++) {
    const PGRnaProperty *p = &pg_rna_properties[i];
    if (STREQ(p->srna, srna) && STREQ(p->identifier, identifier)) {
      return p;
    }
  }
  return NULL;
}

/* makesrna.c rna_clamp_value(): the generated DNA setter is CLAMPIS(value, hardmin, hardmax) unless
 * the hard range is the full type range. */
double pg_rna_clamp(const PGRnaProperty *prop, double value)
{
  return value < prop->hardmin ? prop->hardmin : (value > prop->hardmax ? prop->hardmax : value);
}

int pg_rna_enum_valid(const PGRnaProperty *prop, int value)
{
  for (int i = 0; i < prop->enum_len; i++) {
    if (prop->enum_values[i] == value) {
      return 1;
    }
  }
  return 0;
}

/* ADAPTED: `md = ptr->data` -> md parameter (all six functions). */
void pg_rna_GpencilOpacity_range(const OpacityGpencilModifierData *md, float *min, float *max,
                                 float *softmin, float *softmax)
{
/* rna_GpencilOpacity_range */
/* BEGIN VERBATIM source/blender/makesrna/intern/rna_gpencil_legacy_modifier.c */
  *min = 0.0f;
  *softmin = 0.0f;

  *softmax = (md->flag & GP_OPACITY_NORMALIZE) ? 1.0f : 2.0f;
  *max = *softmax;
/* END VERBATIM */
}

void pg_rna_GpencilOpacity_max_set(OpacityGpencilModifierData *md, float value)
{
/* rna_GpencilOpacity_max_set */
/* BEGIN VERBATIM source/blender/makesrna/intern/rna_gpencil_legacy_modifier.c */
  md->factor = value;
  if (md->flag & GP_OPACITY_NORMALIZE) {
    if (md->factor > 1.0f) {
      md->factor = 1.0f;
    }
  }
/* END VERBATIM */
}

void pg_rna_TimeModifier_start_frame_set(TimeGpencilModifierData *tmd, int value)
{
/* rna_TimeModifier_start_frame_set */
/* BEGIN VERBATIM source/blender/makesrna/intern/rna_gpencil_legacy_modifier.c */
  CLAMP(value, MINFRAME, MAXFRAME);
  tmd->sfra = value;

  if (tmd->sfra >= tmd->efra) {
    tmd->efra = MIN2(tmd->sfra, MAXFRAME);
  }
/* END VERBATIM */
}

void pg_rna_TimeModifier_end_frame_set(TimeGpencilModifierData *tmd, int value)
{
/* rna_TimeModifier_end_frame_set */
/* BEGIN VERBATIM source/blender/makesrna/intern/rna_gpencil_legacy_modifier.c */
  CLAMP(value, MINFRAME, MAXFRAME);
  tmd->efra = value;

  if (tmd->sfra >= tmd->efra) {
    tmd->sfra = MAX2(tmd->efra, MINFRAME);
  }
/* END VERBATIM */
}

void pg_rna_Lineart_start_level_set(LineartGpencilModifierData *lmd, int value)
{
/* rna_Lineart_start_level_set */
/* BEGIN VERBATIM source/blender/makesrna/intern/rna_gpencil_legacy_modifier.c */
  CLAMP(value, 0, 128);
  lmd->level_start = value;
  lmd->level_end = MAX2(value, lmd->level_end);
/* END VERBATIM */
}

void pg_rna_Lineart_end_level_set(LineartGpencilModifierData *lmd, int value)
{
/* rna_Lineart_end_level_set */
/* BEGIN VERBATIM source/blender/makesrna/intern/rna_gpencil_legacy_modifier.c */
  CLAMP(value, 0, 128);
  lmd->level_end = value;
  lmd->level_start = MIN2(value, lmd->level_start);
/* END VERBATIM */
}
