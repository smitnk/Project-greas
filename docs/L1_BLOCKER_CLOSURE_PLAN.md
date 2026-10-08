# L1 blocker closure plan — Armature + Shrinkwrap

Reference: Blender 3.6.23 commit e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca.

## Track A — Legacy GP Armature

Authoritative modifier:
- source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_armature.c

The modifier's actual stroke path is:
1. copy Legacy GP point coordinates
2. BKE_armature_deform_coords_with_gpencil_stroke()
3. write deformed coordinates back
4. BKE_gpencil_stroke_geometry_update()

The deformation function is declared in:
- source/blender/blenkernel/BKE_armature.h

Its implementation is in:
- source/blender/blenkernel/intern/armature_deform.c

Required semantics that must not be replaced with custom bone math:
- armature pose channel evaluation
- vertex-group binding by bone name
- bone-envelope binding
- invert vertex-group behavior
- quaternion/preserve-volume deformation
- armature/object transform conversion
- GP stroke deform-vertex weights
- exclusion of non-deforming bones

The current Project Grease stack has no Armature entry and no armature/pose target runtime. Therefore this cannot be completed by adding a switch alone.

## Track B — Legacy GP Shrinkwrap

Authoritative modifier:
- source/blender/gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_shrinkwrap.c

Core dependency:
- source/blender/blenkernel/intern/shrinkwrap.c
- source/blender/blenkernel/BKE_shrinkwrap.h

Required semantics:
- evaluated target mesh, not base mesh
- nearest surface
- project with positive/negative direction and face culling
- nearest vertex
- target-normal projection
- auxiliary target
- snap modes
- offset
- smoothing/repeat
- influence filters

The project decision is D2 = full evaluated-mesh parity. A base-mesh-only shortcut is forbidden.

The current Project Grease stack has no Shrinkwrap entry and no evaluated mesh/object dependency path. This is a dependency-closure task, not a small modifier implementation.

## Track C — ABI parity

Current CI verifies Android native link closure for:
- arm64-v8a
- x86_64

It does not yet run the same behavioral parity corpus through both Android ABI targets against the pinned Blender 3.6.23 reference. L1 remains open until that gate exists.

## Implementation rule

Do not:
- fake Armature deformation
- implement simplified Shrinkwrap against base mesh
- remove the two modifiers from the Blender feature list
- call L1 complete from a green compile alone

The next implementation commits must first add the minimal real Blender source closure required by each track, then add parity tests before the L1 completion gate is changed.
