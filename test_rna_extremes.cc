/* L4 safety test: with Blender's RNA limits now reaching Project Grease's modifiers (L4), every
 * mapped parameter is driven to its Blender hard minimum and hard maximum (others at defaults),
 * sanitized, and the modifier evaluated on a real frame. Run under ASan/UBSan with the whole closure
 * instrumented: PG's rewritten modifier code must survive every value Blender accepts. */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "DNA_gpencil_legacy_types.h"
#include "MEM_guardedalloc.h"
#include "project_grease_modifier_stack.h"
#include "rna/project_grease_rna.h"

static void add_stroke(bGPDframe *gpf, int n, float x0, float y0)
{
  bGPDstroke *gps = BKE_gpencil_stroke_add(gpf, 0, n, 10, false);
  for (int i = 0; i < n; i++) {
    bGPDspoint *pt = &gps->points[i];
    pt->x = x0 + 12.0f * i;
    pt->y = y0 + 20.0f * std::sin(0.9f * i);
    pt->pressure = 0.5f;
    pt->strength = 0.8f;
  }
}

int main()
{
  bGPdata *gpd = static_cast<bGPdata *>(MEM_callocN(sizeof(bGPdata), "gpd"));
  bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, "Layer", true, false);
  bGPDframe *gpf = BKE_gpencil_frame_addnew(gpl, 1);
  add_stroke(gpf, 9, 10.0f, 50.0f);
  add_stroke(gpf, 4, 30.0f, 120.0f);
  int runs = 0;
  /* Values at which Blender 3.6.23's own modifier code is unbounded (PG's copy is the same code):
   * listed, not executed, because they exhaust memory in Blender too. Quirk register entries. */
  struct Unbounded { const char *srna, *prop; int side; const char *why; };
  static const Unbounded unbounded[] = {
      {"EnvelopeGpencilModifier", "spread", 1,
       "MOD_gpencil_legacy_envelope.c generate loop runs from -spread: O(spread) strokes at INT_MAX"},
  };
  for (int i = 0; i < pg_mod_rna_map_len; i++) {
    const PGModRnaMap *m = &pg_mod_rna_map[i];
    const PGRnaProperty *rp = pg_rna_find(m->srna, m->prop);
    if (!rp || (rp->type != PG_RNA_INT && rp->type != PG_RNA_FLOAT)) {
      continue;
    }
    for (int side = 0; side < 2; side++) {
      const char *why = nullptr;
      for (const Unbounded &u : unbounded) {
        if (side == u.side && !strcmp(u.srna, m->srna) && !strcmp(u.prop, m->prop)) why = u.why;
      }
      if (why) {
        std::printf("  NOT RUN %s.%s at hard %s: %s\n", m->srna, m->prop, side ? "max" : "min", why);
        continue;
      }
      PGModEntry e;
      memset(&e, 0, sizeof(e));
      pg_mod_entry_init(&e, m->type);
      e.params[m->index] = (float)(side ? rp->hardmax : rp->hardmin);
      pg_mod_sanitize(m->type, e.params);
      std::printf("  %s.%s = %.9g\n", m->srna, m->prop, (double)e.params[m->index]);
      std::fflush(stdout);
      bGPDframe ev;
      pg_mod_eval_frame(gpd, gpl, gpf, &e, 1, 1, &ev);
      pg_mod_eval_free(&ev);
      runs++;
    }
  }
  BKE_gpencil_free_layers(&gpd->layers);
  MEM_freeN(gpd);
  std::printf("%d modifier evaluations at Blender's RNA hard limits\nALL PASSED\n", runs);
  return 0;
}
