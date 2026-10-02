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
    PROJECT_SETTINGS, IMPORT_SVG
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
        FeatureId.FREEHAND to wired("Source-derived brush engine (arcs, smoothing, angle, jitter) is unit-tested; $DEVICE"),
        FeatureId.PRESSURE to wired("Pen pressure only; finger input uses 1.0 like a Blender mouse. $DEVICE"),
        FeatureId.ERASER to wired("Hard/soft/stroke eraser use Blender's per-stroke algorithm; square-artifact report not reproduced. $DEVICE"),
        FeatureId.FILL to wired("Fills from a rasterized stroke mask (Blender boundary fill + outline); leak/dilate options not exposed. $DEVICE"),
        FeatureId.LINE to wired("Blender gpencil_primitive.c geometry; committed on release (no handle-edit phase). $DEVICE"),
        FeatureId.RECTANGLE to wired("Blender box geometry; committed on release (no handle-edit phase). $DEVICE"),
        FeatureId.CIRCLE to wired("Blender circle geometry; committed on release (no handle-edit phase). $DEVICE"),
        FeatureId.ARC to wired("Blender arc geometry; committed on release, bulge direction fixed. $DEVICE"),
        FeatureId.POLYLINE to wired("Touch polyline (tap adds a vertex, tap the last vertex to finish). $DEVICE"),
        FeatureId.CURVE to wired("Blender curve geometry (gpencil_primitive.c GP_STROKE_CURVE): drag start->end, drag the end/control handles, tap away or Confirm to commit. No extra Blender keys (extrude, edges, flip). $DEVICE"),
        FeatureId.STABILIZATION to wired("Lazy-mouse semantics ported; radius is in canvas units, not screen pixels. $DEVICE"),
        FeatureId.SMOOTHING to wired("Active smoothing ported but off by default (Blender's preset value not traced). $DEVICE"),
        FeatureId.SPACING to wired("Maps to Blender's Euclidean input filter, not a true spacing control. $DEVICE"),
        FeatureId.PRESSURE_CURVE to wired("Power curve approximation of Blender's CurveMapping. $DEVICE"),

        // ---- selection -----------------------------------------------------------------
        FeatureId.SELECT to wired("Blender click-select (nearest point, whole stroke by default). $DEVICE"),
        FeatureId.LASSO to wired("Blender lasso with eSelectOp semantics; selection highlight rendering not inspected. $DEVICE"),
        FeatureId.SELECT_POINT to wired("pickEntireStrokes=false selects single points; no UI toggle. $DEVICE"),
        FeatureId.SELECT_ALL to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_LINKED to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_ALTERNATE to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_MORE to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_LESS to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_LAST to wired("Native + controller API; no UI control. $DEVICE"),
        FeatureId.SELECT_RANDOM to missing("Needs BLI_rng/BLI_array_randomize in the source closure."),
        FeatureId.SELECT_BOX to wired("Native + controller API; no box-select tool. $DEVICE"),
        FeatureId.SELECT_CIRCLE to wired("Native + controller API; no circle-select tool. $DEVICE"),
        FeatureId.SELECT_SEGMENT to missing("ED_gpencil_select_stroke_segment is not ported."),
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
        FeatureId.KEYFRAME to wired("A frame is a keyframe once created. $DEVICE"),
        FeatureId.INTERPOLATION to wired("In-between frames for matching stroke and point counts with Blender's easing (Linear, Quad..Bounce; In/Out/In-Out) applied to the factor before mixing; Back uses the default overshoot, Elastic is not ported, and Blender's stroke pairing for unequal counts is not. $DEVICE"),
        FeatureId.ONION_SKIN to wired("Sets Legacy GP onion flags; ghost rendering in the Android presentation not inspected. $DEVICE"),
        FeatureId.ONION_RANGE to wired("Sets layer gstep/gstep_next; ghost rendering not inspected. $DEVICE"),
        FeatureId.ONION_OPACITY to wired("Sets gpd->onion_factor; ghost rendering not inspected. $DEVICE"),
        FeatureId.ONION_FADE to missing("Default flag only; no control."),
        FeatureId.ONION_LAYER_FILTER to missing("No per-layer onion control."),
        FeatureId.MULTIFRAME to wired("Edits honour GP_FRAME_SELECT in native selection/transform; no frame-selection UI. $DEVICE"),

        // ---- paint, materials, sculpt --------------------------------------------------
        FeatureId.MATERIALS to wired("Materials live in gpd->mat[]; UI can only add the next slot. $DEVICE"),
        FeatureId.CREATE_MATERIAL to wired("Created implicitly by 'Next brush/material'. $DEVICE"),
        FeatureId.DELETE_MATERIAL to missing("No operation."),
        FeatureId.SELECT_MATERIAL to wired("$DEVICE"),
        FeatureId.STROKE_COLOR to wired("Sets stroke and fill together; eyedropper reads the rendered mask. $DEVICE"),
        FeatureId.FILL_COLOR to missing("No separate fill color control."),
        FeatureId.THICKNESS to wired("$DEVICE"),
        FeatureId.OPACITY to wired("Pushed to the material from the Properties panel (and now the Materials sheet). $DEVICE"),
        FeatureId.FILL_ENABLE to wired("Toggles GP_MATERIAL_FILL_SHOW. $DEVICE"),
        FeatureId.SCULPT to wired("Eight source-derived brushes; $DEVICE"),
        FeatureId.SCULPT_GRAB to wired("$DEVICE"),
        FeatureId.SCULPT_SMOOTH to wired("$DEVICE"),
        FeatureId.SCULPT_THICKNESS to wired("$DEVICE"),
        FeatureId.SCULPT_STRENGTH to wired("$DEVICE"),
        FeatureId.SCULPT_PUSH to wired("$DEVICE"),
        FeatureId.SCULPT_PINCH to wired("$DEVICE"),
        FeatureId.SCULPT_TWIST to wired("$DEVICE"),
        FeatureId.SCULPT_RANDOMIZE to wired("$DEVICE"),
        FeatureId.VERTEX_PAINT to wired("Draw/Blur/Average/Smear/Replace brushes on the editable strokes with Blender's smooth falloff, target Stroke/Fill/Both, paint color from the Materials color; pen pressure scales strength. Not a full port of gpencil_vertex_paint.c: no selection mask, brush curves, spacing or jitter. One undo step per drag. $DEVICE"),
        FeatureId.WEIGHT_PAINT to wired("Draw brush (weight moves toward the chosen value by Blender's smooth falloff x strength x pressure) on the editable strokes; vertex groups (add/rename/remove, active group) stored in bGPdata::vertex_group_names; the canvas shows the active group's weights blue 0 .. red 1 while in the mode; weights and group names are saved. Weights only matter where something reads them: the live stack's Thickness entry and the selection Thickness-by-weight read vertex groups, every other modifier ignores them. Blur/Average/Smear/Sample weight brushes are not ported. $DEVICE"),
        FeatureId.ADVANCED_FILL to missing("Blender fill brush options not exposed."),
        FeatureId.FILL_GAP_TOLERANCE to missing("Native fill accepts a leak size; no control."),
        FeatureId.FILL_EXPANSION to missing("Native fill accepts a dilate size; no control."),
        FeatureId.FILL_BOUNDARY to missing("No boundary-mode control."),
        FeatureId.STROKE_TEXTURES to missing("No texture pipeline in the focused GLES backend."),
        FeatureId.FILL_TEXTURES to missing("No texture pipeline in the focused GLES backend."),
        FeatureId.TEXTURE_SETTINGS to missing("No texture pipeline."),
        FeatureId.TEXTURE_SCALE to missing("No texture pipeline."),
        FeatureId.TEXTURE_OPACITY to missing("No texture pipeline."),

        // ---- view ----------------------------------------------------------------------
        FeatureId.PAN to wired("$DEVICE"),
        FeatureId.ZOOM to wired("Pinch zoom. $DEVICE"),
        FeatureId.FIT_CANVAS to missing("Only reset view exists."),
        FeatureId.RESET_VIEW to wired("$DEVICE"),
        FeatureId.GRID to wired("Compose overlay in screen pixels, not tied to canvas units. $DEVICE"),
        FeatureId.GUIDES to wired("Overlay lines only; no guide snapping. $DEVICE"),
        FeatureId.SNAPPING to wired("Snaps input points to the grid size in canvas units. $DEVICE"),

        // ---- modifiers and effects -----------------------------------------------------
        FeatureId.MODIFIERS to wired("Live per-layer stack (Advanced sheet) of Thickness, Opacity, Tint, Hue/Saturation, Length, Smooth, Simplify, Subdivide, Offset and Noise; Apply bakes one into the strokes. No vertex-group/layer/material/pass filters or custom curves; Build, Dash, Outline, Mirror, Array and the other modifiers are not ported. Onion-skin ghosts and fill-tool hit tests use the unmodified strokes. $DEVICE"),
        FeatureId.MODIFIER_ORDERING to wired("The stack order is the evaluation order (Up/Down in the Advanced sheet); order, enable/disable and apply-equals-live are host-tested. $DEVICE"),
        FeatureId.NOISE to wired("Noise deformStroke() ported from MOD_gpencil_legacy_noise.c with Blender's BLI_hash seeds, evaluated with the current frame; mapped to canvas units (see project_grease_modifier_stack.h); no vertex group or curve. $DEVICE"),
        FeatureId.DASH to missing("Modifier source not traced yet."),
        FeatureId.OUTLINE to missing("Modifier source not traced yet."),
        FeatureId.THICKNESS_MODIFIER to wired("Thickness (MOD_gpencil_legacy_thick.c) and Opacity modifiers as live stack entries; no vertex groups/curve. $DEVICE"),
        FeatureId.COLOR_MODIFIER to wired("Tint and Hue/Saturation modifiers as live stack entries (no vertex groups/curve). $DEVICE"),
        FeatureId.DEFORM to missing("Modifier source not traced yet."),
        FeatureId.GENERATE to missing("Generators are outside the focused closure."),
        FeatureId.STROKE_EFFECTS to missing("Effects act on the whole rendered layer (see Visual effects); there is no stroke-only target."),
        FeatureId.FILL_EFFECTS to missing("Effects act on the whole rendered layer (see Visual effects); there is no fill-only target."),
        FeatureId.VISUAL_EFFECTS to wired("Per-layer effect list in the Layers sheet (Colorize, Pixelate, Flip, Wave Distortion, Swirl, Shadow, Rim, Blur, Glow), saved with the document. Blender's shader-FX pass construction and fragment math are ported to a 2D post-pass (the layer is drawn into an offscreen buffer, the effects ping-pong over it in list order, the result is composited; layers without effects keep the direct path). Adapted: sizes are canvas pixels and scale with zoom; the object origin is the canvas center; the Swirl center is a parameter (fraction of the canvas) instead of another object; Blur has no depth-of-field mode; Shadow has no object pivot; buffers are 8-bit, so Subtract/Divide blends and glow under-composites clamp at 0 and 1. GLSL is pixel-compared with a CPU port of Blender's shader math on a software GLES2 context, not on a device. $DEVICE"),
        FeatureId.LAYER_MASKS to wired("Per-layer mask list with use-mask, invert and hide; the GLES presentation draws a masked layer through an offscreen opacity mask (union of mask layers, Blender's revealage/invert sequence). Pixel-tested on a software GLES2 context, not on a device; onion-skin ghosts of a masked layer are masked too; the fill tool and export ignore masks. $DEVICE"),
        FeatureId.ADVANCED_INTERPOLATION to missing("Blender's sequence interpolation is not ported."),
        FeatureId.ADVANCED_ONION_SKIN to missing("Mode/keyframe-type options not exposed."),
        FeatureId.LINE_ART to blocked("Needs a minimal scene/object/depsgraph closure that is not established."),

        // ---- persistence and export ----------------------------------------------------
        FeatureId.NEW_PROJECT to wired("New Project offers the 2D Animation / Blank / Storyboard templates (GreaseTemplates): layers bottom to top, material slots with stroke color and fill on/off, fps and scene end frame (saved as frameEnd). Material names are not stored (no native name API). $DEVICE"),
        FeatureId.OPEN_PROJECT to wired("Restores points (incl. vertex color), stroke material/thickness/cyclic/fill (incl. fill vertex color), layer name/visibility/lock/opacity and the material palette (round-trip tested); older files load with the old defaults and no vertex color. $DEVICE"),
        FeatureId.SAVE to wired("Saves everything OPEN_PROJECT restores, including each layer's modifier stack (format version 4). $DEVICE"),
        FeatureId.SAVE_AS to missing("The menu item just saves."),
        FeatureId.EXPORT to wired("Project > Export writes SVG (current frame, or a folder with one file per frame) and PDF (one page per frame) through the Storage Access Framework; see VectorExport for what is drawn (saved strokes without modifiers, masks or effects; one color and width per stroke). Raster export is not implemented. $DEVICE"),
        FeatureId.EXPORT_PNG to missing("Only SVG and PDF are exported; no raster pipeline."),
        FeatureId.EXPORT_GIF to missing("No export pipeline."),
        FeatureId.EXPORT_ANIMATION to missing("No export pipeline."),
        FeatureId.PROJECT_SETTINGS to missing("Only app settings exist; no per-project settings."),
        FeatureId.IMPORT_SVG to wired("Project > Import SVG (Storage Access Framework): path/polyline/polygon/line/rect/circle/ellipse become strokes on the active layer/frame, curves flattened (SvgImport); viewBox fitted into the canvas keeping aspect; one material slot per distinct stroke/fill color pair (existing slots reused); thickness = stroke-width, cyclic = closed, fill on when the shape has a fill. No transforms, arcs (straight to end point), gradients or CSS. $DEVICE")
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
