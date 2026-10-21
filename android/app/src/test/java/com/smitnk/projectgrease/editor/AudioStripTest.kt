package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AudioStripTest {
    @Test fun timing() {
        val a = AudioStrip("content://a", "a").apply { start = 13; durationMs = 2000 }
        assertEquals(-1000L, a.offsetMs(1, 12)); assertEquals(500L, a.offsetMs(19, 12))
        assertFalse(a.playsAt(12, 12)); assertTrue(a.playsAt(13, 12)); assertFalse(a.playsAt(37, 12))
        a.muted = true; assertFalse(a.playsAt(14, 12))
    }
    @Test fun savedAndLoaded() {
        val a = AudioStrip("content://a", "song.mp3").apply { start = 5; volume = 0.5f }
        val back = AudioStrip.listFromJson(AudioStrip.listToJson(listOf(a)))
        assertEquals(1, back.size); assertEquals(5, back[0].start); assertEquals(0.5f, back[0].volume, 1e-6f)
        assertEquals("song.mp3", back[0].name)
        a.volume = Float.NaN; assertEquals(1f, a.volume, 0f)
    }
}
