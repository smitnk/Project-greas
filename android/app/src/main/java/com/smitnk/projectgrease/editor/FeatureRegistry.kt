
package com.smitnk.projectgrease.editor

enum class FeatureState { AVAILABLE, IN_PROGRESS, NOT_IMPLEMENTED }

enum class FeatureId {
    FREEHAND, PRESSURE, ERASER, FILL, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE,
    SELECT, LASSO, MOVE, ROTATE, SCALE, MIRROR, DUPLICATE, DELETE, SPLIT, SUBDIVIDE, TRIM, CLOSE,
    JOIN_STROKES, SELECT_FIRST, SELECT_GROUPED,
    LAYERS, LAYER_VISIBILITY, LAYER_LOCKING, LAYER_ORDERING, LAYER_DUPLICATION, LAYER_DELETION, LAYER_RENAME,
    FRAMES, TIMELINE, ADD_FRAME, INSERT_FRAME, DUPLICATE_FRAME, DELETE_FRAME, FRAME_HOLDS,
    PLAYBACK, PAUSE, LOOP, FPS, FRAME_NAVIGATION, KEYFRAME, INTERPOLATION,
    ONION_SKIN, ONION_RANGE, ONION_OPACITY, ONION_FADE, ONION_LAYER_FILTER,
    MATERIALS, CREATE_MATERIAL, DELETE_MATERIAL, SELECT_MATERIAL, STROKE_COLOR, FILL_COLOR,
    THICKNESS, OPACITY, FILL_ENABLE, PAN, ZOOM, FIT_CANVAS, RESET_VIEW, GRID, GUIDES, SNAPPING,
    STABILIZATION, SMOOTHING, SPACING, PRESSURE_CURVE,
    SCULPT, SCULPT_GRAB, SCULPT_SMOOTH, SCULPT_THICKNESS, SCULPT_STRENGTH, SCULPT_PUSH, SCULPT_PINCH, SCULPT_RANDOMIZE,
    ADVANCED_FILL, FILL_GAP_TOLERANCE, FILL_EXPANSION, FILL_BOUNDARY,
    STROKE_TEXTURES, FILL_TEXTURES, TEXTURE_SETTINGS, TEXTURE_SCALE, TEXTURE_OPACITY,
    STROKE_EFFECTS, FILL_EFFECTS, MODIFIERS, MODIFIER_ORDERING,
    NOISE, DASH, OUTLINE, THICKNESS_MODIFIER, COLOR_MODIFIER, DEFORM, GENERATE,
    ADVANCED_INTERPOLATION, MULTIFRAME, ADVANCED_ONION_SKIN, VISUAL_EFFECTS,
    NEW_PROJECT, OPEN_PROJECT, SAVE, SAVE_AS, EXPORT, PROJECT_SETTINGS
}

data class FeatureCapability(
    val id: FeatureId,
    val state: FeatureState,
    val label: String,
    val reason: String
)

object FeatureRegistry {
    // Only functionality with a complete native Legacy GP path is marked available.
    // Everything else stays IN_PROGRESS or NOT_IMPLEMENTED until its real engine path
    // and Android presentation path are validated.
    private val available = setOf(
        // Core Legacy GP drawing/editing paths validated by native tests.
        FeatureId.FREEHAND, FeatureId.PRESSURE, FeatureId.ERASER, FeatureId.FILL,
        FeatureId.LINE, FeatureId.RECTANGLE, FeatureId.CIRCLE, FeatureId.ARC, FeatureId.POLYLINE,
        FeatureId.SELECT, FeatureId.LASSO,
        FeatureId.MOVE, FeatureId.ROTATE, FeatureId.SCALE, FeatureId.MIRROR,
        FeatureId.DUPLICATE, FeatureId.DELETE, FeatureId.SPLIT, FeatureId.SUBDIVIDE, FeatureId.TRIM, FeatureId.CLOSE,
        FeatureId.JOIN_STROKES, FeatureId.SELECT_FIRST, FeatureId.SELECT_GROUPED,
        // Existing native/UI animation path.
        FeatureId.FRAMES, FeatureId.TIMELINE, FeatureId.ADD_FRAME, FeatureId.DUPLICATE_FRAME,
        FeatureId.DELETE_FRAME, FeatureId.PLAYBACK, FeatureId.PAUSE, FeatureId.LOOP,
        FeatureId.FPS, FeatureId.FRAME_NAVIGATION,
        FeatureId.ONION_SKIN, FeatureId.ONION_RANGE, FeatureId.ONION_OPACITY, FeatureId.ONION_FADE,
        // Existing Legacy GP material/style path.
        FeatureId.MATERIALS, FeatureId.SELECT_MATERIAL, FeatureId.STROKE_COLOR,
        FeatureId.THICKNESS, FeatureId.OPACITY,
        FeatureId.STABILIZATION, FeatureId.SMOOTHING, FeatureId.SPACING, FeatureId.PRESSURE_CURVE,
        FeatureId.SCULPT, FeatureId.SCULPT_SMOOTH, FeatureId.SCULPT_THICKNESS, FeatureId.SCULPT_STRENGTH,
        FeatureId.SCULPT_PUSH, FeatureId.SCULPT_GRAB,
        FeatureId.GRID, FeatureId.GUIDES, FeatureId.SNAPPING, FeatureId.PAN, FeatureId.ZOOM, FeatureId.RESET_VIEW,
        FeatureId.LAYERS, FeatureId.LAYER_VISIBILITY, FeatureId.LAYER_LOCKING,
        FeatureId.LAYER_ORDERING, FeatureId.LAYER_DUPLICATION, FeatureId.LAYER_DELETION,
        FeatureId.LAYER_RENAME,
        // Real JSON persistence of Legacy GP layers, frames, strokes and points.
        FeatureId.NEW_PROJECT, FeatureId.OPEN_PROJECT, FeatureId.SAVE
    )
    private val inProgress = setOf(
        FeatureId.INSERT_FRAME,
        // Legacy GP interpolation currently supports only matched stroke topology;
        // Blender 3.6 interpolation also handles topology differences/pairing.
        FeatureId.INTERPOLATION,
        // The flag/selection path exists, but edit operations still target the
        // active frame rather than Blender's true multi-frame edit set.
        FeatureId.MULTIFRAME,
        FeatureId.ONION_LAYER_FILTER
    )

    fun capability(id: FeatureId): FeatureCapability {
        val state = when {
            id in inProgress -> FeatureState.IN_PROGRESS
            id in available -> FeatureState.AVAILABLE
            else -> FeatureState.NOT_IMPLEMENTED
        }
        val reason = when (state) {
            FeatureState.AVAILABLE -> "End-to-end native GP drawing path is connected."
            FeatureState.IN_PROGRESS -> "Native or UI groundwork exists, but the complete controller-to-engine path is not complete."
            FeatureState.NOT_IMPLEMENTED -> "No Project Grease engine operation is connected yet."
        }
        val label = id.name.replace('_', ' ').lowercase().replaceFirstChar { it.uppercase() }
        return FeatureCapability(id, state, label, reason)
    }

    fun all(): List<FeatureCapability> = FeatureId.entries.map(::capability)
}
