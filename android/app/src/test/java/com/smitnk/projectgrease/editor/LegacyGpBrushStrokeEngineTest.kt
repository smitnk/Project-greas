package com.smitnk.projectgrease.editor

import com.smitnk.projectgrease.editor.LegacyGpBrushStrokeEngine.InputEvent
import com.smitnk.projectgrease.editor.LegacyGpBrushStrokeEngine.Settings
import com.smitnk.projectgrease.editor.LegacyGpBrushStrokeEngine.StrokePoint
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Conformance tests against Blender 3.6.23 gpencil_paint.c. Expected numbers were derived from
 * a line-by-line reference model of the same functions, not guessed.
 */
class LegacyGpBrushStrokeEngineTest {

    private class Run(val engine: LegacyGpBrushStrokeEngine, val sent: List<StrokePoint>)

    private fun run(settings: Settings, events: List<InputEvent>): Run {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(settings)
        val sent = ArrayList<StrokePoint>()
        events.forEach { sent += engine.add(it) }
        sent += engine.end()
        return Run(engine, sent)
    }

    private fun line(count: Int, stepX: Float, y: Float = 100f, pressure: Float = 1f) =
        (0 until count).map { InputEvent(stepX * it, y, pressure, 0.016f * it) }

    @Test
    fun firstPointerSampleIsAlwaysAccepted() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings())
        val out = engine.add(InputEvent(10f, 20f, 1f, 5f))
        assertEquals(1, out.size)
        assertEquals(10f, out[0].x, 0f)
        assertEquals(20f, out[0].y, 0f)
    }

    @Test
    fun smallMotionUsesBlenderManhattanAndEuclideanFilter() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings(manhattanThreshold = 1, euclideanThreshold = 1f))
        engine.add(InputEvent(0f, 0f, 1f, 0f))
        // dx = 1, dy = 0: not > manhattan on both axes, and 1 is not > 1 (euclid^2).
        assertTrue(engine.add(InputEvent(1f, 0f, 1f, 0.01f)).isEmpty())
        // dx = 2: euclidean test passes.
        assertEquals(1, engine.add(InputEvent(2f, 0f, 1f, 0.02f)).size)
    }

    @Test
    fun lazyMouseInterpolatesAcceptedSampleTowardPreviousPoint() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings(lazyEnabled = true, smoothStrokeRadius = 5f, smoothStrokeFactor = 0.5f))
        engine.add(InputEvent(0f, 0f, 1f, 0f))
        val out = engine.add(InputEvent(10f, 0f, 1f, 0.01f))
        assertEquals(1, out.size)
        assertEquals(5f, out[0].x, 1e-5f) // interp(current, previous, 0.5)
        // Inside the lazy radius: no point (the previous mouse position is kept).
        assertTrue(engine.add(InputEvent(8f, 0f, 1f, 0.02f)).isEmpty())
    }

    @Test
    fun arcsNeedThreeBufferedPointsAndReplaceTheLastOne() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings(inputSamples = 4)) // min distance = 4 * (10 - 4 + 1) = 28
        val sizes = ArrayList<Int>()
        for (i in 0 until 4) {
            engine.add(InputEvent(40f * i, 0f, 1f, 0.01f * i))
            sizes += engine.bufferedPoints().size
        }
        // Third event still has only two buffered points -> no arc. Fourth: 3 buffered, so an arc
        // with 2 slices replaces the last point: 3 - 1 + 2 + 1 (mouse point) = 5.
        assertEquals(listOf(1, 2, 3, 5), sizes)
    }

    @Test
    fun slowMotionNeverSynthesizesPoints() {
        val events = (0 until 10).map { InputEvent(10f * it, 5f * it, 1f, 0.016f * it) }
        val r = run(Settings(inputSamples = 4), events)
        assertEquals(10, r.sent.size)
    }

    @Test
    fun fastStraightStrokeNeverDoublesBackOrBends() {
        val r = run(Settings(inputSamples = 4), line(12, 40f))
        val xs = r.sent.map { it.x }
        // The old "append after the last point" behaviour jumped from x=80 back to ~65.
        for (i in 1 until xs.size) {
            assertTrue("x[$i]=${xs[i]} went back from ${xs[i - 1]}", xs[i] >= xs[i - 1] - 1e-3f)
        }
        r.sent.forEach { assertEquals(100f, it.y, 1e-3f) }
    }

    @Test
    fun streamSentToNativeEqualsTheFinalBuffer() {
        val r = run(Settings(inputSamples = 4, activeSmooth = 0.5f), line(20, 33f))
        assertEquals(r.engine.bufferedPoints(), r.sent)
    }

    @Test
    fun timeIsNormalizedToStrokeStart() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings())
        val first = engine.add(InputEvent(0f, 0f, 1f, 100f))
        val second = engine.add(InputEvent(10f, 0f, 1f, 100.5f))
        assertEquals(0f, first[0].time, 0f)
        assertEquals(0.5f, second[0].time, 1e-5f)
    }

    @Test
    fun pressureOneStaysOneWithoutPressureModifiers() {
        val r = run(Settings(), line(6, 5f))
        r.sent.forEach { assertEquals(1f, it.pressure, 0f) }
    }

    @Test
    fun changingPressureChangesOnlyPressureWhenStrengthPressureDisabled() {
        val r = run(Settings(drawStrength = 0.8f), line(6, 5f, pressure = 0.3f))
        r.sent.forEach {
            assertEquals(0.3f, it.pressure, 1e-5f)
            assertEquals(0.8f, it.strength, 1e-6f)
        }
    }

    @Test
    fun strengthPressureScalesStrengthButKeepsTheMinimum() {
        val r = run(
            Settings(drawStrength = 0.5f, useStrengthPressure = true),
            line(4, 5f, pressure = 0.5f)
        )
        r.sent.forEach { assertEquals(0.25f, it.strength, 1e-5f) }
    }

    @Test
    fun interpfWeightsTheTargetByTheFactor() {
        // BLI_math_base interpf(target, origin, fac) = fac * target + (1 - fac) * origin
        assertEquals(3f, LegacyGpBrushStrokeEngine.interpf(10f, 0f, 0.3f), 1e-6f)
        assertEquals(7f, LegacyGpBrushStrokeEngine.interpf(0f, 10f, 0.3f), 1e-6f)
    }

    @Test
    fun drawAngleBlendsWithThePreviousPointUsingInterpf() {
        // Angle 0, factor 0.5, horizontal motion. First point: 1.0 - 0.5 * (1.4 - 1) = 0.8.
        // Second: interpf(0.5 - 0.5 * 0, 0.8, 0.3) = 0.3 * 0.5 + 0.7 * 0.8 = 0.71.
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings(drawAngleFactor = 0.5f, drawAngle = 0f))
        engine.add(InputEvent(0f, 0f, 1f, 0f))
        engine.add(InputEvent(20f, 0f, 0.5f, 0.01f))
        engine.end()
        val points = engine.bufferedPoints()
        assertEquals(0.8f, points[0].pressure, 1e-4f)
        assertEquals(0.71f, points[1].pressure, 1e-4f)
    }

    @Test
    fun activeSmoothingKeepsAStraightLineStraight() {
        val events = (0 until 20).map { InputEvent(8f * it, 50f, 0.4f + 0.02f * it, 0.016f * it) }
        val r = run(Settings(activeSmooth = 0.65f), events)
        assertEquals(20, r.sent.size)
        r.sent.forEach {
            assertEquals(50f, it.y, 1e-4f)
            assertTrue(it.pressure in 0.3f..0.9f)
        }
    }

    @Test
    fun activeSmoothingPullsAPressureSpikeTowardItsNeighbours() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings(activeSmooth = 0.5f))
        listOf(1f, 1f, 1f, 0.2f, 1f, 1f).forEachIndexed { i, p ->
            engine.add(InputEvent(10f * i, 0f, p, 0.01f * i))
        }
        val pressures = engine.bufferedPoints().map { it.pressure }
        // Reference model: [1.0, 0.951, 0.874, 0.697, 0.918, 1.0]
        assertEquals(0.697f, pressures[3], 2e-3f)
        assertTrue(pressures[3] > 0.2f && pressures[3] < 1f)
    }

    @Test
    fun activeSmoothingOffLeavesPointsUntouched() {
        val r = run(Settings(activeSmooth = 0f), line(6, 7f, pressure = 0.5f))
        assertEquals(6, r.sent.size)
        r.sent.forEachIndexed { i, p -> assertEquals(7f * i, p.x, 1e-5f) }
    }

    @Test
    fun trailingNearZeroPressureIsTruncatedLikeBlender() {
        // inputSamples = 1 keeps four points private; min arc distance is 40, so no arcs here.
        val events = listOf(1f, 1f, 1f, 1f, 1f, 0f).mapIndexed { i, p ->
            InputEvent(5f * i, 0f, p, 0.01f * i)
        }
        val r = run(Settings(inputSamples = 1), events)
        // Blender sets sbuffer_used = last_i - 1 (dropping one extra point).
        assertEquals(4, r.sent.size)
        assertEquals(r.engine.bufferedPoints(), r.sent)
    }

    @Test
    fun endAndCancelResetState() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(Settings())
        engine.add(InputEvent(0f, 0f, 1f, 0f))
        engine.cancel()
        assertTrue(engine.add(InputEvent(10f, 0f, 1f, 0.1f)).isEmpty())
        assertTrue(engine.bufferedPoints().isEmpty())

        engine.begin(Settings())
        assertEquals(1, engine.add(InputEvent(3f, 3f, 1f, 0f)).size)
        engine.end()
        assertFalse(engine.add(InputEvent(30f, 3f, 1f, 0.1f)).isNotEmpty())
    }
}
