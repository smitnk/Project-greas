package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Kotlin side of the native tool session: the batch handed to native holds EVERY sample of the
 * input event (Android's historical samples first, then the current one), mapped to canvas
 * units with the presenter's own mapping, and the parameter layouts match
 * native/blender_gp/project_grease_tool_session.h.
 */
class ToolSessionInputTest {
    private class FakeTouch(val xs: FloatArray, val ys: FloatArray, val ps: FloatArray, val ts: LongArray) : TouchHistory {
        override val historySize: Int get() = xs.size - 1
        override fun x(h: Int) = xs[h]
        override fun y(h: Int) = ys[h]
        override fun pressure(h: Int) = ps[h]
        override fun timeMs(h: Int) = ts[h]
    }

    private fun touch(n: Int) = FakeTouch(
        FloatArray(n) { 100f + 10f * it }, FloatArray(n) { 200f + 3f * it },
        FloatArray(n) { 0.1f * (it + 1) }, LongArray(n) { 1000L + 8L * it }
    )

    @Test
    fun everyHistoricalSampleIsInTheBatchInOrder() {
        val t = touch(8) // 7 historical + current
        val batch = ToolSampleBatch.from(t, gestureStartMs = 1000L, isPen = true, includeHistory = true) { x, y -> x to y }
        assertEquals(8 * ToolSession.STRIDE, batch.size)
        for (k in 0 until 8) {
            assertEquals(t.xs[k], batch[k * 4], 0f)
            assertEquals(t.ys[k], batch[k * 4 + 1], 0f)
            assertEquals(t.ps[k], batch[k * 4 + 2], 1e-6f) // pen: real pressure
            assertEquals(0.008f * k, batch[k * 4 + 3], 1e-6f) // seconds since the gesture started
        }
    }

    @Test
    fun beginUsesOnlyTheCurrentSampleAndFingersReportPressureOne() {
        val t = touch(5)
        val batch = ToolSampleBatch.from(t, 1000L, isPen = false, includeHistory = false) { x, y -> x to y }
        assertEquals(ToolSession.STRIDE, batch.size)
        assertEquals(t.xs[4], batch[0], 0f)
        assertEquals(1f, batch[2], 0f) // finger: Blender's mouse pressure
    }

    @Test
    fun samplesAreMappedWithThePresenterMapping() {
        val t = touch(3)
        val batch = ToolSampleBatch.from(t, 1000L, true, true) { x, y ->
            CanvasMapping.toCanvas(x, y, 1080f, 1920f, 1280, 720, 1.5f, 20f, -40f, clamp = false)
        }
        for (k in 0 until 3) {
            val back = CanvasMapping.toView(batch[k * 4], batch[k * 4 + 1], 1080f, 1920f, 1280, 720, 1.5f, 20f, -40f)
            assertEquals(t.xs[k], back.first, 1e-3f)
            assertEquals(t.ys[k], back.second, 1e-3f)
        }
        // Same layout as ViewController.canvasRect (presenter update_canvas_map).
        val rect = ViewController.canvasRect(1080f, 1920f, 1280f, 720f, 1.5f, 20f, -40f)
        val origin = CanvasMapping.toView(0f, 0f, 1080f, 1920f, 1280, 720, 1.5f, 20f, -40f)
        assertEquals(rect[0], origin.first, 1e-3f)
        assertEquals(rect[1], origin.second, 1e-3f)
        assertEquals(rect[2] / 1280f, CanvasMapping.pixelsPerUnit(1080f, 1920f, 1280, 720, 1.5f), 1e-5f)
    }

    @Test
    fun lassoPointsUseTheSameCanvasMapping() {
        // A lasso drawn around the canvas centre on screen lands around the canvas centre.
        val center = CanvasMapping.toView(640f, 360f, 1000f, 800f, 1280, 720, 1f, 0f, 0f)
        val c = CanvasMapping.toCanvas(center.first, center.second, 1000f, 800f, 1280, 720, 1f, 0f, 0f)
        assertEquals(640f, c.first, 1e-3f)
        assertEquals(360f, c.second, 1e-3f)
    }

    @Test
    fun packAppendsTheBeginParametersAfterTheSamples() {
        val samples = floatArrayOf(1f, 2f, 0.5f, 0f, 3f, 4f, 0.6f, 0.01f)
        val params = ToolSession.brushParams(ToolSession.GPSCULPT_GRAB, 30f, 0.7f, 2.5f, invert = true, seed = 9)
        val packed = ToolSession.pack(samples, 2, params)
        assertEquals(8 + ToolSession.P_COUNT, packed.size)
        assertArrayEquals(samples, packed.copyOf(8), 0f)
        assertEquals(3f, packed[8 + ToolSession.P_BRUSH], 0f)
        assertEquals(30f, packed[8 + ToolSession.P_RADIUS], 0f)
        assertEquals(2.5f, packed[8 + ToolSession.P_PX_PER_UNIT], 0f)
        assertEquals(1f, packed[8 + ToolSession.P_INVERT], 0f)
        assertEquals(9f, packed[8 + ToolSession.P_SEED], 0f)
        // MOVE batches carry the samples only.
        assertEquals(8, ToolSession.pack(samples, 2, null).size)
    }

    @Test
    fun drawParametersFollowTheNativeLayout() {
        val p = ToolSession.DrawSettings(material = 2, thickness = 12f, inputSamples = 4, lazy = true, lazyRadius = 9f).toParams()
        assertEquals(19, p.size) // PG_DRAW_P_COUNT
        assertEquals(2f, p[0], 0f)
        assertEquals(12f, p[1], 0f)
        assertEquals(4f, p[8], 0f)  // PG_DRAW_P_INPUT_SAMPLES
        assertEquals(1f, p[9], 0f)  // PG_DRAW_P_LAZY
        assertEquals(9f, p[10], 0f) // PG_DRAW_P_LAZY_RADIUS
        assertEquals(1f, p[18], 0f) // PG_DRAW_P_FAKE_POINTS
    }

    @Test
    fun brushIdsAreBlendersToolEnums() {
        assertEquals(0, ToolSession.sculptTool(SculptBrush.SMOOTH))
        assertEquals(3, ToolSession.sculptTool(SculptBrush.GRAB))
        assertEquals(5, ToolSession.sculptTool(SculptBrush.TWIST))
        assertEquals(6, ToolSession.sculptTool(SculptBrush.PINCH))
        assertEquals(7, ToolSession.sculptTool(SculptBrush.RANDOMIZE))
        assertEquals(4, ToolSession.vertexTool(ProjectGreaseSelect.VPAINT_SMEAR))
        assertEquals(5, ToolSession.vertexTool(ProjectGreaseSelect.VPAINT_REPLACE))
        assertTrue(ToolSession.GPWEIGHT_SMEAR == 3)
    }
}
