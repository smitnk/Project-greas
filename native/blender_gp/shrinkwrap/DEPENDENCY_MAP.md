# Shrinkwrap L1 dependency map — Blender 3.6.23

Pinned source: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca

## Authoritative Legacy GP entry
- source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_shrinkwrap.c
- source/blender/makesdna/DNA_gpencil_modifier_types.h (ShrinkwrapGpencilModifierData)

## Core
- source/blender/blenkernel/intern/shrinkwrap.cc
- source/blender/blenkernel/BKE_shrinkwrap.h

## Required target-query/BVH closure
- BKE_bvhtree_from_mesh_get()
- BKE_bvhutils.h / bvhutils.cc
- BLI_bvhtree.h
- BLI_bvhtree implementation
- BLI_kdopbvh.c
- mesh loop-triangle access
- evaluated mesh runtime/cache access

## Required mesh/evaluation closure for D2
- evaluated target Object -> evaluated Mesh
- evaluated Mesh modifier stack / Geometry Nodes result
- evaluated vertex positions
- evaluated polygon/corner topology
- evaluated vertex normals
- evaluated polygon normals when required
- custom loop normals when required
- shrinkwrap boundary runtime data for target-project mode

## Required math/runtime closure
- BLI_space_transform
- vector/matrix geometry used by shrinkwrap.cc
- BKE_deform vertex-group weighting
- task/range execution using the existing serial Android-compatible task configuration
- guarded allocation

## Explicit exclusions
- No Armature dependency closure.
- No base-mesh shortcut for D2.
- No desktop Depsgraph/Main/GHOST/UI import merely to satisfy the source.
- No custom shrinkwrap algorithm replacing Blender's 3.6.23 core.

## Port order
1. Port/close BVH tree construction/query closure.
2. Port the evaluated-mesh provider needed by D2.
3. Port mesh normals/looptri data required by shrinkwrap.
4. Adapt the Legacy GP entry to Project Grease's modifier-stack context.
5. Add Blender 3.6.23 reference fixtures.
6. Verify nearest-vertex, nearest-surface, normal projection, culling, auxiliary target, offset/influence and smoothing.
7. Verify the evaluated target differs from base mesh in a modifier-applied fixture.
8. Run ARM64/x86_64 native tests and only then wire the result into the Android modifier stack.
