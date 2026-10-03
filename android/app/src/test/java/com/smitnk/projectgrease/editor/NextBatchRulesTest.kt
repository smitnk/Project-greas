package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** Kotlin-side rules of the batch: segment select, material removal, onion commands, fit canvas, Save as naming. */
class NextBatchRulesTest {
    @Test
    fun segmentModeIsClickOnlyAndAreaSelectUsesPoint() {
        assertTrue(ProjectGreaseSelect.isValidSelectMode(ProjectGreaseSelect.MODE_SEGMENT))
        assertEquals(ProjectGreaseSelect.MODE_POINT, ProjectGreaseSelect.areaMode(ProjectGreaseSelect.MODE_SEGMENT))
        assertEquals(ProjectGreaseSelect.MODE_STROKE, ProjectGreaseSelect.areaMode(ProjectGreaseSelect.MODE_STROKE))
        // The native box / lasso / circle operators only take point or stroke.
        assertNull(ProjectGreaseSelect.box(ProjectGreaseSelect.OP_SET, ProjectGreaseSelect.MODE_SEGMENT, 0f, 0f, 1f, 1f))
        val c = ProjectGreaseSelect.segmentPick(10f, 20f, 64, ProjectGreaseSelect.PICK_DESELECT_ALL)!!
        assertEquals(86, c.id)
        assertArrayEquals(floatArrayOf(10f, 20f, 64f, 16f), c.args, 0f)
        assertNull(ProjectGreaseSelect.segmentPick(Float.NaN, 0f, 64, 0))
    }

    @Test
    fun materialAndOnionCommandsMatchEdit5() {
        assertEquals(87, ProjectGreaseSelect.materialRemove(2)!!.id)
        assertArrayEquals(floatArrayOf(2f), ProjectGreaseSelect.materialRemove(2)!!.args, 0f)
        assertNull(ProjectGreaseSelect.materialRemove(-1))
        assertArrayEquals(floatArrayOf(3f, 0f), ProjectGreaseSelect.onionLayer(3, false)!!.args, 0f)
        assertEquals(88, ProjectGreaseSelect.onionLayer(0, true)!!.id)
        assertEquals(89, ProjectGreaseSelect.onionFade(true).id)
        assertArrayEquals(floatArrayOf(1f), ProjectGreaseSelect.onionFade(true).args, 0f)
    }

    @Test
    fun fitCanvasShowsTheWholeCanvasWithAMargin() {
        val v = ViewController()
        v.setZoom(3f); v.panBy(120f, -40f)
        v.fitCanvas()
        assertEquals(1f, v.zoom, 0f); assertEquals(0f, v.panX, 0f); assertEquals(0f, v.panY, 0f)
        // 1920x1080 canvas in a 1000x800 view: width-limited, 4% margin left and right, centered.
        val r = ViewController.canvasRect(1000f, 800f, 1920f, 1080f, v.zoom, v.panX, v.panY)
        assertEquals(40f, r[0], 1e-3f)
        assertEquals(920f, r[2], 1e-3f)
        assertEquals((800f - r[3]) / 2f, r[1], 1e-3f)
        assertTrue(r[1] >= 0f && r[1] + r[3] <= 800f)
        // Tall canvas: height-limited.
        val t = ViewController.canvasRect(1000f, 800f, 500f, 2000f, 1f, 0f, 0f)
        assertEquals(32f, t[1], 1e-3f)
        assertEquals(736f, t[3], 1e-3f)
    }

    @Test
    fun saveAsContinuesUnderTheFileName() {
        assertEquals("Sketch v2", SaveAsNaming.projectName("Sketch v2.gpjson"))
        assertEquals("Walk", SaveAsNaming.projectName("Walk.json"))
        assertEquals("Walk.cycle", SaveAsNaming.projectName("Walk.cycle"))
        assertEquals("Shot", SaveAsNaming.projectName("folder/Shot.GPJSON"))
        assertNull(SaveAsNaming.projectName(null))
        assertNull(SaveAsNaming.projectName("  "))
        assertNull(SaveAsNaming.projectName(".gpjson"))
    }
}
