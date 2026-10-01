package com.smitnk.projectgrease.editor

import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Android execution of the input-side Blender 3.6.23 Legacy GP drawing pipeline.
 *
 * The desktop operator owns bContext/RNA/view/depsgraph state which is deliberately
 * outside Project Grease. The source-derived point-processing semantics below are
 * kept aligned with gpencil_paint.c while the native Legacy GP sbuffer/stroke remains
 * authoritative.
 */
class LegacyGpBrushStrokeEngine {
    data class Settings(
        val drawStrength: Float = 1f,
        val usePressure: Boolean = true,
        val useStrengthPressure: Boolean = false,
        val pressureCurve: Float = 1f,
        val strengthCurve: Float = 1f,
        val activeSmooth: Float = 0f,
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
        val alphaMin: Float = 0.1f,
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
    private var lastInputX = 0f
    private var lastInputY = 0f
    private var initialTime = 0f
    private var previousPressure = 1f
    private var previousTime = 0f
    private var rng = 0x6D2B79F5

    fun begin(newSettings: Settings = Settings()) {
        settings = newSettings
        active = true
        buffer.clear()
        lastInputX = 0f
        lastInputY = 0f
        initialTime = 0f
        previousPressure = 1f
        previousTime = 0f
        rng = 0x6D2B79F5
    }

    fun add(event: InputEvent): List<StrokePoint> {
        if (!active) return emptyList()

        if (buffer.isEmpty()) {
            initialTime = event.timeSeconds
            lastInputX = event.x
            lastInputY = event.y
            previousPressure = event.pressure
                .coerceAtLeast(0f)
                .coerceAtMost(1f)
            previousTime = event.timeSeconds
            return listOf(
                makePoint(
                    event.x,
                    event.y,
                    event.pressure,
                    event.timeSeconds
                )
            )
        }

        var x = event.x
        var y = event.y

        /*
         * Exact gpencil_stroke_filtermval ordering:
         * 1) lazy-mode filter,
         * 2) Manhattan filter,
         * 3) Euclidean filter.
         *
         * The Android event coordinates are already float, but Blender converts
         * the deltas to int before testing thresholds.
         */
        if (!filterMval(x, y)) return emptyList()

        /*
         * Exact lazy-mouse interpolation from gpencil_draw_apply:
         * interp(current, current, previous, factor).
         */
        if (settings.lazyEnabled && !settings.disableStabilizer) {
            val factor = settings.smoothStrokeFactor
            x = x + (lastInputX - x) * factor
            y = y + (lastInputY - y) * factor
        }

        /*
         * gpencil_add_fake_points() is performed BEFORE gpencil_draw_apply_event().
         * It receives the original mouse coordinates; generated points are only
         * inserted when the exact distance/sample conditions are met.
         */
        val emitted = ArrayList<StrokePoint>()
        if (settings.synthesizeFastPoints) {
            emitted += addArcPointsIfNeeded(
                currentX = x,
                currentY = y,
                pressure = event.pressure,
                timeSeconds = event.timeSeconds
            )
        }

        emitted += makePoint(x, y, event.pressure, event.timeSeconds)

        /*
         * gpencil_draw_apply then stores the current point as the last mouse value.
         * Keep the accepted/interpolated point for subsequent lazy/angle calculations.
         */
        lastInputX = x
        lastInputY = y
        previousPressure = event.pressure
            .coerceAtLeast(0f)
            .coerceAtMost(1f)
        previousTime = event.timeSeconds

        /*
         * Blender smooths only after fake points were inserted, with five passes
         * of a fixed 0.15 influence. This mutates the buffered points, so return the
         * full affected suffix rather than introducing a different smoothing policy.
         */
        if (emitted.size > 1) {
            smoothGeneratedSuffix(0.15f, emitted.size)
        }

        return emittedFromBufferSuffix(emitted.size)
    }

    fun end(): List<StrokePoint> {
        if (!active) return emptyList()
        active = false
        return emptyList()
    }

    fun cancel() {
        active = false
        buffer.clear()
    }

    fun bufferedPoints(): List<StrokePoint> = buffer.toList()

    private fun filterMval(x: Float, y: Float): Boolean {
        val dx = abs(x - lastInputX).toInt()
        val dy = abs(y - lastInputY).toInt()

        if (settings.lazyEnabled && !settings.disableStabilizer) {
            if (dx * dx + dy * dy >
                settings.smoothStrokeRadius * settings.smoothStrokeRadius
            ) {
                return true
            }

            /*
             * Blender keeps the previous mval when inside the lazy radius.
             * Since the Kotlin event stream uses lastInputX/Y as the authoritative
             * previous mouse value, there is intentionally no position update here.
             */
            return false
        }

        if (dx > settings.manhattanThreshold &&
            dy > settings.manhattanThreshold
        ) {
            return true
        }

        return (dx * dx + dy * dy) >
            settings.euclideanThreshold.coerceAtLeast(0f).pow(2f)
    }

    private fun makePoint(
        x: Float,
        y: Float,
        inputPressure: Float,
        absoluteTime: Float
    ): StrokePoint {
        var pressure = if (settings.usePressure) {
            inputPressure
                .coerceIn(0f, 1f)
                .toDouble()
                .pow(settings.pressureCurve.coerceAtLeast(0.01f).toDouble())
                .toFloat()
        } else {
            1f
        }

        if (settings.drawAngleFactor != 0f && buffer.isNotEmpty()) {
            val previous = buffer.last()
            val vx = x - previous.x
            val vy = y - previous.y
            val len = hypot(vx, vy)
            if (len > 1e-6f) {
                val mx = vx / len
                val my = vy / len
                val v0x = cos(settings.drawAngle)
                val v0y = sin(settings.drawAngle)
                val factor = 1f - abs(v0x * mx + v0y * my)
                pressure = lerp(
                    pressure - settings.drawAngleFactor * factor,
                    previous.pressure,
                    0.3f
                ).coerceIn(settings.alphaMin, 1f)
            }
        }

        if (settings.jitter != 0f && buffer.isNotEmpty()) {
            val previous = buffer.last()
            val vx = x - previous.x
            val vy = y - previous.y
            val len = hypot(vx, vy)
            if (len > 1e-6f) {
                val axisX = 0f
                val axisY = 1f
                var mx = vx / len
                var my = vy / len
                val angle = atan2(my, mx) - atan2(axisY, axisX)
                mx *= cos(angle)
                my *= sin(angle)
                val jitterAmplitude = nextRandom() * settings.jitter * 10f
                val point = StrokePoint(
                    x + mx * jitterAmplitude,
                    y + my * jitterAmplitude,
                    pressure.coerceIn(settings.alphaMin, 1f),
                    strengthFor(pressure),
                    elapsedTime(absoluteTime)
                )
                append(point)
                return point
            }
        }

        val point = StrokePoint(
            x,
            y,
            pressure.coerceIn(settings.alphaMin, 1f),
            strengthFor(pressure),
            elapsedTime(absoluteTime)
        )
        append(point)
        return point
    }

    private fun strengthFor(pressure: Float): Float {
        if (!settings.useStrengthPressure) {
            return settings.drawStrength
        }
        val mapped = pressure.toDouble()
            .pow(settings.strengthCurve.coerceAtLeast(0.01f).toDouble())
            .toFloat()
        return (
            settings.drawStrength * mapped
        ).coerceIn(
            min(0.1f, settings.drawStrength),
            1f
        )
    }

    /**
     * Exact Blender gpencil_add_arc_points() port for the ordinary (non-guide)
     * fast-motion case. It uses the previous two buffered points to build the
     * control vector, rejects angles sharper than 120 degrees, then samples
     * a quarter-turn arc.
     */
    private fun addArcPointsIfNeeded(
        currentX: Float,
        currentY: Float,
        pressure: Float,
        timeSeconds: Float
    ): List<StrokePoint> {
        if (settings.lazyEnabled && !settings.disableStabilizer) return emptyList()

        val inputSamples = settings.inputSamples
        if (inputSamples == 0) return emptyList()

        val samples = 5 - inputSamples + 1
        val minDist = 4f * samples
        val dx = currentX - lastInputX
        val dy = currentY - lastInputY
        val dist = hypot(dx, dy)
        if (!(dist > 3f && dist > minDist)) return emptyList()

        val slices = (dist / minDist).toInt() + 1
        if (slices <= 1 || buffer.size < 2) return emptyList()

        val ptBefore = buffer[buffer.lastIndex]
        val ptPrev = buffer[buffer.lastIndex - 1]

        val vPrevX = ptPrev.x - ptBefore.x
        val vPrevY = ptPrev.y - ptBefore.y
        val vHalfX = (currentX - ptPrev.x) * 0.5f
        val vHalfY = (currentY - ptPrev.y) * 0.5f
        val dot = vPrevX * vHalfX + vPrevY * vHalfY
        val lenSq = vPrevX * vPrevX + vPrevY * vPrevY
        if (lenSq <= 0f) return emptyList()

        val angle = angleBetween(vPrevX, vPrevY, vHalfX, vHalfY)
        if (angle < Math.toRadians(120.0).toFloat()) return emptyList()

        val projectedPrevX = vPrevX * (dot / lenSq)
        val projectedPrevY = vPrevY * (dot / lenSq)

        val ctlX = ptPrev.x + projectedPrevX
        val ctlY = ptPrev.y + projectedPrevY

        val midpointX = (ptPrev.x + currentX) * 0.5f
        val midpointY = (ptPrev.y + currentY) * 0.5f
        val cornerX = midpointX - (ctlX - midpointX)
        val cornerY = midpointY - (ctlY - midpointY)

        val step = (Math.PI / 2.0).toFloat() / (slices + 1)
        var a = step
        val out = ArrayList<StrokePoint>(slices - 1)

        var previousForAngle = ptPrev
        val stepColor = 1f / slices.coerceAtLeast(1)

        for (i in 0 until slices) {
            val x = cornerX +
                (currentX - cornerX) * sin(a) +
                (ptPrev.x - cornerX) * cos(a)
            val y = cornerY +
                (currentY - cornerY) * sin(a) +
                (ptPrev.y - cornerY) * cos(a)

            var interpolatedPressure = ptPrev.pressure
            if (settings.drawAngleFactor != 0f) {
                val vx = x - previousForAngle.x
                val vy = y - previousForAngle.y
                val len = hypot(vx, vy)
                if (len > 1e-6f) {
                    val mx = vx / len
                    val my = vy / len
                    val v0x = cos(settings.drawAngle)
                    val v0y = sin(settings.drawAngle)
                    val fac = 1f - abs(v0x * mx + v0y * my)
                    interpolatedPressure = lerp(
                        interpolatedPressure - settings.drawAngleFactor * fac,
                        previousForAngle.pressure,
                        0.3f
                    )
                        .coerceIn(ptPrev.pressure * 0.5f, 1f)
                    previousForAngle = StrokePoint(
                        x, y, interpolatedPressure, ptPrev.strength, 0f
                    )
                }
            }

            /*
             * The desktop code copies the previous pressure/strength; time is an
             * input-bridge addition because the Android event stream needs monotonic
             * point times for the existing JNI ABI.
             */
            val t = (i + 1).toFloat() / slices.toFloat()
            val time = previousTime + (timeSeconds - previousTime) * t
            val point = StrokePoint(
                x,
                y,
                interpolatedPressure.coerceIn(settings.alphaMin, 1f),
                ptPrev.strength,
                (time - initialTime).coerceAtLeast(0f)
            )
            append(point)
            out += point
            a += step
        }

        return out
    }

    private fun smoothGeneratedSuffix(influence: Float, count: Int) {
        val end = buffer.lastIndex
        val start = (end - count + 1).coerceAtLeast(0)
        if (end - start < 3) return

        repeat(5) {
            smoothSegment(influence, start, end)
        }
    }

    /**
     * Exact gpencil_smooth_segment() indexing and averaging.
     */
    private fun smoothSegment(influence: Float, fromIndex: Int, toIndex: Int) {
        if (toIndex - fromIndex < 3 || influence == 0f || fromIndex <= 2) return

        val average = 0.25f
        for (i in fromIndex..toIndex) {
            val a = if (i >= 3) buffer[i - 3] else null
            val b = if (i >= 2) buffer[i - 2] else null
            val c = if (i >= 1) buffer[i - 1] else buffer[i]
            val d = buffer[i]

            var sx = 0f
            var sy = 0f
            var sp = 0f
            var ss = 0f

            fun add(p: StrokePoint) {
                sx += p.x * average
                sy += p.y * average
                sp += p.pressure * average
                ss += p.strength * average
            }

            add(a ?: c)
            add(b ?: c)
            add(c)
            add(d)

            val targetIndex = (i - 1).coerceIn(0, buffer.lastIndex)
            val current = buffer[targetIndex]
            buffer[targetIndex] = current.copy(
                x = lerp(current.x, sx, influence),
                y = lerp(current.y, sy, influence),
                pressure = lerp(current.pressure, sp, influence),
                strength = lerp(current.strength, ss, influence)
            )
        }
    }

    private fun append(point: StrokePoint) {
        buffer += point
    }

    private fun emittedFromBufferSuffix(count: Int): List<StrokePoint> {
        if (count <= 0) return emptyList()
        val start = (buffer.size - count).coerceAtLeast(0)
        return buffer.subList(start, buffer.size).toList()
    }

    private fun elapsedTime(absoluteTime: Float): Float =
        (absoluteTime - initialTime).coerceAtLeast(0f)

    private fun lerp(a: Float, b: Float, factor: Float): Float =
        a + (b - a) * factor

    private fun angleBetween(ax: Float, ay: Float, bx: Float, by: Float): Float {
        val al = hypot(ax, ay)
        val bl = hypot(bx, by)
        if (al <= 1e-6f || bl <= 1e-6f) return 0f
        val cosine = ((ax * bx + ay * by) / (al * bl)).coerceIn(-1f, 1f)
        return kotlin.math.acos(cosine)
    }

    private fun nextRandom(): Float {
        rng = rng * 1664525 + 1013904223
        return ((rng ushr 8) and 0x00FFFFFF) / 16777215f * 2f - 1f
    }
}
