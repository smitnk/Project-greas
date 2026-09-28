# Project Grease UI / Engine Feature Mapping

Baseline: `native/blender-gp-phase40-cache` at commit `79543c67e6f7abdc78aedb30c31a5d3726f636d8`.

The FlipaClip screenshots are used only as UX references: canvas-first workflow, compact top actions, collapsible tools/properties/timeline, frame strip, project/settings sheets, and touch-sized controls. No FlipaClip branding or proprietary assets are copied.

## Architecture

Android Compose UI
-> EditorController
-> ToolController / DocumentController / HistoryController / AnimationController / MaterialController / ViewController / SelectionController / ModifierController / SculptController / OnionSkinController
-> Project Grease native JNI/bridge
-> Blender 3.6.23 Legacy Grease Pencil subset
-> bGPdata / bGPDlayer / bGPDframe / bGPDstroke / bGPDspoint
-> GP cache / focused Android presentation
-> Android EGL/GLES

The existing native GP path is preserved. No OpenToonz, GL4ES, full Blender UI, GHOST-on-Android, Python, full 3D viewport, or full Blender application graph was added.

## Feature-to-code mapping

| FEATURE | UI LOCATION | CONTROLLER | EXISTING NATIVE SUPPORT | REQUIRED ENGINE WORK | STATUS |
|---|---|---|---|---|---|
| Freehand | Left tool rail / canvas | ToolController + EditorController | begin_stroke/add_point/end_stroke | None for first path | AVAILABLE |
| Pressure sensitivity | Canvas input / Brush settings | EditorController + MaterialController | bGPDspoint pressure is captured | Stylus-specific validation later | AVAILABLE |
| Eraser | Draw rail | ToolController | None | Eraser stroke/point operation | NOT_IMPLEMENTED |
| Line | Draw rail > Shapes | ToolController | None | Primitive stroke generator | NOT_IMPLEMENTED |
| Rectangle | Draw rail > Shapes | ToolController | None | Primitive generator + close | NOT_IMPLEMENTED |
| Circle | Draw rail > Shapes | ToolController | None | Primitive generator + cyclic stroke | NOT_IMPLEMENTED |
| Arc | Draw rail > Shapes | ToolController | None | Arc geometry generator | NOT_IMPLEMENTED |
| Polyline | Draw rail > Shapes | ToolController | None | Multi-segment primitive input | NOT_IMPLEMENTED |
| Select | Edit tools | SelectionController | select_stroke/get_point groundwork | Hit testing + selection model | NOT_IMPLEMENTED |
| Lasso | Edit tools | SelectionController | None | Lasso hit testing | NOT_IMPLEMENTED |
| Move | Selection properties | SelectionController | translate_stroke | Selection transaction + UI | IN_PROGRESS |
| Rotate | Selection properties | SelectionController | None | Point transform | NOT_IMPLEMENTED |
| Scale | Selection properties | SelectionController | None | Point transform | NOT_IMPLEMENTED |
| Mirror | Selection properties | SelectionController | flip_stroke | Selection transaction + UI | IN_PROGRESS |
| Duplicate | Selection properties | SelectionController | duplicate_stroke | Selection transaction + UI | IN_PROGRESS |
| Delete | Selection properties | SelectionController | delete_stroke/delete_last_stroke | Selection transaction + history | IN_PROGRESS |
| Split | Stroke tools | SelectionController | split_stroke | Selection/hit routing | IN_PROGRESS |
| Subdivide | Stroke tools | SelectionController | subdivide_stroke | Selection/hit routing | IN_PROGRESS |
| Trim | Stroke tools | SelectionController | trim_stroke_points | Selection/hit routing | IN_PROGRESS |
| Close | Stroke tools | SelectionController | close_stroke | Selection/hit routing | IN_PROGRESS |
| Layers | Layer panel | DocumentController | create_layer/select_layer/layer_count | Persistent layer model + UI actions | IN_PROGRESS |
| Layer visibility | Layer row | DocumentController | GP layer exists; visibility control not wired | Native visibility flag + presentation filtering | IN_PROGRESS |
| Layer locking | Layer row | DocumentController | GP layer exists; lock control not wired | Native lock state + input filtering | IN_PROGRESS |
| Layer ordering | Layer panel | DocumentController | ListBase layer order exists | Move/reorder operation + history | IN_PROGRESS |
| Layer duplication | Layer panel | DocumentController | None | Deep layer duplication | IN_PROGRESS |
| Layer deletion | Layer panel | DocumentController | None | Safe layer destruction | IN_PROGRESS |
| Layer rename | Layer panel | DocumentController | Layer name exists | Rename API + persistence | IN_PROGRESS |
| Frames | Timeline | AnimationController | create_frame/select_frame/frame_count | Multi-layer frame model | IN_PROGRESS |
| Timeline | Bottom timeline | AnimationController | Frame data exists | Timeline model and synchronized native selection | IN_PROGRESS |
| Add frame | Timeline | AnimationController | create_frame | UI-to-native wiring | IN_PROGRESS |
| Insert frame | Timeline | AnimationController | None | Frame insertion/shift semantics | IN_PROGRESS |
| Duplicate frame | Timeline | AnimationController | None | Deep frame duplication | IN_PROGRESS |
| Delete frame | Timeline | AnimationController | None | Safe frame deletion | IN_PROGRESS |
| Frame holds | Timeline | AnimationController | Legacy frame numbers provide basis | Hold/evaluation model | IN_PROGRESS |
| Playback | Timeline | AnimationController | None | Clock + frame evaluation + redraw | IN_PROGRESS |
| Pause | Timeline | AnimationController | None | Playback state machine | IN_PROGRESS |
| Loop | Timeline | AnimationController | UI state only | Playback loop bounds | IN_PROGRESS |
| FPS | Timeline / project settings | AnimationController | None | Project timing model | IN_PROGRESS |
| Frame navigation | Timeline | AnimationController | select_frame | Full UI/native synchronization | IN_PROGRESS |
| Keyframe workflow | Timeline | AnimationController | GP frames are the storage primitive | Keyframe commands + UI | IN_PROGRESS |
| Interpolation | Timeline properties | AnimationController | None | Interpolation/evaluation engine | NOT_IMPLEMENTED |
| Onion skin enable | Properties | OnionSkinController | GP onion flags exist in document initialization | Multi-frame presentation | IN_PROGRESS |
| Previous frames | Onion panel | OnionSkinController | gstep/gstep_next groundwork | Multi-frame render/composite | IN_PROGRESS |
| Next frames | Onion panel | OnionSkinController | gstep/gstep_next groundwork | Multi-frame render/composite | IN_PROGRESS |
| Onion opacity | Onion panel | OnionSkinController | onion_factor initialized | Presentation opacity | IN_PROGRESS |
| Onion fade | Onion panel | OnionSkinController | GP fade flag initialized | Presentation implementation | IN_PROGRESS |
| Onion range | Onion panel | OnionSkinController | GP range fields exist | Frame collection/presentation | IN_PROGRESS |
| Onion layer filtering | Onion panel | OnionSkinController | None | Layer-aware frame selection | NOT_IMPLEMENTED |
| Materials | Properties > Materials | MaterialController | material_index stored in stroke style | Material data model | IN_PROGRESS |
| Create material | Materials | MaterialController | None | Material allocation/model | NOT_IMPLEMENTED |
| Delete material | Materials | MaterialController | None | Material lifecycle | NOT_IMPLEMENTED |
| Select material | Materials | MaterialController | Stroke style material_index | Native material selection | IN_PROGRESS |
| Stroke color | Brush/Material properties | MaterialController | Presentation currently uses fixed color | Material-to-presentation mapping | IN_PROGRESS |
| Fill color | Fill properties | MaterialController | None | Fill/material data | NOT_IMPLEMENTED |
| Thickness | Brush properties | MaterialController | bGPDstroke thickness | UI is connected to stroke creation | AVAILABLE |
| Opacity | Brush properties | MaterialController | Layer/point strength groundwork | Full material opacity mapping | IN_PROGRESS |
| Fill enable/disable | Fill properties | MaterialController | None | Fill flag/data + renderer | NOT_IMPLEMENTED |
| Pan | Canvas gestures | ViewController | None | Canvas transform/input routing | IN_PROGRESS |
| Zoom | Top bar / gestures | ViewController | UI state only | Canvas transform matrix | IN_PROGRESS |
| Fit canvas | View menu | ViewController | None | Canvas transform | NOT_IMPLEMENTED |
| Reset view | View menu | ViewController | UI reset exists | Apply transform to renderer/input | IN_PROGRESS |
| Grid | View menu | ViewController | None | Grid overlay | NOT_IMPLEMENTED |
| Guides | View menu | ViewController | None | Guide overlay/data | NOT_IMPLEMENTED |
| Snapping | View menu | ViewController | None | Snap solver | NOT_IMPLEMENTED |
| Brush stabilization | Tool settings | ToolController | None | Stroke resampling/stabilizer | NOT_IMPLEMENTED |
| Brush smoothing | Tool settings | ToolController | None | Stroke filtering | NOT_IMPLEMENTED |
| Brush spacing | Tool settings | ToolController | None | Brush sampling | NOT_IMPLEMENTED |
| Pressure curve | Tool settings | MaterialController | Raw pressure captured | Pressure remapping | NOT_IMPLEMENTED |
| Sculpt | Advanced tools | SculptController | bGPDspoint data can be modified | Sculpt engine | NOT_IMPLEMENTED |
| Grab | Sculpt | SculptController | Point access exists | Falloff/deformation | NOT_IMPLEMENTED |
| Smooth | Sculpt | SculptController | Point access exists | Smoothing solver | NOT_IMPLEMENTED |
| Push | Sculpt | SculptController | Point access exists | Directional deformation | NOT_IMPLEMENTED |
| Pinch | Sculpt | SculptController | Point access exists | Radial deformation | NOT_IMPLEMENTED |
| Randomize/deform | Sculpt | SculptController | Point access exists | Controlled displacement | NOT_IMPLEMENTED |
| Advanced fill | Fill tool | ToolController | None | Boundary/fill engine | NOT_IMPLEMENTED |
| Gap tolerance | Fill properties | ToolController | None | Gap detection | NOT_IMPLEMENTED |
| Fill expansion | Fill properties | ToolController | None | Boundary expansion | NOT_IMPLEMENTED |
| Boundary handling | Fill properties | ToolController | None | Closed-region solver | NOT_IMPLEMENTED |
| Stroke textures | Material properties | MaterialController | None | Texture data + presentation | NOT_IMPLEMENTED |
| Fill textures | Material properties | MaterialController | None | Texture data + presentation | NOT_IMPLEMENTED |
| Texture settings | Material properties | MaterialController | None | Texture resource model | NOT_IMPLEMENTED |
| Texture scale | Material properties | MaterialController | None | UV/texture mapping | NOT_IMPLEMENTED |
| Texture opacity | Material properties | MaterialController | None | Texture alpha/presentation | NOT_IMPLEMENTED |
| Stroke effects | Effects panel | ModifierController | None | Effect modules | NOT_IMPLEMENTED |
| Fill effects | Effects panel | ModifierController | None | Effect modules | NOT_IMPLEMENTED |
| Modifier stack | Properties > Modifiers | ModifierController | None | Modifier evaluation pipeline | NOT_IMPLEMENTED |
| Modifier enable/disable | Modifier stack | ModifierController | None | Evaluation gating | NOT_IMPLEMENTED |
| Modifier ordering | Modifier stack | ModifierController | None | Ordered evaluation | NOT_IMPLEMENTED |
| Noise | Modifier stack | ModifierController | None | Point displacement modifier | NOT_IMPLEMENTED |
| Dash | Modifier stack | ModifierController | None | Stroke segmentation | NOT_IMPLEMENTED |
| Outline | Modifier stack | ModifierController | None | Expanded outline geometry | NOT_IMPLEMENTED |
| Thickness modifier | Modifier stack | ModifierController | None | Width evaluation | NOT_IMPLEMENTED |
| Color modifier | Modifier stack | ModifierController | None | Material/color evaluation | NOT_IMPLEMENTED |
| Deform modifier | Modifier stack | ModifierController | None | Point deformation | NOT_IMPLEMENTED |
| Generate modifier | Modifier stack | ModifierController | None | Geometry generation | NOT_IMPLEMENTED |
| Advanced interpolation | Timeline properties | AnimationController | None | Interpolation engine | NOT_IMPLEMENTED |
| Multiframe editing | Timeline | AnimationController | Multiple GP frames exist | Multi-frame selection/edit transactions | NOT_IMPLEMENTED |
| Advanced onion skin | Onion panel | OnionSkinController | Basic GP onion fields | Multi-layer compositing modes | NOT_IMPLEMENTED |
| Visual effects | Effects/presentation | ModifierController | GLES presentation boundary exists | Effect/render modules | NOT_IMPLEMENTED |
| New Project | Project menu | DocumentController | Native document creation | Persistent project model | IN_PROGRESS |
| Open Project | Project menu | DocumentController | None | File format + load | NOT_IMPLEMENTED |
| Save | Project menu | DocumentController | None | File serialization | NOT_IMPLEMENTED |
| Save As | Project menu | DocumentController | None | File picker + serialization | NOT_IMPLEMENTED |
| Export | Project menu | DocumentController | None | PNG/GIF/video export pipeline | NOT_IMPLEMENTED |
| Project Settings | Project menu | DocumentController | UI boundary only | Persistent project settings | IN_PROGRESS |

## Native operations verified in the inspected baseline

The backend exposes real Legacy GP operations for document/layer/frame/stroke/point lifecycle and editing, including:

- create_document
- create_layer
- select_layer
- layer_count
- create_frame
- select_frame
- frame_count
- stroke_count
- point_count
- select_stroke
- get_point
- set_point
- delete_stroke
- delete_last_stroke
- duplicate_stroke
- translate_stroke
- flip_stroke
- subdivide_stroke
- close_stroke
- trim_stroke_points
- split_stroke
- begin_stroke
- add_point
- end_stroke
- initialize_external_gpu_context
- render_external_context

These are not duplicated in the new UI. They remain behind the native adapter boundary.

## Current implementation boundary

The new UI/controller layer does not claim that every visible tool works. Only freehand drawing and pressure capture are currently end-to-end available through the Android input -> EditorController -> JNI -> Legacy GP stroke path.

The remaining features are deliberately represented as IN_PROGRESS or NOT_IMPLEMENTED in one registry so that future engines can activate existing UI controls without a second UI architecture.
