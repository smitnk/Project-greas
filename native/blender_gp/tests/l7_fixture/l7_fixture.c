/* TEST-ONLY fixture for the L7 parity harness (never part of the Android closure).
 * Blender's MOD_gpencil_legacy_*.c files reference their UI panels (panelRegister) and a few
 * depsgraph queries. The panels are never called in the harness; the depsgraph queries are
 * answered from the harness's own scene: one frame number, original == evaluated. */
#include <stddef.h>

struct Depsgraph;
struct ID;
struct Material;
struct Object;
struct Scene;

float pg_l7_cfra = 1.0f;
struct Scene *pg_l7_scene = NULL;

float DEG_get_ctime(const struct Depsgraph *depsgraph) { (void)depsgraph; return pg_l7_cfra; }
struct ID *DEG_get_original_id(struct ID *id) { return id; }
struct Scene *DEG_get_evaluated_scene(const struct Depsgraph *depsgraph) { (void)depsgraph; return pg_l7_scene; }
int DEG_get_mode(const struct Depsgraph *depsgraph) { (void)depsgraph; return 0; /* DAG_EVAL_VIEWPORT */ }
void BKE_scene_graph_update_for_newframe(struct Depsgraph *depsgraph) { (void)depsgraph; }

/* UI: panelRegister callbacks only; unreachable from the harness. */
#define PG_L7_UI_UNREACHABLE(name) void name(void) { __builtin_trap(); }
PG_L7_UI_UNREACHABLE(uiItemR)
PG_L7_UI_UNREACHABLE(gpencil_modifier_subpanel_register)
PG_L7_UI_UNREACHABLE(gpencil_modifier_panel_get_property_pointers)
PG_L7_UI_UNREACHABLE(uiLayoutSetPropSep)
PG_L7_UI_UNREACHABLE(uiLayoutSetActive)
PG_L7_UI_UNREACHABLE(RNA_boolean_get)
PG_L7_UI_UNREACHABLE(uiLayoutRow)
PG_L7_UI_UNREACHABLE(gpencil_modifier_panel_register)
PG_L7_UI_UNREACHABLE(gpencil_modifier_panel_end)
PG_L7_UI_UNREACHABLE(gpencil_modifier_masking_panel_draw)
PG_L7_UI_UNREACHABLE(gpencil_modifier_curve_panel_draw)
PG_L7_UI_UNREACHABLE(gpencil_modifier_curve_header_draw)
PG_L7_UI_UNREACHABLE(uiLayoutColumn)
PG_L7_UI_UNREACHABLE(RNA_enum_get)

/* Materials: as Project Grease keeps them (bGPdata::mat[], see project_grease_blender_select.c),
 * so Blender's modifiers see the same material PG's do. */
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
struct Material *BKE_gpencil_material(struct Object *ob, short act)
{
  const bGPdata *gpd = (const bGPdata *)ob->data;
  return (act >= 1 && act <= gpd->totcol) ? gpd->mat[act - 1] : NULL;
}
struct MaterialGPencilStyle *BKE_gpencil_material_settings(struct Object *ob, short act)
{
  const bGPdata *gpd = (const bGPdata *)ob->data;
  if (act >= 1 && act <= gpd->totcol && gpd->mat[act - 1] && gpd->mat[act - 1]->gp_style) {
    return gpd->mat[act - 1]->gp_style;
  }
  return NULL;
}
PG_L7_UI_UNREACHABLE(DEG_add_object_relation)
PG_L7_UI_UNREACHABLE(uiTemplateColorRamp)
PG_L7_UI_UNREACHABLE(uiItemS)
/* Address-only UI references (see RNA_prototypes.h here). */
char RNA_DashGpencilModifierSegment, RNA_TimeGpencilModifierSegment;
PG_L7_UI_UNREACHABLE(uiItemO)
PG_L7_UI_UNREACHABLE(uiItemEnumO_string)
PG_L7_UI_UNREACHABLE(uiTemplateList)
PG_L7_UI_UNREACHABLE(uiLayoutRowWithHeading)
PG_L7_UI_UNREACHABLE(uiItemPointerR)
PG_L7_UI_UNREACHABLE(WM_uilisttype_add)
PG_L7_UI_UNREACHABLE(RNA_pointer_create)
PG_L7_UI_UNREACHABLE(RNA_int_get)
PG_L7_UI_UNREACHABLE(RNA_enum_set)
