# Final Legacy GP Engine Acceptance Matrix

This document is the executable review matrix for completing the focused Blender 3.6.23 Legacy GP engine.

## Core drawing
- Real Legacy GP sbuffer input lifecycle.
- Pressure, strength, time and point state.
- Brush settings and stroke processing from Blender 3.6.23 source.
- Live preview and final commit use one authoritative GP representation.
- Drawing is stable and does not fragment/cut points.

## Primitive drawing
- Line, polyline, arc, curve, box and circle use the pinned Legacy GP operator algorithms.
- Modal preview/finalization semantics match the upstream behavior that can be retained in the focused context.

## Editing
- Select, lasso/circle/linked selection.
- Move, rotate, scale, mirror, duplicate, delete.
- Split, join/separate, close, subdivide, simplify, smooth, trim, dissolve, stretch, reproject where the 3.6.23 Legacy GP implementation permits.
- Selection is point/stroke aware and preserves layer/frame semantics.

## Erasing/fill
- Point/stroke erasing uses Blender Legacy GP algorithms where available.
- Fill and gap-closing use Blender Legacy GP implementation and triangulation.
- No Project Grease raster replacement remains on the production path where the upstream implementation can be extracted.

## Animation
- Layers, ordering, visibility, lock, opacity and frame lifecycle.
- Keyframe/blank/duplicate frame behavior.
- Playback, looping, interpolation and onion-skin behavior.
- Multiframe editing where the pinned Legacy GP editor path supports it.

## Paint/sculpt/materials
- Legacy GP material/style state.
- Stroke/fill color, thickness, opacity and pressure response.
- Paint and sculpt operations backed by the actual 3.6.23 Legacy GP code closure rather than custom approximations.

## Modifiers and advanced GP
- Each exposed modifier is backed by its real 3.6.23 Legacy GP modifier implementation or explicitly documented focused adapter.
- Line Art, curve/SVG/PDF and rigging are integrated only when their required dependency closure is proven.
- No desktop-only depsgraph/scene behavior is silently simulated as complete.

## Android boundary
- Android owns Activity, input, lifecycle, Surface, EGL and GLES.
- JNI methods are complete and symbol-verified.
- Focused native closure builds through Android NDK/CMake.
- No full Blender Android executable, GHOST, Python or unrelated Blender application graph is introduced.

## Verification gates
A feature can be marked COMPLETE only after:
1. Source trace to Blender 3.6.23.
2. Native regression/conformance test.
3. Android build and APK verification.
4. Runtime/device validation where required.
5. Independent review finds no false-complete exposure.

Build success alone never marks a feature complete.
