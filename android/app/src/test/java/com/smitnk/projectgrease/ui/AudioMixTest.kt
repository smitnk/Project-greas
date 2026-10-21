package com.smitnk.projectgrease.ui

import org.junit.Assert.assertEquals
import org.junit.Test

class AudioMixTest {
    @Test fun monoBecomesStereoAtTheSameRate() {
        val out = AudioMix.resampleStereo(shortArrayOf(100, 200, 300), 44100, 1)
        assertEquals(6, out.size)
        assertEquals(100f, out[0], 0f); assertEquals(100f, out[1], 0f); assertEquals(300f, out[4], 0f)
    }
    @Test fun upsamplingInterpolates() {
        // 22.05 kHz stereo -> twice the frames, the in-between ones halfway
        val out = AudioMix.resampleStereo(shortArrayOf(0, 0, 1000, -1000), 22050, 2)
        assertEquals(8, out.size)
        assertEquals(500f, out[2], 1f); assertEquals(-500f, out[3], 1f)
        assertEquals(1000f, out[4], 1f)
    }
}
