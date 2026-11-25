/* SPDX-License-Identifier: GPL-2.0-or-later
 * Dependency graph of the rig and the Grease Pencil document (project_grease_depsgraph.cc), the
 * subset of Blender 3.6.23's depsgraph for one armature object driving one Grease Pencil object:
 * the same node types and operation codes, relations built as deg_builder_relations(_rig).cc does,
 * update tags flushed as deg_eval_flush.cc and operations run in dependency order as deg_eval.cc's
 * single-threaded pass. Each graph owns a copy-on-write copy of the rig (the evaluated object);
 * the active graph (the scene's) flushes results back to the original as Blender does for
 * DEG_is_active, others (another frame: onion skin, export) leave the original untouched.
 * Names keep the pg_ prefix where the link closure already has a DEG_ stub. */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct ID;
struct bGPdata;
typedef struct PGArmature PGArmature;
struct PGArmExternal;

/* IDs of the graph. */
enum {
  PG_DEG_ID_RIG = 0,       /* the armature object (OBArmature) and its bArmature */
  PG_DEG_ID_GP_DATA = 1,   /* the bGPdata: layers (opacity, hide) */
  PG_DEG_ID_GP_OBJECT = 2, /* the Grease Pencil object: its modifier stacks */
};

/* An active graph (DEG_is_active) flushes evaluated values back to the original data. */
struct Depsgraph *pg_deg_new(bool is_active);
void pg_deg_free(struct Depsgraph *graph);
/* The original rig (NULL removes it). A new rig rebuilds the relations and tags everything
 * (ID_RECALC_ALL, as a new ID node); `defer` keeps that update until the next refresh, so a
 * frame change on the way (a document still loading) does not evaluate drivers early. */
void pg_deg_set_rig(struct Depsgraph *graph, PGArmature *orig, bool defer);
/* The Grease Pencil IDs (gpd NULL: none). ext resolves the driver paths outside the rig;
 * geometry_eval is the GP object's GEOMETRY_EVAL (BKE_gpencil_modifiers_calc: the caller's
 * evaluated frames are dropped). Also registers the graph as gpd's (pg_deg_rig_for_frame). */
void pg_deg_set_gpencil(struct Depsgraph *graph, const struct bGPdata *gpd,
                        const struct PGArmExternal *ext, void (*geometry_eval)(void *user),
                        void *user);
/* DEG_relations_tag_update: rebuild before the next evaluation. */
void pg_deg_relations_tag_update(struct Depsgraph *graph);
/* DEG_id_tag_update: id PG_DEG_ID_*, recalc ID_RECALC_* (DNA_ID.h). */
void pg_deg_id_tag_update(struct Depsgraph *graph, int id, unsigned int recalc);
/* deg_evaluate_on_framechange / deg_evaluate_on_refresh. */
void pg_deg_evaluate_on_framechange(struct Depsgraph *graph, float ctime);
void pg_deg_evaluate_on_refresh(struct Depsgraph *graph);
/* The scene frame: pg_deg_rig_for_frame answers it with this graph's evaluated rig. */
void pg_deg_set_scene_frame(struct Depsgraph *graph, int frame);
/* DEG_get_original_id of the evaluated rig. */
PGArmature *pg_deg_rig_original(struct Depsgraph *graph);
/* DEG_get_evaluated_object of the rig (NULL before the first evaluation). */
PGArmature *pg_deg_rig_evaluated(struct Depsgraph *graph);
/* The evaluated rig at `frame` for the Armature modifier of gpd: the registered graph's own copy
 * at the scene frame, otherwise the copy of a non-active graph evaluated at `frame` (one per frame,
 * kept until the original changes). Without a registered graph, the rig pg_arm_register gave. */
const PGArmature *pg_deg_rig_for_frame(const struct bGPdata *gpd, int frame);

/* A one-shot active graph of `a` at `frame` (pg_arm_evaluate_ex): action, drivers and pose in
 * dependency order, flushed back to `a`. */
void pg_deg_rig_evaluate(PGArmature *a, float frame, const struct PGArmExternal *ext);

/* Queries of depsgraph_query.cc (DEG_is_active, DEG_get_ctime, DEG_get_original_id). */
bool pg_deg_is_active(const struct Depsgraph *graph);
float pg_deg_get_ctime(const struct Depsgraph *graph);
struct ID *pg_deg_get_original_id(struct ID *id);
/* The external paths of the graph (NULL: none). */
const struct PGArmExternal *pg_deg_external(const struct Depsgraph *graph);

/* Debug (tests): the operations the last evaluation ran, in order, as "OPCODE(name)" separated by
 * spaces ("BONE_DONE(Arm)", "DRIVER(pose.bones[\"B\"].location[1])"); returns the length. */
int pg_deg_debug_last_order(const struct Depsgraph *graph, char *buf, int cap);
/* Number of relations marked cyclic by the last build. */
int pg_deg_debug_cycles(const struct Depsgraph *graph);

#ifdef __cplusplus
}
#endif
