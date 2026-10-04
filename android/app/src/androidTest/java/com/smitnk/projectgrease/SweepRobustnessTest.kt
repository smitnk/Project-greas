package com.smitnk.projectgrease

import android.content.pm.ActivityInfo
import android.os.SystemClock
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseTool
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Rotation, background/foreground, low memory, undo/redo chains, 5,000-point strokes, 200-stroke frame time. */
@RunWith(AndroidJUnit4::class)
class SweepRobustnessTest : SweepBase() {

    private fun waitReady() {
        rule.waitUntil(15_000) { runCatching { onUi { controller.rendererReady } }.getOrDefault(false) }
        rule.waitForIdle()
    }

    @Test fun rotationKeepsDocument() {
        line(300f); line(500f)
        val before = signature()
        onUi { rule.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE }
        SystemClock.sleep(2500); waitReady()
        assertEquals("document after rotation", before, signature())
        shot("rotation_landscape")
        onUi { rule.activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT }
        SystemClock.sleep(2500); waitReady()
        assertEquals("document after rotating back", before, signature())
        assertInk(shot("rotation_portrait"), 600f, 300f, "stroke after rotation")
        line(700f)
        assertEquals("drawing still works", 3, strokes().size)
    }

    @Test fun backgroundForegroundKeepsDocument() {
        line(300f)
        val before = signature()
        shell("input keyevent KEYCODE_HOME")
        SystemClock.sleep(2000)
        shell("am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n com.smitnk.projectgrease/.MainActivity")
        SystemClock.sleep(2500); waitReady()
        assertEquals("document after background/foreground", before, signature())
        assertInk(shot("background_foreground"), 600f, 300f, "stroke after resume")
        line(500f)
        assertEquals(2, strokes().size)
    }

    @Test fun lowMemoryKeepsDocument() {
        line(300f)
        val before = signature()
        for (level in listOf("RUNNING_MODERATE", "RUNNING_LOW", "RUNNING_CRITICAL")) {
            shell("am send-trim-memory com.smitnk.projectgrease $level")
            SystemClock.sleep(500)
        }
        shell("input keyevent KEYCODE_HOME"); SystemClock.sleep(1000)
        for (level in listOf("BACKGROUND", "MODERATE", "COMPLETE")) {
            shell("am send-trim-memory com.smitnk.projectgrease $level"); SystemClock.sleep(300)
        }
        shell("am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n com.smitnk.projectgrease/.MainActivity")
        SystemClock.sleep(2500); waitReady()
        assertEquals("document after trim-memory", before, signature())
        assertInk(shot("low_memory"), 600f, 300f, "stroke after trim")
    }

    @Test fun undoRedoChain() {
        val states = arrayListOf(signature())
        line(200f); states += signature()
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }; drag(300f to 300f, 600f to 500f); onUi { controller.confirmShape() }; states += signature()
        onUi { controller.selectAll(); controller.duplicateSelection() }; states += signature()
        onUi { controller.translateSelectedStroke(30f, 30f) }; states += signature()
        onUi { controller.createLayer("X") }; states += signature()
        onUi { controller.selectTool(GreaseTool.DRAW) }; line(650f); states += signature()
        for (i in states.size - 1 downTo 1) {
            assertTrue("undo $i", onUi { controller.undo() })
            assertEquals("undo to state ${i - 1}", states[i - 1], signature())
        }
        for (i in 1 until states.size) {
            assertTrue("redo $i", onUi { controller.redo() })
            assertEquals("redo to state $i", states[i], signature())
        }
        shot("undo_redo_chain")
    }

    @Test fun longStroke5000Points() {
        val times = timedStroke(5000, 300f)
        val s = strokes().single()
        assertTrue("points ${points(s).size}", points(s).size > 1000)
        val med = median(times); val p95 = times.sorted()[times.size * 95 / 100]
        Log.i(TAG, "FRAMETIME long_stroke_5000 median=%.2f p95=%.2f max=%.2f".format(med, p95, times.maxOrNull() ?: 0.0))
        assertInk(shot("long_stroke_5000"), 600f, 300f, "long stroke")
        assertTrue("median frame time $med ms <= 32", med <= 32.0)
    }

    @Test fun frameTime200Strokes() {
        // 200 real strokes, then timed drawing on top of them.
        repeat(200) { i -> val y = 50f + (i % 50) * 18f; val x0 = 50f + (i / 50) * 300f; drag(x0 to y, x0 + 240f to y, steps = 3) }
        assertEquals(200, strokes().size)
        val times = timedStroke(400, 600f)
        val med = median(times); val p95 = times.sorted()[times.size * 95 / 100]
        Log.i(TAG, "FRAMETIME doc_200_strokes median=%.2f p95=%.2f max=%.2f".format(med, p95, times.maxOrNull() ?: 0.0))
        shot("frame_time_200_strokes")
        assertTrue("median frame time $med ms <= 32", med <= 32.0)
    }
}
