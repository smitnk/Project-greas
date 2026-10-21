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
    @Test fun transformBoxGeometry() {
        val r = Rotoscope(); r.set(Rotoscope.Kind.IMAGE, listOf("x"), 1); r.pixW = 200; r.pixH = 100
        // 1000 x 500 canvas: fit 5, so the image covers it
        assertEquals(500f to 250f, r.halfSize(1000, 500))
        assertEquals(0f to 0f, r.canvasToPixel(0f, 0f, 1000, 500))
        assertEquals(200f to 100f, r.canvasToPixel(1000f, 500f, 1000, 500))
        // drag the right edge to x = 750: half as wide (squash), height kept
        r.dragHandle(5, 750f, 250f, 1000, 500)
        assertEquals(0.5f, r.stretchX, 1e-4f); assertEquals(1f, r.stretchY, 1e-4f)
        assertEquals(5, r.handleAt(752f, 250f, 10f, 1000, 500, 40f))
        assertEquals(9, r.handleAt(520f, 260f, 10f, 1000, 500, 40f))
        // a corner: both directions at once
        r.dragHandle(2, 600f, 300f, 1000, 500)
        assertEquals(0.2f, r.stretchX, 1e-4f); assertEquals(0.2f, r.stretchY, 1e-4f)
    }
    @Test fun cutOutPieceKeepsItsPlace() {
        val r = Rotoscope(); r.set(Rotoscope.Kind.IMAGE, listOf("x"), 1); r.pixW = 200; r.pixH = 100
        r.rotation = 0.3f; r.stretchY = 1.5f; r.dx = 20f
        val p = r.pieceAt(50, 20, 40, 30, 1000, 500)
        // the piece's corners land on the same canvas points as those pixels of the original
        val pc = p.corners(1000, 500)
        val tl = r.localToCanvas(50f / 200 * 1000 - 500, 20f / 100 * 500 * 1.5f - 375f, 1000, 500)
        assertEquals(tl.first, pc[0].first, 0.01f); assertEquals(tl.second, pc[0].second, 0.01f)
        val px = p.canvasToPixel(pc[2].first, pc[2].second, 1000, 500)
        assertEquals(40f, px.first, 0.01f); assertEquals(30f, px.second, 0.01f)
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
