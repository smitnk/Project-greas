package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.RigController
import com.smitnk.projectgrease.ui.addSwap
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Onion ghosts of a driven drawing swap (project_grease_depsgraph per-frame GP copy-on-write): keys
 * 1, 2, 3 hold drawings 3, 2, 1 (drawing d: a line at y = 200 + 200 d); the swap is keyed so frame f
 * shows drawing f. With the scene at 2 and onion 1 / 1, the ghost before (Blender's green
 * gcolor_prev) shows drawing 1 (y 400) and the ghost after (blue gcolor_next) drawing 3 (y 800).
 * Without the per-frame swap the ghosts would be their own keys: green at 800, blue at 400.
 */
@RunWith(AndroidJUnit4::class)
class OnionSwapTest : SweepBase() {
    private fun greenish(c: Int) = Color.green(c) > Color.blue(c) + 20 && Color.red(c) < 230
    private fun bluish(c: Int) = Color.blue(c) > Color.green(c) + 20 && Color.red(c) < 230

    @Test fun ghostsShowTheSwapOfTheirFrame() {
        onUi { controller.selectMaterial(0); controller.brushes.setSize(30f) }
        for (k in 1..3) {
            onUi { if (k > 1) controller.createFrame(k); controller.selectFrame(k) }
            line(200f + 200f * (4 - k))
        }
        val layer = onUi { controller.selectedLayer }
        val rig = controller.rig
        onUi {
            assertTrue(rig.create())
            assertTrue(addSwap(controller, layer) >= 0)
            assertTrue(rig.setMode(RigController.MODE_POSE))
            val swap = rig.findBone("Swap." + controller.layerName(layer))
            rig.poseSelect(swap)
            for (f in 1..3) {
                controller.animation.setFrame(f)
                assertTrue(rig.setSwap(controller.layerName(layer), 4 - f)); assertTrue(rig.insertKey(f) > 0)
                rig.setInterpolation(swap, f, RigController.IPO_CONSTANT)
            }
            controller.history.markEdit()
            controller.animation.setFrame(2)
            assertTrue(controller.setOnionSkin(true, 1, 1, 0.8f))
            controller.render()
        }
        val bmp = shot("onion_swap")
        assertTrue("shown drawing 2", inkNear(bmp, 600f, 600f, 4))
        assertTrue("ghost before shows drawing 1", inkNear(bmp, 600f, 400f, 4, ::greenish))
        assertTrue("ghost after shows drawing 3", inkNear(bmp, 600f, 800f, 4, ::bluish))
        assertTrue("no ghost before at its own key", !inkNear(bmp, 600f, 800f, 4, ::greenish))
        assertTrue("no ghost after at its own key", !inkNear(bmp, 600f, 400f, 4, ::bluish))
    }
}
