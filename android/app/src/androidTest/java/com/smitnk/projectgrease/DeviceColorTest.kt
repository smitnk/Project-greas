package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.EraserMode
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/**
 * Device checks of the reported bugs, through real touches on the EGL surface:
 * colour isolation, picked colour shown as picked, Vertex Paint, eraser size. Each assertion names
 * the bug it guards. Points are [x, y, z, pressure, strength, time, r, g, b, a] (sRGB, a = mix).
 */
@RunWith(AndroidJUnit4::class)
class DeviceColorTest : SweepBase() {
    private fun near(a: Int, b: Int, tol: Int) =
        abs(Color.red(a) - Color.red(b)) <= tol && abs(Color.green(a) - Color.green(b)) <= tol && abs(Color.blue(a) - Color.blue(b)) <= tol

    @Test fun secondColourDoesNotRecolourFirstStroke() {
        onUi { controller.setVertexColorMode(true); controller.setMaterialColor(0xFF000000.toInt()); controller.brushes.setSize(30f) }
        line(300f)
        onUi { controller.setMaterialColor(0xFFFF0000.toInt()) }
        line(500f)
        val s = strokes()
        assertEquals(2, s.size)
        val p0 = points(s[0])[5]; val p1 = points(s[1])[5]
        assertTrue("first stroke keeps black vertex colour ${p0.toList()}", p0[6] < 0.05 && p0[9] > 0.99)
        assertTrue("second stroke is red ${p1.toList()}", p1[6] > 0.95 && p1[7] < 0.05 && p1[9] > 0.99)
        val bmp = shot("color_isolation")
        assertTrue("first stroke still dark on screen: " + colorsNear(bmp, 600f, 300f), inkNear(bmp, 600f, 300f))
        assertTrue("second stroke red on screen: " + colorsNear(bmp, 600f, 500f),
            inkNear(bmp, 600f, 500f) { Color.red(it) > 180 && Color.green(it) < 90 && Color.blue(it) < 90 })
    }

    @Test fun materialModeRecoloursLikeBlender() {
        // Blender design: the colour belongs to the material, editing it recolours its strokes.
        onUi { controller.setVertexColorMode(false); controller.setMaterialColor(0xFF000000.toInt()); controller.brushes.setSize(30f) }
        line(300f)
        onUi { controller.setMaterialColor(0xFF0000FF.toInt()) }
        val bmp = shot("color_material_mode")
        assertTrue("material stroke follows the material: " + colorsNear(bmp, 600f, 300f),
            inkNear(bmp, 600f, 300f) { Color.blue(it) > 180 && Color.red(it) < 90 })
    }

    @Test fun midToneShowsAsChosen() {
        // Guards the scene-linear / display mix-up: a mid grey must not render lighter or darker.
        onUi { controller.setVertexColorMode(true); controller.setMaterialColor(0xFF808080.toInt()); controller.brushes.setSize(30f) }
        line(300f)
        val c = pixel(shot("color_midtone"), 600f, 300f)
        assertTrue("grey 128 drawn as ${Integer.toHexString(c)}", near(c, 0xFF808080.toInt(), 14))
    }

    @Test fun eyedropperPicksTheColourOnScreen() {
        val chosen = 0xFF3366CC.toInt()
        onUi { controller.setVertexColorMode(true); controller.setMaterialColor(chosen); controller.brushes.setSize(30f) }
        line(300f)
        onUi { controller.setMaterialColor(0xFF000000.toInt()); controller.selectTool(GreaseTool.EYEDROPPER) }
        rule.waitForIdle()
        tap(600f, 300f)
        val picked = onUi { controller.materials.colorArgb }
        assertTrue("picked ${Integer.toHexString(picked)} for ${Integer.toHexString(chosen)}", near(picked, chosen, 14))
    }

    @Test fun vertexPaintPaintsTheStroke() {
        onUi { controller.setVertexColorMode(false); controller.brushes.setSize(30f) }
        line(300f)
        onUi { controller.setMode(GreaseMode.VERTEX_PAINT); controller.setMaterialColor(0xFFFF0000.toInt()); controller.selectTool(GreaseTool.DRAW) }
        rule.waitForIdle()
        // vertex brush: Blender's Vertex Draw (25 px, strength 0.8), not the pen size
        assertEquals(25f, onUi { controller.vertexBrush.size }, 1e-3f)
        for (i in 0 until 4) drag(500f to 300f, 700f to 300f)
        val mid = points(strokes().single()).let { it[it.size / 2] }
        assertTrue("painted point has red vertex colour ${mid.toList()}", mid[9] > 0.3 && mid[6] > 0.5)
        val bmp = shot("vertex_paint")
        assertTrue("painted part looks red: " + colorsNear(bmp, 600f, 300f),
            inkNear(bmp, 600f, 300f) { Color.red(it) > 120 && Color.green(it) < 90 })
    }

    @Test fun eraserUsesItsOwnSize() {
        // The reported bug: with a big pen (Marker 150) a tap erased the whole stroke.
        onUi { controller.brushes.setSize(150f) }
        line(300f)
        val drawn = strokes()
        assertEquals("one stroke drawn with the 150 pen", 1, drawn.size)
        val xs = points(drawn[0]).map { it[0].toInt() }
        onUi { controller.selectTool(GreaseTool.ERASE); controller.setEraserMode(EraserMode.HARD) }
        assertEquals(30f, onUi { controller.eraserSize }, 1e-3f)
        rule.waitForIdle() // the eraser option row moves the canvas: map the tap after layout
        val view = surface()
        val ppu = onUi { com.smitnk.projectgrease.editor.CanvasMapping.pixelsPerUnit(view.width.toFloat(), view.height.toFloat(),
            controller.document.canvasWidth, controller.document.canvasHeight, controller.view.zoom) }
        tap(600f, 300f)
        val after = strokes()
        assertEquals("tap cuts the stroke in two, it does not delete it (ppu $ppu, radius ${30f / ppu}, " +
            "points before ${xs.size} x=$xs, after ${after.map { s -> points(s).map { it[0].toInt() } }})", 2, after.size)
        assertEquals("pen size unchanged by the eraser", 150f, onUi { controller.brushes.size }, 1e-3f)
    }
}
