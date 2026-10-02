package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ColorMathTest {
    private fun near(e: Float, a: Float) = assertEquals(e, a, 1e-3f)

    @Test
    fun knownHsvValuesMatchBlender() {
        val cases = listOf(
            floatArrayOf(1f, 0f, 0f) to floatArrayOf(0f, 1f, 1f),
            floatArrayOf(0f, 1f, 0f) to floatArrayOf(1f / 3f, 1f, 1f),
            floatArrayOf(0f, 0f, 1f) to floatArrayOf(2f / 3f, 1f, 1f),
            floatArrayOf(0.5f, 0.5f, 0.5f) to floatArrayOf(0f, 0f, 0.5f)
        )
        for ((rgb, hsv) in cases) {
            val got = ColorMath.rgbToHsv(rgb[0], rgb[1], rgb[2])
            near(hsv[0], got[0]); near(hsv[1], got[1]); near(hsv[2], got[2])
            val back = ColorMath.hsvToRgb(got[0], got[1], got[2])
            near(rgb[0], back[0]); near(rgb[1], back[1]); near(rgb[2], back[2])
        }
    }

    @Test
    fun argbRoundTripKeepsAlpha() {
        val argb = 0x80FF8000.toInt()
        assertEquals(argb, ColorMath.hsvaToArgb(ColorMath.argbToHsva(argb)))
        assertEquals(0xFF00FF00.toInt(), ColorMath.hsvaToArgb(ColorMath.Hsva(1f / 3f, 1f, 1f)))
        // hue wraps
        assertEquals(0xFFFF0000.toInt(), ColorMath.hsvaToArgb(ColorMath.Hsva(1f, 1f, 1f)))
    }

    @Test
    fun hexParsing() {
        assertEquals("#FF8000", ColorMath.toHex(0xFFFF8000.toInt()))
        assertEquals(0xFFFF8000.toInt(), ColorMath.parseHex("#ff8000"))
        assertEquals(0xFFFFAA00.toInt(), ColorMath.parseHex("FA0"))
        assertEquals(0x40123456, ColorMath.parseHex("123456", 0x40))
        assertNull(ColorMath.parseHex("#12345"))
        assertNull(ColorMath.parseHex("GG0000"))
    }
}
