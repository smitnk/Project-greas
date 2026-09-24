# Blender GP source import manifest

Pinned source: Blender v3.6.23
Commit: e467db79ca8cc5c1c15e1a0e08bd52ca419f2eca

## Required source families identified by investigation

### Grease Pencil data
- source/blender/blenkernel/BKE_gpencil_legacy.h
- source/blender/blenkernel/intern/gpencil*.c
- source/blender/makesdna/DNA_gpencil_legacy_types.h

### Grease Pencil draw engine
- source/blender/draw/engines/gpencil/
- source/blender/draw/engines/gpencil/gpencil_engine.c
- source/blender/draw/engines/gpencil/gpencil_cache_utils.c
- source/blender/draw/engines/gpencil/gpencil_draw_data.c
- source/blender/draw/engines/gpencil/gpencil_render.c
- source/blender/draw/engines/gpencil/gpencil_shader.c
- source/blender/draw/engines/gpencil/gpencil_shader_fx.c

### GP draw cache / GPU geometry
- source/blender/draw/intern/draw_cache_impl_gpencil.cc

### GP editor sources
- source/blender/editors/gpencil_legacy/

These are **not automatically required** for the first minimal target. The editor/operator/RNA/UI path will be excluded until the one-stroke render proves that direct GP data injection is insufficient.

### GPU/DRW dependencies
The investigated renderer requires Blender DRW/GPU infrastructure, including GPU textures, uniform buffers, framebuffers, batches/shaders and draw-manager context.

### Core dependency families
The first link/build iteration will resolve these from compiler/linker evidence rather than copying all Blender:
- BLI / blenlib
- BKE / blenkernel
- DNA / makesdna generated data
- DEG / depsgraph where required
- DRW / draw manager
- GPU
- required runtime allocation/string/math support

## Android boundary

The target Android chain is:

Android Surface
-> EGL
-> GLES
-> Blender GPU OpenGL backend
-> DRW
-> Grease Pencil draw engine

No GL4ES dependency is planned.

## Explicitly excluded

- Blender desktop UI
- Blender Python UI/application
- Eevee/Workbench render engines unless a proven dependency requires them
- full Blender editor stack
- the old third-party Blender Android application project
- Android Canvas as the drawing backend
