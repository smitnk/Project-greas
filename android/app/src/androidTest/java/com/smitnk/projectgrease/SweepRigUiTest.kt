package com.smitnk.projectgrease

import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.performSemanticsAction
import androidx.compose.ui.semantics.SemanticsActions
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GreaseTool
import com.smitnk.projectgrease.editor.ModifierType
import com.smitnk.projectgrease.editor.RigController
import com.smitnk.projectgrease.editor.RigController.RigIds
import com.smitnk.projectgrease.ui.addSwap
import androidx.compose.ui.test.onAllNodesWithTag
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Rigging on the phone through the studio UI (ui/RigPanel.kt, nativebridge/RigTouch.kt): modes, the
 * armature panel, pose tools on the canvas, bone keys in the dope sheet, a drawing swap and a master
 * slider. Each step asserts the saved document (key "armature") or the native rig.
 */
@RunWith(AndroidJUnit4::class)
class SweepRigUiTest : SweepBase() {
    private val rig get() = controller.rig

    // performScrollTo throws for nodes outside a scroll container (e.g. the timeline key buttons)
    private fun click(tag: String) {
        val n = rule.onNodeWithTag(tag)
        try { n.performScrollTo() } catch (_: AssertionError) {}
        n.performClick(); rule.waitForIdle()
    }
    private fun openRig() { rule.onNodeWithContentDescription("Menu").performClick(); click("menu_rig") }
    // the first BACK only hides the keyboard when a text field (bone name, slider name) still has it
    private fun panelOpen() = listOf("rigAdd", "rigBindEmpty").any { rule.onAllNodesWithTag(it).fetchSemanticsNodes().isNotEmpty() }
    private fun back() {
        repeat(3) { if (it == 0 || panelOpen()) { instrumentation.sendKeyDownUpSync(android.view.KeyEvent.KEYCODE_BACK); rule.waitForIdle() } }
    }
    private fun mode(chip: String) { rule.onNodeWithTag("studioMode").performClick(); click("modeChip_$chip") }
    private fun armature() = document().getJSONObject("armature")
    private fun boneNames(): List<String> = armature().getJSONArray("bones").let { a -> (0 until a.length()).map { a.getJSONObject(it).getString("name") } }

    @Test fun rigFromTheUi() {
        val (w, h) = onUi { controller.document.canvasWidth.toFloat() to controller.document.canvasHeight.toFloat() }
        val cx = w / 2f; val cy = h / 2f

        // 1. Pose needs an armature; Object mode works without one
        mode("POSE"); assertEquals("DRAW", onUi { controller.studioModeName })
        mode("OBJECT"); assertEquals("OBJECT", onUi { controller.studioModeName })

        // 2. armature panel: add, add bone, extrude, rename, parent, display, In Front
        openRig(); click("rigAdd")
        assertTrue(onUi { rig.exists && controller.rigActive })
        assertEquals("EDIT", onUi { controller.studioModeName })
        click("rigBoneAdd")
        assertEquals(1, onUi { rig.boneCount })
        click("rigExtrude")
        assertEquals(2, onUi { rig.boneCount })
        rule.onNodeWithTag("rigBoneName").performScrollTo().performTextReplacement("Jaw")
        click("rigRename")
        assertTrue("Jaw" in boneNames())
        assertEquals(0, onUi { rig.bone(1)!!.parent }) // extrude connects to the parent
        click("rigDisplayStick")
        assertEquals(RigController.DISPLAY_STICK, onUi { rig.displayType })
        click("rigInFront")
        assertTrue(onUi { rig.showInFront })

        // bind: Empty Groups makes one vertex group per deform bone and the Armature modifier
        click("rigBindEmpty")
        assertTrue(onUi { controller.vertexGroups() }.containsAll(boneNames()))
        assertTrue(onUi { controller.modifiers() }.any { it.type == ModifierType.ARMATURE })

        // constraints on Jaw (active): every type, IK chain length + target, influence
        listOf(RigIds.CON_IK, RigIds.CON_COPY_ROTATION, RigIds.CON_COPY_LOCATION, RigIds.CON_DAMPED_TRACK,
            RigIds.CON_LIMIT_ROTATION, RigIds.CON_CHILD_OF).forEach { click("rigConAdd_$it") }
        assertEquals(6, onUi { rig.constraintCount(1) })
        click("rigConChainMore_0")
        assertEquals(1f, onUi { rig.constraint(1, 0) }[7], 0f)
        click("rigConOwner_1_${RigIds.SPACE_LOCAL}")
        assertEquals(RigIds.SPACE_LOCAL.toFloat(), onUi { rig.constraint(1, 1) }[2], 0f)
        rule.onNodeWithTag("rigConInfluence_1").performScrollTo()
            .performSemanticsAction(SemanticsActions.SetProgress) { it(0.5f) }
        rule.waitForIdle()
        assertEquals(0.5f, onUi { rig.constraint(1, 1) }[1], 1e-3f)
        assertTrue(armature().getJSONArray("pose").getJSONObject(1).getJSONArray("constraints").length() == 6)
        // remove all but IK so posing below is plain
        repeat(5) { click("rigConRemove_1") }
        assertEquals(1, onUi { rig.constraintCount(1) })
        click("rigConRemove_0")
        back()

        // 1. modes: Pose / Weight Paint (the active bone's group) / back to Pose
        mode("POSE"); assertEquals("POSE", onUi { controller.studioModeName })
        mode("WEIGHT_PAINT")
        assertEquals("WEIGHT_PAINT", onUi { controller.studioModeName })
        assertEquals(onUi { controller.vertexGroups().indexOf(rig.bone(rig.activeBone)!!.name) }, onUi { controller.weightPaintGroup })
        mode("OBJECT"); tap(cx, cy - 50f) // a tap on a bone makes the armature active
        assertTrue(onUi { controller.rigActive })
        tap(40f, 40f); assertTrue(!onUi { controller.rigActive })
        tap(cx, cy - 50f); mode("POSE")

        // 3. pose tools: tap select, drag rotate, Insert Key, Copy / Paste X-Flipped, Clear
        tap(cx, cy - 50f)
        assertEquals(0, onUi { rig.activeBone })
        onUi { controller.selectTool(GreaseTool.ROTATE) }
        drag(cx to cy - 50f, cx + 50f to cy)
        val turned = onUi { rig.bone(0)!! }
        assertTrue("root bone turned: ${turned.poseTailX}", turned.poseTailX > cx + 10f)
        openRig(); click("rigInsertKey")
        assertEquals(listOf(1f), onUi { controller.animation.boneKeys(0).toList() })
        click("rigCopyPose"); click("rigClearPose")
        assertEquals(cx, onUi { rig.bone(0)!!.poseTailX }, 0.5f)
        click("rigPasteFlipped")
        assertTrue(onUi { rig.bone(0)!!.poseTailX } < cx - 10f)
        click("rigPastePose")
        assertTrue(onUi { rig.bone(0)!!.poseTailX } > cx + 10f)
        onUi { controller.selectTool(GreaseTool.MOVE) }
        back()
        drag(cx to cy - 50f, cx + 30f to cy - 50f)
        assertTrue(onUi { rig.pose(0) }[0] != 0f)

        // 4. dope sheet: bone keys in the timeline, move a key, interpolation
        onUi { controller.animation.setFrame(5); rig.insertKey(5); controller.history.markEdit() }
        rule.onNodeWithContentDescription("Menu").performClick(); click("menu_timeline")
        click("boneKey_Bone_5")
        click("boneKeyRight")
        assertEquals(listOf(1f, 6f), onUi { controller.animation.boneKeys(0).toList() })
        click("boneKeyIpo_Constant")
        assertEquals(RigController.IPO_CONSTANT, onUi { rig.interpolation(0, 6) })
        click("boneKeyIpo_Linear")
        assertEquals(RigController.IPO_LINEAR, onUi { rig.interpolation(0, 6) })
        back()

        // 5. drawing swap on a "Mouth" layer with 3 drawings: Fixed Frame Time Offset driven by Swap.Mouth
        onUi {
            assertTrue(controller.createLayer("Mouth"))
            for (f in 1..3) controller.createFrame(f)
            controller.animation.setFrame(1)
        }
        openRig(); click("rigSwapAdd")
        val mouth = onUi { controller.selectedLayer }
        val time = { onUi { controller.modifiers(mouth) }.first { it.type == ModifierType.TIME } }
        assertEquals(2f, time().params[0], 0f) // Fixed Frame
        assertTrue(onUi { rig.findBone("Swap.Mouth") } >= 0)
        click("rigSwap_3")
        assertEquals(3f, time().params[1], 0f)
        click("rigSwap_2")
        assertEquals(2f, time().params[1], 0f)

        // keyable: key the swap bone at 1 (drawing 2) and 10 (drawing 3); constant keys hold
        onUi {
            rig.setMode(RigController.MODE_POSE)
            val sb = rig.findBone("Swap.Mouth"); rig.poseSelect(sb)
            rig.insertKey(1); rig.setSwap("Mouth", 3); rig.insertKey(10)
            rig.setInterpolation(sb, 1, RigController.IPO_CONSTANT)
            controller.animation.setFrame(5)
        }
        assertEquals(2f, time().params[1], 0f)
        onUi { controller.animation.setFrame(10) }
        assertEquals(3f, time().params[1], 0f)
        onUi { controller.animation.setFrame(1) }

        // 6. master slider "Smile": drives Bone rotation, Jaw Loc Y and the swap
        rule.onNodeWithTag("rigSliderName").performScrollTo().performTextReplacement("Smile")
        click("rigSliderAdd")
        assertEquals("Smile", armature().getJSONArray("props").getJSONObject(0).getString("name"))
        click("rigLinkBone_0_Bone"); click("rigLinkRot_0")
        click("rigLinkBone_0_Jaw"); click("rigLinkLocY_0")
        click("rigLinkBone_0_Swap.Mouth"); click("rigLinkSwap_0")
        assertEquals(4, armature().getJSONArray("drivers").length()) // w + z, Jaw, the swap driver
        rule.onNodeWithTag("rigSlider_Smile").performScrollTo()
            .performSemanticsAction(SemanticsActions.SetProgress) { it(1f) }
        rule.waitForIdle()
        assertEquals(1f, onUi { rig.pose(1) }[1], 1e-4f)                 // Jaw Loc Y = Smile * 1
        assertEquals(kotlin.math.sin(0.5f), onUi { rig.pose(0) }[6], 1e-3f) // quat z = sin(1 rad / 2)
        assertEquals(3f, time().params[1], 0f)                           // round(2 + 1)
        back()
        shot("rig_ui_master_slider")

        // undo / save / reload keep the rig
        onUi { assertTrue(controller.undo()) }
        assertEquals(0f, onUi { rig.property(0) }[1], 1e-6f)
        val saved = onUi { controller.saveDocumentJson() }!!
        onUi { assertTrue(controller.loadDocumentJson(saved)) }
        assertTrue(boneNames().containsAll(listOf("Bone", "Jaw", "Swap.Mouth")))
    }

    @Test fun addSwapHelperThroughController() {
        onUi {
            assertTrue(controller.createLayer("Eyes"))
            assertTrue(rig.create())
            assertTrue(addSwap(controller, controller.selectedLayer) >= 0)
            assertTrue(rig.setSwap("Eyes", 4))
            controller.history.markEdit()
        }
        val t = onUi { controller.modifiers() }.first { it.type == ModifierType.TIME }
        assertEquals(4f, t.params[1], 0f)
        assertEquals(1, armature().getJSONArray("drivers").length())
    }
}
