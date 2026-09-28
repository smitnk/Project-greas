# Blender / Grease Pencil import manifest

Project Grease uses Blender 3.6.23 Legacy Grease Pencil as a focused native 2D drawing/data foundation.

## Selected baseline

- Blender v3.6.23
- Pinned commit: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca
- Legacy GP data: bGPdata / bGPDlayer / bGPDframe / bGPDstroke / bGPDspoint
- Android owns Activity, Surface, input, EGL and GLES.

## Selected source families

### Legacy Grease Pencil data
- source/blender/blenkernel/BKE_gpencil_legacy.h
- source/blender/blenkernel/intern/gpencil*.c
- source/blender/makesdna/DNA_gpencil_legacy_types.h

### GP cache / draw closure
- source/blender/draw/intern/draw_cache_impl_gpencil.cc
- only the draw/cache/GPU support proven necessary by the Project Grease native CI

### GPU closure
- focused GPU vertex/index/batch infrastructure
- Project Grease Android GPU buffer/presentation boundary

## Excluded

- Blender desktop UI/editors
- Blender Python
- GHOST on Android
- Blender window manager
- full Blender scene/Main/ID application architecture on Android
- full 3D viewport
- unrelated render engines
- full Blender CMake dependency graph
- Android Canvas as the native drawing backend
- OpenToonz and GL4ES

## Dependency rule

A new Blender source dependency is admitted only when compiler/linker/runtime evidence shows that it is required by the selected Legacy GP drawing path. Do not import a large Blender subsystem because it happens to contain a similar feature.

## Runtime boundary

Android input
-> Project Grease controllers
-> Project Grease JNI/native adapter
-> Legacy GP data
-> focused GP cache/geometry
-> Project Grease Android GLES presentation

The target remains a 2D animation/drawing application, not Blender running on Android.
