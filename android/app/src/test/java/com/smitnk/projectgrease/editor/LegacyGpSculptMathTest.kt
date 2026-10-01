package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LegacyGpSculptMathTest {
    @Test
    fun influenceIsZeroOutsideBrushRadius() {
        assertEquals(
            0f,
            LegacyGpSculptMath.influence(1f, 1f, 21f, 20f, 1f),
            0.000001f
        )
    }

    @Test
    fun influenceHonorsStrengthPressureAndFalloff() {
        val value = LegacyGpSculptMath.influence(0.8f, 0.5f, 10f, 20f, 1f)
        assertEquals(0.2f, value, 0.000001f)
    }

    @Test
    fun pinchMovesPointTowardBrushCenter() {
        val point = LegacyGpSculptMath.pinch(10f, 0f, 0f, 0f, 0.5f, false)
        assertEquals(7.5f, point.first, 0.000001f)
        assertEquals(0f, point.second, 0.000001f)
    }

    @Test
    fun twistRotatesPointAroundBrushCenter() {
        val point = LegacyGpSculptMath.twist(1f, 0f, 0f, 0f, 90f, false)
        assertTrue(kotlin.math.abs(point.first) < 0.0001f)
        assertEquals(1f, point.second, 0.0001f)
    }

    @Test
    fun pushUsesRadialDirection() {
        val point = LegacyGpSculptMath.push(10f, 0f, 0f, 0f, 2f, 4f, 0.5f)
        assertEquals(11f, point.first, 0.000001f)
        assertEquals(2f, point.second, 0.000001f)
    }
}
