package com.smitnk.projectgrease

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.RigController
import com.smitnk.projectgrease.editor.RigController.RigIds
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.cos
import kotlin.math.sin

/**
 * Rig constraints, IK and drivers on the device (native/blender_gp/project_grease_constraint.c,
 * project_grease_driver.c): a two-bone IK chain with a pole, Copy Rotation / Child Of through the
 * controller, the master slider (one custom property driving two bones, a layer's opacity and its
 * Time Offset modifier), undo / redo and save / reload. Positions are canvas px (y down); the host
 * test native/blender_gp/tests/test_constraints.cc derives the same geometry in armature space.
 */
@RunWith(AndroidJUnit4::class)
class SweepRigConstraintsTest : SweepBase() {

    private val rig get() = controller.rig

    private fun common(own: Int, tar: Int, vararg type: Float) =
        floatArrayOf(1f, own.toFloat(), tar.toFloat(), 0f, 0f, *type)

    @Test fun ikChainWithPoleAndConstraints() {
        val s = sin(0.2).toFloat() * 100f; val c = cos(0.2).toFloat() * 100f
        onUi {
            assertTrue(rig.create())
            assertEquals(0, rig.addBone(600f, 600f, 600f, 500f, "Upper"))
            assertEquals(1, rig.addBone(600f, 500f, 600f + s, 500f - c, "Lower"))
            rig.editSelect(1); rig.editSelect(0, extend = true)
            assertEquals(1, rig.parentSet(RigController.PARENT_CONNECTED))
            assertEquals(2, rig.addBone(700f, 500f, 700f, 450f, "Target"))
            assertEquals(3, rig.addBone(800f, 600f, 800f, 550f, "Pole"))
            assertTrue(rig.setMode(RigController.MODE_POSE))
            controller.history.markEdit()
            // IK on Lower: chain 2, use tail, no stretch, target + pole
            assertEquals(0, rig.addConstraint(1, RigIds.CON_IK))
            assertTrue(rig.setConstraintTarget(1, 0, "Target"))
            assertTrue(rig.setConstraintTarget(1, 0, "Pole", pole = true))
            assertTrue(rig.setConstraint(1, 0, common(RigIds.SPACE_WORLD, RigIds.SPACE_WORLD,
                (RigIds.IK_USE_TAIL or RigIds.IK_POSITION).toFloat(), 2f, 500f, 0f, 1f, 1f)))
            controller.history.markEdit()
        }
        // the tip reaches the target; with the pole on the bend side the elbow stays at (600, 500)
        val lower = onUi { rig.bone(1)!! }
        assertEquals(700f, lower.poseTailX, 0.1f); assertEquals(500f, lower.poseTailY, 0.1f)
        assertEquals(600f, lower.poseHeadX, 0.1f); assertEquals(500f, lower.poseHeadY, 0.1f)
        val v = onUi { rig.constraint(1, 0) }
        assertEquals(RigIds.CON_IK.toFloat(), v[0], 0f)
        assertEquals(2f, v[7], 0f) // chain count

        // Copy Rotation (Local / Local) of Target onto Pole, Target turned 30 degrees on screen
        onUi {
            assertEquals(0, rig.addConstraint(3, RigIds.CON_COPY_ROTATION))
            assertTrue(rig.setConstraintTarget(3, 0, "Target"))
            assertTrue(rig.setConstraint(3, 0, common(RigIds.SPACE_LOCAL, RigIds.SPACE_LOCAL,
                (RigIds.AXIS_X or RigIds.AXIS_Y or RigIds.AXIS_Z).toFloat(), 0f, RigIds.MIX_REPLACE.toFloat())))
            rig.poseSelect(2)
            rig.poseRotate((Math.PI / 6).toFloat())
            controller.history.markEdit()
        }
        val pole = onUi { rig.bone(3)!! }
        // 50 px bone pointing up, turned 30 degrees counter-clockwise on screen
        assertEquals(800f - 25f, pole.poseTailX, 0.2f); assertEquals(600f - 43.30f, pole.poseTailY, 0.2f)

        // undo the Copy Rotation step, redo it
        onUi { assertTrue(controller.undo()) }
        assertEquals(0, onUi { rig.constraintCount(3) })
        onUi { assertTrue(controller.redo()) }
        assertEquals(1, onUi { rig.constraintCount(3) })

        // save / reload keeps the constraints and their targets
        val saved = onUi { controller.saveDocumentJson() }!!
        val arm = JSONObject(saved).getJSONObject("armature")
        assertTrue(arm.getJSONArray("pose").getJSONObject(1).has("constraints"))
        onUi { assertTrue(controller.loadDocumentJson(saved)) }
        assertEquals(1, onUi { rig.constraintCount(1) })
        val again = onUi { rig.bone(1)!! }
        assertEquals(700f, again.poseTailX, 0.1f); assertEquals(500f, again.poseTailY, 0.1f)
        shot("rig_constraints_ik")
    }

    @Test fun childOfSetAndClearInverse() {
        onUi {
            rig.create()
            rig.addBone(400f, 600f, 400f, 500f, "Child")
            rig.addBone(600f, 600f, 600f, 500f, "Parent")
            rig.setMode(RigController.MODE_POSE)
            assertEquals(0, rig.addConstraint(0, RigIds.CON_CHILD_OF))
            assertTrue(rig.setConstraintTarget(0, 0, "Parent")) // the automatic Set Inverse
            rig.poseSelect(1)
            rig.poseRotate((Math.PI / 2).toFloat())
        }
        // the child turns 90 degrees about the parent head (600, 600): (400, 600) -> (600, 800)
        val child = onUi { rig.bone(0)!! }
        assertEquals(600f, child.poseHeadX, 0.2f); assertEquals(800f, child.poseHeadY, 0.2f)
        // Clear Inverse (childof_evaluate: parmat * owner): the rest offset turns with the parent,
        // (0.4, -0.6) -> (0.6, 0.4) + head (0.6, -0.6) = (1.2, -0.2) units = (1200, 200) px
        onUi { assertTrue(rig.childOfClearInverse(0, 0)) }
        assertEquals(1200f, onUi { rig.bone(0)!! }.poseHeadX, 0.2f)
        assertEquals(200f, onUi { rig.bone(0)!! }.poseHeadY, 0.2f)
        onUi { assertTrue(rig.childOfSetInverse(0, 0)) }
        assertEquals(400f, onUi { rig.bone(0)!! }.poseHeadX, 0.2f)
        assertEquals(600f, onUi { rig.bone(0)!! }.poseHeadY, 0.2f)
    }

    @Test fun masterSliderDrivesBonesAndDrawingSwap() {
        onUi {
            assertTrue(controller.createLayer("Mouth"))
            val mouth = controller.selectedLayer
            assertTrue(controller.addModifier(ModifierType.TIME, mouth))
            assertTrue(rig.create())
            rig.addBone(400f, 600f, 400f, 500f, "Brow.L")
            rig.addBone(800f, 600f, 800f, 500f, "Brow.R")
            assertEquals(0, rig.addProperty("slider", value = 0f, min = 0f, max = 1f, default = 0f))
            val drivers = listOf(
                Triple(rig.bonePath("Brow.L", "location"), 1, "var*0.1"),
                Triple(rig.bonePath("Brow.R", "location"), 1, "-var"),
                Triple(rig.timeOffsetPath("Mouth", 0), 0, "var*3.99"),
                Triple(rig.layerPath("Mouth", "opacity"), 0, "var"))
            for ((path, index, expr) in drivers) {
                val d = rig.addDriver(path, index)
                assertTrue("driver on $path", d >= 0)
                assertEquals(0, rig.addDriverVariable(d))
                assertTrue(rig.setDriverVariablePath(d, 0, rig.objectPropertyPath("slider")))
                assertTrue(rig.setDriverExpression(d, expr))
            }
            assertEquals(4, rig.driverCount)
            controller.history.markEdit()
        }
        val mouth = onUi { controller.selectedLayer }
        for ((slider, offset) in listOf(0f to 0f, 0.3f to 1f, 0.75f to 2f, 1f to 3f)) {
            onUi { assertTrue(rig.setProperty(0, slider)); controller.history.markEdit() }
            assertEquals(slider * 0.1f, onUi { rig.pose(0) }[1], 1e-5f)
            assertEquals(-slider, onUi { rig.pose(1) }[1], 1e-5f)
            val time = onUi { controller.modifiers(mouth) }.first { it.type == ModifierType.TIME }
            assertEquals("Time Offset frame offset at slider $slider", offset, time.params[1], 0f)
            val layers = document().getJSONArray("layers")
            val layer = (0 until layers.length()).map { layers.getJSONObject(it) }.first { it.optString("name") == "Mouth" }
            assertEquals(slider.toDouble(), layer.getDouble("opacity"), 1e-5)
        }
        // a frame change evaluates the drivers too
        onUi { controller.animation.setFrame(5) }
        assertEquals(3f, onUi { controller.modifiers(mouth) }.first { it.type == ModifierType.TIME }.params[1], 0f)
        // undo the last slider step: 0.75 again, the drawing swap follows
        onUi { assertTrue(controller.undo()) }
        assertEquals(0.75f, onUi { rig.property(0) }[1], 1e-6f)
        onUi { rig.evaluate(5f) }
        assertEquals(2f, onUi { controller.modifiers(mouth) }.first { it.type == ModifierType.TIME }.params[1], 0f)
        // save / reload keeps the property and the drivers
        val saved = onUi { controller.saveDocumentJson() }!!
        val arm = JSONObject(saved).getJSONObject("armature")
        assertEquals(4, arm.getJSONArray("drivers").length())
        assertEquals("slider", arm.getJSONArray("props").getJSONObject(0).getString("name"))
        onUi { assertTrue(controller.loadDocumentJson(saved)) }
        onUi { assertTrue(rig.setProperty(0, 0.3f)) }
        assertEquals(1f, onUi { controller.modifiers(mouth) }.first { it.type == ModifierType.TIME }.params[1], 0f)
        shot("rig_master_slider")
    }
}
