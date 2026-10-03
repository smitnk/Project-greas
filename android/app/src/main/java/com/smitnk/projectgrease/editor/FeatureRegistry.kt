package com.smitnk.projectgrease.editor

/**
 * Gate state used by the UI (tools whose state is NOT_IMPLEMENTED are disabled/hidden).
 *
 * AVAILABLE is reserved for features that were validated on a device. IN_PROGRESS means the
 * feature is wired (or partly wired) but NOT device-verified or has documented gaps.
 */
enum class FeatureState { AVAILABLE, IN_PROGRESS, NOT_IMPLEMENTED }

/** Status used in engineering audits. COMPLETE additionally requires device validation. */
enum class AuditStatus { COMPLETE, IN_PROGRESS, NOT_IMPLEMENTED, BLOCKED }

enum class FeatureId {
    FREEHAND, PRESSURE, ERASER, FILL, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE, CURVE,
    SELECT, LASSO, SELECT_POINT, SELECT_ALL, SELECT_LINKED, SELECT_ALTERNATE, SELECT_MORE, SELECT_LESS,
    SELECT_LAST, SELECT_RANDOM, SELECT_BOX, SELECT_CIRCLE, SELECT_SEGMENT,
    MOVE, ROTATE, SCALE, MIRROR, TRANSFORM_SELECTION, DUPLICATE, DELETE, DELETE_POINTS, SPLIT, SUBDIVIDE,
    TRIM, CLOSE, JOIN_STROKES, SELECT_FIRST, SELECT_GROUPED,
    LAYERS, LAYER_VISIBILITY, LAYER_LOCKING, LAYER_ORDERING, LAYER_DUPLICATION, LAYER_DELETION, LAYER_RENAME,
    FRAMES, TIMELINE, ADD_FRAME, INSERT_FRAME, DUPLICATE_FRAME, DELETE_FRAME, FRAME_HOLDS,
    PLAYBACK, PAUSE, LOOP, FPS, FRAME_NAVIGATION, KEYFRAME, INTERPOLATION,
    ONION_SKIN, ONION_RANGE, ONION_OPACITY, ONION_FADE, ONION_LAYER_FILTER,
    MATERIALS, CREATE_MATERIAL, DELETE_MATERIAL, SELECT_MATERIAL, STROKE_COLOR, FILL_COLOR,
    THICKNESS, OPACITY, FILL_ENABLE, PAN, ZOOM, FIT_CANVAS, RESET_VIEW, GRID, GUIDES, SNAPPING,
    STABILIZATION, SMOOTHING, SPACING, PRESSURE_CURVE,
    SCULPT, SCULPT_GRAB, SCULPT_SMOOTH, SCULPT_THICKNESS, SCULPT_STRENGTH, SCULPT_PUSH, SCULPT_PINCH,
    SCULPT_TWIST, SCULPT_RANDOMIZE, VERTEX_PAINT, WEIGHT_PAINT,
    ADVANCED_FILL, FILL_GAP_TOLERANCE, FILL_EXPANSION, FILL_BOUNDARY,
    STROKE_TEXTURES, FILL_TEXTURES, TEXTURE_SETTINGS, TEXTURE_SCALE, TEXTURE_OPACITY,
    STROKE_EFFECTS, FILL_EFFECTS, MODIFIERS, MODIFIER_ORDERING,
    NOISE, DASH, OUTLINE, THICKNESS_MODIFIER, COLOR_MODIFIER, DEFORM, GENERATE,
    ADVANCED_INTERPOLATION, LAYER_MASKS, MULTIFRAME, ADVANCED_ONION_SKIN, VISUAL_EFFECTS, LINE_ART,
    NEW_PROJECT, OPEN_PROJECT, SAVE, SAVE_AS, EXPORT, EXPORT_PNG, EXPORT_GIF, EXPORT_ANIMATION,
    PROJECT_SETTINGS, IMPORT_SVG, TRACE_IMAGE, ANNOTATIONS,
    MERGE_BY_DISTANCE, STROKE_CAPS, START_POINT, SEPARATE_TO_LAYER, MOVE_TO_LAYER, COPY_PASTE,
    VERTEX_GROUP_OPS, LAYER_MERGE_DOWN, LAYER_ISOLATE, LAYER_LOCK_ALL,
    MOD_BUILD, MOD_TIME_OFFSET, MOD_HOOK, MOD_LATTICE, MOD_ENVELOPE, MOD_WEIGHT_PROXIMITY, MOD_WEIGHT_ANGLE,
    LIVE_GENERATORS, MODIFIER_INFLUENCE, DRAWING_GUIDES, ONION_KEYTYPE_LOOP, KEYFRAME_TYPES, LAYER_BLEND,
    LAYER_TINT, MATERIAL_SLOTS, LINE_TYPES, BRUSH_PRESETS, PRIMITIVE_EDIT, CURVE_MAPPING, ELASTIC_EASING,
    INTERPOLATE_SEQUENCE, EXPORT_MP4, SELECT_MENU
}

data class FeatureCapability(
    val id: FeatureId,
    val state: FeatureState,
    val label: String,
    val reason: String,
    val limitation: String = "",
    val audit: AuditStatus = AuditStatus.IN_PROGRESS,
    val deviceVerified: Boolean = false
)

object FeatureRegistry {
    private class Entry(
        val state: FeatureState,
        val audit: AuditStatus,
        val limitation: String,
        val deviceVerified: Boolean = false
    )

    /** Code path exists from Android input to the native Legacy GP data; not device-verified. */
    private fun wired(limitation: String) =
        Entry(FeatureState.IN_PROGRESS, AuditStatus.IN_PROGRESS, limitation)

    /** Nothing connects to the engine yet. */
    private fun missing(limitation: String) =
        Entry(FeatureState.NOT_IMPLEMENTED, AuditStatus.NOT_IMPLEMENTED, limitation)

    /** Cannot be done inside the locked architecture until a dependency closure is proven. */
    private fun blocked(limitation: String) =
        Entry(FeatureState.NOT_IMPLEMENTED, AuditStatus.BLOCKED, limitation)

    private const val DEVICE = "Device validation NOT VERIFIED."

    // Every FeatureId needs an entry here; FeatureRegistryTest enforces it, so a feature can no
    // longer be silently NOT_IMPLEMENTED (which would disable its tool) by being forgotten.
    private val entries: Map<FeatureId, Entry> = mapOf(
        // ---- drawing -------------------------------------------------------------------
        FeatureId.FREEHAND to wired("Native tool session: every touch sample (historical ones included) goes to native in one call per batch; the gpencil_paint.c input pipeline (filter, lazy mouse, addpoint, jitter, angle, active smoothing, arc/fake points, trailing truncation) runs natively (project_grease_draw_input.c) and matches the former Kotlin engine bit for bit on 22 golden scenarios. Pressure / strength go through the brush CurveMapping (see CURVE_MAPPING); the drawing guide constrains every sample first (DRAWING_GUIDES). $DEVICE"),
        FeatureId.PRESSURE to wired("Pen pressure only; finger input uses 1.0 like a Blender mouse. $DEVICE"),
        FeatureId.ERASER to wired("Hard/soft/stroke eraser use Blender's per-stroke algorithm; square-artifact report not reproduced. $DEVICE"),
        FeatureId.FILL to wired("Fills from a rasterized stroke mask (Blender boundary fill + outline); leak size, dilate and boundary mode are in the Fill bar. $DEVICE"),
        FeatureId.LINE to wired("Blender gpencil_primitive.c geometry; after the drag the shape stays editable (PRIMITIVE_EDIT). $DEVICE"),
        FeatureId.RECTANGLE to wired("Blender box geometry; editable after the drag (PRIMITIVE_EDIT). $DEVICE"),
        FeatureId.CIRCLE to wired("Blender circle geometry; editable after the drag (PRIMITIVE_EDIT). $DEVICE"),
        FeatureId.ARC to wired("Blender arc geometry, bulge direction fixed; editable after the drag (PRIMITIVE_EDIT). $DEVICE"),
        FeatureId.POLYLINE to wired("Touch polyline (tap adds a vertex, tap the last vertex to finish). $DEVICE"),
        FeatureId.CURVE to wired("Blender curve geometry (gpencil_primitive.c GP_STROKE_CURVE): drag start->end, drag the end/control handles, tap away or Confirm to commit. No extra Blender keys (extrude, edges, flip). $DEVICE"),
        FeatureId.STABILIZATION to wired("Lazy-mouse semantics ported; radius is in canvas units, not screen pixels. $DEVICE"),
        FeatureId.SMOOTHING to wired("Active smoothing ported but off by default (Blender's preset value not traced). $DEVICE"),
        FeatureId.SPACING to wired("Maps to Blender's Euclidean input filter, not a true spacing control. $DEVICE"),
        FeatureId.PRESSURE_CURVE to wired("The brush curve_sensitivity / curve_strength CurveMapping (CURVE_MAPPING) with a curve editor in Advanced > Brush; the former power curve is only used by callers that set no curve. $DEVICE"),

        // ---- selection -----------------------------------------------------------------
        FeatureId.SELECT to wired("Blender click-select (nearest point, whole stroke by default). $DEVICE"),
        FeatureId.LASSO to wired("Blender lasso with eSelectOp semantics on canvas coordinates (screen -> canvas through CanvasMapping, the presenter's own mapping); Edit mode draws the points of the editable strokes with selected points in Blender's vertex-select orange (GLES pixel test). $DEVICE"),
        FeatureId.SELECT_POINT to wired("pickEntireStrokes=false selects single points; no UI toggle. $DEVICE"),
        FeatureId.SELECT_ALL to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_LINKED to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_ALTERNATE to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_MORE to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_LESS to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_LAST to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_RANDOM to wired("Advanced > Select random 50%: edit3 pg_gp_select_random with the real pinned BLI_rng (same sequence as Blender for a seed); the seed advances per use. $DEVICE"),
        FeatureId.SELECT_BOX to wired("Box select tool (Edit rail): drag a rectangle, Blender's box select with the select mode and operation. $DEVICE"),
        FeatureId.SELECT_CIRCLE to wired("Circle select tool (Edit rail): every dab of the drag selects inside the brush radius, the first dab of a Set gesture replaces the selection. $DEVICE"),
        FeatureId.SELECT_SEGMENT to wired("Edit > Select mode Segment: a tap selects the run of points between the nearest crossings with other strokes (ED_gpencil_select_stroke_segment, verbatim, projected on the canvas plane instead of flat_ref's stroke frame); box/lasso/circle select treat Segment like Point. $DEVICE"),
        FeatureId.SELECT_FIRST to wired("Blender select_first; reachable only through controller API. $DEVICE"),
        FeatureId.SELECT_GROUPED to wired("Blender select_grouped (layer/material); controller API only. $DEVICE"),

        // ---- editing -------------------------------------------------------------------
        FeatureId.MOVE to wired("Selection-wide on selected points via native transform. $DEVICE"),
        FeatureId.ROTATE to wired("Selection-wide about the selection median. $DEVICE"),
        FeatureId.SCALE to wired("Selection-wide about the selection median. $DEVICE"),
        FeatureId.MIRROR to wired("Selection-wide about the selection median. $DEVICE"),
        FeatureId.TRANSFORM_SELECTION to wired("Rules from Blender transform conversion; not a copy of its code. $DEVICE"),
        FeatureId.DUPLICATE to wired("Duplicates the whole selected stroke; Blender duplicates selected points. $DEVICE"),
        FeatureId.DELETE to wired("Menu deletes one stroke by index; selection-wide delete has no UI button. $DEVICE"),
        FeatureId.DELETE_POINTS to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SPLIT to wired("BKE_gpencil_stroke_split at a fixed point from the menu. $DEVICE"),
        FeatureId.SUBDIVIDE to wired("BKE_gpencil_stroke_subdivide. $DEVICE"),
        FeatureId.TRIM to wired("BKE_gpencil_stroke_trim to first intersection. $DEVICE"),
        FeatureId.CLOSE to wired("BKE_gpencil_stroke_close. $DEVICE"),
        FeatureId.JOIN_STROKES to wired("Controller API; no UI control. $DEVICE"),

        // ---- layers and animation ------------------------------------------------------
        FeatureId.LAYERS to wired("Sheet name and switches are read from the native layer. $DEVICE"),
        FeatureId.LAYER_VISIBILITY to wired("Sheet switch is read from native state; saved and restored with the project. $DEVICE"),
        FeatureId.LAYER_LOCKING to wired("Sheet switch is read from native state; saved and restored with the project. $DEVICE"),
        FeatureId.LAYER_ORDERING to wired("BLI_listbase_move_index. $DEVICE"),
        FeatureId.LAYER_DUPLICATION to wired("BKE_gpencil_layer_duplicate. $DEVICE"),
        FeatureId.LAYER_DELETION to wired("BKE_gpencil_layer_delete. $DEVICE"),
        FeatureId.LAYER_RENAME to wired("Renames the native layer. $DEVICE"),
        FeatureId.FRAMES to wired("Real bGPDframe lifecycle. $DEVICE"),
        FeatureId.TIMELINE to wired("Frame strip with KEY/HOLD labels. $DEVICE"),
        FeatureId.ADD_FRAME to wired("Creates or selects the frame after the current one. $DEVICE"),
        FeatureId.INSERT_FRAME to wired("Timeline \"Insert blank keyframe\" (GPENCIL_OT_blank_frame_add on the active layer: frames at/after the current one move one later) and \"Clean duplicate frames\". $DEVICE"),
        FeatureId.DUPLICATE_FRAME to wired("BKE_gpencil_frame_duplicate. $DEVICE"),
        FeatureId.DELETE_FRAME to wired("BKE_gpencil_layer_frame_delete. $DEVICE"),
        FeatureId.FRAME_HOLDS to wired("Holds via GP_GETFRAME_USE_PREV. $DEVICE"),
        FeatureId.PLAYBACK to wired("Handler-driven playback; export ignores it. $DEVICE"),
        FeatureId.PAUSE to wired("$DEVICE"),
        FeatureId.LOOP to wired("$DEVICE"),
        FeatureId.FPS to wired("$DEVICE"),
        FeatureId.FRAME_NAVIGATION to wired("$DEVICE"),
        FeatureId.KEYFRAME to wired("A frame is a keyframe once created; its key type is set from the timeline (KEYFRAME_TYPES). $DEVICE"),
        FeatureId.INTERPOLATION to wired("In-between frames with Blender's easing (Linear, Quad..Bounce, Elastic; In/Out/In-Out) applied to the factor before mixing; strokes are paired by index and a pair with different point counts is resampled to the larger count (BKE_gpencil_stroke_uniform_subdivide, as gpencil_interpolate.c); strokes without a partner are skipped. Back uses the default overshoot. $DEVICE"),
        FeatureId.ONION_SKIN to wired("Overlay switch GP_DATA_SHOW_ONIONSKINS + per-layer GP_LAYER_ONIONSKIN; ghost keyframes by onion mode (Relative / Absolute / Selected) with Blender's signed onion_id, optional ghost colours gcolor_prev / gcolor_next. Host pixel tests only; $DEVICE"),
        FeatureId.ONION_RANGE to wired("bGPdata gstep / gstep_next: keyframes (Relative) or frames (Absolute) before / after. $DEVICE"),
        FeatureId.ONION_OPACITY to wired("gpd->onion_factor through Blender's ghost alpha formula. $DEVICE"),
        FeatureId.ONION_FADE to wired("GP_ONION_FADE switch in Onion Skin; ghost alpha 1/|keyframe distance| x factor clamped to 0.1..1 like gpencil_layer_final_tint_and_alpha_get. $DEVICE"),
        FeatureId.ONION_LAYER_FILTER to wired("Per-layer \"Use onion skinning\" switch in Layers (GP_LAYER_ONIONSKIN, on for new layers); not saved in the project file. $DEVICE"),
        FeatureId.MULTIFRAME to wired("Timeline long press > Select frame (GP_FRAME_SELECT, orange outline) and the Multiframe chip (GP_DATA_STROKE_MULTIEDIT): native edits act on every selected frame. No falloff curve. $DEVICE"),

        // ---- paint, materials, sculpt --------------------------------------------------
        FeatureId.MATERIALS to wired("Materials live in gpd->mat[]; Materials sheet: slot list with names, order, lock / hide / solo, line type and pass (MATERIAL_SLOTS). $DEVICE"),
        FeatureId.CREATE_MATERIAL to wired("Created implicitly by 'Next brush/material'. $DEVICE"),
        FeatureId.DELETE_MATERIAL to wired("Materials > Delete material (with confirmation): strokes using the slot are deleted (as specified for this app; Blender's slot removal keeps them on the next slot), higher slots shift down (BKE_gpencil_material_index_reassign), the last slot cannot be deleted; undoable. $DEVICE"),
        FeatureId.SELECT_MATERIAL to wired("$DEVICE"),
        FeatureId.STROKE_COLOR to wired("Sets stroke and fill together; eyedropper reads the rendered mask. $DEVICE"),
        FeatureId.FILL_COLOR to wired("Advanced > Use color as fill color: edit3 sets the active material's fill color (GP_MATERIAL fill_rgba); no separate fill picker. $DEVICE"),
        FeatureId.THICKNESS to wired("$DEVICE"),
        FeatureId.OPACITY to wired("Pushed to the material from the Properties panel (and now the Materials sheet). $DEVICE"),
        FeatureId.FILL_ENABLE to wired("Toggles GP_MATERIAL_FILL_SHOW. $DEVICE"),
        FeatureId.SCULPT to wired("Native tool session: gpencil_sculpt_paint.c brush callbacks and per-point hit test (do_stroke) carried verbatim and applied for every touch sample; falloff BKE_brush_curve_strength (smooth preset); Blender preset flags (position only; Grab without pressure); points re-tested under the brush at each sample. View is the canvas plane (pixels per canvas unit), no auto-masking, clone brush, lock-to-cursor or multiframe falloff curve. $DEVICE"),
        FeatureId.SCULPT_GRAB to wired("Points under the brush at the first sample keep their start weights and follow the drag (gpencil_brush_grab_*). $DEVICE"),
        FeatureId.SCULPT_SMOOTH to wired("BKE_gpencil_stroke_smooth_point with endpoints fixed and position only (thickness/opacity untouched). $DEVICE"),
        FeatureId.SCULPT_THICKNESS to wired("pressure += influence / 10 (invert: -), Blender's formula. $DEVICE"),
        FeatureId.SCULPT_STRENGTH to wired("strength += influence x 0.125 clamped 0..1 (invert: -), Blender's formula. $DEVICE"),
        FeatureId.SCULPT_PUSH to wired("Moves points by the drag delta x influence. $DEVICE"),
        FeatureId.SCULPT_PINCH to wired("Pulls toward the brush center by (influence / 5)^2 (invert inflates). $DEVICE"),
        FeatureId.SCULPT_TWIST to wired("Rotates around the brush center by influence degrees (invert reverses). $DEVICE"),
        FeatureId.SCULPT_RANDOMIZE to wired("Jitter perpendicular to the drag with BLI_rng (position only, Blender's preset). $DEVICE"),
        FeatureId.VERTEX_PAINT to wired("Native tool session: gpencil_vertex_paint.c Draw (tint), Blur, Average, Smear (grid) and Replace carried verbatim with the per-sample selection of points under the brush; target Stroke/Fill/Both; influence = size x pressure x falloff x strength / 100 like Blender. Colors stay sRGB (the editor stores sRGB; Blender converts the brush color to linear); no selection masking. $DEVICE"),
        FeatureId.WEIGHT_PAINT to wired("Native tool session: gpencil_weight_paint.c Draw, Blur, Average and Smear (kd-tree nearest points) carried verbatim with the per-sample selection; Subtract switch; vertex groups (add/rename/remove, active group) stored in bGPdata::vertex_group_names; the canvas shows the active group's weights blue 0 .. red 1; weights are saved. No auto-normalize (no armatures). Weights only matter where something reads them (live Thickness entry, Thickness-by-weight). Vertex-group operators (Assign/Remove/Select/Deselect/Invert/Normalize on the active group) are listed under VERTEX_GROUP_OPS. $DEVICE"),
        FeatureId.ADVANCED_FILL to wired("Fill \"Extend\" slider (brush fill_extend_fac, default 0): open strokes are prolonged at both ends in the fill boundary by that fraction of their length (pg_fill_extend_segments, 1 px lines). Extension lines are not shown on the canvas while filling. $DEVICE"),
        FeatureId.FILL_GAP_TOLERANCE to wired("Fill bar \"Leak\" = Blender fill_leak (px, default 3) for the Legacy boundary fill. $DEVICE"),
        FeatureId.FILL_EXPANSION to wired("Fill bar \"Dilate\" = Blender dilate (px, default 1; negative contracts). $DEVICE"),
        FeatureId.FILL_BOUNDARY to wired("Fill bar boundary All / Strokes / Edit Lines (fill_draw_mode): Edit Lines uses 1 px center lines in the fill mask; extend lines and help lines are not implemented, so All and Strokes give the same mask. $DEVICE"),
        FeatureId.STROKE_TEXTURES to wired("Material stroke style Texture with a picked image (SAF): U along the stroke length (texture_pixsize), V across the width; colour = texture x (1 - mix) + material colour x mix as gpencil_frag.glsl. The image is held by the presenter and its URI saved in the project; Blender's point uv_rot and dots/boxes alignment are not used. $DEVICE"),
        FeatureId.FILL_TEXTURES to wired("Material fill style Texture with a picked image (SAF): UV from the stroke's bounding square through texture_scale / texture_angle / texture_offset, mixed by mix_factor. Image URI saved in the project; Blender's per-stroke fill uv_translation / uv_rotation / uv_scale are not used. $DEVICE"),
        FeatureId.TEXTURE_SETTINGS to wired("Texture mix, pixel size (stroke), scale / offset / angle (fill) in Materials. $DEVICE"),
        FeatureId.TEXTURE_SCALE to wired("Fill texture_scale (x, y) and stroke texture_pixsize. $DEVICE"),
        FeatureId.TEXTURE_OPACITY to wired("Texture mix factor (mix_stroke_factor / mix_factor): 0 = texture only, 1 = material colour only. $DEVICE"),

        // ---- view ----------------------------------------------------------------------
        FeatureId.PAN to wired("$DEVICE"),
        FeatureId.ZOOM to wired("Pinch zoom. $DEVICE"),
        FeatureId.FIT_CANVAS to wired("Fit canvas button: zoom 1 / no pan, which the presenter maps to the whole canvas in view with a 4% margin per side. $DEVICE"),
        FeatureId.RESET_VIEW to wired("$DEVICE"),
        FeatureId.GRID to wired("Compose overlay in screen pixels, not tied to canvas units. $DEVICE"),
        FeatureId.GUIDES to wired("Old overlay lines (Settings > Guides) stay as a visual aid; the Blender drawing guides with snapping are DRAWING_GUIDES. $DEVICE"),
        FeatureId.SNAPPING to wired("Snaps input points to the grid size in canvas units. $DEVICE"),

        // ---- modifiers and effects -----------------------------------------------------
        FeatureId.MODIFIERS to wired("Live per-layer stack (Advanced sheet) of Thickness, Opacity, Tint, Hue/Saturation, Length, Smooth, Simplify, Subdivide, Offset, Noise, Build, Time Offset, Hook, Lattice, Envelope, Vertex Weight Proximity / Angle, Dot Dash, Outline, Mirror, Array and Multiple Strokes; Apply bakes one into the strokes. Every entry has the influence filters and a custom curve (MODIFIER_INFLUENCE). Onion-skin ghosts and fill-tool hit tests use the unmodified strokes. $DEVICE"),
        FeatureId.MODIFIER_ORDERING to wired("The stack order is the evaluation order (Up/Down in the Advanced sheet); order, enable/disable and apply-equals-live are host-tested. $DEVICE"),
        FeatureId.NOISE to wired("Noise deformStroke() ported from MOD_gpencil_legacy_noise.c with Blender's BLI_hash seeds, evaluated with the current frame; mapped to canvas units (see project_grease_modifier_stack.h); no vertex group or curve. $DEVICE"),
        FeatureId.DASH to wired("Advanced > Dash: baked once into the selected strokes (one dash/gap segment pattern in points, like the Dash modifier's first segment); no per-segment radius/opacity/material, not a live modifier. $DEVICE"),
        FeatureId.OUTLINE to wired("Outline modifier baked (Advanced > Outline selected strokes): each selected open stroke becomes the closed perimeter of its thick shape with round caps (pg_gp_outline). Not in the live stack; strokes with vertex weights are skipped. $DEVICE"),
        FeatureId.THICKNESS_MODIFIER to wired("Thickness (MOD_gpencil_legacy_thick.c) and Opacity modifiers as live stack entries; no vertex groups/curve. $DEVICE"),
        FeatureId.COLOR_MODIFIER to wired("Tint and Hue/Saturation modifiers as live stack entries (no vertex groups/curve). $DEVICE"),
        FeatureId.DEFORM to wired("Hook and Lattice as live 2D modifiers (MOD_HOOK, MOD_LATTICE); Armature, Shrinkwrap and object parenting are out of scope for the 2D app. $DEVICE"),
        FeatureId.GENERATE to wired("Live Dot Dash, Outline, Mirror, Array and Multiple Strokes (LIVE_GENERATORS), Build (MOD_BUILD) and Envelope (MOD_ENVELOPE); the Advanced buttons still bake once. $DEVICE"),
        FeatureId.MERGE_BY_DISTANCE to wired("Advanced > Merge by distance: BKE_gpencil_stroke_merge_distance on the selected strokes (threshold 2 canvas px, selected points only). $DEVICE"),
        FeatureId.STROKE_CAPS to wired("Advanced > Toggle caps: GPENCIL_OT_stroke_caps_set (round/flat per end); the stroke renderer draws flat caps when caps[] says FLAT. $DEVICE"),
        FeatureId.START_POINT to wired("Advanced > Set start point: rotates a cyclic stroke so the selected point comes first (GPENCIL_OT_stroke_start_set); open strokes are left alone. $DEVICE"),
        FeatureId.SEPARATE_TO_LAYER to wired("Advanced > Separate to new layer: selected strokes of the active layer move to a new \"Separated\" layer at the same frame numbers. $DEVICE"),
        FeatureId.MOVE_TO_LAYER to wired("Layers > Move selection here: selected strokes move to that layer (frame created at the same number when missing). $DEVICE"),
        FeatureId.COPY_PASTE to wired("Advanced > Copy / Paste strokes: process-wide clipboard (not saved, lost when the app closes); paste goes to the active layer's current frame and selects the pasted strokes. $DEVICE"),
        FeatureId.VERTEX_GROUP_OPS to wired("Weight Paint > Selection: Assign (current weight), Remove from group, Select, Deselect, Invert, Normalize on the active vertex group, over the selected points (object_vgroup.c semantics, no lock/mirror options). Emulator test (API 30 x86_64) in DeviceBugfixTest.assignWeightsSaved checks the weights in the saved project; the other operators are not device-verified."),
        FeatureId.LAYER_MERGE_DOWN to wired("Layers > Merge down: strokes move into the layer below per frame (BKE_gpencil_layer_merge semantics), the active layer is deleted with its modifier/effect stacks and the lower layer becomes active; disabled for the bottom layer. Emulator test (API 30 x86_64) in DeviceBugfixTest.mergeDownKeepsStrokes."),
        FeatureId.LAYER_ISOLATE to wired("Layers > Isolate: hides every layer except the active one (no toggle-back, no lock variant). Emulator test (API 30 x86_64) in DeviceBugfixTest.isolateHidesOtherLayers."),
        FeatureId.LAYER_LOCK_ALL to wired("Layers > Lock all / Unlock all. $DEVICE"),
        FeatureId.MOD_BUILD to wired("Live Build (MOD_gpencil_legacy_build.c): Sequential / Concurrent, Grow / Shrink, delay and length in frames from the keyframe (pg_build_visible); no Additive mode, time alignment or percentage factor. $DEVICE"),
        FeatureId.MOD_TIME_OFFSET to wired("Live Time Offset (MOD_gpencil_legacy_time.c): the layer shows the keyframe at pg_time_offset_frame (Normal / Reverse / Fixed / Ping-pong, offset, scale, custom range + loop); no segments mode. $DEVICE"),
        FeatureId.MOD_HOOK to wired("Live 2D Hook (pg_hook_deform): centre, offset, rotation, scale, radius, Constant / Smooth / Linear falloff, strength; centre and target dragged on the canvas (Edit handles). No hook object or custom falloff curve (the entry's custom curve acts along the stroke). $DEVICE"),
        FeatureId.MOD_LATTICE to wired("Live 2D Lattice (pg_lattice_deform): N x M grid (2..6) over a canvas rectangle, grid nodes dragged on the canvas, bilinear deform, strength. Blender's Lattice is a 3D object deform. $DEVICE"),
        FeatureId.MOD_ENVELOPE to wired("Live Envelope with the pinned MOD_gpencil_legacy_envelope.c functions (verbatim): Deform, Segments and Fills modes, spread, skip, thickness, strength, material. Deform maps pixels to pressure by the stroke thickness (canvas units are pixels). $DEVICE"),
        FeatureId.MOD_WEIGHT_PROXIMITY to wired("Live Vertex Weight Proximity: writes the target group's weights on the evaluated strokes from the distance to a canvas point (handle on the canvas), lowest / highest distance, minimum weight, invert, multiply. Later entries (Thickness with a group) see them. $DEVICE"),
        FeatureId.MOD_WEIGHT_ANGLE to wired("Live Vertex Weight Angle: weight 1 - sin(angle between segment and reference direction) as MOD_gpencil_legacy_weight_angle.c for a front view; minimum weight, invert, multiply. $DEVICE"),
        FeatureId.LIVE_GENERATORS to wired("Live Dot Dash (one segment), Outline, Mirror (pivot handle), Array (constant offset) and Multiple Strokes reuse the baked operators on the evaluated copy; no randomize / fading / object offset, multi-segment dash or outline material. $DEVICE"),
        FeatureId.MODIFIER_INFLUENCE to wired("Every modifier entry: material slot, material pass, layer pass and vertex group filters with invert, and a custom curve along the stroke (CurveMapping), applied by blending each point between before / after. Layer-name filter is not needed (stacks are per layer). $DEVICE"),
        FeatureId.DRAWING_GUIDES to wired("Draw tool guides (Advanced): Circular, Radial, Parallel, Grid, Isometric with centre, angle and spacing (GP_GUIDE_*); every input sample is snapped by pg_guide_snap before the draw pipeline; the guide overlay is drawn natively and never exported. Emulator test (API 30 x86_64): DeviceBugfixTest.guideSnapsDrawnStroke."),
        FeatureId.ONION_KEYTYPE_LOOP to wired("Onion keyframe-type filter (bGPdata.onion_keytype) and Loop (GP_ONION_LOOP, BKE_gpencil_visible_stroke_advanced_iter wrap) in the presenter; render-tested on a software GLES2 context. $DEVICE"),
        FeatureId.KEYFRAME_TYPES to wired("bGPDframe.key_type (Keyframe, Extreme, Breakdown, Jitter, Moving Hold) from the timeline long-press menu, shown as coloured markers, saved (\"keyType\"), used by the onion filter. Emulator test (API 30 x86_64): DeviceBugfixTest.keyframeTypeSavedAndMarked."),
        FeatureId.LAYER_BLEND to wired("Layer blend modes (Regular, Hard Light, Add, Subtract, Multiply, Divide) through gpencil_layer_blend_frag.glsl's blend_mode_output over the offscreen layer, saved; a layer with stroke-/fill-only effects is not blended. Emulator test (API 30 x86_64): DeviceBugfixTest.layerBlendAndTintPixels."),
        FeatureId.LAYER_TINT to wired("Layer tint colour + factor (gpLayerTint) and stroke thickness offset (line_change, gpThicknessOffset) applied while the layer is drawn, saved. Emulator test (API 30 x86_64): DeviceBugfixTest.layerBlendAndTintPixels."),
        FeatureId.MATERIAL_SLOTS to wired("Material names (Material id name), slot order (strokes remapped, textures follow), Lock / Hide / Solo (GP_MATERIAL_LOCKED / HIDE, material isolate) and pass index; saved. Names and flags are not undo steps (shared with history, as the colours). $DEVICE"),
        FeatureId.LINE_TYPES to wired("Line types Dots and Squares (GP_MATERIAL_MODE_DOT / SQUARE): one quad per point of the point thickness, aligned to the path, the canvas or the screen and turned by the alignment rotation; dots are round. No texture or hardness on dots. $DEVICE"),
        FeatureId.BRUSH_PRESETS to wired("Brush chips: Pencil, Pencil Soft, Ink Pen, Ink Pen Rough, Marker Bold, Marker Chisel, Pen, Airbrush, Fill Area and the four erasers with BKE_gpencil_brush_preset_set values (size, strength, pressure switches, input samples, active smoothing, angle, curves). Hardness, random pressure and the preset materials are not applied. Emulator test (API 30 x86_64): DeviceBugfixTest.brushPresetValues."),
        FeatureId.PRIMITIVE_EDIT to wired("Line, Box, Circle, Arc stay editable after the drag (gpencil_primitive.c): drag the start / end handles, Subdiv - / +, Extrude (line becomes a polyline), Confirm / Cancel or tap away. Arc has no separate control handle. Emulator test (API 30 x86_64): DeviceBugfixTest.primitiveHandleEdit."),
        FeatureId.CURVE_MAPPING to wired("colortools.c CurveMapping (auto / vector handles, bezier table, clipped evaluation) ported in project_grease_curvemap.c, used by the brush pressure / strength curves and the modifier custom curves; values match an independent reference of the algorithm. $DEVICE"),
        FeatureId.ELASTIC_EASING to wired("BLI_easing_elastic_* verbatim (easing.c) with amplitude / period; Settings > Elastic. $DEVICE"),
        FeatureId.INTERPOLATE_SEQUENCE to wired("GPENCIL_OT_interpolate_sequence: every frame between the keyframes around the current frame gets an in-between in one undo step (timeline and long-press menu). Step 1 only, active layer only. $DEVICE"),
        FeatureId.EXPORT_MP4 to wired("MP4 / H.264 via MediaCodec + MediaMuxer from the offscreen renderer: project frame range at the project fps with holds, YUV 4:2:0, odd sizes cropped by one pixel. Emulator test (API 30 x86_64): DeviceBugfixTest.mp4ExportPlayable."),
        FeatureId.SELECT_MENU to wired("Edit bar: Select all / none / invert / linked / alternate / more / less / first / last / same layer / same material, Delete points, Join and the Pick points switch. $DEVICE"),
        FeatureId.STROKE_EFFECTS to wired("Each layer effect has a target: whole layer, strokes only or fills only; with a stroke/fill target the layer is drawn as a fills pass and a strokes pass and the effect runs on its pass (fills are then below all strokes of the layer). Not a Blender option. $DEVICE"),
        FeatureId.FILL_EFFECTS to wired("Effect target \"Fills\": see Stroke effects. $DEVICE"),
        FeatureId.VISUAL_EFFECTS to wired("Per-layer effect list in the Layers sheet (Colorize, Pixelate, Flip, Wave Distortion, Swirl, Shadow, Rim, Blur, Glow), saved with the document. Blender's shader-FX pass construction and fragment math are ported to a 2D post-pass (the layer is drawn into an offscreen buffer, the effects ping-pong over it in list order, the result is composited; layers without effects keep the direct path). Adapted: sizes are canvas pixels and scale with zoom; the object origin is the canvas center; the Swirl center is a parameter (fraction of the canvas) instead of another object; Blur has no depth-of-field mode; Shadow has no object pivot; buffers are 8-bit, so Subtract/Divide blends and glow under-composites clamp at 0 and 1. GLSL is pixel-compared with a CPU port of Blender's shader math on a software GLES2 context, not on a device. $DEVICE"),
        FeatureId.LAYER_MASKS to wired("Per-layer mask list with use-mask, invert and hide; the GLES presentation draws a masked layer through an offscreen opacity mask (union of mask layers, Blender's revealage/invert sequence). Pixel-tested on a software GLES2 context, not on a device; onion-skin ghosts of a masked layer are masked too; the fill tool and export ignore masks. $DEVICE"),
        FeatureId.ADVANCED_INTERPOLATION to wired("Unequal strokes are interpolated (resampled to matching point counts) and paired by index; sequence interpolation is INTERPOLATE_SEQUENCE. $DEVICE"),
        FeatureId.ADVANCED_ONION_SKIN to wired("Onion mode Relative / Absolute / Selected (bGPdata.onion_mode), custom ghost colours (GP_ONION_GHOST_PREVCOL / NEXTCOL), keyframe-type filter and loop (ONION_KEYTYPE_LOOP). $DEVICE"),
        FeatureId.LINE_ART to wired("BATCH 3 OF 3: Blender 3.6.23 Line Art on Scene-lite generates strokes. The pinned lineart_cpu.cc is carried as verified verbatim regions (feature lines: contour, crease, material, edge marks, loose, intersection; near/far clipping; bounding areas; occlusion levels) with only the Mesh/Object/Depsgraph loaders and GP output replaced by Scene-lite versions; lineart_chain.c (chaining, occlusion splits, connecting, smoothing, angle split, depth offset) is compiled as pinned; single-threaded. CI compares visible lines and strokes with desktop Blender 3.6.23 on reference scenes. More > 3D reference: import OBJ, set the camera, preview, then Generate Line Art strokes onto a new layer of the current frame (image-space points mapped to the canvas). Not supported: shadows / light contour, materials' Line Art settings, collections, face marks, instancing, per-frame re-bake of an animated scene (only the camera-orbit bake); the reference scene is not saved with the project. $DEVICE"),

        // ---- persistence and export ----------------------------------------------------
        FeatureId.NEW_PROJECT to wired("New Project offers the 2D Animation / Blank / Storyboard templates (GreaseTemplates): layers bottom to top, material slots with stroke color and fill on/off, fps and scene end frame (saved as frameEnd). Material names are not stored (no native name API). $DEVICE"),
        FeatureId.OPEN_PROJECT to wired("Restores points (incl. vertex color), stroke material/thickness/cyclic/fill (incl. fill vertex color), layer name/visibility/lock/opacity and the material palette (round-trip tested); older files load with the old defaults and no vertex color. $DEVICE"),
        FeatureId.SAVE to wired("Saves everything OPEN_PROJECT restores, including each layer's modifier stack (format version 4). $DEVICE"),
        FeatureId.SAVE_AS to wired("Project > Save as: Storage Access Framework create-document writes the project JSON to the chosen file and continues under that name (also stored in the app's project list). $DEVICE"),
        FeatureId.EXPORT to wired("Project > Export writes SVG (current frame, or a folder with one file per frame) and PDF (one page per frame) through the Storage Access Framework; see VectorExport for what is drawn (saved strokes without modifiers, masks or effects; one color and width per stroke). Raster export is not implemented. $DEVICE"),
        FeatureId.EXPORT_PNG to wired("Project > Export > PNG: the current frame rendered offscreen at canvas size by the same presenter (modifiers, masks, effects; no annotations), optional transparent background, saved through the Storage Access Framework. $DEVICE"),
        FeatureId.EXPORT_GIF to wired("Project > Export GIF: frames start..end of the project settings rendered offscreen like PNG export (holds show the previous keyframe), animated GIF89a with a fixed 252-colour palette + transparency at the project fps. $DEVICE"),
        FeatureId.EXPORT_ANIMATION to wired("Project > Export: GIF, PNG sequence (frames start..end with holds) and MP4 video (EXPORT_MP4). $DEVICE"),
        FeatureId.PROJECT_SETTINGS to wired("Project > Settings: canvas size, fps, start/end frame, background colour, transparent background; saved in the project file (\"settings\", older files use defaults); the canvas, timeline end and exports read it. $DEVICE"),
        FeatureId.IMPORT_SVG to wired("Project > Import SVG (Storage Access Framework): path/polyline/polygon/line/rect/circle/ellipse become strokes on the active layer/frame, curves flattened (SvgImport); viewBox fitted into the canvas keeping aspect; one material slot per distinct stroke/fill color pair (existing slots reused); thickness = stroke-width, cyclic = closed, fill on when the shape has a fill. No transforms, arcs (straight to end point), gradients or CSS. $DEVICE"),
        FeatureId.TRACE_IMAGE to wired("Project > Trace image: threshold (0.5), trace bright areas, tolerance (0.6 px); each outline becomes a closed filled stroke on a new \"Trace\" layer scaled to the canvas, in the active color. DIFFERENCE FROM BLENDER: Blender's Trace Image uses potrace (curve fitting, turd size, corner threshold); Project Grease uses pixel-edge outline tracing + Douglas-Peucker (ImageTrace), so outlines are polygonal. Images are subsampled to <= 1024 px. $DEVICE"),
        FeatureId.ANNOTATIONS to wired("Annotate tool (NOTES group): freehand notes in a separate annotation bGPdata (layer \"Note\", own frames that hold like keyframes, Blender annotate_paint.c semantics), drawn over all layers with a fixed screen-space thickness in the layer color; \"Erase notes\" removes annotation points and splits strokes, never the drawing; color, thickness, show/hide, Clear annotations. Saved in the project file (key annotations); left out of SVG/PDF export unless \"Include annotations\" is on. Annotation edits are not undo steps. No raster export exists. $DEVICE")
    )

    /** Ids that have an explicit entry (the test requires this to equal every FeatureId). */
    fun explicitIds(): Set<FeatureId> = entries.keys

    fun capability(id: FeatureId): FeatureCapability {
        val entry = entries[id]
            ?: return FeatureCapability(
                id, FeatureState.NOT_IMPLEMENTED, label(id),
                "No audit entry for this feature.",
                "No audit entry.", AuditStatus.NOT_IMPLEMENTED, false
            )
        val reason = when (entry.state) {
            FeatureState.AVAILABLE -> "Verified on device."
            FeatureState.IN_PROGRESS -> "In progress: " + entry.limitation
            FeatureState.NOT_IMPLEMENTED -> "Not implemented: " + entry.limitation
        }
        return FeatureCapability(
            id, entry.state, label(id), reason, entry.limitation, entry.audit, entry.deviceVerified
        )
    }

    private fun label(id: FeatureId): String =
        id.name.replace('_', ' ').lowercase().replaceFirstChar { it.uppercase() }

    fun all(): List<FeatureCapability> = FeatureId.entries.map(::capability)
}
