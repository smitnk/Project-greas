# Project Grease — L1 completion audit (2026-10-08)

Audited commit: 3438561ce0f4d55e0d1fd3430fe99ed6fdcce948
Blender reference: 3.6.23, pinned commit e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca.

## CI state

Run #862 (37729962419) is green. It passed the full Android Shell workflow, including:
- native primitive, selection, eraser, edit, annotation, scene-lite, document-state, outline, color, RNG tests
- Blender DNA generation
- segment/material/onion tests
- fill tests
- draw/sculpt/vertex-paint/weight-paint tool-session tests
- Line Art reference generation and native comparison
- GLES render tests
- modifier-stack and mod2 tests
- shader-effect tests
- Android unit tests
- Android APK build and verification
- ARM64 and x86_64 native link-closure probes

A green build is not by itself the L1 completion gate.

## L1 implementation matrix

### Drawing / core GP data
- Real Blender 3.6.23 Legacy GP data structures: IMPLEMENTED.
- Freehand stroke allocation and Android input: IMPLEMENTED.
- Pressure capture: IMPLEMENTED.
- Stroke geometry/update path: IMPLEMENTED.
- Primitive generation: IMPLEMENTED and CI tested.
- Fill triangulation/boundary fill: IMPLEMENTED and CI tested.
- Eraser: IMPLEMENTED and CI tested, but editor-level parity is still not closed.
- Selection: IMPLEMENTED and CI tested.
- Move/Rotate/Scale/Mirror: IMPLEMENTED with Legacy GP point conversion; full desktop transform operator/context stack is intentionally not imported.
- Delete/Dissolve/Split/Join/Reverse/Reorder: IMPLEMENTED in the native edit adapter.
- Undo/Redo: IMPLEMENTED using real Legacy GP data snapshots.
- Layers/visibility/locking/order/duplication: IMPLEMENTED at backend/controller level.
- Frames/keyframe editing: IMPLEMENTED at backend/controller level.
- Materials/stroke/fill colors: IMPLEMENTED.
- Vertex groups / point weights: IMPLEMENTED.
- Layer masks: IMPLEMENTED.
- Grid snapping: IMPLEMENTED in the edit adapter.

### Sculpt / paint
- Legacy GP sculpt brush algorithms: IMPLEMENTED through project_grease_tool_sculpt.c.
- Sculpt session bridge: WIRED to the Blender-aligned brush session path.
- Smooth/Thickness/Strength/Grab/Push/Twist/Pinch/Randomize/Clone: source paths exist.
- Vertex paint and weight paint sessions: IMPLEMENTED and CI tested.
- Device-level runtime parity for every brush is NOT yet proven.

### Modifier engine
The current live stack enumerates 23 of the 25 deform/generate/color/modify modifier entries represented by the project adapter:
Thickness, Opacity, Tint, Hue/Saturation, Length, Smooth, Simplify, Subdivide, Offset, Noise, Build, Time, Hook, Lattice, Envelope, Vertex Weight Proximity, Vertex Weight Angle, Dot Dash, Outline, Mirror, Array, Multiple Strokes, Texture Mapping.

The missing Legacy GP modifiers are:
1. Armature
2. Shrinkwrap

These are NOT safe to replace with custom approximations.

Armature requires real Blender armature/pose deformation, vertex-group/envelope semantics and transform handling.
Shrinkwrap requires the real Blender shrinkwrap core plus an evaluated target mesh; the project decision is full evaluated-mesh parity, including the Geometry Nodes/evaluated-mesh path where applicable.

Therefore the modifier subsystem is NOT L1-complete.

### Line Art
- CPU Line Art path: IMPLEMENTED.
- Scene-lite reference comparison: PASS in CI.
- Full mesh-scene/depsgraph parity: NOT closed.
- Full production Line Art dependency closure is therefore NOT an L1 completion item yet.

### Animation/editor integration
- Basic frame/layer data APIs exist.
- Interpolation adapter exists but is explicitly not advertised complete; matched stroke topology is still required.
- Timeline/controller integration is not fully end-to-end.
- Full playback/hold behavior is not closed.

### Persistence / export
- Full persistence/export is NOT implemented. This belongs to the later locked persistence/export phase, so it is not a reason to block the dependency L1 by itself.

## L1 gate result

**L1 = NOT COMPLETE. Do not start L2 as a completed phase.**

Hard blockers:
1. Armature modifier.
2. Shrinkwrap modifier with full evaluated-mesh target path.
3. L1 ABI parity gate is incomplete: ARM64 and x86_64 link closure now pass, but the workflow does not yet execute the same behavioral parity corpus on both Android ABIs against the pinned Blender 3.6.23 reference.
4. Device-level runtime validation is still absent.

## Next implementation tracks

Use three bounded engineering tracks:
1. Armature — map and port only the Blender 3.6.23 Legacy GP Armature deform closure; no fake bone math.
2. Shrinkwrap — map and port the real Blender shrinkwrap/evaluated-mesh closure required by the locked D2 decision; no base-mesh shortcut.
3. ABI parity — add a reproducible ARM64/x86_64 parity corpus and gate L1 completion on both ABIs.

Do not remove the missing modifiers from the type model just to make the checklist green. Do not claim L1 complete until the blockers and the parity gate are verified.
