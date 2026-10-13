package com.smitnk.projectgrease

import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.FxType
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.Rotoscope
import com.smitnk.projectgrease.nativebridge.GPNative
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/**
 * The bugs reported on a phone where Blender's renderer failed, so the old renderer drew, like this
 * emulator (SwiftShader, OpenGL ES 3.0): one test per report, checked on the screen.
 */
@RunWith(AndroidJUnit4::class)
class DeviceReportedBugsTest : SweepBase() {
    private fun near(c: Int, r: Int, g: Int, b: Int, tol: Int = 60) =
        kotlin.math.abs(Color.red(c) - r) < tol && kotlin.math.abs(Color.green(c) - g) < tol && kotlin.math.abs(Color.blue(c) - b) < tol
    private fun isPaper(c: Int) = Color.red(c) > 200 && Color.green(c) > 200 && Color.blue(c) > 200

    /** Reported: "any colour I pick draws black". */
    @Test fun chosenColourIsDrawn() {
        onUi { controller.setVertexColorMode(true); controller.setMaterialColor(0xFFE53935.toInt()); controller.brushes.setSize(30f) }
        line(300f)
        val c = pixel(shot("bug_colour"), 600f, 300f)
        assertTrue("the line is red, not black: ${Integer.toHexString(c)} " + colorsNear(shot("bug_colour2"), 600f, 300f),
            Color.red(c) > 150 && Color.green(c) < 110)
    }

    /** Reported: fill came out of the shape, lines turned into filled blobs, fills got a black outline. */
    @Test fun fillStaysInsideAndLeavesLinesAlone() {
        onUi { controller.setVertexColorMode(false); controller.brushes.setSize(8f); controller.selectTool(GreaseTool.DRAW) }
        // an open wavy line: with fill switched on for its material it would become a blob
        drag(200f to 560f, 400f to 620f, 600f to 560f, 800f to 620f)
        onUi { controller.selectTool(GreaseTool.CIRCLE) }
        drag(450f to 150f, 750f to 450f)
        assertTrue(onUi { controller.confirmShape() })
        onUi { controller.setMaterialColor(0xFF00C8C8.toInt()); controller.selectTool(GreaseTool.FILL) }
        tap(600f, 300f)
        val bmp = shot("bug_fill")
        val inside = pixel(bmp, 600f, 300f)
        assertTrue("inside is filled cyan: ${Integer.toHexString(inside)}", near(inside, 0, 200, 200, 80))
        assertTrue("outside the circle stays paper: ${Integer.toHexString(pixel(bmp, 600f, 500f))}", isPaper(pixel(bmp, 600f, 500f)))
        // the inside of the open line's wave (below its peak) is not filled
        assertTrue("the open line is not filled: ${Integer.toHexString(pixel(bmp, 400f, 585f))}", isPaper(pixel(bmp, 400f, 585f)))
        // the line material keeps fill off (the Fill tool uses its own fill-only material)
        val lineMat = onUi { com.smitnk.projectgrease.nativebridge.GPNative.nativeGetMaterialInfo(controller.nativeHandle, 0) }
        assertTrue("line material fill stays off", lineMat != null && lineMat[9] == 0f)
    }

    /** Reported: effects did nothing. Flip mirrors the layer left-right on screen. */
    @Test fun effectChangesTheScreen() {
        onUi { controller.brushes.setSize(30f) }
        // right of the floating toolbar (it covers the canvas' left edge)
        line(300f, 320f, 500f)
        assertInk(shot("bug_fx_before"), 400f, 300f, "line before the effect")
        assertTrue("add Flip", onUi { controller.addEffect(FxType.FLIP) })
        val bmp = shot("bug_fx_after")
        val w = onUi { controller.document.canvasWidth }.toFloat()
        assertTrue("flipped line shows at the mirrored place: " + colorsNear(bmp, w - 400f, 300f), inkNear(bmp, w - 400f, 300f, 12))
        assertTrue("and not at the old place any more", !inkNear(bmp, 400f, 300f, 6))
    }

    /** Reported: rectangle / circle / arc / polyline / curve handles stopped adjusting the shape. */
    @Test fun shapeHandlesAdjustTheShape() {
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(300f to 200f, 600f to 400f)
        assertTrue("rectangle editable after the drag", onUi { controller.shapeEditing })
        val h = onUi { controller.shapeHandles() }
        assertTrue("handles shown (${h.size})", h.isNotEmpty())
        val corner = h.maxByOrNull { it.first + it.second }!!
        drag(corner, corner.first + 150f to corner.second + 100f, steps = 10)
        val moved = onUi { controller.shapeHandles() }.maxByOrNull { it.first + it.second }!!
        assertTrue("dragging the corner moves it: $corner -> $moved", moved.first > corner.first + 100f)
        assertTrue(onUi { controller.confirmShape() })
        val xs = points(strokes().single()).map { it[0] }
        assertTrue("confirmed rectangle is wider: ${xs.minOrNull()}..${xs.maxOrNull()}", xs.maxOrNull()!! > 700.0)
    }

    /** Reported: an imported photo never showed on the canvas. */
    @Test fun importedPhotoShows() {
        line(300f)
        val file = File(rule.activity.cacheDir, "ref_red.png")
        Bitmap.createBitmap(64, 64, Bitmap.Config.ARGB_8888).apply { eraseColor(Color.RED) }
            .compress(Bitmap.CompressFormat.PNG, 100, file.outputStream())
        onUi {
            controller.rotoscope.set(Rotoscope.Kind.IMAGE, listOf(Uri.fromFile(file).toString()), 1)
            controller.rotoscope.setOpacity(1f)
            controller.rotoscopeChanged()
        }
        Thread.sleep(500)
        onUi { controller.render() }
        val c = pixel(shot("bug_photo"), 640f, 500f)
        assertTrue("the photo shows under the drawing: ${Integer.toHexString(c)}", Color.red(c) > 180 && Color.green(c) < 80)
    }

    /** Reported: with two or more strokes the lasso lagged and showed points that were not selected. */
    @Test fun lassoWithManyStrokesSelectsOnlyInside() {
        onUi { controller.brushes.setSize(10f) }
        for (y in listOf(150f, 250f, 350f, 450f, 550f)) line(y, 350f, 1000f)
        onUi { controller.selectTool(GreaseTool.LASSO) }
        val t0 = System.nanoTime()
        drag(280f to 220f, 1050f to 220f, 1050f to 280f, 280f to 280f, 280f to 220f, steps = 30)
        val ms = (System.nanoTime() - t0) / 1_000_000
        val sel = onUi { GPNative.nativeSelectedPointCount(controller.nativeHandle) }
        val all = onUi { GPNative.nativePointCount(controller.nativeHandle) }
        assertTrue("lasso selects only the stroke inside ($sel of $all points)", sel in 1 until all / 2)
        assertTrue("the lasso drag is not slow ($ms ms)", ms < 6000)
        shot("bug_lasso")
    }
}
