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
    fun pinchUsesLegacyOneFifthInfluenceAndSquaredFalloff() {
        val point = LegacyGpSculptMath.pinch(10f, 0f, 0f, 0f, 0.5f, false)
        assertEquals(9.9f, point.first, 0.000001f)
        assertEquals(0f, point.second, 0.000001f)
    }

    @Test
    fun pinchInvertExpandsAwayFromBrushCenter() {
        val point = LegacyGpSculptMath.pinch(10f, 0f, 0f, 0f, 0.5f, true)
        assertEquals(10.1f, point.first, 0.000001f)
        assertEquals(0f, point.second, 0.000001f)
    }

    @Test
    fun twistUsesOneDegreePerUnitInfluence() {
        val point = LegacyGpSculptMath.twist(1f, 0f, 0f, 0f, 1f, false)
        assertEquals(kotlin.math.cos(Math.toRadians(1.0)), point.first.toDouble(), 0.0001)
        assertEquals(kotlin.math.sin(Math.toRadians(1.0)), point.second.toDouble(), 0.0001)
    }

    @Test
    fun pushUsesBrushInfluence() {
        val point = LegacyGpSculptMath.push(10f, 0f, 2f, 4f, 0.5f)
        assertEquals(11f, point.first, 0.000001f)
        assertEquals(2f, point.second, 0.000001f)
    }
}
