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

    // eGP_Selectmode (segment mode is not ported yet)
    const val MODE_POINT = 0
    const val MODE_STROKE = 1

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
    fun isValidMode(mode: Int): Boolean = mode == MODE_POINT || mode == MODE_STROKE

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
        if (!isValidOp(op) || !isValidMode(mode) || points.size < 3) return null
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
        if (!isValidOp(op) || !isValidMode(mode) || !finite(x0, y0, x1, y1)) return null
        return Command(CMD_BOX, floatArrayOf(op.toFloat(), mode.toFloat(), x0, y0, x1, y1))
    }

    /** args: op, mode, x, y, radius, isFirst. */
    fun circle(op: Int, mode: Int, x: Float, y: Float, radius: Float, isFirst: Boolean): Command? {
        if (!isValidOp(op) || !isValidMode(mode) || !finite(x, y, radius) || radius < 0f) return null
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
}
