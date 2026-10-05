package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.SculptBrush
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/** Parity phase 1: proportional editing, pivots, dope sheet frame ops, gradient fill, Clone, Build additive. */
@RunWith(AndroidJUnit4::class)
class Parity1Test : SweepBase() {

    private fun firstStrokePoints() = points(strokes()[0])
    private fun frameNumbers(): List<Int> = onUi { controller.frameNumbers().toList() }

    /** Proportional move (calculatePropRatio, linear falloff): the selected point moves fully, points
     *  inside the radius by 1 - distance / size, points outside stay. */
    @Test fun proportionalMoveWeights() {
        line(300f, 200f, 1000f)
        val before = firstStrokePoints()
        onUi {
            controller.setMode(GreaseMode.EDIT)
            controller.setSelectMode(S.MODE_POINT)
            controller.deselectAll()
            controller.selectBox(150f, 250f, 215f, 350f) // the first point only
            controller.setProportionalFalloff(4)           // PROP_LIN
            controller.setProportionalSize(300f)
            if (!controller.transformSettings.proportional) controller.toggleProportional()
        }
        undoable("proportional move") { onUi { controller.translateSelectedStroke(0f, 100f) } }
        val after = firstStrokePoints()
        assertEquals(before.size, after.size)
        val dy = after.indices.map { after[it][1] - before[it][1] }
        assertEquals("selected point moves fully", 100.0, dy[0], 1.5)
        val mid = before.indices.firstOrNull { before[it][0] - before[0][0] in 120.0..180.0 }
        if (mid != null) {
            val expect = 100.0 * (1.0 - (before[mid][0] - before[0][0]) / 300.0)
            assertEquals("linear falloff at ${before[mid][0]}", expect, dy[mid], 6.0)
        }
        before.indices.filter { before[it][0] - before[0][0] > 320.0 }.forEach { assertEquals("outside radius", 0.0, dy[it], 0.01) }
        shot("parity_proportional")
        onUi { controller.toggleProportional() }
    }

    private fun centers(): List<Pair<Double, Double>> = strokes().map { s ->
        val p = points(s); p.sumOf { it[0] } / p.size to p.sumOf { it[1] } / p.size
    }

    /** Pivot point: Individual Origins scales each stroke about its own centre, Median about the
     *  common centre, 2D Cursor rotates about the cursor. */
    @Test fun pivotModes() {
        line(200f, 200f, 400f)
        line(400f, 800f, 1000f)
        onUi { controller.setMode(GreaseMode.EDIT); controller.setSelectMode(S.MODE_STROKE); controller.selectAll() }
        val c0 = centers()
        onUi { controller.setPivot(S.PIVOT_INDIVIDUAL) }
        undoable("scale individual") { onUi { controller.scaleSelectedStrokeAround(2f, 2f, 600f, 300f) } }
        val c1 = centers()
        for (i in c0.indices) {
            assertEquals("individual keeps centre x", c0[i].first, c1[i].first, 2.0)
            assertEquals("individual keeps centre y", c0[i].second, c1[i].second, 2.0)
        }
        onUi { controller.setPivot(S.PIVOT_MEDIAN) }
        undoable("scale median") { onUi { controller.scaleSelectedStrokeAround(0.5f, 0.5f, 600f, 300f) } }
        val c2 = centers()
        assertTrue("median moves the centres together", abs(c2[0].first - c2[1].first) < abs(c1[0].first - c1[1].first) - 50)
        onUi { controller.setPivot(S.PIVOT_CURSOR); controller.setCursor2D(600f, 300f) }
        val p0 = firstStrokePoints()[0]
        undoable("rotate about cursor") { onUi { controller.rotateSelectedStrokeAround(Math.PI.toFloat(), 0f, 0f) } }
        val p1 = firstStrokePoints()[0]
        assertEquals("half turn about the cursor x", 1200.0 - p0[0], p1[0], 2.0)
        assertEquals("half turn about the cursor y", 600.0 - p0[1], p1[1], 2.0)
        shot("parity_pivots")
        onUi { controller.setPivot(S.PIVOT_MEDIAN) }
    }

    /** Dope sheet: box select, move, copy and paste with overwrite, each one undo step. */
    @Test fun frameMoveAndPaste() {
        line(300f)
        onUi { controller.createFrame(5); controller.selectFrame(5) }
        line(500f)
        assertTrue("box select frames", onUi { controller.boxSelectFrames(5, 5) })
        undoable("move frames") { onUi { controller.moveSelectedFrames(2) } }
        val keys = frameNumbers()
        assertTrue("frame 5 moved to 7: $keys", 7 in keys && 5 !in keys)
        onUi { controller.deselectTimelineFrames(); controller.boxSelectFrames(1, 1); controller.copySelectedFrames(); controller.animation.setFrame(10) }
        undoable("paste frames") { onUi { controller.pasteFrames() } }
        assertTrue("pasted at 10: ${frameNumbers()}", 10 in frameNumbers())
        onUi { controller.selectFrame(10); controller.render() }
        assertInk(shot("parity_frames_paste"), 600f, 300f, "pasted frame content")
    }

    /** Gradient fill: the two ends of a linear gradient show different colours on screen. */
    @Test fun gradientFillPixels() {
        drag(300f to 200f, 900f to 200f, 900f to 500f, 300f to 500f, 300f to 200f)
        onUi {
            controller.setMode(GreaseMode.EDIT); controller.selectAll(); controller.setSelectionCyclic(S.CYCLIC_CLOSE)
            controller.fillSelectedStroke(); controller.deselectAll()
        }
        val solid = shot("parity_gradient_before")
        val g = floatArrayOf(0f, 0f, 0f, 1f, 1f, 0f, 0f, 1f, 1f, 0f, 0f, 0f) // linear to blue, mix 0
        undoable("gradient fill") { onUi { controller.setMaterialGradient(0, g) } }
        val bmp = shot("parity_gradient")
        val l = pixel(bmp, 360f, 350f); val r = pixel(bmp, 840f, 350f)
        assertTrue("gradient ends differ: ${Integer.toHexString(l)} ${Integer.toHexString(r)} (solid ${Integer.toHexString(pixel(solid, 360f, 350f))})",
            abs(Color.blue(l) - Color.blue(r)) > 60 || abs(Color.red(l) - Color.red(r)) > 60)
        assertTrue("one end is blue", Color.blue(l) > 150 || Color.blue(r) > 150)
        assertTrue("saved", onUi { controller.materialRecord(0)?.gradient } != null)
    }

    /** Clone brush: pastes the copied strokes under the brush (one undo step). */
    @Test fun cloneBrush() {
        line(300f, 300f, 700f)
        onUi { controller.setMode(GreaseMode.EDIT); controller.selectAll(); controller.copySelection(); controller.setMode(GreaseMode.DRAW) }
        onUi { controller.selectTool(GreaseTool.SCULPT); controller.sculpt.select(SculptBrush.CLONE); controller.brushes.setSize(80f) }
        undoableGesture("clone") { tap(600f, 550f) }
        assertEquals("one clone added", 2, strokes().size)
        val c = points(strokes()[1])
        assertEquals("clone centred under the brush", 550.0, c.sumOf { it[1] } / c.size, 15.0)
        assertInk(shot("parity_clone"), 600f, 550f, "cloned stroke")
    }

    /** Build Additive: at the key only the strokes already on the previous key show; after the length
     *  everything does. */
    @Test fun buildAdditive() {
        line(250f)
        onUi { controller.createFrame(11); controller.selectFrame(11) }
        line(250f)
        line(450f)
        onUi {
            assertTrue(controller.addModifier(ModifierType.BUILD))
            val i = controller.modifiers().size - 1
            controller.setModifierParam(i, 0, 2f) // GP_BUILD_MODE_ADDITIVE
            controller.setModifierParam(i, 3, 10f) // length
            controller.animation.setFrame(11); controller.render()
        }
        val start = shot("parity_build_start")
        assertInk(start, 600f, 250f, "stroke of the previous key")
        assertFalse("new stroke hidden at the key", inkNear(start, 600f, 450f, 2))
        onUi { controller.animation.setFrame(30); controller.render() }
        assertInk(shot("parity_build_end"), 600f, 450f, "new stroke after the build")
    }
}
