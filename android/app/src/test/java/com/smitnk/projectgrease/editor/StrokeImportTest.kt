package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class StrokeImportTest {
    @Test
    fun viewBoxAndSizeFallback() {
        assertArrayEquals(floatArrayOf(0f, 0f, 200f, 100f),
            StrokeImport.svgViewBox("""<svg xmlns="x" viewBox="0 0 200 100" width="10"><path d="M0 0"/></svg>"""), 0f)
        assertArrayEquals(floatArrayOf(0f, 0f, 30f, 40f), StrokeImport.svgViewBox("""<svg width="30" height="40px">"""), 0f)
        assertNull(StrokeImport.svgViewBox("""<svg><rect/></svg>"""))
    }

    @Test
    fun fitKeepsAspectAndCenters() {
        val f = StrokeImport.fit(0f, 0f, 200f, 100f, 1000, 1000)
        assertEquals(5f, f.scale, 1e-4f)
        assertEquals(0f, f.x(0f), 1e-3f); assertEquals(250f, f.y(0f), 1e-3f); assertEquals(750f, f.y(100f), 1e-3f)
    }

    @Test
    fun svgStrokesMapToCanvasAndMaterials() {
        val svg = """<svg viewBox="0 0 100 100"><rect x="0" y="0" width="10" height="10" stroke="#ff0000" fill="#00ff00" stroke-width="3"/>
            <line x1="0" y1="0" x2="100" y2="100" stroke="#ff0000"/><line x1="0" y1="0" x2="5" y2="5" stroke="#000000"/></svg>"""
        val sources = StrokeImport.fromSvg(SvgImport.parse(svg))
        val box = StrokeImport.svgViewBox(svg)!!
        val fit = StrokeImport.fit(box[0], box[1], box[2], box[3], 1000, 500)
        // existing slot 0 is opaque black, no fill: the black line reuses it
        val black = MaterialRecord(floatArrayOf(0f, 0f, 0f, 1f), floatArrayOf(0f, 0f, 0f, 1f), true, false)
        val plan = StrokeImport.plan(sources, fit, listOf(black))
        assertEquals(3, plan.strokes.size)
        assertEquals(2, plan.newMaterials.size) // red+green fill, red without fill
        assertEquals(1, plan.strokes[0].material); assertEquals(2, plan.strokes[1].material); assertEquals(0, plan.strokes[2].material)
        assertTrue(plan.newMaterials[0].fillEnabled); assertFalse(plan.newMaterials[1].fillEnabled)
        assertTrue(plan.strokes[0].cyclic); assertEquals(3f, plan.strokes[0].thickness, 0f)
        // scale 5, x offset (1000 - 500) / 2 = 250
        assertEquals(250f, plan.strokes[1].xy[0], 1e-3f); assertEquals(750f, plan.strokes[1].xy[2], 1e-3f)
        assertEquals(500f, plan.strokes[1].xy[3], 1e-3f)
    }
}
