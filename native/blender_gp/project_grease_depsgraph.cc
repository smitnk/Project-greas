/* SPDX-License-Identifier: GPL-2.0-or-later
 * The dependency graph of Project Grease (project_grease_depsgraph.h), ported from Blender 3.6.23's
 * source/blender/depsgraph for one armature object (the rig) and one Grease Pencil object:
 * - builder/deg_builder_nodes.cc / deg_builder_nodes_rig.cc: the operations (build_rig :132-291,
 *   build_ik_pose :66-95, build_pose_constraints :43-63, build_animdata / build_driver
 *   nodes.cc:1100-1209, build_object_data_geometry nodes.cc:1538-1568 and the GD datablock
 *   nodes.cc:1637-1647, build_parameters, the COPY_ON_WRITE operation);
 * - builder/deg_builder_relations.cc / deg_builder_relations_rig.cc: the relations (build_rig
 *   rig.cc:288-466, build_ik_pose rig.cc:51-205, build_inter_ik_chains rig.cc:263-285,
 *   build_constraints relations.cc:1214-1396 with bone_target_opcode :207-225, build_animdata
 *   :1398-1543, build_driver / build_driver_data / build_driver_variables :1614-1932,
 *   build_object_data_geometry :2289-2330, build_copy_on_write_relations :3199-3290,
 *   is_same_bone_dependency relations_impl.h:150-178, RNANodeQuery construct_node_identifier
 *   deg_builder_rna.cc:150-300 for the paths of project_grease_driver.h);
 * - builder/deg_builder_cycle.cc: cycle detection (relations of a cycle are RELATION_FLAG_CYCLIC);
 * - depsgraph_tag.cc: DEG_id_tag_update (depsgraph_tag_to_component_opcode :133-215,
 *   geometry_tag_to_component :575-616, the implied COPY_ON_WRITE tag);
 * - eval/deg_eval_flush.cc deg_graph_flush_updates :73-389 and eval/deg_eval.cc
 *   deg_evaluate_on_refresh :54-469 in its single-threaded order (COPY_ON_WRITE stage first);
 *   depsgraph_eval.cc deg_evaluate_on_framechange (time source tag);
 * - blenkernel/intern/armature_update.c:799-1050, the pose callbacks (BKE_pose_eval_init /
 *   _init_ik / _bone / constraints_evaluate / bone_done / bbone_segments / iktree_evaluate /
 *   eval_cleanup / eval_done, pose_channel_flush_to_orig_if_needed).
 * ADAPTED: Project Grease keeps arm->edbo all the time, so the callbacks do not return early on
 * it (the Armature modifier skips the deform in edit mode instead). Not ported: threads (the
 * single-threaded pass is the same order), visibility, Spline IK, physics, point caches, other
 * object types, the scene and view layer IDs. */
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"

#include "BIK_api.h"

#include "project_grease_armature.h"
#include "project_grease_armature_intern.h"
#include "project_grease_constraint.h"
#include "project_grease_depsgraph.h"
#include "project_grease_driver.h"

extern "C" void BKE_object_to_mat4(Object *ob, float r_mat[4][4]); /* project_grease_constraint.c */

namespace {

enum class NodeType {
  TIMESOURCE,
  COPY_ON_WRITE,
  PARAMETERS,
  ANIMATION,
  TRANSFORM,
  GEOMETRY,
  EVAL_POSE,
  BONE,
};

#define PG_OPCODES(X) \
  X(OPERATION) X(COPY_ON_WRITE) X(PARAMETERS_ENTRY) X(PARAMETERS_EVAL) X(PARAMETERS_EXIT) \
  X(ID_PROPERTY) X(ANIMATION_ENTRY) X(ANIMATION_EVAL) X(ANIMATION_EXIT) X(DRIVER) \
  X(TRANSFORM_LOCAL) X(TRANSFORM_FINAL) X(GEOMETRY_EVAL_INIT) X(GEOMETRY_EVAL) X(POSE_INIT) \
  X(POSE_INIT_IK) X(POSE_IK_SOLVER) X(POSE_CLEANUP) X(POSE_DONE) X(BONE_LOCAL) \
  X(BONE_POSE_PARENT) X(BONE_CONSTRAINTS) X(BONE_READY) X(BONE_DONE) X(BONE_SEGMENTS)
#define PG_ENUM(x) x,
enum class OperationCode { PG_OPCODES(PG_ENUM) };
#define PG_NAME(x) #x,
const char *const opcode_names[] = {PG_OPCODES(PG_NAME)};

/* depsgraph_relation.h */
enum {
  RELATION_FLAG_CYCLIC = (1 << 0),
  RELATION_FLAG_NO_FLUSH = (1 << 1),
  RELATION_FLAG_GODMODE = (1 << 4),
};

struct ComponentNode;
struct OperationNode;
struct Relation {
  OperationNode *from, *to;
  const char *name;
  int flag;
};

struct OperationNode {
  ComponentNode *owner = nullptr;
  OperationCode opcode = OperationCode::OPERATION;
  std::string name;
  int name_tag = -1;
  std::function<void(Depsgraph *)> evaluate; /* empty: a no-op */
  std::vector<Relation *> inlinks, outlinks;
  bool needs_update = false;
  bool scheduled = false;
  int num_links_pending = 0;
};

struct ComponentNode {
  int id = 0;
  NodeType type = NodeType::TIMESOURCE;
  std::string name;
  std::vector<OperationNode *> operations;
  OperationNode *entry_operation = nullptr, *exit_operation = nullptr;
  bool flushed = false;
  /* deg_node_component.cc get_entry_operation / get_exit_operation */
  OperationNode *get_entry_operation() const
  {
    if (entry_operation) return entry_operation;
    return operations.size() == 1 ? operations[0] : nullptr;
  }
  OperationNode *get_exit_operation() const
  {
    if (exit_operation) return exit_operation;
    return operations.size() == 1 ? operations[0] : nullptr;
  }
};

/* RootPChanMap (deg_builder_map.cc): the IK roots each bone is in. */
struct RootPChanMap {
  std::map<std::string, std::vector<std::string>> map;
  void add_bone(const char *bone, const char *root)
  {
    std::vector<std::string> &roots = map[bone];
    for (const std::string &r : roots) {
      if (r == root) return;
    }
    roots.emplace_back(root);
  }
  bool has_common_root(const char *bone1, const char *bone2) const
  {
    auto a = map.find(bone1), b = map.find(bone2);
    if (a == map.end() || b == map.end()) return false;
    for (const std::string &r : a->second) {
      for (const std::string &q : b->second) {
        if (r == q) return true;
      }
    }
    return false;
  }
};

}  // namespace

struct Depsgraph {
  bool is_active = false;
  /* pg_bke_pose_where_is: only the pose of `inplace`, evaluated on it (no copy). */
  Object *inplace = nullptr;
  PGArmature *orig = nullptr;
  PGArmature *cow = nullptr;
  const bGPdata *gpd = nullptr;
  PGArmExternal ext{};
  bool has_ext = false;
  void (*geometry_eval)(void *user) = nullptr;
  void *geometry_user = nullptr;
  float ctime = 1.0f;
  int scene_frame = 1;
  bool need_rebuild = true;
  bool deferred = false;

  std::vector<std::unique_ptr<ComponentNode>> components;
  std::vector<std::unique_ptr<OperationNode>> operations;
  std::vector<std::unique_ptr<Relation>> relations;
  OperationNode *time_source = nullptr;
  std::vector<OperationNode *> entry_tags;
  bool time_tagged = false;
  std::vector<std::pair<int, unsigned int>> pending_tags; /* tags made before the build */
  std::vector<bPoseChannel *> chan_array;                 /* pose->chan_array */
  std::string last_order;
  int cycles = 0;
  /* Non-active graphs of other frames (pg_deg_rig_for_frame). */
  std::map<int, Depsgraph *> frames;
  /* ID_GD copy-on-write: the callbacks (the graph of the scene) and, in a frame's graph, its copy. */
  PGDegGPencilCopy gp_cow_cb{};
  void *gp_cow = nullptr;

  Object *eval_object() { return inplace ? inplace : (cow ? &cow->ob : nullptr); }
  Object *orig_object() { return inplace ? inplace : (orig ? &orig->ob : nullptr); }
};

namespace {

using deg_fn = std::function<void(Depsgraph *)>;

/* ---------------------------------------------------------------------------------------- */
/* nodes                                                                                     */

ComponentNode *find_component(Depsgraph *g, int id, NodeType type, const char *name = "")
{
  for (auto &c : g->components) {
    if (c->id == id && c->type == type && c->name == name) return c.get();
  }
  return nullptr;
}

ComponentNode *add_component(Depsgraph *g, int id, NodeType type, const char *name = "")
{
  if (ComponentNode *c = find_component(g, id, type, name)) return c;
  g->components.push_back(std::make_unique<ComponentNode>());
  ComponentNode *c = g->components.back().get();
  c->id = id;
  c->type = type;
  c->name = name;
  return c;
}

OperationNode *find_operation(Depsgraph *g, int id, NodeType type, const char *comp_name,
                              OperationCode opcode, const char *name = "", int tag = -1)
{
  ComponentNode *c = find_component(g, id, type, comp_name);
  if (!c) return nullptr;
  for (OperationNode *op : c->operations) {
    if (op->opcode == opcode && op->name == name && op->name_tag == tag) return op;
  }
  return nullptr;
}

/* add_operation_node / ensure_operation_node */
OperationNode *add_operation(Depsgraph *g, int id, NodeType type, const char *comp_name,
                             OperationCode opcode, deg_fn fn = nullptr, const char *name = "",
                             int tag = -1)
{
  if (OperationNode *op = find_operation(g, id, type, comp_name, opcode, name, tag)) return op;
  ComponentNode *c = add_component(g, id, type, comp_name);
  g->operations.push_back(std::make_unique<OperationNode>());
  OperationNode *op = g->operations.back().get();
  op->owner = c;
  op->opcode = opcode;
  op->name = name;
  op->name_tag = tag;
  op->evaluate = std::move(fn);
  c->operations.push_back(op);
  return op;
}

/* add_relation, RELATION_CHECK_BEFORE_ADD always (a duplicate adds nothing) */
void add_relation(Depsgraph *g, OperationNode *from, OperationNode *to, const char *name, int flag = 0)
{
  if (!from || !to || from == to) return;
  for (Relation *r : from->outlinks) {
    if (r->to == to) {
      r->flag |= flag;
      return;
    }
  }
  g->relations.push_back(std::make_unique<Relation>(Relation{from, to, name, flag}));
  Relation *rel = g->relations.back().get();
  from->outlinks.push_back(rel);
  to->inlinks.push_back(rel);
}

/* ComponentKey relations: from the exit operation, to the entry operation. */
void add_relation(Depsgraph *g, ComponentNode *from, OperationNode *to, const char *name, int flag = 0)
{
  if (from) add_relation(g, from->get_exit_operation(), to, name, flag);
}
void add_relation(Depsgraph *g, OperationNode *from, ComponentNode *to, const char *name, int flag = 0)
{
  if (to) add_relation(g, from, to->get_entry_operation(), name, flag);
}

OperationNode *bone_op(Depsgraph *g, const char *bone, OperationCode opcode)
{
  return find_operation(g, PG_DEG_ID_RIG, NodeType::BONE, bone, opcode);
}
OperationNode *pose_op(Depsgraph *g, OperationCode opcode, const char *name = "")
{
  return find_operation(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE, "", opcode, name);
}

int pchan_index(const bPose *pose, const bPoseChannel *pchan)
{
  return BLI_findindex(&pose->chanbase, pchan);
}

bPoseChannel *pose_pchan_get_indexed(Depsgraph *g, int index)
{
  return index >= 0 && index < (int)g->chan_array.size() ? g->chan_array[(size_t)index] : nullptr;
}

/* armature.c:2748-2767 BKE_armature_ik_solver_find_root */
bPoseChannel *ik_solver_find_root(bPoseChannel *pchan, bKinematicConstraint *data)
{
  bPoseChannel *rootchan = pchan;
  if (!(data->flag & CONSTRAINT_IK_TIP)) {
    /* Exclude tip from chain. */
    rootchan = rootchan->parent;
  }
  if (rootchan != nullptr) {
    int segcount = 0;
    while (rootchan->parent) {
      /* Continue up chain, until we reach target number of items. */
      segcount++;
      if (segcount == data->rootbone) {
        break;
      }
      rootchan = rootchan->parent;
    }
  }
  return rootchan;
}

bool check_pchan_has_bbone(const bPoseChannel *pchan)
{
  return pchan->bone && pchan->bone->segments > 1;
}

/* ---------------------------------------------------------------------------------------- */
/* eval callbacks (armature_update.c:799-1050, object_update.cc)                             */

/* armature_update.c:917-935, and the deform dual quaternion: callers without a depsgraph (the
 * Armature modifier of a document with no graph) read the original's. */
void pose_channel_flush_to_orig_if_needed(Depsgraph *g, bPoseChannel *pchan)
{
  if (!g->is_active) {
    return;
  }
  bPoseChannel *pchan_orig = pchan->orig_pchan;
  if (pchan_orig == nullptr) {
    return;
  }
  copy_m4_m4(pchan_orig->pose_mat, pchan->pose_mat);
  copy_m4_m4(pchan_orig->chan_mat, pchan->chan_mat);
  copy_v3_v3(pchan_orig->pose_head, pchan->pose_mat[3]);
  copy_m4_m4(pchan_orig->constinv, pchan->constinv);
  copy_v3_v3(pchan_orig->pose_tail, pchan->pose_tail);
  pchan_orig->runtime.deform_dual_quat = pchan->runtime.deform_dual_quat;
}

void BKE_pose_eval_init(Depsgraph *g)
{
  Object *object = g->eval_object();
  bPose *pose = object->pose;
  BKE_pose_update_constraint_flags(pose);
  /* BKE_pose_pchan_index_rebuild */
  g->chan_array.clear();
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) g->chan_array.push_back(pchan);

  /* world_to_object is needed for solvers. */
  invert_m4_m4(object->world_to_object, object->object_to_world);

  /* clear flags */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    pchan->flag &= ~(POSE_DONE | POSE_CHAIN | POSE_IKTREE | POSE_IKSPLINE);
  }
}

void BKE_pose_eval_init_ik(Depsgraph *g)
{
  Object *object = g->eval_object();
  const float ctime = g->ctime;
  bArmature *armature = (bArmature *)object->data;
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  /* construct the IK tree (standard IK) */
  BIK_init_tree(g, nullptr, object, ctime);
}

void BKE_pose_eval_bone(Depsgraph *g, int index)
{
  Object *object = g->eval_object();
  const bArmature *armature = (bArmature *)object->data;
  bPoseChannel *pchan = pose_pchan_get_indexed(g, index);
  if (!pchan) return;
  if (armature->flag & ARM_RESTPOS) {
    Bone *bone = pchan->bone;
    if (bone) {
      copy_m4_m4(pchan->pose_mat, bone->arm_mat);
      copy_v3_v3(pchan->pose_head, bone->arm_head);
      copy_v3_v3(pchan->pose_tail, bone->arm_tail);
    }
  }
  else {
    /* TODO(sergey): Currently if there are constraints full transform is
     * being evaluated in BKE_pose_constraints_evaluate. */
    if (pchan->constraints.first == nullptr) {
      if (pchan->flag & POSE_IKTREE || pchan->flag & POSE_IKSPLINE) {
        /* pass */
      }
      else {
        if ((pchan->flag & POSE_DONE) == 0) {
          BKE_pose_where_is_bone(g, nullptr, object, pchan, g->ctime, true);
        }
      }
    }
  }
}

void BKE_pose_constraints_evaluate(Depsgraph *g, int index)
{
  Object *object = g->eval_object();
  const bArmature *armature = (bArmature *)object->data;
  bPoseChannel *pchan = pose_pchan_get_indexed(g, index);
  if (!pchan || (armature->flag & ARM_RESTPOS)) {
    return;
  }
  if (pchan->flag & POSE_IKTREE || pchan->flag & POSE_IKSPLINE) {
    /* IK are being solved separately/ */
  }
  else {
    if ((pchan->flag & POSE_DONE) == 0) {
      BKE_pose_where_is_bone(g, nullptr, object, pchan, g->ctime, true);
    }
  }
}

void BKE_pose_bone_done(Depsgraph *g, int index)
{
  bPoseChannel *pchan = pose_pchan_get_indexed(g, index);
  float imat[4][4];
  if (!pchan) return;
  if (pchan->bone) {
    invert_m4_m4(imat, pchan->bone->arm_mat);
    mul_m4_m4m4(pchan->chan_mat, pchan->pose_mat, imat);
    if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
      mat4_to_dquat(&pchan->runtime.deform_dual_quat, pchan->bone->arm_mat, pchan->chan_mat);
    }
  }
  pose_channel_flush_to_orig_if_needed(g, pchan);
}

void BKE_pose_eval_bbone_segments(Depsgraph *g, int index)
{
  bPoseChannel *pchan = pose_pchan_get_indexed(g, index);
  if (pchan && pchan->bone != nullptr && pchan->bone->segments > 1) {
    BKE_pchan_bbone_segments_cache_compute(pchan);
  }
}

void BKE_pose_iktree_evaluate(Depsgraph *g, int rootchan_index)
{
  Object *object = g->eval_object();
  const bArmature *armature = (bArmature *)object->data;
  bPoseChannel *rootchan = pose_pchan_get_indexed(g, rootchan_index);
  if (!rootchan || (armature->flag & ARM_RESTPOS)) {
    return;
  }
  BIK_execute_tree(g, nullptr, object, rootchan, g->ctime);
}

void BKE_pose_eval_cleanup(Depsgraph *g)
{
  /* Release the IK tree. */
  BIK_release_tree(nullptr, g->eval_object(), g->ctime);
}

/* object_update.cc BKE_object_eval_local_transform / BKE_object_eval_transform_final */
void BKE_object_eval_local_transform(Depsgraph *g)
{
  Object *ob = g->eval_object();
  BKE_object_to_mat4(ob, ob->object_to_world);
}
void BKE_object_eval_transform_final(Depsgraph *g)
{
  Object *ob = g->eval_object();
  invert_m4_m4(ob->world_to_object, ob->object_to_world);
}

/* deg_eval_copy_on_write.cc deg_update_copy_on_write_datablock: the evaluated copy of the rig,
 * with the links back to the original (ID.orig_id, bPoseChannel.orig_pchan). */
void deg_update_copy_on_write_datablock(Depsgraph *g)
{
  if (g->cow) pg_arm_free(g->cow);
  g->cow = pg_arm_dup(g->orig);
  g->cow->ob.id.orig_id = &g->orig->ob.id;
  bPoseChannel *o = static_cast<bPoseChannel *>(g->orig->pose.chanbase.first);
  LISTBASE_FOREACH (bPoseChannel *, pchan, &g->cow->pose.chanbase) {
    pchan->orig_pchan = o;
    o = o ? o->next : nullptr;
  }
}

/* ---------------------------------------------------------------------------------------- */
/* node builder                                                                              */

void build_transform(Depsgraph *g)
{
  OperationNode *op;
  op = add_operation(g, PG_DEG_ID_RIG, NodeType::TRANSFORM, "", OperationCode::TRANSFORM_LOCAL,
                     BKE_object_eval_local_transform);
  op->owner->entry_operation = op;
  op = add_operation(g, PG_DEG_ID_RIG, NodeType::TRANSFORM, "", OperationCode::TRANSFORM_FINAL,
                     BKE_object_eval_transform_final);
  op->owner->exit_operation = op;
}

/* deg_builder_nodes_rig.cc:132-291 build_rig (+ build_pose_constraints, build_ik_pose) */
void build_rig_nodes(Depsgraph *g, Object *object)
{
  OperationNode *op_node;
  op_node = add_operation(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE, "", OperationCode::POSE_INIT, BKE_pose_eval_init);
  op_node->owner->entry_operation = op_node;
  add_operation(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE, "", OperationCode::POSE_INIT_IK, BKE_pose_eval_init_ik);
  add_operation(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE, "", OperationCode::POSE_CLEANUP, BKE_pose_eval_cleanup);
  /* BKE_pose_eval_done only asserts */
  op_node = add_operation(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE, "", OperationCode::POSE_DONE, [](Depsgraph *) {});
  op_node->owner->exit_operation = op_node;
  int index = 0;
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    const char *name = pchan->name;
    op_node = add_operation(g, PG_DEG_ID_RIG, NodeType::BONE, name, OperationCode::BONE_LOCAL);
    op_node->owner->entry_operation = op_node;
    add_operation(g, PG_DEG_ID_RIG, NodeType::BONE, name, OperationCode::BONE_POSE_PARENT,
                  [index](Depsgraph *d) { BKE_pose_eval_bone(d, index); });
    /* NOTE: Dedicated noop for easier relationship construction. */
    add_operation(g, PG_DEG_ID_RIG, NodeType::BONE, name, OperationCode::BONE_READY);
    op_node = add_operation(g, PG_DEG_ID_RIG, NodeType::BONE, name, OperationCode::BONE_DONE,
                            [index](Depsgraph *d) { BKE_pose_bone_done(d, index); });
    /* B-Bone shape computation - the real last step if present. */
    if (check_pchan_has_bbone(pchan)) {
      op_node = add_operation(g, PG_DEG_ID_RIG, NodeType::BONE, name, OperationCode::BONE_SEGMENTS,
                              [index](Depsgraph *d) { BKE_pose_eval_bbone_segments(d, index); });
    }
    op_node->owner->exit_operation = op_node;
    /* Build constraints. */
    if (pchan->constraints.first != nullptr) {
      add_operation(g, PG_DEG_ID_RIG, NodeType::BONE, name, OperationCode::BONE_CONSTRAINTS,
                    [index](Depsgraph *d) { BKE_pose_constraints_evaluate(d, index); });
    }
    LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
      if (con->type != CONSTRAINT_TYPE_KINEMATIC) continue;
      /* build_ik_pose */
      bPoseChannel *rootchan = ik_solver_find_root(pchan, (bKinematicConstraint *)con->data);
      if (rootchan == nullptr) continue;
      const int rootchan_index = pchan_index(object->pose, rootchan);
      add_operation(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE, "", OperationCode::POSE_IK_SOLVER,
                    [rootchan_index](Depsgraph *d) { BKE_pose_iktree_evaluate(d, rootchan_index); },
                    rootchan->name);
    }
    index++;
  }
}

/* PARAMETERS_ENTRY / _EVAL / _EXIT (build_parameters) */
void build_parameters_nodes(Depsgraph *g, int id)
{
  OperationNode *op = add_operation(g, id, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_ENTRY);
  op->owner->entry_operation = op;
  add_operation(g, id, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_EVAL);
  op = add_operation(g, id, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_EXIT);
  op->owner->exit_operation = op;
}

void build_nodes(Depsgraph *g)
{
  Object *object = g->orig_object();
  g->time_source = add_operation(g, -1, NodeType::TIMESOURCE, "", OperationCode::OPERATION);
  if (!g->inplace) {
    add_operation(g, PG_DEG_ID_RIG, NodeType::COPY_ON_WRITE, "", OperationCode::COPY_ON_WRITE,
                  deg_update_copy_on_write_datablock);
  }
  build_transform(g);
  BKE_pose_update_constraint_flags(object->pose);
  build_rig_nodes(g, object);
  if (g->inplace) return;
  PGArmature *a = g->orig;
  build_parameters_nodes(g, PG_DEG_ID_RIG);
  /* custom properties: ID_PROPERTY in the bone's component or in PARAMETERS */
  LISTBASE_FOREACH (PGProp *, prop, &a->props) {
    if (prop->bone[0]) {
      add_operation(g, PG_DEG_ID_RIG, NodeType::BONE, prop->bone, OperationCode::ID_PROPERTY, nullptr, prop->name);
    }
    else {
      add_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::ID_PROPERTY, nullptr, prop->name);
    }
  }
  /* build_animdata: the action */
  if (!BLI_listbase_is_empty(&a->curves)) {
    OperationNode *op = add_operation(g, PG_DEG_ID_RIG, NodeType::ANIMATION, "", OperationCode::ANIMATION_ENTRY);
    op->owner->entry_operation = op;
    add_operation(g, PG_DEG_ID_RIG, NodeType::ANIMATION, "", OperationCode::ANIMATION_EVAL,
                  [](Depsgraph *d) { pg_animsys_eval_animdata(d, d->cow); });
    op = add_operation(g, PG_DEG_ID_RIG, NodeType::ANIMATION, "", OperationCode::ANIMATION_EXIT);
    op->owner->exit_operation = op;
  }
  /* build_driver */
  int driver_index = 0;
  LISTBASE_FOREACH (FCurve *, fcu, &a->drivers) {
    add_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::DRIVER,
                  [driver_index, fcu](Depsgraph *d) { pg_animsys_eval_driver(d, d->cow, driver_index, fcu); },
                  fcu->rna_path ? fcu->rna_path : "", fcu->array_index);
    driver_index++;
  }
  if (!g->gpd) return;
  /* build_object_data_geometry_datablock (ID_GD_LEGACY): BKE_gpencil_frame_active_set */
  add_operation(g, PG_DEG_ID_GP_DATA, NodeType::GEOMETRY, "", OperationCode::GEOMETRY_EVAL);
  build_parameters_nodes(g, PG_DEG_ID_GP_DATA);
  /* build_object_data_geometry: GEOMETRY_EVAL is BKE_object_eval_uber_data, the modifiers */
  OperationNode *op = add_operation(g, PG_DEG_ID_GP_OBJECT, NodeType::GEOMETRY, "", OperationCode::GEOMETRY_EVAL_INIT);
  op->owner->entry_operation = op;
  op = add_operation(g, PG_DEG_ID_GP_OBJECT, NodeType::GEOMETRY, "", OperationCode::GEOMETRY_EVAL, [](Depsgraph *d) {
    if (d->geometry_eval) d->geometry_eval(d->geometry_user);
  });
  op->owner->exit_operation = op;
  build_parameters_nodes(g, PG_DEG_ID_GP_OBJECT);
}

/* ---------------------------------------------------------------------------------------- */
/* relation builder                                                                          */

/* deg_builder_relations.cc:207-225 */
OperationCode bone_target_opcode(const char *subtarget, const char *component_subdata,
                                 const RootPChanMap *root_map)
{
  /* Same armature. root_map will be nullptr when building object-level constraints, and in that
   * case we don't need to check for the common chains. */
  if (root_map != nullptr) {
    /* Using "done" here breaks in-chain deps, while using
     * "ready" here breaks most production rigs instead.
     * So, we do a compromise here, and only do this when an
     * IK chain conflict may occur. */
    if (root_map->has_common_root(component_subdata, subtarget)) {
      return OperationCode::BONE_READY;
    }
  }
  return OperationCode::BONE_DONE;
}

/* deg_builder_relations.cc:1214-1396 build_constraints for a bone; every target is the rig
 * object itself (with a bone subtarget or none). */
void build_constraints(Depsgraph *g, Object *object, const char *component_subdata,
                       ListBase *constraints, const RootPChanMap *root_map)
{
  OperationNode *constraint_op_key = bone_op(g, component_subdata, OperationCode::BONE_CONSTRAINTS);
  LISTBASE_FOREACH (bConstraint *, con, constraints) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    ListBase targets = {nullptr, nullptr};
    if (cti == nullptr) {
      continue;
    }
    /* BKE_constraint_targets_get without the custom space target (never set here) */
    if (cti->get_constraint_targets && cti->get_constraint_targets(con, &targets)) {
      LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
        if (ct->tar == nullptr) {
          continue;
        }
        if (ELEM(con->type, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
          /* Ignore IK constraints - these are handled separately
           * (on pose level). */
        }
        else if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) {
          /* relation to bone */
          OperationCode opcode = bone_target_opcode(ct->subtarget, component_subdata, root_map);
          /* Armature constraint always wants the final position and chan_mat. */
          if (ELEM(con->type, CONSTRAINT_TYPE_ARMATURE)) {
            opcode = OperationCode::BONE_DONE;
          }
          add_relation(g, bone_op(g, ct->subtarget, opcode), constraint_op_key, cti->name);
        }
        else if (ct->tar == object) {
          /* Constraint targeting own object: just transform -> bone. */
          add_relation(g, find_operation(g, PG_DEG_ID_RIG, NodeType::TRANSFORM, "", OperationCode::TRANSFORM_FINAL),
                       constraint_op_key, cti->name);
        }
        /* Constraints which needs world's matrix for transform. */
        if (ELEM(con->type, CONSTRAINT_TYPE_ROTLIKE, CONSTRAINT_TYPE_SIZELIKE, CONSTRAINT_TYPE_LOCLIKE,
                 CONSTRAINT_TYPE_TRANSLIKE))
        {
          add_relation(g, find_component(g, PG_DEG_ID_RIG, NodeType::TRANSFORM), constraint_op_key, cti->name);
        }
      }
      if (cti->flush_constraint_targets) cti->flush_constraint_targets(con, &targets, true);
    }
  }
}

/* deg_builder_relations_rig.cc:263-285 */
void build_inter_ik_chains(Depsgraph *g, OperationNode *solver_key, const bPoseChannel *rootchan,
                           const RootPChanMap *root_map)
{
  const bPoseChannel *deepest_root = nullptr;
  const char *root_name = rootchan->name;
  /* Find shared IK chain root. */
  for (bPoseChannel *parchan = rootchan->parent; parchan; parchan = parchan->parent) {
    if (!root_map->has_common_root(root_name, parchan->name)) {
      break;
    }
    deepest_root = parchan;
  }
  if (deepest_root == nullptr) {
    return;
  }
  add_relation(g, bone_op(g, deepest_root->name, OperationCode::BONE_DONE), solver_key, "IK Chain Overlap");
}

/* deg_builder_relations_rig.cc:51-205 (the target is always the rig object itself) */
void build_ik_pose(Depsgraph *g, Object *object, bPoseChannel *pchan, bConstraint *con, RootPChanMap *root_map)
{
  if ((con->flag & CONSTRAINT_DISABLE) != 0) {
    return;
  }
  bKinematicConstraint *data = (bKinematicConstraint *)con->data;
  /* Attach owner to IK Solver to. */
  bPoseChannel *rootchan = ik_solver_find_root(pchan, data);
  if (rootchan == nullptr) {
    return;
  }
  OperationNode *init_ik_key = pose_op(g, OperationCode::POSE_INIT_IK);
  OperationNode *solver_key = pose_op(g, OperationCode::POSE_IK_SOLVER, rootchan->name);
  OperationNode *pose_cleanup_key = pose_op(g, OperationCode::POSE_CLEANUP);
  /* ADAPTED: isAnyPropertyAnimated is false (drivers only write bone channels / properties). */
  add_relation(g, init_ik_key, solver_key, "Init IK -> IK Solver");
  /* Never cleanup before solver is run. */
  add_relation(g, solver_key, pose_cleanup_key, "IK Solver -> Cleanup", RELATION_FLAG_GODMODE);
  OperationNode *target_dependent_key = solver_key;
  /* IK target */
  if (data->tar != nullptr && data->subtarget[0]) {
    /* Bone - use the final transformation. */
    add_relation(g, bone_op(g, data->subtarget, OperationCode::BONE_DONE), target_dependent_key, con->name);
    if (data->tar == object) {
      /* Prevent target's constraints from linking to anything from same
       * chain that it controls. */
      root_map->add_bone(data->subtarget, rootchan->name);
    }
  }
  /* Pole Target. */
  if (data->poletar != nullptr && data->polesubtarget[0]) {
    add_relation(g, bone_op(g, data->polesubtarget, OperationCode::BONE_DONE), target_dependent_key, con->name);
  }
  bPoseChannel *parchan = pchan;
  /* Exclude tip from chain if needed. */
  if (!(data->flag & CONSTRAINT_IK_TIP)) {
    parchan = pchan->parent;
  }
  root_map->add_bone(parchan->name, rootchan->name);
  add_relation(g, bone_op(g, parchan->name, OperationCode::BONE_READY), solver_key, "IK Solver Owner");
  /* Walk to the chain's root. */
  int segcount = 0;
  while (parchan != nullptr) {
    /* Make IK-solver dependent on this bone's result, since it can only run
     * after the standard results of the bone are know. Validate links step
     * on the bone will ensure that users of this bone only grab the result
     * with IK solver results. */
    if (parchan != pchan) {
      add_relation(g, bone_op(g, parchan->name, OperationCode::BONE_READY), solver_key, "IK Chain Parent");
      add_relation(g, solver_key, bone_op(g, parchan->name, OperationCode::BONE_DONE), "IK Chain Result");
    }
    else {
      add_relation(g, solver_key, bone_op(g, parchan->name, OperationCode::BONE_DONE), "IK Solver Result");
    }
    root_map->add_bone(parchan->name, rootchan->name);
    /* continue up chain, until we reach target number of items. */
    segcount++;
    if ((segcount == data->rootbone) || (segcount > 255)) {
      break;
    }
    parchan = parchan->parent;
  }
  add_relation(g, solver_key, pose_op(g, OperationCode::POSE_DONE), "PoseEval Result-Bone Link");
  /* Add relation when the root of this IK chain is influenced by another IK chain. */
  build_inter_ik_chains(g, solver_key, rootchan, root_map);
}

/* deg_builder_relations_rig.cc:288-466 */
void build_rig_relations(Depsgraph *g, Object *object)
{
  ComponentNode *local_transform = find_component(g, PG_DEG_ID_RIG, NodeType::TRANSFORM);
  OperationNode *pose_init_key = pose_op(g, OperationCode::POSE_INIT);
  OperationNode *pose_init_ik_key = pose_op(g, OperationCode::POSE_INIT_IK);
  OperationNode *pose_cleanup_key = pose_op(g, OperationCode::POSE_CLEANUP);
  OperationNode *pose_done_key = pose_op(g, OperationCode::POSE_DONE);
  add_relation(g, local_transform, pose_init_key, "Local Transform -> Pose Init");
  add_relation(g, pose_init_key, pose_init_ik_key, "Pose Init -> Pose Init IK");
  add_relation(g, pose_init_ik_key, pose_done_key, "Pose Init IK -> Pose Cleanup");
  /* Run cleanup even when there are no bones. */
  add_relation(g, pose_init_key, pose_cleanup_key, "Init -> Cleanup");
  RootPChanMap root_map;
  bool pose_depends_on_local_transform = false;
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
      switch (con->type) {
        case CONSTRAINT_TYPE_KINEMATIC:
          build_ik_pose(g, object, pchan, con, &root_map);
          pose_depends_on_local_transform = true;
          break;
        case CONSTRAINT_TYPE_ROTLIKE:
        case CONSTRAINT_TYPE_SIZELIKE:
        case CONSTRAINT_TYPE_LOCLIKE:
        case CONSTRAINT_TYPE_TRANSLIKE:
          pose_depends_on_local_transform = true;
          break;
        default:
          break;
      }
    }
  }
  if (pose_depends_on_local_transform) {
    add_relation(g, local_transform, find_component(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE)->get_entry_operation(),
                 "Local Transforms");
  }
  /* Links between operations for each bone. */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    OperationNode *bone_local_key = bone_op(g, pchan->name, OperationCode::BONE_LOCAL);
    OperationNode *bone_pose_key = bone_op(g, pchan->name, OperationCode::BONE_POSE_PARENT);
    OperationNode *bone_ready_key = bone_op(g, pchan->name, OperationCode::BONE_READY);
    OperationNode *bone_done_key = bone_op(g, pchan->name, OperationCode::BONE_DONE);
    /* Pose init to bone local. */
    add_relation(g, pose_init_key, bone_local_key, "Pose Init - Bone Local", RELATION_FLAG_GODMODE);
    /* Local to pose parenting operation. */
    add_relation(g, bone_local_key, bone_pose_key, "Bone Local - Bone Pose");
    /* Parent relation. */
    if (pchan->parent != nullptr) {
      /* NOTE: this difference in handling allows us to prevent lockups
       * while ensuring correct poses for separate chains. */
      const OperationCode parent_key_opcode = root_map.has_common_root(pchan->name, pchan->parent->name) ?
                                                  OperationCode::BONE_READY :
                                                  OperationCode::BONE_DONE;
      add_relation(g, bone_op(g, pchan->parent->name, parent_key_opcode), bone_pose_key, "Parent Bone -> Child Bone");
    }
    /* Build constraints. */
    if (pchan->constraints.first != nullptr) {
      /* Constraints stack and constraint dependencies. */
      build_constraints(g, object, pchan->name, &pchan->constraints, &root_map);
      /* Pose -> constraints. */
      OperationNode *constraints_key = bone_op(g, pchan->name, OperationCode::BONE_CONSTRAINTS);
      add_relation(g, bone_pose_key, constraints_key, "Pose -> Constraints Stack");
      add_relation(g, bone_local_key, constraints_key, "Local -> Constraints Stack");
      /* Constraints -> ready/ */
      add_relation(g, constraints_key, bone_ready_key, "Constraints -> Ready");
    }
    else {
      /* Pose -> Ready */
      add_relation(g, bone_pose_key, bone_ready_key, "Pose -> Ready");
    }
    /* Bone ready -> Bone done. */
    add_relation(g, bone_ready_key, bone_done_key, "Ready -> Done");
    /* B-Bone shape is the real final step after Done if present. */
    if (check_pchan_has_bbone(pchan)) {
      OperationNode *bone_segments_key = bone_op(g, pchan->name, OperationCode::BONE_SEGMENTS);
      /* B-Bone shape depends on the final position of the bone. */
      add_relation(g, bone_done_key, bone_segments_key, "Done -> B-Bone Segments");
      /* B-Bone shape depends on final position of handle bones. */
      bPoseChannel *prev, *next;
      BKE_pchan_bbone_handles_get(pchan, &prev, &next);
      if (prev) {
        OperationCode opcode = OperationCode::BONE_DONE;
        /* Inheriting parent roll requires access to prev handle's B-Bone properties. */
        if ((pchan->bone->bbone_flag & BBONE_ADD_PARENT_END_ROLL) != 0 && check_pchan_has_bbone(prev)) {
          opcode = OperationCode::BONE_SEGMENTS;
        }
        add_relation(g, bone_op(g, prev->name, opcode), bone_segments_key, "Prev Handle -> B-Bone Segments");
      }
      if (next) {
        add_relation(g, bone_op(g, next->name, OperationCode::BONE_DONE), bone_segments_key,
                     "Next Handle -> B-Bone Segments");
      }
      /* Pose requires the B-Bone shape. */
      add_relation(g, bone_segments_key, pose_done_key, "PoseEval Result-Bone Link", RELATION_FLAG_GODMODE);
      add_relation(g, bone_segments_key, pose_cleanup_key, "Cleanup dependency");
    }
    else {
      /* Assume that all bones must be done for the pose to be ready
       * (for deformers). */
      add_relation(g, bone_done_key, pose_done_key, "PoseEval Result-Bone Link");
      /* Bones must be traversed before cleanup. */
      add_relation(g, bone_done_key, pose_cleanup_key, "Done -> Cleanup");
      add_relation(g, bone_ready_key, pose_cleanup_key, "Ready -> Cleanup");
    }
  }
}

/* RNANodeQuery (deg_builder_rna.cc:150-300) for the paths of project_grease_driver.h. entry: the
 * operation a write goes to (RNAPointerSource::ENTRY), otherwise the one a read comes from. */
OperationNode *rna_node_query(Depsgraph *g, const char *path, bool with_index, bool entry)
{
  char bone[64], prop[64];
  switch (pg_drv_path_info(path, with_index, bone, prop)) {
    case PG_DRV_PATH_BONE_LOC:
    case PG_DRV_PATH_BONE_QUAT:
    case PG_DRV_PATH_BONE_EUL:
    case PG_DRV_PATH_BONE_SCALE:
      /* And other properties can always go to the entry operation. */
      return bone_op(g, bone, OperationCode::BONE_LOCAL);
    case PG_DRV_PATH_BONE_PROP:
      /* Custom properties of bones are placed in their components to improve granularity. */
      return find_operation(g, PG_DEG_ID_RIG, NodeType::BONE, bone, OperationCode::ID_PROPERTY, prop);
    case PG_DRV_PATH_OB_PROP:
      return find_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::ID_PROPERTY, prop);
    case PG_DRV_PATH_EXTERNAL: {
      /* GPencilLayer (the GP data) / GpencilModifier (the GP object): the geometry component as
       * the TO operation, PARAMETERS_EVAL as the FROM one */
      const int id = std::strstr(path, "].modifiers[") ? PG_DEG_ID_GP_OBJECT : PG_DEG_ID_GP_DATA;
      if (entry) {
        ComponentNode *c = find_component(g, id, NodeType::GEOMETRY);
        return c ? c->get_entry_operation() : nullptr;
      }
      return find_operation(g, id, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_EVAL);
    }
    default:
      return nullptr;
  }
}

/* relations_impl.h:150-178 */
bool is_same_bone_dependency(const OperationNode *op_from, const OperationNode *op_to)
{
  if (op_from == nullptr || op_to == nullptr) {
    return false;
  }
  /* We are only interested in relations like BONE_DONE -> BONE_LOCAL... */
  if (!(op_from->opcode == OperationCode::BONE_DONE && op_to->opcode == OperationCode::BONE_LOCAL)) {
    return false;
  }
  /* ... BUT, we also need to check if it's same bone. */
  return op_from->owner->name == op_to->owner->name;
}

/* deg_builder_relations.cc:1614-1932 build_driver (+ _data, _variables, _id_property) */
void build_driver(Depsgraph *g, FCurve *fcu)
{
  ChannelDriver *driver = fcu->driver;
  const char *rna_path = fcu->rna_path ? fcu->rna_path : "";
  OperationNode *driver_key = find_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::DRIVER,
                                             rna_path, fcu->array_index);
  OperationNode *parameters_key = find_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_EVAL);
  /* build_driver_data: Driver -> data components */
  OperationNode *self_key = rna_node_query(g, rna_path, false, true);
  if (self_key) {
    add_relation(g, driver_key, self_key, "Driver -> Driven Property");
    if (self_key->opcode == OperationCode::ID_PROPERTY) {
      add_relation(g, self_key, parameters_key, "Driven Property -> Properties");
    }
  }
  if (!driver) return;
  /* build_driver_variables */
  LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
    for (int i = 0; i < dvar->num_targets && i < MAX_DRIVER_TARGETS; i++) {
      DriverTarget *dtar = &dvar->targets[i];
      if (dtar->id == nullptr) {
        continue;
      }
      /* Special handling for directly-named bones. */
      if ((dtar->flag & DTAR_FLAG_STRUCT_REF) && dtar->pchan_name[0]) {
        OperationNode *variable_key = bone_op(g, dtar->pchan_name, OperationCode::BONE_DONE);
        if (variable_key == nullptr || is_same_bone_dependency(variable_key, self_key)) {
          continue;
        }
        add_relation(g, variable_key, driver_key, "Bone Target -> Driver");
      }
      else if (dtar->flag & DTAR_FLAG_STRUCT_REF) {
        /* Ignore input dependency if we're driving properties of
         * the same ID, otherwise we'll be ending up in a cyclic
         * dependency here. */
        continue;
      }
      else if (dtar->rna_path != nullptr && dtar->rna_path[0] != '\0') {
        OperationNode *variable_exit_key = rna_node_query(g, dtar->rna_path, true, false);
        if (variable_exit_key == nullptr || is_same_bone_dependency(variable_exit_key, self_key)) {
          continue;
        }
        add_relation(g, variable_exit_key, driver_key, "RNA Target -> Driver");
      }
    }
  }
  /* It's quite tricky to detect if the driver actually depends on time or not. */
  if (pg_drv_depends_on_time(driver)) {
    add_relation(g, g->time_source, driver_key, "TimeSrc -> Driver");
  }
}

void build_parameters_relations(Depsgraph *g, int id)
{
  OperationNode *entry = find_operation(g, id, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_ENTRY);
  OperationNode *eval = find_operation(g, id, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_EVAL);
  OperationNode *exit = find_operation(g, id, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_EXIT);
  add_relation(g, entry, eval, "Entry -> Eval");
  add_relation(g, eval, exit, "Entry -> Exit");
}

/* deg_builder_relations.cc:3199-3290 for the rig ID */
void build_copy_on_write_relations(Depsgraph *g)
{
  OperationNode *op_cow = find_operation(g, PG_DEG_ID_RIG, NodeType::COPY_ON_WRITE, "", OperationCode::COPY_ON_WRITE);
  if (!op_cow) return;
  for (auto &comp : g->components) {
    ComponentNode *comp_node = comp.get();
    if (comp_node->id != PG_DEG_ID_RIG || comp_node->type == NodeType::COPY_ON_WRITE) {
      continue;
    }
    int rel_flag = (RELATION_FLAG_NO_FLUSH | RELATION_FLAG_GODMODE);
    /* Parameters component is where drivers are living. Changing any of the (custom) properties
     * in the original datablock (even the ones which do not imply other component update) need to
     * make sure drivers are properly updated. */
    if (comp_node->type == NodeType::PARAMETERS) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* All entry operations of each component should wait for a proper
     * copy of ID. */
    OperationNode *op_entry = comp_node->get_entry_operation();
    if (op_entry != nullptr) {
      add_relation(g, op_cow, op_entry, "CoW Dependency", rel_flag);
    }
    /* All dangling operations should also be executed after copy-on-write. */
    for (OperationNode *op_node : comp_node->operations) {
      if (op_node == op_entry) {
        continue;
      }
      bool has_same_comp_dependency = false;
      for (Relation *rel_current : op_node->inlinks) {
        if (rel_current->from->owner == op_node->owner) {
          has_same_comp_dependency = true;
          break;
        }
      }
      if (!has_same_comp_dependency) {
        add_relation(g, op_cow, op_node, "CoW Dependency", rel_flag);
      }
    }
  }
}

void build_relations(Depsgraph *g)
{
  Object *object = g->orig_object();
  build_rig_relations(g, object);
  add_relation(g, find_operation(g, PG_DEG_ID_RIG, NodeType::TRANSFORM, "", OperationCode::TRANSFORM_LOCAL),
               find_operation(g, PG_DEG_ID_RIG, NodeType::TRANSFORM, "", OperationCode::TRANSFORM_FINAL),
               "Transform Local -> Final");
  if (g->inplace) return;
  PGArmature *a = g->orig;
  build_parameters_relations(g, PG_DEG_ID_RIG);
  OperationNode *rig_params_entry = find_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_ENTRY);
  OperationNode *rig_params_exit = find_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::PARAMETERS_EXIT);
  /* build_driver_id_property: ID properties between Init and Done */
  LISTBASE_FOREACH (PGProp *, prop, &a->props) {
    OperationNode *id_property_key = prop->bone[0] ?
        find_operation(g, PG_DEG_ID_RIG, NodeType::BONE, prop->bone, OperationCode::ID_PROPERTY, prop->name) :
        find_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::ID_PROPERTY, prop->name);
    if (prop->bone[0]) add_relation(g, rig_params_entry, id_property_key, "Init -> ID Property");
    add_relation(g, id_property_key, rig_params_exit, "ID Property -> Done");
  }
  /* build_animdata_curves */
  ComponentNode *adt_key = find_component(g, PG_DEG_ID_RIG, NodeType::ANIMATION);
  if (adt_key) {
    OperationNode *entry = find_operation(g, PG_DEG_ID_RIG, NodeType::ANIMATION, "", OperationCode::ANIMATION_ENTRY);
    OperationNode *eval = find_operation(g, PG_DEG_ID_RIG, NodeType::ANIMATION, "", OperationCode::ANIMATION_EVAL);
    OperationNode *exit = find_operation(g, PG_DEG_ID_RIG, NodeType::ANIMATION, "", OperationCode::ANIMATION_EXIT);
    add_relation(g, entry, eval, "Init -> Eval");
    add_relation(g, eval, exit, "Eval -> Exit");
    /* build_animdata_curves_targets */
    LISTBASE_FOREACH (FCurve *, fcu, &a->curves) {
      OperationNode *operation_to = rna_node_query(g, fcu->rna_path, false, true);
      if (operation_to == nullptr) {
        continue;
      }
      /* NOTE: Special case for bones, avoid relation from animation to
       * each of the bones. Bone evaluation could only start from pose
       * init anyway. */
      if (operation_to->opcode == OperationCode::BONE_LOCAL) {
        add_relation(g, adt_key, pose_op(g, OperationCode::POSE_INIT), "Animation -> Prop");
        continue;
      }
      add_relation(g, exit, operation_to, "Animation -> Prop");
    }
    /* build_animdata: Animation -> Parameters; the action depends on time */
    add_relation(g, adt_key, rig_params_entry, "Animation -> Parameters");
    add_relation(g, g->time_source, entry, "TimeSrc -> Animation");
  }
  /* build_animdata_drivers */
  LISTBASE_FOREACH (FCurve *, fcu, &a->drivers) {
    build_driver(g, fcu);
    /* prevent driver from occurring before own animation... */
    if (adt_key) {
      add_relation(g, adt_key,
                   find_operation(g, PG_DEG_ID_RIG, NodeType::PARAMETERS, "", OperationCode::DRIVER,
                                  fcu->rna_path ? fcu->rna_path : "", fcu->array_index),
                   "AnimData Before Drivers");
    }
  }
  if (g->gpd) {
    build_parameters_relations(g, PG_DEG_ID_GP_DATA);
    build_parameters_relations(g, PG_DEG_ID_GP_OBJECT);
    /* build_object_data_geometry */
    OperationNode *geom_init_key = find_operation(g, PG_DEG_ID_GP_OBJECT, NodeType::GEOMETRY, "", OperationCode::GEOMETRY_EVAL_INIT);
    OperationNode *obdata_ubereval_key = find_operation(g, PG_DEG_ID_GP_OBJECT, NodeType::GEOMETRY, "", OperationCode::GEOMETRY_EVAL);
    add_relation(g, find_component(g, PG_DEG_ID_GP_DATA, NodeType::GEOMETRY), geom_init_key, "Object Geometry Base Data");
    add_relation(g, geom_init_key, obdata_ubereval_key, "Object Geometry UberEval");
    /* MOD_gpencil_legacy_armature.c updateDepsgraph (shortcut: as if every layer had an
     * Armature modifier, upgrade when the stacks are part of the graph) */
    add_relation(g, find_component(g, PG_DEG_ID_RIG, NodeType::EVAL_POSE), obdata_ubereval_key, "Armature Modifier");
    add_relation(g, find_component(g, PG_DEG_ID_RIG, NodeType::TRANSFORM), obdata_ubereval_key, "Armature Modifier");
    /* BKE_gpencil_modifier_depends_ontime (Time Offset, Noise, Build ...) */
    add_relation(g, g->time_source, obdata_ubereval_key, "Time Source");
  }
  build_copy_on_write_relations(g);
}

/* builder/deg_builder_cycle.cc: a relation closing a cycle is marked RELATION_FLAG_CYCLIC (the
 * evaluation ignores it, so the cycle runs in some order instead of not at all). */
void detect_cycles(Depsgraph *g)
{
  enum { NONE, ACTIVE, DONE };
  std::map<OperationNode *, int> state;
  g->cycles = 0;
  for (auto &start : g->operations) {
    if (state[start.get()] != NONE) continue;
    std::vector<std::pair<OperationNode *, size_t>> stack{{start.get(), 0}};
    state[start.get()] = ACTIVE;
    while (!stack.empty()) {
      auto &[node, next] = stack.back();
      if (next < node->outlinks.size()) {
        Relation *rel = node->outlinks[next++];
        int &s = state[rel->to];
        if (s == ACTIVE) {
          rel->flag |= RELATION_FLAG_CYCLIC;
          g->cycles++;
        }
        else if (s == NONE) {
          s = ACTIVE;
          stack.emplace_back(rel->to, 0);
        }
      }
      else {
        state[node] = DONE;
        stack.pop_back();
      }
    }
  }
}

void free_frames(Depsgraph *g)
{
  for (auto &f : g->frames) pg_deg_free(f.second);
  g->frames.clear();
}

void graph_clear(Depsgraph *g)
{
  g->entry_tags.clear();
  g->relations.clear();
  g->operations.clear();
  g->components.clear();
  g->time_source = nullptr;
}

void tag_component(Depsgraph *g, ComponentNode *c)
{
  if (!c) return;
  for (OperationNode *op : c->operations) g->entry_tags.push_back(op);
}

/* depsgraph_tag.cc deg_graph_id_tag_update (depsgraph_tag_to_component_opcode,
 * geometry_tag_to_component); every tag of the rig also tags its copy-on-write. */
void id_tag_apply(Depsgraph *g, int id, unsigned int recalc)
{
  if (recalc == ID_RECALC_ALL) {
    for (auto &c : g->components) {
      if (c->id == id) tag_component(g, c.get());
    }
    return;
  }
  if (id == PG_DEG_ID_RIG && recalc != 0) recalc |= ID_RECALC_COPY_ON_WRITE;
  for (int bit = 0; bit < 32; bit++) {
    const unsigned int flag = 1u << bit;
    if (!(recalc & flag)) continue;
    NodeType type;
    switch (flag) {
      case ID_RECALC_TRANSFORM: type = NodeType::TRANSFORM; break;
      case ID_RECALC_GEOMETRY: type = id == PG_DEG_ID_RIG ? NodeType::EVAL_POSE : NodeType::GEOMETRY; break;
      case ID_RECALC_ANIMATION: type = NodeType::ANIMATION; break;
      case ID_RECALC_COPY_ON_WRITE: type = NodeType::COPY_ON_WRITE; break;
      case ID_RECALC_PARAMETERS: type = NodeType::PARAMETERS; break;
      default: continue;
    }
    tag_component(g, find_component(g, id, type));
  }
}

void ensure_built(Depsgraph *g)
{
  if (!g->need_rebuild) return;
  graph_clear(g);
  g->need_rebuild = false;
  if (!g->orig_object()) return;
  build_nodes(g);
  build_relations(g);
  detect_cycles(g);
}

/* deg_eval_flush.cc:345-389 deg_graph_flush_updates */
void deg_graph_flush_updates(Depsgraph *g)
{
  /* time_source->flush_update_tag */
  if (g->time_tagged && g->time_source) {
    for (Relation *rel : g->time_source->outlinks) g->entry_tags.push_back(rel->to);
  }
  g->time_tagged = false;
  if (g->entry_tags.empty()) {
    return;
  }
  for (auto &op : g->operations) op->scheduled = false;
  for (auto &c : g->components) c->flushed = false;
  std::deque<OperationNode *> queue;
  for (OperationNode *op_node : g->entry_tags) {
    if (op_node->scheduled) continue;
    queue.push_back(op_node);
    op_node->scheduled = true;
  }
  while (!queue.empty()) {
    OperationNode *op_node = queue.front();
    queue.pop_front();
    while (op_node != nullptr) {
      /* Tag operation as required for update. */
      op_node->needs_update = true;
      /* flush_handle_component_node: all operations of the component */
      ComponentNode *comp_node = op_node->owner;
      if (!comp_node->flushed) {
        comp_node->flushed = true;
        for (OperationNode *op : comp_node->operations) op->needs_update = true;
        /* when some target changes bone, we might need to re-run the
         * whole IK solver, otherwise result might be unpredictable. */
        if (comp_node->type == NodeType::BONE) {
          ComponentNode *pose_comp = find_component(g, comp_node->id, NodeType::EVAL_POSE);
          if (pose_comp && !pose_comp->flushed) {
            OperationNode *entry = pose_comp->get_entry_operation();
            if (!entry->scheduled) {
              entry->scheduled = true;
              queue.push_front(entry);
            }
          }
        }
      }
      /* flush_schedule_children */
      OperationNode *result = nullptr;
      for (Relation *rel : op_node->outlinks) {
        if (rel->flag & RELATION_FLAG_NO_FLUSH) {
          continue;
        }
        OperationNode *to_node = rel->to;
        if (to_node->scheduled) {
          continue;
        }
        if (result != nullptr) {
          queue.push_front(to_node);
        }
        else {
          result = to_node;
        }
        to_node->scheduled = true;
      }
      op_node = result;
    }
  }
}

/* eval/deg_eval.cc: the single-threaded pass, COPY_ON_WRITE stage first. */
void evaluate_graph(Depsgraph *g)
{
  /* calculate_pending_parents_for_node */
  for (auto &node : g->operations) {
    node->num_links_pending = 0;
    node->scheduled = false;
    if (!node->needs_update) continue;
    for (Relation *rel : node->inlinks) {
      if ((rel->flag & RELATION_FLAG_CYCLIC) == 0 && rel->from->needs_update) {
        ++node->num_links_pending;
      }
    }
  }
  g->last_order.clear();
  for (const bool cow_stage : {true, false}) {
    std::deque<OperationNode *> queue;
    std::function<void(OperationNode *, bool)> schedule_node = [&](OperationNode *node, bool dec_parents) {
      if (!node->needs_update) {
        return;
      }
      if (dec_parents) {
        node->num_links_pending--;
      }
      if (node->num_links_pending != 0) {
        return;
      }
      /* During the COW stage only schedule COW nodes. */
      if (cow_stage && node->owner->type != NodeType::COPY_ON_WRITE) {
        return;
      }
      if (node->scheduled) {
        return;
      }
      node->scheduled = true;
      if (!node->evaluate) {
        /* skip NOOP node, schedule children right away */
        node->needs_update = false;
        if (node != g->time_source) {
          g->last_order += opcode_names[(int)node->opcode];
          g->last_order += "(" + (node->owner->name.empty() ? node->name : node->owner->name) + ") ";
        }
        for (Relation *rel : node->outlinks) {
          if (!(rel->flag & RELATION_FLAG_CYCLIC)) schedule_node(rel->to, true);
        }
      }
      else {
        queue.push_back(node);
      }
    };
    for (auto &node : g->operations) schedule_node(node.get(), false);
    while (!queue.empty()) {
      OperationNode *node = queue.front();
      queue.pop_front();
      node->evaluate(g);
      node->needs_update = false;
      g->last_order += opcode_names[(int)node->opcode];
      g->last_order += "(" + (node->owner->name.empty() ? node->name : node->owner->name) + ") ";
      for (Relation *rel : node->outlinks) {
        if (!(rel->flag & RELATION_FLAG_CYCLIC)) schedule_node(rel->to, true);
      }
    }
  }
  if (!g->last_order.empty()) g->last_order.pop_back();
  /* deg_graph_clear_tags */
  g->entry_tags.clear();
  for (auto &node : g->operations) node->needs_update = false;
}

/* The registered graph of each GP document (the Armature modifier finds its rig there). */
struct Registry {
  const bGPdata *gpd;
  Depsgraph *graph;
};
std::vector<Registry> g_registry;

}  // namespace

/* ---------------------------------------------------------------------------------------- */
/* API                                                                                       */

extern "C" {

Depsgraph *pg_deg_new(bool is_active)
{
  Depsgraph *g = new Depsgraph();
  g->is_active = is_active;
  return g;
}

void pg_deg_free(Depsgraph *g)
{
  if (!g) return;
  for (size_t i = g_registry.size(); i-- > 0;) {
    if (g_registry[i].graph == g) g_registry.erase(g_registry.begin() + (std::ptrdiff_t)i);
  }
  free_frames(g);
  graph_clear(g);
  if (g->cow) pg_arm_free(g->cow);
  if (g->gp_cow && g->gp_cow_cb.free) g->gp_cow_cb.free(g->gp_cow);
  delete g;
}

void pg_deg_set_rig(Depsgraph *g, PGArmature *orig, bool defer)
{
  if (!g) return;
  free_frames(g);
  if (g->cow) pg_arm_free(g->cow);
  g->cow = nullptr;
  g->orig = orig;
  g->need_rebuild = true;
  g->pending_tags.clear();
  g->deferred = defer;
  if (orig) {
    /* a new ID node: everything is evaluated once */
    for (int id : {PG_DEG_ID_RIG, PG_DEG_ID_GP_DATA, PG_DEG_ID_GP_OBJECT}) g->pending_tags.emplace_back(id, ID_RECALC_ALL);
  }
}

void pg_deg_set_gpencil(Depsgraph *g, const bGPdata *gpd, const PGArmExternal *ext,
                        void (*geometry_eval)(void *user), void *user)
{
  if (!g) return;
  if (g->gpd != gpd) g->need_rebuild = true;
  g->gpd = gpd;
  g->has_ext = ext != nullptr;
  if (ext) g->ext = *ext;
  g->geometry_eval = geometry_eval;
  g->geometry_user = user;
  for (size_t i = g_registry.size(); i-- > 0;) {
    if (g_registry[i].graph == g || g_registry[i].gpd == gpd) g_registry.erase(g_registry.begin() + (std::ptrdiff_t)i);
  }
  if (gpd) g_registry.push_back({gpd, g});
}

void pg_deg_relations_tag_update(Depsgraph *g)
{
  if (!g) return;
  g->need_rebuild = true;
  free_frames(g);
}

void pg_deg_id_tag_update(Depsgraph *g, int id, unsigned int recalc)
{
  if (!g) return;
  g->pending_tags.emplace_back(id, recalc);
  free_frames(g);
}

void pg_deg_evaluate_on_refresh(Depsgraph *g)
{
  if (!g) return;
  g->deferred = false;
  if (!g->orig_object()) {
    graph_clear(g);
    return;
  }
  const bool rebuilt = g->need_rebuild;
  ensure_built(g);
  if (rebuilt) {
    /* a rebuild keeps the evaluated copy: only what is tagged runs, except a missing copy */
    if (!g->inplace && !g->cow) g->pending_tags.emplace_back(PG_DEG_ID_RIG, ID_RECALC_ALL);
  }
  for (const auto &t : g->pending_tags) id_tag_apply(g, t.first, t.second);
  g->pending_tags.clear();
  deg_graph_flush_updates(g);
  bool any = false;
  for (auto &op : g->operations) any |= op->needs_update;
  if (!any) {
    g->entry_tags.clear();
    return;
  }
  if (g->is_active) free_frames(g);
  evaluate_graph(g);
}

void pg_deg_evaluate_on_framechange(Depsgraph *g, float ctime)
{
  if (!g) return;
  g->ctime = ctime;
  /* DEG_graph_time_tag_update */
  g->time_tagged = true;
  if (g->deferred) return;
  pg_deg_evaluate_on_refresh(g);
}

void pg_deg_set_scene_frame(Depsgraph *g, int frame)
{
  if (g) g->scene_frame = frame;
}

PGArmature *pg_deg_rig_evaluated(Depsgraph *g) { return g ? g->cow : nullptr; }
PGArmature *pg_deg_rig_original(Depsgraph *g) { return g ? g->orig : nullptr; }

/* The non-active graph of `frame` (made once, kept until the original changes). Its GP data is
 * copied first (deg_update_copy_on_write_datablock of ID_GD), so its drivers write into the copy. */
static Depsgraph *frame_graph(Depsgraph *g, int frame)
{
  auto it = g->frames.find(frame);
  if (it != g->frames.end()) return it->second;
  /* shortcut: at most 32 other frames are kept, all are dropped past that */
  if (g->frames.size() >= 32) free_frames(g);
  Depsgraph *f = pg_deg_new(false);
  if (g->gp_cow_cb.copy) {
    f->gp_cow_cb = g->gp_cow_cb;
    f->gp_cow = g->gp_cow_cb.copy(g->gp_cow_cb.user);
    const PGArmExternal ext = {f->gp_cow, g->gp_cow_cb.get, g->gp_cow_cb.set};
    pg_deg_set_gpencil(f, nullptr, &ext, nullptr, nullptr);
  }
  else {
    pg_deg_set_gpencil(f, nullptr, g->has_ext ? &g->ext : nullptr, nullptr, nullptr);
  }
  pg_deg_set_rig(f, g->orig, false);
  pg_deg_evaluate_on_framechange(f, (float)frame);
  return g->frames.emplace(frame, f).first->second;
}

static Depsgraph *registered_graph(const bGPdata *gpd)
{
  Depsgraph *g = nullptr;
  for (const Registry &r : g_registry) {
    if (r.gpd == gpd) g = r.graph;
  }
  return g;
}

const PGArmature *pg_deg_rig_for_frame(const bGPdata *gpd, int frame)
{
  Depsgraph *g = registered_graph(gpd);
  if (!g) return pg_arm_for_gpd(gpd);
  if (!g->orig) return nullptr;
  if (frame == g->scene_frame) return g->cow ? g->cow : g->orig;
  return frame_graph(g, frame)->cow;
}

void *pg_deg_gpencil_for_frame(const bGPdata *gpd, int frame)
{
  Depsgraph *g = registered_graph(gpd);
  if (!g || !g->orig || !g->gp_cow_cb.copy || frame == g->scene_frame) return nullptr;
  return frame_graph(g, frame)->gp_cow;
}

void pg_deg_set_gpencil_copy(Depsgraph *g, const PGDegGPencilCopy *cow)
{
  if (!g) return;
  free_frames(g);
  g->gp_cow_cb = cow ? *cow : PGDegGPencilCopy{};
}

void pg_deg_free_frames(Depsgraph *g)
{
  if (g) free_frames(g);
}

bool pg_deg_is_active(const Depsgraph *g) { return g && g->is_active; }
float pg_deg_get_ctime(const Depsgraph *g) { return g ? g->ctime : 0.0f; }
ID *pg_deg_get_original_id(ID *id)
{
  if (id == nullptr || id->orig_id == nullptr) return id;
  return id->orig_id;
}
const PGArmExternal *pg_deg_external(const Depsgraph *g) { return g && g->has_ext ? &g->ext : nullptr; }
bool pg_deg_writes_external(const Depsgraph *g) { return g && (g->is_active || g->gp_cow); }

int pg_deg_debug_last_order(const Depsgraph *g, char *buf, int cap)
{
  if (!g) return 0;
  if (buf && cap > 0) BLI_strncpy(buf, g->last_order.c_str(), (size_t)cap);
  return (int)g->last_order.size();
}

int pg_deg_debug_cycles(const Depsgraph *g) { return g ? g->cycles : 0; }

/* The BKE_pose_where_is of the edit operations: the pose-only graph of `ob`, evaluated on it. */
void pg_bke_pose_where_is(Object *ob)
{
  if (!ob || !ob->pose) return;
  Depsgraph g;
  g.inplace = ob;
  ensure_built(&g);
  id_tag_apply(&g, PG_DEG_ID_RIG, ID_RECALC_ALL);
  deg_graph_flush_updates(&g);
  evaluate_graph(&g);
  graph_clear(&g);
}

/* pg_arm_evaluate_ex: a one-shot active graph of the rig at `frame`. */
void pg_deg_rig_evaluate(PGArmature *a, float frame, const PGArmExternal *ext)
{
  if (!a) return;
  Depsgraph *g = pg_deg_new(true);
  if (ext) {
    g->ext = *ext;
    g->has_ext = true;
  }
  pg_deg_set_rig(g, a, false);
  pg_deg_evaluate_on_framechange(g, frame);
  pg_deg_free(g);
}

}  /* extern "C" */
