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
| New/Open/Save | IN_PROGRESS | round-trips strokes (material, thickness, cyclic, fill), per-point and fill vertex colors, layer state and the palette (defect 6, fixed; format v3); per-point deform weights and onion settings are not saved |
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

## Update 2026-10-04: feature registry snapshot

Source: `FeatureRegistry.kt` on `fix/ci-emulator-gate` (base `71fae48`). All 166 entries are `wired(...)`: state IN_PROGRESS, not device-verified. Evidence is the first sentence of each entry's limitation text. Emulator results for the same base are in `STATUS.md`.

| Area | Feature | State | Evidence |
|---|---|---|---|
| drawing | FREEHAND | in progress (wired, not device-verified) | Native tool session: every touch sample (historical ones included) goes to native in one call per batch; |
| drawing | PRESSURE | in progress (wired, not device-verified) | Pen pressure only; |
| drawing | ERASER | in progress (wired, not device-verified) | Hard/soft/stroke eraser use Blender's per-stroke algorithm; |
| drawing | FILL | in progress (wired, not device-verified) | Fills from a rasterized stroke mask (Blender boundary fill + outline); |
| drawing | LINE | in progress (wired, not device-verified) | Blender gpencil_primitive.c geometry; |
| drawing | RECTANGLE | in progress (wired, not device-verified) | Blender box geometry; |
| drawing | CIRCLE | in progress (wired, not device-verified) | Blender circle geometry; |
| drawing | ARC | in progress (wired, not device-verified) | Blender arc geometry, bulge direction fixed; |
| drawing | POLYLINE | in progress (wired, not device-verified) | Touch polyline (tap adds a vertex, tap the last vertex to finish). |
| drawing | CURVE | in progress (wired, not device-verified) | Blender curve geometry (gpencil_primitive.c GP_STROKE_CURVE): drag start->end, drag the end/control handles, tap away or Confirm to commit. |
| drawing | STABILIZATION | in progress (wired, not device-verified) | Lazy-mouse semantics ported; |
| drawing | SMOOTHING | in progress (wired, not device-verified) | Active smoothing ported but off by default (Blender's preset value not traced). |
| drawing | SPACING | in progress (wired, not device-verified) | Maps to Blender's Euclidean input filter, not a true spacing control. |
| drawing | PRESSURE_CURVE | in progress (wired, not device-verified) | The brush curve_sensitivity / curve_strength CurveMapping (CURVE_MAPPING) with a curve editor in Advanced > Brush; |
| selection | SELECT | in progress (wired, not device-verified) | Blender click-select (nearest point, whole stroke by default). |
| selection | LASSO | in progress (wired, not device-verified) | Blender lasso with eSelectOp semantics on canvas coordinates (screen -> canvas through CanvasMapping, the presenter's own mapping); |
| selection | SELECT_POINT | in progress (wired, not device-verified) | pickEntireStrokes=false selects single points; |
| selection | SELECT_ALL | in progress (wired, not device-verified) | Native + controller API; |
| selection | SELECT_LINKED | in progress (wired, not device-verified) | Native + controller API; |
| selection | SELECT_ALTERNATE | in progress (wired, not device-verified) | Native + controller API; |
| selection | SELECT_MORE | in progress (wired, not device-verified) | Native + controller API; |
| selection | SELECT_LESS | in progress (wired, not device-verified) | Native + controller API; |
| selection | SELECT_LAST | in progress (wired, not device-verified) | Native + controller API; |
| selection | SELECT_RANDOM | in progress (wired, not device-verified) | Advanced > Select random 50%: edit3 pg_gp_select_random with the real pinned BLI_rng (same sequence as Blender for a seed); |
| selection | SELECT_BOX | in progress (wired, not device-verified) | Box select tool (Edit rail): drag a rectangle, Blender's box select with the select mode and operation. |
| selection | SELECT_CIRCLE | in progress (wired, not device-verified) | Circle select tool (Edit rail): every dab of the drag selects inside the brush radius, the first dab of a Set gesture replaces the selection. |
| selection | SELECT_SEGMENT | in progress (wired, not device-verified) | Edit > Select mode Segment: a tap selects the run of points between the nearest crossings with other strokes (ED_gpencil_select_stroke_segment, verbatim, projec |
| selection | SELECT_FIRST | in progress (wired, not device-verified) | Blender select_first; |
| selection | SELECT_GROUPED | in progress (wired, not device-verified) | Blender select_grouped (layer/material); |
| editing | MOVE | in progress (wired, not device-verified) | Selection-wide on selected points via native transform. |
| editing | ROTATE | in progress (wired, not device-verified) | Selection-wide about the selection median. |
| editing | SCALE | in progress (wired, not device-verified) | Selection-wide about the selection median. |
| editing | MIRROR | in progress (wired, not device-verified) | Selection-wide about the selection median. |
| editing | TRANSFORM_SELECTION | in progress (wired, not device-verified) | Rules from Blender transform conversion; |
| editing | DUPLICATE | in progress (wired, not device-verified) | Duplicates the whole selected stroke; |
| editing | DELETE | in progress (wired, not device-verified) | Menu deletes one stroke by index; |
| editing | DELETE_POINTS | in progress (wired, not device-verified) | Native + controller API; |
| editing | SPLIT | in progress (wired, not device-verified) | BKE_gpencil_stroke_split at a fixed point from the menu. |
| editing | SUBDIVIDE | in progress (wired, not device-verified) | BKE_gpencil_stroke_subdivide. |
| editing | TRIM | in progress (wired, not device-verified) | BKE_gpencil_stroke_trim to first intersection. |
| editing | CLOSE | in progress (wired, not device-verified) | BKE_gpencil_stroke_close. |
| editing | JOIN_STROKES | in progress (wired, not device-verified) | Controller API; |
| layers and animation | LAYERS | in progress (wired, not device-verified) | Sheet name and switches are read from the native layer. |
| layers and animation | LAYER_VISIBILITY | in progress (wired, not device-verified) | Sheet switch is read from native state; |
| layers and animation | LAYER_LOCKING | in progress (wired, not device-verified) | Sheet switch is read from native state; |
| layers and animation | LAYER_ORDERING | in progress (wired, not device-verified) | BLI_listbase_move_index. |
| layers and animation | LAYER_DUPLICATION | in progress (wired, not device-verified) | BKE_gpencil_layer_duplicate. |
| layers and animation | LAYER_DELETION | in progress (wired, not device-verified) | BKE_gpencil_layer_delete. |
| layers and animation | LAYER_RENAME | in progress (wired, not device-verified) | Renames the native layer. |
| layers and animation | FRAMES | in progress (wired, not device-verified) | Real bGPDframe lifecycle. |
| layers and animation | TIMELINE | in progress (wired, not device-verified) | Frame strip with KEY/HOLD labels. |
| layers and animation | ADD_FRAME | in progress (wired, not device-verified) | Creates or selects the frame after the current one. |
| layers and animation | INSERT_FRAME | in progress (wired, not device-verified) | Timeline "Insert blank keyframe" (GPENCIL_OT_blank_frame_add on the active layer: frames at/after the current one move one later) and "Clean duplicate frames". |
| layers and animation | DUPLICATE_FRAME | in progress (wired, not device-verified) | BKE_gpencil_frame_duplicate. |
| layers and animation | DELETE_FRAME | in progress (wired, not device-verified) | BKE_gpencil_layer_frame_delete. |
| layers and animation | FRAME_HOLDS | in progress (wired, not device-verified) | Holds via GP_GETFRAME_USE_PREV. |
| layers and animation | PLAYBACK | in progress (wired, not device-verified) | Handler-driven playback; |
| layers and animation | PAUSE | in progress (wired, not device-verified) |  |
| layers and animation | LOOP | in progress (wired, not device-verified) |  |
| layers and animation | FPS | in progress (wired, not device-verified) |  |
| layers and animation | FRAME_NAVIGATION | in progress (wired, not device-verified) |  |
| layers and animation | KEYFRAME | in progress (wired, not device-verified) | A frame is a keyframe once created; |
| layers and animation | INTERPOLATION | in progress (wired, not device-verified) | In-between frames with Blender's easing (Linear, Quad..Bounce, Elastic; |
| layers and animation | ONION_SKIN | in progress (wired, not device-verified) | Overlay switch GP_DATA_SHOW_ONIONSKINS + per-layer GP_LAYER_ONIONSKIN; |
| layers and animation | ONION_RANGE | in progress (wired, not device-verified) | bGPdata gstep / gstep_next: keyframes (Relative) or frames (Absolute) before / after. |
| layers and animation | ONION_OPACITY | in progress (wired, not device-verified) | gpd->onion_factor through Blender's ghost alpha formula. |
| layers and animation | ONION_FADE | in progress (wired, not device-verified) | GP_ONION_FADE switch in Onion Skin; |
| layers and animation | ONION_LAYER_FILTER | in progress (wired, not device-verified) | Per-layer "Use onion skinning" switch in Layers (GP_LAYER_ONIONSKIN, on for new layers); |
| layers and animation | MULTIFRAME | in progress (wired, not device-verified) | Timeline long press > Select frame (GP_FRAME_SELECT, orange outline) and the Multiframe chip (GP_DATA_STROKE_MULTIEDIT): native edits act on every selected fram |
| paint, materials, sculpt | MATERIALS | in progress (wired, not device-verified) | Materials live in gpd->mat[]; |
| paint, materials, sculpt | CREATE_MATERIAL | in progress (wired, not device-verified) | Created implicitly by 'Next brush/material'. |
| paint, materials, sculpt | DELETE_MATERIAL | in progress (wired, not device-verified) | Materials > Delete material (with confirmation): strokes using the slot are deleted (as specified for this app; |
| paint, materials, sculpt | SELECT_MATERIAL | in progress (wired, not device-verified) |  |
| paint, materials, sculpt | STROKE_COLOR | in progress (wired, not device-verified) | Sets stroke and fill together; |
| paint, materials, sculpt | FILL_COLOR | in progress (wired, not device-verified) | Advanced > Use color as fill color: edit3 sets the active material's fill color (GP_MATERIAL fill_rgba); |
| paint, materials, sculpt | THICKNESS | in progress (wired, not device-verified) |  |
| paint, materials, sculpt | OPACITY | in progress (wired, not device-verified) | Pushed to the material from the Properties panel (and now the Materials sheet). |
| paint, materials, sculpt | FILL_ENABLE | in progress (wired, not device-verified) | Toggles GP_MATERIAL_FILL_SHOW. |
| paint, materials, sculpt | SCULPT | in progress (wired, not device-verified) | Native tool session: gpencil_sculpt_paint.c brush callbacks and per-point hit test (do_stroke) carried verbatim and applied for every touch sample; |
| paint, materials, sculpt | SCULPT_GRAB | in progress (wired, not device-verified) | Points under the brush at the first sample keep their start weights and follow the drag (gpencil_brush_grab_*). |
| paint, materials, sculpt | SCULPT_SMOOTH | in progress (wired, not device-verified) | BKE_gpencil_stroke_smooth_point with endpoints fixed and position only (thickness/opacity untouched). |
| paint, materials, sculpt | SCULPT_THICKNESS | in progress (wired, not device-verified) | pressure += influence / 10 (invert: -), Blender's formula. |
| paint, materials, sculpt | SCULPT_STRENGTH | in progress (wired, not device-verified) | strength += influence x 0.125 clamped 0..1 (invert: -), Blender's formula. |
| paint, materials, sculpt | SCULPT_PUSH | in progress (wired, not device-verified) | Moves points by the drag delta x influence. |
| paint, materials, sculpt | SCULPT_PINCH | in progress (wired, not device-verified) | Pulls toward the brush center by (influence / 5)^2 (invert inflates). |
| paint, materials, sculpt | SCULPT_TWIST | in progress (wired, not device-verified) | Rotates around the brush center by influence degrees (invert reverses). |
| paint, materials, sculpt | SCULPT_RANDOMIZE | in progress (wired, not device-verified) | Jitter perpendicular to the drag with BLI_rng (position only, Blender's preset). |
| paint, materials, sculpt | VERTEX_PAINT | in progress (wired, not device-verified) | Native tool session: gpencil_vertex_paint.c Draw (tint), Blur, Average, Smear (grid) and Replace carried verbatim with the per-sample selection of points under  |
| paint, materials, sculpt | WEIGHT_PAINT | in progress (wired, not device-verified) | Native tool session: gpencil_weight_paint.c Draw, Blur, Average and Smear (kd-tree nearest points) carried verbatim with the per-sample selection; |
| paint, materials, sculpt | ADVANCED_FILL | in progress (wired, not device-verified) | Fill "Extend" slider (brush fill_extend_fac, default 0): open strokes are prolonged at both ends in the fill boundary by that fraction of their length (pg_fill_ |
| paint, materials, sculpt | FILL_GAP_TOLERANCE | in progress (wired, not device-verified) | Fill bar "Leak" = Blender fill_leak (px, default 3) for the Legacy boundary fill. |
| paint, materials, sculpt | FILL_EXPANSION | in progress (wired, not device-verified) | Fill bar "Dilate" = Blender dilate (px, default 1; |
| paint, materials, sculpt | FILL_BOUNDARY | in progress (wired, not device-verified) | Fill bar boundary All / Strokes / Edit Lines (fill_draw_mode): Edit Lines uses 1 px center lines in the fill mask; |
| paint, materials, sculpt | STROKE_TEXTURES | in progress (wired, not device-verified) | Material stroke style Texture with a picked image (SAF): U along the stroke length (texture_pixsize), V across the width; |
| paint, materials, sculpt | FILL_TEXTURES | in progress (wired, not device-verified) | Material fill style Texture with a picked image (SAF): UV from the stroke's bounding square through texture_scale / texture_angle / texture_offset, mixed by mix |
| paint, materials, sculpt | TEXTURE_SETTINGS | in progress (wired, not device-verified) | Texture mix, pixel size (stroke), scale / offset / angle (fill) in Materials. |
| paint, materials, sculpt | TEXTURE_SCALE | in progress (wired, not device-verified) | Fill texture_scale (x, y) and stroke texture_pixsize. |
| paint, materials, sculpt | TEXTURE_OPACITY | in progress (wired, not device-verified) | Texture mix factor (mix_stroke_factor / mix_factor): 0 = texture only, 1 = material colour only. |
| view | PAN | in progress (wired, not device-verified) |  |
| view | ZOOM | in progress (wired, not device-verified) | Pinch zoom. |
| view | FIT_CANVAS | in progress (wired, not device-verified) | Fit canvas button: zoom 1 / no pan, which the presenter maps to the whole canvas in view with a 4% margin per side. |
| view | RESET_VIEW | in progress (wired, not device-verified) |  |
| view | GRID | in progress (wired, not device-verified) | Compose overlay in screen pixels, not tied to canvas units. |
| view | GUIDES | in progress (wired, not device-verified) | Old overlay lines (Settings > Guides) stay as a visual aid; |
| view | SNAPPING | in progress (wired, not device-verified) | Snaps input points to the grid size in canvas units. |
| modifiers and effects | MODIFIERS | in progress (wired, not device-verified) | Live per-layer stack (Advanced sheet) of Thickness, Opacity, Tint, Hue/Saturation, Length, Smooth, Simplify, Subdivide, Offset, Noise, Build, Time Offset, Hook, |
| modifiers and effects | MODIFIER_ORDERING | in progress (wired, not device-verified) | The stack order is the evaluation order (Up/Down in the Advanced sheet); |
| modifiers and effects | NOISE | in progress (wired, not device-verified) | Noise deformStroke() ported from MOD_gpencil_legacy_noise.c with Blender's BLI_hash seeds, evaluated with the current frame; |
| modifiers and effects | DASH | in progress (wired, not device-verified) | Advanced > Dash: baked once into the selected strokes (one dash/gap segment pattern in points, like the Dash modifier's first segment); |
| modifiers and effects | OUTLINE | in progress (wired, not device-verified) | Outline modifier baked (Advanced > Outline selected strokes): each selected open stroke becomes the closed perimeter of its thick shape with round caps (pg_gp_o |
| modifiers and effects | THICKNESS_MODIFIER | in progress (wired, not device-verified) | Thickness (MOD_gpencil_legacy_thick.c) and Opacity modifiers as live stack entries; |
| modifiers and effects | COLOR_MODIFIER | in progress (wired, not device-verified) | Tint and Hue/Saturation modifiers as live stack entries (no vertex groups/curve). |
| modifiers and effects | DEFORM | in progress (wired, not device-verified) | Hook and Lattice as live 2D modifiers (MOD_HOOK, MOD_LATTICE); |
| modifiers and effects | GENERATE | in progress (wired, not device-verified) | Live Dot Dash, Outline, Mirror, Array and Multiple Strokes (LIVE_GENERATORS), Build (MOD_BUILD) and Envelope (MOD_ENVELOPE); |
| modifiers and effects | MERGE_BY_DISTANCE | in progress (wired, not device-verified) | Advanced > Merge by distance: BKE_gpencil_stroke_merge_distance on the selected strokes (threshold 2 canvas px, selected points only). |
| modifiers and effects | STROKE_CAPS | in progress (wired, not device-verified) | Advanced > Toggle caps: GPENCIL_OT_stroke_caps_set (round/flat per end); |
| modifiers and effects | START_POINT | in progress (wired, not device-verified) | Advanced > Set start point: rotates a cyclic stroke so the selected point comes first (GPENCIL_OT_stroke_start_set); |
| modifiers and effects | SEPARATE_TO_LAYER | in progress (wired, not device-verified) | Advanced > Separate to new layer: selected strokes of the active layer move to a new "Separated" layer at the same frame numbers. |
| modifiers and effects | MOVE_TO_LAYER | in progress (wired, not device-verified) | Layers > Move selection here: selected strokes move to that layer (frame created at the same number when missing). |
| modifiers and effects | COPY_PASTE | in progress (wired, not device-verified) | Advanced > Copy / Paste strokes: process-wide clipboard (not saved, lost when the app closes); |
| modifiers and effects | VERTEX_GROUP_OPS | in progress (wired, not device-verified) | Weight Paint > Selection: Assign (current weight), Remove from group, Select, Deselect, Invert, Normalize on the active vertex group, over the selected points ( |
| modifiers and effects | LAYER_MERGE_DOWN | in progress (wired, not device-verified) | Layers > Merge down: strokes move into the layer below per frame (BKE_gpencil_layer_merge semantics), the active layer is deleted with its modifier/effect stack |
| modifiers and effects | LAYER_ISOLATE | in progress (wired, not device-verified) | Layers > Isolate: hides every layer except the active one (no toggle-back, no lock variant). |
| modifiers and effects | LAYER_LOCK_ALL | in progress (wired, not device-verified) | Layers > Lock all / Unlock all. |
| modifiers and effects | MOD_BUILD | in progress (wired, not device-verified) | Live Build (MOD_gpencil_legacy_build.c): Sequential / Concurrent, Grow / Shrink, delay and length in frames from the keyframe (pg_build_visible); |
| modifiers and effects | MOD_TIME_OFFSET | in progress (wired, not device-verified) | Live Time Offset (MOD_gpencil_legacy_time.c): the layer shows the keyframe at pg_time_offset_frame (Normal / Reverse / Fixed / Ping-pong, offset, scale, custom  |
| modifiers and effects | MOD_HOOK | in progress (wired, not device-verified) | Live 2D Hook (pg_hook_deform): centre, offset, rotation, scale, radius, Constant / Smooth / Linear falloff, strength; |
| modifiers and effects | MOD_LATTICE | in progress (wired, not device-verified) | Live 2D Lattice (pg_lattice_deform): N x M grid (2..6) over a canvas rectangle, grid nodes dragged on the canvas, bilinear deform, strength. |
| modifiers and effects | MOD_ENVELOPE | in progress (wired, not device-verified) | Live Envelope with the pinned MOD_gpencil_legacy_envelope.c functions (verbatim): Deform, Segments and Fills modes, spread, skip, thickness, strength, material. |
| modifiers and effects | MOD_WEIGHT_PROXIMITY | in progress (wired, not device-verified) | Live Vertex Weight Proximity: writes the target group's weights on the evaluated strokes from the distance to a canvas point (handle on the canvas), lowest / hi |
| modifiers and effects | MOD_WEIGHT_ANGLE | in progress (wired, not device-verified) | Live Vertex Weight Angle: weight 1 - sin(angle between segment and reference direction) as MOD_gpencil_legacy_weight_angle.c for a front view; |
| modifiers and effects | LIVE_GENERATORS | in progress (wired, not device-verified) | Live Dot Dash (one segment), Outline, Mirror (pivot handle), Array (constant offset) and Multiple Strokes reuse the baked operators on the evaluated copy; |
| modifiers and effects | MODIFIER_INFLUENCE | in progress (wired, not device-verified) | Every modifier entry: material slot, material pass, layer pass and vertex group filters with invert, and a custom curve along the stroke (CurveMapping), applied |
| modifiers and effects | DRAWING_GUIDES | in progress (wired, not device-verified) | Draw tool guides (Advanced): Circular, Radial, Parallel, Grid, Isometric with centre, angle and spacing (GP_GUIDE_*); |
| modifiers and effects | ONION_KEYTYPE_LOOP | in progress (wired, not device-verified) | Onion keyframe-type filter (bGPdata.onion_keytype) and Loop (GP_ONION_LOOP, BKE_gpencil_visible_stroke_advanced_iter wrap) in the presenter; |
| modifiers and effects | KEYFRAME_TYPES | in progress (wired, not device-verified) | bGPDframe.key_type (Keyframe, Extreme, Breakdown, Jitter, Moving Hold) from the timeline long-press menu, shown as coloured markers, saved ("keyType"), used by  |
| modifiers and effects | LAYER_BLEND | in progress (wired, not device-verified) | Layer blend modes (Regular, Hard Light, Add, Subtract, Multiply, Divide) through gpencil_layer_blend_frag.glsl's blend_mode_output over the offscreen layer, sav |
| modifiers and effects | LAYER_TINT | in progress (wired, not device-verified) | Layer tint colour + factor (gpLayerTint) and stroke thickness offset (line_change, gpThicknessOffset) applied while the layer is drawn, saved. |
| modifiers and effects | MATERIAL_SLOTS | in progress (wired, not device-verified) | Material names (Material id name), slot order (strokes remapped, textures follow), Lock / Hide / Solo (GP_MATERIAL_LOCKED / HIDE, material isolate) and pass ind |
| modifiers and effects | LINE_TYPES | in progress (wired, not device-verified) | Line types Dots and Squares (GP_MATERIAL_MODE_DOT / SQUARE): one quad per point of the point thickness, aligned to the path, the canvas or the screen and turned |
| modifiers and effects | BRUSH_PRESETS | in progress (wired, not device-verified) | Brush chips: Pencil, Pencil Soft, Ink Pen, Ink Pen Rough, Marker Bold, Marker Chisel, Pen, Airbrush, Fill Area and the four erasers with BKE_gpencil_brush_prese |
| modifiers and effects | PRIMITIVE_EDIT | in progress (wired, not device-verified) | Line, Box, Circle, Arc stay editable after the drag (gpencil_primitive.c): drag the start / end handles, Subdiv - / +, Extrude (line becomes a polyline), Confir |
| modifiers and effects | CURVE_MAPPING | in progress (wired, not device-verified) | colortools.c CurveMapping (auto / vector handles, bezier table, clipped evaluation) ported in project_grease_curvemap.c, used by the brush pressure / strength c |
| modifiers and effects | ELASTIC_EASING | in progress (wired, not device-verified) | BLI_easing_elastic_* verbatim (easing.c) with amplitude / period; |
| modifiers and effects | INTERPOLATE_SEQUENCE | in progress (wired, not device-verified) | GPENCIL_OT_interpolate_sequence: every frame between the keyframes around the current frame gets an in-between in one undo step (timeline and long-press menu). |
| modifiers and effects | EXPORT_MP4 | in progress (wired, not device-verified) | MP4 / H.264 via MediaCodec + MediaMuxer from the offscreen renderer: project frame range at the project fps with holds, YUV 4:2:0, odd sizes cropped by one pixe |
| modifiers and effects | SELECT_MENU | in progress (wired, not device-verified) | Edit bar: Select all / none / invert / linked / alternate / more / less / first / last / same layer / same material, Delete points, Join and the Pick points swi |
| modifiers and effects | STROKE_EFFECTS | in progress (wired, not device-verified) | Each layer effect has a target: whole layer, strokes only or fills only; |
| modifiers and effects | FILL_EFFECTS | in progress (wired, not device-verified) | Effect target "Fills": see Stroke effects. |
| modifiers and effects | VISUAL_EFFECTS | in progress (wired, not device-verified) | Per-layer effect list in the Layers sheet (Colorize, Pixelate, Flip, Wave Distortion, Swirl, Shadow, Rim, Blur, Glow), saved with the document. |
| modifiers and effects | LAYER_MASKS | in progress (wired, not device-verified) | Per-layer mask list with use-mask, invert and hide; |
| modifiers and effects | ADVANCED_INTERPOLATION | in progress (wired, not device-verified) | Unequal strokes are interpolated (resampled to matching point counts) and paired by index; |
| modifiers and effects | ADVANCED_ONION_SKIN | in progress (wired, not device-verified) | Onion mode Relative / Absolute / Selected (bGPdata.onion_mode), custom ghost colours (GP_ONION_GHOST_PREVCOL / NEXTCOL), keyframe-type filter and loop (ONION_KE |
| modifiers and effects | LINE_ART | in progress (wired, not device-verified) | BATCH 3 OF 3: Blender 3.6.23 Line Art on Scene-lite generates strokes. |
| persistence and export | NEW_PROJECT | in progress (wired, not device-verified) | New Project offers the 2D Animation / Blank / Storyboard templates (GreaseTemplates): layers bottom to top, material slots with stroke color and fill on/off, fp |
| persistence and export | OPEN_PROJECT | in progress (wired, not device-verified) | Restores points (incl. |
| persistence and export | SAVE | in progress (wired, not device-verified) | Saves everything OPEN_PROJECT restores, including each layer's modifier stack (format version 4). |
| persistence and export | SAVE_AS | in progress (wired, not device-verified) | Project > Save as: Storage Access Framework create-document writes the project JSON to the chosen file and continues under that name (also stored in the app's p |
| persistence and export | EXPORT | in progress (wired, not device-verified) | Project > Export writes SVG (current frame, or a folder with one file per frame) and PDF (one page per frame) through the Storage Access Framework; |
| persistence and export | EXPORT_PNG | in progress (wired, not device-verified) | Project > Export > PNG: the current frame rendered offscreen at canvas size by the same presenter (modifiers, masks, effects; |
| persistence and export | EXPORT_GIF | in progress (wired, not device-verified) | Project > Export GIF: frames start..end of the project settings rendered offscreen like PNG export (holds show the previous keyframe), animated GIF89a with a fi |
| persistence and export | EXPORT_ANIMATION | in progress (wired, not device-verified) | Project > Export: GIF, PNG sequence (frames start..end with holds) and MP4 video (EXPORT_MP4). |
| persistence and export | PROJECT_SETTINGS | in progress (wired, not device-verified) | Project > Settings: canvas size, fps, start/end frame, background colour, transparent background; |
| persistence and export | IMPORT_SVG | in progress (wired, not device-verified) | Project > Import SVG (Storage Access Framework): path/polyline/polygon/line/rect/circle/ellipse become strokes on the active layer/frame, curves flattened (SvgI |
| persistence and export | TRACE_IMAGE | in progress (wired, not device-verified) | Project > Trace image: threshold (0.5), trace bright areas, tolerance (0.6 px); |
| persistence and export | ANNOTATIONS | in progress (wired, not device-verified) | Annotate tool (NOTES group): freehand notes in a separate annotation bGPdata (layer "Note", own frames that hold like keyframes, Blender annotate_paint.c semant |
