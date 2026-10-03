package com.smitnk.projectgrease

import android.graphics.Bitmap
import android.graphics.Color
import android.os.Build
import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.smitnk.projectgrease.editor.CanvasMapping
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTemplates
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.SculptBrush
import com.smitnk.projectgrease.nativebridge.ProjectGreaseDrawingSurfaceView
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/**
 * Device bugfix checks on an emulator: real touches go through the EGL surface into the native tool
 * session, the native document is read back (project JSON, selection count) and the screen is
 * captured. Screenshots land in the app's external files dir (screenshots/), pulled by CI as artifacts.
 */
@RunWith(AndroidJUnit4::class)
class DeviceBugfixTest {
    companion object { const val SHOT_DIR = "/data/local/tmp/pg_screenshots" }

    @get:Rule val rule = createAndroidComposeRule<MainActivity>()

    private val instrumentation get() = InstrumentationRegistry.getInstrumentation()
    private val controller: EditorController get() = rule.activity.controller

    @Before fun openNewProject() {
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

    // ---- helpers ----
    private fun <T> onUi(block: () -> T): T {
        var out: T? = null
        instrumentation.runOnMainSync { out = block() }
        @Suppress("UNCHECKED_CAST") return out as T
    }

    private fun surface(): View {
        fun find(v: View): View? {
            if (v is ProjectGreaseDrawingSurfaceView) return v
            if (v is ViewGroup) for (i in 0 until v.childCount) find(v.getChildAt(i))?.let { return it }
            return null
        }
        return onUi { find(rule.activity.window.decorView) } ?: error("EGL surface not found")
    }

    /** Canvas units -> screen pixels, as the surface maps touches (CanvasMapping). */
    private fun screen(cx: Float, cy: Float): Pair<Float, Float> {
        val v = surface()
        val loc = IntArray(2)
        val (w, h) = onUi { v.getLocationOnScreen(loc); v.width.toFloat() to v.height.toFloat() }
        val (x, y) = onUi {
            CanvasMapping.toView(cx, cy, w, h, controller.document.canvasWidth, controller.document.canvasHeight,
                controller.view.zoom, controller.view.panX, controller.view.panY)
        }
        return loc[0] + x to loc[1] + y
    }

    private fun event(action: Int, down: Long, x: Float, y: Float): MotionEvent =
        MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, x, y, 1f, 1f, 0, 1f, 1f, 0, 0).also {
            it.source = InputDevice.SOURCE_TOUCHSCREEN
        }

    /** A drag through canvas points, ~12 ms per step, as a finger would move. */
    private fun drag(vararg canvas: Pair<Float, Float>, steps: Int = 12) {
        val pts = canvas.map { screen(it.first, it.second) }
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, pts[0].first, pts[0].second))
        for (k in 1 until pts.size) {
            val a = pts[k - 1]
            val b = pts[k]
            for (s in 1..steps) {
                val t = s.toFloat() / steps
                SystemClock.sleep(12)
                instrumentation.sendPointerSync(event(MotionEvent.ACTION_MOVE, down,
                    a.first + (b.first - a.first) * t, a.second + (b.second - a.second) * t))
            }
        }
        val last = pts.last()
        instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, last.first, last.second))
        rule.waitForIdle()
    }

    private fun tap(cx: Float, cy: Float) = drag(cx to cy, steps = 1)

    private fun document(): JSONObject = JSONObject(onUi { controller.saveDocumentJson() } ?: error("no document"))

    /** Every stroke of the document (all layers, all frames). */
    private fun strokes(): List<JSONObject> {
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
        walk(document())
        return out
    }

    private fun screenshot(name: String): Bitmap {
        SystemClock.sleep(400)
        rule.waitForIdle()
        val bmp = instrumentation.uiAutomation.takeScreenshot() ?: error("screenshot failed")
        // Scoped storage (API 30) hides app external dirs from adb; the shell user writes
        // /data/local/tmp, which CI pulls as the screenshot artifact.
        shell("mkdir -p $SHOT_DIR")
        shell("screencap -p $SHOT_DIR/$name.png")
        return bmp
    }

    private fun shell(cmd: String) {
        instrumentation.uiAutomation.executeShellCommand(cmd).use { pfd ->
            java.io.FileInputStream(pfd.fileDescriptor).use { it.readBytes() } // wait for completion
        }
    }

    private fun isDark(c: Int) = Color.red(c) < 90 && Color.green(c) < 90 && Color.blue(c) < 90

    /** Length of the dark run crossing screen x at about y (the stroke's on-screen width). */
    private fun darkRun(bmp: Bitmap, x: Int, y: Int): Int {
        var top = y
        var bottom = y
        while (top > 0 && isDark(bmp.getPixel(x, top - 1))) top--
        while (bottom < bmp.height - 1 && isDark(bmp.getPixel(x, bottom + 1))) bottom++
        return if (isDark(bmp.getPixel(x, y))) bottom - top + 1 else 0
    }

    // ---- tests ----
    @Test fun topBarIsBelowTheStatusBar() {
        val top = rule.onNodeWithTag("editorTopBar").fetchSemanticsNode().boundsInWindow.top
        val inset = if (Build.VERSION.SDK_INT >= 30) onUi {
            rule.activity.window.decorView.rootWindowInsets.getInsets(WindowInsets.Type.statusBars()).top
        } else 0
        screenshot("01_top_bar_inset")
        assertTrue("status bar inset $inset", inset > 0)
        assertTrue("top bar at $top must start below the status bar ($inset)", top >= inset - 1)
    }

    @Test fun sizeSetsTheStrokeThickness() {
        onUi { controller.brushes.setSize(8f) }
        drag(200f to 200f, 1000f to 200f)
        onUi { controller.brushes.setSize(40f) }
        drag(200f to 450f, 1000f to 450f)
        val th = strokes().map { it.getDouble("thickness") }
        assertEquals(listOf(8.0, 40.0), th)
        val bmp = screenshot("02_size_8_and_40")
        val thin = screen(600f, 200f)
        val thick = screen(600f, 450f)
        val tw = darkRun(bmp, thin.first.toInt(), thin.second.toInt())
        val kw = darkRun(bmp, thick.first.toInt(), thick.second.toInt())
        assertTrue("thin $tw px, thick $kw px", tw in 1 until kw && kw >= tw * 3)
    }

    @Test fun strengthFullIsOpaque() {
        drag(200f to 300f, 1000f to 300f)
        val pts = strokes().single().getJSONArray("points")
        for (i in 0 until pts.length()) assertEquals(1.0, pts.getJSONArray(i).getDouble(4), 1e-4)
        val bmp = screenshot("03_strength_100")
        val p = screen(600f, 300f)
        val c = bmp.getPixel(p.first.toInt(), p.second.toInt())
        assertTrue("stroke pixel ${Integer.toHexString(c)} is grey", Color.red(c) < 40 && Color.green(c) < 40 && Color.blue(c) < 40)
    }

    @Test fun plusFrameAdvancesTheCounter() {
        rule.onNodeWithText("+ Frame").performClick()
        rule.waitForIdle()
        val end = onUi { controller.animation.timelineEnd }
        rule.onNodeWithText("Frame 2 / $end").assertExists()
        assertEquals(2, onUi { controller.animation.currentFrame })
        assertEquals(2, onUi { controller.animation.keyframes.size })
        screenshot("04_plus_frame")
    }

    @Test fun lassoSelectsAndHighlights() {
        drag(300f to 300f, 900f to 300f)
        onUi { controller.selectTool(GreaseTool.LASSO) }
        drag(250f to 200f, 950f to 200f, 950f to 400f, 250f to 400f, 250f to 200f)
        val selected = onUi { controller.selectedPointCount() }
        val bmp = screenshot("05_lasso_selection")
        assertTrue("selected points $selected", selected > 0)
        var orange = 0
        for (y in 0 until bmp.height step 2) for (x in 0 until bmp.width step 2) {
            val c = bmp.getPixel(x, y)
            if (Color.red(c) > 230 && Color.green(c) in 100..170 && Color.blue(c) < 40) orange++
        }
        assertTrue("edit overlay highlight pixels $orange", orange > 0)
    }

    @Test fun sculptSmoothKeepsEndpoints() {
        drag(200f to 300f, 400f to 260f, 600f to 340f, 800f to 260f, 1000f to 300f)
        val before = strokes().single().getJSONArray("points")
        onUi {
            controller.selectTool(GreaseTool.SCULPT)
            controller.sculpt.select(SculptBrush.SMOOTH)
            controller.brushes.setSize(80f)
        }
        drag(300f to 300f, 900f to 300f)
        drag(300f to 300f, 900f to 300f)
        val after = strokes().single().getJSONArray("points")
        screenshot("06_sculpt_smooth")
        assertEquals(before.length(), after.length())
        for (k in listOf(0, before.length() - 1)) {
            assertEquals(before.getJSONArray(k).getDouble(0), after.getJSONArray(k).getDouble(0), 1e-3)
            assertEquals(before.getJSONArray(k).getDouble(1), after.getJSONArray(k).getDouble(1), 1e-3)
        }
        var moved = 0
        for (i in 1 until before.length() - 1) {
            if (abs(before.getJSONArray(i).getDouble(1) - after.getJSONArray(i).getDouble(1)) > 1e-3) moved++
        }
        assertTrue("smooth moved $moved interior points", moved > 0)
    }

    @Test fun vertexPaintSetsVertColor() {
        drag(200f to 300f, 1000f to 300f)
        onUi {
            controller.setMode(GreaseMode.VERTEX_PAINT)
            controller.setMaterialColor(0xFFFF0000.toInt())
            controller.brushes.setSize(60f)
        }
        drag(300f to 300f, 900f to 300f)
        screenshot("07_vertex_paint")
        val pts = strokes().single().getJSONArray("points")
        var painted = 0
        for (i in 0 until pts.length()) {
            val p = pts.getJSONArray(i)
            if (p.length() > 9 && p.getDouble(9) > 0.0 && p.getDouble(6) > 0.0) painted++
        }
        assertTrue("points with vert_color $painted", painted > 0)
    }

    @Test fun weightPaintWritesWeights() {
        drag(200f to 300f, 1000f to 300f)
        onUi {
            controller.setMode(GreaseMode.WEIGHT_PAINT)
            controller.brushes.setSize(60f)
        }
        drag(300f to 300f, 900f to 300f)
        screenshot("08_weight_paint")
        val weights = strokes().single().optJSONArray("weights")
        var positive = 0
        if (weights != null) for (i in 0 until weights.length()) {
            val row = weights.getJSONArray(i)
            var k = 2
            while (k < row.length()) { if (row.getDouble(k) > 0.0) positive++; k += 2 }
        }
        assertTrue("weights > 0 on $positive points", positive > 0)
    }

    @Test fun fillInsideClosedRectangle() {
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(300f to 200f, 900f to 500f)
        val before = strokes().size
        onUi { controller.selectTool(GreaseTool.FILL) }
        tap(600f, 350f)
        screenshot("09_fill_rectangle")
        assertEquals(before + 1, strokes().size)
    }

    @Test fun pickSetsTheActiveColor() {
        // A material colour applies to every stroke of that slot (as in Blender): the red stroke gets
        // its own slot, then slot 0 (black) is made active before picking.
        onUi {
            controller.selectMaterial(1); controller.setMaterialColor(0xFFFF0000.toInt())
            controller.brushes.setSize(40f)
        }
        drag(200f to 300f, 1000f to 300f)
        onUi {
            controller.selectMaterial(0); controller.setMaterialColor(0xFF000000.toInt())
            controller.selectTool(GreaseTool.EYEDROPPER)
        }
        tap(600f, 300f)
        val picked = onUi { controller.materials.colorArgb }
        screenshot("10_pick")
        assertTrue("picked ${Integer.toHexString(picked)}",
            Color.red(picked) > 200 && Color.green(picked) < 60 && Color.blue(picked) < 60)
    }

    @Test fun newProjectTemplateLayers() {
        val expected = GreaseTemplates.byId("2d_animation")!!.layers
        val names = onUi { (0 until controller.layerCount()).map { controller.layerName(it) } }
        screenshot("11_new_project_layers")
        val trace = onUi { controller.setupLog.joinToString(" | ") }
        assertEquals("setup trace: $trace", expected, names)
    }

    @Test fun saveReloadIsIdentical() {
        drag(200f to 300f, 600f to 200f, 1000f to 300f)
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(300f to 400f, 700f to 600f)
        val first = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.loadDocumentJson(first) })
        val second = onUi { controller.saveDocumentJson() }!!
        screenshot("12_save_reload")
        assertEquals(first, second)
        assertEquals(2, strokes().size)
    }
}
