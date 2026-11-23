package com.smitnk.projectgrease

import android.os.SystemClock
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.EraserMode
import com.smitnk.projectgrease.editor.GreaseTool
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Final batch: a transform drag ends cleanly (one undo step, idle after), and content checks for the eraser preview. */
@RunWith(AndroidJUnit4::class)
class SweepL9FinalTest : SweepBase() {
    private fun allPoints(): List<List<List<Double>>> = strokes().map { s -> points(s).map { it.toList() } }

    /** Draws a line, drags it with [tool] along [path], and checks the gesture closed as exactly one undo step. */
    private fun transformDragEndsCleanly(tool: GreaseTool, vararg path: Pair<Float, Float>) {
        line(300f)
        val original = allPoints()
        assertEquals(1, original.size)
        onUi { controller.selectTool(tool) }
        val count0 = onUi { controller.history.stepCount }
        val index0 = onUi { controller.history.stepIndex }
        drag(*path)
        assertFalse("no history batch left open after lifting", onUi { controller.history.isBatching })
        val moved = allPoints()
        assertNotEquals("$tool changed the points", original, moved)
        assertEquals("$tool drag is one history step", count0 + 1, onUi { controller.history.stepCount })
        assertEquals("$tool drag moved the step index by one", index0 + 1, onUi { controller.history.stepIndex })

        // idle again: the next one-finger drag draws a new stroke and leaves the transformed one alone
        onUi { controller.selectTool(GreaseTool.DRAW) }
        line(500f)
        assertEquals("the drag after the transform draws", 2, strokes().size)
        assertEquals("the transformed stroke is untouched by the next drag", moved[0], allPoints()[0])

        assertTrue(onUi { controller.undo() }) // the new stroke
        assertEquals(1, strokes().size)
        assertTrue(onUi { controller.undo() }) // the whole transform drag at once
        assertEquals("one undo restores the original points", original, allPoints())
    }

    /** B2: lifting the finger after a Move drag closes the gesture. */
    @Test fun moveDragEndsCleanly() = transformDragEndsCleanly(GreaseTool.MOVE, 600f to 300f, 600f to 400f)

    /** B2: lifting the finger after a Rotate drag closes the gesture. */
    @Test fun rotateDragEndsCleanly() = transformDragEndsCleanly(GreaseTool.ROTATE, 900f to 300f, 600f to 600f)

    /** B2: a Move touch that hits nothing records nothing and leaves no batch open. */
    @Test fun transformMissRecordsNothing() {
        line(300f)
        onUi { controller.selectTool(GreaseTool.MOVE) }
        val count0 = onUi { controller.history.stepCount }
        val original = allPoints()
        drag(600f to 600f, 700f to 650f)
        assertFalse(onUi { controller.history.isBatching })
        assertEquals(count0, onUi { controller.history.stepCount })
        assertEquals(original, allPoints())
    }

    /** B5 (content): the live preview leaves the document alone; the lift cuts the eraser path out of the fill
     *  ("Erase fills like paint", v0.029/v0.038) as a hole, the outline keeps its corners, and undo restores the original points. */
    @Test fun eraserPreviewLeavesStrokeContentAlone() {
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(300f to 200f, 900f to 500f)
        assertTrue(onUi { controller.confirmShape() })
        assertTrue(onUi { controller.selectAll(); controller.fillSelectedStroke() })
        val before = allPoints()
        assertTrue("the rectangle has points", before.isNotEmpty() && before.all { it.isNotEmpty() })
        onUi { controller.selectTool(GreaseTool.ERASE); controller.setEraserMode(EraserMode.HARD); controller.setEraserSize(40f) }
        val a = screen(500f, 350f); val b = screen(700f, 350f)
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, a.first, a.second))
        for (i in 1..20) {
            SystemClock.sleep(15)
            val t = i / 20f
            instrumentation.sendPointerSync(event(MotionEvent.ACTION_MOVE, down, a.first + (b.first - a.first) * t, a.second))
        }
        // no document read while the finger is down: saving would end (and commit) the gesture
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, b.first, b.second))
        rule.waitForIdle()
        val after = allPoints().flatten()
        assertNotEquals("the fill was cut", before, allPoints())
        assertTrue("a hole loop lies along the eraser path", after.any { it[0] in 450.0..750.0 && it[1] in 300.0..400.0 })
        for ((cx, cy) in listOf(300.0 to 200.0, 900.0 to 200.0, 900.0 to 500.0, 300.0 to 500.0))
            assertTrue("the outline keeps the corner $cx,$cy", after.any { kotlin.math.abs(it[0] - cx) <= 3.0 && kotlin.math.abs(it[1] - cy) <= 3.0 })
        assertTrue(onUi { controller.undo() })
        assertEquals("undo restores the rectangle's points exactly", before, allPoints())
    }
}
