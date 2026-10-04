package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/** Selection (click/box/circle/lasso/segment/menu), transforms and stroke operations (edit2..edit8). */
@RunWith(AndroidJUnit4::class)
class SweepEditTest : SweepBase() {

    private fun twoStrokes() { line(250f); line(450f) }
    private fun sel() = onUi { controller.selectedPointCount() }
    private fun total() = onUi { controller.pointCount() }
    private fun editMode() = onUi { controller.setMode(GreaseMode.EDIT) }

    // ---- selection ----
    @Test fun selectClick() {
        twoStrokes()
        onUi { controller.selectTool(GreaseTool.SELECT) }
        tap(600f, 250f)
        val n = sel()
        assertTrue("click selected one stroke ($n of ${total()})", n in 1 until total())
        shot("select_click")
    }

    @Test fun selectBox() {
        twoStrokes()
        onUi { controller.selectTool(GreaseTool.BOX_SELECT) }
        drag(150f to 200f, 1050f to 300f)
        val n = sel()
        assertTrue("box selected the top stroke ($n of ${total()})", n in 1 until total())
        shot("select_box")
    }

    @Test fun selectCircle() {
        twoStrokes()
        onUi { controller.selectTool(GreaseTool.CIRCLE_SELECT); controller.brushes.setSize(30f) }
        drag(300f to 450f, 900f to 450f)
        val n = sel()
        assertTrue("circle selected ($n of ${total()})", n in 1 until total())
        shot("select_circle")
    }

    @Test fun selectLasso() {
        twoStrokes()
        onUi { controller.selectTool(GreaseTool.LASSO) }
        drag(150f to 200f, 1050f to 200f, 1050f to 300f, 150f to 300f, 150f to 200f)
        val n = sel()
        assertTrue("lasso ($n of ${total()})", n in 1 until total())
        val bmp = shot("select_lasso")
        assertTrue("selection highlight", countPixels(bmp) { Color.red(it) > 230 && Color.green(it) in 100..170 && Color.blue(it) < 40 } > 0)
    }

    @Test fun selectSegment() {
        line(300f); drag(600f to 150f, 600f to 450f) // crossing
        onUi { controller.setSelectMode(S.MODE_SEGMENT); controller.selectTool(GreaseTool.SELECT) }
        tap(400f, 300f)
        val n = sel()
        assertTrue("segment selects part of the stroke ($n of ${total()})", n in 1 until points(strokes()[0]).size)
        shot("select_segment")
    }

    @Test fun selectPointMode() {
        line(300f)
        onUi { controller.setSelectMode(S.MODE_POINT); controller.selectTool(GreaseTool.SELECT); controller.setPickEntireStrokes(false) }
        tap(600f, 300f)
        assertEquals(1, sel())
        shot("select_point")
    }

    @Test fun selectMenu() {
        line(300f); line(500f)
        editMode()
        val all = total()
        assertTrue(onUi { controller.selectAll() }); assertEquals(all, sel())
        assertTrue(onUi { controller.deselectAll() }); assertEquals(0, sel())
        onUi { controller.setSelectMode(S.MODE_POINT) }
        assertTrue(onUi { controller.selectBox(550f, 280f, 650f, 320f) })
        val few = sel(); assertTrue(few in 1 until all)
        assertTrue(onUi { controller.selectMore() }); assertTrue("more", sel() > few)
        assertTrue(onUi { controller.selectLess() }); assertTrue("less", sel() <= few + 2)
        assertTrue(onUi { controller.selectLinked() }); assertEquals("linked = whole stroke", points(strokes()[0]).size, sel())
        assertTrue(onUi { controller.invertSelection() }); assertEquals(all - points(strokes()[0]).size, sel())
        onUi { controller.selectAll() }
        assertTrue(onUi { controller.selectAlternate() }); assertTrue("alternate", sel() in 1 until all)
        onUi { controller.deselectAll() }
        assertTrue(onUi { controller.selectFirstPoints() }); assertEquals("first points", 2, sel())
        onUi { controller.deselectAll() }
        assertTrue(onUi { controller.selectLastPoints() }); assertEquals("last points", 2, sel())
        onUi { controller.deselectAll() }
        assertTrue(onUi { controller.selectRandom(0.5f) }); assertTrue("random", sel() in 1 until all)
        onUi { controller.deselectAll(); controller.selectBox(550f, 280f, 650f, 320f) }
        assertTrue(onUi { controller.selectGroupedByLayer() }); assertEquals("grouped by layer", all, sel())
        onUi { controller.deselectAll(); controller.selectBox(550f, 280f, 650f, 320f) }
        assertTrue(onUi { controller.selectGroupedByMaterial() }); assertEquals("grouped by material", all, sel())
        shot("select_menu")
    }

    // ---- transforms (touch tools) ----
    private fun selectedFirst() { line(300f); onUi { controller.selectStroke(0); controller.selectAll() } }
    private fun xs() = points(strokes()[0]).map { it[0] }
    private fun ys() = points(strokes()[0]).map { it[1] }

    @Test fun transformMove() {
        selectedFirst(); onUi { controller.selectTool(GreaseTool.MOVE) }
        undoableGesture("move") { drag(600f to 300f, 600f to 500f) }
        assertEquals(500.0, ys().average(), 5.0)
        assertInk(shot("transform_move"), 600f, 500f, "moved stroke")
    }

    @Test fun transformRotate() {
        selectedFirst(); onUi { controller.selectTool(GreaseTool.ROTATE) }
        undoableGesture("rotate") { drag(900f to 300f, 600f to 600f) }
        assertTrue("rotated: y span ${ys().max() - ys().min()}", ys().max() - ys().min() > 50)
        shot("transform_rotate")
    }

    @Test fun transformScale() {
        selectedFirst(); onUi { controller.selectTool(GreaseTool.SCALE) }
        val w0 = xs().max() - xs().min()
        undoableGesture("scale") { drag(900f to 300f, 750f to 300f) }
        val w1 = xs().max() - xs().min()
        assertTrue("scaled $w0 -> $w1", w1 < w0 * 0.9)
        shot("transform_scale")
    }

    @Test fun transformMirror() {
        drag(200f to 300f, 1000f to 200f); onUi { controller.selectAll() }
        val first = points(strokes()[0])[0][1]
        onUi { controller.selectTool(GreaseTool.MIRROR) }
        undoableGesture("mirror") { drag(600f to 250f, 600f to 500f) } // press on the stroke, drag vertically
        // A vertical drag reflects across the stroke's horizontal centre line: y 300 <-> 200.
        // about the median of the stroke's points (Blender's transform pivot), close to y 250
        assertEquals("mirrored y", 200.0, points(strokes()[0])[0][1], 10.0)
        shot("transform_mirror"); assertTrue(first > 0)
    }

    @Test fun transformSelectionApi() {
        selectedFirst()
        undoable("translate") { controller.translateSelectedStroke(10f, 20f) }
        undoable("rotate") { controller.rotateSelectedStroke(0.5f) }
        undoable("scale") { controller.scaleSelectedStroke(0.5f, 0.5f) }
        undoable("mirror") { controller.mirrorSelectedStroke(true, false) }
        shot("transform_api")
    }

    // ---- stroke operations ----
    private fun op(name: String, setup: () -> Unit = { line(300f); onUi { controller.selectStroke(0); controller.selectAll() } }, check: () -> Unit = {}, block: () -> Boolean) {
        setup()
        undoable(name, block)
        check()
        shot("op_$name")
    }

    @Test fun opDuplicate() = op("duplicate", check = { assertEquals(2, strokes().size) }) { controller.duplicateSelection() }
    @Test fun opDelete() = op("delete_strokes", check = { assertEquals(0, strokes().size) }) { controller.deleteSelectedStrokes() }
    @Test fun opDeletePoints() = op("delete_points", setup = {
        line(300f); onUi { controller.setSelectMode(S.MODE_POINT); controller.selectBox(500f, 280f, 700f, 320f) }
    }, check = { assertTrue(strokes().sumOf { points(it).size } < 40) }) { controller.deleteSelectedPoints() }
    @Test fun opSplit() = op("split", setup = {
        line(300f); onUi { controller.setSelectMode(S.MODE_POINT); controller.selectBox(500f, 280f, 700f, 320f) }
    }, check = { assertEquals("gpencil split: the selected run becomes its own stroke, the rest stays in two pieces", 3, strokes().size) }) { controller.splitSelection() }
    @Test fun opSubdivide() {
        line(300f); onUi { controller.selectStroke(0); controller.selectAll() }
        val n = points(strokes()[0]).size
        undoable("subdivide") { controller.subdivideSelectedStroke(1) }
        assertTrue(points(strokes()[0]).size > n); shot("op_subdivide")
    }
    @Test fun opTrim() {
        // BKE_gpencil_stroke_trim cuts a stroke at its own first self-intersection (a loop).
        drag(200f to 300f, 700f to 300f, 600f to 150f, 450f to 450f, steps = 20)
        onUi { controller.selectStroke(0) }
        undoable("trim") { controller.trimSelectedStrokeToIntersection() }
        shot("op_trim")
    }
    @Test fun opClose() = op("close", setup = { drag(300f to 300f, 600f to 150f, 900f to 300f); onUi { controller.selectStroke(0); controller.selectAll() } },
        check = { assertTrue(strokes()[0].getBoolean("cyclic")) }) { controller.setSelectionCyclic(S.CYCLIC_CLOSE) }
    @Test fun opJoin() = op("join", setup = { line(300f, 200f, 500f); line(300f, 600f, 900f); onUi { controller.selectAll() } },
        check = { assertEquals(1, strokes().size) }) { controller.joinSelection() }
    @Test fun opReverse() {
        line(300f); onUi { controller.selectStroke(0) }
        val x0 = points(strokes()[0])[0][0]
        undoable("reverse") { controller.reverseSelectedStroke() }
        assertTrue(abs(points(strokes()[0])[0][0] - x0) > 100); shot("op_reverse")
    }
    @Test fun opFlipDirection() = op("flip") { controller.flipSelection() }
    @Test fun opDash() = op("dash", check = { assertTrue(strokes().size > 1) }) { controller.dashSelection(3, 2) }
    @Test fun opMultiply() = op("multiply", check = { assertTrue(strokes().size > 1) }) { controller.multiplySelection(2, 8f) }
    @Test fun opArray() = op("array", check = { assertEquals(3, strokes().size) }) { controller.arraySelection(3, 0f, 60f) }
    @Test fun opMergeByDistance() = op("merge_by_distance", setup = { drag(300f to 300f, 900f to 300f, steps = 60); onUi { controller.selectAll() } },
        check = { assertTrue(points(strokes()[0]).size < 40) }) { controller.mergeSelectionByDistance(30f) }
    @Test fun opCaps() = op("caps") { controller.toggleSelectionCaps(S.CAPS_TOGGLE_BOTH) }
    @Test fun opStartPoint() = op("start_point", setup = {
        drag(300f to 300f, 600f to 150f, 900f to 300f, 600f to 450f, 300f to 300f)
        onUi { controller.selectStroke(0); controller.selectAll(); controller.setSelectionCyclic(S.CYCLIC_CLOSE); controller.setSelectMode(S.MODE_POINT); controller.deselectAll(); controller.selectBox(580f, 130f, 620f, 170f) }
    }) { controller.setSelectionStartPoint() }
    @Test fun opSeparateToLayer() {
        line(300f); onUi { controller.selectAll() }
        val layers = onUi { controller.layerCount() }
        undoable("separate") { controller.separateSelectionToLayer() }
        assertEquals(layers + 1, onUi { controller.layerCount() }); shot("op_separate")
    }
    @Test fun opMoveToLayer() {
        onUi { controller.selectLayer(1) }; line(300f); onUi { controller.selectAll() }
        undoable("move_to_layer") { controller.moveSelectionToLayer(0) }
        assertEquals(0, document().getJSONArray("layers").getJSONObject(1).getJSONArray("frames").getJSONObject(0).getJSONArray("strokes").length())
        shot("op_move_to_layer")
    }
    @Test fun opCopyPaste() {
        line(300f); onUi { controller.selectAll() }
        assertTrue(onUi { controller.copySelection() })
        undoable("paste") { controller.pasteStrokes() }
        assertEquals(2, strokes().size); shot("op_copy_paste")
    }
    @Test fun opThicknessModifier() = op("thickness_mod", check = { assertTrue(points(strokes()[0]).all { abs(it[3] - 2.0) < 0.05 } || strokes()[0].getDouble("thickness") != 8.0) }) { controller.applyThicknessModifier(2f) }
    @Test fun opOpacityModifier() = op("opacity_mod", check = { assertTrue(points(strokes()[0]).all { it[4] < 0.9 }) }) { controller.applyOpacityModifier(S.PAINT_STROKE, 0.5f) }
    @Test fun opLengthModifier() = op("length_mod") { controller.applyLengthModifier(0.2f, 0.2f) }
    @Test fun opTintModifier() = op("tint_mod", setup = { line(300f); onUi { controller.selectAll(); controller.setMaterialColor(0xFFFF0000.toInt()) } }) { controller.applyTintModifier(1f) }
    @Test fun opColorModifier() = op("color_mod", setup = { line(300f); onUi { controller.selectAll(); controller.setMaterialColor(0xFFFF0000.toInt()); controller.setSelectionVertexColor() } }) { controller.applyColorModifier(0.3f, 1f, 1f) }
    @Test fun opArrange() = op("arrange", setup = { line(300f); line(320f); onUi { controller.selectStroke(0) } }) { controller.arrangeSelection(S.ARRANGE_TOP) }
    @Test fun opAssignMaterial() = op("assign_material", check = { assertEquals(2, strokes()[0].getInt("material")) }) { controller.selectMaterial(2); controller.assignActiveMaterialToSelection() }
    @Test fun opSnapToGrid() = op("snap_grid", setup = { line(313f, 207f, 993f); onUi { controller.selectAll() } }) { controller.snapSelectionToGrid() }
    @Test fun opDissolve() = op("dissolve", setup = { line(300f); onUi { controller.setSelectMode(S.MODE_POINT); controller.selectBox(500f, 280f, 700f, 320f) } }) { controller.dissolveSelection() }
    @Test fun opExtrude() = op("extrude", setup = { line(300f); onUi { controller.setSelectMode(S.MODE_POINT); controller.deselectAll(); controller.selectLastPoints() } }) { controller.extrudeSelection() }
    @Test fun opSimplify() = op("simplify", setup = { drag(200f to 300f, 1000f to 300f, steps = 60); onUi { controller.selectAll() } }) { controller.simplifySelectionFixed(1) }
    @Test fun opSample() = op("sample") { controller.sampleSelection(10f) }
    @Test fun opNormalize() = op("normalize", setup = { drag(200f to 300f, 1000f to 300f, pressure = 0.5f, pen = true); onUi { controller.selectAll() } }) { controller.normalizeSelection(S.NORMALIZE_THICKNESS, 1f) }
    @Test fun opSmooth() = op("smooth", setup = { drag(200f to 300f, 400f to 200f, 600f to 400f, 800f to 200f, 1000f to 300f); onUi { controller.selectStroke(0); controller.selectAll() } }) { controller.smoothSelectedStroke(1f, 4) }
    @Test fun opOutline() = op("outline", check = { assertTrue(strokes()[0].getBoolean("cyclic")) }) { controller.outlineSelection(2) }
    @Test fun opShrink() = op("shrink") { controller.shrinkSelectedStroke(50f, 1) } // BKE_gpencil_stroke_shrink mode 1 = start
    @Test fun opUniformSubdivide() = op("uniform_subdivide", check = { assertEquals(64, points(strokes()[0]).size) }) { controller.uniformSubdivideSelectedStroke(64) }
    @Test fun opRandomizeColor() = op("randomize_color") { controller.randomizeSelectedStrokeColor() }
    @Test fun opMirrorCopy() = op("mirror_copy", check = { assertEquals(2, strokes().size) }) { controller.mirrorSelectionCopy(true, false) }
    @Test fun opCleanLoose() = op("clean_loose", setup = { line(300f); tap(600f, 500f); onUi { controller.selectAll() } }) { controller.cleanLoosePoints(1) }
    @Test fun opVertexColorOps() {
        line(300f); onUi { controller.selectAll(); controller.setMaterialColor(0xFFFF0000.toInt()) }
        undoable("vcolor_set") { controller.setSelectionVertexColor() }
        undoable("vcolor_invert") { controller.invertSelectionVertexColor() }
        undoable("vcolor_hsv") { controller.selectionVertexColorHsv(0.2f, 1f, 1f) }
        undoable("vcolor_bc") { controller.selectionVertexColorBrightnessContrast(0.2f, 0.2f) }
        undoable("vcolor_levels") { controller.selectionVertexColorLevels(0.1f, 1.2f) }
        undoable("vcolor_reset") { controller.resetSelectionVertexColor() }
        shot("op_vertex_color")
    }
    @Test fun opFillSelected() {
        drag(300f to 200f, 900f to 200f, 900f to 500f, 300f to 500f, 300f to 200f)
        onUi { controller.selectStroke(0); controller.selectAll(); controller.setSelectionCyclic(S.CYCLIC_CLOSE); controller.selectStroke(0) }
        // BKE_gpencil_stroke_fill_triangulate builds the fill triangles (runtime data, not saved):
        // checked on screen with the material's fill switched on.
        assertTrue("fill selected applied", onUi { controller.fillSelectedStroke() })
        onUi { controller.selectMaterial(0); controller.setMaterialFillEnabled(true) }
        val bmp = shot("op_fill_selected")
        assertTrue("filled inside: ${colorsNear(bmp, 600f, 350f, 4)}", inkNear(bmp, 600f, 350f, 3))
    }
}
