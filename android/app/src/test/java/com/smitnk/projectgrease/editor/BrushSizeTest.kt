package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Test

/** Device report: the Size slider showed 8 while strokes drew far thicker. Size and the stroke
 *  thickness are one value, whichever control (Size, Thickness, a preset) changes it. */
class BrushSizeTest {
    @Test fun sizeIsTheStrokeThickness() {
        val materials = MaterialController()
        val brushes = BrushController(materials)
        brushes.setSize(30f)
        assertEquals(30f, materials.thickness, 0f)
        materials.setThickness(64f)              // Properties / Materials "Thickness" slider
        assertEquals(64f, brushes.size, 0f)
        brushes.select(BrushPreset.MARKER)
        assertEquals(14f, brushes.size, 0f)
        assertEquals(14f, materials.thickness, 0f)
    }
}
