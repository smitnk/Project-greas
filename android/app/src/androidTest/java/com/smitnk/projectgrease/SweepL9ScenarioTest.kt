package com.smitnk.projectgrease

import android.graphics.BitmapFactory
import android.os.SystemClock
import android.view.InputDevice
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.EraserMode
import com.smitnk.projectgrease.editor.GifEncoder
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.ProjectSettings
import com.smitnk.projectgrease.ui.ProjectStore
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.ByteArrayOutputStream

/**
 * One animator session end to end through the real controller, EGL surface and touch input: layers,
 * keys on several frames, onion skin, erase, lasso + move, material colour, a modifier, scrubbing,
 * a pinch during playback, a long undo/redo chain across all of it, save / reload, GIF export.
 */
@RunWith(AndroidJUnit4::class)
class SweepL9ScenarioTest : SweepBase() {
    private fun layerIndex(name: String) = onUi { (0 until controller.layerCount()).first { controller.layerName(it) == name } }
    private fun layerJson(name: String, doc: JSONObject = document()) = doc.getJSONArray("layers").let { a ->
        (0 until a.length()).map { a.getJSONObject(it) }.first { it.optString("name") == name }
    }
    private fun frameNumbers(name: String) = layerJson(name).getJSONArray("frames").let { f -> (0 until f.length()).map { f.getJSONObject(it).getInt("number") } }
    private fun frameStrokes(name: String, frame: Int): List<JSONObject> {
        val f = layerJson(name).getJSONArray("frames")
        val fr = (0 until f.length()).map { f.getJSONObject(it) }.first { it.getInt("number") == frame }.getJSONArray("strokes")
        return (0 until fr.length()).map { fr.getJSONObject(it) }
    }
    private fun step() = onUi { controller.history.stepIndex }

    private fun multi(action: Int, down: Long, pts: List<Pair<Float, Float>>): MotionEvent {
        val props = Array(pts.size) { i -> MotionEvent.PointerProperties().apply { id = i; toolType = MotionEvent.TOOL_TYPE_FINGER } }
        val coords = Array(pts.size) { i -> MotionEvent.PointerCoords().apply { x = pts[i].first; y = pts[i].second; pressure = 1f; size = 1f } }
        return MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, pts.size, props, coords, 0, 0, 1f, 1f, 0, 0,
            InputDevice.SOURCE_TOUCHSCREEN, 0)
    }

    /** Two fingers around canvas (600, 400) spread to 2x, without waiting for Compose idle (playback keeps it busy). */
    private fun pinchOutDuringPlayback() {
        val a = screen(400f, 400f); val b = screen(800f, 400f)
        val cx = (a.first + b.first) / 2; val r0 = (b.first - a.first) / 2
        val down = SystemClock.uptimeMillis()
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_DOWN, down, listOf(a)))
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_POINTER_DOWN or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, listOf(a, b)))
        var cur = listOf(a, b)
        for (s in 1..20) {
            SystemClock.sleep(12)
            val r = r0 * (1f + s / 20f)
            cur = listOf(cx - r to a.second, cx + r to a.second)
            instrumentation.sendPointerSync(multi(MotionEvent.ACTION_MOVE, down, cur))
        }
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_POINTER_UP or (1 shl MotionEvent.ACTION_POINTER_INDEX_SHIFT), down, cur))
        instrumentation.sendPointerSync(multi(MotionEvent.ACTION_UP, down, cur.take(1)))
        instrumentation.waitForIdleSync()
    }

    @Test fun animatorSession() {
        // ---- layers ----
        val baseLayers = onUi { controller.layerCount() }
        val s0 = step()
        assertTrue(onUi { controller.createLayer("Ink") })
        assertTrue(onUi { controller.createLayer("Color") })
        assertEquals(baseLayers + 2, onUi { controller.layerCount() })
        assertEquals("each layer add is one undo step", s0 + 2, step())

        // ---- keys on frames 1, 5, 9 of Ink ----
        run { val li = layerIndex("Ink"); assertTrue(onUi { controller.selectLayer(li) && controller.animation.setFrame(1) }) }
        line(300f)
        val afterFirstLine = signature(); val sFirstLine = step()
        assertEquals(1, frameStrokes("Ink", 1).size)
        assertTrue(onUi { controller.createFrame(5) })
        line(500f)
        assertTrue(onUi { controller.createFrame(9) })
        drag(200f to 600f, 600f to 450f, 1000f to 600f)
        assertEquals(listOf(1, 5, 9), frameNumbers("Ink"))
        assertEquals(listOf(1, 1, 1), listOf(1, 5, 9).map { frameStrokes("Ink", it).size })
        val y5 = points(frameStrokes("Ink", 5).single()).map { it[1] }
        assertEquals("frame 5 line at y 500", 500.0, y5.average(), 3.0)

        // ---- Color layer gets its own key on frame 1 ----
        run { val li = layerIndex("Color"); assertTrue(onUi { controller.selectLayer(li) && controller.animation.setFrame(1) }) }
        onUi { controller.brushes.setSize(30f) }; line(200f, 300f, 900f); onUi { controller.brushes.setSize(8f) }
        assertEquals(1, frameStrokes("Color", 1).size)
        assertEquals("Ink frame 1 untouched by the Color stroke", 1, frameStrokes("Ink", 1).size)

        // ---- onion skin on frame 5 ----
        run { val li = layerIndex("Ink"); assertTrue(onUi { controller.selectLayer(li) && controller.animation.setFrame(5) }) }
        assertTrue(onUi { controller.setOnionSkin(true, 1, 1, 0.8f) })
        assertTrue(onUi { controller.onion.enabled })
        onUi { controller.render() }
        // the ghost is faded and tinted, so look for any non-white ink, as SweepLayersFramesTest.onionSkin does
        assertTrue("onion ghost of frame 1 visible", inkNear(shot("scenario_onion"), 600f, 300f, 4) { it != android.graphics.Color.WHITE && android.graphics.Color.red(it) < 250 })

        // ---- erase through the frame 1 line ----
        onUi { controller.setOnionSkin(false); controller.animation.setFrame(1) }
        val pts1 = points(frameStrokes("Ink", 1).single()).size
        onUi { controller.selectTool(GreaseTool.ERASE); controller.setEraserMode(EraserMode.HARD); controller.setEraserSize(30f) }
        undoableGesture("erase") { drag(600f to 200f, 600f to 400f) }
        val erased = frameStrokes("Ink", 1)
        assertEquals("the hard eraser cut the line in two", 2, erased.size)
        assertTrue("erase removed points", erased.sumOf { points(it).size } < pts1)
        assertTrue("erased gap around x 600", erased.flatMap { points(it) }.none { kotlin.math.abs(it[0] - 600.0) < 10.0 })

        // ---- lasso the left half of the frame 5 line, move it down 50 ----
        onUi { controller.animation.setFrame(5); controller.selectTool(GreaseTool.LASSO) }
        drag(150f to 450f, 590f to 450f, 590f to 550f, 150f to 550f, 150f to 450f)
        val before5 = points(frameStrokes("Ink", 5).single())
        val left = before5.count { it[0] < 590.0 }
        assertEquals("lasso selected the left points", left, onUi { controller.selectedPointCount() })
        undoable("move selection") { controller.translateSelectedStroke(0f, 50f) }
        val after5 = points(frameStrokes("Ink", 5).single())
        for (i in before5.indices) {
            val dy = if (before5[i][0] < 590.0) 50.0 else 0.0
            assertEquals("point $i y", before5[i][1] + dy, after5[i][1], 0.01)
            assertEquals("point $i x", before5[i][0], after5[i][0], 0.01)
        }
        onUi { controller.selectAll(com.smitnk.projectgrease.editor.ProjectGreaseSelect.ACTION_DESELECT) }

        // ---- material colour ----
        onUi { controller.selectTool(GreaseTool.DRAW) }
        val matCount = onUi { controller.materialCount() }
        assertTrue(matCount >= 2)
        assertTrue(onUi { controller.selectMaterial(1) && controller.setMaterialColor(0xFFFF0000.toInt()) })
        line(650f)
        val red = frameStrokes("Ink", 5).last()
        assertEquals(1, red.getInt("material"))
        val m1 = document().getJSONArray("materials").getJSONObject(1).getJSONArray("stroke")
        assertEquals(listOf(1.0, 0.0, 0.0, 1.0), (0 until 4).map { m1.getDouble(it) })
        assertEquals(2, frameStrokes("Ink", 5).size)

        // ---- Smooth modifier: evaluated only, the stored points stay ----
        val raw5 = frameStrokes("Ink", 5).map { points(it).map { p -> p.toList() } }
        undoable("add smooth") { controller.addModifier(ModifierType.SMOOTH) }
        assertEquals(listOf(ModifierType.SMOOTH), onUi { controller.modifiers().map { it.type } })
        assertEquals(1, layerJson("Ink").getJSONArray("modifiers").length())
        assertEquals("a modifier does not change the stored strokes", raw5, frameStrokes("Ink", 5).map { points(it).map { p -> p.toList() } })
        val beforePlay = signature(); val sBeforePlay = step()

        // ---- scrub ----
        assertTrue(onUi { controller.scrubTo(9f) }); assertEquals(9, onUi { controller.animation.currentFrame })
        assertTrue(onUi { controller.scrubTo(3.4f) }); assertEquals(3, onUi { controller.animation.currentFrame })
        assertEquals("scrubbing is not an edit", sBeforePlay, step())

        // ---- play, pinch while playing ----
        onUi { controller.animation.setFrame(1); if (!controller.animation.playing) controller.animation.togglePlayback() }
        SystemClock.sleep(300)
        assertTrue(onUi { controller.animation.playing })
        val z0 = onUi { controller.view.zoom }
        pinchOutDuringPlayback()
        assertTrue("still playing after the pinch", onUi { controller.animation.playing })
        assertEquals("pinch zoomed 2x", z0 * 2f, onUi { controller.view.zoom }, 0.25f)
        onUi { controller.stopPlayback() }
        assertEquals("playback and pinch changed no data", beforePlay, signature())
        assertEquals(sBeforePlay, step())

        // ---- undo chain back to the first line, then redo all ----
        val n = sBeforePlay - sFirstLine
        assertTrue("steps recorded since the first line: $n", n >= 6)
        repeat(n) { assertTrue("undo ${it + 1}/$n", onUi { controller.undo() }) }
        assertEquals(sFirstLine, step())
        assertEquals("undo chain reached the first line", afterFirstLine, signature())
        assertEquals(listOf(1), frameNumbers("Ink"))
        repeat(n) { assertTrue("redo ${it + 1}/$n", onUi { controller.redo() }) }
        assertEquals(sBeforePlay, step())
        assertEquals("redo chain restored the session", beforePlay, signature())

        // ---- save, reload, compare ----
        val store = ProjectStore(rule.activity)
        val name = "SweepL9Scenario"
        val saved = onUi { controller.saveDocumentJson() }!!
        store.saveDocument(name, saved)
        assertTrue(onUi { controller.resetDocument() })
        assertEquals(0, strokes().size)
        val loaded = store.loadDocument(name)
        assertEquals("the file holds what was saved", saved, loaded)
        assertTrue(onUi { controller.loadDocumentJson(loaded!!) })
        assertEquals("reloaded document equals the saved one", saved, onUi { controller.saveDocumentJson() })
        assertEquals(listOf(1, 5, 9), frameNumbers("Ink"))
        assertEquals(1, layerJson("Ink").getJSONArray("modifiers").length())
        store.delete(name)

        // ---- GIF export of frames 1..9 ----
        assertEquals(null, onUi {
            controller.applyProjectSettings(ProjectSettings().apply {
                width = controller.document.canvasWidth; height = controller.document.canvasHeight; fps = 12; frameStart = 1; frameEnd = 9
            })
        })
        val out = ByteArrayOutputStream(); var frames = 0
        val ok = onUi {
            val gif = GifEncoder(out, controller.document.canvasWidth, controller.document.canvasHeight, 12)
            gif.begin(); val r = controller.renderExportFrames(false) { _, p -> gif.addFrame(p); frames++ }; gif.finish(); r
        }
        assertTrue(ok); assertEquals(9, frames)
        assertNotNull(BitmapFactory.decodeByteArray(out.toByteArray(), 0, out.size()))
        val f = java.io.File(rule.activity.cacheDir, "scenario.gif"); f.writeBytes(out.toByteArray())
        shell("cp ${f.absolutePath} $SHOT_DIR/scenario.gif")
    }
}
