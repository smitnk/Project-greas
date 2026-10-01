package com.smitnk.projectgrease.editor

import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

object LegacyGpSculptMath {
    fun influence(strength: Float, pressure: Float, distance: Float, radius: Float, curve: Float): Float {
        if (radius <= 0f || distance > radius) return 0f
        val linear = (1f - distance / radius).coerceIn(0f, 1f)
        return strength.coerceIn(0f, 1f) *
            pressure.coerceIn(0f, 1f) *
            kotlin.math.exp(kotlin.math.ln(linear.coerceAtLeast(0.000001f)) * curve.coerceIn(0.25f, 4f))
    }

    fun pinch(x: Float, y: Float, cx: Float, cy: Float, influence: Float, invert: Boolean): Pair<Float, Float> {
        val signed = if (invert) -influence else influence
        val factor = (1f - signed * signed).coerceAtLeast(0f)
        return cx + (x - cx) * factor to cy + (y - cy) * factor
    }

    fun twist(x: Float, y: Float, cx: Float, cy: Float, influence: Float, invert: Boolean): Pair<Float, Float> {
        val angle = (if (invert) -1f else 1f) * influence * Math.PI.toFloat() / 180f
        val dx = x - cx
        val dy = y - cy
        val c = cos(angle)
        val s = sin(angle)
        return cx + dx * c - dy * s to cy + dx * s + dy * c
    }

    fun push(
        x: Float,
        y: Float,
        cx: Float,
        cy: Float,
        deltaX: Float,
        deltaY: Float,
        influence: Float
    ): Pair<Float, Float> {
        val vx = x - cx
        val vy = y - cy
        val length = hypot(vx, vy).coerceAtLeast(0.001f)
        return x + vx / length * deltaX * influence to
            y + vy / length * deltaY * influence
    }
}
