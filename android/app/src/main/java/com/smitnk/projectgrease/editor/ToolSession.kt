package com.smitnk.projectgrease.editor

import kotlin.math.min

/**
 * Kotlin side of the native tool session (native/blender_gp/project_grease_tool_session.h).
 *
 * Kotlin only captures touch: every sample of an input batch (Android's historical samples plus
 * the current one) is mapped to canvas units and handed to native in ONE call per batch
 * (GPNative.nativeToolSamples). Native applies Draw, Sculpt, Vertex Paint or Weight Paint to the
 * Legacy GP data sample by sample, tags the cache and renders once.
 */
object ToolSession {
    const val PHASE_BEGIN = 0
    const val PHASE_MOVE = 1
    const val PHASE_END = 2
    const val PHASE_CANCEL = 3

    const val TOOL_DRAW = 0
    const val TOOL_SCULPT = 1
    const val TOOL_VERTEX_PAINT = 2
    const val TOOL_WEIGHT_PAINT = 3

    /** x, y (canvas units), pressure (0..1), time (seconds since the gesture started). */
    const val STRIDE = 4

    const val RESULT_OK = 1
    const val RESULT_CHANGED = 2
    const val RESULT_ENDED = 4

    // PG_TOOL_P_* (brush tools)
    const val P_BRUSH = 0
    const val P_RADIUS = 1
    const val P_STRENGTH = 2
    const val P_PX_PER_UNIT = 3
    const val P_INVERT = 4
    const val P_R = 5
    const val P_G = 6
    const val P_B = 7
    const val P_TARGET = 8
    const val P_WEIGHT = 9
    const val P_SEED = 10
    const val P_AUTOMASK = 11
    const val P_SELECT_MASK = 12
    const val P_CURVE_PRESET = 13
    const val P_ACTIVE_MATERIAL = 14
    const val P_COUNT = 15

    // Blender tool enums (DNA_brush_enums.h)
    const val GPSCULPT_SMOOTH = 0
    const val GPSCULPT_THICKNESS = 1
    const val GPSCULPT_STRENGTH = 2
    const val GPSCULPT_GRAB = 3
    const val GPSCULPT_PUSH = 4
    const val GPSCULPT_TWIST = 5
    const val GPSCULPT_PINCH = 6
    const val GPSCULPT_RANDOMIZE = 7
    const val GPSCULPT_CLONE = 8
    /** GP_SCULPT_SETT_FLAG_AUTOMASK_* (DNA_scene_types.h). */
    const val AUTOMASK_STROKE = 1 shl 4
    const val AUTOMASK_LAYER_STROKE = 1 shl 5
    const val AUTOMASK_MATERIAL_STROKE = 1 shl 6
    const val AUTOMASK_LAYER_ACTIVE = 1 shl 8
    const val AUTOMASK_MATERIAL_ACTIVE = 1 shl 9
    /** GP_SCULPT_MASK_SELECTMODE_POINT / STROKE / SEGMENT. */
    const val SELECT_MASK_POINT = 1
    const val SELECT_MASK_STROKE = 2
    const val SELECT_MASK_SEGMENT = 4
    /** eBrushCurvePreset (0 keeps the brush default, Smooth). */
    val CURVE_PRESETS = listOf(0 to "Default", 1 to "Smooth", 9 to "Smoother", 2 to "Sphere", 3 to "Root", 4 to "Sharp", 5 to "Linear", 6 to "Pow4", 7 to "Inverse square", 8 to "Constant")
    const val GPVERTEX_DRAW = 0
    const val GPVERTEX_BLUR = 1
    const val GPVERTEX_AVERAGE = 2
    const val GPVERTEX_SMEAR = 4
    const val GPVERTEX_REPLACE = 5
    const val GPVERTEX_TINT = 3
    const val GPWEIGHT_DRAW = 0
    const val GPWEIGHT_BLUR = 1
    const val GPWEIGHT_AVERAGE = 2
    const val GPWEIGHT_SMEAR = 3

    fun sculptTool(brush: SculptBrush): Int = when (brush) {
        SculptBrush.SMOOTH -> GPSCULPT_SMOOTH
        SculptBrush.THICKNESS -> GPSCULPT_THICKNESS
        SculptBrush.STRENGTH -> GPSCULPT_STRENGTH
        SculptBrush.GRAB -> GPSCULPT_GRAB
        SculptBrush.PUSH -> GPSCULPT_PUSH
        SculptBrush.PINCH -> GPSCULPT_PINCH
        SculptBrush.TWIST -> GPSCULPT_TWIST
        SculptBrush.RANDOMIZE -> GPSCULPT_RANDOMIZE
        SculptBrush.CLONE -> GPSCULPT_CLONE
    }

    /** ProjectGreaseSelect.VPAINT_* (UI order) to GPVERTEX_TOOL_*. */
    fun vertexTool(vpaint: Int): Int = when (vpaint) {
        ProjectGreaseSelect.VPAINT_BLUR -> GPVERTEX_BLUR
        ProjectGreaseSelect.VPAINT_AVERAGE -> GPVERTEX_AVERAGE
        ProjectGreaseSelect.VPAINT_SMEAR -> GPVERTEX_SMEAR
        ProjectGreaseSelect.VPAINT_REPLACE -> GPVERTEX_REPLACE
        ProjectGreaseSelect.VPAINT_TINT -> GPVERTEX_TINT
        else -> GPVERTEX_DRAW
    }

    fun brushParams(
        brush: Int, radius: Float, strength: Float, pxPerUnit: Float, invert: Boolean = false,
        r: Float = 0f, g: Float = 0f, b: Float = 0f, target: Int = 0, weight: Float = 1f, seed: Int = 0,
        automask: Int = 0, selectMask: Int = 0, curvePreset: Int = 0, activeMaterial: Int = 0
    ): FloatArray = FloatArray(P_COUNT).also {
        it[P_AUTOMASK] = automask.toFloat(); it[P_SELECT_MASK] = selectMask.toFloat()
        it[P_CURVE_PRESET] = curvePreset.toFloat(); it[P_ACTIVE_MATERIAL] = activeMaterial.toFloat()
        it[P_BRUSH] = brush.toFloat(); it[P_RADIUS] = radius; it[P_STRENGTH] = strength
        it[P_PX_PER_UNIT] = pxPerUnit; it[P_INVERT] = if (invert) 1f else 0f
        it[P_R] = r; it[P_G] = g; it[P_B] = b; it[P_TARGET] = target.toFloat(); it[P_WEIGHT] = weight
        it[P_SEED] = seed.toFloat()
    }

    /** PG_DRAW_P_*: material, thickness, then the gpencil_paint.c input settings, the drawing guide
     *  (GP_GUIDE_* + 1, 0 = off) and the brush CurveMapping points (empty = power curve). */
    data class DrawSettings(
        val material: Int, val thickness: Float, val strength: Float = 1f, val usePressure: Boolean = true,
        val useStrengthPressure: Boolean = false, val pressureCurve: Float = 1f, val strengthCurve: Float = 1f,
        val activeSmooth: Float = 0f, val inputSamples: Int = 0, val lazy: Boolean = false, val lazyRadius: Float = 0f,
        val lazyFactor: Float = 0f, val disableStabilizer: Boolean = false, val manhattan: Int = 1,
        val euclidean: Float = 1f, val jitter: Float = 0f, val angleFactor: Float = 0f, val angle: Float = 0f,
        val fakePoints: Boolean = true,
        val guideType: Int = -1, val guideX: Float = 0f, val guideY: Float = 0f, val guideAngle: Float = 0f,
        val guideSpacing: Float = 0f,
        val pressureCurvePoints: List<Pair<Float, Float>> = emptyList(),
        val strengthCurvePoints: List<Pair<Float, Float>> = emptyList()
    ) {
        fun toParams(): FloatArray {
            val out = FloatArray(DRAW_P_COUNT)
            floatArrayOf(
                material.toFloat(), thickness, strength, flag(usePressure), flag(useStrengthPressure), pressureCurve,
                strengthCurve, activeSmooth, inputSamples.toFloat(), flag(lazy), lazyRadius, lazyFactor,
                flag(disableStabilizer), manhattan.toFloat(), euclidean, jitter, angleFactor, angle, flag(fakePoints),
                (guideType + 1).toFloat(), guideX, guideY, guideAngle, guideSpacing
            ).copyInto(out)
            fun curve(at: Int, pts: List<Pair<Float, Float>>) {
                if (pts.size !in 2..CURVE_MAX_POINTS) return
                out[at] = pts.size.toFloat()
                pts.forEachIndexed { i, (x, y) -> out[at + 1 + 2 * i] = x; out[at + 2 + 2 * i] = y }
            }
            curve(DRAW_P_PRESSURE_CURVE_N, pressureCurvePoints)
            curve(DRAW_P_STRENGTH_CURVE_N, strengthCurvePoints)
            return out
        }
        private fun flag(v: Boolean) = if (v) 1f else 0f
    }
    /** PG_DRAW_P_PRESSURE_CURVE_N / PG_DRAW_P_STRENGTH_CURVE_N / PG_DRAW_P_COUNT (project_grease_tool_session.h). */
    const val DRAW_P_PRESSURE_CURVE_N = 24
    const val DRAW_P_STRENGTH_CURVE_N = 41
    const val DRAW_P_COUNT = 58
    const val CURVE_MAX_POINTS = 8

    /** samples (count * STRIDE floats) followed by the BEGIN parameters. */
    fun pack(samples: FloatArray, count: Int, params: FloatArray?): FloatArray {
        val n = count * STRIDE
        if (params == null || params.isEmpty()) return if (samples.size == n) samples else samples.copyOf(n)
        val out = FloatArray(n + params.size)
        System.arraycopy(samples, 0, out, 0, n)
        System.arraycopy(params, 0, out, n, params.size)
        return out
    }
}

/** One pointer of a touch event with its batched (historical) samples; index historySize = current. */
interface TouchHistory {
    val historySize: Int
    fun x(h: Int): Float
    fun y(h: Int): Float
    fun pressure(h: Int): Float
    fun timeMs(h: Int): Long
}

/** Packs every sample of a batch, historical ones first, in canvas units. */
object ToolSampleBatch {
    fun from(
        touch: TouchHistory, gestureStartMs: Long, isPen: Boolean, includeHistory: Boolean,
        toCanvas: (Float, Float) -> Pair<Float, Float>
    ): FloatArray {
        val first = if (includeHistory) 0 else touch.historySize
        val count = touch.historySize - first + 1
        val out = FloatArray(count * ToolSession.STRIDE)
        for (k in 0 until count) {
            val h = first + k
            val p = toCanvas(touch.x(h), touch.y(h))
            out[k * 4] = p.first
            out[k * 4 + 1] = p.second
            out[k * 4 + 2] = TouchInputRules.pressureFor(isPen, touch.pressure(h))
            out[k * 4 + 3] = TouchInputRules.elapsedSeconds(touch.timeMs(h), gestureStartMs)
        }
        return out
    }
}

/**
 * Viewport <-> canvas mapping of the presenter (android_gp_presentation.cpp update_canvas_map):
 * the canvas is fitted at 92% times the zoom, centered, plus the pan.
 */
object CanvasMapping {
    fun pixelsPerUnit(viewW: Float, viewH: Float, canvasW: Int, canvasH: Int, zoom: Float): Float =
        min(viewW / canvasW.coerceAtLeast(1), viewH / canvasH.coerceAtLeast(1)) * 0.92f * zoom

    fun toCanvas(
        rawX: Float, rawY: Float, viewW: Float, viewH: Float, canvasW: Int, canvasH: Int,
        zoom: Float, panX: Float, panY: Float, clamp: Boolean = true
    ): Pair<Float, Float> {
        val cw = canvasW.coerceAtLeast(1)
        val ch = canvasH.coerceAtLeast(1)
        val fit = pixelsPerUnit(viewW, viewH, cw, ch, zoom)
        val ox = (viewW - cw * fit) * 0.5f + panX
        val oy = (viewH - ch * fit) * 0.5f + panY
        val x = (rawX - ox) / fit
        val y = (rawY - oy) / fit
        return if (clamp) x.coerceIn(0f, cw.toFloat()) to y.coerceIn(0f, ch.toFloat()) else x to y
    }

    fun toView(
        canvasX: Float, canvasY: Float, viewW: Float, viewH: Float, canvasW: Int, canvasH: Int,
        zoom: Float, panX: Float, panY: Float
    ): Pair<Float, Float> {
        val cw = canvasW.coerceAtLeast(1)
        val ch = canvasH.coerceAtLeast(1)
        val fit = pixelsPerUnit(viewW, viewH, cw, ch, zoom)
        return (viewW - cw * fit) * 0.5f + panX + canvasX * fit to (viewH - ch * fit) * 0.5f + panY + canvasY * fit
    }
}
