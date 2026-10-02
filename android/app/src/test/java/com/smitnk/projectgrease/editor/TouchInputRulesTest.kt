package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Test

class TouchInputRulesTest {
    @Test
    fun fingerPressureIsIgnoredLikeABlenderMouse() {
        assertEquals(1f, TouchInputRules.pressureFor(false, 0.07f), 0f)
        assertEquals(1f, TouchInputRules.pressureFor(false, 0f), 0f)
    }

    @Test
    fun penPressureIsPassedThroughWithinZeroToOne() {
        assertEquals(0.4f, TouchInputRules.pressureFor(true, 0.4f), 0f)
        assertEquals(1f, TouchInputRules.pressureFor(true, 1.7f), 0f)
        assertEquals(0f, TouchInputRules.pressureFor(true, -0.2f), 0f)
    }

    @Test
    fun elapsedTimeStaysPreciseAfterLongUptime() {
        val sixDaysMs = 6L * 24 * 3600 * 1000
        assertEquals(0.016f, TouchInputRules.elapsedSeconds(sixDaysMs + 16, sixDaysMs), 1e-6f)
        assertEquals(0f, TouchInputRules.elapsedSeconds(sixDaysMs - 5, sixDaysMs), 0f)
    }
}
