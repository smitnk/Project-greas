package com.smitnk.projectgrease.editor

import kotlin.math.hypot

/**
 * Primitive ids passed to GPNative.nativeGenerateBlenderPrimitive.
 * Must match project_grease_blender_primitive.h. These are Project Grease ids;
 * the native side maps them to Blender 3.6.23's GP_STROKE_* values.
 */
object ProjectGreasePrimitive {
    const val BOX = 0
    const val LINE = 1
    const val POLYLINE = 2
    const val CIRCLE = 3
    const val ARC = 4
    const val CURVE = 5

    fun forTool(tool: GreaseTool): Int? = when (tool) {
        GreaseTool.RECTANGLE -> BOX
        GreaseTool.LINE -> LINE
        GreaseTool.POLYLINE -> POLYLINE
        GreaseTool.CIRCLE -> CIRCLE
        GreaseTool.ARC -> ARC
        GreaseTool.CURVE -> CURVE
        else -> null
    }

    /** gpencil_primitive_set_initdata(): box and circle strokes are cyclic. */
    fun isCyclic(type: Int): Boolean = type == BOX || type == CIRCLE
}

/**
 * Touch version of Blender's polyline primitive (gpencil_primitive_modal, IN_POLYLINE).
 * Blender adds a segment per click and confirms with Enter/right click. Here each
 * drag or tap adds a vertex, and tapping the last vertex again confirms.
 */
class PolylineSession(private val finishRadius: Float = 24f) {
    enum class Release { CONTINUE, FINISH }

    private val committed = ArrayList<Pair<Float, Float>>()
    private var floating: Pair<Float, Float>? = null
    private var gestureStart: Pair<Float, Float>? = null
    private var pressCreatedFirstVertex = false

    val vertices: List<Pair<Float, Float>> get() = committed.toList()
    val isActive: Boolean get() = committed.isNotEmpty()

    fun press(x: Float, y: Float) {
        gestureStart = x to y
        pressCreatedFirstVertex = committed.isEmpty()
        if (pressCreatedFirstVertex) committed += x to y
        floating = x to y
    }

    fun move(x: Float, y: Float) {
        if (gestureStart != null) floating = x to y
    }

    fun release(x: Float, y: Float): Release {
        val start = gestureStart ?: return Release.CONTINUE
        val point = x to y
        val isTap = distance(start, point) <= finishRadius
        val createdFirst = pressCreatedFirstVertex
        gestureStart = null
        floating = null
        pressCreatedFirstVertex = false

        if (createdFirst) {
            if (!isTap) committed += point
            return Release.CONTINUE
        }
        if (isTap && distance(committed.last(), point) <= finishRadius) {
            return if (committed.size >= 2) Release.FINISH else Release.CONTINUE
        }
        committed += point
        return Release.CONTINUE
    }

    /** Gesture interrupted (e.g. a second finger): drop only what this gesture added. */
    fun cancelGesture() {
        if (pressCreatedFirstVertex) committed.clear()
        gestureStart = null
        floating = null
        pressCreatedFirstVertex = false
    }

    fun previewVertices(): List<Pair<Float, Float>> {
        val end = floating ?: return committed.toList()
        if (committed.isNotEmpty() && committed.last() == end) return committed.toList()
        return committed + end
    }

    fun reset() {
        committed.clear()
        floating = null
        gestureStart = null
        pressCreatedFirstVertex = false
    }

    private fun distance(a: Pair<Float, Float>, b: Pair<Float, Float>): Float =
        hypot(a.first - b.first, a.second - b.second)
}

/**
 * Touch version of Blender's curve primitive (gpencil_primitive.c, GP_STROKE_CURVE). Blender drags
 * start -> end, then shows the ends and the two Bezier control points as handles that can be
 * dragged until the curve is confirmed. Here the first drag sets start -> end; the control points
 * start where gpencil_primitive_update_cps() puts them; dragging a handle moves it; a press away
 * from every handle confirms. anchors() is the input of nativeGenerateBlenderPrimitive(CURVE):
 * start, end while the line is dragged, then start, end, cp1, cp2.
 */
class CurveSession {
    enum class Phase { IDLE, DRAG_LINE, EDIT }
    enum class Press { LINE, HANDLE, CONFIRM }

    var phase = Phase.IDLE
        private set
    private var start = 0f to 0f
    private var end = 0f to 0f
    private var cp1 = 0f to 0f
    private var cp2 = 0f to 0f
    private var dragging = -1
    private var dragOrigin: Pair<Float, Float>? = null

    val isActive: Boolean get() = phase != Phase.IDLE

    fun press(x: Float, y: Float, hitRadius: Float): Press {
        if (phase == Phase.EDIT) {
            val handles = handles()
            var best = -1
            var bestDistance = hitRadius
            handles.forEachIndexed { i, h ->
                val d = hypot(h.first - x, h.second - y)
                if (d <= bestDistance) { best = i; bestDistance = d }
            }
            if (best < 0) return Press.CONFIRM
            dragging = best
            dragOrigin = handles[best]
            return Press.HANDLE
        }
        start = x to y
        end = x to y
        phase = Phase.DRAG_LINE
        return Press.LINE
    }

    fun move(x: Float, y: Float) {
        when (phase) {
            Phase.DRAG_LINE -> end = x to y
            Phase.EDIT -> setHandle(dragging, x to y)
            Phase.IDLE -> Unit
        }
    }

    fun release(x: Float, y: Float) {
        move(x, y)
        when (phase) {
            Phase.DRAG_LINE -> {
                // A tap without a drag is not a curve.
                if (start == end) { reset(); return }
                updateControlPoints()
                phase = Phase.EDIT
            }
            Phase.EDIT -> { dragging = -1; dragOrigin = null }
            Phase.IDLE -> Unit
        }
    }

    /** Gesture interrupted: drop a line being dragged, put a dragged handle back. */
    fun cancelGesture() {
        when (phase) {
            Phase.DRAG_LINE -> reset()
            Phase.EDIT -> { dragOrigin?.let { setHandle(dragging, it) }; dragging = -1; dragOrigin = null }
            Phase.IDLE -> Unit
        }
    }

    fun anchors(): List<Pair<Float, Float>> = when (phase) {
        Phase.IDLE -> emptyList()
        Phase.DRAG_LINE -> if (start == end) emptyList() else listOf(start, end)
        Phase.EDIT -> listOf(start, end, cp1, cp2)
    }

    /** start, end, cp1, cp2 while the handles are editable. */
    fun handles(): List<Pair<Float, Float>> = if (phase == Phase.EDIT) listOf(start, end, cp1, cp2) else emptyList()

    fun reset() {
        phase = Phase.IDLE
        dragging = -1
        dragOrigin = null
    }

    private fun setHandle(index: Int, p: Pair<Float, Float>) {
        when (index) {
            0 -> start = p
            1 -> end = p
            2 -> cp1 = p
            3 -> cp2 = p
        }
    }

    /** gpencil_primitive_update_cps(), GP_STROKE_CURVE branch. */
    private fun updateControlPoints() {
        val mid = (start.first + end.first) * 0.5f to (start.second + end.second) * 0.5f
        cp1 = interp(mid, start, 0.33f)
        cp2 = interp(mid, end, 0.33f)
    }

    private fun interp(a: Pair<Float, Float>, b: Pair<Float, Float>, t: Float) =
        a.first + (b.first - a.first) * t to a.second + (b.second - a.second) * t
}


/**
 * Edit phase of Line / Box / Circle / Arc (gpencil_primitive.c: after the drag the primitive stays
 * IN_PROGRESS with its handles until it is confirmed). Handles: start and end (Arc: also the
 * control point, which gpencil_primitive_update_cps puts at the bend); a press on a handle drags
 * it, a press elsewhere confirms. [edges] are the subdivisions (+ / -, 0 = Blender's default).
 * Extrude (Line only, E key) turns the line into a polyline and adds a new end point.
 */
class ShapeEditSession {
    var type = -1
        private set
    var edges = 0
    private val points = ArrayList<Pair<Float, Float>>()
    var dragging = -1
        private set
    private var dragOrigin: Pair<Float, Float>? = null
    private var extruded = false

    val isActive: Boolean get() = type >= 0

    fun start(type: Int, start: Pair<Float, Float>, end: Pair<Float, Float>): Boolean {
        if (type !in setOf(ProjectGreasePrimitive.LINE, ProjectGreasePrimitive.BOX, ProjectGreasePrimitive.CIRCLE, ProjectGreasePrimitive.ARC)) return false
        if (start == end) return false
        this.type = type
        edges = 0
        extruded = false
        points.clear(); points += start; points += end
        return true
    }

    /** The type the geometry is generated with: an extruded line is a polyline. */
    fun effectiveType(): Int = if (extruded) ProjectGreasePrimitive.POLYLINE else type

    fun anchors(): List<Pair<Float, Float>> = points.toList()
    fun handles(): List<Pair<Float, Float>> = if (isActive) points.toList() else emptyList()

    /** True when a handle was hit (it is dragged), false when the press confirms. */
    fun press(x: Float, y: Float, hitRadius: Float): Boolean {
        var best = -1; var bd = hitRadius
        points.forEachIndexed { i, p -> val d = hypot(p.first - x, p.second - y); if (d <= bd) { bd = d; best = i } }
        dragging = best
        dragOrigin = points.getOrNull(best)
        return best >= 0
    }
    fun move(x: Float, y: Float) { if (dragging in points.indices) points[dragging] = x to y }
    fun release() { dragging = -1; dragOrigin = null }
    fun cancelGesture() { dragOrigin?.let { if (dragging in points.indices) points[dragging] = it }; release() }

    fun extrude(): Boolean {
        if (!isActive || (type != ProjectGreasePrimitive.LINE)) return false
        val a = points[points.size - 2]; val b = points.last()
        extruded = true
        points += (b.first + (b.first - a.first) * 0.5f) to (b.second + (b.second - a.second) * 0.5f)
        return true
    }

    fun reset() { type = -1; points.clear(); dragging = -1; dragOrigin = null; edges = 0; extruded = false }
}
