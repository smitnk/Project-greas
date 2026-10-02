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

    /** This camera with its orbit yaw linearly interpolated from yawFrom (t = 0) to yawTo (t = 1). */
    fun orbitAt(yawFrom: Float, yawTo: Float, t: Float): ReferenceCamera =
        copy(yaw = yawFrom + (yawTo - yawFrom) * t.coerceIn(0f, 1f))

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
    /** Show Line Art's visible lines instead of the mesh wireframe. */
    var lineArtPreview = false
        private set
    private var segments: FloatArray = FloatArray(0)
    private var lineArt: FloatArray = FloatArray(0)

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
        lineArt = FloatArray(0)
    }

    fun setVisible(value: Boolean) { visible = value }

    fun setLineArtPreview(value: Boolean, canvasW: Int, canvasH: Int) {
        lineArtPreview = value
        refresh(canvasW, canvasH)
    }

    /** Line Art segments in the preview: total and visible (occlusion 0). */
    fun lineArtCounts(): Pair<Int, Int> {
        var visibleCount = 0
        var i = 0
        while (i + 5 < lineArt.size) { if (lineArt[i + 4] == 0f) visibleCount++; i += 6 }
        return lineArt.size / 6 to visibleCount
    }

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
        lineArt = if (lineArtPreview) GPNative.nativeSceneLiteLineArt(h, 0) ?: FloatArray(0) else FloatArray(0)
    }

    /**
     * What the overlay draws, in canvas coordinates (x0, y0, x1, y1 per line): Line Art's visible
     * lines when the preview is on, otherwise every mesh edge.
     */
    fun canvasSegments(canvasW: Int, canvasH: Int): FloatArray =
        if (lineArtPreview) visibleLineArt(lineArt).let { toCanvas(it, canvasW, canvasH) }
        else toCanvas(segments, canvasW, canvasH)

    companion object {
        /** x0, y0, x1, y1 of the occlusion-0 segments of a nativeSceneLiteLineArt result. */
        fun visibleLineArt(raw: FloatArray): FloatArray {
            val out = ArrayList<Float>()
            var i = 0
            while (i + 5 < raw.size) {
                if (raw[i + 4] == 0f) { out += raw[i]; out += raw[i + 1]; out += raw[i + 2]; out += raw[i + 3] }
                i += 6
            }
            return out.toFloatArray()
        }

        /** Parses nativeSceneLiteLineArtStrokes output into canvas-space polylines. */
        fun parseStrokes(raw: FloatArray, canvasW: Int, canvasH: Int): List<FloatArray> {
            if (raw.isEmpty()) return emptyList()
            val count = raw[0].toInt()
            val out = ArrayList<FloatArray>(count.coerceAtLeast(0))
            var k = 1
            repeat(count) {
                if (k + 3 > raw.size) return out
                val points = raw[k].toInt()
                k += 3
                if (points < 0 || k + points * 2 > raw.size) return out
                out += FloatArray(points * 2).also { xy ->
                    for (p in 0 until points) {
                        val c = ReferenceCamera.fbToCanvas(raw[k + p * 2], raw[k + p * 2 + 1], canvasW, canvasH)
                        xy[p * 2] = c.first
                        xy[p * 2 + 1] = c.second
                    }
                }
                k += points * 2
            }
            return out
        }

        fun toCanvas(fb: FloatArray, canvasW: Int, canvasH: Int): FloatArray {
            val out = FloatArray(fb.size)
            var i = 0
            while (i + 3 < fb.size) {
                val a = ReferenceCamera.fbToCanvas(fb[i], fb[i + 1], canvasW, canvasH)
                val b = ReferenceCamera.fbToCanvas(fb[i + 2], fb[i + 3], canvasW, canvasH)
                out[i] = a.first; out[i + 1] = a.second; out[i + 2] = b.first; out[i + 3] = b.second
                i += 4
            }
            return out
        }
    }

    /**
     * Line Art strokes (Blender's chains) in canvas coordinates, one FloatArray of x, y pairs per
     * stroke; levelEnd 1 also returns lines hidden behind one surface. Empty when nothing is in view.
     */
    fun lineArtStrokes(canvasW: Int, canvasH: Int, levelEnd: Int = 0): List<FloatArray> {
        val h = ensure()
        if (h == 0L) return emptyList()
        GPNative.nativeSceneLiteSetCamera(h, camera.params(canvasW, canvasH))
        return parseStrokes(GPNative.nativeSceneLiteLineArtStrokes(h, levelEnd) ?: return emptyList(), canvasW, canvasH)
    }

    fun release() {
        if (handle != 0L) GPNative.nativeSceneLiteFree(handle)
        handle = 0L
        segments = FloatArray(0)
    }
}
