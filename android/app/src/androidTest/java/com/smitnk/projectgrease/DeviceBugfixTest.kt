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
        val inset = onUi {
            val insets = rule.activity.window.decorView.rootWindowInsets
            if (Build.VERSION.SDK_INT >= 30) insets.getInsets(WindowInsets.Type.statusBars()).top
            else @Suppress("DEPRECATION") insets.systemWindowInsetTop // API 23-29 (no Type below 30)
        }
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
        assertTrue(onUi { controller.confirmShape() })
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
        assertTrue(onUi { controller.confirmShape() })
        val first = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.loadDocumentJson(first) })
        val second = onUi { controller.saveDocumentJson() }!!
        screenshot("12_save_reload")
        assertEquals(first, second)
        assertEquals(2, strokes().size)
    }

    // ---- edit5 batch ----
    @Test fun outlineMakesAClosedStroke() {
        drag(200f to 300f, 600f to 250f, 1000f to 300f)
        val ok = onUi { controller.selectAll(); controller.outlineSelection(2) }
        val stroke = strokes().single()
        screenshot("13_outline")
        assertTrue("outline applied", ok)
        assertTrue("outline is closed", stroke.getBoolean("cyclic"))
        assertEquals(2.0, stroke.getDouble("thickness"), 1e-6)
        assertTrue("perimeter has both sides", stroke.getJSONArray("points").length() > 8)
    }

    @Test fun gifExportDecodes() {
        drag(200f to 300f, 1000f to 300f)
        val applied = onUi {
            val s = com.smitnk.projectgrease.editor.ProjectSettings().apply {
                width = controller.document.canvasWidth; height = controller.document.canvasHeight
                fps = 12; frameStart = 1; frameEnd = 3
            }
            controller.applyProjectSettings(s)
        }
        assertEquals(null, applied)
        val out = java.io.ByteArrayOutputStream()
        var frames = 0
        val ok = onUi {
            val gif = com.smitnk.projectgrease.editor.GifEncoder(out, controller.document.canvasWidth, controller.document.canvasHeight, 12)
            gif.begin()
            val r = controller.renderExportFrames(false) { _, px -> gif.addFrame(px); frames++ }
            gif.finish(); r
        }
        val bytes = out.toByteArray()
        java.io.File(rule.activity.cacheDir, "export.gif").writeBytes(bytes)
        shell("cp ${rule.activity.cacheDir}/export.gif $SHOT_DIR/14_export.gif")
        assertTrue("frames rendered", ok && frames == 3)
        assertEquals("GIF89a", String(bytes, 0, 6, Charsets.US_ASCII))
        val bmp = android.graphics.BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
        assertTrue("GIF decodes", bmp != null)
        assertEquals(onUi { controller.document.canvasWidth }, bmp!!.width)
    }

    @Test fun projectSettingsSurviveSaveAndLoad() {
        val err = onUi {
            controller.applyProjectSettings(com.smitnk.projectgrease.editor.ProjectSettings().apply {
                width = 1280; height = 720; fps = 12; frameStart = 2; frameEnd = 48
                background = 0xFF223344.toInt(); transparentBackground = true
            })
        }
        assertEquals(null, err)
        val json = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.resetDocument() })
        assertTrue(onUi { controller.loadDocumentJson(json) })
        val s = onUi { controller.projectSettings }
        screenshot("15_project_settings")
        assertEquals(listOf(1280, 720, 12, 2, 48), listOf(s.width, s.height, s.fps, s.frameStart, s.frameEnd))
        assertEquals(0xFF223344.toInt(), s.background)
        assertTrue(s.transparentBackground)
        assertEquals(1280, onUi { controller.document.canvasWidth })
        assertEquals(48, onUi { controller.animation.timelineEnd })
    }

    // ---- edit6 batch ----
    @Test fun assignWeightsSaved() {
        drag(200f to 300f, 1000f to 300f)
        val ok = onUi {
            controller.addVertexGroup("Group")
            controller.selectAll()
            controller.assignSelectionToGroup(0.5f)
        }
        assertTrue("assign applied", ok)
        val json = onUi { controller.saveDocumentJson() }!!
        screenshot("16_vgroup_assign")
        val weights = JSONObject(json).let { doc ->
            var found: JSONArray? = null
            fun walk(any: Any?) {
                when (any) {
                    is JSONObject -> { if (any.has("points") && any.has("weights")) found = any.getJSONArray("weights"); any.keys().forEach { walk(any.get(it)) } }
                    is JSONArray -> for (i in 0 until any.length()) walk(any.get(i))
                }
            }
            walk(doc); found
        }
        assertTrue("weights saved", weights != null && weights.length() > 0)
        var half = 0
        for (i in 0 until weights!!.length()) {
            val row = weights.getJSONArray(i)
            var k = 2
            while (k < row.length()) { if (abs(row.getDouble(k) - 0.5) < 1e-4) half++; k += 2 }
        }
        assertTrue("points with weight 0.5: $half", half > 0)
    }

    @Test fun mergeDownKeepsStrokes() {
        assertTrue(onUi { controller.layerCount() } >= 2)
        onUi { controller.selectLayer(0) }
        drag(200f to 250f, 1000f to 250f)
        onUi { controller.selectLayer(1) }
        drag(200f to 450f, 1000f to 450f)
        val before = onUi { controller.layerCount() }
        assertTrue("merge applied", onUi { controller.mergeLayerDown() })
        val bmp = screenshot("17_merge_down")
        assertEquals(before - 1, onUi { controller.layerCount() })
        assertEquals(0, onUi { controller.selectedLayer })
        assertEquals(2, strokes().size)
        for (y in listOf(250f, 450f)) {
            val p = screen(600f, y)
            assertTrue("stroke at $y visible", darkRun(bmp, p.first.toInt(), p.second.toInt()) > 0)
        }
    }

    @Test fun isolateHidesOtherLayers() {
        assertTrue(onUi { controller.layerCount() } >= 2)
        onUi { controller.selectLayer(0) }
        drag(200f to 250f, 1000f to 250f)
        onUi { controller.selectLayer(1) }
        drag(200f to 450f, 1000f to 450f)
        assertTrue("isolate applied", onUi { controller.isolateLayer() })
        val bmp = screenshot("18_isolate")
        val hidden = screen(600f, 250f)
        val shown = screen(600f, 450f)
        assertEquals("other layer hidden", 0, darkRun(bmp, hidden.first.toInt(), hidden.second.toInt()))
        assertTrue("active layer visible", darkRun(bmp, shown.first.toInt(), shown.second.toInt()) > 0)
    }

    // ---- batch 21 ----
    @Test fun guideSnapsDrawnStroke() {
        onUi { controller.view.setDrawingGuide(2, 640f, 360f, 0f, 40f); controller.render() } // parallel, horizontal
        drag(200f to 300f, 400f to 260f, 600f to 360f, 800f to 240f, 1000f to 330f)
        val pts = strokes().single().getJSONArray("points")
        val y0 = pts.getJSONArray(0).getDouble(1)
        var worst = 0.0
        for (i in 0 until pts.length()) worst = maxOf(worst, abs(pts.getJSONArray(i).getDouble(1) - y0))
        val bmp = screenshot("19_guide_parallel")
        onUi { controller.view.setDrawingGuide(-1); controller.render() }
        assertTrue("points left the guide line by $worst", worst < 0.5)
        val p = screen(600f, y0.toFloat())
        assertTrue("snapped stroke drawn", darkRun(bmp, p.first.toInt(), p.second.toInt()) > 0)
    }

    @Test fun layerBlendAndTintPixels() {
        assertTrue(onUi { controller.layerCount() } >= 2)
        onUi { controller.selectLayer(0); controller.selectMaterial(0); controller.setMaterialColor(0xFFFFFF00.toInt()); controller.brushes.setSize(40f) }
        drag(200f to 300f, 800f to 300f)
        onUi { controller.selectLayer(1); controller.selectMaterial(1); controller.setMaterialColor(0xFF00FFFF.toInt()) }
        drag(500f to 300f, 1100f to 300f)
        assertTrue(onUi { controller.setLayerBlend(4, 1) }) // Multiply
        val bmp = screenshot("20_layer_multiply")
        val overlap = screen(650f, 300f)
        val c = bmp.getPixel(overlap.first.toInt(), overlap.second.toInt())
        assertTrue("yellow x cyan = green, got ${Integer.toHexString(c)}", Color.red(c) < 70 && Color.green(c) > 150 && Color.blue(c) < 70)
        assertTrue(onUi { controller.setLayerTint(0xFFFF0000.toInt(), 1f, 0) }) // bottom layer fully red
        val bmp2 = screenshot("21_layer_tint")
        val only = screen(300f, 300f)
        val t = bmp2.getPixel(only.first.toInt(), only.second.toInt())
        assertTrue("tinted red, got ${Integer.toHexString(t)}", Color.red(t) > 180 && Color.green(t) < 70 && Color.blue(t) < 70)
        val json = document()
        val layers = json.getJSONArray("layers")
        assertEquals(4, layers.getJSONObject(1).optInt("blend"))
        assertEquals(1.0, layers.getJSONObject(0).getJSONArray("tint").getDouble(3), 1e-6)
    }

    @Test fun keyframeTypeSavedAndMarked() {
        assertTrue(onUi { controller.createFrame(5) })
        assertTrue(onUi { controller.setFrameKeyType(5, com.smitnk.projectgrease.editor.ProjectGreaseSelect.KEY_BREAKDOWN) })
        rule.waitForIdle()
        rule.onNodeWithTag("keyMark_5_2").assertExists()
        screenshot("22_keyframe_breakdown")
        val raw = onUi { controller.saveDocumentJson() }!!
        val layer = JSONObject(raw).getJSONArray("layers").getJSONObject(onUi { controller.selectedLayer })
        var found = -1
        val frames = layer.getJSONArray("frames")
        for (i in 0 until frames.length()) if (frames.getJSONObject(i).getInt("number") == 5) found = frames.getJSONObject(i).optInt("keyType", 0)
        assertEquals(2, found)
        assertTrue(onUi { controller.loadDocumentJson(raw) })
        onUi { controller.selectLayer(controller.layerCount() - 1) }
        assertEquals(2, onUi { controller.animation.frameNumbers(); controller.frameKeyType(5) })
    }

    @Test fun brushPresetValues() {
        rule.onNodeWithTag("brush_AIRBRUSH").performScrollTo().performClick()
        rule.waitForIdle()
        assertEquals(300f, onUi { controller.brushes.size }, 0f)
        assertEquals(0.4f, onUi { controller.brushes.strength }, 0f)
        rule.onNodeWithTag("brush_INK_PEN").performScrollTo().performClick()
        rule.waitForIdle()
        assertEquals(60f, onUi { controller.brushes.size }, 0f)
        assertEquals(3, onUi { controller.brushes.pressureCurvePoints.size })
        drag(200f to 300f, 1000f to 300f)
        assertEquals(60.0, strokes().single().getDouble("thickness"), 1e-6)
        screenshot("23_brush_ink_pen")
    }

    @Test fun primitiveHandleEdit() {
        onUi { controller.selectTool(GreaseTool.LINE) }
        drag(200f to 300f, 800f to 300f)
        assertTrue("line is still editable", onUi { controller.shapeEditing })
        assertEquals(0, strokes().size)
        drag(800f to 300f, 800f to 500f) // move the end handle
        screenshot("24_line_handles")
        rule.onNodeWithTag("shapeConfirm").performClick()
        rule.waitForIdle()
        assertTrue(!onUi { controller.shapeEditing })
        val pts = strokes().single().getJSONArray("points")
        val last = pts.getJSONArray(pts.length() - 1)
        assertEquals(800.0, last.getDouble(0), 2.0)
        assertEquals(500.0, last.getDouble(1), 2.0)
    }

    @Test fun mp4ExportPlayable() {
        drag(200f to 300f, 1000f to 300f)
        assertEquals(null, onUi {
            controller.applyProjectSettings(com.smitnk.projectgrease.editor.ProjectSettings().apply {
                width = controller.document.canvasWidth; height = controller.document.canvasHeight; fps = 12; frameStart = 1; frameEnd = 6
            })
        })
        val file = java.io.File(rule.activity.cacheDir, "export.mp4")
        val frames = onUi { com.smitnk.projectgrease.ui.VideoExport.exportToFile(controller, file) }
        shell("cp ${file.absolutePath} $SHOT_DIR/25_export.mp4")
        assertEquals(6, frames)
        val r = android.media.MediaMetadataRetriever()
        try {
            r.setDataSource(file.absolutePath)
            assertEquals("yes", r.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO))
            assertEquals((onUi { controller.document.canvasWidth } and 1.inv()).toString(),
                r.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH))
            assertEquals(6, mp4VideoSampleCount(file))
            val first = mp4FirstFrame(r)
            assertTrue("first frame decodes", first != null && first.width > 0)
        } finally { r.release() }
    }
}
