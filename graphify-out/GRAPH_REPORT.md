# Graph Report - Project-greas  (2026-10-03)

## Corpus Check
- 259 files · ~262,196 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 14 file(s) not represented in the graph (top: .obj 6, .xml 3, .properties 2)

## Summary
- 4324 nodes · 11391 edges · 259 communities (118 shown, 141 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 1081 edges (avg confidence: 0.84)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `e709b326`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- FeatureId
- project_grease_gp_bridge.cpp
- from_handle
- project_grease_android_egl_renderer.cpp
- lineart_scene_lite_replacements.cc
- project_grease_gp_backend.cpp
- pg_eraser_dostroke
- project_grease_legacy_sculpt.cpp
- android_gp_presentation.cpp
- test_tool_session.c
- NativeEditorBridge
- AndroidBackend
- EngineFeature
- project_grease_tool_util.c
- ReferenceSheet.kt
- LegacyGeometryOpType
- project_grease_lineart_cpu.cc
- impl_
- ProjectGreaseUI.kt
- ProjectGreaseEglSurface.kt
- project_grease_blender_select.c
- PGLineartSettings
- import_blender_gp.sh
- HistoryController
- FakeDocument
- AndroidVertBuf
- GPNative
- .surfaceCreated
- project_grease_tool_sculpt.c
- Workflow: Native Blender GP Backend
- AndroidIndexBuf
- evaluated_frame
- test_render.cc
- assertequals
- project_grease_tool_weight_paint.c
- PGCameraLite
- project_grease_scene_lite_jni.cpp
- lineart_lite_runtime.cc
- StrokePoint
- AndroidBatch
- Blender 3.6.23 Legacy Grease Pencil (pinned baseline)
- GenerateGolden.kt
- test_lineart_reference.c
- test_backend_modifier_stack.cc
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
- project_grease_blender_edit5.c
- Bundled Blender Legacy GP Engine Worklog
- android_blender_gp_gpu_drw_probe.sh
- project_grease_shader_fx.c
- gen_modifier_golden.py
- blender_reference.py
- ProjectGreaseGPHandle
- Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeGenerateBlenderPrimitive
- string
- project_grease_legacy_fill.cpp
- bGPDlayer
- project_grease_gp_jni.cpp
- StrokeImport
- ModifierRecord
- bGPDstroke
- LineartIsecThread
- VectorExport
- project_grease_tool_vertex_paint.c
- FxRecord
- JNIEXPORT
- ProjectGreaseSelect
- ProjectGreaseGPPoint
- test_blender_edit.c
- android_gp_shader_fx.cpp
- EdgeFeatData
- project_grease_blender_edit4.c
- ProjectDocumentRoundTripTest
- main
- StrokeRecord
- DocumentNative
- unit_m4
- ToolSessionInputTest
- LegacyGpBrushStrokeEngine
- ReferenceScene
- project_grease_blender_edit3.c
- AnnotationData
- test_document_state.c
- pg_stroke_outline
- DeviceBugfixTest.kt
- test_lineart.c
- android_blender_gp_core_probe.sh
- android_blender_gp_minimal_buffer_backend_probe.sh
- android_blender_gp_minimal_probe.sh
- android_blender_gp_route_a_link_probe.sh
- project_grease_blender_primitive.c
- project_grease_blender_edit2.c
- MainActivity.kt
- cmath
- ExportDoc
- ColorMath
- NativeDocumentAdapter
- CurveSession
- .mirror
- ImportDialogs.kt
- PGMeshLite
- project_grease_scene_lite.c
- test_stroke_outline.c
- android_gpu_backend.cpp
- .finite
- project_grease_legacy_build.cpp
- .capability
- ToolSession
- vgroup_at
- .packsArguments
- PGLineartSegment
- bli_math
- BlenderPrimitiveRulesTest
- pg_lite_camera_orbit
- Settings
- .packsArguments
- BlenderSelectRulesTest
- PolylineSession
- project_grease_legacy_primitive.cpp
- .pressureFor
- Feature table
- ProjectStore
- ProjectGreaseGPLegacyGeometryOp
- dna_meshdata_types
- Rng
- Settings
- test_gp_color.c
- BlenderColorModifierRulesTest
- .mirrorCopy
- .opacityModifier
- PGObjectLite
- LegacyPaintSettings
- BlenderPrimitiveRules.kt
- .vertexPaint
- LineartPointTri
- .packsArguments
- jlong
- BLI_spin_lock
- verify_blender_verbatim.py
- apply_select_integration.py
- DocumentFrameEndTest.kt
- GreaseTemplates.java
- gen_lineart_lite.py
- CurveSessionTest
- .from
- .projectName
- project_grease_gp_backend.h
- BrushSizeTest.kt
- LineartData
- TriData
- LooseEdgeData
- project_grease_lineart_lite.h
- FakeTouch
- run_native_edit_tests.sh
- MotionTouch
- run_native_tool_session_tests.sh
- Screen
- run_native_edit5_tests.sh
- DrawMode
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
- project_grease_android_set_frame_evaluator

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

## Communities (259 total, 141 thin omitted)

### Community 0 - "FeatureId"
Cohesion: 0.01
Nodes (140): FeatureId, ADD_FRAME, ADVANCED_FILL, ADVANCED_INTERPOLATION, ADVANCED_ONION_SKIN, ANNOTATIONS, ARC, CIRCLE (+132 more)

### Community 1 - "project_grease_gp_bridge.cpp"
Cohesion: 0.06
Nodes (43): ensure_ready(), project_grease_gp_apply_blender_modifier(), project_grease_gp_apply_blender_modifier_stack(), project_grease_gp_delete_last_stroke(), project_grease_gp_fill_at_screen(), project_grease_gp_frame_end(), project_grease_gp_frame_numbers(), project_grease_gp_fx_remove() (+35 more)

### Community 2 - "from_handle"
Cohesion: 0.05
Nodes (86): project_grease_gp_annotation_dump(), project_grease_gp_annotation_load(), project_grease_gp_apply_edit_command(), project_grease_gp_clear_selection(), project_grease_gp_close_stroke(), project_grease_gp_create_polyline(), project_grease_gp_duplicate_frame(), project_grease_gp_duplicate_layer() (+78 more)

### Community 3 - "project_grease_android_egl_renderer.cpp"
Cohesion: 0.09
Nodes (53): attach_window(), choose_config(), connect_blender_gp(), create_context(), destroy_renderer(), detach_window(), disconnect_blender_gp(), from_handle() (+45 more)

### Community 4 - "lineart_scene_lite_replacements.cc"
Cohesion: 0.13
Nodes (20): lineart_create_render_buffer(), lineart_edge_neighbor_init_task(), lineart_geometry_check_visible(), lineart_geometry_object_load(), lineart_gpencil_generate(), lineart_identify_mlooptri_feature_edges(), lineart_intersection_mask_check(), lineart_intersection_priority_check() (+12 more)

### Community 5 - "project_grease_gp_backend.cpp"
Cohesion: 0.07
Nodes (77): main(), Backend, annotations_visible, apply_blender_generator, apply_blender_modifier, apply_blender_modifier_stack, clear_selection, close_stroke (+69 more)

### Community 6 - "pg_eraser_dostroke"
Cohesion: 0.10
Nodes (32): BLI_lasso_is_point_inside(), closest_to_line_segment_v2(), closest_to_line_segment_v3(), closest_to_line_v2(), closest_to_line_v3(), cross_poly_v2(), dist_squared_to_line_segment_v2(), isect_line_line_v3() (+24 more)

### Community 7 - "project_grease_legacy_sculpt.cpp"
Cohesion: 0.14
Nodes (12): apply(), apply_position_smooth(), apply_strength_smooth(), apply_thickness_smooth(), Context, delta_x, delta_y, mouse_x (+4 more)

### Community 8 - "android_gp_presentation.cpp"
Cohesion: 0.10
Nodes (45): append_fill(), append_outline(), append_stroke_outline(), compile_shader(), coverage_begin(), coverage_end(), draw_frame(), draw_frame_weights() (+37 more)

### Community 10 - "test_tool_session.c"
Cohesion: 0.07
Nodes (57): add_arc_points(), add_fake_points(), angle_between(), append_point(), ensure(), fclamp(), filter_mval(), hold_back() (+49 more)

### Community 13 - "EngineFeature"
Cohesion: 0.07
Nodes (28): engine_feature_map(), EngineFeature, Drawing, Editing, Eraser, Fill, Frames, Interpolation (+20 more)

### Community 14 - "project_grease_tool_util.c"
Cohesion: 0.12
Nodes (15): BKE_boundbox_init_from_minmax(), ED_gpencil_projected_2d_bound_box(), ED_gpencil_stroke_check_collision(), ED_gpencil_stroke_point_is_inside(), ED_view3d_project_float_global(), ED_view3d_project_int_global(), edge_inside_circle(), gpencil_point_3d_to_xy() (+7 more)

### Community 15 - "ReferenceSheet.kt"
Cohesion: 0.05
Nodes (5): DocumentController, ExportDialog(), writePng(), decodeForTrace(), TraceImageDialog()

### Community 16 - "LegacyGeometryOpType"
Cohesion: 0.07
Nodes (28): LegacyGeometryOp, flag0, flag1, int0, int1, type, value0, value1 (+20 more)

### Community 17 - "project_grease_lineart_cpu.cc"
Cohesion: 0.07
Nodes (53): EdgeFeatReduceData, feat_edges, lineart_add_isec_thread(), lineart_add_triangles_worker(), lineart_bounding_area_edge_intersect(), lineart_bounding_area_line_add(), lineart_bounding_area_link_edge(), lineart_bounding_area_link_triangle() (+45 more)

### Community 18 - "impl_"
Cohesion: 0.06
Nodes (36): annotation_data, document_data, history_record, history_reset, impl_, annotations, annotations_visible, document_created (+28 more)

### Community 19 - "ProjectGreaseUI.kt"
Cohesion: 0.13
Nodes (29): AdvancedSheet(), AnnotationBar(), CapabilityRow(), CurveHandlesOverlay(), DrawingGuidesOverlay(), Editor(), FillBar(), FpsDialog() (+21 more)

### Community 21 - "project_grease_blender_select.c"
Cohesion: 0.06
Nodes (68): ED_select_op_action(), ED_select_op_action_deselected(), ED_select_op_modal(), gpencil_stroke_do_circle_sel(), pg_deselect_all_selected(), pg_generic_select_exec(), pg_generic_stroke_select(), pg_gp_select_all() (+60 more)

### Community 22 - "PGLineartSettings"
Cohesion: 0.08
Nodes (29): BLI_spin_end(), pg_lineart_settings_default(), lineart_destroy_render_data(), lineart_destroy_render_data_keep_init(), lineart_end_bounding_area_recursive(), lineart_gpencil_generate(), pg_lineart_compute(), pg_lineart_compute_strokes() (+21 more)

### Community 23 - "import_blender_gp.sh"
Cohesion: 0.19
Nodes (15): Android JNI CMakeLists (projectgrease_jni), --no-undefined / --gc-sections link options, Workflow: Android Blender GP Core Probe, Workflow: Android Blender GP GPU DRW Closure Probe, Workflow: Android Blender GP Minimal Buffer Backend, Workflow: Android Blender GP Minimal Probe, Workflow: Android Blender GP Native Link Probe, Workflow: Android Blender GP Route-A Link Probe (+7 more)

### Community 26 - "FakeDocument"
Cohesion: 0.09
Nodes (3): FakeDocument, Frame, Layer

### Community 27 - "AndroidVertBuf"
Cohesion: 0.18
Nodes (3): vertbuf_alloc, AndroidVertBuf, vbo_id_

### Community 30 - "project_grease_tool_sculpt.c"
Cohesion: 0.19
Nodes (23): gpencil_brush_calc_midpoint(), gpencil_brush_grab_apply_cached(), gpencil_brush_grab_calc_dvec(), gpencil_brush_grab_store_points(), gpencil_brush_grab_stroke_init(), gpencil_brush_influence_calc(), gpencil_brush_invert_check(), gpencil_brush_pinch_apply() (+15 more)

### Community 31 - "Workflow: Native Blender GP Backend"
Cohesion: 0.26
Nodes (14): Workflow: Native Blender GP Backend, CI Link Diagnostic, Generated CMake link.txt recipe comparison (blender vs project_grease_gp_link_test), Run #28 final link failure (271 undefined refs / 113 symbols), Native blender_gp CMakeLists, curve_fit_nd extern sources, gpencil_geom_legacy.cc (BKE GP geometry), project_grease_legacy_fill/primitive/eraser/sculpt static libs (+6 more)

### Community 33 - "evaluated_frame"
Cohesion: 0.11
Nodes (22): create_polyline, create_primitive, evaluated_frame, fill_at_screen, fx_for_layer, point_weight_at, point_weight_count, set_point_weight (+14 more)

### Community 34 - "test_render.cc"
Cohesion: 0.12
Nodes (50): project_grease_android_present_set_view_transform(), project_grease_android_present_set_weight_view(), project_grease_android_set_fx_provider(), add_bar(), add_layer(), Doc, gpd, fx_max_diff() (+42 more)

### Community 36 - "project_grease_tool_weight_paint.c"
Cohesion: 0.19
Nodes (20): BKE_defvert_array_copy(), BKE_defvert_ensure_index(), BKE_defvert_find_index(), BKE_defvert_find_weight(), brush_average_apply(), brush_blur_apply(), brush_calc_brush_dir_2d(), brush_draw_apply() (+12 more)

### Community 37 - "PGCameraLite"
Cohesion: 0.09
Nodes (22): PGCameraLite, clip_end, clip_start, lens, matrix_world, ortho_scale, sensor_fit, sensor_x (+14 more)

### Community 38 - "project_grease_scene_lite_jni.cpp"
Cohesion: 0.31
Nodes (11): Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteClear(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteCreate(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteFree(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArt(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLineArtStrokes(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteLoadObj(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteProjectEdges(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSceneLiteSetCamera() (+3 more)

### Community 39 - "lineart_lite_runtime.cc"
Cohesion: 0.07
Nodes (20): BLI_task_pool_push(), BLI_task_pool_user_data(), BLI_task_pool_work_and_wait(), lineart_find_matching_edge(), lineart_find_matching_eln(), lineart_main_transform_and_add_shadow(), lineart_main_try_generate_shadow(), lineart_register_intersection_shadow_cuts() (+12 more)

### Community 40 - "StrokePoint"
Cohesion: 0.15
Nodes (13): StrokePoint, a, b, g, pressure, r, strength, time (+5 more)

### Community 41 - "AndroidBatch"
Cohesion: 0.13
Nodes (6): AndroidBatch, vao_id_, GPUBatch, project_grease_android_gpu_configure_batch(), project_grease_android_gpu_draw_batch(), project_grease_android_gpu_buffer_backend_probe()

### Community 42 - "Blender 3.6.23 Legacy Grease Pencil (pinned baseline)"
Cohesion: 0.24
Nodes (8): Blender GP Import Manifest (docs), group_of(), select_gp(), extract_grease_pencil.sh script, Workflow: Extract Blender 3.6.23 Grease Pencil Source, Blender GP Source Import Manifest (native), Native Legacy GP Backend README, Blender 3.6.23 Legacy Grease Pencil (pinned baseline)

### Community 43 - "GenerateGolden.kt"
Cohesion: 0.22
Nodes (5): b(), fmt(), line(), main(), InputEvent

### Community 44 - "test_lineart_reference.c"
Cohesion: 0.23
Nodes (11): pg_lite_project(), compare_scene(), compare_strokes(), dist_point_seg(), load_reference(), load_world_strokes(), nearest(), pg_lineart_reference_compare() (+3 more)

### Community 45 - "test_backend_modifier_stack.cc"
Cohesion: 0.11
Nodes (13): history_redo, history_undo, render, render_external_context, render_with_gpu_context, eval_cache_clear(), history_copy_settings(), history_restore_snapshot() (+5 more)

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
Nodes (46): active_layer_data, delete_layer, duplicate_frame, duplicate_layer, fx_add, fx_count, fx_get, fx_move (+38 more)

### Community 53 - "project_grease_modifier_stack.c"
Cohesion: 0.07
Nodes (55): noise_table(), pg_deform_noise(), pg_deform_offset(), pg_deform_simplify(), pg_deform_smooth(), pg_deform_subdiv(), pg_mod_apply(), pg_mod_defaults() (+47 more)

### Community 54 - "project_grease_annotations.c"
Cohesion: 0.19
Nodes (29): pa_frame_ensure(), pa_frame_free(), pa_frames_free(), pa_layer(), pa_link_append(), pa_link_insert_before(), pa_load_walk(), pa_open_stroke() (+21 more)

### Community 55 - "Android Shell README"
Cohesion: 0.40
Nodes (5): projectgrease_jni shared library, Android Shell README, Native EGL renderer (clear and present only), Android SurfaceView -> ANativeWindow -> EGL -> GLES2 transport, Android-owned EGL/GLES context

### Community 56 - "project_grease_blender_edit5.c"
Cohesion: 0.07
Nodes (27): project_grease_android_gp_cache_upload_probe(), ED_gpencil_select_stroke_segment(), gpencil_calc_factor(), gpencil_check_collision(), gpencil_copy_points(), gpencil_insert_point(), pg4_material_editable(), pg4_stroke_2d_xy() (+19 more)

### Community 57 - "Bundled Blender Legacy GP Engine Worklog"
Cohesion: 0.40
Nodes (3): Bundled Blender Legacy GP Engine Worklog, project_grease_gp_engine host-facing engine boundary, Real Legacy GP modifier execution (deformStroke callbacks)

### Community 59 - "android_blender_gp_gpu_drw_probe.sh"
Cohesion: 0.83
Nodes (3): compile_one(), run_group(), android_blender_gp_gpu_drw_probe.sh script

### Community 61 - "project_grease_shader_fx.c"
Cohesion: 0.08
Nodes (65): fx_provider_trampoline(), PGFxEntry, apply_blend(), blend_mode_output(), blend_state_for_mode(), build_blur(), build_colorize(), build_flip() (+57 more)

### Community 62 - "gen_modifier_golden.py"
Cohesion: 0.33
Nodes (12): fmodf(), i32(), noise_points(), offset_point(), f32(), final(), halton_3d(), halton_ex() (+4 more)

### Community 63 - "blender_reference.py"
Cohesion: 0.28
Nodes (3): main(), orbit_matrix(), run_scene()

### Community 65 - "ProjectGreaseGPHandle"
Cohesion: 0.08
Nodes (35): main(), project_grease_gp_begin_stroke(), project_grease_gp_cancel_stroke(), project_grease_gp_create(), project_grease_gp_create_frame(), project_grease_gp_delete_frame(), project_grease_gp_delete_stroke(), project_grease_gp_destroy() (+27 more)

### Community 67 - "string"
Cohesion: 0.06
Nodes (14): BLI_snprintf(), BLI_snprintf_rlen(), BLI_string_split_name_number(), BLI_strncpy(), BLI_strncpy_utf8(), BLI_strncpy_utf8_rlen(), BLI_strnlen(), BLI_uniquename_cb() (+6 more)

### Community 68 - "project_grease_legacy_fill.cpp"
Cohesion: 0.14
Nodes (20): boundary_fill(), contract_shape(), dilate_shape(), Image, rgba_, is_leak_narrow(), normalize_to_legacy_mask(), outline_points() (+12 more)

### Community 69 - "bGPDlayer"
Cohesion: 0.07
Nodes (72): pe3_fn_hsv(), copy_v3_v3_pge(), pg_gp_dissolve(), pg_gp_duplicate(), pg_gp_edit_delete_points(), pg_gp_edit_delete_strokes(), pg_gp_edit_dispatch(), pg_gp_edit_mirror() (+64 more)

### Community 70 - "project_grease_gp_jni.cpp"
Cohesion: 0.09
Nodes (30): project_grease_gp_create_primitive(), project_grease_gp_erase_at(), project_grease_gp_generate_primitive_preview(), project_grease_gp_hit_test_stroke(), project_grease_gp_interpolate_frame(), project_grease_gp_interpolate_frame_eased(), project_grease_gp_rotate_stroke(), project_grease_gp_rotate_stroke_about() (+22 more)

### Community 71 - "StrokeImport"
Cohesion: 0.06
Nodes (11): ImageTrace, Fit, NewMaterial, Plan, PlannedStroke, Source, StrokeImport, Stroke (+3 more)

### Community 72 - "ModifierRecord"
Cohesion: 0.08
Nodes (15): ModifierNative, ModifierRecord, ModifierSpecs, ModifierStackCommands, ModifierStackJson, ModifierStackPacking, ModifierType, ParamKind (+7 more)

### Community 73 - "bGPDstroke"
Cohesion: 0.17
Nodes (17): pg_gp_mod_length(), pg_gp_modstroke_length(), pge_length_modify_stroke(), apply_legacy_geometry_batch, BKE_gpencil_batch_cache_dirty_tag(), BKE_gpencil_stroke_flip(), BKE_gpencil_stroke_join(), BKE_gpencil_stroke_length() (+9 more)

### Community 74 - "LineartIsecThread"
Cohesion: 0.08
Nodes (25): lineart_add_edge_to_array(), lineart_create_edges_from_isec_data(), lineart_destroy_isec_thread(), lineart_finalize_object_edge_array(), lineart_finalize_object_edge_array_reserve(), LineartIsecData, ld, thread_count (+17 more)

### Community 75 - "VectorExport"
Cohesion: 0.15
Nodes (7): VectorExport, VectorLayer, VectorPage, VectorShape, Frame, Layer, VectorExportTest

### Community 76 - "project_grease_tool_vertex_paint.c"
Cohesion: 0.21
Nodes (21): BKE_brush_curve_strength(), brush_average_apply(), brush_blur_apply(), brush_calc_dvec_2d(), brush_influence_calc(), brush_invert_check(), brush_replace_apply(), brush_smear_apply() (+13 more)

### Community 77 - "FxRecord"
Cohesion: 0.08
Nodes (9): FxCommands, FxJson, FxNative, FxPacking, FxRecord, FxSpecs, FxType, FakeFxNative (+1 more)

### Community 78 - "JNIEXPORT"
Cohesion: 0.10
Nodes (26): project_grease_gp_annotation_style(), project_grease_gp_fx_set_enabled(), project_grease_gp_mirror_stroke(), project_grease_gp_mirror_stroke_about(), project_grease_gp_modifier_set_enabled(), project_grease_gp_set_layer_locked(), project_grease_gp_set_layer_use_mask(), project_grease_gp_set_layer_visibility() (+18 more)

### Community 79 - "ProjectGreaseSelect"
Cohesion: 0.12
Nodes (3): Command, ProjectGreaseSelect, NextBatchRulesTest

### Community 80 - "ProjectGreaseGPPoint"
Cohesion: 0.11
Nodes (17): project_grease_gp_add_point(), project_grease_gp_add_stroke(), project_grease_gp_get_point(), project_grease_gp_get_stroke_info(), project_grease_gp_set_point(), ProjectGreaseGPPoint, pressure, strength (+9 more)

### Community 81 - "test_blender_edit.c"
Cohesion: 0.20
Nodes (35): add_frame(), add_layer(), add_stroke(), BLI_lasso_boundbox(), frame_stroke_count(), list_add(), main(), make_gpd() (+27 more)

### Community 82 - "android_gp_shader_fx.cpp"
Cohesion: 0.09
Nodes (36): project_grease_android_present_reset(), bind_tex(), Buffer, color, reveal, compile(), count_passes(), draw_triangle() (+28 more)

### Community 83 - "EdgeFeatData"
Cohesion: 0.11
Nodes (17): EdgeFeatData, crease_threshold, edge_nabr, ld, material_indices, me, tri_array, use_auto_smooth (+9 more)

### Community 84 - "project_grease_blender_edit4.c"
Cohesion: 0.25
Nodes (23): pe4_copy_range(), pe4_deselect(), pe4_editable(), pe4_insert_after(), pe4_move_selected(), pe4_normal(), pe4_sel(), pe4_style() (+15 more)

### Community 86 - "main"
Cohesion: 0.11
Nodes (20): main(), add_point, add_stroke, begin_stroke, cancel_stroke, create_document, create_layer, create_material (+12 more)

### Community 87 - "StrokeRecord"
Cohesion: 0.10
Nodes (8): LayerRecord, MaskRecord, MaterialRecord, ParsedDocument, ParsedFrame, ParsedLayer, ProjectDocumentCodec, StrokeRecord

### Community 89 - "unit_m4"
Cohesion: 0.10
Nodes (12): unit_m4(), pg_sculpt_session_begin(), pg_sculpt_session_end(), pgt_sculpt_preset(), pgt_sculpt_update_geometry(), pg_tool_link_runtime(), pg_tool_material_style(), pg_tool_view_init() (+4 more)

### Community 91 - "LegacyGpBrushStrokeEngine"
Cohesion: 0.22
Nodes (3): LegacyGpBrushStrokeEngine, Settings, StrokePoint

### Community 92 - "ReferenceScene"
Cohesion: 0.09
Nodes (3): ReferenceCamera, ReferenceScene, ReferenceCameraTest

### Community 93 - "project_grease_blender_edit3.c"
Cohesion: 0.18
Nodes (20): pe3_apply_vcolor(), pe3_clampf(), pe3_editable(), pe3_fn_bc(), pe3_fn_levels(), pe3_frames_equal(), pe3_style(), pg_gp_blank_frame_add() (+12 more)

### Community 94 - "AnnotationData"
Cohesion: 0.27
Nodes (4): AnnotationData, Frame, Parsed, AnnotationDataTest

### Community 95 - "test_document_state.c"
Cohesion: 0.17
Nodes (17): clamp01(), pg_doc_layer_info_apply(), pg_doc_layer_info_get(), pg_doc_material_info_apply(), pg_doc_material_info_get(), pg_doc_point_color_apply(), pg_doc_point_color_get(), pg_doc_stroke_info_apply() (+9 more)

### Community 96 - "pg_stroke_outline"
Cohesion: 0.37
Nodes (11): add(), cross2(), dot2(), fan(), mul(), perp(), pg_stroke_outline(), safe_normalize() (+3 more)

### Community 97 - "DeviceBugfixTest.kt"
Cohesion: 0.09
Nodes (4): onUi(), openNewProject(), screen(), surface()

### Community 98 - "test_lineart.c"
Cohesion: 0.36
Nodes (10): pg_lite_scene_clear(), pg_lite_scene_free(), main(), run(), scene_with(), test_cube(), test_intersections(), test_loose_and_empty() (+2 more)

### Community 109 - "project_grease_blender_primitive.c"
Cohesion: 0.16
Nodes (19): pg_anchors_valid(), pg_blender_type(), pg_generate(), pg_resolve_edges(), pg_to_region(), pg_total_points(), project_grease_blender_primitive_default_edges(), project_grease_blender_primitive_generate() (+11 more)

### Community 110 - "project_grease_blender_edit2.c"
Cohesion: 0.20
Nodes (12): pe2_clampf(), pe2_insert_point(), pe2_stroke_editable(), pe2_style(), pg_gp_edit2_dispatch(), pg_gp_extrude(), pg_gp_mod_thickness_vgroup(), pg_gp_modifier_point_weight() (+4 more)

### Community 111 - "MainActivity.kt"
Cohesion: 0.09
Nodes (7): MainActivity, ProjectGreaseTheme(), ProjectGreaseThemeMode, DARK, LIGHT, SYSTEM, Settings()

### Community 112 - "cmath"
Cohesion: 0.19
Nodes (9): legacy_influence(), main(), legacy_pressure_curve(), legacy_spacing_accept(), main(), main(), near(), influence() (+1 more)

### Community 114 - "ColorMath"
Cohesion: 0.20
Nodes (4): ColorMath, Hsva, BlenderColorPicker(), ColorMathTest

### Community 116 - "CurveSession"
Cohesion: 0.16
Nodes (9): CurveSession, Phase, DRAG_LINE, EDIT, IDLE, Press, CONFIRM, HANDLE (+1 more)

### Community 118 - "ImportDialogs.kt"
Cohesion: 0.21
Nodes (4): rememberSvgImport(), toast(), ReferenceOverlay(), ReferenceSheet()

### Community 119 - "PGMeshLite"
Cohesion: 0.06
Nodes (36): BLI_task_parallel_range(), EdgeNeighborData, adj_e, edge_nabr, me, lineart_build_edge_neighbor(), EdgeFeatData, crease_threshold (+28 more)

### Community 121 - "project_grease_scene_lite.c"
Cohesion: 0.19
Nodes (16): BKE_camera_sensor_fit(), BKE_camera_sensor_size(), focallength_to_fov(), grow_push(), invert_m4(), lineart_matrix_ortho_44d(), lineart_matrix_perspective_44d(), mesh_free() (+8 more)

### Community 122 - "test_stroke_outline.c"
Cohesion: 0.39
Nodes (8): build(), covered(), inside(), main(), max_vertex_reach(), seg_dist(), turn_case(), uncovered_samples()

### Community 123 - "android_gpu_backend.cpp"
Cohesion: 0.18
Nodes (4): android_backend_get(), batch_alloc, indexbuf_alloc, GPUBackend::get()

### Community 125 - "project_grease_legacy_build.cpp"
Cohesion: 0.28
Nodes (4): pg_clear_stroke(), pg_fade(), pg_reduce(), project_grease_legacy_build_apply()

### Community 128 - "vgroup_at"
Cohesion: 0.20
Nodes (9): BLI_uniquename(), set_vertex_group_active, vertex_group_active, vertex_group_add, vertex_group_count, vertex_group_name, vertex_group_remove, vertex_group_rename (+1 more)

### Community 130 - "PGLineartSegment"
Cohesion: 0.18
Nodes (11): pg_lineart_free_segments(), pg_lineart_free_segments(), PGLineartSegment, edge_index, edge_type, object_index, occlusion, x0 (+3 more)

### Community 133 - "pg_lite_camera_orbit"
Cohesion: 0.27
Nodes (9): cross_v3(), pg_lite_camera_default(), pg_lite_camera_orbit(), pg_lite_scene_create(), pg_lite_stats(), count_flag(), main(), test_camera() (+1 more)

### Community 134 - "Settings"
Cohesion: 0.18
Nodes (10): bGPdata, bGPDframe, bGPDstroke, Settings, draw_strength, pointer_pressure, soft, soft_strength (+2 more)

### Community 137 - "PolylineSession"
Cohesion: 0.22
Nodes (4): PolylineSession, Release, CONTINUE, FINISH

### Community 138 - "project_grease_legacy_primitive.cpp"
Cohesion: 0.53
Nodes (9): arc(), bezier(), circle(), generate(), lerp(), line(), polyline(), rectangle() (+1 more)

### Community 140 - "Feature table"
Cohesion: 0.22
Nodes (8): Animation, Defects found by this audit, Device validation checklist (only you can produce this evidence), Feature table, Fill, paint, sculpt, Modifiers, Line Art, persistence, Project Grease audit, 2026-10-02, Selection and editing

### Community 142 - "ProjectGreaseGPLegacyGeometryOp"
Cohesion: 0.09
Nodes (21): project_grease_gp_apply_legacy_geometry_batch(), project_grease_gp_set_legacy_paint_settings(), ProjectGreaseGPLegacyGeometryOp, flag0, flag1, int0, int1, type (+13 more)

### Community 143 - "dna_meshdata_types"
Cohesion: 0.18
Nodes (3): CLG_log_str(), CLG_logf(), CLG_logref_init()

### Community 145 - "Settings"
Cohesion: 0.09
Nodes (22): bGPdata, bGPDframe, bGPDstroke, Settings, apply_position, apply_strength, apply_thickness, apply_uv (+14 more)

### Community 146 - "test_gp_color.c"
Cohesion: 0.47
Nodes (8): pg_gp_clamp01(), pg_gp_mix_vertex_color(), pg_gp_stroke_mean_mix(), main(), near3(), test_fill(), test_mean_mix(), test_mix()

### Community 150 - "PGObjectLite"
Cohesion: 0.16
Nodes (15): lineart_add_edge_to_array_thread(), lineart_geometry_check_visible(), lineart_geometry_load_assign_thread(), lineart_geometry_object_load(), lineart_intersection_mask_check(), lineart_intersection_priority_check(), lineart_object_load_single_instance(), lineart_object_load_worker() (+7 more)

### Community 151 - "LegacyPaintSettings"
Cohesion: 0.29
Nodes (7): set_legacy_paint_settings, LegacyPaintSettings, draw_smooth_factor, draw_smooth_level, input_samples, smooth_position, smooth_strength

### Community 154 - "LineartPointTri"
Cohesion: 0.50
Nodes (4): LineartPointTri, LRT_INSIDE_TRIANGLE, LRT_ON_TRIANGLE, LRT_OUTSIDE_TRIANGLE

### Community 156 - "jlong"
Cohesion: 0.11
Nodes (20): project_grease_gp_apply_blender_modifier_named(), project_grease_gp_create_layer(), project_grease_gp_create_material(), project_grease_gp_delete_layer(), project_grease_gp_flip_stroke(), project_grease_gp_modifier_apply(), project_grease_gp_rename_layer(), project_grease_gp_subdivide_stroke() (+12 more)

### Community 159 - "BLI_spin_lock"
Cohesion: 0.33
Nodes (7): BLI_spin_lock(), BLI_spin_unlock(), lineart_discard_segment(), lineart_give_segment(), lineart_occlusion_make_task_info(), lineart_occlusion_worker(), lineart_schedule_new_triangle_task()

### Community 161 - "apply_select_integration.py"
Cohesion: 0.60
Nodes (3): main(), patch(), replace_once()

### Community 163 - "GreaseTemplates.java"
Cohesion: 0.31
Nodes (3): GreaseTemplates, Material, Template

### Community 164 - "gen_lineart_lite.py"
Cohesion: 0.36
Nodes (4): generate(), item_spans(), main(), replacement_sections()

### Community 170 - "LineartData"
Cohesion: 0.13
Nodes (30): BLI_spin_init(), BLI_task_pool_create(), BLI_task_pool_free(), lineart_main_make_enclosed_shapes(), pg_lineart_compute_occlusion(), lineart_bounding_areas_connect_new(), lineart_bounding_areas_connect_recursive(), lineart_create_render_buffer() (+22 more)

### Community 171 - "TriData"
Cohesion: 0.25
Nodes (7): TriData, lineart_triangle_size, me, ob_info, tri_adj, tri_arr, vert_arr

### Community 172 - "LooseEdgeData"
Cohesion: 0.67
Nodes (3): LooseEdgeData, loose_array, loose_count

### Community 174 - "project_grease_lineart_lite.h"
Cohesion: 0.25
Nodes (7): PGLineartStroke, edge_type, first, level, object_index, point_count, PGSceneLite

### Community 180 - "Screen"
Cohesion: 0.40
Nodes (5): Screen, EDITOR, HOME, NEW, SETTINGS

### Community 183 - "DrawMode"
Cohesion: 0.40
Nodes (5): DrawMode, DRAW_INVERT, DRAW_NORMAL, DRAW_PREMULT, DRAW_REVEALAGE

## Knowledge Gaps
- **566 isolated node(s):** `display`, `context`, `surface`, `window`, `width` (+561 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1089 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **141 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `GPNative` connect `GPNative` to `.vertexGroupName`, `.duplicateLayer`, `NativeEditorBridge`, `ReferenceSheet.kt`, `HistoryController`, `.surfaceCreated`, `.vertexGroupCount`, `.deleteFrame`, `.deleteStroke`, `.duplicateStroke`, `.createFrame`, `.deleteLayer`, `.stats`, `.duplicateFrame`, `.endStroke`, `.deleteLastStroke`, `.createMaterial`, `.flipStroke`, `.frameEnd`, `.fxMove`, `.setLayerOpacity`, `.fxAdd`, `.historyCanRedo`, `ReferenceScene`, `.historyCanUndo`, `.hitTestStroke`, `.interpolateFrame`, `.layerCount`, `.layerName`, `.rotateStrokeAbout`, `.maskAdd`, `.vertexGroupRemove`, `.modifierApply`, `.maskRemove`, `.maskSetFlags`, `.materialCount`, `.selectFrameOrHold`, `.setLayerLocked`, `.mirrorStrokeAbout`, `MainActivity.kt`, `.modifierAdd`, `.strokeCount`, `.modifierCount`, `.modifierRemove`, `.modifierSetEnabled`, `.pointCount`, `.renameLayer`, `.resetDocument`, `FloatArray`, `.scaleStrokeAbout`, `.setLayerVisibility`, `.setOnionSkin`, `.splitStroke`, `.subdivideStroke`, `.trimStrokeToIntersection`, `.vertexGroupActive`?**
  _High betweenness centrality (0.029) - this node is a cross-community bridge._
- **Why does `NativeEditorBridge` connect `NativeEditorBridge` to `.vertexGroupName`, `.duplicateLayer`, `ReferenceSheet.kt`, `HistoryController`, `GPNative`, `.deleteFrame`, `.deleteStroke`, `.duplicateStroke`, `.createFrame`, `.deleteLayer`, `.stats`, `.duplicateFrame`, `.endStroke`, `.deleteLastStroke`, `.createMaterial`, `.flipStroke`, `.frameEnd`, `.fxMove`, `.setLayerOpacity`, `.fxAdd`, `.historyCanRedo`, `NativeDocumentAdapter`, `ModifierRecord`, `FxRecord`, `.historyCanUndo`, `.hitTestStroke`, `.interpolateFrame`, `.layerCount`, `.layerName`, `.rotateStrokeAbout`, `.maskAdd`, `.vertexGroupRemove`, `.modifierApply`, `.maskRemove`, `.maskSetFlags`, `.materialCount`, `.selectFrameOrHold`, `.setLayerLocked`, `.mirrorStrokeAbout`, `.modifierAdd`, `.strokeCount`, `.vertexGroupCount`, `.modifierCount`, `.modifierRemove`, `.modifierSetEnabled`, `.pointCount`, `.renameLayer`, `.resetDocument`, `FloatArray`, `.scaleStrokeAbout`, `.setLayerVisibility`, `.setOnionSkin`, `.splitStroke`, `.subdivideStroke`, `.trimStrokeToIntersection`, `.vertexGroupActive`?**
  _High betweenness centrality (0.023) - this node is a cross-community bridge._
- **Why does `FeatureId` connect `FeatureId` to `ProjectGreaseUI.kt`, `FeatureRegistry`, `.capability`?**
  _High betweenness centrality (0.022) - this node is a cross-community bridge._
- **What connects `display`, `context`, `surface` to the rest of the system?**
  _566 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `FeatureId` be split into smaller, more focused modules?**
  _Cohesion score 0.014285714285714285 - nodes in this community are weakly interconnected._
- **Should `project_grease_gp_bridge.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.06471631205673758 - nodes in this community are weakly interconnected._
- **Should `from_handle` be split into smaller, more focused modules?**
  _Cohesion score 0.053989488772097464 - nodes in this community are weakly interconnected._