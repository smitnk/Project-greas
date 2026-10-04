package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.FxType
import com.smitnk.projectgrease.editor.MaterialTexture
import com.smitnk.projectgrease.editor.ModifierType as M
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Live modifiers (every type + apply), visual effects, materials (textures, dots/squares, name/order/lock/hide/solo). */
@RunWith(AndroidJUnit4::class)
class SweepModsMaterialsTest : SweepBase() {

    private fun scene() {
        onUi { controller.brushes.setSize(20f) }
        drag(200f to 300f, 400f to 250f, 600f to 330f, 800f to 260f, 1000f to 300f)
    }

    private fun canvasPixels(): IntArray = onUi { controller.renderCanvasPixels(false) }!!

    /** Adds the modifier, checks it is live (render + persistence), then applies it. */
    private fun modifier(type: Int, name: String, changesRender: Boolean, changesOnApply: Boolean,
                         param: Pair<Int, Float>? = null) {
        scene()
        val plain = canvasPixels()
        undoable("add $name") { controller.addModifier(type) }
        // Blender's defaults of some modifiers (factor 1) leave the strokes as they are.
        param?.let { (i, v) -> undoable("$name param") { controller.setModifierParam(0, i, v) } }
        val mods = onUi { controller.modifiers() }
        assertEquals(1, mods.size); assertEquals(type, mods[0].type)
        val live = canvasPixels()
        shot("modifier_$name")
        if (changesRender) assertTrue("$name changes the render", !plain.contentEquals(live))
        // survives save/load with the same params
        val raw = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.loadDocumentJson(raw) })
        val reloaded = onUi { controller.modifiers() }
        assertEquals(1, reloaded.size)
        assertTrue("$name params persist", reloaded[0].params.contentEquals(mods[0].params))
        // disable / enable
        assertTrue(onUi { controller.setModifierEnabled(0, false) })
        if (changesRender) assertTrue("$name disabled = plain", canvasPixels().contentEquals(plain))
        assertTrue(onUi { controller.setModifierEnabled(0, true) })
        // apply bakes it into the strokes
        val before = signature()
        assertTrue("apply $name", onUi { controller.applyLayerModifier(0) })
        assertEquals(0, onUi { controller.modifiers().size })
        if (changesOnApply) assertNotEquals("$name apply changed the strokes", before, signature())
        shot("modifier_${name}_applied")
    }

    @Test fun modThickness() = modifier(M.THICKNESS, "thickness", true, true, 2 to 3f)
    @Test fun modOpacity() = modifier(M.OPACITY, "opacity", true, true, 1 to 0.3f)
    @Test fun modTint() = modifier(M.TINT, "tint", true, true)
    @Test fun modColor() = modifier(M.COLOR, "color", false, false)
    @Test fun modLength() = modifier(M.LENGTH, "length", true, true)
    @Test fun modSmooth() = modifier(M.SMOOTH, "smooth", true, true)
    @Test fun modSimplify() = modifier(M.SIMPLIFY, "simplify", false, true)
    @Test fun modSubdiv() = modifier(M.SUBDIV, "subdiv", true, true)
    @Test fun modOffset() = modifier(M.OFFSET, "offset", false, false)
    @Test fun modNoise() = modifier(M.NOISE, "noise", true, true)
    @Test fun modBuild() = modifier(M.BUILD, "build", false, false)
    @Test fun modTimeOffset() = modifier(M.TIME, "time_offset", false, false)
    @Test fun modHook() = modifier(M.HOOK, "hook", false, false)
    @Test fun modLattice() = modifier(M.LATTICE, "lattice", false, false)
    @Test fun modEnvelope() = modifier(M.ENVELOPE, "envelope", true, true)
    @Test fun modWeightProximity() = modifier(M.WEIGHT_PROXIMITY, "weight_proximity", false, false)
    @Test fun modWeightAngle() = modifier(M.WEIGHT_ANGLE, "weight_angle", false, false)
    @Test fun modDash() = modifier(M.DASH, "dash", true, true)
    @Test fun modOutline() = modifier(M.OUTLINE, "outline", true, true)
    @Test fun modMirror() = modifier(M.MIRROR, "mirror", true, true)
    @Test fun modArray() = modifier(M.ARRAY, "array", true, true)
    @Test fun modMultiply() = modifier(M.MULTIPLY, "multiply", true, true)

    @Test fun modifierOrderingAndInfluence() {
        scene()
        onUi { controller.addModifier(M.THICKNESS); controller.addModifier(M.NOISE) }
        undoable("move modifier") { controller.moveModifier(1, -1) }
        assertEquals(listOf(M.NOISE, M.THICKNESS), onUi { controller.modifiers().map { it.type } })
        undoable("modifier param") { controller.setModifierParam(1, 0, 3f) }
        undoable("modifier curve") { controller.setModifierCurve(1, true, listOf(0f to 0f, 0.5f to 1f, 1f to 0f)) }
        undoable("remove modifier") { controller.removeModifier(0) }
        assertEquals(1, onUi { controller.modifiers().size })
        shot("modifier_ordering")
    }

    // ---- effects ----
    private fun effect(type: Int, name: String) {
        scene()
        val plain = canvasPixels()
        undoable("add fx $name") { controller.addEffect(type) }
        assertEquals(type, onUi { controller.effects().single().type })
        val bmp = shot("fx_$name")
        val live = canvasPixels()
        assertTrue("$name changes the render", !plain.contentEquals(live))
        val raw = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.loadDocumentJson(raw) })
        assertEquals(type, onUi { controller.effects().single().type })
        assertTrue(onUi { controller.setEffectEnabled(0, false) })
        assertTrue("$name disabled = plain", canvasPixels().contentEquals(plain))
        assertTrue(bmp.width > 0)
    }

    @Test fun fxBlur() = effect(FxType.BLUR, "blur")
    @Test fun fxFlip() = effect(FxType.FLIP, "flip")
    @Test fun fxPixel() = effect(FxType.PIXEL, "pixel")
    @Test fun fxSwirl() = effect(FxType.SWIRL, "swirl")
    @Test fun fxWave() = effect(FxType.WAVE, "wave")
    @Test fun fxRim() = effect(FxType.RIM, "rim")
    @Test fun fxColorize() = effect(FxType.COLORIZE, "colorize")
    @Test fun fxShadow() = effect(FxType.SHADOW, "shadow")
    @Test fun fxGlow() = effect(FxType.GLOW, "glow")

    @Test fun fxTargetOrderParams() {
        scene()
        onUi { controller.addEffect(FxType.BLUR); controller.addEffect(FxType.SHADOW) }
        undoable("move fx") { controller.moveEffect(1, -1) }
        undoable("fx target") { controller.setEffectTarget(0, 1) }
        undoable("fx param") { controller.setEffectParam(1, 0, 12f) }
        undoable("remove fx") { controller.removeEffect(0) }
        shot("fx_ordering")
    }

    // ---- materials ----
    @Test fun materialCreateSelect() {
        val n = onUi { controller.materialCount() }
        assertTrue("new slot", onUi { controller.selectMaterial(n) })
        assertEquals(n + 1, onUi { controller.materialCount() })
        assertEquals(n, onUi { controller.materials.activeMaterial })
        onUi { controller.setMaterialColor(0xFFFF8000.toInt()); controller.brushes.setSize(30f) }
        line(300f)
        assertEquals(n, strokes().single().getInt("material"))
        val raw = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.loadDocumentJson(raw) })
        assertEquals(n + 1, onUi { controller.materialCount() })
        val c = pixel(shot("material_create"), 600f, 300f)
        assertTrue("orange ${Integer.toHexString(c)}", Color.red(c) > 200 && Color.green(c) in 80..180 && Color.blue(c) < 60)
    }

    @Test fun materialColorsAndFill() {
        drag(300f to 200f, 900f to 200f, 900f to 500f, 300f to 500f, 300f to 200f)
        onUi { controller.selectAll(); controller.setSelectionCyclic(com.smitnk.projectgrease.editor.ProjectGreaseSelect.CYCLIC_CLOSE) }
        undoable("fill enable") { controller.setMaterialFillEnabled(true) }
        undoable("fill color") { controller.setFillColor(0xFF00FF00.toInt()) }
        val c = pixel(shot("material_fill"), 600f, 350f)
        assertTrue("green fill ${Integer.toHexString(c)}", Color.green(c) > 180 && Color.red(c) < 80)
    }

    @Test fun materialNameOrderLockHideSolo() {
        // both slots black: the template's slot 1 is White, invisible on the canvas
        onUi { controller.selectMaterial(1); controller.setMaterialColor(0xFF000000.toInt()) }; line(300f)
        onUi { controller.selectMaterial(2); controller.setMaterialColor(0xFF000000.toInt()) }; line(500f)
        undoable("rename material") { controller.renameMaterial(1, "Ink") }
        assertEquals("Ink", onUi { controller.materialName(1) })
        undoable("move material") { controller.moveMaterial(1, 1) }
        assertEquals("Ink", onUi { controller.materialName(2) })
        undoable("hide material") { controller.setMaterialHidden(2, true) }
        val bmp = shot("material_hidden")
        assertNoInk(bmp, 600f, 300f, "hidden material")
        onUi { controller.setMaterialHidden(2, false) }
        undoable("solo material") { controller.soloMaterial(2) }
        val solo = shot("material_solo")
        assertInk(solo, 600f, 300f, "solo material"); assertNoInk(solo, 600f, 500f, "other material")
        undoable("lock material") { controller.setMaterialLocked(1, true) }
        assertTrue(onUi { controller.materialRecord(1) }!!.locked)
    }

    @Test fun materialDotsAndSquares() {
        onUi { controller.brushes.setSize(30f) }
        for ((mode, name) in listOf(1 to "dots", 2 to "squares")) {
            undoable("line type $name") { controller.setMaterialLineType(mode) }
            line(200f + mode * 150f)
            shot("material_$name")
            assertInk(shot("material_${name}_2"), 600f, 200f + mode * 150f, name)
        }
        assertEquals(2, onUi { controller.materialRecord() }!!.mode)
    }

    @Test fun materialTextures() {
        onUi { controller.brushes.setSize(60f) }; line(300f)
        val w = 8; val h = 8
        val checker = IntArray(w * h) { i -> if (((i % w) + (i / w)) % 2 == 0) 0xFFFF0000.toInt() else 0xFF0000FF.toInt() }
        val tex = MaterialTexture(uri = "test://checker", enabled = true, mix = 1f, scaleX = 1f, scaleY = 1f, pixelSize = 20f)
        undoable("stroke texture") { controller.setMaterialTexture(0, false, tex, checker, w, h) }
        assertTrue(onUi { controller.materialTexture(0, false) }.enabled)
        val bmp = shot("material_texture")
        assertTrue("textured stroke red/blue", inkNear(bmp, 600f, 300f, 12) { Color.red(it) > 150 || Color.blue(it) > 150 })
    }

    @Test fun materialDelete() {
        val n = onUi { controller.materialCount() }
        onUi { controller.selectMaterial(n - 1) }
        undoable("delete material") { controller.deleteMaterial(n - 1) }
        assertEquals(n - 1, onUi { controller.materialCount() })
        shot("material_delete")
    }
}
