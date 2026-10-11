/* TEST-ONLY stand-in for the RNA_prototypes.h a full Blender build generates (makesrna). Only the
 * two StructRNA objects MOD_gpencil_legacy_dash.c / _time.c take the address of in their UI panel
 * code, which the L7 harness never runs. */
#pragma once
struct StructRNA;
extern struct StructRNA RNA_DashGpencilModifierSegment;
extern struct StructRNA RNA_TimeGpencilModifierSegment;
