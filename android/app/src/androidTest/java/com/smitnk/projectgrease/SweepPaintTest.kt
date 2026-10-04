package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.SculptBrush
import com.smitnk.projectgrease.editor.ToolSession
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Sculpt (every brush), vertex paint (every brush), weight paint + vertex groups. */
@RunWith(AndroidJUnit4::class)
class SweepPaintTest : SweepBase() {

    private fun wavy() = drag(200f to 300f, 400f to 260f, 600f to 340f, 800f to 260f, 1000f to 300f)

    private fun sculpt(brush: SculptBrush, gesture: () -> Unit = { drag(400f to 300f, 800f to 360f) }) {
        wavy()
        onUi { controller.selectTool(GreaseTool.SCULPT); controller.sculpt.select(brush); controller.brushes.setSize(80f); controller.brushes.setStrength(1f) }
        undoableGesture("sculpt ${brush.name}", gesture)
        assertEquals("sculpt keeps one stroke", 1, strokes().size)
        assertInk(shot("sculpt_${brush.name.lowercase()}"), 200f, 300f, "sculpted stroke start")
    }

    @Test fun sculptSmooth() = sculpt(SculptBrush.SMOOTH) { drag(300f to 300f, 900f to 300f) } // one stroke = one undo step
    @Test fun sculptThickness() = sculpt(SculptBrush.THICKNESS)
    @Test fun sculptStrength() = sculpt(SculptBrush.STRENGTH) { onUi { controller.sculpt.setInvert(true) }; drag(400f to 300f, 800f to 300f) }
    @Test fun sculptGrab() = sculpt(SculptBrush.GRAB) { drag(600f to 330f, 600f to 500f) }
    @Test fun sculptPush() = sculpt(SculptBrush.PUSH) { drag(600f to 250f, 600f to 450f) }
    @Test fun sculptPinch() = sculpt(SculptBrush.PINCH)
    @Test fun sculptTwist() = sculpt(SculptBrush.TWIST)
    @Test fun sculptRandomize() = sculpt(SculptBrush.RANDOMIZE)

    private fun vpaint(brush: Int, name: String, prepaint: Boolean) {
        line(300f)
        onUi { controller.setMode(GreaseMode.VERTEX_PAINT); controller.setMaterialColor(0xFFFF0000.toInt()); controller.brushes.setSize(60f); controller.brushes.setStrength(1f) }
        if (prepaint) drag(300f to 300f, 600f to 300f)
        onUi { controller.setVertexPaintBrush(brush) }
        undoableGesture("vpaint $name") { drag(500f to 300f, 900f to 300f) }
        val painted = points(strokes()[0]).count { it.size > 9 && it[9] > 0.0 }
        assertTrue("$name painted points $painted", painted > 0)
        val bmp = shot("vpaint_$name")
        if (brush == S.VPAINT_DRAW || brush == S.VPAINT_REPLACE) assertTrue("red on screen", inkNear(bmp, 700f, 300f, 4) { Color.red(it) > 150 && Color.green(it) < 90 })
    }

    @Test fun vertexPaintDraw() = vpaint(S.VPAINT_DRAW, "draw", false)
    @Test fun vertexPaintBlur() = vpaint(S.VPAINT_BLUR, "blur", true)
    @Test fun vertexPaintAverage() = vpaint(S.VPAINT_AVERAGE, "average", true)
    @Test fun vertexPaintSmear() = vpaint(S.VPAINT_SMEAR, "smear", true)
    @Test fun vertexPaintReplace() = vpaint(S.VPAINT_REPLACE, "replace", true)

    @Test fun vertexPaintFillTarget() {
        drag(300f to 200f, 900f to 200f, 900f to 500f, 300f to 500f, 300f to 200f)
        onUi { controller.selectAll(); controller.setSelectionCyclic(S.CYCLIC_CLOSE); controller.fillSelectedStroke() }
        onUi { controller.setMode(GreaseMode.VERTEX_PAINT); controller.setVertexPaintTarget(S.PAINT_FILL); controller.setMaterialColor(0xFF00FF00.toInt()); controller.brushes.setSize(200f) }
        undoableGesture("vpaint fill") { drag(500f to 350f, 700f to 350f) }
        shot("vpaint_fill")
    }

    private fun weights(): Int {
        var positive = 0
        val w = strokes()[0].optJSONArray("weights") ?: return 0
        for (i in 0 until w.length()) { val row = w.getJSONArray(i); var k = 2; while (k < row.length()) { if (row.getDouble(k) > 0.0) positive++; k += 2 } }
        return positive
    }

    private fun wpaint(brush: Int, name: String) {
        line(300f)
        onUi { controller.setMode(GreaseMode.WEIGHT_PAINT); controller.brushes.setSize(60f); controller.setWeightPaintValue(1f) }
        if (brush != ToolSession.GPWEIGHT_DRAW) drag(300f to 300f, 600f to 300f)
        onUi { controller.setWeightPaintBrush(brush) }
        undoableGesture("wpaint $name") { drag(450f to 300f, 900f to 300f) }
        assertTrue("$name weights", weights() > 0)
        val bmp = shot("wpaint_$name")
        assertTrue("weight view red: ${colorsNear(bmp, 700f, 300f, 6)}", inkNear(bmp, 700f, 300f, 4) { Color.red(it) > 150 && Color.blue(it) < 120 })
    }

    @Test fun weightPaintDraw() = wpaint(ToolSession.GPWEIGHT_DRAW, "draw")
    @Test fun weightPaintBlur() = wpaint(ToolSession.GPWEIGHT_BLUR, "blur")
    @Test fun weightPaintAverage() = wpaint(ToolSession.GPWEIGHT_AVERAGE, "average")
    @Test fun weightPaintSmear() = wpaint(ToolSession.GPWEIGHT_SMEAR, "smear")

    @Test fun vertexGroups() {
        line(300f)
        assertTrue(onUi { controller.addVertexGroup("A") }); assertTrue(onUi { controller.addVertexGroup("B") })
        assertEquals(listOf("A", "B"), onUi { controller.vertexGroups() })
        assertTrue(onUi { controller.renameVertexGroup(1, "Bee") }); assertEquals("Bee", onUi { controller.vertexGroups()[1] })
        onUi { controller.selectVertexGroup(0); controller.selectAll() }
        undoable("vg assign") { controller.assignSelectionToGroup(0.25f) } // 0.5 would be its own inverse
        assertTrue(weights() > 0)
        onUi { controller.deselectAll() }
        assertTrue(onUi { controller.selectGroupPoints() }); assertEquals(onUi { controller.pointCount() }, onUi { controller.selectedPointCount() })
        assertTrue(onUi { controller.deselectGroupPoints() }); assertEquals(0, onUi { controller.selectedPointCount() })
        onUi { controller.selectAll() } // the group operators act on the selected strokes
        undoable("vg invert") { controller.invertGroupWeights() }
        undoable("vg normalize") { controller.normalizeGroupWeights() }
        onUi { controller.selectAll() }
        undoable("vg weighted thickness") { controller.applyThicknessModifierWithWeights(2f) }
        undoable("vg remove") { controller.removeSelectionFromGroup() }
        assertTrue(onUi { controller.removeVertexGroup(1) }); assertEquals(1, onUi { controller.vertexGroups().size })
        val raw = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.loadDocumentJson(raw) })
        assertEquals(listOf("A"), onUi { controller.vertexGroups() })
        shot("vertex_groups")
    }
}
