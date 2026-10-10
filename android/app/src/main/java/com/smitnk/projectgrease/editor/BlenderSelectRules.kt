package com.smitnk.projectgrease.editor

/**
 * Ids and argument layouts for the Blender 3.6.23 Legacy GP selection operators.
 * Values mirror native/blender_gp/project_grease_blender_select.h, which mirrors
 * Blender's ED_select_utils.h (SEL_*, eSelectOp) and eGP_Selectmode.
 */
object ProjectGreaseSelect {
    // SEL_TOGGLE / SEL_SELECT / SEL_DESELECT / SEL_INVERT
    const val ACTION_TOGGLE = 0
    const val ACTION_SELECT = 1
    const val ACTION_DESELECT = 2
    const val ACTION_INVERT = 3

    // eSelectOp
    const val OP_ADD = 1
    const val OP_SUB = 2
    const val OP_SET = 3
    const val OP_AND = 4
    const val OP_XOR = 5

    // eGP_Selectmode
    const val MODE_POINT = 0
    const val MODE_STROKE = 1
    const val MODE_SEGMENT = 2

    // project_grease_blender_edit5.h
    const val CMD_SELECT_SEGMENT = 86
    const val CMD_MATERIAL_REMOVE = 87
    const val CMD_ONION_LAYER = 88
    const val CMD_ONION_FADE = 89

    // GPENCIL_OT_select_grouped "type"
    const val GROUP_LAYER = 0
    const val GROUP_MATERIAL = 1

    // applyEditCommand ids
    const val CMD_ALL = 20
    const val CMD_LINKED = 21
    const val CMD_ALTERNATE = 22
    const val CMD_MORE = 23
    const val CMD_LESS = 24
    const val CMD_FIRST = 25
    const val CMD_LAST = 26
    const val CMD_GROUPED = 27
    const val CMD_LASSO = 28
    const val CMD_BOX = 29
    const val CMD_CIRCLE = 30

    class Command(val id: Int, val args: FloatArray)

    fun isValidOp(op: Int): Boolean = op in OP_ADD..OP_XOR
    /** Modes the native pick operator takes directly (segment click uses its dedicated operator). */
    fun isValidMode(mode: Int): Boolean = mode == MODE_POINT || mode == MODE_STROKE
    /** Modes of the Edit-mode switch (Point / Stroke / Segment). */
    fun isValidSelectMode(mode: Int): Boolean = mode == MODE_POINT || mode == MODE_STROKE || mode == MODE_SEGMENT
    /** Preserve Blender's selected mode for area selection; native Legacy GP expands hit points to segments. */
    fun areaMode(mode: Int): Int = if (isValidSelectMode(mode)) mode else MODE_POINT

    /** Click select in segment mode: args x, y, radius_squared, flags. */
    fun segmentPick(x: Float, y: Float, radiusSquared: Int, flags: Int): Command? {
        if (radiusSquared < 0 || !finite(x, y)) return null
        return Command(CMD_SELECT_SEGMENT, floatArrayOf(x, y, radiusSquared.toFloat(), flags.toFloat()))
    }
    fun materialRemove(index: Int): Command? =
        if (index >= 0) Command(CMD_MATERIAL_REMOVE, floatArrayOf(index.toFloat())) else null
    fun onionLayer(layer: Int, enabled: Boolean): Command? =
        if (layer >= 0) Command(CMD_ONION_LAYER, floatArrayOf(layer.toFloat(), flag(enabled))) else null
    fun onionFade(enabled: Boolean) = Command(CMD_ONION_FADE, floatArrayOf(flag(enabled)))

    /** ED_select_op_modal(): only the first sample of a SET gesture replaces the selection. */
    fun opModal(op: Int, isFirst: Boolean): Int = if (op == OP_SET && !isFirst) OP_ADD else op

    private fun finite(vararg values: Float): Boolean = values.all { it.isFinite() }
    private fun flag(value: Boolean): Float = if (value) 1f else 0f

    fun all(action: Int): Command? =
        if (action in ACTION_TOGGLE..ACTION_INVERT) Command(CMD_ALL, floatArrayOf(action.toFloat())) else null

    fun linked() = Command(CMD_LINKED, FloatArray(0))
    fun alternate(unselectEnds: Boolean) = Command(CMD_ALTERNATE, floatArrayOf(flag(unselectEnds)))
    fun more() = Command(CMD_MORE, FloatArray(0))
    fun less() = Command(CMD_LESS, FloatArray(0))
    fun first(onlySelectedStrokes: Boolean, extend: Boolean) =
        Command(CMD_FIRST, floatArrayOf(flag(onlySelectedStrokes), flag(extend)))
    fun last(onlySelectedStrokes: Boolean, extend: Boolean) =
        Command(CMD_LAST, floatArrayOf(flag(onlySelectedStrokes), flag(extend)))

    fun grouped(type: Int): Command? =
        if (type == GROUP_LAYER || type == GROUP_MATERIAL) Command(CMD_GROUPED, floatArrayOf(type.toFloat())) else null

    /** args: op, mode, x0, y0, x1, y1, ... (needs at least three points). */
    fun lasso(op: Int, mode: Int, points: List<Pair<Float, Float>>): Command? {
        if (!isValidOp(op) || !isValidSelectMode(mode) || points.size < 3) return null
        val args = FloatArray(2 + points.size * 2)
        args[0] = op.toFloat()
        args[1] = mode.toFloat()
        points.forEachIndexed { i, p ->
            if (!finite(p.first, p.second)) return null
            args[2 + i * 2] = p.first
            args[2 + i * 2 + 1] = p.second
        }
        return Command(CMD_LASSO, args)
    }

    /** args: op, mode, xmin, ymin, xmax, ymax. */
    fun box(op: Int, mode: Int, x0: Float, y0: Float, x1: Float, y1: Float): Command? {
        if (!isValidOp(op) || !isValidSelectMode(mode) || !finite(x0, y0, x1, y1)) return null
        return Command(CMD_BOX, floatArrayOf(op.toFloat(), mode.toFloat(), x0, y0, x1, y1))
    }

    /** args: op, mode, x, y, radius, isFirst. */
    fun circle(op: Int, mode: Int, x: Float, y: Float, radius: Float, isFirst: Boolean): Command? {
        if (!isValidOp(op) || !isValidSelectMode(mode) || !finite(x, y, radius) || radius < 0f) return null
        return Command(CMD_CIRCLE, floatArrayOf(op.toFloat(), mode.toFloat(), x, y, radius, flag(isFirst)))
    }

    // ---- selection-aware editing (native/blender_gp/project_grease_blender_edit.h) ----
    const val PICK_EXTEND = 1
    const val PICK_DESELECT = 2
    const val PICK_TOGGLE = 4
    const val PICK_ENTIRE = 8
    const val PICK_DESELECT_ALL = 16
    const val PICK_PASSTHROUGH = 32

    const val CMD_PICK = 31
    const val CMD_TRANSLATE = 32
    const val CMD_ROTATE = 33
    const val CMD_SCALE = 34
    const val CMD_MIRROR = 35
    const val CMD_DELETE_STROKES = 36
    const val CMD_DELETE_POINTS = 37

    /** gpencil_select_exec(): radius = 0.4 * widget_unit (20) = 8, scaled to canvas units by the zoom. */
    const val PICK_RADIUS = 8f

    /** `(int)(radius * radius)`: Blender compares the Manhattan distance with this squared radius. */
    fun pickRadiusSquared(zoom: Float): Int {
        val radius = PICK_RADIUS / zoom.coerceAtLeast(0.1f)
        return (radius * radius).toInt()
    }

    fun pick(x: Float, y: Float, radiusSquared: Int, flags: Int, mode: Int): Command? {
        if (!isValidMode(mode) || radiusSquared < 0 || !finite(x, y)) return null
        return Command(CMD_PICK, floatArrayOf(x, y, radiusSquared.toFloat(), flags.toFloat(), mode.toFloat()))
    }

    private fun withPivot(values: FloatArray, pivot: FloatArray?): FloatArray? {
        if (pivot == null) return values
        if (pivot.size < 2 || !finite(pivot[0], pivot[1])) return null
        return values + floatArrayOf(pivot[0], pivot[1])
    }

    fun translate(dx: Float, dy: Float): Command? =
        if (finite(dx, dy)) Command(CMD_TRANSLATE, floatArrayOf(dx, dy)) else null

    fun rotate(radians: Float, pivot: FloatArray? = null): Command? {
        if (!finite(radians)) return null
        return withPivot(floatArrayOf(radians), pivot)?.let { Command(CMD_ROTATE, it) }
    }

    fun scale(sx: Float, sy: Float, pivot: FloatArray? = null): Command? {
        if (!finite(sx, sy) || sx == 0f || sy == 0f) return null
        return withPivot(floatArrayOf(sx, sy), pivot)?.let { Command(CMD_SCALE, it) }
    }

    fun mirror(mirrorX: Boolean, mirrorY: Boolean, pivot: FloatArray? = null): Command? {
        if (!mirrorX && !mirrorY) return null
        return withPivot(floatArrayOf(flag(mirrorX), flag(mirrorY)), pivot)?.let { Command(CMD_MIRROR, it) }
    }

    fun deleteStrokes() = Command(CMD_DELETE_STROKES, FloatArray(0))
    fun deletePoints() = Command(CMD_DELETE_POINTS, FloatArray(0))

    // Legacy GP modifiers baked into the selected strokes (MOD_gpencil_legacy_thick/opacity.c)
    const val CMD_MOD_THICKNESS = 40
    const val CMD_MOD_OPACITY = 41
    const val MODIFY_BOTH = 0
    const val MODIFY_STROKE = 1
    const val MODIFY_FILL = 2
    const val MODIFY_HARDNESS = 3

    fun thicknessModifier(factor: Float, normalize: Boolean = false, thickness: Int = 0): Command? =
        if (finite(factor) && thickness >= 0) Command(CMD_MOD_THICKNESS, floatArrayOf(flag(normalize), thickness.toFloat(), factor)) else null

    fun opacityModifier(mode: Int, factor: Float, normalize: Boolean = false, hardness: Float = 1f): Command? =
        if (mode in MODIFY_BOTH..MODIFY_HARDNESS && finite(factor, hardness))
            Command(CMD_MOD_OPACITY, floatArrayOf(mode.toFloat(), factor, flag(normalize), hardness)) else null

    // MOD_gpencil_legacy_length.c (no random offsets). Defaults follow DNA_gpencil_modifier_defaults.h.
    const val CMD_MOD_LENGTH = 42
    const val LENGTH_RELATIVE = 0
    const val LENGTH_ABSOLUTE = 1

    fun lengthModifier(start: Float, end: Float, mode: Int = LENGTH_RELATIVE, overshoot: Float = 0.1f,
                       useCurvature: Boolean = false, pointDensity: Float = 30f, segmentInfluence: Float = 0f,
                       maxAngle: Float = 2.9670597f, invertCurvature: Boolean = false): Command? {
        if (mode != LENGTH_RELATIVE && mode != LENGTH_ABSOLUTE) return null
        if (!finite(start, end) || !finite(overshoot, pointDensity) || !finite(segmentInfluence, maxAngle)) return null
        return Command(CMD_MOD_LENGTH, floatArrayOf(mode.toFloat(), start, end, overshoot, flag(useCurvature),
            pointDensity, segmentInfluence, maxAngle, flag(invertCurvature)))
    }

    // MOD_gpencil_legacy_tint.c, uniform type (gradient needs a scene object)
    const val CMD_MOD_TINT = 43
    const val PAINT_STROKE = 0
    const val PAINT_FILL = 1
    const val PAINT_BOTH = 2

    fun tintModifier(mode: Int, factor: Float, r: Float, g: Float, b: Float): Command? {
        if (mode !in PAINT_STROKE..PAINT_BOTH || !finite(factor) || !finite(r, g) || !finite(b)) return null
        return Command(CMD_MOD_TINT, floatArrayOf(mode.toFloat(), factor, r, g, b))
    }

    // MOD_gpencil_legacy_color.c (Hue/Saturation). Defaults: hue 0.5 (no shift), saturation 1, value 1.
    const val CMD_MOD_COLOR = 44

    fun colorModifier(mode: Int, hue: Float = 0.5f, saturation: Float = 1f, value: Float = 1f): Command? {
        if (mode !in MODIFY_BOTH..MODIFY_FILL || !finite(hue, saturation) || !finite(value)) return null
        return Command(CMD_MOD_COLOR, floatArrayOf(mode.toFloat(), hue, saturation, value))
    }

    // Legacy GP stroke operators (stroke_arrange, stroke_change_color, stroke_reset_vertex_color,
    // stroke_flip, stroke_cyclical_set, snap_to_grid)
    const val CMD_ARRANGE = 45
    const val CMD_SET_MATERIAL = 46
    const val CMD_RESET_VCOLOR = 47
    const val CMD_FLIP = 48
    const val CMD_CYCLIC = 49
    const val CMD_SNAP_GRID = 50
    const val ARRANGE_TOP = 0
    const val ARRANGE_UP = 1
    const val ARRANGE_DOWN = 2
    const val ARRANGE_BOTTOM = 3
    const val CYCLIC_CLOSE = 1
    const val CYCLIC_OPEN = 2
    const val CYCLIC_TOGGLE = 3

    fun arrange(direction: Int): Command? =
        if (direction in ARRANGE_TOP..ARRANGE_BOTTOM) Command(CMD_ARRANGE, floatArrayOf(direction.toFloat())) else null
    fun setMaterial(index: Int): Command? =
        if (index >= 0) Command(CMD_SET_MATERIAL, floatArrayOf(index.toFloat())) else null
    fun resetVertexColor(mode: Int): Command? =
        if (mode in PAINT_STROKE..PAINT_BOTH) Command(CMD_RESET_VCOLOR, floatArrayOf(mode.toFloat())) else null
    fun flip() = Command(CMD_FLIP, FloatArray(0))
    fun cyclic(type: Int): Command? =
        if (type in CYCLIC_CLOSE..CYCLIC_TOGGLE) Command(CMD_CYCLIC, floatArrayOf(type.toFloat())) else null
    fun snapToGrid(grid: Float): Command? =
        if (finite(grid) && grid > 0f) Command(CMD_SNAP_GRID, floatArrayOf(grid)) else null

    // GPENCIL_OT_duplicate / dissolve / stroke_split / stroke_join
    const val CMD_DUPLICATE = 51
    const val CMD_DISSOLVE = 52
    const val CMD_SPLIT = 53
    const val CMD_JOIN = 54
    const val DISSOLVE_POINTS = 0
    const val DISSOLVE_BETWEEN = 1
    const val DISSOLVE_UNSELECT = 2

    fun duplicate() = Command(CMD_DUPLICATE, FloatArray(0))
    fun dissolve(type: Int): Command? =
        if (type in DISSOLVE_POINTS..DISSOLVE_UNSELECT) Command(CMD_DISSOLVE, floatArrayOf(type.toFloat())) else null
    fun split() = Command(CMD_SPLIT, FloatArray(0))
    fun join(leaveGaps: Boolean = false) = Command(CMD_JOIN, floatArrayOf(flag(leaveGaps)))

    // Vertex Paint mode brushes (gpencil_vertex_paint.c)
    const val CMD_VERTEX_PAINT = 55
    const val VPAINT_DRAW = 0
    const val VPAINT_BLUR = 1
    const val VPAINT_AVERAGE = 2
    const val VPAINT_SMEAR = 3
    const val VPAINT_REPLACE = 4
    /** GPAINT_TOOL_TINT / GPVERTEX_TOOL_TINT (the Draw-mode Tint tool; native session only). */
    const val VPAINT_TINT = 5

    fun vertexPaint(brush: Int, x: Float, y: Float, radius: Float, strength: Float,
                    r: Float, g: Float, b: Float, target: Int, dx: Float = 0f, dy: Float = 0f): Command? {
        if (brush !in VPAINT_DRAW..VPAINT_REPLACE || target !in PAINT_STROKE..PAINT_BOTH) return null
        if (!finite(x, y) || !finite(radius, strength) || radius <= 0f || !finite(r, g) || !finite(b) || !finite(dx, dy)) return null
        return Command(CMD_VERTEX_PAINT, floatArrayOf(brush.toFloat(), x, y, radius, strength, r, g, b, target.toFloat(), dx, dy))
    }

    // Mirror modifier baked as copies, Weight Paint draw, interpolation easing ids
    const val CMD_MIRROR_COPY = 56
    const val CMD_WEIGHT_PAINT = 57
    const val EASE_LINEAR = 0
    const val EASE_QUAD = 1
    const val EASE_CUBIC = 2
    const val EASE_QUART = 3
    const val EASE_QUINT = 4
    const val EASE_SINE = 5
    const val EASE_EXPO = 6
    const val EASE_CIRC = 7
    const val EASE_BACK = 8
    const val EASE_BOUNCE = 9
    /** BEZT_IPO_ELASTIC (BLI_easing_elastic_*, amplitude / period set with [easingParams]). */
    const val EASE_ELASTIC = 10
    const val EASE_IN = 0
    const val EASE_OUT = 1
    const val EASE_IN_OUT = 2

    fun mirrorCopy(axisX: Boolean, axisY: Boolean, pivotX: Float, pivotY: Float): Command? {
        if ((!axisX && !axisY) || !finite(pivotX, pivotY)) return null
        return Command(CMD_MIRROR_COPY, floatArrayOf(flag(axisX), flag(axisY), pivotX, pivotY))
    }
    fun weightPaint(group: Int, x: Float, y: Float, radius: Float, strength: Float, weight: Float): Command? {
        if (group < 0 || !finite(x, y) || !finite(radius, strength) || !finite(weight) || radius <= 0f) return null
        return Command(CMD_WEIGHT_PAINT, floatArrayOf(group.toFloat(), x, y, radius, strength, weight))
    }

    // project_grease_blender_edit2.h
    const val CMD_MOD_THICKNESS_VGROUP = 58
    const val CMD_SELECT_VCOLOR = 59
    const val CMD_NORMALIZE = 60
    const val CMD_SIMPLIFY_FIXED = 61
    const val CMD_SAMPLE = 62
    const val CMD_EXTRUDE = 63
    const val NORMALIZE_THICKNESS = 0
    const val NORMALIZE_OPACITY = 1

    fun thicknessModifierVGroup(group: Int, invert: Boolean, factor: Float, normalize: Boolean = false, thickness: Int = 0): Command? {
        if (group < -1 || thickness < 0 || !finite(factor)) return null
        return Command(CMD_MOD_THICKNESS_VGROUP, floatArrayOf(group.toFloat(), flag(invert), flag(normalize), thickness.toFloat(), factor))
    }
    fun selectVertexColor(r: Float, g: Float, b: Float, threshold: Float, extend: Boolean = false): Command? {
        if (!finite(r, g) || !finite(b, threshold) || threshold < 0f) return null
        return Command(CMD_SELECT_VCOLOR, floatArrayOf(r, g, b, threshold, flag(extend)))
    }
    fun normalize(mode: Int, value: Float): Command? =
        if ((mode == NORMALIZE_THICKNESS || mode == NORMALIZE_OPACITY) && finite(value)) Command(CMD_NORMALIZE, floatArrayOf(mode.toFloat(), value)) else null
    fun simplifyFixed(steps: Int): Command? = if (steps in 1..100) Command(CMD_SIMPLIFY_FIXED, floatArrayOf(steps.toFloat())) else null
    fun sample(length: Float, sharpThreshold: Float = 0.1f): Command? =
        if (finite(length, sharpThreshold) && length > 0f) Command(CMD_SAMPLE, floatArrayOf(length, sharpThreshold)) else null
    fun extrude() = Command(CMD_EXTRUDE, FloatArray(0))

    // project_grease_blender_edit3.h
    const val CMD_SELECT_RANDOM = 66
    const val CMD_BLANK_FRAME = 67
    const val CMD_FILL_COLOR = 68
    const val CMD_CLEAN_LOOSE = 69
    const val CMD_CLEAN_DUP_FRAMES = 70
    const val CMD_VCOLOR_SET = 71
    const val CMD_VCOLOR_INVERT = 72
    const val CMD_VCOLOR_BC = 73
    const val CMD_VCOLOR_HSV = 74
    const val CMD_VCOLOR_LEVELS = 75

    fun selectRandom(ratio: Float, seed: Int, select: Boolean = true): Command? =
        if (finite(ratio) && seed >= 0) Command(CMD_SELECT_RANDOM, floatArrayOf(ratio.coerceIn(0f, 1f), seed.toFloat(), flag(select))) else null
    fun blankFrame(frame: Int): Command? = if (frame >= 0) Command(CMD_BLANK_FRAME, floatArrayOf(frame.toFloat())) else null
    fun fillColor(material: Int, r: Float, g: Float, b: Float, a: Float): Command? =
        if (material >= 0 && finite(r, g) && finite(b, a)) Command(CMD_FILL_COLOR, floatArrayOf(material.toFloat(), r, g, b, a)) else null
    fun cleanLoose(limit: Int = 1): Command? = if (limit >= 1) Command(CMD_CLEAN_LOOSE, floatArrayOf(limit.toFloat())) else null
    fun cleanDuplicateFrames() = Command(CMD_CLEAN_DUP_FRAMES, FloatArray(0))
    private fun paintMode(mode: Int) = mode in PAINT_STROKE..PAINT_BOTH
    fun vcolorSet(mode: Int, r: Float, g: Float, b: Float, factor: Float = 1f): Command? =
        if (paintMode(mode) && finite(r, g) && finite(b, factor)) Command(CMD_VCOLOR_SET, floatArrayOf(mode.toFloat(), r, g, b, factor)) else null
    fun vcolorInvert(mode: Int): Command? = if (paintMode(mode)) Command(CMD_VCOLOR_INVERT, floatArrayOf(mode.toFloat())) else null
    fun vcolorBrightnessContrast(mode: Int, brightness: Float, contrast: Float): Command? =
        if (paintMode(mode) && finite(brightness, contrast)) Command(CMD_VCOLOR_BC, floatArrayOf(mode.toFloat(), brightness, contrast)) else null
    fun vcolorHsv(mode: Int, h: Float = 0.5f, s: Float = 1f, v: Float = 1f): Command? =
        if (paintMode(mode) && finite(h, s) && finite(v)) Command(CMD_VCOLOR_HSV, floatArrayOf(mode.toFloat(), h, s, v)) else null
    fun vcolorLevels(mode: Int, offset: Float, gain: Float): Command? =
        if (paintMode(mode) && finite(offset, gain)) Command(CMD_VCOLOR_LEVELS, floatArrayOf(mode.toFloat(), offset, gain)) else null

    // project_grease_blender_edit4.h
    const val CMD_DASH = 76
    const val CMD_MULTIPLY = 77
    const val CMD_ARRAY = 78
    const val CMD_MERGE_DISTANCE = 79
    const val CMD_CAPS = 80
    const val CMD_START_SET = 81
    const val CMD_SEPARATE_LAYER = 82
    const val CMD_MOVE_TO_LAYER = 83
    const val CMD_COPY = 84
    const val CMD_PASTE = 85
    /** GPENCIL_OT_stroke_separate (modes POINT / STROKE) into new layers; edit10 id 135. */
    const val CMD_SEPARATE = 135
    const val SEPARATE_POINT = 0
    const val SEPARATE_STROKE = 1
    const val CAPS_TOGGLE_BOTH = 0
    const val CAPS_TOGGLE_START = 1
    const val CAPS_TOGGLE_END = 2
    const val CAPS_DEFAULT = 3

    fun dash(dash: Int, gap: Int, offset: Int = 0): Command? =
        if (dash >= 1 && gap >= 1) Command(CMD_DASH, floatArrayOf(dash.toFloat(), gap.toFloat(), offset.toFloat())) else null
    fun multiply(duplications: Int, distance: Float): Command? =
        if (duplications in 1..100 && finite(distance)) Command(CMD_MULTIPLY, floatArrayOf(duplications.toFloat(), distance)) else null
    fun array(count: Int, offsetX: Float, offsetY: Float): Command? =
        if (count in 2..1000 && finite(offsetX, offsetY)) Command(CMD_ARRAY, floatArrayOf(count.toFloat(), offsetX, offsetY)) else null
    fun mergeByDistance(threshold: Float, useUnselected: Boolean = false): Command? =
        if (finite(threshold) && threshold > 0f) Command(CMD_MERGE_DISTANCE, floatArrayOf(threshold, flag(useUnselected))) else null
    fun caps(type: Int): Command? =
        if (type in CAPS_TOGGLE_BOTH..CAPS_DEFAULT) Command(CMD_CAPS, floatArrayOf(type.toFloat())) else null
    fun startSet() = Command(CMD_START_SET, FloatArray(0))
    fun separateToLayer() = Command(CMD_SEPARATE_LAYER, FloatArray(0))
    fun separate(mode: Int): Command? =
        if (mode == SEPARATE_POINT || mode == SEPARATE_STROKE) Command(CMD_SEPARATE, floatArrayOf(mode.toFloat())) else null
    fun moveToLayer(index: Int): Command? = if (index >= 0) Command(CMD_MOVE_TO_LAYER, floatArrayOf(index.toFloat())) else null
    fun copy() = Command(CMD_COPY, FloatArray(0))
    fun paste() = Command(CMD_PASTE, FloatArray(0))

    // project_grease_blender_edit6.h (86..89 are edit5's segment / material / onion commands)
    const val CMD_OUTLINE = 90
    const val CMD_ONION_STYLE = 91
    const val CMD_MATERIAL_TEXTURE = 92
    // project_grease_blender_edit7.h: vertex-group operators on the selection and layer operators
    const val CMD_VG_ASSIGN = 93
    const val CMD_VG_REMOVE = 94
    const val CMD_VG_SELECT = 95
    const val CMD_VG_DESELECT = 96
    const val CMD_VG_INVERT = 97
    const val CMD_VG_NORMALIZE = 98
    const val CMD_LAYER_MERGE = 99
    const val CMD_LAYER_ISOLATE = 100
    const val CMD_LOCK_ALL = 101
    const val CMD_UNLOCK_ALL = 102
    fun vgAssign(group: Int, weight: Float = 1f): Command? =
        if (group >= 0 && finite(weight)) Command(CMD_VG_ASSIGN, floatArrayOf(group.toFloat(), weight.coerceIn(0f, 1f))) else null
    fun vgOp(cmd: Int, group: Int): Command? =
        if (group >= 0 && cmd in CMD_VG_REMOVE..CMD_VG_NORMALIZE) Command(cmd, floatArrayOf(group.toFloat())) else null
    fun layerOp(cmd: Int): Command? = if (cmd in CMD_LAYER_MERGE..CMD_UNLOCK_ALL) Command(cmd, FloatArray(0)) else null

    // ---- Batch 21 document state (project_grease_blender_edit8.h) ----
    const val CMD_FRAME_KEYTYPE = 103
    const val CMD_FRAME_SELECT = 104
    const val CMD_FRAME_DESELECT = 105
    const val CMD_LAYER_BLEND = 106
    const val CMD_LAYER_TINT = 107
    const val CMD_LAYER_LINE = 108
    const val CMD_MATERIAL_MOVE = 109
    const val CMD_MATERIAL_FLAGS = 110
    const val CMD_MATERIAL_SOLO = 111
    const val CMD_MATERIAL_MODE = 112
    const val CMD_ONION_FILTER = 113
    const val CMD_LAYER_PASS = 114
    const val CMD_MATERIAL_PASS = 115
    const val CMD_EASING_PARAMS = 116
    /** BEZT_KEYTYPE_*: keyframe, extreme, breakdown, jitter, moving hold. */
    const val KEY_KEYFRAME = 0
    const val KEY_EXTREME = 1
    const val KEY_BREAKDOWN = 2
    const val KEY_JITTER = 3
    const val KEY_MOVEHOLD = 4
    val KEY_TYPE_LABELS = listOf("Keyframe", "Extreme", "Breakdown", "Jitter", "Moving Hold")
    /** Frame selection modes of CMD_FRAME_SELECT. */
    const val FRAME_SELECT_SET = 0
    const val FRAME_SELECT_TOGGLE = 1
    const val FRAME_SELECT_ADD = 2
    /** eGPLayerBlendModes */
    val BLEND_LABELS = listOf("Regular", "Hard Light", "Add", "Subtract", "Multiply", "Divide")
    /** GP_MATERIAL_MODE_* and GP_MATERIAL_FOLLOW_* */
    val LINE_TYPE_LABELS = listOf("Line", "Dots", "Squares")
    val ALIGNMENT_LABELS = listOf("Path", "Object", "Fixed")

    fun frameKeyType(frame: Int, type: Int, allLayers: Boolean = false): Command? =
        if (type in KEY_KEYFRAME..KEY_MOVEHOLD) Command(CMD_FRAME_KEYTYPE, floatArrayOf(frame.toFloat(), type.toFloat(), if (allLayers) 1f else 0f)) else null
    fun frameSelect(frame: Int, mode: Int, allLayers: Boolean = false): Command =
        Command(CMD_FRAME_SELECT, floatArrayOf(frame.toFloat(), mode.coerceIn(0, 2).toFloat(), if (allLayers) 1f else 0f))
    fun frameDeselect(allLayers: Boolean = false) = Command(CMD_FRAME_DESELECT, floatArrayOf(if (allLayers) 1f else 0f))
    fun layerBlend(layer: Int, mode: Int): Command? = if (mode in 0..5) Command(CMD_LAYER_BLEND, floatArrayOf(layer.toFloat(), mode.toFloat())) else null
    fun layerTint(layer: Int, r: Float, g: Float, b: Float, factor: Float) = Command(CMD_LAYER_TINT, floatArrayOf(layer.toFloat(), r, g, b, factor))
    fun layerLineChange(layer: Int, px: Int) = Command(CMD_LAYER_LINE, floatArrayOf(layer.toFloat(), px.toFloat()))
    fun layerPass(layer: Int, pass: Int) = Command(CMD_LAYER_PASS, floatArrayOf(layer.toFloat(), pass.toFloat()))
    fun materialMove(from: Int, to: Int) = Command(CMD_MATERIAL_MOVE, floatArrayOf(from.toFloat(), to.toFloat()))
    fun materialFlags(slot: Int, locked: Boolean, hidden: Boolean) = Command(CMD_MATERIAL_FLAGS, floatArrayOf(slot.toFloat(), if (locked) 1f else 0f, if (hidden) 1f else 0f))
    fun materialSolo(slot: Int) = Command(CMD_MATERIAL_SOLO, floatArrayOf(slot.toFloat()))
    fun materialMode(slot: Int, mode: Int, alignment: Int, rotation: Float): Command? =
        if (mode in 0..2 && alignment in 0..2) Command(CMD_MATERIAL_MODE, floatArrayOf(slot.toFloat(), mode.toFloat(), alignment.toFloat(), rotation)) else null
    fun materialPass(slot: Int, pass: Int) = Command(CMD_MATERIAL_PASS, floatArrayOf(slot.toFloat(), pass.toFloat()))
    fun onionFilter(keyType: Int, loop: Boolean): Command? =
        if (keyType in -1..KEY_MOVEHOLD) Command(CMD_ONION_FILTER, floatArrayOf(keyType.toFloat(), if (loop) 1f else 0f)) else null
    fun easingParams(amplitude: Float, period: Float) = Command(CMD_EASING_PARAMS, floatArrayOf(amplitude, period))

    /** edit9 (project_grease_blender_edit9.h): proportional transform, pivots, dope-sheet frame ops, dash segments. */
    const val CMD_TRANSFORM = 117
    const val CMD_FRAMES_SELECT_RANGE = 118
    const val CMD_FRAMES_MOVE = 119
    const val CMD_FRAMES_SCALE = 120
    const val CMD_FRAMES_COPY = 121
    const val CMD_FRAMES_PASTE = 122
    const val CMD_DASH_SEGMENTS = 123
    const val CMD_MATERIAL_GRADIENT = 124
    const val CMD_MATERIAL_OPTIONS = 125
    /** pg_gp_interp_dispatch (project_grease_blender_interp.h). */
    const val CMD_INTERPOLATE = 126
    const val INTERP_NOFLIP = 0
    const val INTERP_FLIP = 1
    const val INTERP_FLIP_AUTO = 2
    val INTERP_FLIP_LABELS = listOf("No flip", "Flip", "Auto flip")
    fun interpolate(frame: Int, step: Int, flip: Int, onlySelected: Boolean, excludeBreakdowns: Boolean, easingType: Int,
                    easingMode: Int, smoothFactor: Float, smoothSteps: Int, single: Boolean, allLayers: Boolean = false) =
        Command(CMD_INTERPOLATE, floatArrayOf(frame.toFloat(), step.coerceAtLeast(1).toFloat(), flip.coerceIn(0, 2).toFloat(),
            if (onlySelected) 1f else 0f, if (excludeBreakdowns) 1f else 0f, if (allLayers) 1f else 0f, easingType.toFloat(),
            easingMode.toFloat(), smoothFactor.coerceIn(0f, 2f), smoothSteps.coerceIn(1, 3).toFloat(), if (single) 1f else 0f))
    val GRADIENT_TYPE_LABELS = listOf("Linear", "Radial")
    /** [g] = [type, mix r, g, b, a, mix factor, angle, scale x, y, offset x, y, flip] (MaterialRecord.gradient). */
    fun materialGradient(slot: Int, enabled: Boolean, g: FloatArray): Command? {
        if (g.size < 12 || g.any { !it.isFinite() } || g[0].toInt() !in 0..1) return null
        return Command(CMD_MATERIAL_GRADIENT, floatArrayOf(slot.toFloat(), if (enabled) 1f else 0f) + g)
    }
    fun materialOptions(slot: Int, strokeHoldout: Boolean, fillHoldout: Boolean, selfOverlap: Boolean) =
        Command(CMD_MATERIAL_OPTIONS, floatArrayOf(slot.toFloat(), if (strokeHoldout) 1f else 0f, if (fillHoldout) 1f else 0f, if (selfOverlap) 1f else 0f))
    const val XFORM_TRANSLATE = 0
    const val XFORM_ROTATE = 1
    const val XFORM_SCALE = 2
    /** Pivot point (V3D_AROUND_*) order of PG_PIVOT_*. */
    const val PIVOT_MEDIAN = 0
    const val PIVOT_BOUNDS = 1
    const val PIVOT_INDIVIDUAL = 2
    const val PIVOT_CURSOR = 3
    val PIVOT_LABELS = listOf("Median Point", "Bounding Box Center", "Individual Origins", "2D Cursor")
    /** Proportional falloff (PROP_*), RANDOM (6) is not offered. */
    val FALLOFF_VALUES = intArrayOf(0, 1, 2, 3, 4, 5, 7)
    val FALLOFF_LABELS = listOf("Smooth", "Sphere", "Root", "Sharp", "Linear", "Constant", "Inverse Square")

    data class TransformSettings(
        val pivot: Int = PIVOT_MEDIAN,
        val cursorX: Float = 0f,
        val cursorY: Float = 0f,
        val proportional: Boolean = false,
        val connected: Boolean = false,
        val falloff: Int = 0,
        val size: Float = 100f,
        val snapIncrement: Float = 0f,
    ) {
        /** True when the plain (non-edit9) transform path would give a different result. */
        val needsEdit9: Boolean get() = proportional || pivot != PIVOT_MEDIAN || snapIncrement > 0f
    }

    fun transform(type: Int, a: Float, b: Float, s: TransformSettings): Command? {
        if (type !in XFORM_TRANSLATE..XFORM_SCALE || s.pivot !in PIVOT_MEDIAN..PIVOT_CURSOR || !a.isFinite() || !b.isFinite()) return null
        return Command(CMD_TRANSFORM, floatArrayOf(type.toFloat(), a, b, s.pivot.toFloat(), s.cursorX, s.cursorY,
            if (s.proportional) 1f else 0f, if (s.connected) 1f else 0f, s.falloff.toFloat(), s.size, 0f))
    }
    fun framesSelectRange(fmin: Int, fmax: Int, extend: Boolean = false, allLayers: Boolean = false) =
        Command(CMD_FRAMES_SELECT_RANGE, floatArrayOf(minOf(fmin, fmax).toFloat(), maxOf(fmin, fmax).toFloat(), if (extend) 1f else 0f, if (allLayers) 1f else 0f))
    fun framesMove(offset: Int, allLayers: Boolean = false) = Command(CMD_FRAMES_MOVE, floatArrayOf(offset.toFloat(), if (allLayers) 1f else 0f))
    fun framesScale(center: Int, factor: Float, allLayers: Boolean = false): Command? =
        if (factor.isFinite() && factor > 0f) Command(CMD_FRAMES_SCALE, floatArrayOf(center.toFloat(), factor, if (allLayers) 1f else 0f)) else null
    fun framesCopy(allLayers: Boolean = false) = Command(CMD_FRAMES_COPY, floatArrayOf(if (allLayers) 1f else 0f))
    fun framesPaste(frame: Int) = Command(CMD_FRAMES_PASTE, floatArrayOf(frame.toFloat()))
    /** Multi-segment dash (DashGpencilModifierSegment list): pairs of dash/gap point counts. */
    fun dashSegments(offset: Int, segments: List<Pair<Int, Int>>): Command? {
        if (segments.isEmpty() || segments.size > 32 || segments.any { it.first < 1 || it.second < 0 }) return null
        val a = FloatArray(2 + 2 * segments.size)
        a[0] = offset.toFloat(); a[1] = segments.size.toFloat()
        segments.forEachIndexed { k, (d, g) -> a[2 + 2 * k] = d.toFloat(); a[3 + 2 * k] = g.toFloat() }
        return Command(CMD_DASH_SEGMENTS, a)
    }

    /** Blender bGPdata.onion_mode values (GP_ONION_MODE_*). */
    const val ONION_MODE_ABSOLUTE = 0
    const val ONION_MODE_RELATIVE = 1
    const val ONION_MODE_SELECTED = 2
    /** pg_gp_doc_query: indices of the active frame's selected strokes (PG_DOC_Q_SELECTED_STROKES). */
    const val DOC_Q_SELECTED_STROKES = 4

    fun outline(thickness: Int = 2, capSegments: Int = 8): Command? =
        if (thickness >= 1 && capSegments in 1..64) Command(CMD_OUTLINE, floatArrayOf(thickness.toFloat(), capSegments.toFloat())) else null
    /** Onion mode and ghost colours (ARGB) with their switches. */
    fun onionStyle(mode: Int, usePrevColor: Boolean, useNextColor: Boolean, prevArgb: Int, nextArgb: Int): Command? {
        if (mode !in ONION_MODE_ABSOLUTE..ONION_MODE_SELECTED) return null
        fun c(argb: Int, shift: Int) = ((argb ushr shift) and 0xFF) / 255f
        return Command(CMD_ONION_STYLE, floatArrayOf(mode.toFloat(), flag(usePrevColor), flag(useNextColor),
            c(prevArgb, 16), c(prevArgb, 8), c(prevArgb, 0), c(nextArgb, 16), c(nextArgb, 8), c(nextArgb, 0)))
    }
    /** Material texture settings: fill = false for the stroke texture. */
    fun materialTexture(material: Int, fill: Boolean, enabled: Boolean, mix: Float, scaleX: Float = 1f, scaleY: Float = 1f,
                        offsetX: Float = 0f, offsetY: Float = 0f, angle: Float = 0f, pixelSize: Float = 100f): Command? =
        if (material >= 0 && finite(mix, scaleX, scaleY, offsetX, offsetY) && finite(angle, pixelSize) && scaleX != 0f && scaleY != 0f)
            Command(CMD_MATERIAL_TEXTURE, floatArrayOf(material.toFloat(), flag(fill), flag(enabled), mix.coerceIn(0f, 1f),
                scaleX, scaleY, offsetX, offsetY, angle, pixelSize))
        else null
}
