package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PhotoPixelsTest {
    private fun opaque(w: Int, h: Int) = IntArray(w * h) { 0xFF336699.toInt() }

    @Test fun eraserClearsTheCircleOnly() {
        val px = opaque(20, 20)
        assertTrue(PhotoPixels.eraseEllipse(px, 20, 20, 10f, 10f, 4f, 4f, soft = 0f))
        assertEquals(0, px[10 * 20 + 10] ushr 24)
        assertEquals(0xFF, px[0] ushr 24)
        assertEquals("outside the radius", 0xFF, px[10 * 20 + 15] ushr 24)
        assertFalse("nothing left to clear", PhotoPixels.eraseEllipse(px, 20, 20, 10f, 10f, 2f, 2f, soft = 0f))
    }

    @Test fun lassoCutsAPieceAndClearsIt() {
        val px = opaque(10, 10)
        val square = floatArrayOf(2f, 2f, 6f, 2f, 6f, 6f, 2f, 6f)
        val p = PhotoPixels.cut(px, 10, 10, square, clear = true)
        assertNotNull(p)
        assertEquals(4, p!!.w); assertEquals(4, p.h); assertEquals(2, p.x); assertEquals(2, p.y)
        assertEquals(0xFF336699.toInt(), p.px[0])
        assertEquals("cut out of the source", 0, px[3 * 10 + 3])
        assertEquals(0xFF336699.toInt(), px[0])
        assertNull("nothing left there", PhotoPixels.cut(px, 10, 10, square, clear = false))
    }
}
