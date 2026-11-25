# Blender 3.6.23 port inventory

For each user-facing feature: the app code, the Blender code it should run, and how close the app is
to it. App paths are relative to `native/blender_gp/` unless they start with `android/`. Blender
paths are relative to `third_party/blender/source/blender/`. `EC.kt` is
`android/app/src/main/java/com/smitnk/projectgrease/editor/EditorControllers.kt`.

What ships on Android is `tools/android_gp_source_manifest.txt`, not `native/blender_gp/CMakeLists.txt`.
The desktop CMake also compiles `editors/gpencil_legacy/gpencil_paint.c`, `gpencil_utils.c`,
`gpencil_legacy.c` (editor) and `MOD_gpencillegacy.c`. The APK does **not** compile them. Every feature
that names them below runs a port in the app.

Classes:

- **VERBATIM**: Blender's file compiled unchanged, or a region that `tools/verify_blender_verbatim.py`
  checks byte for byte (153 regions, 0 failures).
- **ADAPTED**: Blender's code with the context it reads (`bContext`, RNA, the depsgraph, the 3D view)
  replaced. The reason is given in the row.
- **CUSTOM**: logic the app wrote itself, not Blender's.

Out of scope here: the armature/rig, the depsgraph, constraints, drivers (owned by other workers) and
the 3D view.

## Summary

Each table row counts once. A row whose algorithm is Blender's byte for byte (compiled files or
VERBATIM regions, with only glue adapted) counts as VERBATIM.

| Class | Rows | Which |
|---|---|---|
| VERBATIM | 9 | Smooth, Simplify, Subdivide, Offset, Noise, Envelope, Dot Dash/Mirror/Array/Multiple modifiers; shader effects on the engine path; onion skin; render engine |
| ADAPTED | 28 | everything else not listed as CUSTOM |
| CUSTOM | 8 | draw input, transform, .blend IO, undo, fallback presenter, Erase fills like paint (intentional), old primitive generator (dead), old sculpt module (dead) |
| missing | 1 row | Hook, Lattice, Vertex Weight Proximity modifiers (need 3D objects) |

Port done in this pass: **Draw commit Post-Processing**. It was missing (CUSTOM); it is now ADAPTED.
See "Ports" below.

## Draw

| Feature | App | Blender | Class | Missing Blender dependencies |
|---|---|---|---|---|
| Input filter, lazy mouse, add point, jitter, angle, buffer smoothing, arc and fake points | `project_grease_draw_input.c:168` filter_mval, `:183` smooth_buffer, `:237` append_point, `:337` add_arc_points, `:396` add_fake_points, `:416` pg_draw_input_add | `editors/gpencil_legacy/gpencil_paint.c:346` gpencil_stroke_filtermval, `:542` gpencil_smooth_buffer, `:747` gpencil_stroke_addpoint, `:475` gpencil_brush_jitter, `:497` gpencil_brush_angle, `:3416` gpencil_add_arc_points, `:3586` gpencil_add_fake_points, `:2815` gpencil_draw_apply | CUSTOM | A statement-by-statement retrace that came from the former Kotlin engine. Its golden data (`tools/draw_input_golden`) is the Kotlin engine's output, not Blender's. The fallback "power curve" (used when no CurveMapping is set) is not Blender. Skipped: tGPsdata, `ED_gpencil_sbuffer_*` editing in place (the engine holds points back instead), the guides' `gpencil_snap_to_guide`, and the depth / view projection (`gpencil_stroke_convertcoords`). |
| Stroke buffer | `project_grease_legacy_sbuffer.c:23` ED_gpencil_sbuffer_ensure | `editors/gpencil_legacy/gpencil_utils.c` ED_gpencil_sbuffer_ensure | ADAPTED | An exact copy without VERBATIM markers. It is a separate file so that the rest of gpencil_utils.c (RNA, WM) is not pulled in. |
| Stroke commit | `project_grease_gp_backend.cpp:3725` Backend::end_stroke | `gpencil_paint.c:987` gpencil_stroke_newfrombuffer | ADAPTED | Points copy and BKE_gpencil_stroke_add are Blender's. Skipped: `gpencil_stroke_to_outline` (`:921`, needs the view), `gpencil_reproject_toplane`, `ED_gpencil_project_stroke_to_view`, auto-merge (`ED_gpencil_stroke_join_and_trim`, `ED_gpencil_stroke_close_by_distance`), `GP_TOOL_FLAG_CREATE_WEIGHTS`, `BKE_gpencil_stroke_copy_to_keyframes` (multiframe). Paint-on-back is done by reordering afterwards (`EC.kt:1115`). |
| Post-Processing (subdivide, smooth, simplify, trim) | `project_grease_gp_backend.cpp:3795`, `project_grease_legacy_sbuffer.c:69`, `EC.kt:1638` | `gpencil_paint.c:1260-1289, 1349-1353`; `gpencil_utils.c:1266` gpencil_subdivide_stroke; presets in `blenkernel/intern/brush.cc:747-1004` | ADAPTED (ported here, was missing) | Outline post-process (see above). Trim is wired but no preset enables it and there is no UI switch. |

## Erase

| Feature | App | Blender | Class | Missing Blender dependencies |
|---|---|---|---|---|
| Hard / Soft / Point eraser | `project_grease_blender_eraser.c:143` pg_eraser_dostroke (`:107` gpencil_stroke_soft_refine VERBATIM), `project_grease_legacy_eraser.cpp` (BKE_gpencil_stroke_delete_tagged_points), `project_grease_gp_backend.cpp:2178` Backend::erase | `gpencil_paint.c:1537` gpencil_stroke_eraser_dostroke, `:1790` gpencil_stroke_doeraser | ADAPTED | Screen space is the canvas. `gpencil_stroke_eraser_is_occluded` (`:1404`, depth buffer) always reports "visible". |
| Stroke eraser | same path, mode STROKE | `gpencil_paint.c:1537` (GP_BRUSH_ERASER_STROKE branch) | ADAPTED | Same as above. |
| Erase fills like paint | `EC.kt:1311` pixelEraseFills, `EC.kt:3680` eraseFillsAlongPath (a holdout-material stroke along the path) | none: Blender's eraser removes fill outline points | CUSTOM (intentional, the user's feature; keep) | Uses Blender's holdout material (`GP_MATERIAL_IS_FILL_HOLDOUT`) and its draw engine. No equivalent exists in Blender. |

## Fill, primitives, select, transform

| Feature | App | Blender | Class | Missing Blender dependencies |
|---|---|---|---|---|
| Fill | `project_grease_blender_fill.c:870` gpencil_boundaryfill_area, `:1121` dilate_shape, `:1319` gpencil_get_outline_points (7 VERBATIM regions), `project_grease_gp_backend.cpp:5182` fill_outline_snapshot | `editors/gpencil_legacy/gpencil_fill.c:1468`, `:1719`, `:1917`, `:2108`, `:2136` | ADAPTED | `gpencil_render_offscreen` (`:1216`): the GLES presenter renders the stroke mask instead, and it is resampled on the CPU. ImBuf is a CPU float buffer. |
| Primitives (line, box, circle, arc, curve, polyline) | `project_grease_blender_primitive_verbatim.inc:159`, `:319` (4 VERBATIM regions), `project_grease_blender_primitive.c`, JNI `nativeGenerateBlenderPrimitive` | `editors/gpencil_legacy/gpencil_primitive.c` gpencil_primitive_rectangle/circle/arc/bezier, `:691` gpencil_primitive_update_strokes | ADAPTED | tGPDprimitive is reduced to the fields the regions read. The edit-handle modal stays in Kotlin. |
| Old primitive generator | `project_grease_legacy_primitive.cpp`, `project_grease_gp_backend.cpp:1988` create_primitive, JNI `nativeCreatePrimitive` / `nativeGeneratePrimitivePreview` | none | CUSTOM (dead) | No Kotlin caller. It can be removed. |
| Select box / lasso / circle | `project_grease_blender_select.c:1376` pg_gp_select_box, `:1398` lasso, `:1149` circle (20 VERBATIM regions of gpencil_select.c, 3 of ED_select_utils.h, 2 of select_utils.c) | `editors/gpencil_legacy/gpencil_select.c:2246`, `:2298`, `:1665`, `:2150` gpencil_generic_select_exec | ADAPTED | The projection is the identity (`ED_gpencil_stroke_point_is_inside` on the canvas). There is no `view3d` region. |
| Transform (grab / rotate / scale) | `project_grease_gp_backend.cpp:1270` transform_selected_points(m[9]); the gesture and matrix are computed in Kotlin | `editors/transform/transform_convert_gpencil_legacy.c:382` createTransGPencil_strokes, `:34` center_get; the transform system | CUSTOM | All of Blender's transform system: TransData, proportional editing (falloff), snapping, the pivot modes, Transform Fill / Stroke Thickness / Opacity. |

## Sculpt, vertex paint, weight paint

| Feature | App | Blender | Class | Missing Blender dependencies |
|---|---|---|---|---|
| Sculpt brushes | `project_grease_tool_sculpt.c:805` gpencil_sculpt_brush_do_stroke (verbatim callbacks; 4 regions), `:1399` session | `editors/gpencil_legacy/gpencil_sculpt_paint.c:1462`, `:2100` gpencil_sculpt_brush_apply | ADAPTED | bContext, depsgraph evaluated copies (points link to themselves), layer/object matrices (identity), lock axis. |
| Old sculpt module | `project_grease_legacy_sculpt.cpp` | none | CUSTOM (dead) | Nothing links it except its own test. It can be removed. |
| Vertex paint | `project_grease_tool_vertex_paint.c:712` gpencil_vertexpaint_select_stroke, `:1012` session (2 regions) | `editors/gpencil_legacy/gpencil_vertex_paint.c:1170` gpencil_vertexpaint_brush_apply | ADAPTED | Same context replacements as sculpt. |
| Weight paint | `project_grease_tool_weight_paint.c:310` brush_draw_apply, `:808` session (4 regions) | `editors/gpencil_legacy/gpencil_weight_paint.c:1229` gpencil_weightpaint_brush_apply | ADAPTED | Auto-normalize over bone-deformed groups (the armature belongs to the rig worker). Vertex group ensure is done by the caller. |

## Modifiers (live stack and Apply)

The stack is `project_grease_modifier_stack.c`, `project_grease_modifier_stack2.c` and
`project_grease_modifier_gen.c`. Common adaptations: no `Object` / `Depsgraph`, the influence filter
`pg_mod_stroke_affected` instead of `is_stroke_affected_by_modifier`, fixed names for seed hashing,
and object space at `PG_MOD_PIXELS_PER_UNIT` (1000 px per unit).

| Modifier | App | Blender | Class |
|---|---|---|---|
| Thickness | `project_grease_blender_edit.c:582` pg_gp_modstroke_thickness_vgroup | `gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_thick.c:79` deformStroke | ADAPTED (a transliteration with local `interpf`/`CLAMP`; no custom curve) |
| Opacity | `project_grease_blender_edit.c:635` | `MOD_gpencil_legacy_opacity.c:70` | ADAPTED (same) |
| Tint | `project_grease_blender_edit.c:788` | `MOD_gpencil_legacy_tint.c:101` | ADAPTED (same; no gradient-object mode, which needs an Object) |
| Hue/Saturation | `project_grease_blender_edit.c:920` | `MOD_gpencil_legacy_color.c:71` | ADAPTED (same) |
| Length | `project_grease_blender_edit.c:714`, `project_grease_modifier_stack.c:329` | `MOD_gpencil_legacy_length.c:208` | ADAPTED (BKE_gpencil_stroke_extend/shrink are Blender's) |
| Smooth | `project_grease_modifier_stack.c:688` (2 regions) | `MOD_gpencil_legacy_smooth.c:75` | VERBATIM regions + ADAPTED glue |
| Simplify | `project_grease_modifier_stack.c:723` (1 region) | `MOD_gpencil_legacy_simplify.c:54` | VERBATIM region + ADAPTED glue |
| Subdivide | `project_grease_modifier_stack.c:769` (1 region) | `MOD_gpencil_legacy_subdiv.c:53` | VERBATIM region + ADAPTED glue |
| Offset | `project_grease_modifier_stack.c:523` (5 regions) | `MOD_gpencil_legacy_offset.c:65` | VERBATIM regions + ADAPTED glue (y flip) |
| Noise | `project_grease_modifier_stack.c:357` (9 regions) | `MOD_gpencil_legacy_noise.c:109` | VERBATIM regions + ADAPTED glue |
| Build | `project_grease_blender_build.c:357`, `project_grease_blender_mod2.c:10` | `MOD_gpencil_legacy_build.c:702` generate_geometry | ADAPTED (a "Port of" with no VERBATIM regions) |
| Time Offset | `project_grease_blender_mod2.c:33` pg_time_remap (1 region in mod2) | `MOD_gpencil_legacy_time.c` | ADAPTED |
| Envelope | `project_grease_modifier_stack2.c:418` (2 regions) | `MOD_gpencil_legacy_envelope.c:298`, `:568` | VERBATIM regions + ADAPTED glue |
| Vertex Weight Angle | `project_grease_modifier_stack2.c` | `MOD_gpencil_legacy_weight_angle.c:58` | ADAPTED |
| Dot Dash, Mirror, Array, Multiple Strokes | `project_grease_modifier_gen.c` (5 regions) | `MOD_gpencil_legacy_dash.c:259`, `mirror.c:147`, `array.c:333`, `multiply.c:235` | VERBATIM regions + ADAPTED glue |
| Outline | `project_grease_modifier_stack2.c` (reuses the bake operator) | `MOD_gpencil_legacy_outline.c:187` | ADAPTED (no camera Object: the view is the canvas) |
| Texture Mapping | `project_grease_modifier_stack2.c:920` m2_texture | `MOD_gpencil_legacy_texture.c:57` | ADAPTED |
| Armature | rig worker (`project_grease_armature*.c`) | `MOD_gpencil_legacy_armature.c:98` | out of scope |
| Hook, Lattice, Vertex Weight Proximity | none | `MOD_gpencil_legacy_{hook,lattice,weight_proximity}.c` | missing: they need 3D objects (3D view out of scope) |

## Shader effects

| Feature | App | Blender | Class |
|---|---|---|---|
| Effects on the GLES engine path | `draw/engines/gpencil/gpencil_shader_fx.c` compiled unchanged (manifest) | same | VERBATIM |
| Effects on the fallback presenter (Blur, Colorize, Flip, Glow, Pixelate, Rim, Shadow, Swirl, Wave) | `project_grease_shader_fx.c:622` pg_fx_build_passes (pass arithmetic) + CPU port of `gpencil_vfx_frag.glsl`; `android_gp_shader_fx.cpp` GLES2 passes | `draw/engines/gpencil/gpencil_shader_fx.c:82-535` gpencil_vfx_*, `draw/engines/gpencil/shaders/gpencil_vfx_frag.glsl` | ADAPTED (DRW calls become PGFxPass; on GLES2 each pass runs twice because there is no MRT) |

## Onion skin, layers, frames, interpolation, materials

| Feature | App | Blender | Class | Missing Blender dependencies |
|---|---|---|---|---|
| Onion skin | engine: `gpencil_engine.c` + `blenkernel/intern/gpencil_legacy.c:2435` BKE_gpencil_visible_stroke_advanced_iter (both compiled); `android_gp_presentation.cpp:1394` builds the frame copy, `:156` ghost tint | same | VERBATIM (engine) / ADAPTED (presenter copy) | none of note |
| Layers | `project_grease_wm_layer.c:33` exec_layer_add and the other exec bodies | `editors/gpencil_legacy/gpencil_data.c:195` gpencil_layer_add_exec etc. | ADAPTED | bContext/RNA; the BKE_gpencil_layer_* calls are Blender's. |
| Frames / keyframes | `project_grease_blender_edit8.c:34` set_keytype, `:50` frame_select; `project_grease_blender_edit*.c` | `editors/gpencil_legacy/editaction_gpencil.c`, `gpencil_data.c` | ADAPTED | The dopesheet's `bAnimContext`. |
| Interpolation (interpolate, sequence) | `project_grease_blender_interp.c:136` pg_gp_interpolate_run, `:110` update_points, `:67` get_related | `editors/gpencil_legacy/gpencil_interpolate.c:1231` gpencil_interpolate_seq_exec, `:314`, `:235` | ADAPTED (a "Port of"; 1 VERBATIM region elsewhere) | The interactive GPENCIL_OT_interpolate modal (tGPDinterpolate), and the easing curve (`CurveMapping` custom easing). |
| Materials | `project_grease_wm_stroke.c` (material operators on `bGPdata::mat[]`) | `editors/gpencil_legacy/gpencil_data.c:1880-3581` | ADAPTED | Material ID data-blocks / `Main`. The slots are `bGPdata::mat[]` and `PGOpContext::actcol`. |

## Line Art, file IO, render, undo

| Feature | App | Blender | Class | Missing Blender dependencies |
|---|---|---|---|---|
| Line Art | `lineart/project_grease_lineart_cpu.cc:506`, `:1251` (10 VERBATIM regions), `lineart/project_grease_lineart_shadow.c` (2), `project_grease_scene_lite.c` (camera, 5 regions); `lineart_chain.c`, `lineart_util.c` compiled | `gpencil_modifiers_legacy/intern/lineart/lineart_cpu.cc` | ADAPTED | Scene, Collection and Mesh are scene-lite stand-ins. The 3D scene objects are out of scope. |
| .blend IO | Save/load is JSON: `android/.../editor/ProjectDocumentCodec.kt`, `EC.kt:2477` saveDocumentJson, `:2510` loadDocumentJson | `blenloader/intern/readfile.cc`, `writefile.cc`, `blenkernel/intern/gpencil_legacy.c` blend read/write callbacks | CUSTOM | No .blend is read or written. Only the FileReader layer (`blenlib/intern/filereader_*.c`, zstd, zlib) is compiled and tested, and no app code calls it. |
| Render engine (gles/) | `draw/engines/gpencil/*.c` and `draw/intern/draw_cache_impl_gpencil.cc`, `gpu/intern/gpu_batch.cc` and the buffers, compiled UNMODIFIED; `gles/pg_drw_gles.cc` draw-manager subset, `gles/pg_gpu_gles.cc` GPU API on GLES 3.2, `gles/pg_gp_engine_glue.c` | `draw/intern/draw_manager_exec.c`, `draw_manager_data.cc`, `gpu/opengl/*.cc` | VERBATIM (engine) / ADAPTED (draw manager, GL backend) | The full draw manager and GPU module. Shaders are pre-generated (`tools/gles_shaders`) from Blender's own assembly. |
| Fallback presenter | `android_gp_presentation.cpp` | none (stands in for the engine for what it doesn't carry: exports, the weight view) | CUSTOM | Covered by the engine path where that path is on. |
| Undo | `project_grease_gp_backend.cpp:846` history_record: a full document snapshot per step | `editors/gpencil_legacy/gpencil_undo.cc:97`, and memfile undo (`editors/undo/memfile_undo.cc`) | CUSTOM | Blender's undo stack (`BKE_undosys_*`). The step granularity matches; storage is a whole-document copy (`BKE_gpencil_data_duplicate`). |

## Ports

1. **Draw commit Post-Processing.** Blender: `gpencil_paint.c:1260-1289` and `:1349-1353`
   (gpencil_stroke_newfrombuffer), `gpencil_utils.c:1266` (gpencil_subdivide_stroke, now VERBATIM in
   `project_grease_legacy_sbuffer.c:69`), and the preset values in `blenkernel/intern/brush.cc:747-1004`.
   App: `project_grease_gp_backend.cpp:3795`, `EC.kt:1638`, JNI `nativeSetBrushPostProcess`.
   Visible change: Pencil, Pen, Ink Pen and the Markers simplify their strokes (simplify_f 0.002), Pen
   subdivides once, and Ink Pen / Marker Bold smooth at 0.1. Before, every raw input point stayed in the
   stroke. Test: `tests/test_backend_paint_post.cc` (in `tools/run_native_modifier_stack_tests.sh`). It
   compares the committed stroke with Blender's own functions applied to the pre-port stroke.

## CUSTOM items left

| Item | Why it is left |
|---|---|
| Draw input engine | The next candidate. Porting means compiling gpencil_paint.c's input functions against a tGPsdata shim and swapping the golden data for Blender's; that needs a reference run of real Blender. It was not attempted in this pass. |
| Transform | Blender's transform system (TransInfo, the modal, proportional editing, snapping) is a large closure across `editors/transform/`. |
| .blend IO | It needs readfile/writefile plus the DNA versioning closure. The JSON format is the app's document format. |
| Undo | It behaves the same per step. Blender's undosys depends on Main and memfile. |
| Fallback presenter | It is only used where the GLES engine path isn't. |
| Old primitive generator, old sculpt module | Dead code. They are left for the owner of the shared JNI/backend to delete. |
| Erase fills like paint | Intentionally custom (the user's own feature). |
