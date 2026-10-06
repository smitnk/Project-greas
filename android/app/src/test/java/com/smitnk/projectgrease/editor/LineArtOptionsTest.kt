package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LineArtOptionsTest {
    @Test
    fun defaultsAreANewLineArtModifier() {
        val o = LineArtOptions()
        val i = o.ints()
        val f = o.floats()
        assertEquals(15, i.size)
        assertEquals(12, f.size)
        assertEquals(0x37, i[0])
        assertEquals((1 shl 2) or (1 shl 4) or (1 shl 8) or (1 shl 16) or (1 shl 18), i[1])
        assertEquals(0, i[2])
        assertEquals(LineArtOptions.SOURCE_SCENE, i[6])
        assertEquals(-1, i[14])
        assertEquals(Math.toRadians(140.0).toFloat(), f[0], 1e-6f)
        assertEquals(0.1f, f[1], 0f)
        assertEquals(0.001f, f[2], 0f)
        assertEquals(0.05f, f[5], 0f)
        assertEquals(200f, f[8], 0f)
    }

    @Test
    fun flagsTypesAndLevels() {
        var o = LineArtOptions().withType(LineArtOptions.EDGE_SHADOW, true).withFlag(LineArtOptions.CHAIN_LOOSE_EDGES, true)
        assertTrue(o.hasType(LineArtOptions.EDGE_SHADOW))
        assertTrue(o.hasFlag(LineArtOptions.CHAIN_LOOSE_EDGES))
        o = o.withType(LineArtOptions.EDGE_CONTOUR, false)
        assertFalse(o.hasType(LineArtOptions.EDGE_CONTOUR))
        val hidden = LineArtOptions().forLevels(1)
        assertTrue(hidden.useMultipleLevels)
        assertEquals(0, hidden.levelStart)
        assertEquals(1, hidden.levelEnd)
        assertEquals(LineArtOptions(levelStart = 2), LineArtOptions(levelStart = 2).forLevels(0))
        val packed = LineArtOptions(invertCollection = true, invertSilhouette = true, lightType = LineArtOptions.LIGHT_SUN).ints()
        assertEquals((1 shl 6) or (1 shl 7), packed[8])
        assertEquals(1, packed[14])
    }
}
