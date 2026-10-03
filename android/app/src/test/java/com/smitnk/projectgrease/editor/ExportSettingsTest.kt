package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream

class ExportSettingsTest {
    @Test
    fun gifHasHeaderFramesAndTrailer() {
        val out = ByteArrayOutputStream()
        val g = GifEncoder(out, 4, 3, 12)
        g.begin(); g.addFrame(IntArray(12) { 0xFFFF0000.toInt() }); g.addFrame(IntArray(12) { 0 }); g.finish()
        val b = out.toByteArray()
        assertEquals("GIF89a", String(b, 0, 6, Charsets.US_ASCII))
        assertEquals(0x3B, b.last().toInt() and 0xFF)
        assertEquals(2, b.count { it.toInt() and 0xFF == 0x2C }.coerceAtMost(2))
        assertEquals(GifEncoder.TRANSPARENT_INDEX, GifEncoder.index(0x00FFFFFF))
    }

    @Test
    fun sequencePlanHonoursHolds() {
        val p = FrameSequenceExport.plan(intArrayOf(1, 5), 1, 6, "shot", "png")
        assertEquals(6, p.size); assertEquals(1, p[3].shownKeyframe); assertEquals(5, p[4].shownKeyframe)
        assertEquals("shot_0001.png", p[0].fileName)
    }

    @Test
    fun settingsRoundTrip() {
        val s = ProjectSettings().apply { width = 1280; fps = 12; background = 0xFF112233.toInt(); transparentBackground = true }
        val back = ProjectSettings.fromJson(s.toJson())
        assertEquals(1280, back.width); assertEquals(12, back.fps); assertTrue(back.transparentBackground)
        assertEquals(24, ProjectSettings.fromJson("{}").fps)
        s.fps = 0; assertNotNull(s.validate())
    }
}
