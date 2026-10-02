package com.smitnk.projectgrease.editor

import com.smitnk.projectgrease.nativebridge.GPNative

/**
 * Camera of the 3D reference scene (Blender Camera parameters plus an orbit around a target),
 * packed for GPNative.nativeSceneLiteSetCamera. Defaults are a new Blender camera's.
 */
data class ReferenceCamera(
    val orthographic: Boolean = false,
    val lens: Float = 50f,
    val orthoScale: Float = 6f,
    val sensorX: Float = 36f,
    val sensorY: Float = 24f,
    val sensorFit: Int = 0, // AUTO / HOR / VERT
    val shiftX: Float = 0f,
    val shiftY: Float = 0f,
    val clipStart: Float = 0.1f,
    val clipEnd: Float = 100f,
    val target: FloatArray = floatArrayOf(0f, 0f, 0f),
    val yaw: Float = 0.8f,
    val pitch: Float = 0.45f,
    val distance: Float = 12f
) {
    fun params(width: Int, height: Int): FloatArray = floatArrayOf(
        if (orthographic) 1f else 0f, lens, orthoScale, sensorX, sensorY, sensorFit.toFloat(), shiftX, shiftY,
        clipStart, clipEnd, target[0], target[1], target[2], yaw, pitch, distance,
        width.coerceAtLeast(1).toFloat(), height.coerceAtLeast(1).toFloat()
    )

    override fun equals(other: Any?) = other is ReferenceCamera && params(1, 1).contentEquals(other.params(1, 1))
    override fun hashCode() = params(1, 1).contentHashCode()

    companion object {
        /** Line Art frame-buffer coordinates (-1..1, y up) to canvas coordinates (y down); the render size is the canvas. */
        fun fbToCanvas(fbX: Float, fbY: Float, canvasW: Int, canvasH: Int): Pair<Float, Float> =
            (fbX + 1f) * 0.5f * canvasW to (1f - fbY) * 0.5f * canvasH
    }
}

/**
 * The 3D reference scene for Line Art (SPEC_LINE_ART_ARCHITECTURE batch 1): OBJ meshes and a
 * camera in native Scene-lite, previewed as a wireframe of the mesh edges over the canvas. Line
 * Art itself (occlusion, edge types, chaining, strokes) comes in later batches. Not saved with
 * the project yet.
 */
class ReferenceScene {
    private var handle = 0L
    var camera = ReferenceCamera()
        private set
    var visible = true
        private set
    private var segments: FloatArray = FloatArray(0)

    private fun ensure(): Long {
        if (handle == 0L) handle = runCatching { GPNative.nativeSceneLiteCreate() }.getOrDefault(0L)
        return handle
    }

    /** objects, vertices, triangles, edges, loose edges */
    fun stats(): IntArray = (if (handle != 0L) GPNative.nativeSceneLiteStats(handle) else null) ?: IntArray(5)
    val isEmpty: Boolean get() = stats()[0] == 0

    /** Adds the objects of an OBJ file; returns how many. */
    fun importObj(bytes: ByteArray, canvasW: Int, canvasH: Int): Int {
        val h = ensure()
        if (h == 0L) return 0
        val added = GPNative.nativeSceneLiteLoadObj(h, bytes)
        if (added > 0) refresh(canvasW, canvasH)
        return added
    }

    fun clear() {
        if (handle != 0L) GPNative.nativeSceneLiteClear(handle)
        segments = FloatArray(0)
    }

    fun setVisible(value: Boolean) { visible = value }

    fun setCamera(value: ReferenceCamera, canvasW: Int, canvasH: Int) {
        camera = value
        refresh(canvasW, canvasH)
    }

    /** Re-projects the edges with the current camera; the render size follows the canvas. */
    fun refresh(canvasW: Int, canvasH: Int) {
        val h = ensure()
        if (h == 0L) return
        GPNative.nativeSceneLiteSetCamera(h, camera.params(canvasW, canvasH))
        segments = GPNative.nativeSceneLiteProjectEdges(h) ?: FloatArray(0)
    }

    /** Projected edges in canvas coordinates: x0, y0, x1, y1 per edge. */
    fun canvasSegments(canvasW: Int, canvasH: Int): FloatArray {
        val out = FloatArray(segments.size)
        var i = 0
        while (i + 3 < segments.size) {
            val a = ReferenceCamera.fbToCanvas(segments[i], segments[i + 1], canvasW, canvasH)
            val b = ReferenceCamera.fbToCanvas(segments[i + 2], segments[i + 3], canvasW, canvasH)
            out[i] = a.first; out[i + 1] = a.second; out[i + 2] = b.first; out[i + 3] = b.second
            i += 4
        }
        return out
    }

    fun release() {
        if (handle != 0L) GPNative.nativeSceneLiteFree(handle)
        handle = 0L
        segments = FloatArray(0)
    }
}
