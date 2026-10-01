package com.smitnk.projectgrease.editor

import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.pow
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin

/**
 * Kotlin execution of the input-side Blender 3.6.23 Legacy GP drawing pipeline.
 *
 * Source-derived stages:
 * - gpencil_stroke_filtermval()
 * - lazy-mouse interpolation
 * - gpencil_add_fake_points()/gpencil_add_arc_points()
 * - gpencil_brush_jitter()
 * - gpencil_brush_angle()
 * - gpencil_smooth_buffer()
 * - normalized stroke time
 *
 * Desktop-only context/depsgraph/RNA/view-depth branches intentionally stay
 * outside this Android engine. Native Blender Legacy GP sbuffer/bGPDstroke
 * remains the authoritative storage and commit path.
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
        val emitted = ArrayList<StrokePoint>()

        if (buffer.isEmpty()) {
            initialTime = event.timeSeconds
            lastInputX = event.x
            lastInputY = event.y
            previousPressure = event.pressure.coerceIn(0f, 1f)
            previousTime = event.timeSeconds

            emitted += makePoint(
                event.x,
                event.y,
                event.pressure,
                event.timeSeconds
            )
            return emitted
        }

        var mx = event.x
        var my = event.y
        val accepted = filterMval(mx, my)
        if (!accepted) {
            return emptyList()
        }

        if (settings.lazyEnabled && !settings.disableStabilizer) {
            val f = settings.smoothStrokeFactor.coerceIn(0f, 1f)
            mx = mx + (lastInputX - mx) * f
            my = my + (lastInputY - my) * f
        }

        val before = buffer.size
        if (settings.synthesizeFastPoints) {
            emitted.addAll(addFakePoints(
                lastInputX,
                lastInputY,
                mx,
                my,
                event.pressure,
                event.timeSeconds
            ))
        }

        emitted += makePoint(mx, my, event.pressure, event.timeSeconds)

        lastInputX = event.x
        lastInputY = event.y
        previousPressure = event.pressure.coerceIn(0f, 1f)
        previousTime = event.timeSeconds

        if (buffer.size - before > 1) {
            smoothSegment(0.15f, before - 1, buffer.lastIndex)
        }

        return if (emitted.isEmpty()) emptyList() else {
            val start = max(0, before)
            buffer.drop(start)
        }
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
        if (buffer.isEmpty()) return true
        val dx = abs(x - lastInputX)
        val dy = abs(y - lastInputY)

        if (settings.lazyEnabled && !settings.disableStabilizer) {
            val radius = settings.smoothStrokeRadius.coerceAtLeast(0f)
            if (dx * dx + dy * dy > radius * radius) {
                return true
            }
            return false
        }

        if (dx > settings.manhattanThreshold && dy > settings.manhattanThreshold) {
            return true
        }
        return (dx * dx + dy * dy) >
            settings.euclideanThreshold.coerceAtLeast(0f).pow2()
    }

    private fun makePoint(
        x: Float,
        y: Float,
        inputPressure: Float,
        absoluteTime: Float
    ): StrokePoint {
        var p = if (settings.usePressure) {
            inputPressure.coerceIn(0f, 1f).toDouble()
                .pow(settings.pressureCurve.coerceAtLeast(0.01f).toDouble()).toFloat()
        } else {
            1f
        }

        if (settings.drawAngleFactor != 0f && buffer.isNotEmpty()) {
            val previous = buffer.last()
            val movement = atan2(y - previous.y, x - previous.x)
            val v0x = cos(settings.drawAngle)
            val v0y = sin(settings.drawAngle)
            val len = hypot(x - previous.x, y - previous.y)
            if (len > 1e-6f) {
                val mx = (x - previous.x) / len
                val my = (y - previous.y) / len
                val factor = 1f - abs(v0x * mx + v0y * my)
                p = ((p - settings.drawAngleFactor * factor) * 0.3f +
                    previous.pressure * 0.7f)
                    .coerceIn(settings.alphaMin, 1f)
            }
        }

        if (settings.jitter > 0f && buffer.isNotEmpty()) {
            val previous = buffer.last()
            val vx = x - previous.x
            val vy = y - previous.y
            val len = hypot(vx, vy)
            if (len > 1e-6f) {
                val nx = -vy / len
                val ny = vx / len
                val j = nextRandom() * settings.jitter * 10f
                return StrokePoint(
                    x + nx * j,
                    y + ny * j,
                    p.coerceIn(settings.alphaMin, 1f),
                    strengthFor(p),
                    (absoluteTime - initialTime).coerceAtLeast(0f)
                ).also { appendProcessed(it) }
            }
        }

        return StrokePoint(
            x,
            y,
            p.coerceIn(settings.alphaMin, 1f),
            strengthFor(p),
            (absoluteTime - initialTime).coerceAtLeast(0f)
        ).also {
            appendProcessed(it)
        }
    }

    private fun strengthFor(pressure: Float): Float {
        if (!settings.useStrengthPressure) return settings.drawStrength.coerceIn(0f, 1f)
        val mapped = pressure.toDouble()
            .pow(settings.strengthCurve.coerceAtLeast(0.01f).toDouble()).toFloat()
        return (settings.drawStrength * mapped).coerceIn(0f, 1f)
    }

    private fun addFakePoints(
        fromX: Float,
        fromY: Float,
        toX: Float,
        toY: Float,
        pressure: Float,
        absoluteTime: Float
    ) {
        val out = ArrayList<StrokePoint>()
        if (!settings.synthesizeFastPoints || settings.inputSamples == 0) return out

        val samples = (5 - settings.inputSamples).coerceAtLeast(1)
        val minDist = 4f * samples
        val dist = hypot(toX - fromX, toY - fromY)
        if (!(dist > 3f && dist > minDist)) return out

        val slices = (dist / minDist).toInt() + 1
        if (slices <= 1) return out

        val controlX = (fromX + toX) * 0.5f
        val controlY = (fromY + toY) * 0.5f
        val vx = controlX - fromX
        val vy = controlY - fromY
        val perpX = -vy
        val perpY = vx
        val scale = if (dist > 1e-6f) min(0.25f * dist, hypot(vx, vy) * 0.75f) / max(hypot(vx, vy), 1e-6f) else 0f
        val cpX = controlX + perpX * scale
        val cpY = controlY + perpY * scale

        for (i in 1 until slices) {
            val t = i.toFloat() / slices.toFloat()
            val u = 1f - t
            val x = u * u * fromX + 2f * u * t * cpX + t * t * toX
            val y = u * u * fromY + 2f * u * t * cpY + t * t * toY
            val time = previousTime + (absoluteTime - previousTime) * t
            val point = StrokePoint(
                x,
                y,
                previousPressure.coerceIn(settings.alphaMin, 1f),
                settings.drawStrength.coerceIn(0f, 1f),
                (time - initialTime).coerceAtLeast(0f)
            )
            appendProcessed(point)
            out += point
        }
        return out
    }

    private fun appendProcessed(point: StrokePoint) {
        buffer += point
        if (settings.activeSmooth <= 0f || buffer.size < 3) return
        repeat(settings.activeSmoothPasses.coerceIn(1, 3)) { pass ->
            smoothBuffer(settings.activeSmooth * ((3f - pass) / 3f), buffer.lastIndex)
        }
    }

    private fun smoothBuffer(inf: Float, idx: Int) {
        if (idx < 2 || buffer.size < 3 || inf == 0f) return
        val steps = if (idx < 3) 3f else 4f
        val cIndex = idx - 1
        val indexes = listOfNotNull(
            if (idx >= 3) idx - 3 else null,
            if (idx >= 2) idx - 2 else null,
            if (idx >= 2) idx - 1 else null,
            idx
        )
        val sx = indexes.sumOf { buffer[it].x.toDouble() }.toFloat() / steps
        val sy = indexes.sumOf { buffer[it].y.toDouble() }.toFloat() / steps
        val sp = indexes.sumOf { buffer[it].pressure.toDouble() }.toFloat() / steps
        val ss = indexes.sumOf { buffer[it].strength.toDouble() }.toFloat() / steps
        val current = buffer[cIndex]
        val f = inf.coerceIn(0f, 1f)
        buffer[cIndex] = current.copy(
            x = current.x + (sx - current.x) * f,
            y = current.y + (sy - current.y) * f,
            pressure = current.pressure + (sp - current.pressure) * f,
            strength = current.strength + (ss - current.strength) * f
        )
    }

    private fun smoothSegment(inf: Float, fromIndex: Int, toIndex: Int) {
        if (toIndex - fromIndex < 3 || inf == 0f || fromIndex <= 2) return
        for (passIndex in 0 until 5) {
            for (i in fromIndex..toIndex) {
                val a = if (i >= 3) buffer[i - 3] else buffer[max(0, i - 1)]
                val b = if (i >= 2) buffer[i - 2] else buffer[max(0, i - 1)]
                val c = if (i >= 1) buffer[i - 1] else buffer[i]
                val d = buffer[i]
                val sx = (a.x + b.x + c.x + d.x) * 0.25f
                val sy = (a.y + b.y + c.y + d.y) * 0.25f
                val sp = (a.pressure + b.pressure + c.pressure + d.pressure) * 0.25f
                val ss = (a.strength + b.strength + c.strength + d.strength) * 0.25f
                buffer[cIndexSafe(i)] = c.copy(
                    x = c.x + (sx - c.x) * inf,
                    y = c.y + (sy - c.y) * inf,
                    pressure = c.pressure + (sp - c.pressure) * inf,
                    strength = c.strength + (ss - c.strength) * inf
                )
            }
        }
    }

    private fun cIndexSafe(index: Int): Int = (index - 1).coerceIn(0, buffer.lastIndex)

    private fun nextRandom(): Float {
        rng = rng * 1664525 + 1013904223
        return ((rng ushr 8) and 0x00FFFFFF) / 16777215f * 2f - 1f
    }

    private fun Float.pow2(): Float = this * this
}
