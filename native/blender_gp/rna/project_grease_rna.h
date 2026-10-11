/* L4: Blender 3.6.23 RNA property metadata for the 2D Grease Pencil structs, without RNA's runtime
 * (no PointerRNA, Python or introspection). The table (project_grease_rna_gen.c) is generated from
 * Blender's own RNA definitions; the custom range/set functions are in project_grease_rna.c. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* PropertyType values (RNA_types.h). */
enum { PG_RNA_BOOLEAN = 0, PG_RNA_INT = 1, PG_RNA_FLOAT = 2, PG_RNA_STRING = 3, PG_RNA_ENUM = 4,
       PG_RNA_POINTER = 5, PG_RNA_COLLECTION = 6 };

typedef struct PGRnaProperty {
  const char *srna;       /* RNA struct identifier, e.g. "NoiseGpencilModifier" */
  const char *identifier; /* RNA property identifier, e.g. "factor" */
  const char *dna_struct; /* DNA struct and field it wraps (NULL when none) */
  const char *dna_field;
  int type, subtype, array_length;
  double hardmin, hardmax, softmin, softmax, step; /* int and float properties */
  int precision;
  const int *enum_values; /* enum properties: valid item values */
  int enum_len;
  long long boolean_bit; /* boolean properties stored as a flag bit */
  int boolean_negative;
  /* Names of Blender's custom functions (implemented in project_grease_rna.c where in scope). */
  const char *range_fn, *set_fn, *item_fn;
} PGRnaProperty;

extern const PGRnaProperty pg_rna_properties[];
extern const int pg_rna_properties_len;

/* The property srna.identifier, or NULL. */
const PGRnaProperty *pg_rna_find(const char *srna, const char *identifier);
/* Clamps to the hard range, as RNA_property_float_set / RNA_property_int_set do (static range only;
 * properties with range_fn need the struct, see below). */
double pg_rna_clamp(const PGRnaProperty *prop, double value);
/* 1 if value is one of the enum's item values. */
int pg_rna_enum_valid(const PGRnaProperty *prop, int value);

/* Blender's custom range / set functions for 2D GP modifier properties (rna_gpencil_legacy_modifier.c). */
struct OpacityGpencilModifierData;
struct TimeGpencilModifierData;
struct LineartGpencilModifierData;
void pg_rna_GpencilOpacity_range(const struct OpacityGpencilModifierData *md, float *min, float *max,
                                 float *softmin, float *softmax);
void pg_rna_GpencilOpacity_max_set(struct OpacityGpencilModifierData *md, float value);
void pg_rna_TimeModifier_start_frame_set(struct TimeGpencilModifierData *tmd, int value);
void pg_rna_TimeModifier_end_frame_set(struct TimeGpencilModifierData *tmd, int value);
void pg_rna_Lineart_start_level_set(struct LineartGpencilModifierData *lmd, int value);
void pg_rna_Lineart_end_level_set(struct LineartGpencilModifierData *lmd, int value);

/* Project Grease modifier parameters that are Blender RNA properties (project_grease_mod_rna.c). */
typedef struct PGModRnaMap {
  int type, index;        /* PG_MOD_* and PG_P_* index */
  const char *srna, *prop; /* the RNA property it is */
} PGModRnaMap;
extern const PGModRnaMap pg_mod_rna_map[];
extern const int pg_mod_rna_map_len;
/* Gives every mapped parameter of `type` Blender's RNA limits; def = pg_mod_defaults(type). */
void pg_mod_rna_sanitize(int type, float *params, const float *def);

#ifdef __cplusplus
}
#endif
