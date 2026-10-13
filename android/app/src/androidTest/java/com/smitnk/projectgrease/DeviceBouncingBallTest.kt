package com.smitnk.projectgrease

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GifEncoder
import com.smitnk.projectgrease.editor.GreaseMode
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ProjectSettings
import com.smitnk.projectgrease.editor.SculptBrush
import com.smitnk.projectgrease.editor.ToolSession
import com.smitnk.projectgrease.nativebridge.GPNative
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.ByteArrayOutputStream
import java.io.File

/**
 * A 7-frame bouncing ball made the way a user would, touching most features on the way: layers,
 * frames, circle primitive, fill, vertex colour (Mix Factor, Stroke / Fill), lasso + lattice warp
 * (squash and a mesh), Vertex Paint with the selection mask, sculpt, onion skin, playback and GIF
 * export. Every step runs even if an earlier one fails; the test lists all failures at the end.
 */
@RunWith(AndroidJUnit4::class)
class DeviceBouncingBallTest : SweepBase() {
    private val failures = mutableListOf<String>()
    private fun step(name: String, body: () -> Unit) {
        try { body() } catch (t: Throwable) { failures += "$name: ${t.message ?: t.javaClass.simpleName}" }
    }
    private fun check(what: String, ok: Boolean) { if (!ok) throw AssertionError(what) }

    // ball centre per frame: falls, squashes on the floor at frame 4, rises
    private val xs = floatArrayOf(250f, 370f, 490f, 610f, 730f, 850f, 970f)
    private val ys = floatArrayOf(160f, 290f, 430f, 540f, 430f, 290f, 160f)
    private val r = 60f
    private fun strokeCount() = onUi { GPNative.nativeStrokeCount(controller.nativeHandle) }
    private fun selectedPoints() = onUi { GPNative.nativeSelectedPointCount(controller.nativeHandle) }
    private fun goTo(f: Int) = onUi { controller.selectFrame(f); controller.render() }

    @Test fun bouncingBallSevenFrames() {
        step("project settings 7 frames") {
            check("settings", onUi {
                controller.applyProjectSettings(ProjectSettings().apply {
                    width = controller.document.canvasWidth; height = controller.document.canvasHeight
                    fps = 12; frameStart = 1; frameEnd = 7
                })
            } == null)
        }
        step("layers") {
            check("shadow layer", onUi { controller.createLayer("Shadow") })
            check("ball layer", onUi { controller.createLayer("Ball") })
            check("three layers", onUi { controller.layerCount() } >= 3)
        }
        // ---- the ball: a filled red circle per frame ----
        for (f in 1..7) step("frame $f ball") {
            onUi {
                controller.selectLayer(controller.layerCount() - 1)
                controller.createFrame(f); controller.selectFrame(f)
                controller.setVertexColorMode(true); controller.setMaterialColor(0xFFE53935.toInt())
                controller.selectTool(GreaseTool.CIRCLE)
            }
            val cx = xs[f - 1]; val cy = ys[f - 1]
            drag(cx - r to cy - r, cx + r to cy + r)
            check("circle confirms", onUi { controller.confirmShape() })
            onUi { controller.selectTool(GreaseTool.FILL) }
            tap(cx, cy)
            check("frame $f has the ball (${strokeCount()} strokes)", strokeCount() >= 1)
        }
        step("frame 1 shot") { goTo(1); assertInk(shot("ball_f1"), xs[0] + r, ys[0], "ball outline") }
        // ---- shadow on its own layer ----
        step("shadow") {
            onUi { controller.selectLayer(1); controller.createFrame(1); controller.selectFrame(1)
                controller.setMaterialColor(0xFF555555.toInt()); controller.selectTool(GreaseTool.DRAW) }
            line(620f, 520f, 700f)
            check("shadow stroke", strokeCount() >= 1)
        }
        // ---- Mix Factor and Stroke-only vertex colour ----
        step("mix factor + stroke only") {
            onUi { controller.selectLayer(controller.layerCount() - 1); controller.selectFrame(1)
                controller.updateVertexFactor(0.5f); controller.updateDrawVertexTarget(0)
                controller.setMaterialColor(0xFF1E88E5.toInt()); controller.selectTool(GreaseTool.DRAW) }
            line(80f, 150f, 350f)
            check("factor kept", onUi { controller.vertexFactor } == 0.5f)
            onUi { controller.updateVertexFactor(1f); controller.updateDrawVertexTarget(2) }
            shot("ball_mix_factor")
        }
        // ---- squash on the floor: lasso, then the 2x2 lattice (Warp box) ----
        step("frame 4 squash with the warp box") {
            goTo(4)
            onUi { controller.selectTool(GreaseTool.LASSO) }
            val cx = xs[3]; val cy = ys[3]; val l = r + 25f
            drag(cx - l to cy - l, cx + l to cy - l, cx + l to cy + l, cx - l to cy + l, cx - l to cy - l, steps = 6)
            check("lasso selects the ball (${selectedPoints()} points)", selectedPoints() > 0)
            val b0 = onUi { controller.selectionBounds() }!!
            check("warp starts", onUi { controller.beginWarp(2, 2) })
            check("warp overlay shown", rule.onNodeWithTagExists("warpOverlay"))
            // top edge (lattice row 0) down 40, both sides out 20: a squash
            onUi { controller.dragWarp(0, -20f, 40f); controller.dragWarp(1, 20f, 40f); controller.dragWarp(2, -20f, 0f); controller.dragWarp(3, 20f, 0f) }
            val b1 = onUi { controller.selectionBounds() }!!
            check("squashed: height ${b0[3] - b0[1]} -> ${b1[3] - b1[1]}", (b1[3] - b1[1]) < (b0[3] - b0[1]) - 20f)
            check("wider: ${b0[2] - b0[0]} -> ${b1[2] - b1[0]}", (b1[2] - b1[0]) > (b0[2] - b0[0]) + 20f)
            onUi { controller.endWarp(true) }
            shot("ball_f4_squash")
            // (no selectAll here: selecting is an undo step of its own and would drop the redo)
            // measured on the ball's points: undo does not keep the selection
            fun ballHeight() = strokes().flatMap { points(it) }.filter { kotlin.math.abs(it[0] - cx) < r + 30 && kotlin.math.abs(it[1] - cy) < r + 15 } // not the shadow at y 620
                .let { pts -> (pts.maxOfOrNull { it[1] } ?: 0.0) - (pts.minOfOrNull { it[1] } ?: 0.0) }
            val squashed = ballHeight()
            check("undo", onUi { controller.undo() })
            val round = ballHeight()
            check("undo restores the round ball: $squashed -> $round (was ${b0[3] - b0[1]})", kotlin.math.abs(round - (b0[3] - b0[1])) < 3.0)
            check("redo squashes again", onUi { controller.redo() } && kotlin.math.abs(ballHeight() - squashed) < 3.0)
        }
        step("warp cancel restores") {
            goTo(5); onUi { controller.selectAll() }
            val b0 = onUi { controller.selectionBounds() }!!
            onUi { controller.beginWarp(2, 2); controller.dragWarp(-1, 100f, 0f); controller.endWarp(false) }
            val b1 = onUi { controller.selectionBounds() }!!
            check("cancel puts it back", kotlin.math.abs(b1[0] - b0[0]) < 0.5f)
        }
        step("frame 5 warp mesh 3x3") {
            goTo(5); onUi { controller.selectAll() }
            check("mesh starts", onUi { controller.beginWarp(3, 3) })
            check("9 lattice points", onUi { controller.warpPoints?.size } == 18)
            onUi { controller.setWarpInterp(2); controller.dragWarp(4, 0f, -35f); controller.endWarp(true) }
            shot("ball_f5_mesh")
        }
        // ---- Vertex Paint with the selection mask (the reported bug) ----
        step("vertex paint mask strokes") {
            goTo(2)
            check("vertex paint mode", onUi { controller.setMode(GreaseMode.VERTEX_PAINT) })
            onUi { controller.vertexSelectMask = ToolSession.SELECT_MASK_STROKE; controller.deselectAll(); controller.selectTool(GreaseTool.LASSO) }
            val cx = xs[1]; val cy = ys[1]; val l = r + 25f
            drag(cx - l to cy - l, cx + l to cy - l, cx + l to cy + l, cx - l to cy + l, cx - l to cy - l, steps = 6)
            check("lasso selects in Vertex Paint (${selectedPoints()} points)", selectedPoints() > 0)
            check("selected points shown", onUi { controller.selectionOverlayVisible() })
            onUi { controller.setMaterialColor(0xFF00C853.toInt()); controller.vertexBrush.setSize(80f); controller.vertexBrush.setStrength(1f)
                controller.selectTool(GreaseTool.DRAW) }
            drag(cx - r to cy, cx + r to cy, cx - r to cy, cx + r to cy)
            shot("ball_f2_vertex_paint")
            // the screen shows the selected points (orange) over the outline, so read the colour:
            // point layout x, y, z, pressure, strength, time?, r, g, b, a (vertex colour from index 6)
            val painted = strokes().flatMap { points(it) }.filter { kotlin.math.abs(it[1] - cy) < r + 5 && kotlin.math.abs(it[0] - cx) < r + 5 }
            check("ball points found (${painted.size})", painted.isNotEmpty())
            check("ball painted green: " + painted.take(3).map { it.drop(6).take(4) },
                painted.any { it[9] > 0.3 && it[7] > it[6] })
        }
        step("vertex paint mask points") {
            onUi { controller.vertexSelectMask = ToolSession.SELECT_MASK_POINT; controller.deselectAll(); controller.selectTool(GreaseTool.LASSO) }
            val cx = xs[1]; val cy = ys[1]
            drag(cx + 20f to cy - r - 20f, cx + r + 20f to cy - r - 20f, cx + r + 20f to cy + r + 20f, cx + 20f to cy + r + 20f, cx + 20f to cy - r - 20f, steps = 6)
            val n = selectedPoints()
            onUi { controller.selectAll() }
            check("point mask selects part of the ball ($n of ${selectedPoints()})", n in 1 until selectedPoints())
            onUi { controller.setMode(GreaseMode.DRAW); controller.vertexSelectMask = 0 }
        }
        // ---- sculpt smooth on frame 6 ----
        step("sculpt smooth") {
            goTo(6)
            val before = document().toString()
            onUi { controller.selectTool(GreaseTool.SCULPT); controller.sculpt.select(SculptBrush.SMOOTH); controller.sculptBrush.setSize(80f) }
            drag(xs[5] - r to ys[5], xs[5] + r to ys[5])
            check("sculpt changed the ball", document().toString() != before)
            onUi { controller.selectTool(GreaseTool.DRAW) }
        }
        // ---- onion skin, playback, export ----
        step("onion skin") {
            check("onion on", onUi { controller.setOnionSkin(true, 1, 1, 0.35f) })
            goTo(4); shot("ball_f4_onion")
        }
        step("playback") {
            goTo(1); onUi { controller.animation.togglePlayback() }
            Thread.sleep(900)
            onUi { controller.animation.togglePlayback() }
            check("playback advanced (${onUi { controller.animation.currentFrame }})", onUi { controller.animation.currentFrame } != 1)
        }
        step("gif export 7 frames") {
            val out = ByteArrayOutputStream(); var frames = 0
            val ok = onUi {
                val gif = GifEncoder(out, controller.document.canvasWidth, controller.document.canvasHeight, 12)
                gif.begin(); val r = controller.renderExportFrames(false) { _, p -> gif.addFrame(p); frames++ }; gif.finish(); r
            }
            val f = File(rule.activity.cacheDir, "bouncing_ball.gif"); f.writeBytes(out.toByteArray())
            shell("cp ${f.absolutePath} $SHOT_DIR/bouncing_ball.gif")
            check("export ok, $frames frames", ok && frames == 7)
        }
        assertTrue("bouncing ball found ${failures.size} problem(s): " + failures.joinToString(" || "), failures.isEmpty())
    }

    private fun androidx.compose.ui.test.junit4.AndroidComposeTestRule<*, *>.onNodeWithTagExists(tag: String): Boolean {
        waitForIdle()
        return onAllNodes(androidx.compose.ui.test.hasTestTag(tag)).fetchSemanticsNodes().isNotEmpty()
    }
}
