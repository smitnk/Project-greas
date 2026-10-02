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
