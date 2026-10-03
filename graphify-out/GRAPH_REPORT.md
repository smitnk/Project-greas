# Graph Report - Project-greas  (2026-10-03)

## Corpus Check
- 259 files · ~261,897 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 14 file(s) not represented in the graph (top: .obj 6, .xml 3, .properties 2)

## Summary
- 4346 nodes · 11473 edges · 244 communities (117 shown, 127 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 1080 edges (avg confidence: 0.84)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `699b0422`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- FeatureId
- project_grease_gp_bridge.cpp
- from_handle
- project_grease_android_egl_renderer.cpp
- Backend
- project_grease_gp_backend.cpp
- test_blender_eraser.c
- project_grease_legacy_sculpt.cpp
- android_gp_presentation.cpp
- GPNative
- test_tool_session.c
- NativeEditorBridge
- AndroidBackend
- EngineFeature
- project_grease_tool_util.c
- ReferenceSheet.kt
- LegacyGeometryOpType
- LineartTriangle
- impl_
- ProjectGreaseUI.kt
- ProjectGreaseEglSurface.kt
- test_blender_select.c
- lineart_scene_lite_replacements.cc
- import_blender_gp.sh
- EditorControllers.kt
- android_gpu_backend.cpp
- project_grease_android_gpu_configure_batch
- AndroidVertBuf
- FloatArray
- .surfaceCreated
- project_grease_tool_sculpt.c
- Workflow: Native Blender GP Backend
- AndroidIndexBuf
- main
- test_render.cc
- assertequals
- project_grease_tool_weight_paint.c
- project_grease_blender_select.c
- TaskPool
- lineart_lite_runtime.cc
- StrokePoint
- AndroidBatch
- Blender 3.6.23 Legacy Grease Pencil (pinned baseline)
- GenerateGolden.kt
- project_grease_shader_fx.c
- FakeDocument
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
- pg_fx_build_passes
- math
- blender_reference.py
- ProjectGreaseGPHandle
- test_shader_fx.cc
- blender_string_legacy_extract.c
- cmath
- project_grease_blender_edit.c
- project_grease_gp_jni.cpp
- StrokeImport
- ModifierRecord
- bGPDstroke
- LineartIsecThread
- VectorExport
- project_grease_tool_vertex_paint.c
- FxRecord
- jobject
- ProjectGreaseSelect
- ProjectGreaseGPPoint
- test_blender_edit.c
- android_gp_shader_fx.cpp
- EdgeFeatData
- project_grease_blender_edit4.c
- ProjectDocumentRoundTripTest
- project_grease_gp_backend.h
- StrokeRecord
- DocumentNative
- pg_sculpt_session_begin
- .from
- LegacyGpBrushStrokeEngine
- ReferenceScene
- bGPDlayer
- AnnotationData
- project_grease_blender_edit5.c
- DeviceBugfixTest
- ImportDialogs.kt
- android_blender_gp_core_probe.sh
- android_blender_gp_minimal_buffer_backend_probe.sh
- android_blender_gp_minimal_probe.sh
- android_blender_gp_route_a_link_probe.sh
- project_grease_blender_primitive.c
- project_grease_blender_edit2.c
- MainActivity.kt
- bGPdata
- ExportDoc
- ColorMath
- NativeDocumentAdapter
- CurveSession
- .mirror
- .segmentModeIsClickOnlyAndAreaSelectUsesPoint
- PGMeshLite
- project_grease_scene_lite_jni.cpp
- project_grease_scene_lite.c
- blender_math_geom_legacy_extract.cc
- pg_eraser_dostroke
- .finite
- dna_gpencil_legacy_types
- .capability
- ToolSession
- Tool
- .packsArguments
- PGCameraLite
- bli_utildefines
- BlenderPrimitiveRulesTest
- PGFxEntry
- Settings
- .packsArguments
- BlenderSelectRulesTest
- PolylineSession
- stdio
- .pressureFor
- Feature table
- ProjectGreaseGPLegacyGeometryOp
- mem_guardedalloc
- test_lineart_reference.c
- Settings
- BlenderColorModifierRulesTest
- .mirrorCopy
- .opacityModifier
- PGObjectLite
- LegacyPaintSettings
- BlenderPrimitiveRules.kt
- .vertexPaint
- LineartPointTri
- .packsArguments
- Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyBlenderModifier
- lineart_geometry_object_load
- verify_blender_verbatim.py
- apply_select_integration.py
- DocumentFrameEndTest.kt
- evaluated_frame
- gen_lineart_lite.py
- CurveSessionTest
- BLI_math_vector.h
- .projectName
- Phase
- BrushSizeTest.kt
- project_grease_lineart_cpu.cc
- Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGetMaterialInfo
- LooseEdgeData
- JNIEnv
- run_native_edit_tests.sh
- run_native_tool_session_tests.sh
- run_native_edit5_tests.sh
- run_native_lineart_tests.sh
- CLAUDE.md
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
- run_native_stroke_outline_tests.sh
- generate.sh
- run_blender_reference.sh
- BLI_lasso_boundbox

## God Nodes (most connected - your core abstractions)
1. `GPNative` - 160 edges
2. `Backend` - 158 edges
3. `FeatureId` - 148 edges
4. `ProjectGreaseGPHandle` - 139 edges
5. `bGPDlayer` - 132 edges
6. `ensure_ready()` - 123 edges
7. `from_handle()` - 119 edges
8. `NativeEditorBridge` - 115 edges
9. `ProjectGreaseSelect` - 78 edges
10. `Command` - 68 edges

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

## Communities (244 total, 127 thin omitted)

### Community 0 - "FeatureId"
Cohesion: 0.01
Nodes (140): FeatureId, ADD_FRAME, ADVANCED_FILL, ADVANCED_INTERPOLATION, ADVANCED_ONION_SKIN, ANNOTATIONS, ARC, CIRCLE (+132 more)

### Community 1 - "project_grease_gp_bridge.cpp"
Cohesion: 0.07
Nodes (41): ensure_ready(), project_grease_gp_apply_blender_modifier(), project_grease_gp_apply_blender_modifier_stack(), project_grease_gp_create_material(), project_grease_gp_delete_last_stroke(), project_grease_gp_fill_at_screen(), project_grease_gp_frame_end(), project_grease_gp_frame_numbers() (+33 more)

### Community 2 - "from_handle"
Cohesion: 0.06
Nodes (79): project_grease_gp_annotation_dump(), project_grease_gp_close_stroke(), project_grease_gp_delete_layer(), project_grease_gp_duplicate_frame(), project_grease_gp_duplicate_layer(), project_grease_gp_fill_stroke(), project_grease_gp_flip_stroke(), project_grease_gp_fx_add() (+71 more)

### Community 3 - "project_grease_android_egl_renderer.cpp"
Cohesion: 0.09
Nodes (53): attach_window(), choose_config(), connect_blender_gp(), create_context(), destroy_renderer(), detach_window(), disconnect_blender_gp(), from_handle() (+45 more)

### Community 4 - "Backend"
Cohesion: 0.07
Nodes (39): Backend, annotations_visible, current_frame_number, frame_end, frame_numbers, frame_selected_point_count, get_layer_info, hit_test_stroke (+31 more)

### Community 5 - "project_grease_gp_backend.cpp"
Cohesion: 0.07
Nodes (53): main(), apply_blender_generator, apply_blender_modifier, apply_blender_modifier_stack, clear_selection, close_stroke, delete_frame, delete_last_stroke (+45 more)

### Community 6 - "test_blender_eraser.c"
Cohesion: 0.32
Nodes (13): main(), make_stroke(), params(), tag_mask(), test_hard_eraser(), test_segment_spans_previous_point(), test_soft_cull_uses_blender_thresholds(), test_soft_eraser() (+5 more)

### Community 7 - "project_grease_legacy_sculpt.cpp"
Cohesion: 0.14
Nodes (12): apply(), apply_position_smooth(), apply_strength_smooth(), apply_thickness_smooth(), Context, delta_x, delta_y, mouse_x (+4 more)

### Community 8 - "android_gp_presentation.cpp"
Cohesion: 0.05
Nodes (77): append_fill(), append_outline(), append_stroke_outline(), compile_shader(), coverage_begin(), coverage_end(), draw_frame(), draw_frame_weights() (+69 more)

### Community 10 - "test_tool_session.c"
Cohesion: 0.07
Nodes (57): add_arc_points(), add_fake_points(), angle_between(), append_point(), ensure(), fclamp(), filter_mval(), hold_back() (+49 more)

### Community 13 - "EngineFeature"
Cohesion: 0.07
Nodes (28): engine_feature_map(), EngineFeature, Drawing, Editing, Eraser, Fill, Frames, Interpolation (+20 more)

### Community 14 - "project_grease_tool_util.c"
Cohesion: 0.18
Nodes (18): BKE_boundbox_init_from_minmax(), ED_gpencil_projected_2d_bound_box(), ED_gpencil_stroke_check_collision(), ED_gpencil_stroke_point_is_inside(), ED_view3d_project_float_global(), ED_view3d_project_int_global(), ED_view3d_win_to_delta(), edge_inside_circle() (+10 more)

### Community 15 - "ReferenceSheet.kt"
Cohesion: 0.06
Nodes (4): ExportDialog(), writePng(), decodeForTrace(), TraceImageDialog()

### Community 16 - "LegacyGeometryOpType"
Cohesion: 0.07
Nodes (28): LegacyGeometryOp, flag0, flag1, int0, int1, type, value0, value1 (+20 more)

### Community 17 - "LineartTriangle"
Cohesion: 0.07
Nodes (38): EdgeFeatData, crease_threshold, edge_nabr, ld, material_indices, me, tri_array, use_auto_smooth (+30 more)

### Community 18 - "impl_"
Cohesion: 0.06
Nodes (36): annotation_data, document_data, history_record, history_reset, impl_, annotations, annotations_visible, document_created (+28 more)

### Community 19 - "ProjectGreaseUI.kt"
Cohesion: 0.13
Nodes (29): AdvancedSheet(), AnnotationBar(), CapabilityRow(), CurveHandlesOverlay(), DrawingGuidesOverlay(), Editor(), FillBar(), FpsDialog() (+21 more)

### Community 21 - "test_blender_select.c"
Cohesion: 0.22
Nodes (27): add_frame(), add_layer(), add_stroke(), BKE_gpencil_batch_cache_dirty_tag(), BKE_gpencil_layer_is_editable(), BKE_gpencil_stroke_select_check(), BKE_gpencil_stroke_select_index_reset(), BKE_gpencil_stroke_select_index_set() (+19 more)

### Community 22 - "lineart_scene_lite_replacements.cc"
Cohesion: 0.04
Nodes (60): BLI_spin_end(), lineart_edge_neighbor_init_task(), lineart_geometry_check_visible(), lineart_gpencil_generate(), lineart_intersection_mask_check(), lineart_intersection_priority_check(), lineart_load_tri_task(), lineart_object_load_single_instance() (+52 more)

### Community 23 - "import_blender_gp.sh"
Cohesion: 0.19
Nodes (15): Android JNI CMakeLists (projectgrease_jni), --no-undefined / --gc-sections link options, Workflow: Android Blender GP Core Probe, Workflow: Android Blender GP GPU DRW Closure Probe, Workflow: Android Blender GP Minimal Buffer Backend, Workflow: Android Blender GP Minimal Probe, Workflow: Android Blender GP Native Link Probe, Workflow: Android Blender GP Route-A Link Probe (+7 more)

### Community 26 - "project_grease_android_gpu_configure_batch"
Cohesion: 0.29
Nodes (4): GPUBatch, project_grease_android_gpu_configure_batch(), project_grease_android_gpu_draw_batch(), project_grease_android_gpu_buffer_backend_probe()

### Community 27 - "AndroidVertBuf"
Cohesion: 0.12
Nodes (3): vertbuf_alloc, AndroidVertBuf, vbo_id_

### Community 30 - "project_grease_tool_sculpt.c"
Cohesion: 0.20
Nodes (22): gpencil_brush_calc_midpoint(), gpencil_brush_grab_apply_cached(), gpencil_brush_grab_calc_dvec(), gpencil_brush_grab_store_points(), gpencil_brush_grab_stroke_init(), gpencil_brush_influence_calc(), gpencil_brush_invert_check(), gpencil_brush_pinch_apply() (+14 more)

### Community 31 - "Workflow: Native Blender GP Backend"
Cohesion: 0.26
Nodes (14): Workflow: Native Blender GP Backend, CI Link Diagnostic, Generated CMake link.txt recipe comparison (blender vs project_grease_gp_link_test), Run #28 final link failure (271 undefined refs / 113 symbols), Native blender_gp CMakeLists, curve_fit_nd extern sources, gpencil_geom_legacy.cc (BKE GP geometry), project_grease_legacy_fill/primitive/eraser/sculpt static libs (+6 more)

### Community 32 - "AndroidIndexBuf"
Cohesion: 0.16
Nodes (3): indexbuf_alloc, AndroidIndexBuf, ibo_id_

### Community 33 - "main"
Cohesion: 0.10
Nodes (25): main(), add_point, add_stroke, begin_stroke, create_document, create_material, create_polyline, create_primitive (+17 more)

### Community 34 - "test_render.cc"
Cohesion: 0.12
Nodes (49): project_grease_android_present_set_view_transform(), project_grease_android_present_set_weight_view(), project_grease_android_set_fx_provider(), add_bar(), add_layer(), Doc, gpd, fx_provider() (+41 more)

### Community 36 - "project_grease_tool_weight_paint.c"
Cohesion: 0.10
Nodes (22): BKE_defvert_array_copy(), BKE_defvert_ensure_index(), BKE_defvert_find_index(), BKE_defvert_find_weight(), BKE_brush_curve_strength(), brush_average_apply(), brush_blur_apply(), brush_calc_brush_dir_2d() (+14 more)

### Community 37 - "project_grease_blender_select.c"
Cohesion: 0.10
Nodes (21): ED_select_op_action(), ED_select_op_action_deselected(), ED_select_op_modal(), gpencil_stroke_do_circle_sel(), pg_deselect_all_selected(), pg_generic_select_exec(), pg_generic_stroke_select(), pg_material_style() (+13 more)

### Community 38 - "TaskPool"
Cohesion: 0.10
Nodes (30): BLI_task_pool_create(), BLI_task_pool_free(), BLI_task_pool_push(), BLI_task_pool_user_data(), BLI_task_pool_work_and_wait(), PGTask, data, free_data (+22 more)

### Community 39 - "lineart_lite_runtime.cc"
Cohesion: 0.11
Nodes (7): lineart_find_matching_edge(), lineart_find_matching_eln(), lineart_main_make_enclosed_shapes(), lineart_main_transform_and_add_shadow(), lineart_main_try_generate_shadow(), lineart_register_intersection_shadow_cuts(), lineart_register_shadow_cuts()

### Community 40 - "StrokePoint"
Cohesion: 0.15
Nodes (13): StrokePoint, a, b, g, pressure, r, strength, time (+5 more)

### Community 41 - "AndroidBatch"
Cohesion: 0.29
Nodes (3): batch_alloc, AndroidBatch, vao_id_

### Community 42 - "Blender 3.6.23 Legacy Grease Pencil (pinned baseline)"
Cohesion: 0.24
Nodes (8): Blender GP Import Manifest (docs), group_of(), select_gp(), extract_grease_pencil.sh script, Workflow: Extract Blender 3.6.23 Grease Pencil Source, Blender GP Source Import Manifest (native), Native Legacy GP Backend README, Blender 3.6.23 Legacy Grease Pencil (pinned baseline)

### Community 43 - "GenerateGolden.kt"
Cohesion: 0.24
Nodes (5): b(), fmt(), line(), main(), InputEvent

### Community 44 - "project_grease_shader_fx.c"
Cohesion: 0.21
Nodes (22): apply_blend(), blend_mode_output(), blend_state_for_mode(), floor_v2_nonzero(), fx_clampf(), fx_dot3(), fx_mixf(), gaussian_weight() (+14 more)

### Community 45 - "FakeDocument"
Cohesion: 0.09
Nodes (3): FakeDocument, Frame, Layer

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
Nodes (39): BLI_uniquename(), active_layer_data, create_layer, delete_layer, duplicate_layer, fx_add, fx_count, fx_get (+31 more)

### Community 53 - "project_grease_modifier_stack.c"
Cohesion: 0.07
Nodes (55): noise_table(), pg_deform_noise(), pg_deform_offset(), pg_deform_simplify(), pg_deform_smooth(), pg_deform_subdiv(), pg_mod_apply(), pg_mod_defaults() (+47 more)

### Community 54 - "project_grease_annotations.c"
Cohesion: 0.17
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

### Community 61 - "pg_fx_build_passes"
Cohesion: 0.32
Nodes (16): build_blur(), build_colorize(), build_flip(), build_glow(), build_pixel(), build_rim(), build_shadow(), build_swirl() (+8 more)

### Community 62 - "math"
Cohesion: 0.19
Nodes (13): fmodf(), i32(), noise_points(), offset_point(), f32(), final(), halton_3d(), halton_ex() (+5 more)

### Community 63 - "blender_reference.py"
Cohesion: 0.28
Nodes (3): main(), orbit_matrix(), run_scene()

### Community 65 - "ProjectGreaseGPHandle"
Cohesion: 0.06
Nodes (43): main(), project_grease_gp_cancel_stroke(), project_grease_gp_create(), project_grease_gp_create_frame(), project_grease_gp_delete_frame(), project_grease_gp_delete_stroke(), project_grease_gp_destroy(), project_grease_gp_duplicate_stroke() (+35 more)

### Community 66 - "test_shader_fx.cc"
Cohesion: 0.26
Nodes (21): copy_plane(), pg_fx_composite_cpu(), pg_fx_image_free(), pg_fx_image_from_premult(), pg_fx_image_new(), pg_fx_run_cpu(), fx_max_diff(), C() (+13 more)

### Community 67 - "blender_string_legacy_extract.c"
Cohesion: 0.16
Nodes (13): BLI_snprintf(), BLI_snprintf_rlen(), BLI_string_split_name_number(), BLI_strncpy(), BLI_strncpy_utf8(), BLI_strncpy_utf8_rlen(), BLI_strnlen(), BLI_uniquename_cb() (+5 more)

### Community 68 - "cmath"
Cohesion: 0.07
Nodes (41): legacy_influence(), main(), boundary_fill(), contract_shape(), dilate_shape(), Image, rgba_, is_leak_narrow() (+33 more)

### Community 69 - "project_grease_blender_edit.c"
Cohesion: 0.08
Nodes (62): copy_v3_v3_pge(), pg_gp_dissolve(), pg_gp_duplicate(), pg_gp_edit_delete_points(), pg_gp_edit_delete_strokes(), pg_gp_edit_dispatch(), pg_gp_edit_mirror(), pg_gp_edit_pick() (+54 more)

### Community 70 - "project_grease_gp_jni.cpp"
Cohesion: 0.09
Nodes (32): project_grease_gp_begin_stroke(), project_grease_gp_create_primitive(), project_grease_gp_erase_at(), project_grease_gp_generate_primitive_preview(), project_grease_gp_hit_test_stroke(), project_grease_gp_interpolate_frame(), project_grease_gp_interpolate_frame_eased(), project_grease_gp_rotate_stroke() (+24 more)

### Community 71 - "StrokeImport"
Cohesion: 0.05
Nodes (14): GreaseTemplates, Material, Template, ImageTrace, Fit, NewMaterial, Plan, PlannedStroke (+6 more)

### Community 72 - "ModifierRecord"
Cohesion: 0.08
Nodes (15): ModifierNative, ModifierRecord, ModifierSpecs, ModifierStackCommands, ModifierStackJson, ModifierStackPacking, ModifierType, ParamKind (+7 more)

### Community 73 - "bGPDstroke"
Cohesion: 0.12
Nodes (20): pg_gp_mod_length(), pg_gp_modstroke_length(), pge_length_modify_stroke(), apply_legacy_geometry_batch, process_stroke(), BKE_gpencil_batch_cache_dirty_tag(), BKE_gpencil_free_stroke(), BKE_gpencil_stroke_delete_tagged_points() (+12 more)

### Community 74 - "LineartIsecThread"
Cohesion: 0.12
Nodes (16): LineartIsecSingle, tri1, tri2, v1, v2, LineartIsecThread, array, count_test (+8 more)

### Community 75 - "VectorExport"
Cohesion: 0.15
Nodes (7): VectorExport, VectorLayer, VectorPage, VectorShape, Frame, Layer, VectorExportTest

### Community 76 - "project_grease_tool_vertex_paint.c"
Cohesion: 0.26
Nodes (18): brush_average_apply(), brush_blur_apply(), brush_calc_dvec_2d(), brush_influence_calc(), brush_invert_check(), brush_replace_apply(), brush_smear_apply(), brush_tint_apply() (+10 more)

### Community 77 - "FxRecord"
Cohesion: 0.09
Nodes (9): FxCommands, FxJson, FxNative, FxPacking, FxRecord, FxSpecs, FxType, FakeFxNative (+1 more)

### Community 78 - "jobject"
Cohesion: 0.09
Nodes (28): project_grease_gp_annotation_style(), project_grease_gp_apply_legacy_geometry_batch(), project_grease_gp_fx_set_enabled(), project_grease_gp_mirror_stroke(), project_grease_gp_mirror_stroke_about(), project_grease_gp_modifier_set_enabled(), project_grease_gp_set_layer_locked(), project_grease_gp_set_layer_use_mask() (+20 more)

### Community 80 - "ProjectGreaseGPPoint"
Cohesion: 0.13
Nodes (15): project_grease_gp_add_point(), project_grease_gp_create_polyline(), project_grease_gp_get_point(), project_grease_gp_set_point(), ProjectGreaseGPPoint, pressure, strength, time (+7 more)

### Community 81 - "test_blender_edit.c"
Cohesion: 0.20
Nodes (34): add_frame(), add_layer(), add_stroke(), BLI_lasso_boundbox(), frame_stroke_count(), list_add(), main(), make_gpd() (+26 more)

### Community 82 - "android_gp_shader_fx.cpp"
Cohesion: 0.09
Nodes (34): project_grease_android_present_reset(), bind_tex(), Buffer, color, reveal, compile(), draw_triangle(), ensure_convert_composite() (+26 more)

### Community 83 - "EdgeFeatData"
Cohesion: 0.11
Nodes (17): EdgeFeatData, crease_threshold, edge_nabr, ld, material_indices, me, tri_array, use_auto_smooth (+9 more)

### Community 84 - "project_grease_blender_edit4.c"
Cohesion: 0.24
Nodes (23): pe4_copy_range(), pe4_deselect(), pe4_editable(), pe4_insert_after(), pe4_move_selected(), pe4_normal(), pe4_sel(), pe4_style() (+15 more)

### Community 86 - "project_grease_gp_backend.h"
Cohesion: 0.07
Nodes (20): cancel_stroke, create_frame, history_redo, history_undo, render, render_external_context, render_with_gpu_context, reset_document (+12 more)

### Community 87 - "StrokeRecord"
Cohesion: 0.10
Nodes (8): LayerRecord, MaskRecord, MaterialRecord, ParsedDocument, ParsedFrame, ParsedLayer, ProjectDocumentCodec, StrokeRecord

### Community 89 - "pg_sculpt_session_begin"
Cohesion: 0.13
Nodes (9): pg_sculpt_session_begin(), pg_sculpt_session_end(), pgt_sculpt_preset(), pgt_sculpt_update_geometry(), pg_tool_link_runtime(), pg_tool_material_style(), pg_tool_view_init(), pg_vpaint_session_begin() (+1 more)

### Community 90 - ".from"
Cohesion: 0.14
Nodes (5): CanvasMapping, ToolSampleBatch, TouchHistory, FakeTouch, ToolSessionInputTest

### Community 91 - "LegacyGpBrushStrokeEngine"
Cohesion: 0.22
Nodes (3): LegacyGpBrushStrokeEngine, Settings, StrokePoint

### Community 92 - "ReferenceScene"
Cohesion: 0.09
Nodes (3): ReferenceCamera, ReferenceScene, ReferenceCameraTest

### Community 93 - "bGPDlayer"
Cohesion: 0.20
Nodes (22): pe3_apply_vcolor(), pe3_clampf(), pe3_editable(), pe3_fn_bc(), pe3_fn_levels(), pe3_frames_equal(), pe3_style(), pg_gp_blank_frame_add() (+14 more)

### Community 94 - "AnnotationData"
Cohesion: 0.15
Nodes (6): AnnotationData, Frame, Parsed, ProjectRecord, ProjectStore, AnnotationDataTest

### Community 95 - "project_grease_blender_edit5.c"
Cohesion: 0.07
Nodes (41): project_grease_android_gp_cache_upload_probe(), ED_gpencil_select_stroke_segment(), gpencil_calc_factor(), gpencil_check_collision(), gpencil_copy_points(), gpencil_insert_point(), pg4_material_editable(), pg4_stroke_2d_xy() (+33 more)

### Community 98 - "ImportDialogs.kt"
Cohesion: 0.21
Nodes (4): rememberSvgImport(), toast(), ReferenceOverlay(), ReferenceSheet()

### Community 109 - "project_grease_blender_primitive.c"
Cohesion: 0.08
Nodes (21): Java_com_smitnk_projectgrease_nativebridge_GPNative_nativePing(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGenerateBlenderPrimitive(), pg_anchors_valid(), pg_blender_type(), pg_generate(), pg_resolve_edges(), pg_to_region(), pg_total_points() (+13 more)

### Community 110 - "project_grease_blender_edit2.c"
Cohesion: 0.14
Nodes (18): pe2_clampf(), pe2_insert_point(), pe2_stroke_editable(), pe2_style(), pg_gp_edit2_dispatch(), pg_gp_extrude(), pg_gp_mod_thickness_vgroup(), pg_gp_modifier_point_weight() (+10 more)

### Community 111 - "MainActivity.kt"
Cohesion: 0.09
Nodes (7): MainActivity, ProjectGreaseTheme(), ProjectGreaseThemeMode, DARK, LIGHT, SYSTEM, Settings()

### Community 112 - "bGPdata"
Cohesion: 0.28
Nodes (19): pg_gp_select_all(), pg_gp_select_alternate(), pg_gp_select_box(), pg_gp_select_circle(), pg_gp_select_dispatch(), pg_gp_select_first(), pg_gp_select_grouped(), pg_gp_select_lasso() (+11 more)

### Community 114 - "ColorMath"
Cohesion: 0.19
Nodes (4): ColorMath, Hsva, BlenderColorPicker(), ColorMathTest

### Community 116 - "CurveSession"
Cohesion: 0.22
Nodes (5): CurveSession, Press, CONFIRM, HANDLE, LINE

### Community 119 - "PGMeshLite"
Cohesion: 0.07
Nodes (33): EdgeNeighborData, adj_e, edge_nabr, me, lineart_identify_mlooptri_feature_edges(), pg_lite_find_edge(), pg_lite_looptri_get_real_edges(), EdgeNeighborData (+25 more)

### Community 120 - "project_grease_scene_lite_jni.cpp"
Cohesion: 0.31
Nodes (11): Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteClear(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteCreate(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteFree(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArt(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArtStrokes(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLoadObj(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteProjectEdges(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteSetCamera() (+3 more)

### Community 121 - "project_grease_scene_lite.c"
Cohesion: 0.15
Nodes (23): BKE_camera_sensor_fit(), BKE_camera_sensor_size(), cross_v3(), focallength_to_fov(), grow_push(), invert_m4(), lineart_matrix_ortho_44d(), lineart_matrix_perspective_44d() (+15 more)

### Community 122 - "blender_math_geom_legacy_extract.cc"
Cohesion: 0.21
Nodes (11): BLI_lasso_is_point_inside(), closest_to_line_segment_v2(), closest_to_line_segment_v3(), closest_to_line_v2(), closest_to_line_v3(), cross_poly_v2(), dist_squared_to_line_segment_v2(), isect_line_line_v3() (+3 more)

### Community 123 - "pg_eraser_dostroke"
Cohesion: 0.28
Nodes (8): gpencil_stroke_soft_refine(), pg_calc_influence(), pg_clipped(), pg_eraser_dostroke(), pg_len_v2v2_int(), pg_point_to_xy(), pg_rect_isect_pt(), pg_stroke_inside_circle()

### Community 125 - "dna_gpencil_legacy_types"
Cohesion: 0.22
Nodes (4): pg_clear_stroke(), pg_fade(), pg_reduce(), project_grease_legacy_build_apply()

### Community 128 - "Tool"
Cohesion: 0.15
Nodes (12): bGPdata, bGPDframe, bGPDstroke, Tool, Grab, Pinch, Push, Randomize (+4 more)

### Community 130 - "PGCameraLite"
Cohesion: 0.09
Nodes (22): PGCameraLite, clip_end, clip_start, lens, matrix_world, ortho_scale, sensor_fit, sensor_x (+14 more)

### Community 133 - "PGFxEntry"
Cohesion: 0.47
Nodes (5): count_passes(), project_grease_fx_pass_count(), fx_provider_trampoline(), PGFxEntry, pg_fx_entry_init()

### Community 134 - "Settings"
Cohesion: 0.18
Nodes (10): bGPdata, bGPDframe, bGPDstroke, Settings, draw_strength, pointer_pressure, soft, soft_strength (+2 more)

### Community 137 - "PolylineSession"
Cohesion: 0.22
Nodes (4): PolylineSession, Release, CONTINUE, FINISH

### Community 138 - "stdio"
Cohesion: 0.18
Nodes (15): pg_lite_scene_clear(), pg_lite_scene_create(), pg_lite_scene_free(), pg_lite_stats(), main(), run(), scene_with(), test_cube() (+7 more)

### Community 140 - "Feature table"
Cohesion: 0.22
Nodes (8): Animation, Defects found by this audit, Device validation checklist (only you can produce this evidence), Feature table, Fill, paint, sculpt, Modifiers, Line Art, persistence, Project Grease audit, 2026-10-02, Selection and editing

### Community 142 - "ProjectGreaseGPLegacyGeometryOp"
Cohesion: 0.22
Nodes (9): ProjectGreaseGPLegacyGeometryOp, flag0, flag1, int0, int1, type, value0, value1 (+1 more)

### Community 143 - "mem_guardedalloc"
Cohesion: 0.17
Nodes (3): CLG_log_str(), CLG_logf(), CLG_logref_init()

### Community 144 - "test_lineart_reference.c"
Cohesion: 0.26
Nodes (10): compare_scene(), compare_strokes(), dist_point_seg(), load_reference(), load_world_strokes(), nearest(), pg_lineart_reference_compare(), push() (+2 more)

### Community 145 - "Settings"
Cohesion: 0.20
Nodes (10): Settings, apply_position, apply_strength, apply_thickness, apply_uv, brush_alpha, invert, multiframe_falloff (+2 more)

### Community 150 - "PGObjectLite"
Cohesion: 0.20
Nodes (12): lineart_geometry_check_visible(), lineart_geometry_load_assign_thread(), lineart_intersection_mask_check(), lineart_intersection_priority_check(), lineart_object_load_single_instance(), lineart_object_load_worker(), lineart_usage_check(), PGObjectLite (+4 more)

### Community 151 - "LegacyPaintSettings"
Cohesion: 0.29
Nodes (7): set_legacy_paint_settings, LegacyPaintSettings, draw_smooth_factor, draw_smooth_level, input_samples, smooth_position, smooth_strength

### Community 154 - "LineartPointTri"
Cohesion: 0.33
Nodes (6): lineart_point_on_line_segment(), lineart_point_triangle_relation(), LineartPointTri, LRT_INSIDE_TRIANGLE, LRT_ON_TRIANGLE, LRT_OUTSIDE_TRIANGLE

### Community 156 - "Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyBlenderModifier"
Cohesion: 0.18
Nodes (10): project_grease_gp_apply_blender_modifier_named(), project_grease_gp_create_layer(), project_grease_gp_rename_layer(), project_grease_gp_vertex_group_add(), project_grease_gp_vertex_group_rename(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyBlenderModifier(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCreateLayer(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeRenameLayer() (+2 more)

### Community 159 - "lineart_geometry_object_load"
Cohesion: 0.12
Nodes (16): BLI_spin_lock(), BLI_spin_unlock(), BLI_task_parallel_range(), lineart_build_edge_neighbor(), lineart_geometry_object_load(), pg_obi_object(), lineart_add_edge_to_array_thread(), lineart_build_edge_neighbor() (+8 more)

### Community 161 - "apply_select_integration.py"
Cohesion: 0.60
Nodes (3): main(), patch(), replace_once()

### Community 163 - "evaluated_frame"
Cohesion: 0.15
Nodes (13): evaluated_frame, point_weight_at, point_weight_count, set_point_weight, eval_frame_trampoline(), EvalCacheEntry, cfra, frame (+5 more)

### Community 164 - "gen_lineart_lite.py"
Cohesion: 0.36
Nodes (4): generate(), item_spans(), main(), replacement_sections()

### Community 166 - "BLI_math_vector.h"
Cohesion: 0.40
Nodes (4): add_v2_v2v2(), interp_v2_v2v2(), interp_v2_v2v2v2v2_cubic(), rotate_v2_v2fl()

### Community 168 - "Phase"
Cohesion: 0.50
Nodes (4): Phase, DRAG_LINE, EDIT, IDLE

### Community 170 - "project_grease_lineart_cpu.cc"
Cohesion: 0.10
Nodes (45): BLI_spin_init(), lineart_create_render_buffer(), pg_lineart_compute_occlusion(), EdgeFeatReduceData, feat_edges, lineart_add_triangles_worker(), lineart_bounding_area_edge_intersect(), lineart_bounding_area_line_add() (+37 more)

### Community 172 - "LooseEdgeData"
Cohesion: 0.67
Nodes (3): LooseEdgeData, loose_array, loose_count

### Community 175 - "JNIEnv"
Cohesion: 0.13
Nodes (19): project_grease_gp_add_stroke(), project_grease_gp_annotation_load(), project_grease_gp_apply_edit_command(), project_grease_gp_clear_selection(), project_grease_gp_fx_set_params(), project_grease_gp_get_stroke_info(), project_grease_gp_lasso_select(), project_grease_gp_modifier_set_params() (+11 more)

## Knowledge Gaps
- **566 isolated node(s):** `display`, `context`, `surface`, `window`, `width` (+561 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1087 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **127 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `GPNative` connect `GPNative` to `NativeEditorBridge`, `EditorControllers.kt`, `FloatArray`, `.surfaceCreated`, `.deleteFrame`, `.deleteStroke`, `.duplicateStroke`, `.stats`, `.deleteLastStroke`, `.createMaterial`, `.frameCount`, `.frameEnd`, `.moveLayer`, `.setLayerOpacity`, `.fxAdd`, `.historyCanRedo`, `ReferenceScene`, `.historyCanUndo`, `.maskCount`, `.maskFlags`, `.trimStroke`, `.maskName`, `.setVertexGroupActive`, `.vertexGroupRemove`, `.modifierApply`, `.modifierMove`, `.rotateStroke`, `.selectedPointCount`, `.selectFrameOrHold`, `.setLayerLocked`, `.setMultiframeEditing`, `MainActivity.kt`, `.smoothStroke`, `.strokeCount`, `.vertexGroupCount`, `.vertexGroupRename`?**
  _High betweenness centrality (0.029) - this node is a cross-community bridge._
- **Why does `NativeEditorBridge` connect `NativeEditorBridge` to `GPNative`, `EditorControllers.kt`, `FloatArray`, `.deleteFrame`, `.deleteStroke`, `.duplicateStroke`, `.stats`, `.deleteLastStroke`, `.createMaterial`, `.frameCount`, `.frameEnd`, `.moveLayer`, `.setLayerOpacity`, `.fxAdd`, `.historyCanRedo`, `ModifierRecord`, `FxRecord`, `.historyCanUndo`, `.maskCount`, `.maskFlags`, `.trimStroke`, `.maskName`, `.setVertexGroupActive`, `.vertexGroupRemove`, `.modifierApply`, `.modifierMove`, `.rotateStroke`, `.selectedPointCount`, `.selectFrameOrHold`, `.setLayerLocked`, `.setMultiframeEditing`, `.smoothStroke`, `.strokeCount`, `.vertexGroupCount`, `.vertexGroupRename`, `NativeDocumentAdapter`?**
  _High betweenness centrality (0.022) - this node is a cross-community bridge._
- **Why does `FeatureId` connect `FeatureId` to `ProjectGreaseUI.kt`, `FeatureRegistry`, `.capability`?**
  _High betweenness centrality (0.022) - this node is a cross-community bridge._
- **What connects `display`, `context`, `surface` to the rest of the system?**
  _566 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FeatureId` be split into smaller, more focused modules?**
  _Cohesion score 0.014285714285714285 - nodes in this community are weakly interconnected._
- **Should `project_grease_gp_bridge.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.06763285024154589 - nodes in this community are weakly interconnected._
- **Should `from_handle` be split into smaller, more focused modules?**
  _Cohesion score 0.05906553041434029 - nodes in this community are weakly interconnected._