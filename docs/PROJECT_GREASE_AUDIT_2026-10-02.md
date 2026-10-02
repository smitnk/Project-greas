# Project Grease audit, 2026-10-02

Base: `feature/project-grease-ui-real-integration` at `9dc26d8` (merge of PR #15).
Evidence level used below:

- **Source** = traced to Blender 3.6.23 (pinned `e467db79`).
- **Native tests** = host tests in `native/blender_gp/tests` (CI step per suite).
- **Android tests** = JUnit in `android/app/src/test`.
- **Build/APK** = CI workflow `android-shell` green at the merge (run for PR #15; also #706, #708).
  CI builds the app, runs the native link probe and verifies the APK. It does not run the app.
- **Device** = NOT VERIFIED for every feature. No device evidence exists in the repository, so by the
  acceptance rule in `LEGACY_GP_ENGINE_ACCEPTANCE.md` **nothing is COMPLETE**.

There is no `STATUS.md`; status lives in `FeatureRegistry.kt`, the acceptance matrix, and this file.

## Defects found by this audit

| # | Defect (verified in source) | Effect | Action |
|---|---|---|---|
| 1 | `FeatureRegistry` had no entry for LINE, RECTANGLE, CIRCLE, ARC, POLYLINE, FILL, so they defaulted to NOT_IMPLEMENTED; the tool rail greys them out and `ToolController.select` refuses them | Finished shape and fill code unreachable from the UI | Fixed: explicit entry for all 129 features + test that every `GreaseTool` is selectable |
| 2 | Registry marked 40+ features AVAILABLE with only native-test evidence | False completeness | Fixed: nothing is AVAILABLE without device evidence; each feature states its limitation |
| 3 | Materials sheet Opacity slider updated a Kotlin field and never called `setMaterialColor` | Slider did not reach the engine | Fixed |
| 4 | Moving the stabilizer factor slider called `setStabilizer(true, it)` | Slider silently enabled stabilization | Fixed |
| 5 | `setSpacing(0)` left the Euclidean filter at the last non-zero value | Spacing could not be turned back off | Fixed |
| 6 | `saveDocumentJson`/`loadDocumentJson` stored only layers, frames, strokes and points | On load every stroke got material 0 and the brush thickness; cyclic flags, layer names, hidden/locked state and material colors were lost | **Fixed** (format v2): native getters + JNI for stroke material/thickness/cyclic/fill, layer name/visibility/lock/opacity and the material palette; `add_stroke` restores strokes without paint smoothing. Tests: `run_native_document_state_tests.sh`, `ProjectDocumentRoundTripTest` |
| 7 | Project sheet: "Open project" only closes the sheet, "Save as" = Save, "Export" shows a toast | No such features | Registry says NOT_IMPLEMENTED |
| 8 | Layers sheet switches (Visible/Locked) started at fixed values and were not read from the native layer | Switch could disagree with the layer | **Fixed**: the sheet reads name/visibility/lock from the native layer and re-reads when the selected layer or layer count changes |
| 9 | Transform menu items still call per-stroke operations; selection-wide delete/pick modes have no buttons | Features exist in code but are not reachable | **Open** (UI work) |
| 10 | Strength slider also multiplies the live-preview color alpha | Possible double application in the preview only; final strokes use per-point strength | **Open**, needs the renderer source |

## Feature table

Columns: Blender source | Project Grease path | tests | status | limitation.
All rows: Build/APK = CI green; Device = NOT VERIFIED.

### Drawing

| Feature | Blender 3.6.23 source | Implementation | Tests | Status | Limitation |
|---|---|---|---|---|---|
| Freehand | gpencil_paint.c: addpoint, filtermval, newfrombuffer | `LegacyGpBrushStrokeEngine.kt` -> native sbuffer -> `BKE_gpencil_stroke_add` | Kotlin engine tests | IN_PROGRESS | newest 4 points held back (live line lags a few points) |
| Pressure | gpencil_draw_apply_event | `TouchInputRules` (pen only; finger = 1.0) | Kotlin | IN_PROGRESS | no tilt, no stylus eraser end |
| Pressure curve | CurveMapping `curve_sensitivity` | power curve | Kotlin | IN_PROGRESS | approximation |
| Strength | addpoint strength path | per-point `drawStrength` | Kotlin | IN_PROGRESS | see defect 10 |
| Stabilization | filtermval lazy branch, draw_apply | engine lazy mode | Kotlin | IN_PROGRESS | radius in canvas units |
| Smoothing | gpencil_smooth_buffer, smooth_segment | engine, off by default | Kotlin | IN_PROGRESS | Blender preset value not traced |
| Spacing | MIN_EUCLIDEAN_PX | Euclidean filter | Kotlin | IN_PROGRESS | not a true spacing |
| Jitter, brush angle | gpencil_brush_jitter/angle(_segment) | engine | Kotlin | IN_PROGRESS | no UI; RNG differs |
| Fake/arc points | gpencil_add_fake_points/arc_points | engine (replace-last) | Kotlin + model | IN_PROGRESS | arc point times interpolated |
| Eraser | gpencil_stroke_eraser_dostroke, calc_influence, soft_refine | `project_grease_blender_eraser.c` + real delete_tagged_points | native (9 mutants caught) | IN_PROGRESS | occlusion/select-mask omitted; artifact report open |
| Line/Box/Circle/Arc | gpencil_primitive.c | verbatim regions + driver, `nativeGenerateBlenderPrimitive` | native + Kotlin | IN_PROGRESS | commit on release, no handle edit phase |
| Polyline | gpencil_primitive.c segment bookkeeping | `PolylineSession` | Kotlin + native | IN_PROGRESS | touch confirm gesture |
| Curve | gpencil_primitive.c bezier | generator only | native | NOT_IMPLEMENTED | no tool or control points |

### Selection and editing

| Feature | Source | Implementation | Tests | Status | Limitation |
|---|---|---|---|---|---|
| Select/point/stroke | gpencil_select_exec | `project_grease_blender_edit.c` pick | native | IN_PROGRESS | taps pick whole strokes by default |
| All/Linked/Alternate/More/Less/First/Last/Grouped | gpencil_select.c, ED_gpencil_select_toggle_all | `project_grease_blender_select.c` (23 verbatim regions) | native | IN_PROGRESS | controller API only |
| Box/Circle/Lasso, ADD/SUB/SET/AND/XOR, modal | gpencil_select.c, select_utils.c | same | native | IN_PROGRESS | no box/circle tools; lasso only |
| Select Random | gpencil_select_random_exec | not ported | none | NOT_IMPLEMENTED | needs BLI_rng closure |
| Segment select | ED_gpencil_select_stroke_segment | not ported | none | NOT_IMPLEMENTED | |
| Move/Rotate/Scale/Mirror | transform conversion rules | selection-wide native transforms, median pivot | native (mutation-checked) | IN_PROGRESS | not copied from Blender source |
| Delete strokes/points | GPENCIL_OT_delete | native using `delete_tagged_points` | native (stand-in splitter) | IN_PROGRESS | no UI button; splitting only tested in app build |
| Duplicate | GPENCIL_OT_duplicate | whole-stroke duplicate | none new | IN_PROGRESS | Blender duplicates selected points |
| Split/Subdivide/Trim/Close | BKE_gpencil_stroke_* | Backend methods | none | IN_PROGRESS | fixed menu parameters |
| Join | BKE join | Backend method | none | IN_PROGRESS | no UI |

### Fill, paint, sculpt

| Feature | Source | Implementation | Tests | Status | Limitation |
|---|---|---|---|---|---|
| Fill | gpencil_fill.c boundary fill/outline | `project_grease_legacy_fill.cpp` + mask readback | existing native | IN_PROGRESS | leak/dilate not exposed |
| Stroke color / opacity / thickness / materials | material style | gpd->mat[] | none | IN_PROGRESS | one color for stroke and fill |
| Sculpt (8 brushes) | gpencil_sculpt_paint.c | `project_grease_legacy_sculpt.cpp`, `LegacyGpSculptEngine.kt` | Kotlin + native | IN_PROGRESS | no clone/automask |
| Vertex / weight paint | gpencil_vertex_paint.c / weight | none | none | NOT_IMPLEMENTED | |

### Animation

| Feature | Source | Implementation | Status | Limitation |
|---|---|---|---|---|
| Layers (add/order/dup/delete/rename/visibility/lock) | BKE_gpencil_layer_* | Backend | IN_PROGRESS | no layer opacity control in the UI (opacity is saved and restored) |
| Frames add/duplicate/delete/holds | BKE_gpencil_frame_* | Backend | IN_PROGRESS | insert-and-shift missing |
| Playback/loop/FPS/navigation | n/a (Android) | `AnimationController` | IN_PROGRESS | |
| Interpolation | gpencil_interpolate.c | linear, matching topology | IN_PROGRESS | no pairing/easing |
| Onion skin | onion flags, DRW | sets flags | IN_PROGRESS | ghost drawing not inspected |
| Multiframe | GP_DATA_STROKE_MULTIEDIT | flag + frame select | IN_PROGRESS | no frame-selection UI |

### Modifiers, Line Art, persistence

| Feature | Status | Notes |
|---|---|---|
| Smooth / Simplify / Subdivide | IN_PROGRESS | applied destructively to one stroke; not a stack |
| Noise, Dash, Outline, Thickness, Color, Deform, Generators | NOT_IMPLEMENTED | modifier sources not traced |
| Modifier stack ordering | NOT_IMPLEMENTED | needs an object-level stack |
| Shader effects | BLOCKED | needs DRW pipeline |
| Line Art | BLOCKED | needs scene/object/depsgraph closure not established |
| New/Open/Save | IN_PROGRESS | round-trips strokes (material, thickness, cyclic, fill), layer state and the palette (defect 6, fixed); point vertex colors, per-point weights and onion settings are not saved |
| Save as, Export PNG/GIF/animation | NOT_IMPLEMENTED | no pipeline |

## Device validation checklist (only you can produce this evidence)

Record pass/fail per line; any pass lets that feature move to AVAILABLE.

1. Draw a fast straight line with a finger and with a pen: stays straight, no hooks.
2. Draw a curve with the stabilizer on and off, and with the factor slider moved: only the toggle changes stabilization.
3. Pen pressure changes width; finger width is constant.
4. Tools Line, Rect, Circle, Arc, Polyline, Fill are now enabled; each creates the expected shape.
5. Erase across a stroke (Hard, Soft, Stroke): note any square ends, and send a screenshot.
6. Lasso a group, then Move/Rotate/Scale/Mirror: the whole selection moves; a filled shape's fill follows.
7. Tap a stroke with Select; tap empty space (deselects).
8. Materials sheet Opacity slider changes the next stroke immediately.
9. Add frames, play, onion skin on: ghosts visible?
10. Save, close, reopen: strokes keep their material, thickness and closed flag; layers keep name, hidden/locked state and opacity; the material palette keeps its colors. Note anything that differs.
11. Open the Layers sheet on a hidden or locked layer: the switches match the layer.
