package com.smitnk.projectgrease.editor

import kotlin.math.abs
import kotlin.math.acos
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin

/**
 * Android execution of the input-side Blender 3.6.23 Legacy GP drawing pipeline
 * (source/blender/editors/gpencil_legacy/gpencil_paint.c).
 *
 * Traced functions: gpencil_stroke_filtermval, gpencil_draw_apply (lazy mouse),
 * gpencil_stroke_addpoint, gpencil_brush_jitter, gpencil_brush_angle,
 * gpencil_brush_angle_segment, gpencil_smooth_buffer, gpencil_smooth_segment,
 * gpencil_add_fake_points, gpencil_add_arc_points and the trailing low-pressure
 * truncation of gpencil_stroke_newfrombuffer.
 *
 * Blender edits its stroke buffer in place: an arc REPLACES the last buffered point and the
 * smoothing passes rewrite up to the last four points. The native sbuffer on the other side of
 * JNI is append-only, so this engine keeps the newest [holdBack] points private until no later
 * event can change them and only then releases them. [end] flushes whatever is left. The stream
 * of released points therefore equals the final buffer exactly.
 *
 * Desktop-only state (bContext, RNA, view/depth, depsgraph, guides, curve-mapping widgets) is
 * intentionally outside this class. Known approximations: the pressure/strength curves are
 * power curves instead of Blender's CurveMapping; random jitter uses a local generator; arc
 * points get interpolated times (Blender leaves them unset).
 */
class LegacyGpBrushStrokeEngine {
    companion object {
        /** GP_MAX_INPUT_SAMPLES (DNA_gpencil_legacy_types.h). */
        const val MAX_INPUT_SAMPLES = 10

        /** GPENCIL_ALPHA_OPACITY_THRESH (BKE_gpencil_legacy.h). */
        const val ALPHA_OPACITY_THRESH = 0.001f

        /** GPENCIL_STRENGTH_MIN (DNA_gpencil_legacy_types.h). */
        const val STRENGTH_MIN = 0.003f

        /** Points that active smoothing / arc replacement can still modify. */
        const val SMOOTH_HOLD_BACK = 4

        /** BLI_math_base interpf(): the `target` is weighted by `fac`. */
        fun interpf(target: Float, origin: Float, fac: Float): Float =
            fac * target + (1f - fac) * origin
    }

    data class Settings(
        val drawStrength: Float = 1f,
        val usePressure: Boolean = true,
        val useStrengthPressure: Boolean = false,
        val pressureCurve: Float = 1f,
        val strengthCurve: Float = 1f,
        val activeSmooth: Float = 0f,
        /** Kept for source compatibility: Blender fixes the active-smooth passes at 3. */
        val activeSmoothPasses: Int = 3,
        val inputSamples: Int = 0,
        val lazyEnabled: Boolean = false,
        val smoothStrokeRadius: Float = 0f,
        val smoothStrokeFactor: Float = 0f,
        val disableStabilizer: Boolean = false,
        val manhattanThreshold: Int = 1,
        val euclideanThreshold: Float = 1f,
        val jitter: Float = 0f,
        val drawAngleFactor: Float = 0f,
        val drawAngle: Float = 0f,
        /** Blender only floors pressure on the draw-angle path, at GPENCIL_ALPHA_OPACITY_THRESH. */
        val alphaMin: Float = ALPHA_OPACITY_THRESH,
        val synthesizeFastPoints: Boolean = true
    )

    data class InputEvent(
        val x: Float,
        val y: Float,
        val pressure: Float,
        val timeSeconds: Float
    )

    data class StrokePoint(
        val x: Float,
        val y: Float,
        val pressure: Float,
        val strength: Float,
        val time: Float
    )

    private var settings = Settings()
    private var active = false
    private val buffer = ArrayList<StrokePoint>()
    private var released = 0
    private var lastInputX = 0f
    private var lastInputY = 0f
    private var initialTime = 0f
    private var previousTime = 0f
    private var rng = 0x6D2B79F5

    fun begin(newSettings: Settings = Settings()) {
        settings = newSettings
        active = true
        buffer.clear()
        released = 0
        lastInputX = 0f
        lastInputY = 0f
        initialTime = 0f
        previousTime = 0f
        rng = 0x6D2B79F5
    }

    /**
     * Feeds one input sample. Returns the points that are now final and must be appended to the
     * native stroke buffer, in order.
     */
    fun add(event: InputEvent): List<StrokePoint> {
        if (!active) return emptyList()

        if (buffer.isEmpty()) {
            initialTime = event.timeSeconds
            lastInputX = event.x
            lastInputY = event.y
            previousTime = event.timeSeconds
            appendPoint(event.x, event.y, event.pressure, event.timeSeconds, event.x, event.y)
            return release()
        }

        if (!filterMval(event.x, event.y)) return emptyList()

        val mouseX = event.x
        val mouseY = event.y
        val sizeBefore = buffer.size

        // gpencil_draw_modal(): fake points are added BEFORE gpencil_draw_apply_event().
        if (settings.synthesizeFastPoints) {
            addFakePoints(mouseX, mouseY, event.timeSeconds)
        }

        // gpencil_draw_apply(): lazy mouse interpolates the current and the last position.
        var x = mouseX
        var y = mouseY
        if (settings.lazyEnabled && !settings.disableStabilizer) {
            val factor = settings.smoothStrokeFactor
            x = mouseX + (lastInputX - mouseX) * factor
            y = mouseY + (lastInputY - mouseY) * factor
        }

        appendPoint(x, y, event.pressure, event.timeSeconds, mouseX, mouseY)

        lastInputX = x
        lastInputY = y
        previousTime = event.timeSeconds

        // gpencil_draw_modal(): smooth the segment when fake points were added (five passes of
        // a fixed 0.15 influence). Blender's indices are used unchanged.
        val sizeAfter = buffer.size
        if (sizeAfter - sizeBefore > 1) {
            repeat(5) { smoothSegment(0.15f, sizeBefore - 1, sizeAfter - 1) }
        }
        return release()
    }

    /**
     * Ends the stroke: applies Blender's trailing low-pressure truncation to the points that
     * have not been released yet and returns every remaining point.
     */
    fun end(): List<StrokePoint> {
        if (!active) return emptyList()
        active = false

        // gpencil_stroke_newfrombuffer(): "For very low pressure at the end, truncate stroke."
        // Blender sets sbuffer_used = last_i - 1 (keeping its off-by-one). Points that were
        // already released cannot be taken back.
        var lastIndex = buffer.size - 1
        var used = buffer.size
        while (lastIndex > 0) {
            if (buffer[lastIndex].pressure > ALPHA_OPACITY_THRESH) break
            used = max(lastIndex - 1, 1)
            lastIndex--
        }
        val finalSize = max(used, released)
        while (buffer.size > finalSize) buffer.removeAt(buffer.lastIndex)

        val out = ArrayList(buffer.subList(released, buffer.size))
        released = buffer.size
        return out
    }

    fun cancel() {
        active = false
        buffer.clear()
        released = 0
    }

    fun bufferedPoints(): List<StrokePoint> = buffer.toList()

    // ------------------------------------------------------------------------------------

    private fun holdBack(): Int = when {
        settings.inputSamples > 0 || settings.activeSmooth > 0f -> SMOOTH_HOLD_BACK
        settings.drawAngleFactor != 0f -> 1
        else -> 0
    }

    private fun release(): List<StrokePoint> {
        val end = max(released, buffer.size - holdBack())
        if (end <= released) return emptyList()
        val out = ArrayList(buffer.subList(released, end))
        released = end
        return out
    }

    /** gpencil_stroke_filtermval() with the lazy-mouse and Manhattan/Euclidean tests. */
    private fun filterMval(x: Float, y: Float): Boolean {
        val dx = abs(x - lastInputX).toInt()
        val dy = abs(y - lastInputY).toInt()

        if (settings.lazyEnabled && !settings.disableStabilizer) {
            return dx * dx + dy * dy > settings.smoothStrokeRadius * settings.smoothStrokeRadius
        }
        if (dx > settings.manhattanThreshold && dy > settings.manhattanThreshold) {
            return true
        }
        val euclid = settings.euclideanThreshold.coerceAtLeast(0f)
        return (dx * dx + dy * dy).toFloat() > euclid * euclid
    }

    /** gpencil_stroke_addpoint() for GP_PAINTMODE_DRAW. */
    private fun appendPoint(
        x: Float,
        y: Float,
        inputPressure: Float,
        absoluteTime: Float,
        mouseX: Float,
        mouseY: Float
    ) {
        val pressure01 = inputPressure.coerceIn(0f, 1f)

        var pressure = 1f
        if (settings.usePressure) {
            pressure *= pressure01.toDouble()
                .pow(settings.pressureCurve.coerceAtLeast(0.01f).toDouble())
                .toFloat()
        }

        var strength = settings.drawStrength
        if (settings.useStrengthPressure) {
            strength *= pressure01.toDouble()
                .pow(settings.strengthCurve.coerceAtLeast(0.01f).toDouble())
                .toFloat()
        }
        strength = strength.coerceIn(min(STRENGTH_MIN, settings.drawStrength), 1f)

        var px = x
        var py = y
        val used = buffer.size

        // gpencil_brush_jitter(): perpendicular displacement, needs two buffered points.
        if (settings.jitter > 0f && used > 1) {
            val previous = buffer.last()
            val vx = px - previous.x
            val vy = py - previous.y
            val length = hypot(vx, vy)
            if (length > 1e-6f) {
                val mx = vx / length
                val my = vy / length
                val angle = acos(my.coerceIn(-1f, 1f)) // angle_v2v2(mvec, {0, 1})
                val exponent = settings.jitter + 2f
                val amplitude = nextRandom() * exponent * exponent
                px += mx * cos(angle) * amplitude * 10f
                py += my * sin(angle) * amplitude * 10f
            }
        }

        // gpencil_brush_angle(): uses the raw mouse position, not the jittered one.
        if (settings.drawAngleFactor != 0f && used >= 1) {
            val sen = settings.drawAngleFactor
            val v0x = cos(settings.drawAngle)
            val v0y = sin(settings.drawAngle)
            val previousIndex = buffer.lastIndex
            var previous = buffer[previousIndex]
            val mvx = mouseX - previous.x
            val mvy = mouseY - previous.y
            val length = hypot(mvx, mvy)
            val nx = if (length > 1e-6f) mvx / length else 0f
            val ny = if (length > 1e-6f) mvy / length else 0f

            if (used == 1) {
                // "uses > 1.0f to get a smooth transition in first point"
                val fac = 1.4f - abs(v0x * nx + v0y * ny)
                previous = previous.copy(
                    pressure = (previous.pressure - sen * fac)
                        .coerceIn(ALPHA_OPACITY_THRESH, 1f)
                )
                buffer[previousIndex] = previous
            }
            val fac = 1f - abs(v0x * nx + v0y * ny)
            pressure = interpf(pressure - sen * fac, previous.pressure, 0.3f)
                .coerceIn(ALPHA_OPACITY_THRESH, 1f)
        }

        buffer += StrokePoint(px, py, pressure, strength, (absoluteTime - initialTime).coerceAtLeast(0f))

        // "Smooth while drawing previous points with a reduction factor for previous."
        if (settings.activeSmooth > 0f) {
            for (s in 0 until 3) {
                smoothBuffer(settings.activeSmooth * ((3f - s) / 3f), buffer.size - s)
            }
        }
    }

    /**
     * gpencil_smooth_buffer(): smooths point C using the points before it (A, B) and D.
     * Position blends toward the average by `inf`; pressure and strength use interpf(), which
     * weights the CURRENT value by `inf` (the opposite direction).
     */
    private fun smoothBuffer(inf: Float, idx: Int) {
        if (buffer.size < 3 || idx < 3 || inf == 0f) return

        val steps = if (idx < 4) 3f else 4f
        val averageFac = 1f / steps
        val pta = if (idx >= 4) buffer[idx - 4] else null
        val ptb = buffer[idx - 3]
        val ptc = buffer[idx - 2]
        val ptd = buffer[idx - 1]

        var sx = 0f
        var sy = 0f
        var pressure = 0f
        var strength = 0f
        for (p in arrayOf(pta, ptb, ptc, ptd)) {
            if (p == null) continue
            sx += p.x * averageFac
            sy += p.y * averageFac
            pressure += p.pressure * averageFac
            strength += p.strength * averageFac
        }

        buffer[idx - 2] = ptc.copy(
            x = ptc.x + (sx - ptc.x) * inf,
            y = ptc.y + (sy - ptc.y) * inf,
            pressure = interpf(ptc.pressure, pressure, inf),
            strength = interpf(ptc.strength, strength, inf)
        )
    }

    /** gpencil_smooth_segment(): exact indexing and averaging. */
    private fun smoothSegment(inf: Float, fromIndex: Int, toIndex: Int) {
        if (toIndex - fromIndex < 3 || inf == 0f) return
        if (fromIndex <= 2) return

        val averageFac = 0.25f
        for (i in fromIndex..toIndex) {
            val pta = if (i >= 3) buffer[i - 3] else null
            val ptb = if (i >= 2) buffer[i - 2] else null
            val ptc = if (i >= 1) buffer[i - 1] else buffer[i]
            val ptd = buffer[i]

            var sx = 0f
            var sy = 0f
            var pressure = 0f
            var strength = 0f
            for (p in arrayOf(pta ?: ptc, ptb ?: ptc, ptc, ptd)) {
                sx += p.x * averageFac
                sy += p.y * averageFac
                pressure += p.pressure * averageFac
                strength += p.strength * averageFac
            }

            val target = if (i >= 1) i - 1 else i
            buffer[target] = ptc.copy(
                x = ptc.x + (sx - ptc.x) * inf,
                y = ptc.y + (sy - ptc.y) * inf,
                pressure = interpf(ptc.pressure, pressure, inf),
                strength = interpf(ptc.strength, strength, inf)
            )
        }
    }

    /** gpencil_add_fake_points() (no guides). Uses the raw mouse positions. */
    private fun addFakePoints(mouseX: Float, mouseY: Float, timeSeconds: Float) {
        // Lazy mode does not use fake events.
        if (settings.lazyEnabled && !settings.disableStabilizer) return

        val inputSamples = settings.inputSamples.coerceIn(0, MAX_INPUT_SAMPLES)
        if (inputSamples == 0) return

        val samples = MAX_INPUT_SAMPLES - inputSamples + 1
        val minDist = 4f * samples
        val dist = hypot(mouseX - lastInputX, mouseY - lastInputY)

        if (dist > 3f && dist > minDist) {
            val slices = (dist / minDist).toInt() + 1
            addArcPoints(mouseX, mouseY, slices, timeSeconds)
        }
    }

    /**
     * gpencil_add_arc_points(): arc between the previous point and the mouse, using the
     * previous segment to place the vertex. The first arc point REPLACES the last buffered
     * point (`pt = &points[idx_prev + i - 1]`), so the buffer grows by segments - 1.
     */
    private fun addArcPoints(mouseX: Float, mouseY: Float, segments: Int, timeSeconds: Float) {
        // Needs three buffered points; fewer only receive randomness in Blender.
        if (buffer.size < 3) return

        val ptBefore = buffer[buffer.lastIndex] // current - 2 in Blender's comment
        val ptPrev = buffer[buffer.lastIndex - 1] // previous

        var vPrevX = ptPrev.x - ptBefore.x
        var vPrevY = ptPrev.y - ptBefore.y
        val vHalfX = (ptPrev.x + mouseX) * 0.5f - ptPrev.x
        val vHalfY = (ptPrev.y + mouseY) * 0.5f - ptPrev.y

        // If angle is too sharp undo all changes and return.
        val angle = angleBetween(vPrevX, vPrevY, vHalfX, vHalfY)
        if (angle < Math.toRadians(120.0).toFloat()) return

        // Project the half vector to the previous vector and calculate the mid projected point.
        val dot = vPrevX * vHalfX + vPrevY * vHalfY
        val lengthSq = vPrevX * vPrevX + vPrevY * vPrevY
        if (lengthSq > 0f) {
            vPrevX *= dot / lengthSq
            vPrevY *= dot / lengthSq
        }

        // Calc the position of the control point.
        val ctlX = ptPrev.x + vPrevX
        val ctlY = ptPrev.y + vPrevY

        val step = (Math.PI / 2.0).toFloat() / (segments + 1).toFloat()
        var a = step

        val midpointX = (ptPrev.x + mouseX) * 0.5f
        val midpointY = (ptPrev.y + mouseY) * 0.5f
        val cornerX = midpointX - (ctlX - midpointX)
        val cornerY = midpointY - (ctlY - midpointY)

        val arc = ArrayList<StrokePoint>(segments)
        var ptStep = ptPrev
        for (i in 0 until segments) {
            val x = cornerX + (mouseX - cornerX) * sin(a) + (ptPrev.x - cornerX) * cos(a)
            val y = cornerY + (mouseY - cornerY) * sin(a) + (ptPrev.y - cornerY) * cos(a)

            // "Set pressure and strength equals to previous. It will be smoothed later."
            var pressure = ptPrev.pressure

            // gpencil_brush_angle_segment(), slightly attenuated for interpolated points.
            if (settings.drawAngleFactor != 0f) {
                val sen = settings.drawAngleFactor
                val v0x = cos(settings.drawAngle)
                val v0y = sin(settings.drawAngle)
                val mvx = x - ptStep.x
                val mvy = y - ptStep.y
                val length = hypot(mvx, mvy)
                val nx = if (length > 1e-6f) mvx / length else 0f
                val ny = if (length > 1e-6f) mvy / length else 0f
                val fac = 1f - abs(v0x * nx + v0y * ny)
                pressure = interpf(pressure - sen * fac, ptStep.pressure, 0.3f)
                    .coerceIn(ALPHA_OPACITY_THRESH, 1f)
                pressure = pressure.coerceIn(ptPrev.pressure * 0.5f, 1f)
            }

            // Blender leaves arc point times unset; interpolate so times stay monotonic.
            val t = (i + 1).toFloat() / segments.toFloat()
            val time = previousTime + (timeSeconds - previousTime) * t
            val point = StrokePoint(
                x,
                y,
                pressure,
                ptPrev.strength,
                (time - initialTime).coerceAtLeast(0f)
            )
            arc += point
            if (settings.drawAngleFactor != 0f) {
                ptStep = point // "Use the previous interpolated point for next segment."
            }
            a += step
        }

        buffer.removeAt(buffer.lastIndex)
        buffer.addAll(arc)
    }

    private fun angleBetween(ax: Float, ay: Float, bx: Float, by: Float): Float {
        val al = hypot(ax, ay)
        val bl = hypot(bx, by)
        if (al <= 1e-6f || bl <= 1e-6f) return 0f
        val cosine = ((ax * bx + ay * by) / (al * bl)).coerceIn(-1f, 1f)
        return acos(cosine)
    }

    /** Returns a value in [-1, 1] (BLI_rng_get_float() * 2 - 1). */
    private fun nextRandom(): Float {
        rng = rng * 1664525 + 1013904223
        return ((rng ushr 8) and 0x00FFFFFF) / 16777215f * 2f - 1f
    }
}
