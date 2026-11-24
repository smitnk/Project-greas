package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.RigController
import com.smitnk.projectgrease.nativebridge.View3DNative
import com.smitnk.projectgrease.nativebridge.View3DUi
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs

/**
 * Bone overlay (native/blender_gp/android_gp_armature_overlay.inc, Blender 3.6.23
 * overlay_armature.cc): Octahedral and Stick display with the default theme's edit / pose
 * selection colours, In Front over the strokes, the display saved with the rig, and tap select
 * (RigController.pick, ED_view3d_select_dist_px) in the Camera view and the 3D view.
 */
@RunWith(AndroidJUnit4::class)
class SweepRigOverlayTest : SweepBase() {
    private val rig get() = controller.rig
    private val ys = floatArrayOf(300f, 500f, 700f)

    @After fun back2d() = onUi { View3DUi.draw = false; View3DUi.setActive(false, controller); View3DNative.nativeReset() }

    private fun near(c: Int, r: Int, g: Int, b: Int, tol: Int = 24) =
        abs(Color.red(c) - r) <= tol && abs(Color.green(c) - g) <= tol && abs(Color.blue(c) - b) <= tol

    /** Three bones (x 300 .. 800); bone 1 selected, bone 2 active and selected. */
    private fun threeBones(mode: Int) = onUi {
        assertTrue(rig.create())
        for (y in ys) rig.addBone(300f, y, 800f, y)
        assertTrue(rig.setMode(mode))
        if (mode == RigController.MODE_EDIT) { rig.editSelect(1); rig.editSelect(2, extend = true) }
        else { rig.poseSelect(1); rig.poseSelect(2, extend = true) }
        assertEquals(2, rig.activeBone)
        controller.history.markEdit()
    }

    /** The outline of bone b: near its widest point (10 % of the length, 0.1 x length off the axis). */
    private fun hasColour(bmp: android.graphics.Bitmap, b: Int, r: Int, g: Int, bl: Int) =
        inkNear(bmp, 350f, ys[b] - 50f, 18) { near(it, r, g, bl) } || inkNear(bmp, 350f, ys[b] + 50f, 18) { near(it, r, g, bl) }

    @Test fun octahedralEditAndPoseColours() {
        threeBones(RigController.MODE_EDIT)
        assertEquals(RigController.DISPLAY_OCTAHEDRAL, onUi { rig.displayType }) // DNA default
        assertFalse(onUi { rig.showInFront })
        var bmp = shot("rig_overlay_octa_edit")
        // DRW_globals_update: bone_active = edge_select +60, bone_select = edge_select -20
        assertTrue("active edit bone", hasColour(bmp, 2, 255, 220, 60))
        assertTrue("selected edit bone", hasColour(bmp, 1, 235, 140, 0))
        assertFalse("normal edit bone has no selection colour", hasColour(bmp, 0, 235, 140, 0) || hasColour(bmp, 0, 255, 220, 60))
        // the octahedron solid: lit bone_solid grey at the bone's widest point
        val c = pixel(bmp, 350f, ys[0])
        assertTrue("grey solid ${Integer.toHexString(c)}", abs(Color.red(c) - Color.blue(c)) < 16 && Color.red(c) in 70..210)

        onUi { rig.setMode(RigController.MODE_POSE); rig.poseSelect(1); rig.poseSelect(2, extend = true); controller.history.markEdit() }
        bmp = shot("rig_overlay_octa_pose")
        assertTrue("active pose bone (bone_pose_active)", hasColour(bmp, 2, 140, 255, 255))
        assertTrue("selected pose bone (bone_pose)", hasColour(bmp, 1, 80, 200, 255))
        assertFalse("normal pose bone", hasColour(bmp, 0, 80, 200, 255) || hasColour(bmp, 0, 140, 255, 255))
    }

    @Test fun stickDisplaySavedWithTheRig() {
        threeBones(RigController.MODE_POSE)
        onUi { assertTrue(rig.setDisplay(RigController.DISPLAY_STICK, false)); controller.history.markEdit() }
        val bmp = shot("rig_overlay_stick_pose")
        // overlay_armature_stick: the wire around the stick in the pose colours, 10 px from the axis
        assertTrue("active stick", inkNear(bmp, 550f, ys[2], 14) { near(it, 140, 255, 255) })
        assertTrue("selected stick", inkNear(bmp, 550f, ys[1], 14) { near(it, 80, 200, 255) })
        assertFalse("no octahedron at the widest point", inkNear(bmp, 350f, ys[1] - 50f, 4) { near(it, 80, 200, 255) })
        val saved = onUi { controller.saveDocumentJson() }!!
        val arm = JSONObject(saved).getJSONObject("armature")
        assertEquals(RigController.DISPLAY_STICK, arm.getInt("drawtype"))
        assertEquals(0, arm.getInt("in_front"))
        onUi { assertTrue(controller.loadDocumentJson(saved)) }
        assertEquals(RigController.DISPLAY_STICK, onUi { rig.displayType })
    }

    @Test fun inFrontDrawsOverTheStrokes() {
        drag(250f to ys[0], 550f to ys[0], 850f to ys[0]) // a stroke along bone 0
        threeBones(RigController.MODE_POSE)
        var bmp = shot("rig_overlay_behind")
        assertTrue("bone under the stroke", isDark(pixel(bmp, 350f, ys[0])))
        onUi { assertTrue(rig.setDisplay(RigController.DISPLAY_OCTAHEDRAL, true)); controller.history.markEdit() }
        assertTrue(onUi { rig.showInFront })
        bmp = shot("rig_overlay_in_front")
        assertFalse("bone over the stroke", isDark(pixel(bmp, 350f, ys[0])))
    }

    @Test fun tapSelectCameraAnd3dView() {
        threeBones(RigController.MODE_POSE)
        // Camera view: canvas px
        assertEquals(0, onUi { rig.pick(550f, ys[0] + 20f, RigController.SELECT_DIST_PX) })
        assertTrue(onUi { rig.isSelected(0) && !rig.isSelected(1) && !rig.isSelected(2) })
        assertEquals(0, onUi { rig.activeBone })
        assertEquals(-1, onUi { rig.pick(550f, 1000f, RigController.SELECT_DIST_PX) })
        assertTrue("a tap on nothing deselects", onUi { (0 until 3).none { rig.isSelected(it) } })
        // edit mode: a tap on the tail joint selects that end only
        onUi { rig.setMode(RigController.MODE_EDIT) }
        assertEquals(1, onUi { rig.pick(803f, ys[1], RigController.SELECT_DIST_PX) })
        val flag = onUi { rig.bone(1)!!.flag }
        assertEquals(RigController.SELECT_TAIL, flag and (RigController.SELECT_BONE or RigController.SELECT_HEAD or RigController.SELECT_TAIL))
        // 3D view: view px through the view's canvas matrix
        onUi { rig.setMode(RigController.MODE_POSE); View3DUi.draw = false; View3DUi.setActive(true, controller) }
        rule.waitForIdle()
        shot("rig_overlay_3d")
        val s = onUi { View3DNative.nativeCanvasToScreen(550f, ys[2], 0f) }!!
        val m = onUi { View3DNative.nativeCanvasMatrix() }
        assertEquals(2, onUi { rig.pick(s[0], s[1], RigController.SELECT_DIST_PX, false, m) })
        assertTrue(onUi { rig.isSelected(2) && !rig.isSelected(0) })
    }
}
