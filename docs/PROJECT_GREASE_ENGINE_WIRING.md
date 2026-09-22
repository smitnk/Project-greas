# Project Grease — Blender engine wiring

This branch wires the MotionCanvas-style Project Grease UI above Blender's real Android NativeActivity/Vulkan/GHOST rendering surface.

## Runtime path

Project Grease UI
→ transparent Android overlay activity
→ center touch forwarding JNI
→ GHOST_SystemAndroid
→ Blender window manager / Grease Pencil
→ Blender Draw Manager
→ Blender GPU abstraction
→ Vulkan
→ Android native surface

The UI does not create a replacement artboard or stroke renderer.

## Drawing-surface rule

The center of the Project Grease UI is transparent.

Blender owns the real native rendering surface. The overlay only draws:
- top controls
- left tools
- right layers/properties
- bottom timeline
- hide/show controls
- home/new-project screen

This is specifically to avoid the previous failure mode where a UI canvas/placeholder covered the actual engine viewport.

## Input rule

Touches in the UI regions are consumed by the Project Grease overlay.

Touches in the transparent center are forwarded through JNI into GHOST_SystemAndroid. The bridge preserves:
- X/Y coordinates
- action DOWN/MOVE/UP/CANCEL
- stylus pressure
- Android stylus tool type
- Android eraser tool type

The native bridge queues the events and dispatches them on Blender's GHOST thread.

## Grease Pencil startup

The wiring also installs project_grease_startup.py into Blender's Android APK assets. On first launch it is copied to the app's private files directory and passed to Blender with --python.

The startup script:
1. looks for an existing Grease Pencil object;
2. creates a real Grease Pencil object if none exists;
3. makes it active;
4. enters Blender Grease Pencil Draw Mode.

Therefore the center is intended to open on an actual Grease Pencil scene rather than an empty fake canvas.

## UI

The UI follows the uploaded MotionCanvas reference:
- Brush
- Eraser
- Select
- Lasso
- Fill
- Text
- Eyedropper
- Pan
- Arrow
- Shape
- Undo/Redo
- Brush/Color areas
- Layers
- Materials
- Reference
- Advanced
- Audio
- Timeline
- FPS
- Onion Skin
- 2D / 3D viewport mode
- Home/New Project

Panel visibility has explicit controls:
- T — Tools
- L — Layers/properties
- TL — Timeline

## Current scope

Implemented in the wiring layer:
- real Blender NativeActivity remains the renderer owner
- transparent Project Grease overlay
- real center touch → GHOST bridge
- stylus pressure/tool information forwarding
- real Grease Pencil startup object/mode
- MotionCanvas-style UI
- panel hide/show
- scrollable tools/properties/timeline behavior

Not yet claimed as complete:
- every UI button mapped to a corresponding Blender operator
- multi-pointer overlay forwarding
- full Project Grease timeline/layer state synchronization with Blender data
- save/load UI integration
- physical-device APK smoke test

## Build

From the repository root:

    ./android/apply_project_grease_wiring.sh
    ./android/build_project_grease_lite.sh

The scripts modify the checked-out Blender Android submodule working tree before building. The repository's main branch is not modified.