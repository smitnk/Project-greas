package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.FxType
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/** #14 lasso under a zoomed / turned view and #17 effect banding, on the device. */
@RunWith(AndroidJUnit4::class)
class SweepL10LassoFxTest : SweepBase() {

    /** The same on-screen lasso around the same strokes selects the same points at 1x, at 4x and at
     *  4x with the view turned 30 degrees (GPENCIL_OT_select_lasso tests in region pixels). */
    @Test fun lassoSameRegionAtZoomAndRotation() {
        val (cx, cy) = onUi { controller.document.canvasWidth / 2f to controller.document.canvasHeight / 2f }
        drag(cx - 30f to cy - 10f, cx - 10f to cy - 10f)   // inside the lasso
        drag(cx + 10f to cy + 10f, cx + 30f to cy + 10f)   // inside
        drag(cx + 50f to cy + 30f, cx + 60f to cy + 30f)   // outside
        val loop = arrayOf(cx - 40f to cy - 20f, cx + 40f to cy - 20f, cx + 40f to cy + 20f, cx - 40f to cy + 20f, cx - 40f to cy - 20f)
        val views = listOf(1f to 0f, 4f to 0f, 4f to (Math.PI / 6).toFloat())
        val counts = views.map { (zoom, rot) ->
            onUi {
                controller.setMode(GreaseMode.EDIT); controller.setSelectMode(S.MODE_POINT); controller.deselectAll()
                controller.view.setZoom(zoom); controller.view.setPan(0f, 0f); controller.view.setRotation(rot)
                controller.selectTool(GreaseTool.LASSO); controller.render()
            }
            drag(*loop)
            shot("l10_lasso_zoom${zoom.toInt()}_rot${Math.round(Math.toDegrees(rot.toDouble()))}")
            onUi { controller.selectedPointCount() }
        }
        val total = strokes().sumOf { points(it).size }
        val outside = points(strokes()[2]).size
        assertTrue("lasso selected points at 1x: $counts", counts[0] > 0)
        assertEquals("only the two inside strokes ($total points, $outside outside)", total - outside, counts[0])
        assertEquals("1x vs 4x: $counts", counts[0], counts[1])
        assertEquals("1x vs 4x turned 30 degrees: $counts", counts[0], counts[2])
        onUi { controller.view.reset(); controller.render() }
    }

    /** Blur + Shadow on a gradient fill: the on-screen ramp has no bands (RGBA16F effect buffers, or
     *  the dithered RGBA8 fallback). */
    @Test fun effectsOnGradientHaveNoBands() {
        drag(300f to 200f, 900f to 200f, 900f to 500f, 300f to 500f, 300f to 200f)
        onUi {
            controller.setMode(GreaseMode.EDIT); controller.selectAll(); controller.setSelectionCyclic(S.CYCLIC_CLOSE)
            controller.fillSelectedStroke(); controller.deselectAll()
            controller.setMaterialGradient(0, floatArrayOf(0f, 0f, 0f, 1f, 1f, 0f, 0f, 1f, 1f, 0f, 0f, 0f)) // linear to blue
            controller.addEffect(FxType.BLUR); controller.addEffect(FxType.SHADOW)
            controller.setMode(GreaseMode.DRAW); controller.render()
        }
        val bmp = shot("l10_fx_gradient_blur_shadow")
        val y = screen(600f, 350f).second.toInt()
        val x0 = screen(380f, 350f).first.toInt(); val x1 = screen(820f, 350f).first.toInt()
        var worst = 0; var run = 1; var longest = 0
        for (x in x0 + 1..x1) {
            val a = bmp.getPixel(x - 1, y); val b = bmp.getPixel(x, y)
            worst = maxOf(worst, abs(Color.blue(b) - Color.blue(a)), abs(Color.red(b) - Color.red(a)))
            run = if (b == a) run + 1 else 1
            longest = maxOf(longest, run)
        }
        val span = abs(Color.blue(bmp.getPixel(x1, y)) - Color.blue(bmp.getPixel(x0, y)))
        assertTrue("the gradient spans colours ($span)", span > 60)
        assertTrue("neighbour step $worst LSB", worst <= 2)
        // a band is a run much longer than the ramp's pixels per LSB
        assertTrue("longest run of one colour $longest px over ${x1 - x0} px, span $span", longest <= 3 * (x1 - x0) / span.coerceAtLeast(1) + 2)
    }
}
