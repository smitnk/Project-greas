package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LegacyGpBrushStrokeMathTest {
    @Test
    fun influenceMatchesLegacyBrushAlphaPressureFalloff() {
        assertEquals(
            0.2f,
            LegacyGpBrushStrokeMath.influence(0.8f, 0.5f, 10f, 20f, 1f, 1f),
            0.000001f
        )
    }

    @Test
    fun pinchShrinksByInfluenceSquaredDividedByFive() {
        assertEquals(0.99f, LegacyGpBrushStrokeMath.pinchFactor(1f, 0.5f, false), 0.000001f)
        assertEquals(1.01f, LegacyGpBrushStrokeMath.pinchFactor(1f, 0.5f, true), 0.000001f)
    }

    @Test
    fun twistUsesOneDegreePerFullInfluence() {
        val point = LegacyGpBrushStrokeMath.rotate(1f, 0f, 0f, 0f, 90f, false)
        assertTrue(kotlin.math.abs(point.first) < 0.001f)
        assertEquals(1f, point.second, 0.001f)
    }
}
