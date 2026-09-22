# Project Grease — Blender engine wiring

This wiring keeps Blender's real Android NativeActivity/Vulkan/GHOST surface as the
drawing surface. The MotionCanvas-style Project Grease UI is a transparent Android
activity above it.

## Runtime path

Project Grease UI
→ Android overlay activity
→ center touch forwarding JNI
→ GHOST_SystemAndroid
→ Blender window manager
→ Grease Pencil draw operators/input
→ Blender Draw Manager
→ Blender GPU/Vulkan
→ Android native surface

The UI never draws a replacement artboard.

## Why this avoids the previous canvas failure

The center of the UI overlay is transparent. It does not create a Compose/HTML
canvas, white placeholder, custom stroke renderer, or second rendering surface.

Blender continues to own the native rendering surface. Android's SurfaceView/native
surface model is designed for an external renderer while regular UI can be composited
above it. The Android Blender port already uses NativeActivity + ANativeWindow +
Vulkan for its real window. citeturn1search0turn2search0

The overlay forwards center touch coordinates and pressure to the Blender GHOST
input queue rather than trying to redraw strokes itself. Android identifies stylus
and eraser tool types separately, and the bridge preserves those values. citeturn12search1

## Current scope

Implemented in the wiring layer:
- MotionCanvas-style Project Grease UI
- Home/New Project surface
- left tools
- layers/properties panel
- timeline
- hide/show panel state
- 2D/3D viewport state
- center touch forwarding
- stylus pressure forwarding
- stylus eraser tool type forwarding
- transparent center with no fake canvas

Not yet claimed as complete:
- every toolbar button mapped to a Blender operator
- multi-pointer gesture forwarding from the overlay
- save/load/project model synchronization
- APK build verification on a physical device

## Build

From the repository root:

    ./android/apply_project_grease_wiring.sh
    ./android/build_project_grease_lite.sh

The scripts patch the pinned Blender-for-Android source in the submodule's working
tree before building. No changes are made to the repository's main branch.
