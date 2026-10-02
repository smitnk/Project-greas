# Graph Report - Project-greas  (2026-10-02)

## Corpus Check
- 221 files · ~184,964 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 8 file(s) not represented in the graph (top: .xml 3, .properties 2, .jar 1)

## Summary
- 3493 nodes · 8995 edges · 221 communities (94 shown, 127 thin omitted)
- Extraction: 92% EXTRACTED · 8% INFERRED · 0% AMBIGUOUS · INFERRED: 761 edges (avg confidence: 0.84)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `0a38a869`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- FeatureId
- ProjectGreaseGPHandle
- from_handle
- project_grease_android_egl_renderer.cpp
- cmath
- project_grease_gp_backend.cpp
- math
- project_grease_legacy_sculpt.cpp
- android_gp_presentation.cpp
- GPNative
- LegacyGpBrushStrokeEngineTest
- NativeEditorBridge
- AndroidBackend
- EngineFeature
- LegacyGpSculptEngine
- ReferenceSheet.kt
- LegacyGeometryOpType
- Backend
- impl_
- ProjectGreaseUI.kt
- ProjectGreaseEglSurface.kt
- project_grease_blender_select.c
- blender_string_legacy_extract.c
- import_blender_gp.sh
- EditorControllers.kt
- android_gpu_backend.cpp
- AndroidVertBuf
- FloatArray
- .surfaceCreated
- Workflow: Native Blender GP Backend
- AndroidIndexBuf
- main
- evaluated_frame
- Test
- MainActivity.kt
- ProjectStore
- mem_guardedalloc
- StrokePoint
- AndroidBatch
- Blender 3.6.23 Legacy Grease Pencil (pinned baseline)
- bli_utildefines
- project_grease_gp_backend.h
- Native Backend Status
- ProjectGreaseDrawingSurfaceView
- Sheet
- Android GP Source Manifest (authoritative closure)
- Project Grease GP Engine Scope
- FeatureRegistry
- main
- project_grease_modifier_stack.c
- project_grease_annotations.c
- Android Shell README
- Screen
- Bundled Blender Legacy GP Engine Worklog
- android_blender_gp_gpu_drw_probe.sh
- project_grease_shader_fx.c
- bGPDlayer
- StrokeImport
- ModifierRecord
- ExportDoc
- FxRecord
- ProjectGreaseSelect
- project_grease_gp_bridge.cpp
- test_blender_edit.c
- android_gp_shader_fx.cpp
- ProjectDocumentRoundTripTest
- test_render.cc
- StrokeRecord
- DocumentNative
- JNIEXPORT
- jobject
- ReferenceScene
- project_grease_blender_edit3.c
- test_document_state.c
- ProjectGreaseGPPoint
- android_blender_gp_core_probe.sh
- android_blender_gp_minimal_buffer_backend_probe.sh
- android_blender_gp_minimal_probe.sh
- android_blender_gp_route_a_link_probe.sh
- project_grease_blender_primitive.c
- project_grease_blender_edit2.c
- bGPDstroke
- project_grease_gp_jni.cpp
- FakeDocument
- ColorMath
- NativeDocumentAdapter
- CurveSession
- .mirror
- project_grease_legacy_fill.cpp
- PGMeshLite
- project_grease_scene_lite_jni.cpp
- project_grease_scene_lite.c
- BlenderPrimitiveRulesTest
- vector
- project_grease_legacy_build.cpp
- .capability
- Tool
- project_grease_legacy_primitive.cpp
- PGCameraLite
- PolylineSession
- vgroup_at
- Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyBlenderModifier
- Settings
- .packsArguments
- BlenderSelectRulesTest
- Settings
- PGSceneLite
- TouchInputRules
- Feature table
- pg_lite_load_obj
- ProjectGreaseGPLegacyGeometryOp
- Rect
- .packsArguments
- PGFxEntry
- stdio
- BlenderColorModifierRulesTest
- .mirrorCopy
- .opacityModifier
- Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGenerateBlenderPrimitive
- LegacyPaintSettings
- jni
- .vertexPaint
- CurveSessionTest
- pge_apply_hsv
- bGPDspoint
- DrawMode
- verify_blender_verbatim.py
- apply_select_integration.py
- DocumentFrameEndTest.kt
- pge_defvert_ensure_index
- run_native_edit_tests.sh
- CLAUDE.md
- project_grease_android_set_frame_evaluator
- project_grease_android_set_fx_provider
- native_host_closure.sh
- run_native_annotation_tests.sh
- run_native_color_tests.sh
- run_native_document_state_tests.sh
- run_native_eraser_tests.sh
- run_native_modifier_stack_tests.sh
- run_native_primitive_tests.sh
- run_native_render_tests.sh
- run_native_rng_tests.sh
- run_native_scene_lite_tests.sh
- run_native_select_tests.sh
- run_native_shader_fx_tests.sh

## God Nodes (most connected - your core abstractions)
1. `Backend` - 157 edges
2. `GPNative` - 154 edges
3. `FeatureId` - 142 edges
4. `ProjectGreaseGPHandle` - 133 edges
5. `ensure_ready()` - 121 edges
6. `from_handle()` - 118 edges
7. `NativeEditorBridge` - 114 edges
8. `bGPDlayer` - 111 edges
9. `main()` - 64 edges
10. `project_grease_gp_tag()` - 63 edges

## Surprising Connections (you probably didn't know these)
- `Drawing` --references--> `PolylineSession`  [INFERRED]
  docs/PROJECT_GREASE_AUDIT_2026-10-02.md → android/app/src/main/java/com/smitnk/projectgrease/editor/BlenderPrimitiveRules.kt
- `Defects found by this audit` --references--> `FeatureRegistry`  [INFERRED]
  docs/PROJECT_GREASE_AUDIT_2026-10-02.md → android/app/src/main/java/com/smitnk/projectgrease/editor/FeatureRegistry.kt
- `Drawing` --references--> `TouchInputRules`  [INFERRED]
  docs/PROJECT_GREASE_AUDIT_2026-10-02.md → android/app/src/main/java/com/smitnk/projectgrease/editor/TouchInputRules.kt
- `Defects found by this audit` --references--> `ProjectDocumentRoundTripTest`  [INFERRED]
  docs/PROJECT_GREASE_AUDIT_2026-10-02.md → android/app/src/test/java/com/smitnk/projectgrease/editor/ProjectDocumentRoundTripTest.kt
- `Error-solving procedure (smallest missing closure)` --semantically_similar_to--> `Minimal dependency rule (evidence-driven imports)`  [INFERRED] [semantically similar]
  docs/PROJECT_GREASE_ENGINE_WORKLOG.md → PROJECT_GREASE_GP_SCOPE.md

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Android Blender GP probe workflows (core, minimal, gpu-draw, buffer-backend, native-link, route-a)** — github_workflows_android_blender_gp_core_probe, github_workflows_android_blender_gp_minimal_probe, github_workflows_android_blender_gp_gpu_draw_probe, github_workflows_android_blender_gp_minimal_buffer_backend, github_workflows_android_blender_gp_native_link_probe, github_workflows_android_blender_gp_route_a_link_probe [EXTRACTED 1.00]
- **Evidence-driven minimal Blender dependency philosophy** — project_grease_gp_scope_minimal_dependency_rule, docs_project_grease_legacy_gp_engine_plan_no_approximation_rule, docs_project_grease_engine_worklog_error_solving_procedure, native_blender_gp_readme_rules, native_blender_gp_ci_link_diagnostic [INFERRED 0.85]
- **Focused Android Legacy GP native closure (manifest + JNI CMake + link probe)** — tools_android_gp_source_manifest, android_app_src_main_cpp_cmakelists, tools_android_blender_gp_native_link_probe, native_blender_gp_project_grease_android_link_closure_notes [INFERRED 0.90]

## Communities (221 total, 127 thin omitted)

### Community 0 - "FeatureId"
Cohesion: 0.01
Nodes (134): FeatureId, ADD_FRAME, ADVANCED_FILL, ADVANCED_INTERPOLATION, ADVANCED_ONION_SKIN, ANNOTATIONS, ARC, CIRCLE (+126 more)

### Community 1 - "ProjectGreaseGPHandle"
Cohesion: 0.09
Nodes (33): main(), project_grease_gp_begin_stroke(), project_grease_gp_cancel_stroke(), project_grease_gp_create(), project_grease_gp_create_frame(), project_grease_gp_delete_frame(), project_grease_gp_delete_stroke(), project_grease_gp_destroy() (+25 more)

### Community 2 - "from_handle"
Cohesion: 0.06
Nodes (77): project_grease_gp_annotation_style(), project_grease_gp_clear_selection(), project_grease_gp_close_stroke(), project_grease_gp_delete_layer(), project_grease_gp_duplicate_frame(), project_grease_gp_duplicate_layer(), project_grease_gp_fill_stroke(), project_grease_gp_flip_stroke() (+69 more)

### Community 3 - "project_grease_android_egl_renderer.cpp"
Cohesion: 0.10
Nodes (45): attach_window(), choose_config(), connect_blender_gp(), create_context(), destroy_renderer(), detach_window(), disconnect_blender_gp(), from_handle() (+37 more)

### Community 4 - "cmath"
Cohesion: 0.14
Nodes (12): legacy_influence(), main(), legacy_pressure_curve(), legacy_spacing_accept(), main(), Point, x, y (+4 more)

### Community 5 - "project_grease_gp_backend.cpp"
Cohesion: 0.08
Nodes (56): main(), apply_blender_generator, apply_blender_modifier, apply_blender_modifier_stack, apply_legacy_geometry_batch, clear_selection, close_stroke, delete_frame (+48 more)

### Community 6 - "math"
Cohesion: 0.05
Nodes (54): BLI_lasso_is_point_inside(), closest_to_line_segment_v2(), closest_to_line_segment_v3(), closest_to_line_v2(), closest_to_line_v3(), cross_poly_v2(), dist_squared_to_line_segment_v2(), isect_line_line_v3() (+46 more)

### Community 7 - "project_grease_legacy_sculpt.cpp"
Cohesion: 0.14
Nodes (12): apply(), apply_position_smooth(), apply_strength_smooth(), apply_thickness_smooth(), Context, delta_x, delta_y, mouse_x (+4 more)

### Community 8 - "android_gp_presentation.cpp"
Cohesion: 0.14
Nodes (30): append_dot(), append_fill(), append_segment(), compile_shader(), draw_frame(), draw_frame_weights(), draw_invert_pass(), draw_sbuffer() (+22 more)

### Community 10 - "LegacyGpBrushStrokeEngineTest"
Cohesion: 0.10
Nodes (6): InputEvent, LegacyGpBrushStrokeEngine, Settings, StrokePoint, LegacyGpBrushStrokeEngineTest, Run

### Community 13 - "EngineFeature"
Cohesion: 0.07
Nodes (28): engine_feature_map(), EngineFeature, Drawing, Editing, Eraser, Fill, Frames, Interpolation (+20 more)

### Community 14 - "LegacyGpSculptEngine"
Cohesion: 0.15
Nodes (12): LegacyGpSculptEngine, Point, Settings, Tool, GRAB, PINCH, PUSH, RANDOMIZE (+4 more)

### Community 15 - "ReferenceSheet.kt"
Cohesion: 0.07
Nodes (7): ExportDialog(), rememberSvgImport(), toast(), ReferenceOverlay(), ReferenceSheet(), decodeForTrace(), TraceImageDialog()

### Community 16 - "LegacyGeometryOpType"
Cohesion: 0.07
Nodes (28): LegacyGeometryOp, flag0, flag1, int0, int1, type, value0, value1 (+20 more)

### Community 17 - "Backend"
Cohesion: 0.08
Nodes (33): Backend, add_stroke, annotations_visible, current_frame_number, frame_end, frame_numbers, get_stroke_info, hit_test_stroke (+25 more)

### Community 18 - "impl_"
Cohesion: 0.06
Nodes (40): annotation_data, document_data, history_record, history_redo, history_reset, history_undo, impl_, annotations (+32 more)

### Community 19 - "ProjectGreaseUI.kt"
Cohesion: 0.12
Nodes (27): AdvancedSheet(), AnnotationBar(), CapabilityRow(), CurveHandlesOverlay(), DrawingGuidesOverlay(), Editor(), FpsDialog(), GreaseUiState (+19 more)

### Community 21 - "project_grease_blender_select.c"
Cohesion: 0.07
Nodes (68): ED_select_op_action(), ED_select_op_action_deselected(), ED_select_op_modal(), gpencil_stroke_do_circle_sel(), pg_deselect_all_selected(), pg_generic_select_exec(), pg_generic_stroke_select(), pg_gp_select_all() (+60 more)

### Community 22 - "blender_string_legacy_extract.c"
Cohesion: 0.16
Nodes (13): BLI_snprintf(), BLI_snprintf_rlen(), BLI_string_split_name_number(), BLI_strncpy(), BLI_strncpy_utf8(), BLI_strncpy_utf8_rlen(), BLI_strnlen(), BLI_uniquename_cb() (+5 more)

### Community 23 - "import_blender_gp.sh"
Cohesion: 0.19
Nodes (15): Android JNI CMakeLists (projectgrease_jni), --no-undefined / --gc-sections link options, Workflow: Android Blender GP Core Probe, Workflow: Android Blender GP GPU DRW Closure Probe, Workflow: Android Blender GP Minimal Buffer Backend, Workflow: Android Blender GP Minimal Probe, Workflow: Android Blender GP Native Link Probe, Workflow: Android Blender GP Route-A Link Probe (+7 more)

### Community 26 - "android_gpu_backend.cpp"
Cohesion: 0.12
Nodes (9): android_backend_get(), batch_alloc, indexbuf_alloc, GPUBackend::get(), GPUBatch, project_grease_android_gpu_configure_batch(), project_grease_android_gpu_draw_batch(), project_grease_android_gp_cache_upload_probe() (+1 more)

### Community 27 - "AndroidVertBuf"
Cohesion: 0.18
Nodes (3): vertbuf_alloc, AndroidVertBuf, vbo_id_

### Community 31 - "Workflow: Native Blender GP Backend"
Cohesion: 0.26
Nodes (14): Workflow: Native Blender GP Backend, CI Link Diagnostic, Generated CMake link.txt recipe comparison (blender vs project_grease_gp_link_test), Run #28 final link failure (271 undefined refs / 113 symbols), Native blender_gp CMakeLists, curve_fit_nd extern sources, gpencil_geom_legacy.cc (BKE GP geometry), project_grease_legacy_fill/primitive/eraser/sculpt static libs (+6 more)

### Community 33 - "main"
Cohesion: 0.14
Nodes (17): main(), add_point, begin_stroke, cancel_stroke, create_document, create_frame, create_material, end_stroke (+9 more)

### Community 34 - "evaluated_frame"
Cohesion: 0.11
Nodes (20): create_polyline, create_primitive, evaluated_frame, fill_at_screen, fx_for_layer, get_point, eval_frame_trampoline(), EvalCacheEntry (+12 more)

### Community 36 - "Test"
Cohesion: 0.18
Nodes (3): LegacyGpSculptMath, LegacyGpSculptMathFidelityTest, LegacyGpSculptMathTest

### Community 37 - "MainActivity.kt"
Cohesion: 0.10
Nodes (7): MainActivity, ProjectGreaseTheme(), ProjectGreaseThemeMode, DARK, LIGHT, SYSTEM, Settings()

### Community 39 - "mem_guardedalloc"
Cohesion: 0.14
Nodes (7): CLG_log_str(), CLG_logf(), CLG_logref_init(), BKE_defvert_array_copy(), BKE_defvert_ensure_index(), BKE_defvert_find_index(), BKE_defvert_find_weight()

### Community 40 - "StrokePoint"
Cohesion: 0.15
Nodes (13): StrokePoint, a, b, g, pressure, r, strength, time (+5 more)

### Community 42 - "Blender 3.6.23 Legacy Grease Pencil (pinned baseline)"
Cohesion: 0.24
Nodes (8): Blender GP Import Manifest (docs), group_of(), select_gp(), extract_grease_pencil.sh script, Workflow: Extract Blender 3.6.23 Grease Pencil Source, Blender GP Source Import Manifest (native), Native Legacy GP Backend README, Blender 3.6.23 Legacy Grease Pencil (pinned baseline)

### Community 45 - "project_grease_gp_backend.h"
Cohesion: 0.12
Nodes (6): bGPdata, bGPDframe, project_grease_gp_dirty_tag_callback(), DRW_gpencil_batch_cache_dirty_tag(), DRW_gpencil_batch_cache_free(), first_x()

### Community 46 - "Native Backend Status"
Cohesion: 0.20
Nodes (8): Legacy GP Engine Acceptance Matrix, Verification gates for COMPLETE feature status, Final Legacy GP Engine Plan, Target dependency closure (11 tiers of Legacy GP), Feature-to-code mapping table with AVAILABLE/IN_PROGRESS/NOT_IMPLEMENTED status, Native Backend Status, FeatureRegistry (single capability source for UI), Transform selected points only (3.6.23 Legacy GP transform-conversion rule)

### Community 48 - "Sheet"
Cohesion: 0.20
Nodes (10): Sheet, ADVANCED, LAYERS, MATERIALS, MORE, NONE, ONION, PROJECT (+2 more)

### Community 49 - "Android GP Source Manifest (authoritative closure)"
Cohesion: 0.25
Nodes (9): ED_gpencil_sbuffer_ensure, GP_STROKE_BUFFER_CHUNK constant (2048), project_grease_legacy_sbuffer.c focused extraction, Real Legacy GP paint stroke buffer (sbuffer / tGPspoint) bundle, Android Link Closure Notes, Android GP Source Manifest (authoritative closure), android_gpu_backend.cpp / android_gp_presentation.cpp, CXX|/C| source entries consumed by JNI target and link probe (+1 more)

### Community 50 - "Project Grease GP Engine Scope"
Cohesion: 0.31
Nodes (7): UI / Engine Feature Mapping, Compose UI controllers (EditorController, ToolController, DocumentController, SelectionController, etc.), Project Grease GP Engine Scope, Legacy GP data model (bGPdata/bGPDlayer/bGPDframe/bGPDstroke/bGPDspoint), Production rendering path (touch -> JNI -> GP data -> draw cache -> GLES backend), Project Grease README, Android UI -> controllers -> JNI -> Legacy GP -> GLES architecture

### Community 51 - "FeatureRegistry"
Cohesion: 0.16
Nodes (12): AuditStatus, BLOCKED, COMPLETE, IN_PROGRESS, NOT_IMPLEMENTED, Entry, FeatureCapability, FeatureRegistry (+4 more)

### Community 52 - "main"
Cohesion: 0.09
Nodes (43): active_layer_data, create_layer, delete_layer, duplicate_layer, fx_add, fx_count, fx_get, fx_move (+35 more)

### Community 53 - "project_grease_modifier_stack.c"
Cohesion: 0.07
Nodes (55): noise_table(), pg_deform_noise(), pg_deform_offset(), pg_deform_simplify(), pg_deform_smooth(), pg_deform_subdiv(), pg_mod_apply(), pg_mod_defaults() (+47 more)

### Community 54 - "project_grease_annotations.c"
Cohesion: 0.16
Nodes (30): pa_frame_ensure(), pa_frame_free(), pa_frames_free(), pa_layer(), pa_link_append(), pa_link_insert_before(), pa_load_walk(), pa_open_stroke() (+22 more)

### Community 55 - "Android Shell README"
Cohesion: 0.40
Nodes (5): projectgrease_jni shared library, Android Shell README, Native EGL renderer (clear and present only), Android SurfaceView -> ANativeWindow -> EGL -> GLES2 transport, Android-owned EGL/GLES context

### Community 56 - "Screen"
Cohesion: 0.40
Nodes (5): Screen, EDITOR, HOME, NEW, SETTINGS

### Community 57 - "Bundled Blender Legacy GP Engine Worklog"
Cohesion: 0.40
Nodes (3): Bundled Blender Legacy GP Engine Worklog, project_grease_gp_engine host-facing engine boundary, Real Legacy GP modifier execution (deformStroke callbacks)

### Community 59 - "android_blender_gp_gpu_drw_probe.sh"
Cohesion: 0.83
Nodes (3): compile_one(), run_group(), android_blender_gp_gpu_drw_probe.sh script

### Community 66 - "project_grease_shader_fx.c"
Cohesion: 0.09
Nodes (61): apply_blend(), blend_mode_output(), blend_state_for_mode(), build_blur(), build_colorize(), build_flip(), build_glow(), build_pixel() (+53 more)

### Community 69 - "bGPDlayer"
Cohesion: 0.10
Nodes (59): copy_v3_v3_pge(), pg_gp_dissolve(), pg_gp_duplicate(), pg_gp_edit_delete_points(), pg_gp_edit_delete_strokes(), pg_gp_edit_dispatch(), pg_gp_edit_mirror(), pg_gp_edit_pick() (+51 more)

### Community 71 - "StrokeImport"
Cohesion: 0.05
Nodes (14): GreaseTemplates, Material, Template, ImageTrace, Fit, NewMaterial, Plan, PlannedStroke (+6 more)

### Community 72 - "ModifierRecord"
Cohesion: 0.07
Nodes (15): ModifierNative, ModifierRecord, ModifierSpecs, ModifierStackCommands, ModifierStackJson, ModifierStackPacking, ModifierType, ParamKind (+7 more)

### Community 75 - "ExportDoc"
Cohesion: 0.07
Nodes (12): AnnotationData, Frame, Parsed, VectorExport, VectorLayer, VectorPage, VectorShape, AnnotationDataTest (+4 more)

### Community 77 - "FxRecord"
Cohesion: 0.08
Nodes (9): FxCommands, FxJson, FxNative, FxPacking, FxRecord, FxSpecs, FxType, FakeFxNative (+1 more)

### Community 79 - "ProjectGreaseSelect"
Cohesion: 0.12
Nodes (4): Command, ProjectGreaseSelect, BlenderEdit2RulesTest, BlenderEdit3RulesTest

### Community 80 - "project_grease_gp_bridge.cpp"
Cohesion: 0.07
Nodes (42): ensure_ready(), project_grease_gp_apply_blender_modifier(), project_grease_gp_apply_blender_modifier_stack(), project_grease_gp_delete_last_stroke(), project_grease_gp_fill_at_screen(), project_grease_gp_frame_end(), project_grease_gp_frame_numbers(), project_grease_gp_fx_add() (+34 more)

### Community 81 - "test_blender_edit.c"
Cohesion: 0.20
Nodes (34): add_frame(), add_layer(), add_stroke(), BKE_gpencil_layer_is_editable(), BLI_lasso_boundbox(), list_add(), main(), make_gpd() (+26 more)

### Community 82 - "android_gp_shader_fx.cpp"
Cohesion: 0.10
Nodes (33): project_grease_android_present_reset(), bind_tex(), Buffer, color, reveal, compile(), draw_triangle(), ensure_convert_composite() (+25 more)

### Community 86 - "test_render.cc"
Cohesion: 0.15
Nodes (29): project_grease_android_present_set_canvas_size(), project_grease_android_present_set_view_transform(), project_grease_android_present_set_weight_view(), add_bar(), add_layer(), Doc, gpd, fx_max_diff() (+21 more)

### Community 87 - "StrokeRecord"
Cohesion: 0.10
Nodes (7): LayerRecord, MaskRecord, MaterialRecord, ParsedDocument, ParsedFrame, ParsedLayer, StrokeRecord

### Community 89 - "JNIEXPORT"
Cohesion: 0.09
Nodes (30): project_grease_gp_create_material(), project_grease_gp_create_primitive(), project_grease_gp_erase_at(), project_grease_gp_generate_primitive_preview(), project_grease_gp_hit_test_stroke(), project_grease_gp_interpolate_frame(), project_grease_gp_interpolate_frame_eased(), project_grease_gp_rotate_stroke() (+22 more)

### Community 90 - "jobject"
Cohesion: 0.09
Nodes (28): project_grease_gp_annotation_dump(), project_grease_gp_apply_legacy_geometry_batch(), project_grease_gp_fx_set_enabled(), project_grease_gp_mirror_stroke(), project_grease_gp_mirror_stroke_about(), project_grease_gp_modifier_set_enabled(), project_grease_gp_set_layer_locked(), project_grease_gp_set_layer_use_mask() (+20 more)

### Community 92 - "ReferenceScene"
Cohesion: 0.11
Nodes (3): ReferenceCamera, ReferenceScene, ReferenceCameraTest

### Community 93 - "project_grease_blender_edit3.c"
Cohesion: 0.19
Nodes (20): pe3_apply_vcolor(), pe3_clampf(), pe3_editable(), pe3_fn_bc(), pe3_fn_levels(), pe3_frames_equal(), pe3_style(), pg_gp_blank_frame_add() (+12 more)

### Community 95 - "test_document_state.c"
Cohesion: 0.16
Nodes (17): clamp01(), pg_doc_layer_info_apply(), pg_doc_layer_info_get(), pg_doc_material_info_apply(), pg_doc_material_info_get(), pg_doc_point_color_apply(), pg_doc_point_color_get(), pg_doc_stroke_info_apply() (+9 more)

### Community 98 - "ProjectGreaseGPPoint"
Cohesion: 0.08
Nodes (23): project_grease_gp_add_point(), project_grease_gp_get_point(), project_grease_gp_set_legacy_paint_settings(), project_grease_gp_set_point(), ProjectGreaseGPLegacyPaintSettings, draw_smooth_factor, draw_smooth_level, input_samples (+15 more)

### Community 109 - "project_grease_blender_primitive.c"
Cohesion: 0.17
Nodes (19): pg_anchors_valid(), pg_blender_type(), pg_generate(), pg_resolve_edges(), pg_to_region(), pg_total_points(), project_grease_blender_primitive_default_edges(), project_grease_blender_primitive_generate() (+11 more)

### Community 110 - "project_grease_blender_edit2.c"
Cohesion: 0.17
Nodes (13): pe2_clampf(), pe2_insert_point(), pe2_stroke_editable(), pe2_style(), pg_gp_edit2_dispatch(), pg_gp_extrude(), pg_gp_mod_thickness_vgroup(), pg_gp_modifier_point_weight() (+5 more)

### Community 111 - "bGPDstroke"
Cohesion: 0.12
Nodes (17): pge_length_modify_stroke(), process_stroke(), BLI_remlink(), BKE_gpencil_batch_cache_dirty_tag(), BKE_gpencil_free_stroke(), BKE_gpencil_layer_frame_delete(), BKE_gpencil_stroke_delete_tagged_points(), BKE_gpencil_stroke_duplicate() (+9 more)

### Community 112 - "project_grease_gp_jni.cpp"
Cohesion: 0.12
Nodes (21): project_grease_gp_add_stroke(), project_grease_gp_annotation_load(), project_grease_gp_apply_edit_command(), project_grease_gp_create_polyline(), project_grease_gp_fx_set_params(), project_grease_gp_get_stroke_info(), project_grease_gp_lasso_select(), project_grease_gp_modifier_set_params() (+13 more)

### Community 113 - "FakeDocument"
Cohesion: 0.09
Nodes (3): FakeDocument, Frame, Layer

### Community 114 - "ColorMath"
Cohesion: 0.19
Nodes (4): ColorMath, Hsva, BlenderColorPicker(), ColorMathTest

### Community 116 - "CurveSession"
Cohesion: 0.16
Nodes (9): CurveSession, Phase, DRAG_LINE, EDIT, IDLE, Press, CONFIRM, HANDLE (+1 more)

### Community 118 - "project_grease_legacy_fill.cpp"
Cohesion: 0.29
Nodes (12): boundary_fill(), contract_shape(), dilate_shape(), Image, rgba_, is_leak_narrow(), normalize_to_legacy_mask(), outline_points() (+4 more)

### Community 119 - "PGMeshLite"
Cohesion: 0.12
Nodes (17): PGLiteEdge, flag, tri, v, PGMeshLite, edges, totedge, totpoly (+9 more)

### Community 120 - "project_grease_scene_lite_jni.cpp"
Cohesion: 0.36
Nodes (9): Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteClear(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteCreate(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteFree(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLoadObj(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteProjectEdges(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteSetCamera(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteStats(), scene_from() (+1 more)

### Community 121 - "project_grease_scene_lite.c"
Cohesion: 0.22
Nodes (14): BKE_camera_sensor_fit(), BKE_camera_sensor_size(), cross_v3(), focallength_to_fov(), invert_m4(), lineart_matrix_ortho_44d(), lineart_matrix_perspective_44d(), normalize_v3() (+6 more)

### Community 124 - "vector"
Cohesion: 0.14
Nodes (8): Point, x, y, Result, border_contact, outline, valid, main()

### Community 125 - "project_grease_legacy_build.cpp"
Cohesion: 0.25
Nodes (4): pg_clear_stroke(), pg_fade(), pg_reduce(), project_grease_legacy_build_apply()

### Community 128 - "Tool"
Cohesion: 0.15
Nodes (12): bGPdata, bGPDframe, bGPDstroke, Tool, Grab, Pinch, Push, Randomize (+4 more)

### Community 129 - "project_grease_legacy_primitive.cpp"
Cohesion: 0.53
Nodes (9): arc(), bezier(), circle(), generate(), lerp(), line(), polyline(), rectangle() (+1 more)

### Community 130 - "PGCameraLite"
Cohesion: 0.17
Nodes (12): PGCameraLite, clip_end, clip_start, lens, matrix_world, ortho_scale, sensor_fit, sensor_x (+4 more)

### Community 131 - "PolylineSession"
Cohesion: 0.22
Nodes (4): PolylineSession, Release, CONTINUE, FINISH

### Community 132 - "vgroup_at"
Cohesion: 0.20
Nodes (9): BLI_uniquename(), set_vertex_group_active, vertex_group_active, vertex_group_add, vertex_group_count, vertex_group_name, vertex_group_remove, vertex_group_rename (+1 more)

### Community 133 - "Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyBlenderModifier"
Cohesion: 0.18
Nodes (10): project_grease_gp_apply_blender_modifier_named(), project_grease_gp_create_layer(), project_grease_gp_rename_layer(), project_grease_gp_vertex_group_add(), project_grease_gp_vertex_group_rename(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyBlenderModifier(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreateLayer(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRenameLayer() (+2 more)

### Community 134 - "Settings"
Cohesion: 0.18
Nodes (10): bGPdata, bGPDframe, bGPDstroke, Settings, draw_strength, pointer_pressure, soft, soft_strength (+2 more)

### Community 137 - "Settings"
Cohesion: 0.20
Nodes (10): Settings, apply_position, apply_strength, apply_thickness, apply_uv, brush_alpha, invert, multiframe_falloff (+2 more)

### Community 138 - "PGSceneLite"
Cohesion: 0.22
Nodes (10): mesh_free(), pg_lite_scene_clear(), pg_lite_scene_free(), pg_lite_stats(), PGSceneLite, camera, height, objects (+2 more)

### Community 140 - "Feature table"
Cohesion: 0.22
Nodes (8): Animation, Defects found by this audit, Device validation checklist (only you can produce this evidence), Feature table, Fill, paint, sculpt, Modifiers, Line Art, persistence, Project Grease audit, 2026-10-02, Selection and editing

### Community 141 - "pg_lite_load_obj"
Cohesion: 0.33
Nodes (7): grow_push(), obj_index(), pending_emit(), pending_free(), pending_init(), pg_lite_load_obj(), pg_lite_mesh_build_edges()

### Community 142 - "ProjectGreaseGPLegacyGeometryOp"
Cohesion: 0.22
Nodes (9): ProjectGreaseGPLegacyGeometryOp, flag0, flag1, int0, int1, type, value0, value1 (+1 more)

### Community 143 - "Rect"
Cohesion: 0.22
Nodes (9): Rect, a, b, g, h, r, w, x (+1 more)

### Community 145 - "PGFxEntry"
Cohesion: 0.36
Nodes (7): count_passes(), project_grease_fx_pass_count(), fx_provider_trampoline(), PGFxEntry, pg_fx_entry_init(), fx_provider(), fxe()

### Community 146 - "stdio"
Cohesion: 0.36
Nodes (5): pg_lite_project(), count_flag(), main(), test_camera(), test_obj()

### Community 151 - "LegacyPaintSettings"
Cohesion: 0.29
Nodes (7): set_legacy_paint_settings, LegacyPaintSettings, draw_smooth_factor, draw_smooth_level, input_samples, smooth_position, smooth_strength

### Community 155 - "pge_apply_hsv"
Cohesion: 0.47
Nodes (6): pe3_fn_hsv(), pg_hsv_to_rgb(), pg_rgb_to_hsv(), pge_apply_hsv(), pge_fractf(), test_hsv_conversion()

### Community 156 - "bGPDspoint"
Cohesion: 0.33
Nodes (5): pge_fn_mirror(), pge_fn_rotate(), pge_fn_scale(), pge_fn_translate(), pge_point_to_xy()

### Community 159 - "DrawMode"
Cohesion: 0.40
Nodes (5): DrawMode, DRAW_INVERT, DRAW_NORMAL, DRAW_PREMULT, DRAW_REVEALAGE

### Community 161 - "apply_select_integration.py"
Cohesion: 0.60
Nodes (3): main(), patch(), replace_once()

## Knowledge Gaps
- **459 isolated node(s):** `display`, `context`, `surface`, `window`, `width` (+454 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 872 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **127 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `GPNative` connect `GPNative` to `NativeEditorBridge`, `LegacyGpSculptEngine`, `EditorControllers.kt`, `FloatArray`, `.surfaceCreated`, `MainActivity.kt`, `LegacyGpBrushStrokeEngine.kt`, `.closeStroke`, `.deleteLastStroke`, `.duplicateLayer`, `.frameCount`, `.fxAdd`, `.fxCount`, `.fxMove`, `.layerName`, `.maskName`, `.maskSetFlags`, `.modifierAdd`, `.stats`, `.applyModifier`, `.beginStroke`, `.createFrame`, `.createMaterial`, `.deleteFrame`, `.modifierApply`, `.deleteStroke`, `.duplicateFrame`, `.modifierCount`, `.endStroke`, `.modifierMove`, `.modifierSetEnabled`, `.historyCanRedo`, `.historyCanUndo`, `.scaleStrokeAbout`, `.layerCount`, `.setPointWeight`, `.mirrorStroke`, `.nativeStrokeCount`, `.render`, `.resetDocument`, `.vertexGroupCount`, `.setMaterialFillEnabled`, `ReferenceScene`, `.splitStroke`, `.translateStroke`, `.trimStroke`?**
  _High betweenness centrality (0.051) - this node is a cross-community bridge._
- **Why does `FeatureId` connect `FeatureId` to `ProjectGreaseUI.kt`, `FeatureRegistry`, `.capability`?**
  _High betweenness centrality (0.048) - this node is a cross-community bridge._
- **Why does `NativeEditorBridge` connect `NativeEditorBridge` to `GPNative`, `LegacyGpSculptEngine`, `EditorControllers.kt`, `FloatArray`, `.closeStroke`, `.deleteLastStroke`, `.duplicateLayer`, `.frameCount`, `.fxAdd`, `.fxCount`, `.fxMove`, `.layerName`, `.maskName`, `.maskSetFlags`, `.modifierAdd`, `.stats`, `.applyModifier`, `.beginStroke`, `.createFrame`, `.createMaterial`, `.deleteFrame`, `.modifierApply`, `.deleteStroke`, `.duplicateFrame`, `.modifierCount`, `.endStroke`, `.modifierMove`, `.modifierSetEnabled`, `.historyCanRedo`, `.historyCanUndo`, `.scaleStrokeAbout`, `.layerCount`, `.setPointWeight`, `.mirrorStroke`, `ModifierRecord`, `FxRecord`, `.nativeStrokeCount`, `.render`, `.resetDocument`, `.vertexGroupCount`, `.setMaterialFillEnabled`, `.splitStroke`, `.translateStroke`, `.trimStroke`, `NativeDocumentAdapter`?**
  _High betweenness centrality (0.022) - this node is a cross-community bridge._
- **What connects `display`, `context`, `surface` to the rest of the system?**
  _459 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FeatureId` be split into smaller, more focused modules?**
  _Cohesion score 0.014925373134328358 - nodes in this community are weakly interconnected._
- **Should `ProjectGreaseGPHandle` be split into smaller, more focused modules?**
  _Cohesion score 0.08901515151515152 - nodes in this community are weakly interconnected._
- **Should `from_handle` be split into smaller, more focused modules?**
  _Cohesion score 0.05871725383920506 - nodes in this community are weakly interconnected._