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
}
