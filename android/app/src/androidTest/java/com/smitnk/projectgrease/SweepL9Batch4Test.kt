package com.smitnk.projectgrease

import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.EraserMode
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/** Batch 4: one gesture state per touch type, tool carries its mode, audio follows scrub / fps, view gestures during playback. */
@RunWith(AndroidJUnit4::class)
class SweepL9Batch4Test : SweepBase() {
    private fun multi(action: Int, down: Long, pts: List<Pair<Float, Float>>): MotionEvent {
        val props = Array(pts.size) { i -> MotionEvent.PointerProperties().apply { id = i; toolType = MotionEvent.TOOL_TYPE_FINGER } }
        val coords = Array(pts.size) { i -> MotionEvent.PointerCoords().apply { x = pts[i].first; y = pts[i].second; pressure = 1f; size = 1f } }
        return MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, pts.size, props, coords, 0, 0, 1f, 1f, 0, 0,
            InputDevice.SOURCE_TOUCHSCREEN, 0)
    }

    /** n fingers down together, moved by [move] (finger, fraction), then up. */
    private fun fingers(start: List<Pair<Float, Float>>, steps: Int = 0, move: (Int, Float) -> Pair<Float, Float> = { i, _ -> start[i] }) {
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_DOWN, down, start.take(1)))
        for (k in 2..start.size)
            instrumentation.sendPointerSync(multi(MotionEvent.ACTION_POINTER_DOWN or ((k - 1) shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, start.take(k)))
        var cur = start
        for (s in 1..steps) {
            SystemClock.sleep(12)
            cur = start.indices.map { move(it, s.toFloat() / steps) }
            instrumentation.sendPointerSync(multi(MotionEvent.ACTION_MOVE, down, cur))
        }
        for (k in start.size downTo 2)
            instrumentation.sendPointerSync(multi(MotionEvent.ACTION_POINTER_UP or ((k - 1) shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, cur.take(k)))
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_UP, down, cur.take(1)))
        rule.waitForIdle()
    }

    /** Two fingers 200 px apart around canvas (600, 400), spread to 1.5x. */
    private fun pinchOut() {
        val a = screen(500f, 400f); val b = screen(700f, 400f)
        val cx = (a.first + b.first) / 2; val r0 = (b.first - a.first) / 2
        fingers(listOf(a, b), steps = 20) { i, t -> val r = r0 * (1f + 0.5f * t); (if (i == 0) cx - r else cx + r) to a.second }
    }

    // ---- B1: one test per gesture type ----
    @Test fun gestureDrawAddsStroke() {
        line(300f)
        assertEquals(1, strokes().size)
    }

    @Test fun gestureEraseRemovesPoints() {
        line(300f)
        val before = points(strokes().single()).size
        onUi { controller.selectTool(GreaseTool.ERASE); controller.setEraserMode(EraserMode.HARD); controller.setEraserSize(30f) }
        drag(600f to 200f, 600f to 400f)
        val after = strokes().sumOf { points(it).size }
        assertTrue("erase removed points ($before -> $after, ${strokes().size} strokes)", after < before)
    }

    @Test fun gesturePanMovesView() {
        val p0 = onUi { controller.view.panX to controller.view.panY }
        onUi { controller.selectTool(GreaseTool.PAN) }
        drag(400f to 300f, 600f to 400f)
        val p1 = onUi { controller.view.panX to controller.view.panY }
        assertTrue("pan offset changed $p0 -> $p1", abs(p1.first - p0.first) > 20f && abs(p1.second - p0.second) > 20f)
        assertEquals(0, strokes().size)
    }

    @Test fun gesturePinchZooms() {
        val z0 = onUi { controller.view.zoom }
        pinchOut()
        assertEquals(z0 * 1.5f, onUi { controller.view.zoom }, 0.15f)
        assertEquals("a pinch draws nothing", 0, strokes().size)
    }

    @Test fun gestureTwoFingerTapUndoes() {
        line(300f)
        assertEquals(1, strokes().size)
        fingers(listOf(screen(500f, 500f), screen(700f, 500f)))
        assertEquals("two-finger tap undid the stroke", 0, strokes().size)
    }

    @Test fun gestureTransformMovesSelection() {
        line(300f)
        onUi { controller.selectTool(GreaseTool.MOVE) }
        drag(600f to 300f, 600f to 400f)
        val ys = points(strokes().single()).map { it[1] }
        assertEquals("the stroke moved down with the drag", 400.0, ys.average(), 8.0)
    }

    @Test fun gestureLassoSelects() {
        line(300f)
        assertEquals(0, onUi { controller.selectedPointCount() })
        onUi { controller.selectTool(GreaseTool.LASSO) }
        drag(250f to 200f, 950f to 200f, 950f to 400f, 250f to 400f, 250f to 200f)
        assertTrue("lasso selected points", onUi { controller.selectedPointCount() } > 0)
        assertEquals("the lasso adds no stroke", 1, strokes().size)
    }

    @Test fun gestureWarpApplies() {
        line(300f, 300f, 900f)
        onUi { controller.selectAll() }
        assertTrue(onUi { controller.beginWarp(2, 2) })
        rule.waitForIdle()
        val p = onUi { controller.warpPoints!!.copyOf() }
        // drag lattice point 0 (top-left) down by 80 canvas units
        drag(p[0] to p[1], p[0] to p[1] + 80f)
        onUi { controller.endWarp(true) }
        val before = 300.0
        val ys = points(strokes().single()).map { it[1] }
        assertTrue("warp bent the stroke (ys ${ys.minOrNull()}..${ys.maxOrNull()})", ys.any { abs(it - before) > 5.0 })
    }

    // ---- B4 ----
    @Test fun toolAndModeNeverDisagree() {
        for (m in GreaseMode.entries) for (t in GreaseTool.entries) {
            val modeOk = onUi { controller.setMode(m) }
            assertTrue("$m keeps a valid tool (${onUi { controller.tools.activeTool }})", onUi { controller.tools.activeTool.allowedIn(controller.mode) })
            if (!onUi { controller.selectTool(t) }) continue
            val (tool, mode) = onUi { controller.tools.activeTool to controller.mode }
            assertEquals(t, tool)
            assertTrue("$t in $mode (from $m, set $modeOk)", tool.allowedIn(mode))
            t.mode?.let { assertEquals("$t is its mode", it, mode) }
            if (modeOk && t.mode == null && m != GreaseMode.SCULPT && t.allowedIn(m)) assertEquals("$t keeps mode $m", m, mode)
        }
        onUi { controller.setMode(GreaseMode.EDIT); controller.selectTool(GreaseTool.DRAW) }
        assertEquals("the Draw brush returns to Draw mode", GreaseMode.DRAW, onUi { controller.mode })
    }

    // ---- B7 ----
    private fun wav(seconds: Int): java.io.File {
        val rate = 8000; val n = rate * seconds
        val f = java.io.File(instrumentation.targetContext.cacheDir, "b4_tone.wav")
        val b = java.nio.ByteBuffer.allocate(44 + 2 * n).order(java.nio.ByteOrder.LITTLE_ENDIAN)
        b.put("RIFF".toByteArray()).putInt(36 + 2 * n).put("WAVEfmt ".toByteArray()).putInt(16).putShort(1).putShort(1)
            .putInt(rate).putInt(2 * rate).putShort(2).putShort(16).put("data".toByteArray()).putInt(2 * n)
        for (i in 0 until n) b.putShort((8000 * kotlin.math.sin(2 * Math.PI * 440 * i / rate)).toInt().toShort())
        f.writeBytes(b.array())
        return f
    }

    /** Waits until the sound's position matches the playhead's time; returns the last (position, expected). */
    private fun awaitAudioAt(strip: com.smitnk.projectgrease.editor.AudioStrip): Pair<Long?, Long> {
        var last: Pair<Long?, Long> = null to 0L
        val end = SystemClock.uptimeMillis() + 2000
        while (SystemClock.uptimeMillis() < end) {
            last = onUi { controller.audioPlayback?.positionMs(strip) to strip.fileMs(controller.animation.currentFrame, controller.animation.fps) }
            val pos = last.first
            if (pos != null && abs(pos - last.second) <= 250) return last
            SystemClock.sleep(50)
        }
        return last
    }

    @Test fun audioFollowsScrubAndFps() {
        val f = wav(8)
        val strip = onUi {
            controller.animation.setFps(12)
            controller.animation.setFrame(1)
            controller.addAudio(android.net.Uri.fromFile(f).toString(), "tone", 8000L, start = 1)
        }
        assertNotNull(strip)
        onUi { controller.animation.togglePlayback() }
        SystemClock.sleep(400)
        assertTrue(onUi { controller.animation.playing })
        onUi { controller.animation.scrubTo(60f) }
        val (p1, e1) = awaitAudioAt(strip!!)
        assertTrue("after scrub to 60 at 12 fps audio at $p1 ms, expected ~$e1 ms (59/12 s)", p1 != null && abs(p1 - e1) <= 250)
        onUi { controller.animation.setFps(24) }
        val (p2, e2) = awaitAudioAt(strip)
        assertTrue("after fps 24 audio at $p2 ms, expected ~$e2 ms", p2 != null && abs(p2 - e2) <= 250)
        onUi { controller.stopPlayback() }
    }

    // ---- B10 ----
    private fun startPlayback() {
        onUi { controller.createFrame(24); controller.animation.setFrame(1) }
        onUi { if (!controller.animation.playing) controller.animation.togglePlayback() }
        SystemClock.sleep(200)
        assertTrue("playing", onUi { controller.animation.playing })
    }

    @Test fun pinchDuringPlaybackKeepsPlaying() {
        startPlayback()
        val z0 = onUi { controller.view.zoom }
        pinchOut()
        assertTrue("still playing after the pinch", onUi { controller.animation.playing })
        assertNotEquals("zoom changed", z0, onUi { controller.view.zoom }, 0.1f)
        onUi { controller.selectTool(GreaseTool.PAN) }
        val px = onUi { controller.view.panX }
        drag(400f to 300f, 600f to 300f)
        assertTrue("still playing after the pan", onUi { controller.animation.playing })
        assertNotEquals("pan changed", px, onUi { controller.view.panX }, 10f)
        onUi { controller.stopPlayback() }
        assertEquals("view gestures drew nothing", 0, strokes().size)
    }

    @Test fun drawDuringPlaybackStops() {
        startPlayback()
        line(300f)
        assertFalse("drawing stopped playback", onUi { controller.animation.playing })
        assertEquals("the stroke was drawn", 1, strokes().size)
    }
}
