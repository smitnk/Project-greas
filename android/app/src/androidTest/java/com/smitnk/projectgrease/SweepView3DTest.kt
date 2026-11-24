package com.smitnk.projectgrease

import android.graphics.Bitmap
import android.os.SystemClock
import android.util.Log
import android.view.MotionEvent
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.nativebridge.GPNative
import com.smitnk.projectgrease.nativebridge.View3DNative
import com.smitnk.projectgrease.nativebridge.View3DUi
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/** The 3D viewport (Blender 3.6.23 view3d navigation, overlay grid, GP drawing planes): orbit, the
 *  numpad views, the persp toggle, drawing on each plane, and the Camera view drawing exactly the 2D
 *  canvas it replaces. */
@RunWith(AndroidJUnit4::class)
class SweepView3DTest : SweepBase() {
    private fun state() = onUi { View3DNative.nativeState() }
    private fun on3d(on: Boolean) = onUi { View3DUi.draw = false; View3DUi.setActive(on, controller) }.also { rule.waitForIdle() }
    private fun tag(t: String) { rule.onNodeWithTag(t).performClick(); rule.waitForIdle() }

    @After fun back2d() = onUi { View3DUi.draw = false; View3DUi.setActive(false, controller); View3DNative.nativeReset() }

    /** Surface-local -> screen. */
    private fun onScreen(x: Float, y: Float): Pair<Float, Float> {
        val loc = IntArray(2); val v = surface(); onUi { v.getLocationOnScreen(loc) }
        return loc[0] + x to loc[1] + y
    }
    private fun surfaceSize(): Pair<Float, Float> { val v = surface(); return onUi { v.width.toFloat() to v.height.toFloat() } }

    /** A one-finger drag between screen points, [ms] long, one move per ~16 ms; returns the 3D frame
     *  times read after each move. */
    private fun fingerDrag(a: Pair<Float, Float>, b: Pair<Float, Float>, ms: Long): List<Float> {
        val down = SystemClock.uptimeMillis()
        val times = ArrayList<Float>()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, a.first, a.second))
        val n = (ms / 16).toInt().coerceAtLeast(2)
        for (i in 1..n) {
            SystemClock.sleep(16)
            val t = i.toFloat() / n
            instrumentation.sendPointerSync(event(MotionEvent.ACTION_MOVE, down, a.first + (b.first - a.first) * t, a.second + (b.second - a.second) * t))
            times += state()[10]
        }
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, b.first, b.second))
        rule.waitForIdle()
        return times
    }

    /** 24 keyframes, 4 strokes of 52 points each: 4992 points. */
    private fun build24Frames(): Int {
        val h = onUi { controller.nativeHandle }
        val cw = onUi { controller.document.canvasWidth.toFloat() }; val ch = onUi { controller.document.canvasHeight.toFloat() }
        for (f in 1..24) {
            onUi { controller.createFrame(f); controller.selectFrame(f) }
            for (s in 0 until 4) {
                val n = 52
                val pts = FloatArray(n * 6)
                for (i in 0 until n) {
                    val t = i / (n - 1f)
                    pts[i * 6] = cw * (0.1f + 0.8f * t)
                    pts[i * 6 + 1] = ch * (0.2f + 0.15f * s) + 40f * kotlin.math.sin(t * 6.28f + f * 0.3f)
                    pts[i * 6 + 3] = 1f; pts[i * 6 + 4] = 1f; pts[i * 6 + 5] = t
                }
                assertTrue(onUi { GPNative.nativeAddStroke(h, pts, n, floatArrayOf(0f, 20f, 0f, 0f, 0f, 0f, 0f, 0f), FloatArray(n * 4)) })
            }
        }
        onUi { controller.selectFrame(1); controller.render() }
        return 24 * 4 * 52
    }

    @Test fun orbitThreeSecondsOver24Frames() {
        val points = build24Frames()
        on3d(true)
        val q0 = state().copyOfRange(3, 7).toList()
        val (w, hgt) = surfaceSize()
        val times = fingerDrag(onScreen(w * 0.3f, hgt * 0.5f), onScreen(w * 0.7f, hgt * 0.4f), 3000)
        times.forEachIndexed { i, ms -> Log.i("ProjectGrease", "view3d orbit frame $i ${"%.2f".format(ms)} ms") }
        val sorted = times.sorted()
        Log.i("ProjectGrease", "view3d orbit: $points points, ${times.size} moves, median ${sorted[sorted.size / 2]} ms, max ${sorted.last()} ms")
        assertNotEquals("the finger orbited the view", q0, state().copyOfRange(3, 7).toList())
        assertTrue("3D frames were drawn", times.any { it > 0f })
    }

    @Test fun numpadViewsAndPerspToggle() {
        on3d(true)
        tag("view3dFront"); assertEquals(View3DNative.VIEW_FRONT.toFloat(), state()[1], 0f); assertEquals(0f, state()[0], 0f) // auto ortho
        tag("view3dRight"); assertEquals(View3DNative.VIEW_RIGHT.toFloat(), state()[1], 0f)
        tag("view3dTop"); assertEquals(View3DNative.VIEW_TOP.toFloat(), state()[1], 0f)
        tag("view3dPersp"); assertEquals(1f, state()[0], 0f)
        tag("view3dPersp"); assertEquals(0f, state()[0], 0f)
        // orbiting leaves the axis view, in perspective again (ED_view3d_persp_ensure)
        val (w, h) = surfaceSize()
        fingerDrag(onScreen(w * 0.4f, h * 0.5f), onScreen(w * 0.6f, h * 0.5f), 300)
        assertEquals(0f, state()[1], 0f); assertEquals(1f, state()[0], 0f)
        tag("view3dToggle")
        assertFalse("0 Camera returns to the 2D canvas", onUi { View3DNative.nativeActive() })
        tag("view3dToggle")
        assertTrue("one tap back to 3D", onUi { View3DNative.nativeActive() })
    }

    @Test fun twoFingersPanAndPinch() {
        on3d(true)
        val s0 = state()
        val (w, h) = surfaceSize()
        val a = onScreen(w * 0.4f, h * 0.5f); val b = onScreen(w * 0.6f, h * 0.5f)
        val down = SystemClock.uptimeMillis()
        fun two(action: Int, d: Float, dy: Float): MotionEvent {
            val props = Array(2) { i -> MotionEvent.PointerProperties().apply { id = i; toolType = MotionEvent.TOOL_TYPE_FINGER } }
            val coords = arrayOf(MotionEvent.PointerCoords().apply { x = a.first - d; y = a.second + dy; pressure = 1f; size = 1f },
                MotionEvent.PointerCoords().apply { x = b.first + d; y = b.second + dy; pressure = 1f; size = 1f })
            return MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, 2, props, coords, 0, 0, 1f, 1f, 0, 0, android.view.InputDevice.SOURCE_TOUCHSCREEN, 0)
        }
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, a.first, a.second))
        instrumentation.sendPointerSync(two(MotionEvent.ACTION_POINTER_DOWN or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), 0f, 0f))
        for (i in 1..20) { SystemClock.sleep(16); instrumentation.sendPointerSync(two(MotionEvent.ACTION_MOVE, i * 5f, i * 4f)) }
        instrumentation.sendPointerSync(two(MotionEvent.ACTION_POINTER_UP or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), 100f, 80f))
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, a.first - 100f, a.second + 80f))
        rule.waitForIdle()
        val s1 = state()
        assertTrue("pinch apart zooms in: dist ${s0[2]} -> ${s1[2]}", s1[2] < s0[2])
        assertNotEquals("the view centre moved", s0.copyOfRange(7, 10).toList(), s1.copyOfRange(7, 10).toList())
        assertEquals("pan / zoom do not rotate", s0.copyOfRange(3, 7).toList(), s1.copyOfRange(3, 7).toList())
    }

    /** One finger draws (Draw) on each plane: the points land on the plane and under the finger. */
    @Test fun drawOnEachPlane() {
        on3d(true)
        onUi { View3DUi.draw = true }
        val cw = onUi { controller.document.canvasWidth.toFloat() }; val ch = onUi { controller.document.canvasHeight.toFloat() }
        val h = onUi { controller.nativeHandle }
        for (lock in listOf(View3DNative.LOCK_Y, View3DNative.LOCK_Z, View3DNative.LOCK_X, View3DNative.LOCK_VIEW, View3DNative.LOCK_CURSOR)) {
            onUi { View3DUi.lock = lock; View3DNative.nativeSetPlacement(View3DUi.placement, lock) }
            val before = onUi { GPNative.nativeStrokeCount(h) }
            drag(cw * 0.35f to ch * 0.45f, cw * 0.65f to ch * 0.55f, steps = 20)
            assertEquals("plane $lock: one stroke", before + 1, onUi { GPNative.nativeStrokeCount(h) })
            val p = onUi { GPNative.nativeGetPoint(h, before, 0) }!!
            Log.i("ProjectGrease", "view3d plane $lock first point ${p[0]} ${p[1]} ${p[2]}")
            when (lock) {
                View3DNative.LOCK_Y -> assertTrue("Front X-Z: on the canvas plane", abs(p[2]) < 0.5f)
                View3DNative.LOCK_Z -> assertTrue("Top X-Y: at the floor", abs(p[1] - ch * 0.5f) < 0.5f)
                View3DNative.LOCK_X -> assertTrue("Side Y-Z: at X = 0", abs(p[0] - cw * 0.5f) < 0.5f)
            }
            // under the finger: the 3D view shows the first point where the touch began
            val s = onUi { View3DNative.nativeCanvasToScreen(p[0], p[1], p[2]) }
            if (s != null && lock != View3DNative.LOCK_X) {
                val loc = IntArray(2); val v = surface(); onUi { v.getLocationOnScreen(loc) }
                val start = screen(cw * 0.35f, ch * 0.45f)
                assertTrue("plane $lock: under the finger (${loc[0] + s[0]},${loc[1] + s[1]} vs $start)",
                    abs(loc[0] + s[0] - start.first) < 4f && abs(loc[1] + s[1] - start.second) < 4f)
            }
        }
    }

    @Test fun objectModeSelectAndMove() {
        on3d(true)
        assertEquals(1, onUi { View3DNative.nativeObjectCount() })
        val empty = onUi { View3DNative.nativeAddObject(View3DNative.OB_EMPTY) }
        tag("view3dAddArmature")
        assertEquals(3, onUi { View3DNative.nativeObjectCount() })
        val o = onUi { View3DNative.nativeObject(empty) }!!
        assertEquals(View3DNative.OB_EMPTY.toFloat(), o[0], 0f)
        // the GP origin (canvas centre): drag it, the GP object moves and is the only selection
        val cw = onUi { controller.document.canvasWidth.toFloat() }; val ch = onUi { controller.document.canvasHeight.toFloat() }
        // the empty and the armature sit on the cursor, at the GP origin: move the view a little so a drag
        // starting there picks the nearest (any of them is an origin under the finger)
        val s = onUi { View3DNative.nativeCanvasToScreen(cw / 2, ch / 2, 0f) }!!
        val a = onScreen(s[0], s[1])
        fingerDrag(a, a.first + 120f to a.second, 400)
        val moved = (0 until 3).map { onUi { View3DNative.nativeObject(it) }!! }.filter { it[4] == 1f }
        assertEquals("one object selected", 1, moved.size)
        assertTrue("it moved", abs(moved[0][1]) + abs(moved[0][2]) + abs(moved[0][3]) > 1e-4f)
        // a tap on empty space deselects all and places the 3D cursor
        val (w, h) = surfaceSize()
        val t = onScreen(w * 0.1f, h * 0.15f)
        fingerDrag(t, t, 16)
        assertTrue("deselected", (0 until 3).all { onUi { View3DNative.nativeObject(it) }!![4] == 0f })
    }

    /** The Camera view is today's 2D canvas, pixel for pixel. */
    @Test fun cameraViewMatchesThe2dView() {
        line(300f); line(500f, 300f, 900f)
        val before = shot("view3d_camera_before")
        on3d(true)
        val three = shot("view3d_3d")
        on3d(false)
        val after = shot("view3d_camera_after")
        val v = surface(); val loc = IntArray(2); onUi { v.getLocationOnScreen(loc) }
        val (w, h) = surfaceSize()
        // the canvas area above the bar (the bar's label changes)
        fun region(b: Bitmap) = IntArray(w.toInt() * (h * 0.7f).toInt()).also { b.getPixels(it, 0, w.toInt(), loc[0], loc[1], w.toInt(), (h * 0.7f).toInt()) }
        val r0 = region(before); val r1 = region(after)
        val diff = r0.indices.count { r0[it] != r1[it] }
        assertEquals("Camera view pixels equal the 2D view", 0, diff)
        val r3 = region(three)
        assertTrue("the 3D view is a different picture", r3.indices.count { r3[it] != r0[it] } > 1000)
    }
}
