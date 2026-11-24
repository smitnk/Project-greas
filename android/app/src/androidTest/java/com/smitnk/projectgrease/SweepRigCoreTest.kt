package com.smitnk.projectgrease

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.RigController
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.abs
import kotlin.math.hypot

/**
 * Rig core on the device (native/blender_gp/project_grease_armature*): armature and bones, bind with
 * automatic weights, pose, LocRotScale keys, scrub, undo / redo, save / reload. The deform is
 * checked by applying the Armature modifier (the same evaluation the canvas draws) and undoing it.
 */
@RunWith(AndroidJUnit4::class)
class SweepRigCoreTest : SweepBase() {

    private val rig get() = controller.rig

    /** First point of the first stroke as saved. */
    private fun firstPoint(doc: JSONObject = document()) = points(strokes(doc)[0])[0]

    @Test fun armatureBindPoseKeyScrubUndoSave() {
        // a vertical stroke along where the bone will be
        drag(600f to 600f, 600f to 500f, 600f to 400f, 600f to 300f)
        val before = firstPoint()
        // the tip: the root's head (600, 600) is the rotation pivot and does not move
        val tipBefore = points(strokes(document())[0]).last()

        // armature with a bone along the stroke and an extruded, connected child
        onUi {
            assertTrue(rig.create())
            assertEquals(0, rig.addBone(600f, 600f, 600f, 300f, "Arm.L"))
            assertEquals(1, rig.extrude(200f, 0f))
            controller.history.markEdit()
        }
        val bones = onUi { rig.bones() }
        assertEquals(2, bones.size)
        assertEquals("Arm.L.001", bones[1].name) // ED_armature_ebone_unique_name
        assertEquals(0, bones[1].parent)
        assertTrue("extruded bone is connected", bones[1].connected)
        assertEquals(600f, bones[1].headX, 0.01f); assertEquals(300f, bones[1].headY, 0.01f)
        assertEquals(800f, bones[1].tailX, 0.01f)

        // Parent with Automatic Weights: groups named after the bones, weights on the stroke, an
        // Armature modifier on the layer; the groups are the ones Weight Paint edits
        onUi {
            assertTrue(rig.bind(RigController.BIND_AUTOMATIC) >= 2)
            controller.history.markEdit()
        }
        val doc = document()
        val groups = doc.getJSONArray("vertexGroups")
        assertEquals("Arm.L", groups.getString(0))
        assertEquals("Arm.L.001", groups.getString(1))
        assertTrue("stroke carries weights", strokes(doc)[0].has("weights"))
        val mods = onUi { controller.modifiers() }
        val armIndex = mods.indexOfFirst { it.type == ModifierType.ARMATURE }
        assertTrue("Armature modifier added: $mods", armIndex >= 0)

        // pose: rotate the root 90 degrees, key it at frame 1 (rest) and frame 10 (rotated)
        onUi {
            assertTrue(rig.setMode(RigController.MODE_POSE))
            rig.poseSelect(0)
            assertTrue(rig.insertKey(1) > 0)
            assertEquals(1, rig.poseRotate((Math.PI / 2).toFloat()))
            assertTrue(rig.insertKey(10) > 0)
            controller.history.markEdit()
        }
        assertTrue(onUi { controller.animation.boneKeys(0).toList() } == listOf(1f, 10f))
        val rotated = onUi { rig.bone(1)!! }
        // root turned counter-clockwise on screen: its tail (600, 300) goes to (300, 600)
        assertEquals(300f, rotated.poseHeadX, 0.5f); assertEquals(600f, rotated.poseHeadY, 0.5f)

        // scrub: frame 1 is the rest pose, halfway is between, frame 10 rotated again
        onUi { controller.animation.setFrame(1) }
        val atRest = onUi { rig.bone(1)!! }
        assertEquals(600f, atRest.poseHeadX, 0.5f); assertEquals(300f, atRest.poseHeadY, 0.5f)
        onUi { controller.animation.setFrame(5) }
        val mid = onUi { rig.bone(1)!! }
        assertTrue("in-between pose: $mid", mid.poseHeadX in 301f..599f)
        assertEquals("bone length kept", 300f, hypot(mid.poseHeadX - 600f, mid.poseHeadY - 600f), 0.5f)
        onUi { controller.animation.setFrame(10) }
        assertEquals(300f, onUi { rig.bone(1)!! }.poseHeadX, 0.5f)

        // the Armature modifier deforms the stroke: bake it and compare, then undo the bake
        onUi { assertTrue(controller.applyLayerModifier(armIndex)) }
        val pivot = firstPoint()
        val tip = points(strokes(document())[0]).last()
        assertEquals("pivot stays: ${before.toList()} -> ${pivot.toList()}", before[0], pivot[0], 0.5)
        assertEquals("pivot stays: ${before.toList()} -> ${pivot.toList()}", before[1], pivot[1], 0.5)
        // a quarter turn about the head: the tip (600, 300) swings to (300, 600)
        assertTrue("stroke deformed: ${tipBefore.toList()} -> ${tip.toList()}", abs(tip[0] - tipBefore[0]) > 100.0 || abs(tip[1] - tipBefore[1]) > 100.0)
        onUi { assertTrue(controller.undo()) }
        assertEquals(before[0], firstPoint()[0], 0.01)

        // undo / redo of the rig: undo the keys + rotation (one step), then redo
        val keyed = onUi { rig.keys(0).size }
        onUi { assertTrue(controller.undo()) }
        assertEquals(0, onUi { rig.keys(0).size })
        onUi { assertTrue(controller.redo()) }
        assertEquals(keyed, onUi { rig.keys(0).size })
        onUi { assertTrue(controller.undo()); assertTrue(controller.undo()); assertTrue(controller.undo()) }
        assertEquals("undo past the armature: no rig", -1, onUi { rig.boneCount })
        onUi { assertTrue(controller.redo()); assertTrue(controller.redo()); assertTrue(controller.redo()) }
        assertEquals(2, onUi { rig.boneCount })

        // save / reload keeps bones, weights, keys and the pose at the frame
        val saved = onUi { controller.saveDocumentJson() }
        assertNotNull(saved)
        assertTrue(JSONObject(saved!!).has("armature"))
        onUi { assertTrue(controller.loadDocumentJson(saved)) }
        assertEquals(2, onUi { rig.boneCount })
        assertEquals("Arm.L.001", onUi { rig.bone(1)!!.name })
        assertEquals(listOf(1f, 10f), onUi { rig.keys(0).toList() })
        assertEquals(RigController.MODE_POSE, onUi { rig.mode })
        onUi { controller.animation.setFrame(10) }
        assertEquals(300f, onUi { rig.bone(1)!! }.poseHeadX, 0.5f)
        assertTrue(onUi { controller.modifiers() }.any { it.type == ModifierType.ARMATURE })
        shot("rig_core_reloaded")
    }

    @Test fun editAndPoseOperators() {
        onUi {
            rig.create()
            rig.addBone(400f, 600f, 400f, 400f, "Hand.L")
            rig.addBone(800f, 600f, 800f, 400f, "Hand.R")
            // keep offset parent, then clear
            rig.editSelect(1); rig.editSelect(0, extend = true) // active Hand.L
            assertEquals(1, rig.parentSet(RigController.PARENT_KEEP_OFFSET))
        }
        assertEquals(0, onUi { rig.bone(1)!!.parent })
        onUi { rig.editSelect(1); rig.parentClear(RigController.CLEAR_PARENT) }
        assertEquals(-1, onUi { rig.bone(1)!!.parent })
        // rename with the unique-name rule
        onUi { assertTrue(rig.rename(1, "Hand.L")) }
        assertEquals("Hand.L.001", onUi { rig.bone(1)!!.name })
        onUi { rig.rename(1, "Hand.R") }
        // paste X-flipped: Hand.L's pose goes onto Hand.R with x mirrored
        onUi {
            rig.setMode(RigController.MODE_POSE)
            rig.poseSelect(0)
            rig.poseTranslate(50f, 0f)
            rig.poseCopy()
            assertEquals(1, rig.posePaste(flipped = true))
        }
        val l = onUi { rig.pose(0) }; val r = onUi { rig.pose(1) }
        assertEquals(-l[0], r[0], 1e-5f)
        // clear transforms
        onUi { rig.poseSelect(0); rig.poseClear(RigController.CLEAR_LOCATION or RigController.CLEAR_ROTATION or RigController.CLEAR_SCALE) }
        assertEquals(0f, onUi { rig.pose(0) }[0], 0f)
        // delete in edit mode
        onUi { rig.setMode(RigController.MODE_EDIT); rig.editSelect(1); assertEquals(1, rig.deleteSelected()) }
        assertEquals(1, onUi { rig.boneCount })
    }
}
