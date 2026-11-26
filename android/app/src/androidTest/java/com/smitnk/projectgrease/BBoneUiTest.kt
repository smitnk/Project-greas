package com.smitnk.projectgrease

import android.graphics.Bitmap
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextReplacement
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.RigController
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Bone properties > Bendy Bones in the rig panel (ui/RigPanel.kt BendyBonesPanel) and Display As
 * B-Bone (android_gp_armature_overlay.inc draw_bone_box): Segments and Curve In / Out X set the
 * native rig and the saved document, and the bent B-Bone draws differently on the canvas.
 */
@RunWith(AndroidJUnit4::class)
class BBoneUiTest : SweepBase() {
    private val rig get() = controller.rig

    private fun click(tag: String) {
        val n = rule.onNodeWithTag(tag)
        try { n.performScrollTo() } catch (_: AssertionError) {}
        n.performClick(); rule.waitForIdle()
    }
    private fun type(tag: String, text: String) {
        rule.onNodeWithTag(tag).performScrollTo().performTextReplacement(text); rule.waitForIdle()
    }
    private fun openRig() { rule.onNodeWithContentDescription("Menu").performClick(); click("menu_rig") }
    private fun panelOpen() = listOf("rigAdd", "rigBindEmpty").any { rule.onAllNodesWithTag(it).fetchSemanticsNodes().isNotEmpty() }
    private fun back() {
        repeat(3) {
            if (it == 0 || panelOpen()) {
                instrumentation.sendKeyDownUpSync(android.view.KeyEvent.KEYCODE_BACK); rule.waitForIdle()
                runCatching { rule.waitUntil(1500) { !panelOpen() } }
            }
        }
    }
    private fun savedBone() = document().getJSONObject("armature").getJSONArray("bones").getJSONObject(0)

    /** screen pixels that differ between [a] and [b] inside the canvas box */
    private fun changed(a: Bitmap, b: Bitmap, x0: Float, y0: Float, x1: Float, y1: Float): Int {
        val p0 = screen(x0, y0); val p1 = screen(x1, y1)
        var n = 0
        for (y in minOf(p0.second, p1.second).toInt()..maxOf(p0.second, p1.second).toInt())
            for (x in minOf(p0.first, p1.first).toInt()..maxOf(p0.first, p1.first).toInt())
                if (x in 0 until a.width && y in 0 until a.height && a.getPixel(x, y) != b.getPixel(x, y)) n++
        return n
    }

    @Test fun segmentsAndCurveFromThePanel() {
        val (w, h) = onUi { controller.document.canvasWidth.toFloat() to controller.document.canvasHeight.toFloat() }
        val cx = w / 2f; val cy = h / 2f
        openRig(); click("rigAdd")
        click("rigBoneAdd")
        assertEquals(1, onUi { rig.boneCount })
        // Blender's defaults (rna_armature.c / ED_armature_ebone_add)
        onUi { rig.bbone(0)!! }.let {
            assertEquals(1, it.segments); assertEquals(1f, it.values[RigController.BB_EASE_IN], 1e-6f)
            assertEquals(1f, it.values[RigController.BB_SCALE_IN], 1e-6f); assertEquals(0f, it.values[RigController.BB_CURVE_IN_X], 1e-6f)
        }
        click("rigDisplayBBone")
        assertEquals(RigController.DISPLAY_BBONE, onUi { rig.displayType })
        assertEquals(RigController.DISPLAY_BBONE, document().getJSONObject("armature").getInt("drawtype"))

        // Segments: the field and the + button (bbone_segments 1..32)
        type("rigBBoneSegments", "7")
        click("rigBBoneSegmentsMore")
        assertEquals(8, onUi { rig.bbone(0)!!.segments })
        type("rigBBoneSegments", "99")
        assertEquals(32, onUi { rig.bbone(0)!!.segments })
        type("rigBBoneSegments", "8")
        click("rigBBoneDone")
        assertEquals(8, savedBone().getInt("segments"))
        // a thin box so the bend leaves the straight bone's footprint
        type("rigBBoneDisplayX", "0.01"); type("rigBBoneDisplayZ", "0.01")
        click("rigBBoneDone")
        back()
        val straight = shot("bbone_straight")

        openRig()
        type("rigBBoneCurveInX", "0.04")
        type("rigBBoneCurveOutX", "0.04")
        click("rigBBoneDone")
        onUi { rig.bbone(0)!! }.let {
            assertEquals(0.04f, it.values[RigController.BB_CURVE_IN_X], 1e-5f)
            assertEquals(0.04f, it.values[RigController.BB_CURVE_OUT_X], 1e-5f)
            assertEquals(8, it.segments) // the curve fields keep the segments
        }
        savedBone().getJSONArray("bbone").let {
            assertEquals(0.04, it.getDouble(RigController.BB_CURVE_IN_X), 1e-5)
            assertEquals(0.04, it.getDouble(RigController.BB_CURVE_OUT_X), 1e-5)
        }
        back()
        val bent = shot("bbone_bent")
        val diff = changed(straight, bent, cx - 80f, cy - 110f, cx + 80f, cy + 10f)
        assertTrue("the bent B-Bone draws differently ($diff px changed)", diff > 40)

        // Pose mode: the panel edits the pose bone's values, the rest values stay
        rule.onNodeWithTag("studioMode").performClick(); click("modeChip_POSE")
        assertEquals(RigController.MODE_POSE, onUi { rig.mode })
        openRig()
        type("rigBBoneCurveInZ", "0.02")
        click("rigBBoneDone")
        assertEquals(0.02f, onUi { rig.poseBBone(0)!![RigController.BB_CURVE_IN_Z] }, 1e-5f)
        assertEquals(0f, onUi { rig.bbone(0)!!.values[RigController.BB_CURVE_IN_Z] }, 1e-6f)
        back()
    }
}
