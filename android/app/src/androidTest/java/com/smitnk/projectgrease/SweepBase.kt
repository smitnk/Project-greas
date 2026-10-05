package com.smitnk.projectgrease

import android.graphics.Bitmap
import android.graphics.Color
import android.os.SystemClock
import android.util.Log
import android.view.InputDevice
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.platform.app.InstrumentationRegistry
import com.smitnk.projectgrease.editor.CanvasMapping
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.nativebridge.ProjectGreaseDrawingSurfaceView
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.rules.TestWatcher
import org.junit.runner.Description

/**
 * Shared base of the full emulator sweep. Canvas tools are driven by real touch events through the
 * EGL surface (UI -> controller -> JNI -> native tool session -> renderer); panel actions call the
 * same EditorController entry points their buttons call. Every test is checked against the native
 * document (saveDocumentJson / JNI queries) and against the screen (screencap pixels). A screenshot
 * is saved for every test, pass or fail, under /data/local/tmp/pg_screenshots/sweep/.
 */
abstract class SweepBase {
    companion object {
        const val SHOT_DIR = "/data/local/tmp/pg_screenshots/sweep"
        const val TAG = "PGSweep"
    }

    @get:Rule(order = 0) val rule = createAndroidComposeRule<MainActivity>()

    @get:Rule(order = 1) val evidence = object : TestWatcher() {
        override fun succeeded(d: Description) = save(d, "PASS")
        override fun failed(e: Throwable, d: Description) = save(d, "FAIL")
        private fun save(d: Description, result: String) {
            Log.i(TAG, "RESULT ${d.className.substringAfterLast('.')}.${d.methodName} $result")
            runCatching { shot("${d.className.substringAfterLast('.')}_${d.methodName}") }
        }
    }

    protected val instrumentation get() = InstrumentationRegistry.getInstrumentation()
    protected val controller: EditorController get() = rule.activity.controller

    @Before fun openNewProject() {
        // a system ANR dialog left by a slow emulator would own the focus and block event injection
        shell("am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS")
        rule.onNodeWithContentDescription("New").performClick()
        rule.onNodeWithText("Create Project").performScrollTo().performClick()
        rule.waitUntil(15_000) { onUi { controller.rendererReady } }
        rule.waitForIdle()
        onUi {
            controller.setMaterialColor(0xFF000000.toInt())
            controller.setBrushStrength(1f)
            controller.brushes.setSize(8f)
            controller.selectTool(GreaseTool.DRAW)
        }
    }

    // ---- driving ----
    protected fun <T> onUi(block: () -> T): T {
        var out: T? = null
        instrumentation.runOnMainSync { out = block() }
        @Suppress("UNCHECKED_CAST") return out as T
    }

    protected fun surface(): View {
        fun find(v: View): View? {
            if (v is ProjectGreaseDrawingSurfaceView) return v
            if (v is ViewGroup) for (i in 0 until v.childCount) find(v.getChildAt(i))?.let { return it }
            return null
        }
        return onUi { find(rule.activity.window.decorView) } ?: error("EGL surface not found")
    }

    /** Canvas units -> screen pixels, as the surface maps touches. */
    protected fun screen(cx: Float, cy: Float): Pair<Float, Float> {
        val v = surface()
        val loc = IntArray(2)
        val (w, h) = onUi { v.getLocationOnScreen(loc); v.width.toFloat() to v.height.toFloat() }
        val (x, y) = onUi {
            CanvasMapping.toView(cx, cy, w, h, controller.document.canvasWidth, controller.document.canvasHeight,
                controller.view.zoom, controller.view.panX, controller.view.panY)
        }
        return loc[0] + x to loc[1] + y
    }

    protected fun event(action: Int, down: Long, x: Float, y: Float, pressure: Float = 1f, pen: Boolean = false): MotionEvent {
        if (!pen) return MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, x, y, pressure, 1f, 0, 1f, 1f, 0, 0).also {
            it.source = InputDevice.SOURCE_TOUCHSCREEN
        }
        // A stylus: fingers always draw at pressure 1 (TouchInputRules), as a mouse does in Blender.
        val props = arrayOf(MotionEvent.PointerProperties().apply { id = 0; toolType = MotionEvent.TOOL_TYPE_STYLUS })
        val coords = arrayOf(MotionEvent.PointerCoords().apply { this.x = x; this.y = y; this.pressure = pressure; size = 1f })
        return MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, 1, props, coords, 0, 0, 1f, 1f, 0, 0,
            InputDevice.SOURCE_STYLUS or InputDevice.SOURCE_TOUCHSCREEN, 0)
    }

    /** A finger drag through canvas points (~12 ms per step). */
    protected fun drag(vararg canvas: Pair<Float, Float>, steps: Int = 12, pressure: Float = 1f, pen: Boolean = false) {
        val pts = canvas.map { screen(it.first, it.second) }
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, pts[0].first, pts[0].second, pressure, pen))
        for (k in 1 until pts.size) {
            val a = pts[k - 1]; val b = pts[k]
            for (s in 1..steps) {
                val t = s.toFloat() / steps
                SystemClock.sleep(12)
                instrumentation.sendPointerSync(event(MotionEvent.ACTION_MOVE, down,
                    a.first + (b.first - a.first) * t, a.second + (b.second - a.second) * t, pressure, pen))
            }
        }
        val last = pts.last()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, last.first, last.second, pressure, pen))
        rule.waitForIdle()
    }

    protected fun tap(cx: Float, cy: Float) = drag(cx to cy, steps = 1)

    /** A horizontal line stroke at canvas y. */
    protected fun line(y: Float, x0: Float = 200f, x1: Float = 1000f) = drag(x0 to y, x1 to y)

    /**
     * Dispatches a stroke of [n] moves straight to the surface on the main thread and returns the
     * per-event handling times in ms (input -> native session -> render -> swap: the frame time).
     */
    /** Draws one stroke of [n] MOVE samples and returns the time the surface took for each. With
     *  [intervalMs] > 0 the samples come at that pace (8 ms = a 120 Hz touch panel), as from a finger;
     *  0 injects them back to back. */
    protected fun timedStroke(n: Int, y: Float, x0: Float = 100f, x1: Float = 1100f, intervalMs: Long = 0): List<Double> {
        val v = surface()
        val a = screen(x0, y); val b = screen(x1, y)
        val loc = IntArray(2); onUi { v.getLocationOnScreen(loc) }
        val down = SystemClock.uptimeMillis()
        fun local(e: MotionEvent) = e.also { it.offsetLocation(-loc[0].toFloat(), -loc[1].toFloat()) }
        onUi { v.dispatchTouchEvent(local(event(MotionEvent.ACTION_DOWN, down, a.first, a.second))) }
        val times = ArrayList<Double>(n)
        for (i in 1..n) {
            val t = i.toFloat() / n
            val x = a.first + (b.first - a.first) * t
            val yy = a.second + 20f * kotlin.math.sin(i * 0.05f)
            val ev = local(event(MotionEvent.ACTION_MOVE, down, x, yy))
            times += onUi { val t0 = System.nanoTime(); v.dispatchTouchEvent(ev); (System.nanoTime() - t0) / 1e6 }
            if (intervalMs > 0) SystemClock.sleep(intervalMs)
        }
        onUi { v.dispatchTouchEvent(local(event(MotionEvent.ACTION_UP, down, b.first, b.second))) }
        rule.waitForIdle()
        return times
    }

    protected fun median(v: List<Double>) = v.sorted().let { if (it.isEmpty()) 0.0 else it[it.size / 2] }

    protected fun shell(cmd: String): String =
        instrumentation.uiAutomation.executeShellCommand(cmd).use { pfd ->
            java.io.FileInputStream(pfd.fileDescriptor).use { String(it.readBytes()) }
        }

    // ---- document state (JNI read APIs) ----
    protected fun document(): JSONObject = JSONObject(onUi { controller.saveDocumentJson() } ?: error("no document"))

    protected fun strokes(doc: JSONObject = document()): List<JSONObject> {
        val out = ArrayList<JSONObject>()
        fun walk(any: Any?) {
            when (any) {
                is JSONObject -> {
                    if (any.has("points") && any.has("thickness")) out += any
                    any.keys().forEach { walk(any.get(it)) }
                }
                is JSONArray -> for (i in 0 until any.length()) walk(any.get(i))
            }
        }
        walk(doc)
        return out
    }

    protected fun points(s: JSONObject): List<DoubleArray> {
        val p = s.getJSONArray("points")
        return (0 until p.length()).map { i -> val r = p.getJSONArray(i); DoubleArray(r.length()) { r.getDouble(it) } }
    }

    /** The saved document minus the current-frame cursor: what undo/redo must restore exactly. */
    protected fun signature(doc: JSONObject = document()): String = doc.also { it.remove("frame") }.toString()

    /**
     * Runs [op] (a document change) and checks it was recorded: the document changed, Undo restores
     * the exact previous geometry, Redo reapplies it.
     */
    protected fun undoable(name: String, op: () -> Boolean) {
        val before = signature()
        assertTrue("$name applied", onUi(op))
        val after = signature()
        assertNotEquals("$name changed the document", before, after)
        assertTrue("$name: undo", onUi { controller.undo() })
        assertEquals("$name: undo restores", before, signature())
        assertTrue("$name: redo", onUi { controller.redo() })
        assertEquals("$name: redo reapplies", after, signature())
    }

    /** Same as [undoable] for a gesture: [gesture] runs off the main thread (touch injection). */
    protected fun undoableGesture(name: String, gesture: () -> Unit) {
        val before = signature()
        gesture()
        val after = signature()
        assertNotEquals("$name changed the document", before, after)
        assertTrue("$name: undo", onUi { controller.undo() })
        assertEquals("$name: undo restores", before, signature())
        assertTrue("$name: redo", onUi { controller.redo() })
        assertEquals("$name: redo reapplies", after, signature())
    }

    // ---- screen ----
    protected fun shot(name: String): Bitmap {
        SystemClock.sleep(300)
        rule.waitForIdle()
        val bmp = instrumentation.uiAutomation.takeScreenshot() ?: error("screenshot failed")
        shell("mkdir -p $SHOT_DIR")
        shell("screencap -p $SHOT_DIR/$name.png")
        return bmp
    }

    protected fun isDark(c: Int) = Color.red(c) < 90 && Color.green(c) < 90 && Color.blue(c) < 90

    /** Pixel of the screen at a canvas point. */
    protected fun pixel(bmp: Bitmap, cx: Float, cy: Float): Int {
        val p = screen(cx, cy)
        return bmp.getPixel(p.first.toInt().coerceIn(0, bmp.width - 1), p.second.toInt().coerceIn(0, bmp.height - 1))
    }

    /** Any dark pixel within [r] screen px of the canvas point. */
    protected fun inkNear(bmp: Bitmap, cx: Float, cy: Float, r: Int = 6, test: (Int) -> Boolean = ::isDark): Boolean {
        val p = screen(cx, cy)
        for (dy in -r..r) for (dx in -r..r) {
            val x = p.first.toInt() + dx; val y = p.second.toInt() + dy
            if (x in 0 until bmp.width && y in 0 until bmp.height && test(bmp.getPixel(x, y))) return true
        }
        return false
    }

    protected fun assertInk(bmp: Bitmap, cx: Float, cy: Float, what: String) =
        assertTrue("$what visible at ($cx,$cy)", inkNear(bmp, cx, cy))

    protected fun assertNoInk(bmp: Bitmap, cx: Float, cy: Float, what: String) =
        assertTrue("$what absent at ($cx,$cy)", !inkNear(bmp, cx, cy, 2))

    /** Count of screen pixels matching [test] over the canvas surface. */
    protected fun countPixels(bmp: Bitmap, step: Int = 3, test: (Int) -> Boolean): Int {
        val v = surface(); val loc = IntArray(2)
        val (w, h) = onUi { v.getLocationOnScreen(loc); v.width to v.height }
        var n = 0
        var y = loc[1]
        while (y < minOf(bmp.height, loc[1] + h)) {
            var x = loc[0]
            while (x < minOf(bmp.width, loc[0] + w)) { if (test(bmp.getPixel(x, y))) n++; x += step }
            y += step
        }
        return n
    }

    /** The most frequent colours within [r] screen px of a canvas point (failure diagnostics). */
    protected fun colorsNear(bmp: Bitmap, cx: Float, cy: Float, r: Int = 20): String {
        val p = screen(cx, cy)
        val hist = HashMap<Int, Int>()
        for (dy in -r..r) for (dx in -r..r) {
            val x = p.first.toInt() + dx; val y = p.second.toInt() + dy
            if (x in 0 until bmp.width && y in 0 until bmp.height) hist.merge(bmp.getPixel(x, y), 1, Int::plus)
        }
        return "screen ${p.first.toInt()},${p.second.toInt()}: " +
            hist.entries.sortedByDescending { it.value }.take(5).joinToString { "#" + Integer.toHexString(it.key) + "x" + it.value }
    }

    protected fun selectAllStrokes() = onUi { controller.selectAll() }
}
