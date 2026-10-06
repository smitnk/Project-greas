package com.smitnk.projectgrease.editor

import java.io.ByteArrayOutputStream
import java.util.Random
import kotlin.math.abs
import kotlin.math.max
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GifQuantizerTest {
    private fun err(a: Int, b: Int) = max(abs(((a shr 16) and 255) - ((b shr 16) and 255)),
        max(abs(((a shr 8) and 255) - ((b shr 8) and 255)), abs((a and 255) - (b and 255))))

    @Test
    fun exactColorsArePreservedUpTo255() {
        val px = IntArray(255 * 3) { (0xFF shl 24) or ((it % 255) * 65793 xor 0x123456) }
        val palette = GifQuantizer.palette(px, 255)
        assertEquals(255, palette.toSet().size)
        val idx = GifQuantizer.indexFrame(px, px.size, 1, palette, false, 255)
        for (i in px.indices) assertEquals(px[i] and 0xFFFFFF, palette[idx[i].toInt() and 0xFF])
        // dithering an exactly representable image changes nothing
        assertArrayEquals(idx, GifQuantizer.indexFrame(px, px.size, 1, palette, true, 255))
    }

    @Test
    fun transparentPixelsUseTheTransparentIndexAndAreNotInThePalette() {
        val px = intArrayOf(0x00FF0000, 0xFF00FF00.toInt(), 0x7F0000FF)
        val palette = GifQuantizer.palette(px, 255)
        assertArrayEquals(intArrayOf(0x00FF00), palette)
        val idx = GifQuantizer.indexFrame(px, 3, 1, palette, true, 255)
        assertEquals(255, idx[0].toInt() and 0xFF); assertEquals(0, idx[1].toInt()); assertEquals(255, idx[2].toInt() and 0xFF)
    }

    @Test
    fun medianCutErrorIsBoundedForManyColors() {
        val rnd = Random(42)
        val px = IntArray(64 * 64) { (0xFF shl 24) or rnd.nextInt(0x1000000) }
        val palette = GifQuantizer.palette(px, 255)
        assertEquals(255, palette.size)
        var worst = 0
        var sum = 0L
        for (c in px) {
            val e = err(c, palette[GifQuantizer.nearest(palette, (c shr 16) and 255, (c shr 8) and 255, c and 255)])
            worst = max(worst, e); sum += e
        }
        // uniform noise is the worst case for median cut: every color within 64 per channel, mean under 32
        assertTrue("worst $worst", worst <= 64)
        assertTrue("mean ${sum.toDouble() / px.size}", sum.toDouble() / px.size < 32.0)
        // clustered colors (the usual drawing) stay close
        val clustered = IntArray(4096) { (0xFF shl 24) or (listOf(0x102030, 0x80A0C0, 0xF0E0D0)[it % 3] + (it % 7)) }
        val cp = GifQuantizer.palette(clustered, 8)
        for (c in clustered) assertTrue(err(c, cp[GifQuantizer.nearest(cp, (c shr 16) and 255, (c shr 8) and 255, c and 255)]) <= 4)
    }

    @Test
    fun ditheringIsDeterministicAndDiffusesError() {
        val w = 256; val h = 16
        val px = IntArray(w * h) { i -> val x = i % w; val y = i / w; (0xFF shl 24) or (x shl 16) or ((y * 16) shl 8) or (255 - x) }
        val palette = GifQuantizer.palette(px, 255)
        val a = GifQuantizer.indexFrame(px, w, h, palette, true, 255)
        val b = GifQuantizer.indexFrame(px, w, h, palette, true, 255)
        assertArrayEquals(a, b)
        assertFalse(a.contentEquals(GifQuantizer.indexFrame(px, w, h, palette, false, 255)))
        fun encode(dither: Boolean) = ByteArrayOutputStream().also { o ->
            GifEncoder(o, w, h, 12, dither).apply { begin(); addFrame(px); addFrame(px); finish() }
        }.toByteArray()
        assertArrayEquals(encode(true), encode(true))
        assertEquals("GIF89a", String(encode(false), 0, 6, Charsets.US_ASCII))
    }
}
