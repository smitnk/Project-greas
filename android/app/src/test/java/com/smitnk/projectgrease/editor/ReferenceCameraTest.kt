package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Test

class ReferenceCameraTest {
    @Test
    fun packsParametersInTheNativeOrder() {
        val p = ReferenceCamera(orthographic = true, lens = 35f, shiftX = 0.1f, distance = 7f).params(1920, 1080)
        assertEquals(18, p.size)
        assertEquals(1f, p[0], 0f); assertEquals(35f, p[1], 0f); assertEquals(6f, p[2], 0f)
        assertEquals(0.1f, p[6], 0f); assertEquals(0.1f, p[8], 0f); assertEquals(100f, p[9], 0f)
        assertEquals(7f, p[15], 0f); assertEquals(1920f, p[16], 0f); assertEquals(1080f, p[17], 0f)
        assertEquals(1f, ReferenceCamera().params(0, -5)[17], 0f)
    }

    @Test
    fun frameBufferToCanvas() {
        val c = ReferenceCamera.fbToCanvas(-1f, 1f, 1920, 1080)
        assertEquals(0f, c.first, 0f); assertEquals(0f, c.second, 0f)
        val d = ReferenceCamera.fbToCanvas(0f, -1f, 1920, 1080)
        assertEquals(960f, d.first, 0f); assertEquals(1080f, d.second, 0f)
    }

    @Test
    fun lineArtPreviewKeepsVisibleSegmentsOnly() {
        val raw = floatArrayOf(-1f, 1f, 0f, -1f, 0f, 2f, 0f, 0f, 1f, 1f, 1f, 4f)
        val visible = ReferenceScene.visibleLineArt(raw)
        assertEquals(4, visible.size)
        val c = ReferenceScene.toCanvas(visible, 200, 100)
        assertEquals(0f, c[0], 0f); assertEquals(0f, c[1], 0f); assertEquals(100f, c[2], 0f); assertEquals(100f, c[3], 0f)
    }

    @Test
    fun lineArtStrokesParse() {
        // two strokes: 3 points (contour, level 0) and 2 points (crease, level 1)
        val raw = floatArrayOf(2f, 3f, 2f, 0f, -1f, 1f, 0f, 0f, 1f, -1f, 2f, 4f, 1f, 0f, 1f, 1f, 1f)
        val s = ReferenceScene.parseStrokes(raw, 200, 100)
        assertEquals(2, s.size)
        assertEquals(6, s[0].size); assertEquals(4, s[1].size)
        assertEquals(0f, s[0][0], 0f); assertEquals(0f, s[0][1], 0f)
        assertEquals(200f, s[0][4], 0f); assertEquals(100f, s[0][5], 0f)
        assertEquals(0, ReferenceScene.parseStrokes(floatArrayOf(5f, 9f), 10, 10).size)
    }
}
