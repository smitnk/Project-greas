package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LegacyGpBrushStrokeEngineTest {
    @Test
    fun firstPointerSampleIsAlwaysAccepted() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(LegacyGpBrushStrokeEngine.Settings())
        val out = engine.add(LegacyGpBrushStrokeEngine.InputEvent(10f, 20f, 1f, 1f))
        assertEquals(1, out.size)
        assertEquals(10f, out[0].x, 0.0001f)
        assertEquals(20f, out[0].y, 0.0001f)
    }

    @Test
    fun smallMotionUsesManhattanOrEuclideanFilter() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(LegacyGpBrushStrokeEngine.Settings(
            manhattanThreshold = 2,
            euclideanThreshold = 4f
        ))
        assertEquals(1, engine.add(LegacyGpBrushStrokeEngine.InputEvent(0f, 0f, 1f, 0f)).size)
        assertTrue(engine.add(LegacyGpBrushStrokeEngine.InputEvent(3f, 3f, 1f, 0.01f)).isNotEmpty())
        assertFalse(engine.add(LegacyGpBrushStrokeEngine.InputEvent(4f, 3f, 1f, 0.02f)).isNotEmpty())
    }

    @Test
    fun lazyMouseInterpolatesAcceptedSampleTowardPreviousPoint() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(LegacyGpBrushStrokeEngine.Settings(
            lazyEnabled = true,
            smoothStrokeRadius = 4f,
            smoothStrokeFactor = 0.5f
        ))
        engine.add(LegacyGpBrushStrokeEngine.InputEvent(0f, 0f, 1f, 0f))
        val out = engine.add(LegacyGpBrushStrokeEngine.InputEvent(10f, 0f, 1f, 1f))
        assertEquals(1, out.size)
        assertEquals(5f, out[0].x, 0.0001f)
    }

    @Test
    fun fastMotionAddsIntermediateSamples() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(LegacyGpBrushStrokeEngine.Settings(inputSamples = 4))
        engine.add(LegacyGpBrushStrokeEngine.InputEvent(0f, 0f, 1f, 0f))
        val out = engine.add(LegacyGpBrushStrokeEngine.InputEvent(30f, 0f, 1f, 0.1f))
        assertTrue(out.size > 1)
        assertEquals(30f, out.last().x, 0.0001f)
    }

    @Test
    fun activeSmoothingUsesFourPointWindow() {
        val settings = LegacyGpBrushStrokeEngine.Settings(
            activeSmooth = 1f,
            activeSmoothPasses = 1,
            manhattanThreshold = 0,
            euclideanThreshold = 0f
        )
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(settings)
        engine.add(LegacyGpBrushStrokeEngine.InputEvent(0f, 0f, 1f, 0f))
        engine.add(LegacyGpBrushStrokeEngine.InputEvent(10f, 0f, 1f, 1f))
        engine.add(LegacyGpBrushStrokeEngine.InputEvent(20f, 0f, 1f, 2f))
        val out = engine.add(LegacyGpBrushStrokeEngine.InputEvent(30f, 0f, 1f, 3f))
        assertTrue(out.isNotEmpty())
    }

    @Test
    fun timeIsNormalizedToStrokeStart() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(LegacyGpBrushStrokeEngine.Settings(
            manhattanThreshold = 0,
            euclideanThreshold = 0f
        ))
        engine.add(LegacyGpBrushStrokeEngine.InputEvent(0f, 0f, 1f, 10f))
        val out = engine.add(LegacyGpBrushStrokeEngine.InputEvent(1f, 0f, 1f, 10.25f))
        assertEquals(0.25f, out.single().time, 0.0001f)
    }

    @Test
    fun endAndCancelResetState() {
        val engine = LegacyGpBrushStrokeEngine()
        engine.begin(LegacyGpBrushStrokeEngine.Settings())
        engine.add(LegacyGpBrushStrokeEngine.InputEvent(0f, 0f, 1f, 0f))
        engine.end()
        assertTrue(engine.add(LegacyGpBrushStrokeEngine.InputEvent(1f, 1f, 1f, 1f)).isEmpty())
        engine.begin(LegacyGpBrushStrokeEngine.Settings())
        engine.cancel()
        assertTrue(engine.add(LegacyGpBrushStrokeEngine.InputEvent(2f, 2f, 1f, 2f)).isEmpty())
    }
}
