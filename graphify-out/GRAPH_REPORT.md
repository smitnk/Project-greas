# Graph Report - Project-greas  (2026-10-02)

## Corpus Check
- 96 files · ~61,306 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 8 file(s) not represented in the graph (top: .xml 3, .properties 2, .jar 1)

## Summary
- 1480 nodes · 3312 edges · 109 communities (42 shown, 67 thin omitted)
- Extraction: 92% EXTRACTED · 8% INFERRED · 0% AMBIGUOUS · INFERRED: 257 edges (avg confidence: 0.84)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- UI Feature Registry Enums
- Native GP Bridge C API
- Brush Stroke Engine
- NativeEditorBridge
- Community 12
- Engine Feature Map
- Sculpt Engine and Tools
- New Project Screen
- Legacy Geometry Ops
- Backend Render and Transform
- Backend Internal State
- Activity and EGL Surface
- History and Materials API
- Editor Controllers
- GPU Batch Backend
- Vertex Buffer Backend
- Community 29
- EGL Renderer
- Community 32
- Stroke Drawing API
- Community 35
- Community 36
- Theme and Settings
- App State and Projects
- Fill and Shape Algorithms
- Stroke Point Data
- Batch VAO Backend
- Community 47
- Editor Sheet Types
- Feature Capability States
- App Screens
- Blender Math and Lasso Utils
- GP Modifier Smoothing
- GLES Draw Pipeline
- GPNative Kotlin Facade
- Editor Compose UI
- JNI Entry Points
- Blender String Utils
- Document and Frame API
- Blender Logging (CLG)
- Community 44
- Edit Command Backend
- Layer API
- Vertex Group Utils
- Core Probe Script
- Buffer Backend Probe Script
- Minimal Probe Script
- Route-A Probe Script
- Android Probe Workflows
- Blender Source Extraction
- GPU DRW Probe Script
- Native Link CI Diagnostics
- Acceptance and Status Docs
- Android Link Closure
- Scope and Architecture Docs
- Android EGL Transport
- Engine Worklog Docs

## God Nodes (most connected - your core abstractions)
1. `FeatureId` - 114 edges
2. `Backend` - 108 edges
3. `GPNative` - 100 edges
4. `ProjectGreaseGPHandle` - 89 edges
5. `ensure_ready()` - 80 edges
6. `from_handle()` - 74 edges
7. `NativeEditorBridge` - 72 edges
8. `main()` - 60 edges
9. `project_grease_gp_tag()` - 51 edges
10. `LegacyGpBrushStrokeEngine` - 31 edges

## Surprising Connections (you probably didn't know these)
- `Error-solving procedure (smallest missing closure)` --semantically_similar_to--> `Minimal dependency rule (evidence-driven imports)`  [INFERRED] [semantically similar]
  docs/PROJECT_GREASE_ENGINE_WORKLOG.md → PROJECT_GREASE_GP_SCOPE.md
- `detach_window()` --calls--> `project_grease_android_present_reset()`  [INFERRED]
  android/app/src/main/cpp/project_grease_android_egl_renderer.cpp → native/blender_gp/android_gp_presentation.cpp
- `Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetCanvasSize()` --calls--> `project_grease_android_present_set_canvas_size()`  [INFERRED]
  android/app/src/main/cpp/project_grease_android_egl_renderer.cpp → native/blender_gp/android_gp_presentation.cpp
- `Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetViewTransform()` --calls--> `project_grease_android_present_set_view_transform()`  [INFERRED]
  android/app/src/main/cpp/project_grease_android_egl_renderer.cpp → native/blender_gp/android_gp_presentation.cpp
- `Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeSetStrokeColorEglRenderer()` --calls--> `project_grease_android_present_set_color()`  [INFERRED]
  android/app/src/main/cpp/project_grease_android_egl_renderer.cpp → native/blender_gp/android_gp_presentation.cpp

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Android Blender GP probe workflows (core, minimal, gpu-draw, buffer-backend, native-link, route-a)** — github_workflows_android_blender_gp_core_probe, github_workflows_android_blender_gp_minimal_probe, github_workflows_android_blender_gp_gpu_draw_probe, github_workflows_android_blender_gp_minimal_buffer_backend, github_workflows_android_blender_gp_native_link_probe, github_workflows_android_blender_gp_route_a_link_probe [EXTRACTED 1.00]
- **Focused Android Legacy GP native closure (manifest + JNI CMake + link probe)** — tools_android_gp_source_manifest, android_app_src_main_cpp_cmakelists, tools_android_blender_gp_native_link_probe, native_blender_gp_project_grease_android_link_closure_notes [INFERRED 0.90]
- **Evidence-driven minimal Blender dependency philosophy** — project_grease_gp_scope_minimal_dependency_rule, docs_project_grease_legacy_gp_engine_plan_no_approximation_rule, docs_project_grease_engine_worklog_error_solving_procedure, native_blender_gp_readme_rules, native_blender_gp_ci_link_diagnostic [INFERRED 0.85]

## Communities (109 total, 67 thin omitted)

### Community 0 - "UI Feature Registry Enums"
Cohesion: 0.02
Nodes (109): FeatureId, ADD_FRAME, ADVANCED_FILL, ADVANCED_INTERPOLATION, ADVANCED_ONION_SKIN, ARC, CIRCLE, CLOSE (+101 more)

### Community 1 - "Native GP Bridge C API"
Cohesion: 0.07
Nodes (87): ProjectGreaseGPHandle, main(), ensure_ready(), project_grease_gp_add_point(), project_grease_gp_apply_blender_modifier(), project_grease_gp_apply_blender_modifier_named(), project_grease_gp_apply_blender_modifier_stack(), project_grease_gp_apply_edit_command() (+79 more)

### Community 10 - "Brush Stroke Engine"
Cohesion: 0.14
Nodes (5): InputEvent, LegacyGpBrushStrokeEngine, Settings, StrokePoint, LegacyGpBrushStrokeEngineTest

### Community 13 - "Engine Feature Map"
Cohesion: 0.07
Nodes (28): EngineFeature, EngineFeatureState, engine_feature_map(), Drawing, Editing, Eraser, Fill, Frames (+20 more)

### Community 14 - "Sculpt Engine and Tools"
Cohesion: 0.17
Nodes (12): LegacyGpSculptEngine, Point, Settings, Tool, GRAB, PINCH, PUSH, RANDOMIZE (+4 more)

### Community 15 - "New Project Screen"
Cohesion: 0.08
Nodes (3): Preset, ToolEntry, NewProject()

### Community 16 - "Legacy Geometry Ops"
Cohesion: 0.07
Nodes (28): LegacyGeometryOp, LegacyGeometryOpType, flag0, flag1, int0, int1, type, value0 (+20 more)

### Community 17 - "Backend Render and Transform"
Cohesion: 0.12
Nodes (24): Backend, Engine, selected_point_count(), hit_test_stroke, stroke_center, translate_stroke, rotate_stroke, rotate_stroke_about (+16 more)

### Community 18 - "Backend Internal State"
Cohesion: 0.08
Nodes (25): LegacyPaintSettings, impl_, set_legacy_paint_settings, document_created, frame, frame_created, gpd, gpu_external_context (+17 more)

### Community 21 - "History and Materials API"
Cohesion: 0.14
Nodes (16): HistorySnapshot, gp_material_at(), history_copy_settings(), history_gp_duplicate(), history_restore_snapshot(), history_snapshot_clear(), history_snapshot_create(), history_snapshot_free() (+8 more)

### Community 26 - "GPU Batch Backend"
Cohesion: 0.13
Nodes (8): GPUBatch, android_backend_get(), GPUBackend::get(), project_grease_android_gpu_configure_batch(), project_grease_android_gpu_draw_batch(), project_grease_android_gp_cache_upload_probe(), project_grease_android_gpu_buffer_backend_probe(), indexbuf_alloc

### Community 27 - "Vertex Buffer Backend"
Cohesion: 0.14
Nodes (3): AndroidVertBuf, vertbuf_alloc, vbo_id_

### Community 3 - "EGL Renderer"
Cohesion: 0.08
Nodes (45): Renderer, attach_window(), choose_config(), connect_blender_gp(), create_context(), destroy_renderer(), detach_window(), disconnect_blender_gp() (+37 more)

### Community 34 - "Stroke Drawing API"
Cohesion: 0.19
Nodes (13): StrokeStyle, ED_gpencil_sbuffer_ensure(), create_primitive, create_polyline, get_point, set_point, begin_stroke, add_point (+5 more)

### Community 37 - "Theme and Settings"
Cohesion: 0.18
Nodes (6): ProjectGreaseThemeMode, ProjectGreaseTheme(), Settings(), DARK, LIGHT, SYSTEM

### Community 38 - "App State and Projects"
Cohesion: 0.23
Nodes (5): GreaseUiState, ProjectRecord, ProjectStore, Home(), ProjectGreaseApp()

### Community 4 - "Fill and Shape Algorithms"
Cohesion: 0.07
Nodes (41): Image, Point, Result, Point, legacy_influence(), main(), boundary_fill(), contract_shape() (+33 more)

### Community 40 - "Stroke Point Data"
Cohesion: 0.15
Nodes (13): StrokePoint, a, b, g, pressure, r, strength, time (+5 more)

### Community 41 - "Batch VAO Backend"
Cohesion: 0.22
Nodes (3): AndroidBatch, batch_alloc, vao_id_

### Community 48 - "Editor Sheet Types"
Cohesion: 0.22
Nodes (9): Sheet, ADVANCED, LAYERS, MATERIALS, MORE, NONE, ONION, PROJECT (+1 more)

### Community 51 - "Feature Capability States"
Cohesion: 0.32
Nodes (6): FeatureCapability, FeatureRegistry, FeatureState, AVAILABLE, IN_PROGRESS, NOT_IMPLEMENTED

### Community 56 - "App Screens"
Cohesion: 0.40
Nodes (5): Screen, EDITOR, HOME, NEW, SETTINGS

### Community 6 - "Blender Math and Lasso Utils"
Cohesion: 0.06
Nodes (28): bGPdata, bGPDframe, bGPDstroke, Settings, BLI_lasso_is_point_inside(), closest_to_line_segment_v2(), closest_to_line_segment_v3(), closest_to_line_v2() (+20 more)

### Community 7 - "GP Modifier Smoothing"
Cohesion: 0.06
Nodes (34): bGPdata, bGPDframe, bGPDstroke, Context, Settings, Tool, apply(), apply_position_smooth() (+26 more)

### Community 8 - "GLES Draw Pipeline"
Cohesion: 0.12
Nodes (23): Vertex, append_dot(), append_fill(), append_segment(), compile_shader(), draw_frame(), draw_sbuffer(), draw_vertices() (+15 more)

### Community 19 - "Editor Compose UI"
Cohesion: 0.29
Nodes (15): AdvancedSheet(), CapabilityRow(), DrawingGuidesOverlay(), Editor(), FpsDialog(), LayersSheet(), MaterialsSheet(), ModeBrushBar() (+7 more)

### Community 2 - "JNI Entry Points"
Cohesion: 0.15
Nodes (75): from_handle(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeAddPoint(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyBlenderModifier(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyEditCommand(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeApplyLegacyGeometry(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeBeginStroke(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeClearSelection(), Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeCloseStroke() (+67 more)

### Community 22 - "Blender String Utils"
Cohesion: 0.17
Nodes (14): BLI_snprintf(), BLI_snprintf_rlen(), BLI_string_split_name_number(), BLI_strncpy(), BLI_strncpy_utf8(), BLI_strncpy_utf8_rlen(), BLI_strnlen(), BLI_uniquename() (+6 more)

### Community 33 - "Document and Frame API"
Cohesion: 0.18
Nodes (13): main(), gp_material_ensure_slot(), gp_materials_free(), cancel_stroke, initialize_external_gpu_context, create_material, last_error, initialize (+5 more)

### Community 39 - "Blender Logging (CLG)"
Cohesion: 0.17
Nodes (3): CLG_log_str(), CLG_logf(), CLG_logref_init()

### Community 5 - "Edit Command Backend"
Cohesion: 0.10
Nodes (47): main(), project_grease_gp_tag(), project_grease_point_in_polygon(), delete_frame, stroke_count, point_count, select_stroke, erase_at (+39 more)

### Community 52 - "Layer API"
Cohesion: 0.29
Nodes (7): layer_at(), layer_count, set_layer_visibility, set_layer_locked, move_layer, delete_layer, rename_layer

### Community 53 - "Vertex Group Utils"
Cohesion: 0.57
Nodes (4): BKE_defvert_array_copy(), BKE_defvert_ensure_index(), BKE_defvert_find_index(), BKE_defvert_find_weight()

### Community 23 - "Android Probe Workflows"
Cohesion: 0.19
Nodes (15): android_blender_gp_generate_dna.sh script, android_blender_gp_native_link_probe.sh script, import_blender_gp.sh script, Android JNI CMakeLists (projectgrease_jni), Workflow: Android Blender GP Core Probe, Workflow: Android Blender GP GPU DRW Closure Probe, Workflow: Android Blender GP Minimal Buffer Backend, Workflow: Android Blender GP Minimal Probe (+7 more)

### Community 42 - "Blender Source Extraction"
Cohesion: 0.24
Nodes (8): group_of(), select_gp(), extract_grease_pencil.sh script, Workflow: Extract Blender 3.6.23 Grease Pencil Source, Blender 3.6.23 Legacy Grease Pencil (pinned baseline), Blender GP Import Manifest (docs), Blender GP Source Import Manifest (native), Native Legacy GP Backend README

### Community 59 - "GPU DRW Probe Script"
Cohesion: 0.83
Nodes (3): compile_one(), run_group(), android_blender_gp_gpu_drw_probe.sh script

### Community 31 - "Native Link CI Diagnostics"
Cohesion: 0.26
Nodes (14): Workflow: Native Blender GP Backend, Native blender_gp CMakeLists, gpencil_geom_legacy.cc (BKE GP geometry), project_grease_legacy_fill/primitive/eraser/sculpt static libs, project_grease_blender_gp static library target, project_grease_gp_bridge library, project_grease_gp_bridge_simulation executable, project_grease_gp_external_context_test executable (+6 more)

### Community 46 - "Acceptance and Status Docs"
Cohesion: 0.20
Nodes (8): Verification gates for COMPLETE feature status, Target dependency closure (11 tiers of Legacy GP), Feature-to-code mapping table with AVAILABLE/IN_PROGRESS/NOT_IMPLEMENTED status, FeatureRegistry (single capability source for UI), Transform selected points only (3.6.23 Legacy GP transform-conversion rule), Legacy GP Engine Acceptance Matrix, Final Legacy GP Engine Plan, Native Backend Status

### Community 49 - "Android Link Closure"
Cohesion: 0.25
Nodes (9): ED_gpencil_sbuffer_ensure, GP_STROKE_BUFFER_CHUNK constant (2048), project_grease_legacy_sbuffer.c focused extraction, Android GP Source Manifest (authoritative closure), android_gpu_backend.cpp / android_gp_presentation.cpp, draw_cache_impl_gpencil.cc, Real Legacy GP paint stroke buffer (sbuffer / tGPspoint) bundle, CXX|/C| source entries consumed by JNI target and link probe (+1 more)

### Community 50 - "Scope and Architecture Docs"
Cohesion: 0.31
Nodes (7): Compose UI controllers (EditorController, ToolController, DocumentController, SelectionController, etc.), Legacy GP data model (bGPdata/bGPDlayer/bGPDframe/bGPDstroke/bGPDspoint), Production rendering path (touch -> JNI -> GP data -> draw cache -> GLES backend), Android UI -> controllers -> JNI -> Legacy GP -> GLES architecture, UI / Engine Feature Mapping, Project Grease GP Engine Scope, Project Grease README

### Community 55 - "Android EGL Transport"
Cohesion: 0.40
Nodes (5): projectgrease_jni shared library, Native EGL renderer (clear and present only), Android SurfaceView -> ANativeWindow -> EGL -> GLES2 transport, Android-owned EGL/GLES context, Android Shell README

### Community 57 - "Engine Worklog Docs"
Cohesion: 0.40
Nodes (3): project_grease_gp_engine host-facing engine boundary, Real Legacy GP modifier execution (deformStroke callbacks), Bundled Blender Legacy GP Engine Worklog

## Knowledge Gaps
- **301 isolated node(s):** `display`, `context`, `surface`, `window`, `width` (+296 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 489 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **67 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `GPNative` connect `GPNative Kotlin Facade` to `NativeEditorBridge`, `Sculpt Engine and Tools`, `Activity and EGL Surface`, `Editor Controllers`, `Community 28`, `Community 29`, `Kotlin Sculpt Math`, `Community 60`, `Community 61`, `Community 62`, `Community 63`, `Community 64`, `Community 65`, `Community 66`, `Community 67`, `Community 68`, `Community 69`, `Community 70`, `Community 71`, `Community 72`, `Community 73`, `Community 74`, `Community 75`, `Community 76`, `Community 77`, `Community 78`, `Community 79`, `Community 80`, `Community 81`, `Community 82`, `Community 83`, `Community 84`, `Community 85`, `Community 86`, `Community 87`, `Community 88`, `Community 89`, `Community 90`, `Community 91`, `Community 92`, `Community 93`, `Community 94`, `Community 95`, `Community 96`, `Community 97`, `Community 98`?**
  _High betweenness centrality (0.068) - this node is a cross-community bridge._
- **Why does `FeatureId` connect `UI Feature Registry Enums` to `Editor Compose UI`, `Feature Capability States`, `New Project Screen`?**
  _High betweenness centrality (0.057) - this node is a cross-community bridge._
- **Why does `Backend` connect `Backend Render and Transform` to `Document and Frame API`, `Stroke Drawing API`, `Native GP Bridge C API`, `Edit Command Backend`, `Backend Tests and Headers`, `Legacy Geometry Ops`, `Backend Internal State`, `Layer API`, `History and Materials API`?**
  _High betweenness centrality (0.030) - this node is a cross-community bridge._
- **What connects `display`, `context`, `surface` to the rest of the system?**
  _301 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `UI Feature Registry Enums` be split into smaller, more focused modules?**
  _Cohesion score 0.01834862385321101 - nodes in this community are weakly interconnected._
- **Should `Native GP Bridge C API` be split into smaller, more focused modules?**
  _Cohesion score 0.06593406593406594 - nodes in this community are weakly interconnected._
- **Should `Brush Stroke Engine` be split into smaller, more focused modules?**
  _Cohesion score 0.14015151515151514 - nodes in this community are weakly interconnected._