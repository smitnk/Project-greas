/* L4 host test: Project Grease's parameter sanitizing (pg_mod_sanitize, the limits every PG panel
 * uses) against Blender 3.6.23's RNA limits from the generated table (project_grease_rna_gen.c).
 * Each mapped parameter is set far outside its range and sanitized; the result must be what
 * Blender's RNA setter stores (makesrna rna_clamp_value: CLAMPIS to the hard range, or to the custom
 * range function). Enum parameters must end on a valid RNA item value, and valid values must pass
 * unchanged. Parameters PG adapted to its 2D canvas are not mapped (see test_dna_defaults.c). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DNA_gpencil_modifier_types.h"

#include "project_grease_modifier_stack.h"
#include "rna/project_grease_rna.h"

static int failures = 0, checked = 0;

typedef PGModRnaMap Map;
#define MAP pg_mod_rna_map

/* The value Blender's RNA setter stores for `v` (static or custom range). */
static double rna_store(const Map *m, const PGRnaProperty *rp, const float *p, double v)
{
  if (rp->range_fn && strcmp(rp->range_fn, "rna_GpencilOpacity_range") == 0) {
    OpacityGpencilModifierData md = {0};
    md.flag = p[PG_P_OPACITY_NORMALIZE] != 0.0f ? GP_OPACITY_NORMALIZE : 0;
    float lo, hi, slo, shi;
    pg_rna_GpencilOpacity_range(&md, &lo, &hi, &slo, &shi);
    return v < lo ? lo : (v > hi ? hi : v);
  }
  if (rp->set_fn && strncmp(rp->set_fn, "rna_TimeModifier_", 17) == 0) {
    TimeGpencilModifierData tmd = {0};
    tmd.sfra = (int)p[PG_P_TIME_SFRA];
    tmd.efra = (int)p[PG_P_TIME_EFRA];
    if (m->index == PG_P_TIME_SFRA) {
      pg_rna_TimeModifier_start_frame_set(&tmd, (int)v);
      return tmd.sfra;
    }
    pg_rna_TimeModifier_end_frame_set(&tmd, (int)v);
    return tmd.efra;
  }
  double r = pg_rna_clamp(rp, v);
  return rp->type == PG_RNA_INT ? (double)(int)r : (double)(float)r;
}

int main(void)
{
  const double probes[] = {-1e9, -1.5, 0.25, 7.0, 1e9};
  for (size_t i = 0; i < (size_t)pg_mod_rna_map_len; i++) {
    const Map *m = &MAP[i];
    const PGRnaProperty *rp = pg_rna_find(m->srna, m->prop);
    if (!rp) {
      printf("FAIL: no RNA property %s.%s\n", m->srna, m->prop);
      failures++;
      continue;
    }
    float p[PG_MOD_MAX_PARAMS];
    if (rp->type == PG_RNA_ENUM) {
      for (int v = -2; v <= 12; v++) {
        pg_mod_defaults(m->type, p);
        p[m->index] = (float)v;
        pg_mod_sanitize(m->type, p);
        checked++;
        const int got = (int)p[m->index];
        if (!pg_rna_enum_valid(rp, got) || (pg_rna_enum_valid(rp, v) && got != v)) {
          printf("FAIL: %s.%s: PG sanitizes enum %d to %d (RNA items:", m->srna, m->prop, v, got);
          for (int k = 0; k < rp->enum_len; k++) printf(" %d", rp->enum_values[k]);
          printf(")\n");
          failures++;
        }
      }
      continue;
    }
    if (rp->type != PG_RNA_INT && rp->type != PG_RNA_FLOAT) {
      printf("FAIL: %s.%s is not numeric (type %d)\n", m->srna, m->prop, rp->type);
      failures++;
      continue;
    }
    for (size_t k = 0; k < sizeof(probes) / sizeof(probes[0]); k++) {
      pg_mod_defaults(m->type, p);
      const double v = rp->type == PG_RNA_INT ? (double)(int)probes[k] : probes[k];
      p[m->index] = (float)v;
      pg_mod_sanitize(m->type, p);
      const double want = rna_store(m, rp, p, v);
      checked++;
      if (fabs((double)p[m->index] - want) > 1e-6 * fmax(1.0, fabs(want))) {
        printf("FAIL: %s.%s: set %.9g -> PG %.9g, Blender RNA %.9g (hard %.9g..%.9g%s%s)\n", m->srna,
               m->prop, v, (double)p[m->index], want, rp->hardmin, rp->hardmax,
               rp->range_fn ? ", range " : "", rp->range_fn ? rp->range_fn : "");
        failures++;
      }
    }
  }
  printf("%d sanitize results compared with Blender 3.6.23 RNA (%d mapped properties)\n", checked,
         pg_mod_rna_map_len);
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
