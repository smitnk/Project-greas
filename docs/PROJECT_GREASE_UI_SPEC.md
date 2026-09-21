# Project Grease UI

This UI adapts the MotionCanvas UI reference to the imported Blender-for-Android stack.

## UI/engine boundary

The Compose UI is only the control surface. It does not implement a replacement stroke renderer.

The intended runtime path is:

Android touch/stylus
→ Project Grease input bridge
→ Blender Grease Pencil
→ Blender Draw Manager
→ Blender GPU abstraction
→ Android Vulkan surface

The Blender 3D Viewport remains available as the viewport infrastructure.

## MotionCanvas features retained

- Brush/draw tool
- Eraser
- Select
- Lasso
- Fill
- Eyedropper
- Shapes
- Pan
- Undo/redo
- Color and brush controls
- Layers
- Timeline
- Frame insertion/duplication/deletion
- Playback and loop
- FPS
- Onion-skin control
- Audio controls
- Advanced/pro tools

## Blender/Grease Pencil adaptations

- Draw tool is routed to Grease Pencil rather than DrawStroke.
- Stroke width/pressure are treated as Grease Pencil drawing properties.
- Layer panel represents Grease Pencil layers.
- Timeline represents Grease Pencil drawing frames.
- Material panel represents Grease Pencil materials.
- Viewport controls expose 3D Viewport navigation and orthographic/perspective state.
- Rendering is delegated to Blender Draw Manager/GPU; the UI must not paint a second custom canvas over the Blender viewport.

## UI name

Product name: Project Grease
Package/UI labels must use Project Grease rather than MotionCanvas.
