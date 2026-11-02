package com.smitnk.projectgrease

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.FxType
import com.smitnk.projectgrease.editor.GifEncoder
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import com.smitnk.projectgrease.editor.ProjectSettings
import com.smitnk.projectgrease.editor.SculptBrush
import com.smitnk.projectgrease.nativebridge.GPNative
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.ByteArrayOutputStream
import java.io.File

/**
 * A 2-second, 12 fps (24 frame) animation made the way a user would: a filled character walks
 * across while the background stays put. Keys are drawn by hand (draw, circle, fill, vertex colour),
 * edited (lasso cut, erase, sculpt, warp), coloured in Vertex Paint (draw + smear on a fill, the
 * reported "smear turns it black" case), in-betweened with Interpolate Sequence, given a modifier and
 * an effect, played back and exported to GIF. Every step runs; failures are listed at the end.
 */
@RunWith(AndroidJUnit4::class)
class DeviceTwoSecondAnimationTest : SweepBase() {
    private val failures = mutableListOf<String>()
    private fun step(name: String, body: () -> Unit) {
        try { body() } catch (t: Throwable) { failures += "$name: ${t.message ?: t.javaClass.simpleName}" }
    }
    private fun check(what: String, ok: Boolean) { if (!ok) throw AssertionError(what) }
    private fun strokeCount() = onUi { GPNative.nativeStrokeCount(controller.nativeHandle) }
    private fun goTo(f: Int) = onUi { controller.selectFrame(f); controller.render() }
    private val keys = intArrayOf(1, 7, 13, 19, 24)
    private fun xAt(f: Int) = 220f + (f - 1) * 30f
    private val y = 330f
    private val r = 70f

    @Test fun twoSecondsAtTwelveFps() {
        step("project 12 fps, 24 frames") {
            check("settings", onUi {
                controller.applyProjectSettings(ProjectSettings().apply {
                    width = controller.document.canvasWidth; height = controller.document.canvasHeight
                    fps = 12; frameStart = 1; frameEnd = 24
                })
            } == null)
        }
        step("layers") {
            check("bg layer", onUi { controller.createLayer("Background") })
            check("char layer", onUi { controller.createLayer("Character") })
        }
        step("background ground line") {
            onUi { controller.selectLayer(1); controller.createFrame(1); controller.selectFrame(1)
                controller.setVertexColorMode(false); controller.setMaterialColor(0xFF3E2723.toInt()); controller.selectTool(GreaseTool.DRAW) }
            line(y + r + 30f, 100f, 1100f)
            check("ground drawn", strokeCount() >= 1)
        }
        // ---- hand-drawn keys: black outline (circle) + pink fill with vertex colour ----
        for (f in keys) step("key $f") {
            onUi {
                controller.selectLayer(controller.layerCount() - 1)
                controller.createFrame(f); controller.selectFrame(f)
                controller.setVertexColorMode(true); controller.updateDrawVertexTarget(0)
                controller.setMaterialColor(0xFF000000.toInt()); controller.selectTool(GreaseTool.CIRCLE)
            }
            val cx = xAt(f)
            drag(cx - r to y - r, cx + r to y + r)
            check("circle confirms", onUi { controller.confirmShape() })
            onUi { controller.updateDrawVertexTarget(2); controller.setMaterialColor(0xFFF67474.toInt()); controller.selectTool(GreaseTool.FILL) }
            tap(cx, y)
            onUi { controller.updateDrawVertexTarget(0); controller.setMaterialColor(0xFF000000.toInt()); controller.selectTool(GreaseTool.DRAW) }
            drag(cx - 20f to y - 15f, cx - 10f to y - 15f) // eye
            check("key $f has strokes (${strokeCount()})", strokeCount() >= 2)
        }
        step("key 1 shot") { goTo(1); assertInk(shot("anim_key1"), xAt(1) + r, y, "outline") }
        // ---- edits on keys ----
        step("lasso cut on key 7") {
            goTo(7); val before = signature()
            onUi { controller.selectTool(GreaseTool.LASSO) }
            val cx = xAt(7)
            drag(cx + r - 15f to y - 30f, cx + r + 30f to y - 30f, cx + r + 30f to y + 30f, cx + r - 15f to y + 30f, cx + r - 15f to y - 30f, steps = 4)
            check("lasso used", signature() != before || onUi { controller.selectionBounds() } != null)
        }
        step("erase on key 13") {
            goTo(13); val before = signature()
            check("eraseAt", onUi { controller.eraseAt(xAt(13) - 20f, y - 15f, 15f) })
            check("eye erased", signature() != before)
        }
        step("sculpt on key 19") {
            goTo(19); val before = signature()
            onUi { controller.selectTool(GreaseTool.SCULPT); controller.sculpt.select(SculptBrush.SMOOTH); controller.sculptBrush.setSize(80f) }
            drag(xAt(19) - r to y, xAt(19) + r to y)
            check("sculpt changed", signature() != before)
            onUi { controller.selectTool(GreaseTool.DRAW) }
        }
        step("warp squash on key 24") {
            goTo(24); onUi { controller.selectAll() }
            val b0 = onUi { controller.selectionBounds() }!!
            check("warp", onUi { controller.beginWarp(2, 2) })
            onUi { controller.dragWarp(0, -15f, 30f); controller.dragWarp(1, 15f, 30f); controller.endWarp(true) }
            val b1 = onUi { controller.selectionBounds() }!!
            check("squashed ${b0[3] - b0[1]} -> ${b1[3] - b1[1]}", (b1[3] - b1[1]) < (b0[3] - b0[1]))
            onUi { controller.deselectAll() }
        }
        // ---- Vertex Paint: draw, then smear across the fill (the reported bug) ----
        step("vertex paint draw on key 1") {
            goTo(1); val before = signature()
            check("vertex mode", onUi { controller.setMode(GreaseMode.VERTEX_PAINT) })
            onUi { controller.setVertexPaintBrush(S.VPAINT_DRAW); controller.setVertexPaintTarget(S.PAINT_BOTH)
                controller.setMaterialColor(0xFFDAF674.toInt()); controller.vertexBrush.setSize(60f); controller.selectTool(GreaseTool.DRAW) }
            drag(xAt(1) - r to y, xAt(1) + r to y)
            check("painted", signature() != before)
            shot("anim_vpaint_draw")
        }
        step("vertex paint smear on key 7 keeps colour") {
            goTo(7)
            onUi { controller.setVertexPaintBrush(S.VPAINT_SMEAR); controller.vertexBrush.setSize(60f) }
            val cx = xAt(7)
            drag(cx - r to y - r + 10f, cx to y, cx + r to y + r - 10f)
            val bmp = shot("anim_vpaint_smear")
            val inside = countPixels(bmp) { isDark(it) }
            onUi { controller.setMode(GreaseMode.DRAW) }
            val fills = strokes().mapNotNull { it.optJSONArray("fillColor") ?: it.optJSONArray("vertColorFill") }
            android.util.Log.i("PGLog", "SMEAR fill colours after smear: $fills dark pixels $inside")
            check("fill not black after smear: $fills", fills.none { a -> a.length() >= 3 && (0..2).all { a.getDouble(it) < 0.05 } && (a.length() < 4 || a.getDouble(3) > 0.5) })
        }
        // ---- in-betweens, modifier, effect ----
        step("interpolate sequence") {
            // Blender's Interpolate Sequence fills only the gap around the current frame: run it per gap
            val n = keys.dropLast(1).sumOf { k -> goTo(k); onUi { controller.interpolateSequence(k) } }
            check("in-betweens made ($n)", n > 0)
            val empty = (1..24).filter { f -> goTo(f); strokeCount() == 0 }
            check("every frame has the character, empty: $empty", empty.isEmpty())
        }
        step("modifier + effect") {
            check("noise modifier", onUi { controller.addModifier(ModifierType.NOISE) })
            check("shadow effect", onUi { controller.addEffect(FxType.SHADOW) })
            goTo(10); val bmp = shot("anim_f10_mods")
            check("frame 10 shows the character after modifier + effect", inkNear(bmp, xAt(10), y, 40) || inkNear(bmp, xAt(10) + r, y, 40))
        }
        step("other layer active: playhead still drives the character") {
            // background (one key) active, playhead on 19: the character layer must show its key 19, not key 1
            onUi { controller.selectLayer(1); controller.animation.setFrame(19); controller.render() }
            val bmp = shot("anim_f19_bg_active")
            check("character at frame 19 position", inkNear(bmp, xAt(19) + r, y, 30))
            check("not the frame 1 drawing", !inkNear(bmp, xAt(1) - r, y, 10))
            onUi { controller.selectLayer(controller.layerCount() - 1) }
        }
        step("onion + playback") {
            check("onion", onUi { controller.setOnionSkin(true, 1, 1, 0.35f) })
            goTo(1); onUi { controller.animation.togglePlayback() }
            Thread.sleep(1200)
            onUi { controller.animation.togglePlayback() }
            check("played (${onUi { controller.animation.currentFrame }})", onUi { controller.animation.currentFrame } != 1)
        }
        step("gif export 24 frames") {
            val out = ByteArrayOutputStream(); var frames = 0
            val ok = onUi {
                val gif = GifEncoder(out, controller.document.canvasWidth, controller.document.canvasHeight, 12)
                gif.begin(); val r = controller.renderExportFrames(false) { _, p -> gif.addFrame(p); frames++ }; gif.finish(); r
            }
            val f = File(rule.activity.cacheDir, "two_seconds.gif"); f.writeBytes(out.toByteArray())
            shell("cp ${f.absolutePath} $SHOT_DIR/two_seconds_12fps.gif")
            check("export ok, $frames frames", ok && frames == 24)
        }
        assertTrue("2 s animation found ${failures.size} problem(s): " + failures.joinToString(" || "), failures.isEmpty())
    }
}
