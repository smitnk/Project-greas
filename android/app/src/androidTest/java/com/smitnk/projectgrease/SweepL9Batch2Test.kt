package com.smitnk.projectgrease

import android.graphics.Bitmap
import android.graphics.Color
import android.os.SystemClock
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/** Engine depth batch 2: live stroke preview (A9), timeline selection history (A5), GPU trim (A6). */
@RunWith(AndroidJUnit4::class)
class SweepL9Batch2Test : SweepBase() {

    /** A9: the stroke being drawn (finger still down) looks like the stroke committed on lift. */
    @Test fun livePreviewMatchesCommittedStroke() {
        onUi { controller.brushes.setSize(30f); controller.brushes.setHardness(0.3f); controller.brushes.setCaps(true); controller.brushes.setAspect(1f, 0.5f) }
        val a = screen(250f, 300f); val b = screen(950f, 330f)
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, a.first, a.second))
        val n = 30
        for (i in 1..n) {
            SystemClock.sleep(15)
            val t = i.toFloat() / n
            instrumentation.sendPointerSync(event(MotionEvent.ACTION_MOVE, down, a.first + (b.first - a.first) * t, a.second + (b.second - a.second) * t))
        }
        val live = shot("l9_live_stroke")
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, b.first, b.second))
        rule.waitForIdle()
        assertEquals(1, strokes().size)
        val done = shot("l9_committed_stroke")
        // the middle of the stroke (the ends hold the finger / the pointer overlays)
        val p0 = screen(450f, 290f); val p1 = screen(750f, 340f)
        val x0 = p0.first.toInt(); val x1 = p1.first.toInt(); val y0 = p0.second.toInt() - 40; val y1 = p1.second.toInt() + 40
        var total = 0; var differ = 0; var ink = 0
        for (y in y0..y1) for (x in x0..x1) {
            val c1 = live.getPixel(x, y); val c2 = done.getPixel(x, y)
            total++
            if (isDark(c2)) ink++
            if (abs(Color.red(c1) - Color.red(c2)) > 24 || abs(Color.green(c1) - Color.green(c2)) > 24 || abs(Color.blue(c1) - Color.blue(c2)) > 24) differ++
        }
        assertTrue("the committed stroke is on screen ($ink dark px)", ink > 200)
        assertTrue("live preview differs from the committed stroke in $differ of $total px", differ * 200 <= total)
    }

    /** A5: GP_FRAME_SELECT is part of the undo snapshots, so selecting frames is its own step. */
    @Test fun timelineSelectionIsAnUndoStep() {
        onUi { controller.createFrame(3); controller.createFrame(5) }
        val before = onUi { controller.animation.refreshKeyInfo(); controller.animation.selectedFrames.toSet() }
        assertTrue(onUi { controller.selectTimelineFrame(3, S.FRAME_SELECT_SET) })
        assertEquals(setOf(3), onUi { controller.animation.selectedFrames.toSet() })
        assertTrue(onUi { controller.undo() })
        assertEquals(before, onUi { controller.animation.refreshKeyInfo(); controller.animation.selectedFrames.toSet() })
        assertTrue(onUi { controller.redo() })
        assertEquals(setOf(3), onUi { controller.animation.refreshKeyInfo(); controller.animation.selectedFrames.toSet() })
        assertTrue(onUi { controller.deselectTimelineFrames() })
        assertTrue(onUi { controller.undo() })
        assertEquals(setOf(3), onUi { controller.animation.refreshKeyInfo(); controller.animation.selectedFrames.toSet() })
    }

    /** A6: trimming the GPU pool (export, background) leaves the canvas drawing. */
    @Test fun trimGpuMemoryKeepsDrawing() {
        line(300f)
        assertEquals(1, strokes().size)
        assertTrue(onUi { controller.trimGpuMemory(false) } >= 0)
        assertTrue(onUi { controller.trimGpuMemory(true) } >= 0)
        onUi { controller.render() }
        val bmp = shot("l9_after_trim")
        assertInk(bmp, 600f, 300f, "stroke after trimming the pool")
        // an export-sized frame, then trimmed, then the screen frame again
        assertTrue(onUi { controller.renderCanvasPixels(false) } != null)
        assertTrue(onUi { controller.trimGpuMemory(true) } >= 0)
        assertInk(shot("l9_after_export_trim"), 600f, 300f, "stroke after export and trim")
    }
}
