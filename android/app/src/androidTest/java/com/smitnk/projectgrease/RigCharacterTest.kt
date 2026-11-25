/*
 * Test art: Kenney "Toon Characters 1" (Male person, PNG parts), in androidTest/assets/rig-test/.
 * License: Creative Commons Zero 1.0 Universal (CC0 1.0, public domain dedication)
 * License URL: https://creativecommons.org/publicdomain/zero/1.0/
 * Source page: https://kenney.nl/assets/toon-characters (CC0 verified there)
 * Downloaded from mirror: https://opengameart.org/content/toon-characters-1 (kenney_toonCharacters1.zip, CC0 verified there; pack License.txt also CC0)
 * Author: Kenney (www.kenney.nl)
 */
package com.smitnk.projectgrease

import android.graphics.BitmapFactory
import android.os.SystemClock
import android.util.Log
import android.view.MotionEvent
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.ProjectSettings
import com.smitnk.projectgrease.editor.RigController
import com.smitnk.projectgrease.editor.RigController.RigIds
import com.smitnk.projectgrease.nativebridge.GPNative
import com.smitnk.projectgrease.nativebridge.View3DNative
import com.smitnk.projectgrease.nativebridge.View3DUi
import com.smitnk.projectgrease.ui.addSwap
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import kotlin.math.hypot
import kotlin.math.roundToInt
import kotlin.math.sin

/**
 * Part 5 rig test: a real cartoon character built the way a user would. Each Kenney part is traced
 * (Project > Trace image, controller.traceImage) onto its own layer in a standing pose, a 13-bone
 * armature with IK on Arm.L / Leg.L is bound with Automatic Weights, the mouth is a 3-drawing swap,
 * the "Smile" master slider drives the swap and the head tilt. A 24-frame wave-and-walk at 12 fps is
 * keyed, checked frame by frame, played, shot, orbited in 3D (frame times logged: the Part 2 speed
 * figure for a 24-frame rig) and exported to MP4. Shots and the MP4 land in SHOT_DIR (device-shots/sweep).
 */
@RunWith(AndroidJUnit4::class)
class RigCharacterTest : SweepBase() {
    private val rig get() = controller.rig

    // character space: sprite px, x right, y up negative, feet at 0 (see NOTES.txt for sprite sizes)
    private var s = 1f; private var ox = 0f; private var oy = 0f
    private fun cx(x: Float) = ox + s * x
    private fun cy(y: Float) = oy + s * y

    private fun layerIndex(name: String) = onUi { (0 until controller.layerCount()).first { controller.layerName(it) == name } }
    private fun layerJson(name: String): JSONObject {
        val a = document().getJSONArray("layers")
        return (0 until a.length()).map { a.getJSONObject(it) }.first { it.optString("name") == name }
    }

    /** Trace one PNG part, drawn at its place on a canvas-sized image so the trace fit is 1:1. */
    private fun tracePart(file: String, layer: String, x0: Float, y0: Float) {
        val bmp = instrumentation.context.assets.open("rig-test/$file").use { BitmapFactory.decodeStream(it) }
        val src = IntArray(bmp.width * bmp.height).also { bmp.getPixels(it, 0, bmp.width, 0, 0, bmp.width, bmp.height) }
        val (w, h) = onUi { controller.document.canvasWidth to controller.document.canvasHeight }
        val img = IntArray(w * h) { 0x00FFFFFF }
        var r = 0L; var g = 0L; var b = 0L; var n = 0L
        val left = cx(x0); val top = cy(y0)
        for (py in top.toInt() until (top + s * bmp.height).toInt()) for (px in left.toInt() until (left + s * bmp.width).toInt()) {
            if (px !in 0 until w || py !in 0 until h) continue
            val sx = ((px - left) / s).toInt().coerceIn(0, bmp.width - 1); val sy = ((py - top) / s).toInt().coerceIn(0, bmp.height - 1)
            val c = src[sy * bmp.width + sx]
            if ((c ushr 24) < 128) continue
            img[py * w + px] = 0xFF000000.toInt() // the silhouette is traced; the part's colour is the material
            r += (c shr 16) and 0xFF; g += (c shr 8) and 0xFF; b += c and 0xFF; n++
        }
        assertTrue("$file has opaque pixels", n > 0)
        val avg = (0xFF shl 24) or ((r / n).toInt() shl 16) or ((g / n).toInt() shl 8) or (b / n).toInt()
        val traced = onUi {
            controller.animation.setFrame(1)
            controller.setMaterialColor(avg)
            controller.traceImage(img, w, h, 0.5f, false, 1f).also { controller.renameLayer(name = layer) }
        }
        assertTrue("$layer traced: $traced outlines", traced >= 1)
        assertEquals(layer, onUi { controller.layerName(controller.selectedLayer) })
    }

    /** Mouth drawing [frame]: a GP stroke on the Mouth layer (the pack has no mouth sprites). */
    private fun mouthStroke(frame: Int, shape: (Float) -> Pair<Float, Float>) = onUi {
        controller.createFrame(frame); controller.selectFrame(frame)
        val n = 24
        val pts = FloatArray(n * 6)
        for (i in 0 until n) {
            val t = i / (n - 1f)
            val (x, y) = shape(t)
            pts[i * 6] = cx(x); pts[i * 6 + 1] = cy(y); pts[i * 6 + 3] = 1f; pts[i * 6 + 4] = 1f; pts[i * 6 + 5] = t
        }
        assertTrue(GPNative.nativeAddStroke(controller.nativeHandle, pts, n, floatArrayOf(0f, 12f, 0f, 0f, 0f, 0f, 0f, 0f), FloatArray(n * 4)))
        controller.history.markEdit(); controller.render()
    }

    /** Bone in character space; [parent] connected or keep offset (ED_armature_parent_set). */
    private fun bone(name: String, hx: Float, hy: Float, tx: Float, ty: Float, parent: Int = -1, connected: Boolean = false): Int {
        val i = rig.addBone(cx(hx), cy(hy), cx(tx), cy(ty), name)
        assertTrue("bone $name added", i >= 0)
        if (parent >= 0) {
            rig.editSelect(i); rig.editSelect(parent, extend = true)
            assertEquals(1, rig.parentSet(if (connected) RigController.PARENT_CONNECTED else RigController.PARENT_KEEP_OFFSET))
        }
        return i
    }

    private fun ik(owner: Int, target: String, pole: String) {
        assertEquals(0, rig.addConstraint(owner, RigIds.CON_IK))
        assertTrue(rig.setConstraintTarget(owner, 0, target))
        assertTrue(rig.setConstraintTarget(owner, 0, pole, pole = true))
        // influence, owner/target space, head/tail, mute; IK flag, chain 2, iterations, pole angle, weight, orient weight
        assertTrue(rig.setConstraint(owner, 0, floatArrayOf(1f, 0f, 0f, 0f, 0f,
            (RigIds.IK_USE_TAIL or RigIds.IK_POSITION).toFloat(), 2f, 500f, 0f, 1f, 1f)))
    }

    /** Pose key: [b] moved by (dx, dy) character px from rest (or turned by [angle]), keyed at [frame]. */
    private fun key(b: Int, frame: Int, dx: Float = 0f, dy: Float = 0f, angle: Float = 0f) {
        rig.poseSelect(b)
        rig.poseClear(RigController.CLEAR_LOCATION or RigController.CLEAR_ROTATION or RigController.CLEAR_SCALE)
        if (dx != 0f || dy != 0f) rig.poseTranslate(s * dx, s * dy)
        if (angle != 0f) rig.poseRotate(angle)
        assertTrue("key bone $b at $frame", rig.insertKey(frame) > 0)
    }

    /** Points of [layer]'s strokes deformed at [frame]: the Armature modifier applied (the canvas evaluation), then undone. */
    private fun deformed(layer: String, frame: Int): List<DoubleArray> {
        val li = layerIndex(layer)
        onUi {
            controller.selectLayer(li); controller.animation.setFrame(frame)
            val m = controller.modifiers(li).indexOfFirst { it.type == ModifierType.ARMATURE }
            assertTrue("$layer has an Armature modifier", m >= 0)
            assertTrue(controller.applyLayerModifier(m, li))
        }
        val pts = strokes(layerJson(layer)).flatMap { points(it) }
        onUi { assertTrue(controller.undo()) }
        return pts
    }

    @Test fun waveAndWalkCycle() {
        val (w, h) = onUi { controller.document.canvasWidth.toFloat() to controller.document.canvasHeight.toFloat() }
        s = h * 0.75f / 106f; ox = w / 2f; oy = h * 0.9f
        assertEquals(null, onUi {
            controller.applyProjectSettings(ProjectSettings().apply {
                width = controller.document.canvasWidth; height = controller.document.canvasHeight; fps = 12; frameStart = 1; frameEnd = 24
            })
        })

        // 1. the parts, traced onto their own layers, back to front, in a standing pose
        tracePart("leg_r.png", "Leg.R", -16f, -29f)
        tracePart("leg_l.png", "Leg.L", -2f, -29f)
        tracePart("body.png", "Body", -19f, -64f)
        tracePart("arm_r.png", "Arm.R", -36f, -62f)
        tracePart("arm_l.png", "Arm.L", 15f, -62f)
        tracePart("head.png", "Head", -26.5f, -106f)
        onUi { controller.setMaterialColor(0xFF000000.toInt()); assertTrue(controller.createLayer("Mouth")) }
        mouthStroke(1) { t -> -6f + 12f * t to -72f }                                     // neutral
        mouthStroke(2) { t -> -6f + 12f * t to -72f + 3f * sin(t * Math.PI.toFloat()) }    // smile (y up is negative)
        mouthStroke(3) { t -> 4f * kotlin.math.cos(t * 6.2832f) to -72f + 3f * sin(t * 6.2832f) } // open "O"
        onUi { controller.animation.setFrame(1) }
        val parts = listOf("Head", "Body", "Arm.L", "Arm.R", "Leg.L", "Leg.R", "Mouth")
        parts.forEach { assertTrue("$it has strokes", strokes(layerJson(it)).isNotEmpty()) }
        shot("rig_character_parts")

        // armature (.L is the character's left: screen right). IK targets and poles do not deform.
        val deform = listOf("Hip", "Spine", "Head", "UpperArm.L", "LowerArm.L", "UpperArm.R", "LowerArm.R",
            "UpperLeg.L", "LowerLeg.L", "UpperLeg.R", "LowerLeg.R")
        onUi {
            assertTrue(rig.create())
            val hip = bone("Hip", 0f, -27f, 0f, -38f)
            val spine = bone("Spine", 0f, -38f, 0f, -58f, hip, true)
            bone("Head", 0f, -58f, 0f, -100f, spine, true)
            for (side in listOf(1f, -1f)) {
                val sfx = if (side > 0) "L" else "R"
                val ua = bone("UpperArm.$sfx", 22f * side, -58f, 25f * side, -46f, spine)
                bone("LowerArm.$sfx", 25f * side, -46f, 28f * side, -34f, ua, true)
                val ul = bone("UpperLeg.$sfx", 7f * side, -27f, 8f * side, -14f, hip)
                bone("LowerLeg.$sfx", 8f * side, -14f, 7f * side, -2f, ul, true)
            }
            for ((name, c) in listOf("IK.Hand.L" to floatArrayOf(28f, -34f, 28f, -28f), "Pole.Arm.L" to floatArrayOf(40f, -46f, 40f, -40f),
                    "IK.Foot.L" to floatArrayOf(7f, -2f, 13f, -2f), "Pole.Leg.L" to floatArrayOf(20f, -14f, 20f, -8f))) {
                val b = bone(name, c[0], c[1], c[2], c[3])
                val e = rig.envelope(b)
                assertTrue(rig.setEnvelope(b, e[0], e[1], e[2], e[3], deform = false, multiplyVg = false) >= 0)
            }
            controller.history.markEdit()
        }
        assertEquals(15, onUi { rig.boneCount })

        // Bind with Automatic Weights: frame 1 (all parts) and the mouth's drawings 2 and 3
        onUi {
            for (f in 1..3) { controller.animation.setFrame(f); assertTrue("bind at $f", rig.bind(RigController.BIND_AUTOMATIC) >= 0) }
            controller.animation.setFrame(1)
            controller.history.markEdit()
        }
        assertTrue(onUi { controller.vertexGroups() }.containsAll(deform))
        parts.forEach { p -> val li = layerIndex(p); assertTrue("$p bound", onUi { controller.modifiers(li) }.any { it.type == ModifierType.ARMATURE }) }

        // IK, chain 2, with a pole: Arm.L and Leg.L
        val lowerArm = onUi { rig.findBone("LowerArm.L") }; val lowerLeg = onUi { rig.findBone("LowerLeg.L") }
        onUi { ik(lowerArm, "IK.Hand.L", "Pole.Arm.L"); ik(lowerLeg, "IK.Foot.L", "Pole.Leg.L"); controller.history.markEdit() }
        assertEquals(2f, onUi { rig.constraint(lowerArm, 0) }[7], 0f)

        // mouth swap (3 drawings) and the master slider Smile -> swap + head tilt
        val mouth = layerIndex("Mouth")
        onUi { controller.selectLayer(mouth); assertTrue(addSwap(controller, mouth) >= 0) }
        val timeMod = onUi { controller.modifiers(mouth).indexOfLast { it.type == ModifierType.TIME } }
        val swapOffset = { onUi { controller.modifiers(mouth)[timeMod].params[1] } }
        onUi {
            assertEquals(0, rig.addProperty("Smile", value = 0f, min = 0f, max = 1f, default = 0f))
            assertEquals(1, rig.linkSlider("Smile", rig.timeOffsetPath("Mouth", timeMod), 0, 2f))
            assertEquals(2, rig.linkSlider("Smile", rig.bonePath("Head", "rotation_quaternion"), 0, 0.3f, rotation = true))
            controller.history.markEdit()
        }

        // 2. animate: 24 frames, wave (IK hand), steps (IK foot + FK right leg), swap keyed constant
        val hand = onUi { rig.findBone("IK.Hand.L") }; val foot = onUi { rig.findBone("IK.Foot.L") }
        val legR = onUi { rig.findBone("UpperLeg.R") }; val swapBone = onUi { rig.findBone("Swap.Mouth") }
        val handRest = onUi { rig.bone(hand)!! }
        onUi {
            assertTrue(rig.setMode(RigController.MODE_POSE))
            val wave = listOf(1 to (6f to -44f), 7 to (12f to -38f), 13 to (6f to -44f), 19 to (12f to -38f), 24 to (6f to -44f))
            for ((f, d) in wave) key(hand, f, d.first, d.second)
            val step = listOf(1 to (0f to 0f), 7 to (6f to -6f), 13 to (0f to 0f), 19 to (-3f to -2f), 24 to (0f to 0f))
            for ((f, d) in step) key(foot, f, d.first, d.second)
            for ((f, a) in listOf(1 to 0f, 7 to -0.25f, 13 to 0f, 19 to 0.3f, 24 to 0f)) key(legR, f, angle = a)
            rig.poseSelect(swapBone)
            for ((f, d) in listOf(1 to 1, 9 to 2, 17 to 3)) {
                assertTrue(rig.setSwap("Mouth", d)); assertTrue(rig.insertKey(f) > 0)
                rig.setInterpolation(swapBone, f, RigController.IPO_CONSTANT)
            }
            controller.history.markEdit()
        }
        assertEquals(listOf(1f, 7f, 13f, 19f, 24f), onUi { controller.animation.boneKeys(hand).toList() })

        // the IK target moved by the key (canvas px), and the hand reaches it at every key
        onUi { controller.animation.setFrame(7) }
        assertEquals(handRest.poseHeadX + 12f * s, onUi { rig.bone(hand)!! }.poseHeadX, 0.5f)
        for (f in listOf(1, 7, 13, 19)) {
            onUi { controller.animation.setFrame(f) }
            for ((tip, tgt) in listOf(lowerArm to hand, lowerLeg to foot)) {
                val a = onUi { rig.bone(tip)!! }; val t = onUi { rig.bone(tgt)!! }
                val miss = hypot(a.poseTailX - t.poseHeadX, a.poseTailY - t.poseHeadY)
                Log.i(TAG, "rig ik frame $f ${a.name}: miss ${"%.2f".format(miss)} px")
                assertTrue("IK ${a.name} reaches target at frame $f: miss $miss px", miss < 3f)
            }
        }

        // the deform works: points of the arm and leg strokes move between frames
        for (layer in listOf("Arm.L", "Leg.L", "Leg.R")) {
            val a = deformed(layer, 1); val b = deformed(layer, 7)
            assertEquals(a.size, b.size)
            val moved = a.indices.maxOf { hypot(a[it][0] - b[it][0], a[it][1] - b[it][1]) }
            Log.i(TAG, "rig deform $layer frame 1 -> 7: max point move ${"%.1f".format(moved)} px")
            assertTrue("$layer strokes deform between frames 1 and 7: $moved px", moved > 5.0)
        }

        // the swap shows drawing 1 / 2 / 3 per frame (constant keys at 1, 9, 17)
        for (f in 1..24) {
            onUi { controller.animation.setFrame(f) }
            assertEquals("mouth drawing at frame $f", if (f < 9) 1f else if (f < 17) 2f else 3f, swapOffset(), 0f)
        }

        // the slider set over frames 1..6 tilts the head and pushes the swap: round(1 + 2 * Smile)
        val headBone = onUi { rig.findBone("Head") }
        val tilts = ArrayList<Float>()
        for (f in 1..6) {
            val smile = (f - 1) / 5f
            onUi { controller.animation.setFrame(f); assertTrue(rig.setProperty(0, smile)); controller.history.markEdit() }
            assertEquals("head quat z at Smile $smile", sin(0.15f * smile), onUi { rig.pose(headBone) }[6], 1e-3f)
            assertEquals("swap at Smile $smile", (1f + 2f * smile).roundToInt().toFloat(), swapOffset(), 0f)
            tilts += onUi { rig.bone(headBone)!!.poseTailX }
            if (f == 6) shot("rig_character_smile")
        }
        assertTrue("Smile turns the head bone: ${tilts.first()} -> ${tilts.last()}", kotlin.math.abs(tilts.last() - tilts.first()) > 2f)
        onUi { rig.setProperty(0, 0f); controller.animation.setFrame(1); controller.history.markEdit() }

        // 3. play it in the Camera (2D) view, then shots of frames 1, 6, 12, 18
        onUi { controller.animation.togglePlayback() }
        val seen = HashSet<Int>()
        val t0 = SystemClock.uptimeMillis()
        while (SystemClock.uptimeMillis() - t0 < 2200) { seen += onUi { controller.animation.currentFrame }; SystemClock.sleep(40) }
        onUi { controller.stopPlayback() }
        assertTrue("playback advanced: $seen", seen.size >= 6)
        for (f in listOf(1, 6, 12, 18)) {
            onUi { controller.animation.setFrame(f); controller.render() }
            shot("rig_character_f%02d".format(f))
        }

        // 3D view: orbit ~3 s around the rig, a shot per second, frame times logged
        onUi { controller.animation.setFrame(12) }
        try {
            onUi { View3DUi.draw = false; View3DUi.setActive(true, controller) }; rule.waitForIdle()
            val q0 = onUi { View3DNative.nativeState() }.copyOfRange(3, 7).toList()
            val v = surface(); val loc = IntArray(2)
            val (sw, sh) = onUi { v.getLocationOnScreen(loc); v.width.toFloat() to v.height.toFloat() }
            val times = ArrayList<Float>()
            for (k in 0 until 3) {
                val down = SystemClock.uptimeMillis()
                val ax = loc[0] + sw * 0.3f; val bx = loc[0] + sw * 0.7f; val y = loc[1] + sh * 0.5f
                instrumentation.sendPointerSync(event(MotionEvent.ACTION_DOWN, down, ax, y))
                for (i in 1..62) {
                    SystemClock.sleep(16)
                    instrumentation.sendPointerSync(event(MotionEvent.ACTION_MOVE, down, ax + (bx - ax) * i / 62f, y - sh * 0.1f * i / 62f))
                    times += onUi { View3DNative.nativeState() }[10]
                }
                instrumentation.sendPointerSync(event(MotionEvent.ACTION_UP, down, bx, y - sh * 0.1f))
                rule.waitForIdle()
                shot("rig_character_3d_$k")
            }
            times.forEachIndexed { i, ms -> Log.i("ProjectGrease", "view3d rig orbit frame $i ${"%.2f".format(ms)} ms") }
            val sorted = times.sorted()
            Log.i("ProjectGrease", "view3d rig orbit (24-frame rig, 15 bones): ${times.size} moves, median ${sorted[sorted.size / 2]} ms, max ${sorted.last()} ms")
            assertNotEquals("the finger orbited the view", q0, onUi { View3DNative.nativeState() }.copyOfRange(3, 7).toList())
            assertTrue("3D frames were drawn", times.any { it > 0f })
        } finally {
            onUi { View3DUi.draw = false; View3DUi.setActive(false, controller); View3DNative.nativeReset() }
        }

        // 4. export with the app's MP4 export, copied next to the shots
        val file = File(rule.activity.cacheDir, "rig_character.mp4")
        val frames = onUi { com.smitnk.projectgrease.ui.VideoExport.exportToFile(controller, file) }
        shell("mkdir -p $SHOT_DIR"); shell("cp ${file.absolutePath} $SHOT_DIR/rig_character.mp4")
        assertEquals(24, frames)
        val r = android.media.MediaMetadataRetriever()
        try {
            r.setDataSource(file.absolutePath)
            assertEquals("24", r.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_FRAME_COUNT))
        } finally { r.release() }
    }
}
