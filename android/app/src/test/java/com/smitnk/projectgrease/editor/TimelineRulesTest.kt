package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TimelineRulesTest {
    @Test fun markerNameMatchesEdMarkerAdd() {
        assertEquals("F_01", TimelineRules.defaultMarkerName(1))
        assertEquals("F_12", TimelineRules.defaultMarkerName(12))
        assertEquals("F_250", TimelineRules.defaultMarkerName(250))
    }

    @Test fun addSelectsOnlyTheNewMarkerAndRefusesDuplicates() {
        val one = TimelineRules.addMarker(emptyList(), 10)!!
        val two = TimelineRules.addMarker(one, 5)!!
        assertEquals(listOf(5, 10), two.map { it.frame })
        assertEquals(listOf(true, false), two.map { it.selected })
        assertNull(TimelineRules.addMarker(two, 10))
    }

    @Test fun renameMoveDeleteActOnSelection() {
        var m = TimelineRules.addMarker(TimelineRules.addMarker(emptyList(), 3)!!, 8)!!
        m = TimelineRules.renameMarker(m, "Hit")!!
        assertEquals("Hit", m.first { it.frame == 8 }.name)
        assertEquals("F_03", m.first { it.frame == 3 }.name)
        assertEquals(63, TimelineRules.renameMarker(m, "x".repeat(100))!!.first { it.selected }.name.length)
        m = TimelineRules.moveMarkers(m, -6)!!
        assertEquals(listOf(2, 3), m.map { it.frame })
        assertNull(TimelineRules.moveMarkers(m, 0))
        m = TimelineRules.selectMarker(m, 3, extend = true)
        assertEquals(2, m.count { it.selected })
        m = TimelineRules.selectMarker(m, 3)
        assertEquals(listOf(false, true), m.map { it.selected })
        m = TimelineRules.deleteMarkers(m)!!
        assertEquals(listOf(2), m.map { it.frame })
        assertNull(TimelineRules.deleteMarkers(TimelineRules.selectMarker(m, 99)))
        assertNull(TimelineRules.renameMarker(TimelineRules.selectMarker(m, 99), "a"))
    }

    @Test fun scrubRoundsToNearestFrameAndClamps() {
        assertEquals(4, TimelineRules.scrubFrame(3.5f, 1, 10))
        assertEquals(3, TimelineRules.scrubFrame(3.49f, 1, 10))
        assertEquals(1, TimelineRules.scrubFrame(-7f, 1, 10))
        assertEquals(10, TimelineRules.scrubFrame(40f, 1, 10))
        assertEquals(1, TimelineRules.scrubFrame(Float.NaN, 1, 10))
    }

    @Test fun scrubSnapsToNearestKeyWhenOn() {
        val keys = intArrayOf(1, 6, 12)
        assertEquals(6, TimelineRules.scrubFrame(8.4f, 1, 20, keys, snapToKeys = true))
        assertEquals(12, TimelineRules.scrubFrame(9.6f, 1, 20, keys, snapToKeys = true))
        assertEquals(8, TimelineRules.scrubFrame(8.4f, 1, 20, keys, snapToKeys = false))
        assertEquals(8, TimelineRules.scrubFrame(8.4f, 1, 20, IntArray(0), snapToKeys = true))
    }

    @Test fun framePositionMapsCellsToFrames() {
        // cell 0 = frame 1, 50 px cells; the last case is scrolled by 2.5 cells
        assertEquals(1, TimelineRules.scrubFrame(TimelineRules.framePosition(10f, 50f), 1, 100))
        assertEquals(2, TimelineRules.scrubFrame(TimelineRules.framePosition(60f, 50f), 1, 100))
        assertEquals(4, TimelineRules.scrubFrame(TimelineRules.framePosition(30f, 50f, 2.5f), 1, 100))
    }

    @Test fun previewRangeClampsAndLoops() {
        val p = TimelineRules.setPreviewRange(9, 4)
        assertTrue(p.enabled)
        assertEquals(9, p.start); assertEquals(9, p.end)
        assertEquals(1, TimelineRules.setPreviewRange(-3, 0).start)
        val range = TimelineRules.setPreviewRange(5, 8)
        assertEquals(6, TimelineRules.nextPlaybackFrame(5, range, 100, true))
        assertEquals(5, TimelineRules.nextPlaybackFrame(8, range, 100, true))
        assertNull(TimelineRules.nextPlaybackFrame(8, range, 100, false))
        assertEquals(5, TimelineRules.nextPlaybackFrame(1, range, 100, true))
        assertEquals(1, TimelineRules.nextPlaybackFrame(100, PreviewRange(), 100, true))
        assertEquals(51, TimelineRules.nextPlaybackFrame(50, PreviewRange(), 100, true))
        assertNull(TimelineRules.nextPlaybackFrame(100, PreviewRange(), 100, false))
    }

    @Test fun jsonRoundTripAndOldFiles() {
        val state = TimelineState(listOf(TimeMarker(2, "A", true), TimeMarker(7, "B")), TimelineRules.setPreviewRange(3, 9))
        val (m, p) = TimelineRules.toJson(state)
        assertEquals(state, TimelineRules.fromJson(m, p))
        val (none, noPreview) = TimelineRules.toJson(TimelineState())
        assertNull(none); assertNull(noPreview)
        assertEquals(TimelineState(), TimelineRules.fromJson(null, null))
        assertFalse(TimelineRules.fromJson(null, org.json.JSONObject().put("start", 3)).preview.enabled)
    }
}
