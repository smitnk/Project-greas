package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.BrushPreset
import com.smitnk.projectgrease.editor.EraserMode
import com.smitnk.projectgrease.editor.GreaseTool
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/** Drawing, brushes, pressure, eraser, primitives (+ edit phase), fill, pick, guides, annotations. */
@RunWith(AndroidJUnit4::class)
class SweepDrawTest : SweepBase() {

    // ---- drawing / brushes / pressure ----
    @Test fun freehand() {
        undoableGesture("draw") { drag(200f to 300f, 600f to 250f, 1000f to 300f) }
        val s = strokes().single()
        assertTrue("points ${points(s).size}", points(s).size > 5)
        assertInk(shot("draw_freehand"), 600f, 250f, "stroke")
    }

    @Test fun pressure() {
        onUi { controller.brushes.setUsePressure(true) }
        drag(200f to 300f, 1000f to 300f, pressure = 0.3f, pen = true)
        val p = points(strokes().single())
        assertTrue("pressure recorded ${p[p.size / 2][3]}", p[p.size / 2][3] in 0.05..0.6)
        shot("draw_pressure")
    }

    @Test fun pressureCurve() {
        onUi {
            controller.brushes.setUsePressure(true)
            controller.brushes.setPressureCurvePoints(listOf(0f to 1f, 1f to 1f)) // flat: always full
        }
        drag(200f to 300f, 1000f to 300f, pressure = 0.2f, pen = true)
        val p = points(strokes().single())
        assertEquals("curve maps 0.2 to 1", 1.0, p[p.size / 2][3], 0.05)
        shot("draw_pressure_curve")
    }

    @Test fun thickness() {
        onUi { controller.brushes.setSize(30f) }
        line(300f)
        assertEquals(30.0, strokes().single().getDouble("thickness"), 1e-6)
        shot("draw_thickness")
    }

    @Test fun opacityStrength() {
        onUi { controller.setBrushStrength(0.3f) }
        line(300f)
        val p = points(strokes().single())
        assertEquals(0.3, p[p.size / 2][4], 0.02)
        val c = pixel(shot("draw_strength_30"), 600f, 300f)
        assertTrue("30% black is grey: ${Integer.toHexString(c)}", Color.red(c) in 100..235)
    }

    @Test fun stabilization() {
        onUi { controller.setStabilizer(true, 0.9f, 20f) }
        drag(200f to 300f, 400f to 200f, 600f to 400f, 800f to 200f, 1000f to 300f)
        onUi { controller.setStabilizer(false) }
        val p = points(strokes().single())
        val span = p.maxOf { it[1] } - p.minOf { it[1] }
        assertTrue("stabilized stroke keeps away from the zigzag peaks: span $span", span < 200.0)
        shot("draw_stabilizer")
    }

    @Test fun smoothing() {
        onUi { controller.setActiveSmooth(1f) }
        drag(200f to 300f, 400f to 200f, 600f to 400f, 800f to 200f, 1000f to 300f)
        assertTrue(points(strokes().single()).size > 4)
        shot("draw_smoothing")
    }

    @Test fun spacing() {
        // 60 moves of ~13 units: the default filter keeps them all, spacing 40 drops most.
        drag(200f to 300f, 1000f to 300f, steps = 60)
        val dense = points(strokes().single()).size
        onUi { controller.setSpacing(40f) }
        drag(200f to 500f, 1000f to 500f, steps = 60)
        val sparse = points(strokes()[1]).size
        assertTrue("spacing 40 has fewer points ($sparse < $dense)", sparse < dense)
        shot("draw_spacing")
    }

    @Test fun brushPresets() {
        for (preset in BrushPreset.values()) {
            onUi { controller.selectBrush(preset) }
            assertEquals(preset, onUi { controller.brushes.preset })
            assertEquals(preset.pressureCurve.size, onUi { controller.brushes.pressureCurvePoints.size })
        }
        onUi { controller.selectBrush(BrushPreset.values().first()) }
        line(300f)
        assertEquals(onUi { controller.brushes.size }.toDouble(), strokes().single().getDouble("thickness"), 1e-3)
        shot("draw_brush_presets")
    }

    @Test fun strokeColor() {
        onUi { controller.selectMaterial(1); controller.setMaterialColor(0xFF0000FF.toInt()); controller.brushes.setSize(30f) }
        line(300f)
        assertEquals(1, strokes().single().getInt("material"))
        val c = pixel(shot("draw_stroke_color_blue"), 600f, 300f)
        assertTrue("blue ${Integer.toHexString(c)}", Color.blue(c) > 180 && Color.red(c) < 80)
    }

    // ---- eraser ----
    private fun eraserCase(mode: EraserMode, name: String, check: (Int) -> Unit) {
        line(300f)
        onUi { controller.selectTool(GreaseTool.ERASE); controller.setEraserMode(mode); controller.brushes.setSize(30f) }
        undoableGesture("erase $name") { drag(600f to 200f, 600f to 400f) }
        check(strokes().size)
        shot("eraser_$name")
    }

    @Test fun eraserHard() = eraserCase(EraserMode.HARD, "hard") { n ->
        assertEquals("hard eraser splits the stroke in two", 2, n)
    }

    @Test fun eraserSoft() = eraserCase(EraserMode.SOFT, "soft") { n -> assertTrue("soft keeps strokes ($n)", n >= 1) }

    @Test fun eraserStroke() = eraserCase(EraserMode.STROKE, "stroke") { n ->
        assertEquals("stroke eraser removes the whole stroke", 0, n)
    }

    // ---- primitives + edit phase ----
    private fun primitive(tool: GreaseTool, name: String, cyclic: Boolean) {
        onUi { controller.selectTool(tool) }
        drag(300f to 200f, 900f to 500f)
        assertTrue("$name enters the edit phase", onUi { controller.shapeEditing })
        assertEquals(0, strokes().size)
        assertTrue(onUi { controller.confirmShape() })
        val s = strokes().single()
        assertEquals("$name cyclic", cyclic, s.getBoolean("cyclic"))
        assertTrue(onUi { controller.undo() }); assertEquals(0, strokes().size)
        assertTrue(onUi { controller.redo() }); assertEquals(1, strokes().size)
        val bmp = shot("primitive_$name")
        val p = points(s)[0]
        assertInk(bmp, p[0].toFloat(), p[1].toFloat(), name)
    }

    @Test fun primitiveLine() = primitive(GreaseTool.LINE, "line", false)
    @Test fun primitiveRectangle() = primitive(GreaseTool.RECTANGLE, "box", true)
    @Test fun primitiveCircle() = primitive(GreaseTool.CIRCLE, "circle", true)
    @Test fun primitiveArc() = primitive(GreaseTool.ARC, "arc", false)

    @Test fun primitivePolyline() {
        onUi { controller.selectTool(GreaseTool.POLYLINE) }
        tap(300f, 300f); tap(600f, 200f); tap(900f, 300f); tap(900f, 300f)
        val s = strokes().single()
        assertTrue("polyline points ${points(s).size}", points(s).size >= 3)
        assertInk(shot("primitive_polyline"), 600f, 200f, "polyline corner")
    }

    @Test fun primitiveCurve() {
        onUi { controller.selectTool(GreaseTool.CURVE) }
        drag(300f to 400f, 900f to 400f)
        if (onUi { controller.curveEditing }) assertTrue(onUi { controller.confirmCurve() })
        assertEquals(1, strokes().size)
        shot("primitive_curve")
    }

    @Test fun primitiveEditSubdivideExtrude() {
        onUi { controller.selectTool(GreaseTool.LINE) }
        drag(200f to 300f, 800f to 300f)
        assertTrue(onUi { controller.changeShapeSubdivisions(4) })
        assertTrue(onUi { controller.extrudeShape() })
        assertTrue("extrude adds a handle", onUi { controller.shapeHandles().size } >= 3)
        shot("primitive_edit_phase")
        assertTrue(onUi { controller.confirmShape() })
        assertTrue(points(strokes().single()).size >= 3)
    }

    @Test fun primitiveEditCancel() {
        onUi { controller.selectTool(GreaseTool.CIRCLE) }
        drag(300f to 200f, 700f to 500f)
        onUi { controller.cancelShape() }
        assertEquals(0, strokes().size)
        assertTrue(!onUi { controller.shapeEditing })
        assertNoInk(shot("primitive_cancel"), 500f, 200f, "cancelled circle")
    }

    // ---- fill ----
    private fun box() {
        onUi { controller.selectTool(GreaseTool.RECTANGLE) }
        drag(300f to 200f, 900f to 500f)
        assertTrue(onUi { controller.confirmShape() })
    }

    private fun fillAt(name: String) {
        onUi { controller.selectMaterial(2); controller.setMaterialColor(0xFFFF0000.toInt()); controller.selectTool(GreaseTool.FILL) }
        val before = signature()
        tap(600f, 350f)
        fun diag(): String {
            val s = strokes().lastOrNull() ?: return "no strokes"
            val p = points(s)
            return "last stroke: material ${s.optInt("material")} cyclic ${s.optBoolean("cyclic")} points ${p.size} " +
                "x ${p.minOfOrNull { it[0] }}..${p.maxOfOrNull { it[0] }} y ${p.minOfOrNull { it[1] }}..${p.maxOfOrNull { it[1] }}; " +
                "material 2 ${document().getJSONArray("materials").optJSONObject(2)}"
        }
        val c = pixel(shot("fill_$name"), 600f, 350f)
        assertTrue("$name: red inside ${Integer.toHexString(c)}; ${diag()}", Color.red(c) > 180 && Color.green(c) < 90)
        val after = signature()
        assertTrue("fill $name changed the document", before != after)
        assertTrue(onUi { controller.undo() }); assertEquals("fill $name: undo restores", before, signature())
        assertTrue(onUi { controller.redo() }); assertEquals("fill $name: redo reapplies", after, signature())
        val c2 = pixel(shot("fill_${name}_redo"), 600f, 350f)
        assertTrue("$name after redo: red inside ${Integer.toHexString(c2)}; ${diag()}", Color.red(c2) > 180 && Color.green(c2) < 90)
    }

    @Test fun fillClosed() { box(); fillAt("closed"); assertEquals(2, strokes().size) }

    @Test fun fillLeakGap() {
        line(200f, 300f, 900f); line(500f, 300f, 900f)
        drag(300f to 200f, 300f to 500f); drag(900f to 200f, 900f to 480f) // 20 unit gap at the bottom right
        onUi { controller.setFillOptions(leak = 30) }
        fillAt("leak_gap")
    }

    @Test fun fillDilate() {
        box(); onUi { controller.setFillOptions(dilate = 5) }; fillAt("dilate")
        assertEquals(5, onUi { controller.fillDilate })
    }

    @Test fun fillBoundary() {
        box(); onUi { controller.setFillOptions(boundary = 1) }; fillAt("boundary")
        assertEquals(1, onUi { controller.fillBoundary })
    }

    @Test fun fillExtend() {
        // Open corner closed by extending the stroke ends (Blender's extend lines).
        line(200f, 300f, 880f); drag(900f to 220f, 900f to 500f); line(500f, 900f, 300f); drag(300f to 500f, 300f to 200f)
        onUi { controller.setFillExtend(0.5f) }
        fillAt("extend")
    }

    // ---- pick ----
    @Test fun pick() {
        onUi { controller.selectMaterial(1); controller.setMaterialColor(0xFF00C000.toInt()); controller.brushes.setSize(40f) }
        line(300f)
        onUi { controller.selectMaterial(0); controller.selectTool(GreaseTool.EYEDROPPER) }
        tap(600f, 300f)
        val c = onUi { controller.materials.colorArgb }
        shot("pick")
        assertTrue("picked green ${Integer.toHexString(c)}", Color.green(c) > 150 && Color.red(c) < 60)
    }

    // ---- guides / grid / snapping ----
    @Test fun guideTypes() {
        for (type in 0..4) {
            onUi { controller.view.setDrawingGuide(type, 640f, 360f, 0f, 40f); controller.render() }
            assertEquals(type, onUi { controller.view.guideType })
        }
        onUi { controller.view.setDrawingGuide(2, 640f, 360f, 0f, 40f) }
        drag(200f to 300f, 600f to 360f, 1000f to 260f)
        val p = points(strokes().single())
        val worst = p.maxOf { abs(it[1] - p[0][1]) }
        shot("guide_parallel")
        onUi { controller.view.setDrawingGuide(-1) }
        assertTrue("guided stroke straight ($worst)", worst < 0.5)
    }

    @Test fun gridAndSnapping() {
        onUi { if (!controller.view.showGrid) controller.view.toggleGrid(); controller.view.setGridSize(50f); if (!controller.view.snapEnabled) controller.view.toggleSnapping(); controller.render() }
        val snapped = onUi { controller.view.snapPoint(123f, 177f) }
        assertEquals(100f to 200f, snapped)
        shot("grid_snapping")
        onUi { controller.view.toggleSnapping(); controller.view.toggleGrid() }
    }

    // ---- view ----
    @Test fun panZoomFit() {
        line(300f)
        onUi { controller.view.setZoom(2f); controller.view.panBy(50f, 30f); controller.render() }
        assertEquals(2f, onUi { controller.view.zoom }, 1e-4f)
        assertInk(shot("view_zoom_2"), 600f, 300f, "stroke under zoom")
        onUi { controller.fitCanvas(); controller.render() }
        assertInk(shot("view_fit"), 600f, 300f, "stroke after fit")
        onUi { controller.view.reset(); controller.render() }
        assertEquals(1f, onUi { controller.view.zoom }, 1e-4f)
    }

    // ---- annotations ----
    @Test fun annotations() {
        onUi { controller.selectTool(GreaseTool.ANNOTATE); controller.setAnnotationColor(0xFF00A0FF.toInt()) }
        drag(200f to 600f, 1000f to 600f)
        assertEquals(1, onUi { controller.annotationCount() })
        assertEquals("annotations are not document strokes", 0, strokes().size)
        val bmp = shot("annotation")
        // The annotation line is thin and antialiased: bluish rather than pure blue.
        val bluish = { c: Int -> Color.blue(c) > Color.red(c) + 50 }
        assertTrue("annotation drawn near (600,600); bluish pixels on the canvas: ${countPixels(bmp, 2, bluish)}", inkNear(bmp, 600f, 600f, 8, bluish))
        onUi { controller.setAnnotationsVisible(false) }
        assertTrue("annotation hidden", !inkNear(shot("annotation_hidden"), 600f, 600f, 8, bluish))
        onUi { controller.setAnnotationsVisible(true); controller.clearAnnotations() }
        assertEquals(0, onUi { controller.annotationCount() })
    }
}
