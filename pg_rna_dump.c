/* L4 generator tool (host only): runs Blender 3.6.23's own RNA definitions for the Grease Pencil
 * structs (RNA_def_gpencil, RNA_def_greasepencil_modifier, RNA_def_shader_fx, exactly as makesrna's
 * preprocess does) through the real rna_define.c, then prints every property's effective metadata:
 * identifier, DNA struct.field, type, subtype, array length, hard/soft range, step, precision,
 * enum item values, boolean bit, and the names of custom range/set functions. Nothing is computed
 * here; the values are those rna_define.c stored. Output: one tab-separated line per property. */
#include <stdio.h>
#include <string.h>

#include "CLG_log.h"
#include "RNA_define.h"
#include "rna_internal.h"

static const char *s(const void *p) { return p ? (const char *)p : "-"; } /* preprocess stores names */

int main(void)
{
  CLG_init(); /* as makesrna main() */
  CLG_output_use_basename_set(true);
  BlenderRNA *brna = RNA_create();
  /* makesrna PROCESS_ITEMS order: the base structs the GP structs derive from come first. */
  RNA_def_rna(brna);
  RNA_def_ID(brna);
  StructDefRNA *before_gp = DefRNA.structs.last; /* dump only what the GP definitions add */
  RNA_def_gpencil(brna);
  RNA_def_greasepencil_modifier(brna);
  RNA_def_shader_fx(brna);
  if (DefRNA.error) {
    fprintf(stderr, "RNA definition error\n");
    return 1;
  }
  for (StructDefRNA *ds = before_gp->cont.next; ds; ds = ds->cont.next) {
    const StructRNA *srna = ds->srna;
    for (PropertyDefRNA *dp = ds->cont.properties.first; dp; dp = dp->next) {
      const PropertyRNA *prop = dp->prop;
      printf("%s\t%s\t%s.%s\t%d\t%d\t%u", srna->identifier, prop->identifier, s(dp->dnastructname),
             s(dp->dnaname), (int)prop->type, (int)prop->subtype, prop->totarraylength);
      if (prop->type == PROP_INT) {
        const IntPropertyRNA *ip = (const IntPropertyRNA *)prop;
        printf("\thard=%d..%d\tsoft=%d..%d\tstep=%d\trange_fn=%s\tset_fn=%s", ip->hardmin, ip->hardmax,
               ip->softmin, ip->softmax, ip->step, s(ip->range), s(ip->set));
      }
      else if (prop->type == PROP_FLOAT) {
        const FloatPropertyRNA *fp = (const FloatPropertyRNA *)prop;
        printf("\thard=%.9g..%.9g\tsoft=%.9g..%.9g\tstep=%.9g\tprecision=%d\trange_fn=%s\tset_fn=%s",
               fp->hardmin, fp->hardmax, fp->softmin, fp->softmax, fp->step, fp->precision, s(fp->range),
               s(fp->set));
      }
      else if (prop->type == PROP_ENUM) {
        const EnumPropertyRNA *ep = (const EnumPropertyRNA *)prop;
        printf("\titems=");
        int n = 0; /* items with an empty identifier are UI separators/headings, not values */
        for (int i = 0; i < ep->totitem; i++) {
          if (ep->item[i].identifier[0]) {
            printf("%s%d:%s", n++ ? "," : "", ep->item[i].value, ep->item[i].identifier);
          }
        }
        printf("\titem_fn=%s\tset_fn=%s", s(ep->item_fn), s(ep->set));
      }
      else if (prop->type == PROP_BOOLEAN) {
        const BoolPropertyRNA *bp = (const BoolPropertyRNA *)prop;
        printf("\tbit=%lld%s\tset_fn=%s", (long long)dp->booleanbit, dp->booleannegative ? "\tnegative" : "", s(bp->set));
      }
      printf("\n");
    }
  }
  CLG_exit();
  return 0;
}
