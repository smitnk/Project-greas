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
 * The Line Art modifier's options (LineartGpencilModifierData / PGLineartSettings), packed for
 * GPNative.nativeSceneLiteLineArtStrokesEx. Defaults are a new Blender Line Art modifier's. Bit
 * values are Blender's: edge types LRT_EDGE_FLAG_*, calculation flags eLineartMainFlags, mask
 * switches eLineartGpencilMaskSwitches, shadow LRT_SHADOW_FILTER_*, silhouette LRT_SILHOUETTE_FILTER_*.
 */
data class LineArtOptions(
    val edgeTypes: Int = EDGE_INIT,
    val calculationFlags: Int = DEFAULT_CALCULATION_FLAGS,
    val creaseThreshold: Float = Math.toRadians(140.0).toFloat(),
    val useMultipleLevels: Boolean = false,
    val levelStart: Int = 0,
    val levelEnd: Int = 0,
    val sourceType: Int = SOURCE_SCENE,
    val sourceIndex: Int = -1,
    val invertCollection: Boolean = false,
    val maskSwitches: Int = 0,
    val materialMaskBits: Int = 0,
    val intersectionMask: Int = 0,
    val chainingImageThreshold: Float = 0.001f,
    val chainSmoothTolerance: Float = 0f,
    val angleSplittingThreshold: Float = 0f,
    val overscan: Float = 0.1f,
    val strokeDepthOffset: Float = 0.05f,
    /** -1 = no light_contour_object, else LIGHT_POINT / LIGHT_SUN. */
    val lightType: Int = -1,
    val lightYaw: Float = 0.4f,
    val lightPitch: Float = 1.2f,
    val lightDistance: Float = 12f,
    val shadowSelection: Int = 0,
    val silhouetteSelection: Int = 0,
    val invertSilhouette: Boolean = false,
    val shadowCameraNear: Float = 0.1f,
    val shadowCameraFar: Float = 200f,
    val shadowCameraSize: Float = 200f,
    val sourceVertexGroup: String = ""
) {
    fun hasFlag(flag: Int): Boolean = (calculationFlags and flag) != 0
    fun withFlag(flag: Int, on: Boolean): LineArtOptions =
        copy(calculationFlags = if (on) calculationFlags or flag else calculationFlags and flag.inv())
    fun hasType(type: Int): Boolean = (edgeTypes and type) != 0
    fun withType(type: Int, on: Boolean): LineArtOptions =
        copy(edgeTypes = if (on) edgeTypes or type else edgeTypes and type.inv())

    /** These options, also returning lines hidden up to hiddenUpTo (the "include hidden lines" switch). */
    fun forLevels(hiddenUpTo: Int): LineArtOptions =
        if (hiddenUpTo <= 0) this
        else copy(
            useMultipleLevels = true,
            levelStart = if (useMultipleLevels) levelStart else 0,
            levelEnd = maxOf(hiddenUpTo, if (useMultipleLevels) levelEnd else levelStart)
        )

    fun ints(): IntArray = intArrayOf(
        edgeTypes, calculationFlags, if (useMultipleLevels) 1 else 0, levelStart, levelEnd, EDGE_ALL,
        sourceType, sourceIndex,
        (if (invertCollection) INVERT_COLLECTION else 0) or (if (invertSilhouette) INVERT_SILHOUETTE else 0),
        maskSwitches, materialMaskBits, intersectionMask, shadowSelection, silhouetteSelection, lightType
    )

    fun floats(): FloatArray = floatArrayOf(
        creaseThreshold, overscan, chainingImageThreshold, chainSmoothTolerance, angleSplittingThreshold,
        strokeDepthOffset, shadowCameraNear, shadowCameraFar, shadowCameraSize, lightYaw, lightPitch, lightDistance
    )

    companion object {
        const val EDGE_MARK = 1 shl 0
        const val EDGE_CONTOUR = 1 shl 1
        const val EDGE_CREASE = 1 shl 2
        const val EDGE_MATERIAL = 1 shl 3
        const val EDGE_INTERSECTION = 1 shl 4
        const val EDGE_LOOSE = 1 shl 5
        const val EDGE_LIGHT_CONTOUR = 1 shl 6
        const val EDGE_SHADOW = 1 shl 8
        const val EDGE_INIT = 0x37
        const val EDGE_ALL = 0x1ff
        const val INTERSECTION_AS_CONTOUR = 1 shl 0
        const val INVERT_SOURCE_VGROUP = 1 shl 7
        const val CHAIN_LOOSE_EDGES = 1 shl 12
        const val CHAIN_GEOMETRY_SPACE = 1 shl 13
        const val USE_CREASE_ON_SMOOTH = 1 shl 15
        const val USE_BACK_FACE_CULLING = 1 shl 19
        const val USE_IMAGE_BOUNDARY_TRIMMING = 1 shl 20
        const val CHAIN_PRESERVE_DETAILS = 1 shl 22
        /** LRT_ALLOW_DUPLI_OBJECTS | LRT_ALLOW_CLIPPING_BOUNDARIES | LRT_GPENCIL_MATCH_OUTPUT_VGROUP |
         *  LRT_USE_CREASE_ON_SHARP_EDGES | LRT_FILTER_FACE_MARK_KEEP_CONTOUR */
        const val DEFAULT_CALCULATION_FLAGS = (1 shl 2) or (1 shl 4) or (1 shl 8) or (1 shl 16) or (1 shl 18)
        const val SOURCE_COLLECTION = 0
        const val SOURCE_OBJECT = 1
        const val SOURCE_SCENE = 2
        const val INVERT_COLLECTION = 1 shl 6
        const val INVERT_SILHOUETTE = 1 shl 7
        const val MATERIAL_MASK_ENABLE = 1 shl 0
        const val MATERIAL_MASK_MATCH = 1 shl 1
        const val INTERSECTION_MATCH = 1 shl 2
        const val LIGHT_POINT = 0
        const val LIGHT_SUN = 1
        /** Object line art usage (eObjectLineArt_Usage): label to value. */
        val OBJECT_USAGES: List<Pair<String, Int>> = listOf(
            "Inherit" to 0, "Include" to 1, "Occlusion only" to 2, "Exclude" to 4,
            "Intersection only" to 8, "No intersection" to 16, "Force intersection" to 32
        )
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
    /** Line Art modifier options used for generated / baked strokes. */
    var lineArtOptions = LineArtOptions()
    /** Per-object line art usage (eObjectLineArt_Usage), by object index; kept until clear(). */
    private val objectUsage = HashMap<Int, Int>()

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
        objectUsage.clear()
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
        val options = lineArtOptions.forLevels(levelEnd)
        val raw = GPNative.nativeSceneLiteLineArtStrokesEx(h, options.ints(), options.floats(),
            options.sourceVertexGroup.ifEmpty { null }) ?: return emptyList()
        return parseStrokes(raw, canvasW, canvasH)
    }

    /** Names of the reference's objects (kind 0), materials (1) or collections (2). */
    fun names(kind: Int): List<String> =
        (if (handle != 0L) GPNative.nativeSceneLiteNames(handle, kind) else null)?.toList() ?: emptyList()

    fun objectUsage(index: Int): Int = objectUsage[index] ?: 0

    /** Sets an object's line art usage (Object > Line Art > Usage in Blender). */
    fun setObjectUsage(index: Int, usage: Int): Boolean {
        if (handle == 0L) return false
        val ok = GPNative.nativeSceneLiteSetObjectLineArt(handle, index, usage, 0, Math.toRadians(140.0).toFloat(), 0, -1)
        if (ok) objectUsage[index] = usage
        return ok
    }

    /** Sets a material's line art mask bits (Material > Line Art > Material Mask) and occlusion. */
    fun setMaterialLineArt(index: Int, maskBits: Int, occlusion: Int = 1): Boolean =
        handle != 0L && GPNative.nativeSceneLiteSetMaterialLineArt(handle, index, if (maskBits != 0) 1 else 0, maskBits, occlusion, 0, false)

    fun release() {
        if (handle != 0L) GPNative.nativeSceneLiteFree(handle)
        handle = 0L
        segments = FloatArray(0)
    }
}
