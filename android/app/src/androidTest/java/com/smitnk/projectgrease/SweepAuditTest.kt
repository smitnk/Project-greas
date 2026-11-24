package com.smitnk.projectgrease

import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.EraserMode
import com.smitnk.projectgrease.editor.GreaseTool
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Audit fixes of the fill eraser: draw order and one undo step, locked materials, a second finger cancels. */
@RunWith(AndroidJUnit4::class)
class SweepAuditTest : SweepBase() {
    private fun allPoints(): List<List<List<Double>>> = strokes().map { s -> points(s).map { it.toList() } }
    private fun redoCount() = onUi { controller.history.stepCount - controller.history.stepIndex - 1 }

    private fun rect(x0: Float, y0: Float, x1: Float, y1: Float) {
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(x0 to y0, x1 to y1)
        assertTrue(onUi { controller.confirmShape() })
    }

    private fun multi(action: Int, down: Long, pts: List<Pair<Float, Float>>): MotionEvent {
        val props = Array(pts.size) { i -> MotionEvent.PointerProperties().apply { id = i; toolType = MotionEvent.TOOL_TYPE_FINGER } }
        val coords = Array(pts.size) { i -> MotionEvent.PointerCoords().apply { x = pts[i].first; y = pts[i].second; pressure = 1f; size = 1f } }
        return MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, pts.size, props, coords, 0, 0, 1f, 1f, 0, 0,
            InputDevice.SOURCE_TOUCHSCREEN, 0)
    }

    /** An erase drag along y from x0 to x1; with [secondFinger] a still second finger lands before the lift
     *  (held past the tap timeout so it is not a two-finger undo tap). No document read while a finger is down. */
    private fun eraseDrag(y: Float, x0: Float, x1: Float, secondFinger: Boolean = false) {
        onUi { controller.selectTool(GreaseTool.ERASE); controller.setEraserMode(EraserMode.HARD); controller.setEraserSize(40f) }
        val a = screen(x0, y); val b = screen(x1, y); val c = screen(600f, 650f)
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_DOWN, down, listOf(a)))
        var cur = a
        for (i in 1..20) {
            SystemClock.sleep(15)
            cur = a.first + (b.first - a.first) * i / 20f to a.second
            instrumentation.sendPointerSync(multi(MotionEvent.ACTION_MOVE, down, listOf(cur)))
        }
        if (secondFinger) {
            instrumentation.sendPointerSync(multi(MotionEvent.ACTION_POINTER_DOWN or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, listOf(cur, c)))
            SystemClock.sleep(500)
            instrumentation.sendPointerSync(multi(MotionEvent.ACTION_POINTER_UP or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, listOf(cur, c)))
        }
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_UP, down, listOf(cur)))
        rule.waitForIdle()
    }

    @Test fun eraseThroughOverlappingFillsKeepsOrderAndIsOneStep() {
        rect(200f, 200f, 700f, 500f)
        rect(500f, 250f, 1000f, 450f)
        assertTrue(onUi { controller.selectAll(); controller.fillSelectedStroke() })
        val before = allPoints()
        assertEquals(2, before.size)
        eraseDrag(350f, 300f, 900f)
        val after = allPoints()
        assertNotEquals("the fills were cut", before, after)
        val minX = after.map { s -> s.minOf { it[0] } }
        val fromA = minX.indices.filter { minX[it] < 350.0 }; val fromB = minX.indices.filter { minX[it] >= 450.0 }
        assertTrue("both fills remain: $minX", fromA.isNotEmpty() && fromB.isNotEmpty())
        assertTrue("the first fill stays below the second: $minX", fromA.max() < fromB.min())
        assertTrue(onUi { controller.undo() })
        assertEquals("one undo restores both fills exactly", before, allPoints())
        assertTrue(onUi { controller.redo() })
        assertEquals("one redo re-applies the cut", after, allPoints())
    }

    @Test fun lockedMaterialFillIsNotErased() {
        rect(300f, 200f, 900f, 500f)
        assertTrue(onUi { controller.selectAll(); controller.fillSelectedStroke() })
        val slot = strokes().single().getInt("material")
        assertTrue(onUi { controller.setMaterialLocked(slot, true) })
        val before = allPoints()
        eraseDrag(350f, 400f, 800f)
        assertEquals("a locked material's fill is untouched", before, allPoints())
    }

    @Test fun secondFingerCancelsTheErase() {
        rect(300f, 200f, 900f, 500f)
        assertTrue(onUi { controller.selectAll(); controller.fillSelectedStroke() })
        onUi { controller.selectTool(GreaseTool.DRAW) }
        line(350f)
        line(600f)
        assertTrue(onUi { controller.undo() }) // leaves a redo entry the cancel must keep
        val redo0 = redoCount()
        assertEquals(1, redo0)
        val before = signature()
        eraseDrag(350f, 250f, 950f, secondFinger = true)
        assertEquals("the document is the pre-erase document", before, signature())
        assertEquals("the redo count is unchanged", redo0, redoCount())
    }
}
