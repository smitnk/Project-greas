# Blender 3.6.23 Legacy Grease Pencil — Dependency-Closure Audit

**Project:** Project Grease (smitnk/Project-greas)
**Status:** 26/26 modifiers VERIFIED (other areas still IN PROGRESS) (research only, no implementation)
**Goal:** FULL Blender 3.6.23 Legacy GP behavior on Android. Port whatever the real Blender code path requires; replace only platform/UI infrastructure.
**Source:** official Blender repo, branch `blender-v3.6-release` (3.6.23 is a patch release on it).

## Classification key
- **REQUIRED** — must port for exact Legacy GP behavior
- **PLATFORM REPLACE** — desktop infra replaceable without changing GP behavior
- **FEATURE OPTIONAL** — only if that separate feature is dropped
- **NOT REQUIRED** — proven unrelated by source tracing
- **UNVERIFIED** — not yet read from source

## Evidence (files read from source)
| ID | File |
|---|---|
| E1 | source/blender/gpencil_modifiers_legacy/CMakeLists.txt |
| E2 | source/blender/blenkernel/intern/gpencil_legacy.c |
| E3 | source/blender/editors/gpencil_legacy/CMakeLists.txt |
| E4 | source/blender/draw/engines/gpencil/gpencil_engine.c |
| E5 | source/blender/io/gpencil/CMakeLists.txt |
| E6 | gpencil_modifiers_legacy/intern/lineart/lineart_intern.h |
| E8 | source/blender/blenkernel/intern/gpencil_modifier_legacy.c |
| E9 | source/blender/draw/intern/draw_cache_impl_gpencil.cc |
| E10 | source/blender/blenkernel/intern/shrinkwrap.cc |
| E12 | gpencil_modifiers_legacy/intern/lineart/lineart_cpu.cc (includes + calls) |
| E13–E17 | MOD_gpencil_legacy_armature.c, _util.c, _outline.c, _hook.c, _build.c |
| E18 | source/blender/blenkernel/intern/gpencil_geom_legacy.cc |
| E19–E21 | MOD_gpencil_legacy_lattice.c, _weight_proximity.c, _shrinkwrap.c |
| E22–E25 | MOD_gpencil_legacy_time.c, _texture.c, _noise.c, _mirror.c |
| E26 | source/blender/blenkernel/intern/gpencil_curve_legacy.c |
| E27 | source/blender/blenkernel/intern/DerivedMesh.cc |
| E28 | MOD_gpencil_legacy_envelope.c |
| E29 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_array.c |
| E30 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_offset.c |
| E31 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_length.c |
| E32 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_simplify.c |
| E33 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_subdiv.c |
| E34 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_multiply.c |
| E35 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_color.c |
| E36 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_opacity.c |
| E37 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_dash.c |
| E38 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_smooth.c |
| E39 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_tint.c |
| E40 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_thick.c |
| E41 | gpencil_modifiers_legacy/intern/MOD_gpencil_legacy_weight_angle.c |
| E42 | source/blender/blenkernel/intern/object_update.cc (`BKE_object_handle_data_update`, `BKE_object_eval_uber_data`) |

## Verified conclusions

### 1. Depsgraph — full Blender depsgraph REQUIRED
- Relation builder: `DEG_add_object_relation` (TRANSFORM, GEOMETRY, PARAMETERS, EVAL_POSE), `DEG_add_customdata_mask`, `DEG_add_special_eval_flag`, `DEG_add_depends_on_transform_relation`.
- Copy-on-write: `LIB_TAG_COPIED_ON_WRITE`, `DEG_get_original_id`, `ob->runtime.gpd_eval`, orig/eval pointers on strokes/points/frames (E8).
- Queries: `DEG_get_ctime`, `DEG_get_mode` (viewport vs render), `DEG_is_active`, `DEG_get_eval_flags_for_id`, `DEG_get_customdata_mask_for_object`.
- Object iteration incl. duplis + set scenes (Line Art, E12).
- `BKE_scene_graph_update_for_newframe` used by modifier bake/apply with retime (E14).

### 2. Mesh evaluation — REQUIRED for Shrinkwrap/Line Art targets
- `DerivedMesh.cc` `mesh_calc_modifiers`: full mesh modifier stack incl. Geometry Nodes geometry sets, shape-key virtual modifiers, orco, origindex, split normals (E27).
- Edit-mode path `editbmesh_calc_modifiers` uses BMesh.

### 3. Shrinkwrap boundary path (fully traced)
GP Shrinkwrap `updateDepsgraph` → `DEG_add_special_eval_flag(DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY)` + `DEG_add_customdata_mask(CD_MASK_NORMAL|CD_MASK_CUSTOMLOOPNORMAL)` (E21)
→ `makeDerivedMesh` → `mesh_build_data` → `mesh_calc_modifiers` → `mesh_build_extra_data` → `DEG_get_eval_flags_for_id` → `BKE_shrinkwrap_compute_boundary_data` (E27)
→ `mesh->runtime->shrinkwrap_data` → `BKE_shrinkwrap_init_tree` → `target_project_edge` (E10).
**Not computed in edit mode** (`editbmesh_build_data` skips `mesh_build_extra_data`) — replicate.
Shrinkwrap also needs: BVH (`BKE_bvhtree_from_mesh_get`, `BLI_bvhtree_*`), mesh wrapper, normals, custom normals, `sharp_face`, `BLI_task`, `BLI_newton3d_solve`.

### 4. GPU — GLES 3.2 class REQUIRED (E4, E9)
- Vertex pulling: VBOs with `GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY` read as texture buffers (`gp_pos_tx`, `gp_col_tx`); bit-packed vertex IDs `(v << GP_VERTEX_ID_SHIFT) | GP_IS_STROKE_VERTEX_BIT`.
- Formats: `pos` F32x4, `ma` I32x4 (int fetch), `uv` F32x4; `col`/`fcol` F32x4; edit `vflag` U32 + `weight` F32; curve `pos` F32x3 + `data` U32.
- 32-bit index buffers + primitive restart 0xFFFFFFFF; TRIS, LINE_STRIP, LINES, POINTS; multi-VBO batches; ranged draws; procedural fullscreen triangles.
- UBOs (materials, lights); textures RGBA8, RGBA16F, R11F_G11F_B10F, DEPTH24_STENCIL8, R8, R16; 3-attachment MRT; framebuffer blit; multi-clear.
- `DRW_STATE_LOGIC_INVERT` has no GLES equivalent → shader emulation.
- Vertex layout and bit packing must stay identical (shaders depend on it).
- Live stroke (sbuffer) depends on Brush gpencil_settings and `ED_gpencil_tpoint_to_point` / `ED_gpencil_drawing_reference_get`.

### 5. Line Art (E6, E12)
- `DEG_OBJECT_ITER` (linked directly, via set, visible, dupli), `BKE_collection_has_object(_recursive_instanced)`, `BKE_object_visibility`.
- Geometry: `BKE_object_get_evaluated_mesh` (meshes; skips edit mode), `BKE_mesh_new_from_object` (metaball, legacy curve, surface, text).
- Freestyle edge/face marks, `sharp_edge`/`sharp_face` attributes, material indices, deform weights.
- Camera: modifier source camera → `RE_GetSceneRender(scene)->camera_override` → `BKE_scene_camera_switch_update` (markers); `BKE_camera_sensor_fit/size`.
- Light contour object (Light DNA; LA_SUN = ortho).
- Threads: `BKE_render_num_threads`, `BLI_task_parallel_range`, `BLI_task_pool_*`. CPU only, no GPU.

### 6. GP Geometry (E18)
`BKE_gpencil_stroke_geometry_update` = edit-curve sync → `BKE_gpencil_stroke_fill_triangulate` (`BLI_polyfill_calc`, fill UVs) → `uv_update` → bbox.
Also: sample, smooth (binomial kernel), simplify adaptive/fixed, subdivide, uniform_subdivide (`BLI_heap`), stretch, shrink, trim, split, close, join, merge_distance, dissolve, delete_tagged_points (retimes `time`/`inittime`), perimeter_from_view, convert_mesh, boundbox orig/eval sync.

### 7. GP Curve (E26) — REQUIRED
`extern/curve_fit_nd` (`curve_fit_cubic_to_points_refit_fl`), `BKE_curve_forward_diff_bezier`, `BKE_nurb_handle_calc`, `BKE_curve_calc_coords_axis_len`. Curve→GP convert (`BKE_nurb_makeCurve`, scene collections) is FEATURE OPTIONAL.

### 8. Behavior-critical details
- Noise seed = seed + stroke index + `BLI_hash_string(object name)` + `BLI_hash_string(modifier name)` (+ frame/step) — names and hash must match Blender exactly.
- Build draw-speed mode reads per-point `time` + stroke `inittime` → paint tool must record identical timing.
- Hook writes `ob->world_to_object` on the evaluated object as a side effect.
- Envelope filter min points = 3; generated strokes are prepended (drawn underneath).

### 9. Findings from the 13 remaining modifiers (E29–E41)
- None of the 13 uses `DEG_add_customdata_mask`, `DEG_add_special_eval_flag`, `remapTime` or `dependsOnTime`; none needs mesh evaluation.
- Generators (Array, Multiply, Dash) evaluate on `BKE_gpencil_frame_retime_get` (time-offset aware frame); their bake paths do not use `generic_bake_deform_stroke`.
- Only Tint bakes with retime = true (→ `BKE_scene_graph_update_for_newframe`); all other deform bakes use retime = false.
- Random patterns (Array, Offset, Length) need bit-exact `BLI_hash_string`, `BLI_hash_int_01`, `BLI_hash_int_2d`, `BLI_halton_2d/3d` and the float `sin`-hash formula.
- Material colour fallback (Color, Tint) reads `MaterialGPencilStyle` via `BKE_gpencil_material_settings`.

### 10. Depsgraph → mesh evaluation entry (E42)
`deg_builder_nodes.cc` → `BKE_object_eval_uber_data` → `BKE_object_handle_data_update` →
- `OB_MESH`: masks = `scene->customdata_mask` + BAREMESH + PROP_ALL/CREASE/MDEFORMVERT (+ FREESTYLE_EDGE/FACE with WITH_FREESTYLE, + ORCO in render mode) → `makeDerivedMesh` (E27).
- `OB_GPENCIL_LEGACY`: `BKE_gpencil_prepare_eval_data` → `BKE_gpencil_modifiers_calc` → `BKE_gpencil_update_layer_transforms`; then `BKE_object_batch_cache_dirty_tag`.
- Line Art needs the FREESTYLE masks → port must define WITH_FREESTYLE-equivalent mask behaviour.
- Other caller: `crazyspace.cc` (edit-mode, CD_MASK_BAREMESH).

### 11. New REQUIRED dependencies (from E29–E42)
- BLI_rand: `BLI_halton_2d`, `BLI_halton_3d`; BLI_hash: `BLI_hash_int_01`, `BLI_hash_int_2d` (with `BLI_hash_string`).
- BLI_math_color: `rgb_to_hsv_v`, `hsv_to_rgb_v`.
- BLI_math: `loc_eul_size_to_mat4`, `mat4_to_scale`, `rotate_normalized_v3_v3v3fl`, `angle_on_axis_v3v3_v3`, `interpf`, `fractf`.
- BKE_colorband: `BKE_colorband_add`, `BKE_colorband_init`, `BKE_colorband_evaluate` (Tint).
- BKE_colortools CurveMapping: `BKE_curvemapping_add/init/copy/free/evaluateF` (Color, Opacity, Smooth, Tint, Thick).
- BKE_material: `BKE_gpencil_material_settings`.
- BKE_deform: `BKE_object_defgroup_name_index`, `BKE_defvert_ensure_index`.
- BKE_gpencil_legacy: `BKE_gpencil_stroke_new`, `BKE_gpencil_stroke_duplicate`, `BKE_gpencil_free_stroke`, `BKE_gpencil_dvert_ensure`.
- BKE_gpencil_geom_legacy: `stroke_minmax`, `stroke_boundingbox_calc`, `stroke_normal`, `stroke_length`, `stroke_stretch`, `stroke_shrink`, `stroke_simplify_fixed/adaptive`, `stroke_sample`, `stroke_merge_distance`, `stroke_subdivide`, `stroke_smooth`, `stroke_geometry_update`.
- BKE_gpencil_modifier_legacy: `BKE_gpencil_frame_retime_get`, `generic_bake_deform_stroke`, `BKE_gpencil_modifier_copydata_generic`.
- DNA: `DNA_struct_default_alloc` (Dash segments), all 13 `*GpencilModifierData` defaults.
- Depsgraph: `BKE_object_eval_uber_data`/`BKE_object_handle_data_update` dispatch incl. scene customdata mask merge (E42).

## Modifier status
| Modifier | Status | Notable dependencies |
|---|---|---|
| Armature | VERIFIED | `BKE_armature_deform_coords_with_gpencil_stroke`, Pose, DEG EVAL_POSE, retime bake |
| Hook | VERIFIED | pose channel, curvemapping (8 falloffs), retime bake |
| Outline | VERIFIED | scene camera + marker switch, `stroke_perimeter_from_view`, layer matrix |
| Build | VERIFIED | scene fps, CoW orig pointers, point timing, NoApply |
| Lattice | VERIFIED | `lattice_deform_data_create/eval_co`, retime bake |
| WeightProximity | VERIFIED | object matrices, vertex groups |
| Shrinkwrap | VERIFIED | evaluated mesh, BVH, eval flag, CD masks, smooth after deform |
| Time | VERIFIED | `remapTime`, scene sfra/efra, segments, NoApply |
| Texture | VERIFIED | fill/stroke UV transforms |
| Noise | VERIFIED | `BLI_hash` incl. name strings, curvemap bell preset |
| Mirror | VERIFIED | object-space mirror, retime bake |
| Envelope | VERIFIED | plane/conic radius limit, segments/fills generation |
| Line Art | VERIFIED (via lineart_cpu.cc) | see section 5 |
| Array | VERIFIED (E29) | `generateStrokes` + `bakeModifier` (both call `BKE_gpencil_frame_retime_get`, no `generic_bake_deform_stroke`); `BKE_gpencil_stroke_duplicate`, `BKE_gpencil_stroke_minmax` (relative offset, on `gpl->actframe`), `BKE_gpencil_stroke_boundingbox_calc`; offset object via `object_to_world` inverse; seed = seed + `BLI_hash_string`(ob name, md name) → `BLI_hash_int_01`; `BLI_halton_3d` primes {2,3,7} + `fmodf(sin(x*12.9898+j*78.233)*43758.5453)`; `loc_eul_size_to_mat4`; copies prepended (`BLI_addhead`); material override; DEG: offset object GEOMETRY+TRANSFORM, self TRANSFORM; flag SupportsEditmode |
| Offset | VERIFIED (E30) | `deformStroke`; bake retime **false**; vertex group (`BKE_object_defgroup_name_index`, `get_modifier_point_weight`); modes Random (Halton by stroke index + name hash) / Stroke / Material / Layer step-offset formula; pressure scaled by mean abs scale; `BKE_gpencil_stroke_geometry_update`; DEG: self TRANSFORM; SupportsEditmode |
| Length | VERIFIED (E31) | `deformStroke` (skips cyclic strokes); bake retime false; `BKE_gpencil_stroke_length`, `BKE_gpencil_stroke_stretch` (curvature params), `BKE_gpencil_stroke_shrink`, geometry_update; random: name hashes + `DEG_get_ctime/step` when `use_random`, `BLI_hash_int_2d` noise table, `BLI_halton_2d` {2,3}; second overshoot HACK formula must be copied verbatim; no updateDepsgraph; `dependsOnTime` = NULL although it reads ctime; SupportsEditmode |
| Simplify | VERIFIED (E32) | `deformStroke`; bake retime false; modes → `BKE_gpencil_stroke_simplify_fixed` (×step), `_simplify_adaptive` (RDP), `_sample` (length, sharp_threshold), `_merge_distance` (with gpf); min points 2 (sample) / 3; no updateDepsgraph; SupportsEditmode |
| Subdiv | VERIFIED (E33) | `deformStroke`; bake retime false; `BKE_gpencil_stroke_subdivide(level, type)`, forced SIMPLE when < 3 points; min points 2; SupportsEditmode |
| Multiply | VERIFIED (E34) | `generateStrokes` (uses `BKE_gpencil_frame_retime_get`); `bakeModifier` loops **all frames** directly; `BKE_gpencil_stroke_normal`, `mat4_to_scale(object_to_world)`, `BKE_gpencil_stroke_duplicate`, geometry_update (only last processed stroke = original); fading on pressure/strength; duplicates appended via `BLI_movelisttolist`; filter uses `GP_MIRROR_INVERT_*` constants (quirk — keep); flags **0** (no edit mode); no updateDepsgraph |
| Color (Hue/Saturation) | VERIFIED (E35) | `deformStroke`; bake retime false; `BKE_gpencil_material_settings` (material colour fallback when vertex colour alpha = 0), `rgb_to_hsv_v`/`hsv_to_rgb_v` (BLI_math_color), CurveMapping (`BKE_curvemapping_add/init/copy/free/evaluateF`); SupportsEditmode |
| Opacity | VERIFIED (E36) | `deformStroke`; bake retime false; hardness mode (`gps->hardeness`), normalize / weight-factor branches, vertex group, CurveMapping, `fill_opacity_fac` from first point weight; SupportsEditmode |
| Dash (Dot Dash) | VERIFIED (E37) | `generateStrokes` (`BKE_gpencil_frame_retime_get`), bake loops all frames; `isDisabled` (sequence length < 1); `BKE_gpencil_stroke_new`, `BKE_gpencil_dvert_ensure`, `BKE_gpencil_free_stroke`, geometry_update; copies `gps_orig`/`pt_orig`/`idx_orig` runtime pointers (CoW orig mapping); segment array (`DNA_struct_default_alloc(DashGpencilModifierSegment)`, `MEM_dupallocN`); operators `GPENCIL_OT_segment_add/remove/move` + `MOD_UL_dash_segment` UI list (PLATFORM REPLACE, keep behavior); SupportsEditmode |
| Smooth | VERIFIED (E38) | `deformStroke`; bake retime false; `BKE_gpencil_stroke_smooth(factor, step, location, strength, thickness, uv, keep_shape, weights)`; per-point weights from vertex group × CurveMapping; min points 3; SupportsEditmode |
| Tint | VERIFIED (E39) | `deformStroke`; bake retime **true** → `BKE_scene_graph_update_for_newframe` (only one of these 13); ColorBand (`BKE_colorband_add/init/evaluate`), CurveMapping, `BKE_gpencil_material_settings`; gradient mode uses target object `world_to_object` × `object_to_world`, radius; `isDisabled` (gradient w/o object); DEG: object GEOMETRY+TRANSFORM, self TRANSFORM; SupportsEditmode |
| Thick | VERIFIED (E40) | `deformStroke`; bake retime false; normalized (`thickness / gps->thickness`) vs factor mode, weight-factor branch, vertex group, CurveMapping `curve_thickness`; SupportsEditmode |
| WeightAngle | VERIFIED (E41) | `deformStroke`; bake retime false; **writes** vertex weights: `BKE_gpencil_dvert_ensure`, `BKE_defvert_ensure_index`; `rotate_normalized_v3_v3v3fl`, `angle_on_axis_v3v3_v3`, `mul_mat3_m4_v3`/`object_to_world` (world/local space); `isDisabled` (no target group); flags **0** (no edit mode); no updateDepsgraph |
Classification: all 26 modifier wrappers are **REQUIRED** (each is a user-facing Legacy GP modifier); their panels are PLATFORM REPLACE.

Shared modifier infrastructure (all verified): `DNA_struct_default_get`, `is_stroke_affected_by_modifier` (layer/material/pass filters via `DEG_get_original_id`), `get_modifier_point_weight`, `generic_bake_deform_stroke`, `updateDepsgraph` relations, panels (PLATFORM REPLACE, keep every RNA property).

## Classification summary
- **PLATFORM REPLACE:** GHOST → Android window/EGL; WM events/operators → MotionEvent + Grease tool system; desktop UI/RNA panels → Project Grease UI (keep all properties and operator behavior); desktop GL backend → GLES 3.2; TBB → BLI_task backend.
- **FEATURE OPTIONAL:** potrace (Trace Image); SVG/PDF I/O (nanosvg, pugixml, libharu + TIFF); mesh→GP and curve→GP convert; PIL timing (debug).
- **NOT REQUIRED:** Cycles, EEVEE, sequencer, Alembic/USD/Collada/OBJ/STL/PLY/AVI I/O. Subsurf/CCG in shrinkwrap.cc is unreachable from the GP path (build-time only). `subsurf_levels` in GP Shrinkwrap is never read.

## Still UNVERIFIED
1. ~~13 modifier wrappers~~ — VERIFIED (E29–E41).
2. ~~Caller of `makeDerivedMesh`~~ — VERIFIED (E42). Remaining: `deg_builder_nodes.cc` scheduling details, `BKE_gpencil_prepare_eval_data` / `BKE_gpencil_update_layer_transforms` bodies.
3. RNA soft/hard ranges for the 13 modifiers (e.g. Dash `dash` min — `stroke_dash` loops forever on `size == 0`), and bit values of `GP_MIRROR_INVERT_*` vs Multiply flags.
4. Depth of mesh modifier stack / Geometry Nodes, `BKE_mesh_new_from_object`, `duplilist`, `RE_GetSceneRender` / render_types.
5. GP engine: `gpencil_cache_utils.c`, `gpencil_draw_data.c`, `gpencil_shader.c`, `gpencil_shader_fx.c`, `gpencil_antialiasing.c`, `gpencil_render.c`, GLSL shaders; overlay engine GP parts.
6. `editors/gpencil_legacy` (30 files) + `editors/transform` GP converters.
7. `gpencil_undo.cc`; zlib use in .blend I/O.

## Pending architecture decision
Full behavior for Shrinkwrap/Line Art targets that have mesh modifiers requires porting Blender mesh evaluation (incl. mesh modifiers + Geometry Nodes). The alternative — non-parity for those targets — conflicts with the full-behavior requirement.
