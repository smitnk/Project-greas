package com.smitnk.projectgrease.editor

import com.smitnk.projectgrease.nativebridge.GPNative
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sqrt

/**
 * Android-native adaptation of the Blender 3.6.23 Legacy GP sculpt point-processing core.
 *
 * The original desktop operator owns context/view/Brush/Scene/Depsgraph state.
 * This class keeps the brush math and point mutations on Android while the
 * bGPDstroke storage remains the authoritative data model in native Blender GP.
 */
class LegacyGpSculptEngine(
    private val handleProvider: () -> Long
) {
    enum class Tool {
        SMOOTH, THICKNESS, STRENGTH, GRAB, PUSH, PINCH, TWIST, RANDOMIZE
    }

    data class Settings(
        var radius: Float = 24f,
        var strength: Float = 1f,
        var pressure: Float = 1f,
        var pressureCurve: Float = 1f,
        var invert: Boolean = false,
        var smoothingIterations: Int = 2
    )

    private data class Point(
        val stroke: Int,
        val index: Int,
        var x: Float,
        var y: Float,
        var z: Float,
        var pressure: Float,
        var strength: Float,
        var time: Float,
        var baseX: Float = x,
        var baseY: Float = y,
        var basePressure: Float = pressure,
        var baseStrength: Float = strength
    )

    private var active = false
    private var activeTool = Tool.SMOOTH
    private var startX = 0f
    private var startY = 0f
    private var previousX = 0f
    private var previousY = 0f
    private val cached = ArrayList<Point>()
    private var randomState = 0x6D2B79F5

    fun begin(tool: Tool, x: Float, y: Float, settings: Settings): Boolean {
        val handle = handleProvider()
        if (handle == 0L) return false

        cached.clear()
        active = true
        activeTool = tool
        startX = x
        startY = y
        previousX = x
        previousY = y
        randomState = seed(x, y)

        val strokes = GPNative.nativeStrokeCount(handle)
        for (strokeIndex in 0 until strokes) {
            var pointIndex = 0
            while (true) {
                val p = GPNative.nativeGetPoint(handle, strokeIndex, pointIndex) ?: break
                val dx = p[0] - x
                val dy = p[1] - y
                if (hypot(dx, dy) <= settings.radius) {
                    cached += Point(
                        strokeIndex,
                        pointIndex,
                        p[0], p[1], p[2], p[3], p[4], p[5],
                        p[0], p[1], p[3], p[4]
                    )
                }
                pointIndex++
            }
        }
        return cached.isNotEmpty()
    }

    fun update(x: Float, y: Float, settings: Settings): Boolean {
        if (!active) {
            return begin(activeTool, x, y, settings)
        }

        val handle = handleProvider()
        if (handle == 0L) return false

        val dx = x - startX
        val dy = y - startY
        val stepX = x - previousX
        val stepY = y - previousY
        var changed = false

        when (activeTool) {
            Tool.SMOOTH -> changed = smooth(handle, x, y, settings)
            Tool.THICKNESS -> changed = scalar(handle, x, y, settings, true)
            Tool.STRENGTH -> changed = scalar(handle, x, y, settings, false)
            Tool.GRAB -> changed = grab(handle, dx, dy, settings)
            Tool.PUSH -> changed = push(handle, x, y, stepX, stepY, settings)
            Tool.PINCH -> changed = pinch(handle, x, y, settings)
            Tool.TWIST -> changed = twist(handle, x, y, settings)
            Tool.RANDOMIZE -> changed = randomize(handle, x, y, settings)
        }

        previousX = x
        previousY = y
        return changed
    }

    fun end() {
        active = false
        cached.clear()
    }

    fun cancel() = end()

    private fun influence(p: Point, x: Float, y: Float, settings: Settings): Float {
        val distance = hypot(p.x - x, p.y - y)
        return LegacyGpSculptMath.influence(
            settings.strength,
            settings.pressure,
            distance,
            settings.radius,
            settings.pressureCurve
        )
    }

    private fun smooth(handle: Long, x: Float, y: Float, settings: Settings): Boolean {
        var changed = false
        val byStroke = cached.groupBy { it.stroke }
        for ((strokeIndex, selected) in byStroke) {
            if (selected.size < 1) continue
            val all = readStroke(handle, strokeIndex)
            if (all.isEmpty()) continue

            for (point in selected) {
                val inf = influence(point, x, y, settings)
                if (inf <= 0f) continue
                val i = point.index
                val from = max(0, i - 2)
                val to = min(all.lastIndex, i + 2)
                if (from == to) continue

                var sx = 0f
                var sy = 0f
                var ss = 0f
                var sp = 0f
                var count = 0
                for (j in from..to) {
                    if (j == i) continue
                    sx += all[j].x
                    sy += all[j].y
                    ss += all[j].strength
                    sp += all[j].pressure
                    count++
                }
                if (count == 0) continue

                val factor = (inf / settings.smoothingIterations.coerceAtLeast(1)).coerceIn(0f, 1f)
                point.x += (sx / count - point.x) * factor
                point.y += (sy / count - point.y) * factor
                point.strength += (ss / count - point.strength) * factor
                point.pressure += (sp / count - point.pressure) * factor
                changed = changed or write(handle, point)
            }
        }
        return changed
    }

    private fun scalar(
        handle: Long,
        x: Float,
        y: Float,
        settings: Settings,
        thickness: Boolean
    ): Boolean {
        var changed = false
        for (point in cached) {
            val inf = influence(point, x, y, settings)
            if (inf <= 0f) continue
            val signed = if (settings.invert) -inf else inf
            if (thickness) {
                point.pressure = max(0f, point.pressure + signed / 10f)
            } else {
                point.strength = (point.strength + signed * 0.125f).coerceIn(0f, 1f)
            }
            changed = changed or write(handle, point)
        }
        return changed
    }

    private fun grab(handle: Long, dx: Float, dy: Float, settings: Settings): Boolean {
        var changed = false
        for (point in cached) {
            val inf = influence(point, startX, startY, settings)
            if (inf <= 0f) continue
            point.x = point.baseX + dx * inf
            point.y = point.baseY + dy * inf
            changed = changed or write(handle, point)
        }
        return changed
    }

    private fun push(
        handle: Long,
        x: Float,
        y: Float,
        dx: Float,
        dy: Float,
        settings: Settings
    ): Boolean {
        var changed = false
        for (point in cached) {
            val inf = influence(point, x, y, settings)
            if (inf <= 0f) continue
            val (nx, ny) = LegacyGpSculptMath.push(
                point.x, point.y, dx, dy, inf
            )
            point.x = nx
            point.y = ny
            changed = changed or write(handle, point)
        }
        return changed
    }

    private fun pinch(handle: Long, x: Float, y: Float, settings: Settings): Boolean {
        var changed = false
        for (point in cached) {
            val inf = influence(point, x, y, settings)
            if (inf <= 0f) continue
            val (nx, ny) = LegacyGpSculptMath.pinch(
                point.x, point.y, x, y, inf, settings.invert
            )
            point.x = nx
            point.y = ny
            changed = changed or write(handle, point)
        }
        return changed
    }

    private fun twist(handle: Long, x: Float, y: Float, settings: Settings): Boolean {
        var changed = false
        for (point in cached) {
            val inf = influence(point, x, y, settings)
            if (inf <= 0f) continue
            val (nx, ny) = LegacyGpSculptMath.twist(
                point.x, point.y, x, y, inf, settings.invert
            )
            point.x = nx
            point.y = ny
            changed = changed or write(handle, point)
        }
        return changed
    }

    private fun randomize(handle: Long, x: Float, y: Float, settings: Settings): Boolean {
        var changed = false
        for (point in cached) {
            val inf = influence(point, x, y, settings)
            if (inf <= 0f) continue

            val jx = nextRandom() * inf
            val jy = nextRandom() * inf
            point.x += jx
            point.y += jy
            point.strength = (point.strength + nextRandom() * inf).coerceIn(0f, 1f)
            point.pressure = max(0f, point.pressure + nextRandom() * inf)
            changed = changed or write(handle, point)
        }
        return changed
    }

    private fun readStroke(handle: Long, strokeIndex: Int): List<Point> {
        val result = ArrayList<Point>()
        var i = 0
        while (true) {
            val p = GPNative.nativeGetPoint(handle, strokeIndex, i) ?: break
            result += Point(strokeIndex, i, p[0], p[1], p[2], p[3], p[4], p[5])
            i++
        }
        return result
    }

    private fun write(handle: Long, point: Point): Boolean {
        return GPNative.nativeSetPoint(
            handle,
            point.stroke,
            point.index,
            point.x,
            point.y,
            point.z,
            point.pressure,
            point.strength,
            point.time,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f
        )
    }

    private fun seed(x: Float, y: Float): Int =
        (x.toBits() * 73856093) xor (y.toBits() * 19349663)

    private fun nextRandom(): Float {
        randomState = randomState * 1664525 + 1013904223
        return ((randomState ushr 8) and 0x00FFFFFF) / 16777215f * 2f - 1f
    }

}
