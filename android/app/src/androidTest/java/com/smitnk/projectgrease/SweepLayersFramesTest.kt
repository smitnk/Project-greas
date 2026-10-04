package com.smitnk.projectgrease

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.ProjectGreaseSelect as S
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/** Layers (blend/tint/masks/merge/isolate/lock/order/...), frames, timeline, keyframe types, interpolation, onion skin. */
@RunWith(AndroidJUnit4::class)
class SweepLayersFramesTest : SweepBase() {

    private fun layers() = document().getJSONArray("layers")
    private fun names() = onUi { (0 until controller.layerCount()).map { controller.layerName(it) } }

    // ---- layers ----
    @Test fun layerCreateRenameDelete() {
        val n = onUi { controller.layerCount() }
        undoable("create layer") { controller.createLayer("Extra") }
        assertEquals(n + 1, onUi { controller.layerCount() })
        undoable("rename layer") { controller.renameLayer(controller.selectedLayer, "Renamed") }
        assertTrue("Renamed" in names())
        undoable("delete layer") { controller.deleteLayer() }
        assertEquals(n, onUi { controller.layerCount() })
        shot("layer_create_rename_delete")
    }

    @Test fun layerDuplicate() {
        line(300f)
        undoable("duplicate layer") { controller.duplicateLayer() }
        assertEquals(2, strokes().size)
        shot("layer_duplicate")
    }

    @Test fun layerOrdering() {
        val before = names()
        undoable("move layer") { controller.moveLayer(0, 1) }
        assertEquals(before[0], names()[1])
        shot("layer_order")
    }

    @Test fun layerVisibility() {
        line(300f)
        val l = onUi { controller.selectedLayer }
        undoable("hide layer") { controller.setLayerVisibility(l, false) }
        assertNoInk(shot("layer_hidden"), 600f, 300f, "hidden layer")
        onUi { controller.setLayerVisibility(l, true) }
        assertInk(shot("layer_shown"), 600f, 300f, "shown layer")
    }

    @Test fun layerLocking() {
        val l = onUi { controller.selectedLayer }
        undoable("lock layer") { controller.setLayerLocked(l, true) }
        drag(200f to 300f, 1000f to 300f)
        assertEquals("locked layer takes no strokes", 0, strokes().size)
        shot("layer_locked")
    }

    @Test fun layerLockAllUnlockAll() {
        undoable("lock all") { controller.lockAllLayers() }
        assertTrue((0 until layers().length()).all { layers().getJSONObject(it).getBoolean("locked") })
        undoable("unlock all") { controller.unlockAllLayers() }
        shot("layer_lock_all")
    }

    @Test fun layerBlendModes() {
        onUi { controller.selectLayer(0); controller.selectMaterial(0); controller.setMaterialColor(0xFFFFFF00.toInt()); controller.brushes.setSize(40f) }
        line(300f, 200f, 800f)
        onUi { controller.selectLayer(1); controller.selectMaterial(1); controller.setMaterialColor(0xFF00FFFF.toInt()) }
        line(300f, 500f, 1100f)
        for (mode in 1..5) {
            undoable("blend $mode") { controller.setLayerBlend(mode, 1) }
            shot("layer_blend_$mode")
            onUi { controller.setLayerBlend(0, 1) }
        }
        onUi { controller.setLayerBlend(4, 1) }
        val c = pixel(shot("layer_blend_multiply"), 650f, 300f)
        assertTrue("multiply yellow*cyan = green ${Integer.toHexString(c)}", Color.green(c) > 150 && Color.red(c) < 70 && Color.blue(c) < 70)
        assertEquals(4, layers().getJSONObject(1).optInt("blend"))
    }

    @Test fun layerTint() {
        onUi { controller.brushes.setSize(40f) }; line(300f)
        undoable("tint") { controller.setLayerTint(0xFFFF0000.toInt(), 1f) }
        val c = pixel(shot("layer_tint"), 600f, 300f)
        assertTrue("tinted red ${Integer.toHexString(c)}", Color.red(c) > 180 && Color.green(c) < 70)
    }

    @Test fun layerMasks() {
        onUi { controller.selectLayer(0); controller.brushes.setSize(80f) }
        line(300f, 500f, 700f) // mask shape on layer 0
        onUi { controller.selectLayer(1); controller.selectMaterial(2); controller.setMaterialColor(0xFFFF0000.toInt()); controller.brushes.setSize(20f) }
        line(300f) // full line on layer 1
        undoable("add mask") { controller.addLayerMask(0, 1) }
        assertTrue("adding a mask turns Use Mask on (gpencil_layer_mask_add)", onUi { controller.layerUsesMask(1) })
        val bmp = shot("layer_mask")
        assertTrue("inside the mask red", inkNear(bmp, 600f, 300f, 3) { Color.red(it) > 150 && Color.green(it) < 90 })
        assertTrue("outside the mask hidden", !inkNear(bmp, 300f, 300f, 2) { Color.red(it) > 150 && Color.green(it) < 90 })
        undoable("invert mask") { controller.setLayerMaskFlags(0, false, true, 1) }
        shot("layer_mask_inverted")
        assertEquals(1, onUi { controller.layerMasks(1).size })
        undoable("use mask off") { controller.setLayerUsesMask(false, 1) }
        assertTrue("unmasked red line visible", inkNear(shot("layer_mask_off"), 300f, 300f, 3) { Color.red(it) > 150 && Color.green(it) < 90 })
    }

    @Test fun layerMergeDown() {
        onUi { controller.selectLayer(0) }; line(250f)
        onUi { controller.selectLayer(1) }; line(450f)
        val n = onUi { controller.layerCount() }
        undoable("merge down") { controller.mergeLayerDown() }
        assertEquals(n - 1, onUi { controller.layerCount() })
        val bmp = shot("layer_merge_down")
        assertInk(bmp, 600f, 250f, "lower"); assertInk(bmp, 600f, 450f, "merged")
    }

    @Test fun layerIsolate() {
        onUi { controller.selectLayer(0) }; line(250f)
        onUi { controller.selectLayer(1) }; line(450f)
        undoable("isolate") { controller.isolateLayer() }
        val bmp = shot("layer_isolate")
        assertNoInk(bmp, 600f, 250f, "other layer"); assertInk(bmp, 600f, 450f, "active layer")
    }

    // ---- frames / timeline ----
    @Test fun frameAdd() {
        line(300f)
        undoable("add frame") { controller.createFrame(5) }
        onUi { controller.selectFrame(5); controller.render() }
        assertTrue(5 in onUi { controller.frameNumbers() }.toList())
        assertNoInk(shot("frame_add_5"), 600f, 300f, "empty frame 5")
    }

    @Test fun frameInsertBlank() {
        line(300f)
        undoable("insert blank") { controller.insertBlankFrame() }
        shot("frame_insert_blank")
    }

    @Test fun frameDuplicate() {
        line(300f)
        undoable("duplicate frame") { controller.duplicateFrame(1, 3) }
        onUi { controller.animation.setFrame(3); controller.render() }
        assertInk(shot("frame_duplicate_3"), 600f, 300f, "duplicated drawing")
    }

    @Test fun frameDelete() {
        line(300f) // frame 1 (the last keyframe of a layer is kept, as the Delete button does)
        onUi { controller.createFrame(4) }
        assertEquals("keys before delete", listOf(1, 4), onUi { controller.frameNumbers() }.sorted())
        undoable("delete frame") { controller.deleteFrame(4) }
        assertTrue(4 !in onUi { controller.frameNumbers() }.toList())
        shot("frame_delete")
    }

    @Test fun frameHolds() {
        line(300f)
        onUi { controller.createFrame(10); controller.animation.setFrame(6); controller.render() }
        assertInk(shot("frame_hold_6"), 600f, 300f, "frame 1 held at 6")
    }

    @Test fun frameNavigationPlaybackLoopFps() {
        onUi { controller.createFrame(3); controller.animation.setFps(12) }
        assertEquals(12, onUi { controller.animation.fps })
        onUi { controller.animation.setFrame(3) }
        assertEquals(3, onUi { controller.animation.currentFrame })
        onUi { controller.animation.toggleLoop() }
        val loop = onUi { controller.animation.loop }
        onUi { controller.animation.togglePlayback() }
        assertTrue(onUi { controller.animation.playing })
        Thread.sleep(500)
        onUi { controller.animation.stop() }
        assertTrue(!onUi { controller.animation.playing })
        onUi { controller.animation.toggleLoop() }
        assertEquals(!loop, onUi { controller.animation.loop })
        shot("frame_playback")
    }

    @Test fun keyframeTypes() {
        onUi { controller.createFrame(5) }
        for (t in listOf(S.KEY_EXTREME, S.KEY_BREAKDOWN, S.KEY_JITTER, S.KEY_MOVEHOLD)) {
            undoable("keytype $t") { controller.setFrameKeyType(5, t) }
            assertEquals(t, onUi { controller.frameKeyType(5) })
        }
        shot("keyframe_types")
    }

    @Test fun timelineFrameSelection() {
        onUi { controller.createFrame(3); controller.createFrame(5) }
        assertTrue(onUi { controller.selectTimelineFrame(3, S.FRAME_SELECT_SET) })
        assertTrue(onUi { controller.selectTimelineFrame(5, S.FRAME_SELECT_ADD) })
        assertEquals(setOf(3, 5), onUi { controller.animation.selectedFrames.toSet() })
        assertTrue(onUi { controller.deselectTimelineFrames() })
        shot("timeline_select")
    }

    @Test fun interpolation() {
        drag(200f to 300f, 600f to 300f)
        onUi { controller.createFrame(9) }
        drag(200f to 600f, 600f to 600f)
        onUi { controller.animation.setFrame(5) }
        undoable("interpolate") { controller.interpolateFrameAt(5) }
        onUi { controller.animation.setFrame(5); controller.render() }
        assertInk(shot("interpolate_5"), 400f, 450f, "in-between at frame 5")
    }

    @Test fun interpolationSequenceEasing() {
        drag(200f to 300f, 600f to 300f)
        onUi { controller.createFrame(9); controller.selectFrame(9) }
        drag(200f to 600f, 600f to 600f)
        onUi { controller.animation.setEasing(3, 0); controller.animation.setElastic(0.5f, 0.3f) }
        val made = onUi { controller.interpolateSequence(1) }
        assertEquals(7, made)
        assertTrue(onUi { controller.frameNumbers() }.toList().containsAll((2..8).toList()))
        shot("interpolate_sequence")
    }

    @Test fun multiframeEditing() {
        line(300f)
        onUi { controller.createFrame(3) }
        line(500f)
        assertTrue(onUi { controller.setMultiframeEditing(true) })
        // Multiframe edits the keyframes selected in the timeline (GP_FRAME_SELECT), as in Blender.
        assertTrue(onUi { controller.selectTimelineFrame(1, S.FRAME_SELECT_SET) && controller.selectTimelineFrame(3, S.FRAME_SELECT_ADD) })
        onUi { controller.selectAll() }
        // An edit in multiframe mode reaches every selected keyframe: both strokes move.
        val ys = { strokes().map { s -> points(s).map { it[1] }.average() } }
        val before = ys()
        assertTrue(onUi { controller.translateSelectedStroke(0f, 50f) })
        val after = ys()
        assertEquals("frame 1 moved", before[0] + 50.0, after[0], 1.0)
        assertEquals("frame 3 moved", before[1] + 50.0, after[1], 1.0)
        shot("multiframe")
        onUi { controller.setMultiframeEditing(false) }
    }

    // ---- onion skin ----
    @Test fun onionSkin() {
        onUi { controller.selectMaterial(0); controller.brushes.setSize(30f) }
        line(300f)
        onUi { controller.createFrame(2); controller.selectFrame(2) }
        assertTrue(onUi { controller.setOnionSkin(true, 1, 1, 0.8f) })
        onUi { controller.render() }
        val bmp = shot("onion_on")
        assertTrue("ghost of frame 1 visible", inkNear(bmp, 600f, 300f, 4) { it != Color.WHITE && Color.red(it) < 250 })
        onUi { controller.setOnionSkin(false); controller.render() }
        assertNoInk(shot("onion_off"), 600f, 300f, "ghost")
    }

    @Test fun onionRangeOpacityFade() {
        onUi { controller.onion.setBefore(3); controller.onion.setAfter(2); controller.onion.setOpacity(0.5f) }
        assertEquals(3, onUi { controller.onion.beforeFrames }); assertEquals(2, onUi { controller.onion.afterFrames })
        assertTrue(onUi { controller.setOnionFade(true) })
        assertTrue(onUi { controller.onion.fade })
        shot("onion_range")
    }

    @Test fun onionLayerFilterAndKeytype() {
        val l = onUi { controller.selectedLayer }
        assertTrue(onUi { controller.setLayerOnion(l, false) }); assertTrue(!onUi { controller.layerOnion(l) })
        assertTrue(onUi { controller.setOnionFilter(S.KEY_BREAKDOWN, true) })
        assertEquals(S.KEY_BREAKDOWN, onUi { controller.onion.keyTypeFilter })
        assertTrue(onUi { controller.setOnionStyle(S.ONION_MODE_RELATIVE) })
        shot("onion_filter")
    }
}
