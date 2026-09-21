# Blender / Grease Pencil import manifest

Reference source: Wanderson-Magalhaes/blender_for_android

## Initial source groups to study/import

### Android port/build
- ANDROID_AI_GUIDE.md
- build_files/android/
- Android-specific source changes under source/ identified by the port guide

### Grease Pencil data/runtime
- source/blender/blenkernel/ — Grease Pencil data/runtime implementation and public BKE headers
- source/blender/geometry/ — geometry/curves support used by drawings
- source/blender/makesdna/ — DNA types required by Blender data structures
- source/blender/makesrna/ — only where generated/runtime RNA is actually required
- source/blender/blenlib/ — required utility/math/container support

### Grease Pencil drawing/rendering
- source/blender/draw/engines/gpencil/
- relevant source/blender/draw/ shared drawing infrastructure
- source/blender/gpu/ and Vulkan backend used by the Android port
- Grease Pencil shader sources under the relevant draw/GPU shader directories

### Input/editing — later, not part of the first native proof
- source/blender/editors/grease_pencil/
- relevant source/blender/editors/sculpt_paint/ Grease Pencil draw/erase code
- transform support for Grease Pencil

## Deliberately excluded from the first import

- Blender desktop UI/editors unrelated to drawing
- Outliner, Properties, 3D View UI
- unrelated render engines
- Cycles, simulation, compositor, video editor, etc.
- legacy Grease Pencil unless a dependency audit proves it is required

## Important

The exact file-level dependency graph must be generated from the selected Blender revision. Directory names alone are not sufficient because Grease Pencil code includes many shared Blender libraries.

The first runtime milestone is: Android Surface -> native input -> one Grease Pencil drawing/stroke -> visible rendering.
