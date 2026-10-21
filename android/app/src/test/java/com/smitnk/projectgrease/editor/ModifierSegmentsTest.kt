package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Test

class ModifierSegmentsTest {
    private fun shownIndices(type: Int, p: FloatArray) =
        ModifierSpecs.specs(type).filter { ModifierSpecs.shown(type, p, it) }.map { it.index }.toSet()

    @Test fun chainSegmentsOnlyInChainMode() {
        val p = FloatArray(120); p[7] = 2f
        assertEquals((0..6).toSet(), shownIndices(ModifierType.TIME, p))
        p[0] = 4f // chain: the count and two segments
        assertEquals((0..15).toSet(), shownIndices(ModifierType.TIME, p))
    }
    @Test fun dashSegmentsUpToCount() {
        val p = FloatArray(120); p[7] = 1f
        assertEquals((0..7).toSet(), shownIndices(ModifierType.DASH, p))
        p[7] = 3f
        assertEquals((0..19).toSet(), shownIndices(ModifierType.DASH, p))
    }
}
