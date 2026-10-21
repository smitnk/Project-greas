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
    @Test fun trimmedStrip() {
        val a = AudioStrip("content://a", "a").apply { start = 1; durationMs = 10_000 }
        a.trim(2_000, 4_000)
        assertEquals(2_000L, a.lengthMs); assertEquals(24, a.frames(12)); assertEquals(24, a.endFrame(12))
        assertEquals(2_000L, a.fileMs(1, 12)); assertEquals(3_000L, a.fileMs(13, 12))
        assertTrue(a.playsAt(24, 12)); assertFalse(a.playsAt(25, 12))
        val back = AudioStrip.listFromJson(AudioStrip.listToJson(listOf(a)))[0]
        assertEquals(2_000L, back.trimStartMs); assertEquals(4_000L, back.trimEndMs)
        a.trim(9_000, 50_000); assertEquals(10_000L, a.trimEndMs)
    }
    @Test fun savedAndLoaded() {
        val a = AudioStrip("content://a", "song.mp3").apply { start = 5; volume = 0.5f }
        val back = AudioStrip.listFromJson(AudioStrip.listToJson(listOf(a)))
        assertEquals(1, back.size); assertEquals(5, back[0].start); assertEquals(0.5f, back[0].volume, 1e-6f)
        assertEquals("song.mp3", back[0].name)
        a.volume = Float.NaN; assertEquals(1f, a.volume, 0f)
    }
    @Test fun softSplitAndCopy() {
        val a = AudioStrip("content://a", "a").apply { start = 10; durationMs = 10_000; track = 2 }
        a.trim(1_000, 3_000) // 2 s = 24 frames at 12 fps: frames 10..33
        assertEquals(null, a.split(10, 12)); assertEquals(null, a.split(34, 12))
        val r = a.split(16, 12)!!
        assertEquals(10, a.start); assertEquals(1_000L, a.trimStartMs); assertEquals(1_500L, a.trimEndMs); assertEquals(15, a.endFrame(12))
        assertEquals(16, r.start); assertEquals(1_500L, r.trimStartMs); assertEquals(3_000L, r.trimEndMs); assertEquals(33, r.endFrame(12))
        assertEquals(2, r.track)
        val back = AudioStrip.listFromJson(AudioStrip.listToJson(listOf(r)))[0]
        assertEquals(2, back.track)
        val c = r.copy(); c.start = 50; assertEquals(16, r.start)
    }
}
