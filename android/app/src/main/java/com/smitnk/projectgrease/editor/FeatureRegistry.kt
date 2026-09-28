package com.smitnk.projectgrease.editor

enum class FeatureState { AVAILABLE, IN_PROGRESS, NOT_IMPLEMENTED }

enum class FeatureId {
    FREEHAND, PRESSURE, ERASER, LINE, RECTANGLE, CIRCLE, ARC, POLYLINE,
    SELECT, LASSO, MOVE, ROTATE, SCALE, MIRROR, DUPLICATE, DELETE, SPLIT, SUBDIVIDE, TRIM, CLOSE,
    LAYERS, LAYER_VISIBILITY, LAYER_LOCKING, LAYER_ORDERING, LAYER_DUPLICATION, LAYER_DELETION, LAYER_RENAME,
    FRAMES, TIMELINE, ADD_FRAME, INSERT_FRAME, DUPLICATE_FRAME, DELETE_FRAME, FRAME_HOLDS,
    PLAYBACK, PAUSE, LOOP, FPS, FRAME_NAVIGATION, KEYFRAME, INTERPOLATION,
    ONION_SKIN, ONION_RANGE, ONION_OPACITY, ONION_FADE, ONION_LAYER_FILTER,
    MATERIALS, CREATE_MATERIAL, DELETE_MATERIAL, SELECT_MATERIAL, STROKE_COLOR, FILL_COLOR,
    THICKNESS, OPACITY, FILL_ENABLE, PAN, ZOOM, FIT_CANVAS, RESET_VIEW, GRID, GUIDES, SNAPPING,
    STABILIZATION, SMOOTHING, SPACING, PRESSURE_CURVE,
    SCULPT, SCULPT_GRAB, SCULPT_SMOOTH, SCULPT_PUSH, SCULPT_PINCH, SCULPT_RANDOMIZE,
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
    private val available = setOf(FeatureId.FREEHAND, FeatureId.PRESSURE)
    private val inProgress = setOf(
        FeatureId.DUPLICATE, FeatureId.DELETE, FeatureId.SPLIT, FeatureId.SUBDIVIDE,
        FeatureId.TRIM, FeatureId.CLOSE, FeatureId.MOVE, FeatureId.MIRROR,
        FeatureId.LAYERS, FeatureId.LAYER_VISIBILITY, FeatureId.LAYER_LOCKING,
        FeatureId.LAYER_ORDERING, FeatureId.LAYER_DUPLICATION, FeatureId.LAYER_DELETION,
        FeatureId.LAYER_RENAME, FeatureId.FRAMES, FeatureId.TIMELINE, FeatureId.ADD_FRAME,
        FeatureId.INSERT_FRAME, FeatureId.DUPLICATE_FRAME, FeatureId.DELETE_FRAME,
        FeatureId.FRAME_HOLDS, FeatureId.PLAYBACK, FeatureId.PAUSE, FeatureId.LOOP,
        FeatureId.FPS, FeatureId.FRAME_NAVIGATION, FeatureId.MATERIALS,
        FeatureId.SELECT_MATERIAL, FeatureId.THICKNESS, FeatureId.OPACITY,
        FeatureId.ONION_SKIN, FeatureId.PAN, FeatureId.ZOOM, FeatureId.RESET_VIEW
    )

    fun capability(id: FeatureId): FeatureCapability {
        val state = when {
            id in available -> FeatureState.AVAILABLE
            id in inProgress -> FeatureState.IN_PROGRESS
            else -> FeatureState.NOT_IMPLEMENTED
        }
        val reason = when (state) {
            FeatureState.AVAILABLE -> "End-to-end native GP drawing path is connected."
            FeatureState.IN_PROGRESS -> "Native or UI groundwork exists, but the complete controller-to-engine path is not complete."
            FeatureState.NOT_IMPLEMENTED -> "No Project Grease engine operation is connected yet."
        }
        val label = id.name.replace('_', ' ').lowercase().replaceFirstChar { it.uppercaseChar() }
        return FeatureCapability(id, state, label, reason)
    }

    fun all(): List<FeatureCapability> = FeatureId.entries.map(::capability)
}
