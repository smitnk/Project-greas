package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Test

/** BKE_image_user_frame_get cases worked out from the Blender 3.6.23 code. */
class RotoscopeTest {
    private fun f(cfra: Int, frames: Int = 10, sfra: Int = 1, offset: Int = 0, cycl: Boolean = false) =
        Rotoscope.userFrameGet(frames, sfra, offset, cycl, cfra)

    @Test fun followsTheTimeline() {
        assertEquals(1, f(1)); assertEquals(5, f(5)); assertEquals(10, f(10))
        assertEquals("holds the last frame", 10, f(25))
        assertEquals("before the start", 0, f(0))
    }
    @Test fun startAndOffset() {
        assertEquals(1, f(20, sfra = 20)); assertEquals(3, f(22, sfra = 20))
        assertEquals(6, f(5, offset = 1))
    }
    @Test fun cyclic() {
        assertEquals(10, f(10, cycl = true)); assertEquals(1, f(11, cycl = true)); assertEquals(10, f(0, cycl = true))
    }
    @Test fun stillImageAlwaysShows() {
        val r = Rotoscope(); r.set(Rotoscope.Kind.IMAGE, listOf("content://x"), 1)
        assertEquals(1, r.imageFrame(300))
        r.set(Rotoscope.Kind.SEQUENCE, listOf("a", "b", "c"), 3)
        assertEquals(2, r.imageFrame(2)); assertEquals(3, r.imageFrame(99))
        r.offset = 5
        assertEquals("offset past the end: none", 0, r.imageFrame(2))
    }
    @Test fun placementIsSavedAndClamped() {
        val r = Rotoscope(); r.set(Rotoscope.Kind.IMAGE, listOf("content://x"), 1)
        r.moveBy(40f, -10f, 2f, 0.5f); r.moveBy(0f, 0f, 100f)
        assertEquals(20f, r.scale, 1e-4f)
        val back = Rotoscope(); back.fromJson(r.toJson())
        assertEquals(40f, back.dx, 1e-4f); assertEquals(-10f, back.dy, 1e-4f)
        assertEquals(20f, back.scale, 1e-4f); assertEquals(0.5f, back.rotation, 1e-4f)
        r.moveBy(Float.NaN, 0f); assertEquals(40f, r.dx, 1e-4f)
        back.fromJson(org.json.JSONObject().put("uris", org.json.JSONArray(listOf("a"))))
        assertEquals("old files: centred, fitted", 1f, back.scale, 1e-4f)
    }
    @Test fun trimShowsOnlyThePickedPart() {
        val r = Rotoscope(); r.set(Rotoscope.Kind.MOVIE, listOf("content://v"), 100)
        r.trim(31, 40); r.start = 5
        assertEquals(10, r.usedFrames)
        assertEquals(31, r.imageFrame(5)); assertEquals(40, r.imageFrame(14))
        assertEquals("after the trim: none", 0, r.imageFrame(15)); assertEquals(0, r.imageFrame(4))
        val back = Rotoscope(); back.fromJson(r.toJson())
        assertEquals(35, back.imageFrame(9))
    }
    @Test fun photoOnParticularFramesAndSquash() {
        val r = Rotoscope(); r.set(Rotoscope.Kind.IMAGE, listOf("content://x"), 1)
        r.onFrames.add(3); r.stretchX = 0.5f
        assertEquals(0, r.imageFrame(2)); assertEquals(1, r.imageFrame(3))
        val c = r.copy(); c.onFrames.clear(); c.onFrames.add(7)
        assertEquals(0, c.imageFrame(3)); assertEquals(1, c.imageFrame(7)); assertEquals(0.5f, c.stretchX, 1e-6f)
        assertEquals("the original keeps its frame", 1, r.imageFrame(3))
    }
}
