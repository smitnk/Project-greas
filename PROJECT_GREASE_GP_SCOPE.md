# Project Grease — GP Engine Scope

## Non-negotiable architecture rule

Project Grease is **not** a full Blender Android port.

The project uses a **minimal subset of Blender's 2D Legacy Grease Pencil drawing/cache engine** as the native drawing backend for the Project Grease Android editor.

## Included

- Real Legacy Grease Pencil data: bGPdata, bGPDlayer, bGPDframe, bGPDstroke and bGPDspoint.
- Minimum GP geometry/cache code required to create and update real GP stroke batches.
- Minimum GPU buffer/batch closure required by that GP cache.
- Project Grease Android GLES buffer/batch backend.
- Android-owned EGL/GLES context.
- Project Grease JNI bridge and stroke API.
- Minimal generated Blender DNA metadata required by the selected GP structures.

## Excluded from Android

- Blender desktop editor/UI
- Blender Main/ID database ownership
- Blender Python
- Blender GHOST on Android
- Blender window manager
- Blender scene system
- Blender 3D viewport
- Blender modifiers, physics and full rendering pipeline
- Full Blender application executable
- Full Blender CMake build graph

Android owns the Activity, Surface, lifecycle, input, EGL context and GLES context.

## Desktop CI rule

The desktop Native Blender GP workflow is only a **host-side proof harness** for the minimal GP/cache closure. Its GHOST/GPU context is test infrastructure and must not become an Android dependency.

## Dependency rule

When a build fails, add only the smallest source/implementation needed by the GP drawing path and verify that the dependency is actually referenced. Do not solve a GP dependency failure by importing the full Blender application.

## Production rendering path

Android touch/stylus
-> Project Grease JNI
-> real Blender Legacy GP stroke data
-> Blender GP draw/cache code
-> Project Grease Android GPU buffer/batch backend
-> Android GLES
-> Project Grease canvas

The target is a working **2D drawing engine inside Project Grease**, not Blender running on Android.
