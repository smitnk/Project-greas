# Graph Report - Project-greas  (2026-10-02)

## Corpus Check
- 232 files · ~215,392 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 14 file(s) not represented in the graph (top: .obj 6, .xml 3, .properties 2)

## Summary
- 3891 nodes · 9942 edges · 228 communities (115 shown, 113 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 858 edges (avg confidence: 0.84)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `29911210`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- FeatureId
- project_grease_gp_bridge.cpp
- project_grease_gp_jni.cpp
- project_grease_android_egl_renderer.cpp
- cmath
- project_grease_gp_backend.cpp
- pg_eraser_dostroke
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
- LineartTriangle
- impl_
- ProjectGreaseUI.kt
- ProjectGreaseEglSurface.kt
- project_grease_blender_select.c
- project_grease_lineart_cpu.cc
- import_blender_gp.sh
- EditorControllers.kt
- android_gpu_backend.cpp
- project_grease_android_gpu_configure_batch
- AndroidVertBuf
- FloatArray
- .surfaceCreated
- test_blender_select.c
- Workflow: Native Blender GP Backend
- AndroidIndexBuf
- main
- evaluated_frame
- assertequals
- Test
- Backend
- AnnotationData
- lineart_lite_runtime.cc
- StrokePoint
- AndroidBatch
- Blender 3.6.23 Legacy Grease Pencil (pinned baseline)
- bli_utildefines
- bGPdata
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
- test_modifier_stack.cc
- PGObjectLite
- project_grease_shader_fx.c
- lineart_scene_lite_replacements.cc
- bGPDlayer
- TaskPool
- StrokeImport
- ModifierRecord
- bGPDstroke
- LineartIsecThread
- VectorExport
- ExportDoc
- FxRecord
- ProjectGreaseSelect
- project_grease_gp_bridge.h
- test_blender_edit.c
- android_gp_shader_fx.cpp
- EdgeFeatData
- android_legacy_runtime_compat.c
- ProjectDocumentRoundTripTest
- test_render.cc
- StrokeRecord
- DocumentNative
- from_handle
- JNIEnv
- test_blender_rng.cc
- ReferenceScene
- project_grease_blender_edit3.c
- math
- BLI_spin_lock
- cstring
- ProjectGreaseGPPoint
- android_blender_gp_core_probe.sh
- android_blender_gp_minimal_buffer_backend_probe.sh
- android_blender_gp_minimal_probe.sh
- android_blender_gp_route_a_link_probe.sh
- project_grease_blender_primitive.c
- project_grease_blender_edit2.c
- MainActivity.kt
- jobject
- FakeDocument
- ColorMath
- NativeDocumentAdapter
- CurveSession
- .mirror
- Doc
- PGMeshLite
- project_grease_scene_lite_jni.cpp
- project_grease_scene_lite.c
- BlenderPrimitiveRulesTest
- CurveSessionTest
- project_grease_legacy_build.cpp
- .capability
- Context
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
- test_lineart.c
- TouchInputRules
- Feature table
- string
- ProjectGreaseGPLegacyGeometryOp
- Rect
- test_lineart_reference.c
- LineartPointTri
- .packsArguments
- BlenderColorModifierRulesTest
- .mirrorCopy
- .opacityModifier
- PGLineartSettings
- LegacyPaintSettings
- test_gp_color.c
- .vertexPaint
- lineart_create_render_buffer
- pge_clampf
- LooseEdgeData
- DrawMode
- verify_blender_verbatim.py
- apply_select_integration.py
- DocumentFrameEndTest.kt
- project_grease_gp_backend.h
- gen_lineart_lite.py
- run_native_edit_tests.sh
- run_native_lineart_tests.sh
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
- run_blender_reference.sh

## God Nodes (most connected - your core abstractions)
1. `Backend` - 157 edges
2. `GPNative` - 156 edges
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

## Communities (228 total, 113 thin omitted)

### Community 0 - "FeatureId"
Cohesion: 0.01
Nodes (134): FeatureId, ADD_FRAME, ADVANCED_FILL, ADVANCED_INTERPOLATION, ADVANCED_ONION_SKIN, ANNOTATIONS, ARC, CIRCLE (+126 more)

### Community 1 - "project_grease_gp_bridge.cpp"
Cohesion: 0.07
Nodes (67): connect_blender_gp(), main(), ensure_ready(), project_grease_gp_apply_blender_modifier(), project_grease_gp_apply_blender_modifier_stack(), project_grease_gp_begin_stroke(), project_grease_gp_cancel_stroke(), project_grease_gp_create() (+59 more)

### Community 2 - "project_grease_gp_jni.cpp"
Cohesion: 0.06
Nodes (82): project_grease_gp_close_stroke(), project_grease_gp_delete_layer(), project_grease_gp_duplicate_frame(), project_grease_gp_duplicate_layer(), project_grease_gp_fill_stroke(), project_grease_gp_flip_stroke(), project_grease_gp_fx_add(), project_grease_gp_fx_count() (+74 more)

### Community 3 - "project_grease_android_egl_renderer.cpp"
Cohesion: 0.11
Nodes (44): attach_window(), choose_config(), create_context(), destroy_renderer(), detach_window(), disconnect_blender_gp(), from_handle(), initialize_egl() (+36 more)

### Community 4 - "cmath"
Cohesion: 0.21
Nodes (9): legacy_influence(), main(), legacy_pressure_curve(), legacy_spacing_accept(), main(), main(), near(), influence() (+1 more)

### Community 5 - "project_grease_gp_backend.cpp"
Cohesion: 0.08
Nodes (49): main(), apply_blender_generator, apply_blender_modifier, apply_blender_modifier_stack, clear_selection, close_stroke, delete_frame, delete_last_stroke (+41 more)

### Community 6 - "pg_eraser_dostroke"
Cohesion: 0.10
Nodes (33): BLI_lasso_is_point_inside(), closest_to_line_segment_v2(), closest_to_line_segment_v3(), closest_to_line_v2(), closest_to_line_v3(), cross_poly_v2(), dist_squared_to_line_segment_v2(), isect_line_line_v3() (+25 more)

### Community 7 - "project_grease_legacy_sculpt.cpp"
Cohesion: 0.26
Nodes (5): apply(), apply_position_smooth(), apply_strength_smooth(), apply_thickness_smooth(), influence()

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

### Community 17 - "LineartTriangle"
Cohesion: 0.10
Nodes (28): lineart_add_isec_thread(), lineart_discard_duplicated_edges(), lineart_edge_cut(), lineart_edge_from_triangle(), lineart_edge_match(), lineart_occlusion_is_adjacent_intersection(), lineart_occlusion_single_line(), lineart_point_inside_triangle3d() (+20 more)

### Community 18 - "impl_"
Cohesion: 0.07
Nodes (33): history_record, history_reset, impl_, annotations, annotations_visible, document_created, eval_cache, eval_count (+25 more)

### Community 19 - "ProjectGreaseUI.kt"
Cohesion: 0.12
Nodes (28): AdvancedSheet(), AnnotationBar(), CapabilityRow(), CurveHandlesOverlay(), DrawingGuidesOverlay(), Editor(), FpsDialog(), GreaseUiState (+20 more)

### Community 21 - "project_grease_blender_select.c"
Cohesion: 0.10
Nodes (40): ED_select_op_action(), ED_select_op_action_deselected(), ED_select_op_modal(), gpencil_stroke_do_circle_sel(), pg_deselect_all_selected(), pg_generic_select_exec(), pg_generic_stroke_select(), pg_gp_select_all() (+32 more)

### Community 22 - "project_grease_lineart_cpu.cc"
Cohesion: 0.08
Nodes (57): BLI_spin_end(), BLI_task_pool_create(), pg_lineart_compute(), pg_lineart_compute_occlusion(), EdgeFeatReduceData, feat_edges, lineart_bounding_area_edge_intersect(), lineart_bounding_area_line_add() (+49 more)

### Community 23 - "import_blender_gp.sh"
Cohesion: 0.19
Nodes (15): Android JNI CMakeLists (projectgrease_jni), --no-undefined / --gc-sections link options, Workflow: Android Blender GP Core Probe, Workflow: Android Blender GP GPU DRW Closure Probe, Workflow: Android Blender GP Minimal Buffer Backend, Workflow: Android Blender GP Minimal Probe, Workflow: Android Blender GP Native Link Probe, Workflow: Android Blender GP Route-A Link Probe (+7 more)

### Community 26 - "project_grease_android_gpu_configure_batch"
Cohesion: 0.29
Nodes (5): GPUBatch, project_grease_android_gpu_configure_batch(), project_grease_android_gpu_draw_batch(), project_grease_android_gp_cache_upload_probe(), project_grease_android_gpu_buffer_backend_probe()

### Community 27 - "AndroidVertBuf"
Cohesion: 0.18
Nodes (3): vertbuf_alloc, AndroidVertBuf, vbo_id_

### Community 30 - "test_blender_select.c"
Cohesion: 0.07
Nodes (41): BLI_snprintf(), BLI_snprintf_rlen(), BLI_string_split_name_number(), BLI_strncpy(), BLI_strncpy_utf8(), BLI_strncpy_utf8_rlen(), BLI_strnlen(), BLI_uniquename_cb() (+33 more)

### Community 31 - "Workflow: Native Blender GP Backend"
Cohesion: 0.26
Nodes (14): Workflow: Native Blender GP Backend, CI Link Diagnostic, Generated CMake link.txt recipe comparison (blender vs project_grease_gp_link_test), Run #28 final link failure (271 undefined refs / 113 symbols), Native blender_gp CMakeLists, curve_fit_nd extern sources, gpencil_geom_legacy.cc (BKE GP geometry), project_grease_legacy_fill/primitive/eraser/sculpt static libs (+6 more)

### Community 32 - "AndroidIndexBuf"
Cohesion: 0.16
Nodes (3): indexbuf_alloc, AndroidIndexBuf, ibo_id_

### Community 33 - "main"
Cohesion: 0.12
Nodes (20): main(), add_point, add_stroke, begin_stroke, cancel_stroke, create_document, create_frame, create_layer (+12 more)

### Community 34 - "evaluated_frame"
Cohesion: 0.11
Nodes (22): create_polyline, create_primitive, evaluated_frame, fill_at_screen, fx_for_layer, render, render_external_context, render_with_gpu_context (+14 more)

### Community 36 - "Test"
Cohesion: 0.18
Nodes (3): LegacyGpSculptMath, LegacyGpSculptMathFidelityTest, LegacyGpSculptMathTest

### Community 37 - "Backend"
Cohesion: 0.10
Nodes (28): Backend, annotations_visible, current_frame_number, frame_end, frame_numbers, hit_test_stroke, material_count, mirror_stroke (+20 more)

### Community 38 - "AnnotationData"
Cohesion: 0.15
Nodes (6): AnnotationData, Frame, Parsed, ProjectRecord, ProjectStore, AnnotationDataTest

### Community 39 - "lineart_lite_runtime.cc"
Cohesion: 0.10
Nodes (7): lineart_find_matching_edge(), lineart_find_matching_eln(), lineart_main_make_enclosed_shapes(), lineart_main_transform_and_add_shadow(), lineart_main_try_generate_shadow(), lineart_register_intersection_shadow_cuts(), lineart_register_shadow_cuts()

### Community 40 - "StrokePoint"
Cohesion: 0.15
Nodes (13): StrokePoint, a, b, g, pressure, r, strength, time (+5 more)

### Community 41 - "AndroidBatch"
Cohesion: 0.16
Nodes (3): batch_alloc, AndroidBatch, vao_id_

### Community 42 - "Blender 3.6.23 Legacy Grease Pencil (pinned baseline)"
Cohesion: 0.24
Nodes (8): Blender GP Import Manifest (docs), group_of(), select_gp(), extract_grease_pencil.sh script, Workflow: Extract Blender 3.6.23 Grease Pencil Source, Blender GP Source Import Manifest (native), Native Legacy GP Backend README, Blender 3.6.23 Legacy Grease Pencil (pinned baseline)

### Community 45 - "bGPdata"
Cohesion: 0.11
Nodes (14): annotation_data, document_data, history_redo, history_undo, shutdown, eval_cache_clear(), gp_materials_free(), history_copy_settings() (+6 more)

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
Cohesion: 0.08
Nodes (45): active_layer_data, delete_layer, duplicate_layer, fx_add, fx_count, fx_get, fx_move, fx_remove (+37 more)

### Community 53 - "project_grease_modifier_stack.c"
Cohesion: 0.14
Nodes (23): noise_table(), pg_deform_noise(), pg_deform_offset(), pg_deform_simplify(), pg_deform_smooth(), pg_deform_subdiv(), pg_mod_apply(), pg_mod_defaults() (+15 more)

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

### Community 61 - "test_modifier_stack.cc"
Cohesion: 0.34
Nodes (21): entry(), eval(), free_doc(), main(), make_doc(), make_pair_doc(), near(), Pt (+13 more)

### Community 65 - "PGObjectLite"
Cohesion: 0.12
Nodes (20): lineart_geometry_check_visible(), lineart_intersection_mask_check(), lineart_intersection_priority_check(), lineart_object_load_single_instance(), lineart_usage_check(), lineart_add_edge_to_array_thread(), lineart_geometry_check_visible(), lineart_geometry_load_assign_thread() (+12 more)

### Community 66 - "project_grease_shader_fx.c"
Cohesion: 0.08
Nodes (67): fx_provider_trampoline(), PGFxEntry, apply_blend(), blend_mode_output(), blend_state_for_mode(), build_blur(), build_colorize(), build_flip() (+59 more)

### Community 68 - "lineart_scene_lite_replacements.cc"
Cohesion: 0.10
Nodes (23): lineart_edge_neighbor_init_task(), lineart_geometry_object_load(), lineart_gpencil_generate(), lineart_identify_mlooptri_feature_edges(), lineart_load_tri_task(), pg_lineart_compute_strokes(), pg_lineart_free_strokes(), pg_lineart_object_index() (+15 more)

### Community 69 - "bGPDlayer"
Cohesion: 0.09
Nodes (61): copy_v3_v3_pge(), pg_gp_dissolve(), pg_gp_duplicate(), pg_gp_edit_delete_points(), pg_gp_edit_delete_strokes(), pg_gp_edit_dispatch(), pg_gp_edit_mirror(), pg_gp_edit_pick() (+53 more)

### Community 70 - "TaskPool"
Cohesion: 0.12
Nodes (20): BLI_task_pool_free(), BLI_task_pool_push(), BLI_task_pool_user_data(), BLI_task_pool_work_and_wait(), PGTask, data, free_data, free_fn (+12 more)

### Community 71 - "StrokeImport"
Cohesion: 0.05
Nodes (14): GreaseTemplates, Material, Template, ImageTrace, Fit, NewMaterial, Plan, PlannedStroke (+6 more)

### Community 72 - "ModifierRecord"
Cohesion: 0.08
Nodes (15): ModifierNative, ModifierRecord, ModifierSpecs, ModifierStackCommands, ModifierStackJson, ModifierStackPacking, ModifierType, ParamKind (+7 more)

### Community 73 - "bGPDstroke"
Cohesion: 0.13
Nodes (18): pge_length_modify_stroke(), apply_legacy_geometry_batch, process_stroke(), BKE_gpencil_batch_cache_dirty_tag(), BKE_gpencil_free_stroke(), BKE_gpencil_stroke_delete_tagged_points(), BKE_gpencil_stroke_duplicate(), BKE_gpencil_stroke_flip() (+10 more)

### Community 74 - "LineartIsecThread"
Cohesion: 0.09
Nodes (22): lineart_add_edge_to_array(), lineart_create_edges_from_isec_data(), LineartIsecData, ld, thread_count, threads, LineartIsecSingle, tri1 (+14 more)

### Community 75 - "VectorExport"
Cohesion: 0.16
Nodes (7): VectorExport, VectorLayer, VectorPage, VectorShape, Frame, Layer, VectorExportTest

### Community 77 - "FxRecord"
Cohesion: 0.09
Nodes (9): FxCommands, FxJson, FxNative, FxPacking, FxRecord, FxSpecs, FxType, FakeFxNative (+1 more)

### Community 79 - "ProjectGreaseSelect"
Cohesion: 0.11
Nodes (3): Command, ProjectGreaseSelect, BlenderEdit3RulesTest

### Community 80 - "project_grease_gp_bridge.h"
Cohesion: 0.15
Nodes (10): project_grease_gp_set_legacy_paint_settings(), ProjectGreaseGPLegacyPaintSettings, draw_smooth_factor, draw_smooth_level, input_samples, smooth_position, smooth_strength, ProjectGreaseGPStrokeStyle (+2 more)

### Community 81 - "test_blender_edit.c"
Cohesion: 0.19
Nodes (36): add_frame(), add_layer(), add_stroke(), BKE_gpencil_layer_frame_delete(), BKE_gpencil_layer_is_editable(), BLI_lasso_boundbox(), list_add(), main() (+28 more)

### Community 82 - "android_gp_shader_fx.cpp"
Cohesion: 0.09
Nodes (35): project_grease_android_present_reset(), bind_tex(), Buffer, color, reveal, compile(), count_passes(), draw_triangle() (+27 more)

### Community 83 - "EdgeFeatData"
Cohesion: 0.11
Nodes (17): EdgeFeatData, crease_threshold, edge_nabr, ld, material_indices, me, tri_array, use_auto_smooth (+9 more)

### Community 84 - "android_legacy_runtime_compat.c"
Cohesion: 0.20
Nodes (3): CLG_log_str(), CLG_logf(), CLG_logref_init()

### Community 86 - "test_render.cc"
Cohesion: 0.17
Nodes (28): project_grease_android_present_set_canvas_size(), project_grease_android_present_set_view_transform(), project_grease_android_present_set_weight_view(), add_bar(), add_layer(), Doc, gpd, fxe() (+20 more)

### Community 87 - "StrokeRecord"
Cohesion: 0.14
Nodes (6): MaterialRecord, ParsedDocument, ParsedFrame, ParsedLayer, ProjectDocumentCodec, StrokeRecord

### Community 89 - "from_handle"
Cohesion: 0.08
Nodes (33): project_grease_gp_annotation_dump(), project_grease_gp_create_primitive(), project_grease_gp_erase_at(), project_grease_gp_generate_primitive_preview(), project_grease_gp_hit_test_stroke(), project_grease_gp_interpolate_frame(), project_grease_gp_interpolate_frame_eased(), project_grease_gp_rotate_stroke() (+25 more)

### Community 90 - "JNIEnv"
Cohesion: 0.09
Nodes (28): project_grease_gp_apply_legacy_geometry_batch(), project_grease_gp_clear_selection(), project_grease_gp_fx_set_enabled(), project_grease_gp_mirror_stroke(), project_grease_gp_mirror_stroke_about(), project_grease_gp_modifier_set_enabled(), project_grease_gp_set_layer_locked(), project_grease_gp_set_layer_use_mask() (+20 more)

### Community 91 - "test_blender_rng.cc"
Cohesion: 0.16
Nodes (5): main(), test_halton(), test_hash(), test_rng_floats_and_copy(), test_rng_ints()

### Community 92 - "ReferenceScene"
Cohesion: 0.10
Nodes (3): ReferenceCamera, ReferenceScene, ReferenceCameraTest

### Community 93 - "project_grease_blender_edit3.c"
Cohesion: 0.19
Nodes (20): pe3_apply_vcolor(), pe3_clampf(), pe3_editable(), pe3_fn_bc(), pe3_fn_levels(), pe3_frames_equal(), pe3_style(), pg_gp_blank_frame_add() (+12 more)

### Community 95 - "math"
Cohesion: 0.07
Nodes (33): clamp01(), pg_doc_layer_info_apply(), pg_doc_layer_info_get(), pg_doc_material_info_apply(), pg_doc_material_info_get(), pg_doc_point_color_apply(), pg_doc_point_color_get(), pg_doc_stroke_info_apply() (+25 more)

### Community 96 - "BLI_spin_lock"
Cohesion: 0.33
Nodes (7): BLI_spin_lock(), BLI_spin_unlock(), lineart_discard_segment(), lineart_give_segment(), lineart_occlusion_make_task_info(), lineart_occlusion_worker(), lineart_schedule_new_triangle_task()

### Community 97 - "cstring"
Cohesion: 0.39
Nodes (4): BKE_defvert_array_copy(), BKE_defvert_ensure_index(), BKE_defvert_find_index(), BKE_defvert_find_weight()

### Community 98 - "ProjectGreaseGPPoint"
Cohesion: 0.11
Nodes (17): project_grease_gp_add_point(), project_grease_gp_add_stroke(), project_grease_gp_get_point(), project_grease_gp_get_stroke_info(), project_grease_gp_set_point(), ProjectGreaseGPPoint, pressure, strength (+9 more)

### Community 109 - "project_grease_blender_primitive.c"
Cohesion: 0.05
Nodes (41): Java_com_smitnk_projectgrease_nativebridge_GPNative_nativePing(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGenerateBlenderPrimitive(), pg_anchors_valid(), pg_blender_type(), pg_generate(), pg_resolve_edges(), pg_to_region(), pg_total_points() (+33 more)

### Community 110 - "project_grease_blender_edit2.c"
Cohesion: 0.17
Nodes (12): pe2_clampf(), pe2_insert_point(), pe2_stroke_editable(), pe2_style(), pg_gp_edit2_dispatch(), pg_gp_extrude(), pg_gp_mod_thickness_vgroup(), pg_gp_modifier_point_weight() (+4 more)

### Community 111 - "MainActivity.kt"
Cohesion: 0.10
Nodes (6): MainActivity, ProjectGreaseTheme(), ProjectGreaseThemeMode, DARK, LIGHT, SYSTEM

### Community 112 - "jobject"
Cohesion: 0.14
Nodes (18): project_grease_gp_annotation_load(), project_grease_gp_annotation_style(), project_grease_gp_apply_edit_command(), project_grease_gp_create_polyline(), project_grease_gp_fx_set_params(), project_grease_gp_lasso_select(), project_grease_gp_modifier_set_params(), project_grease_gp_set_material_colors() (+10 more)

### Community 113 - "FakeDocument"
Cohesion: 0.09
Nodes (3): FakeDocument, Frame, Layer

### Community 114 - "ColorMath"
Cohesion: 0.20
Nodes (4): ColorMath, Hsva, BlenderColorPicker(), ColorMathTest

### Community 115 - "NativeDocumentAdapter"
Cohesion: 0.07
Nodes (3): NativeDocumentAdapter, LayerRecord, MaskRecord

### Community 116 - "CurveSession"
Cohesion: 0.16
Nodes (9): CurveSession, Phase, DRAG_LINE, EDIT, IDLE, Press, CONFIRM, HANDLE (+1 more)

### Community 118 - "Doc"
Cohesion: 0.25
Nodes (6): add_stroke(), Doc, f1, f5, gpd, gpl

### Community 119 - "PGMeshLite"
Cohesion: 0.04
Nodes (48): BLI_task_parallel_range(), EdgeNeighborData, adj_e, edge_nabr, me, lineart_build_edge_neighbor(), EdgeFeatData, crease_threshold (+40 more)

### Community 120 - "project_grease_scene_lite_jni.cpp"
Cohesion: 0.31
Nodes (11): Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteClear(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteCreate(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteFree(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArt(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArtStrokes(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLoadObj(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteProjectEdges(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteSetCamera() (+3 more)

### Community 121 - "project_grease_scene_lite.c"
Cohesion: 0.19
Nodes (16): BKE_camera_sensor_fit(), BKE_camera_sensor_size(), focallength_to_fov(), grow_push(), invert_m4(), lineart_matrix_ortho_44d(), lineart_matrix_perspective_44d(), mesh_free() (+8 more)

### Community 125 - "project_grease_legacy_build.cpp"
Cohesion: 0.25
Nodes (4): pg_clear_stroke(), pg_fade(), pg_reduce(), project_grease_legacy_build_apply()

### Community 127 - "Context"
Cohesion: 0.17
Nodes (10): bGPdata, bGPDframe, bGPDstroke, Context, delta_x, delta_y, mouse_x, mouse_y (+2 more)

### Community 128 - "Tool"
Cohesion: 0.22
Nodes (9): Tool, Grab, Pinch, Push, Randomize, Smooth, Strength, Thickness (+1 more)

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

### Community 138 - "test_lineart.c"
Cohesion: 0.36
Nodes (10): pg_lite_scene_clear(), pg_lite_scene_free(), main(), run(), scene_with(), test_cube(), test_intersections(), test_loose_and_empty() (+2 more)

### Community 140 - "Feature table"
Cohesion: 0.22
Nodes (8): Animation, Defects found by this audit, Device validation checklist (only you can produce this evidence), Feature table, Fill, paint, sculpt, Modifiers, Line Art, persistence, Project Grease audit, 2026-10-02, Selection and editing

### Community 141 - "string"
Cohesion: 0.18
Nodes (10): cross_v3(), pg_lite_camera_default(), pg_lite_camera_orbit(), pg_lite_project(), pg_lite_scene_create(), pg_lite_stats(), count_flag(), main() (+2 more)

### Community 142 - "ProjectGreaseGPLegacyGeometryOp"
Cohesion: 0.22
Nodes (9): ProjectGreaseGPLegacyGeometryOp, flag0, flag1, int0, int1, type, value0, value1 (+1 more)

### Community 143 - "Rect"
Cohesion: 0.22
Nodes (9): Rect, a, b, g, h, r, w, x (+1 more)

### Community 144 - "test_lineart_reference.c"
Cohesion: 0.26
Nodes (10): compare_scene(), compare_strokes(), dist_point_seg(), load_reference(), load_world_strokes(), nearest(), pg_lineart_reference_compare(), push() (+2 more)

### Community 145 - "LineartPointTri"
Cohesion: 0.33
Nodes (6): lineart_point_on_line_segment(), lineart_point_triangle_relation(), LineartPointTri, LRT_INSIDE_TRIANGLE, LRT_ON_TRIANGLE, LRT_OUTSIDE_TRIANGLE

### Community 150 - "PGLineartSettings"
Cohesion: 0.06
Nodes (32): pg_lineart_free_segments(), pg_lineart_settings_default(), pg_lineart_free_segments(), pg_lineart_settings_default(), PGLineartSegment, edge_index, edge_type, object_index (+24 more)

### Community 151 - "LegacyPaintSettings"
Cohesion: 0.29
Nodes (7): set_legacy_paint_settings, LegacyPaintSettings, draw_smooth_factor, draw_smooth_level, input_samples, smooth_position, smooth_strength

### Community 152 - "test_gp_color.c"
Cohesion: 0.40
Nodes (8): pg_gp_clamp01(), pg_gp_mix_vertex_color(), pg_gp_stroke_mean_mix(), main(), near3(), test_fill(), test_mean_mix(), test_mix()

### Community 154 - "lineart_create_render_buffer"
Cohesion: 0.60
Nodes (5): BLI_spin_init(), lineart_create_render_buffer(), lineart_create_render_buffer(), normalize_v3(), pg_lite_camera_shift()

### Community 155 - "pge_clampf"
Cohesion: 0.27
Nodes (10): pe3_fn_hsv(), pg_gp_interpolate_easing(), pg_hsv_to_rgb(), pg_rgb_to_hsv(), pge_apply_hsv(), pge_bounce_out(), pge_clampf(), pge_ease_in() (+2 more)

### Community 156 - "LooseEdgeData"
Cohesion: 0.67
Nodes (3): LooseEdgeData, loose_array, loose_count

### Community 159 - "DrawMode"
Cohesion: 0.40
Nodes (5): DrawMode, DRAW_INVERT, DRAW_NORMAL, DRAW_PREMULT, DRAW_REVEALAGE

### Community 161 - "apply_select_integration.py"
Cohesion: 0.60
Nodes (3): main(), patch(), replace_once()

### Community 163 - "project_grease_gp_backend.h"
Cohesion: 0.18
Nodes (5): bGPdata, bGPDframe, Point, x, y

### Community 164 - "gen_lineart_lite.py"
Cohesion: 0.36
Nodes (4): generate(), item_spans(), main(), replacement_sections()

## Knowledge Gaps
- **560 isolated node(s):** `display`, `context`, `surface`, `window`, `width` (+555 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1005 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **113 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `GPNative` connect `GPNative` to `NativeEditorBridge`, `LegacyGpSculptEngine`, `EditorControllers.kt`, `FloatArray`, `.surfaceCreated`, `.fillStroke`, `.frameEnd`, `LegacyGpBrushStrokeEngine.kt`, `.fxCount`, `.hitTestStroke`, `.layerCount`, `.duplicateLayer`, `.layerUseMask`, `.fxAdd`, `.maskName`, `.materialCount`, `.modifierAdd`, `.modifierApply`, `.stats`, `.modifierMove`, `.beginStroke`, `.createFrame`, `.createMaterial`, `.renameLayer`, `.resetDocument`, `.deleteStroke`, `.interpolateFrame`, `.rotateStrokeAbout`, `.setLayerUseMask`, `.setVertexGroupActive`, `ReferenceScene`, `.nativeStrokeCount`, `.splitStroke`, `.translateStroke`, `.moveLayer`, `.vertexGroupRemove`, `.setOnionSkin`, `.subdivideStroke`, `MainActivity.kt`?**
  _High betweenness centrality (0.043) - this node is a cross-community bridge._
- **Why does `FeatureId` connect `FeatureId` to `ProjectGreaseUI.kt`, `FeatureRegistry`, `.capability`?**
  _High betweenness centrality (0.042) - this node is a cross-community bridge._
- **Why does `NativeEditorBridge` connect `NativeEditorBridge` to `GPNative`, `LegacyGpSculptEngine`, `EditorControllers.kt`, `FloatArray`, `.fillStroke`, `.frameEnd`, `.fxCount`, `.hitTestStroke`, `.layerCount`, `.duplicateLayer`, `.layerUseMask`, `.fxAdd`, `.maskName`, `.materialCount`, `.modifierAdd`, `.modifierApply`, `.stats`, `.modifierMove`, `.beginStroke`, `.createFrame`, `.createMaterial`, `.renameLayer`, `.resetDocument`, `.deleteStroke`, `.interpolateFrame`, `.rotateStrokeAbout`, `.setLayerUseMask`, `.setVertexGroupActive`, `ModifierRecord`, `FxRecord`, `.nativeStrokeCount`, `.splitStroke`, `.translateStroke`, `.moveLayer`, `.vertexGroupRemove`, `.setOnionSkin`, `.subdivideStroke`, `NativeDocumentAdapter`?**
  _High betweenness centrality (0.019) - this node is a cross-community bridge._
- **What connects `display`, `context`, `surface` to the rest of the system?**
  _560 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FeatureId` be split into smaller, more focused modules?**
  _Cohesion score 0.014925373134328358 - nodes in this community are weakly interconnected._
- **Should `project_grease_gp_bridge.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.06716417910447761 - nodes in this community are weakly interconnected._
- **Should `project_grease_gp_jni.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.05512016718913271 - nodes in this community are weakly interconnected._