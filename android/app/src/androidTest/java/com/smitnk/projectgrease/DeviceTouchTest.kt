package com.smitnk.projectgrease

import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin

/** Touch tools on the device: multi-finger view gestures and taps, and the lasso actions. */
@RunWith(AndroidJUnit4::class)
class DeviceTouchTest : SweepBase() {
    private fun multi(action: Int, down: Long, pts: List<Pair<Float, Float>>): MotionEvent {
        val props = Array(pts.size) { i -> MotionEvent.PointerProperties().apply { id = i; toolType = MotionEvent.TOOL_TYPE_FINGER } }
        val coords = Array(pts.size) { i -> MotionEvent.PointerCoords().apply { x = pts[i].first; y = pts[i].second; pressure = 1f; size = 1f } }
        return MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, pts.size, props, coords, 0, 0, 1f, 1f, 0, 0,
            InputDevice.SOURCE_TOUCHSCREEN, 0)
    }

    /** n fingers down one after the other, moved along [path] (per-step offsets of each finger), then up. */
    private fun fingers(start: List<Pair<Float, Float>>, steps: Int = 0, move: (Int, Int, Float) -> Pair<Float, Float> = { i, _, _ -> start[i] }) {
        val down = SystemClock.uptimeMillis()
        val inst = instrumentation
        inst.sendPointerSync(multi(MotionEvent.ACTION_DOWN, down, start.take(1)))
        for (k in 2..start.size) {
            inst.sendPointerSync(multi(MotionEvent.ACTION_POINTER_DOWN or ((k - 1) shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, start.take(k)))
        }
        var cur = start
        for (s in 1..steps) {
            SystemClock.sleep(12)
            cur = start.indices.map { move(it, s, s.toFloat() / steps) }
            inst.sendPointerSync(multi(MotionEvent.ACTION_MOVE, down, cur))
        }
        for (k in start.size downTo 2) {
            inst.sendPointerSync(multi(MotionEvent.ACTION_POINTER_UP or ((k - 1) shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, cur.take(k)))
        }
        inst.sendPointerSync(multi(MotionEvent.ACTION_UP, down, cur.take(1)))
        rule.waitForIdle()
    }

    @Test fun twoFingerTapUndoesThreeFingerTapRedoes() {
        line(300f)
        assertEquals(1, strokes().size)
        val a = screen(500f, 500f); val b = screen(700f, 500f); val c = screen(600f, 600f)
        val canUndo = onUi { controller.history.canUndo }
        fingers(listOf(a, b))
        val log = onUi { "canUndo before $canUndo after ${controller.history.canUndo} redo ${controller.history.canRedo} view ${controller.view.zoom} ${controller.view.rotation}" }
        assertEquals("two-finger tap undoes the stroke ($log)", 0, strokes().size)
        fingers(listOf(a, b, c))
        assertEquals("three-finger tap redoes it", 1, strokes().size)
    }

    @Test fun twoFingersZoomPanAndRotate() {
        val z0 = onUi { controller.view.zoom }
        val a = screen(500f, 400f); val b = screen(700f, 400f)
        val cx = (a.first + b.first) / 2; val cy = (a.second + b.second) / 2
        val r0 = (b.first - a.first) / 2
        // spread to 1.5x, turn 30 degrees, move 60 px down
        fingers(listOf(a, b), steps = 20) { i, _, t ->
            val r = r0 * (1f + 0.5f * t); val ang = (if (i == 0) Math.PI.toFloat() else 0f) + 0.5236f * t
            cx + r * cos(ang) to cy + 60f * t + r * sin(ang)
        }
        val v = onUi { Triple(controller.view.zoom, controller.view.rotation, controller.view.panY) }
        assertEquals("zoom follows the spread", z0 * 1.5f, v.first, 0.15f)
        assertEquals("view turns with the fingers", 0.5236f, v.second, 0.08f)
        assertTrue("view pans with the fingers (${v.third})", abs(v.third) > 20f)
        assertEquals("a view gesture draws nothing", 0, strokes().size)
        // drawing still lands where the finger is on the rotated view (near the centre: the zoomed
        // canvas edges are off screen)
        line(340f, 540f, 760f)
        val p = points(strokes().single())
        assertEquals(300.0, p[p.size / 2][1], 6.0)
        shot("touch_rotated_view")
    }

    @Test fun lassoBreakOffCopyCutPaste() {
        line(300f)
        // point select (Blender's vertex select mode): break off takes the selected points
        onUi { controller.setSelectMode(com.smitnk.projectgrease.editor.ProjectGreaseSelect.MODE_POINT) }
        val boxed = onUi { controller.selectBox(500f, 250f, 700f, 350f) }
        val selected = onUi { com.smitnk.projectgrease.nativebridge.GPNative.nativeSelectedPointCount(controller.nativeHandle) }
        val split = onUi { controller.breakOffSelection() }
        val native = onUi { com.smitnk.projectgrease.nativebridge.GPNative.nativeStrokeCount(controller.nativeHandle) }
        assertTrue("the selected part is its own stroke now: doc ${strokes().size}, native $native, box $boxed, selected $selected, split $split",
            strokes().size >= 2)
        val before = strokes().size
        onUi { controller.selectAll() }
        assertTrue("cut", onUi { controller.cutSelection() })
        assertEquals("cut removes the selection", 0, strokes().size)
        assertTrue("paste", onUi { controller.pasteStrokes() })
        assertEquals("paste brings it back", before, strokes().size)
    }

    @Test fun lassoShearAndPerspective() {
        line(300f); line(500f)
        onUi { controller.selectAll() }
        fun xs(i: Int) = points(strokes()[i]).map { it[0] }
        val topW0 = xs(0).maxOrNull()!! - xs(0).minOrNull()!!
        assertTrue("perspective", onUi { controller.perspectiveSelection(0.3f, 0f) })
        val topW1 = xs(0).maxOrNull()!! - xs(0).minOrNull()!!
        assertEquals("perspective narrows the top edge by 30%", topW0 * 0.7, topW1, topW0 * 0.05)
        val x0 = points(strokes()[1])[0][0]
        assertTrue("shear", onUi { controller.shearSelection(0.2f, 0f) })
        assertTrue("shear moves the bottom stroke sideways", abs(points(strokes()[1])[0][0] - x0) > 5)
        assertTrue("undo", onUi { controller.undo() })
    }
}
