package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AnnotationDataTest {
    // frames 1 (two strokes) and 5 (one stroke), as pg_annot_dump() writes them
    private val dump = floatArrayOf(2f, 1f, 2f, 2f, 0f, 0f, 10f, 0f, 1f, 5f, 5f, 5f, 1f, 3f, 1f, 1f, 2f, 2f, 3f, 3f)

    @Test
    fun parsesAndRewritesTheNativeDump() {
        val frames = AnnotationData.frames(dump)!!
        assertEquals(listOf(1, 5), frames.map { it.frame })
        assertEquals(2, frames[0].strokes.size); assertEquals(3, frames[1].strokes[0].size / 2)
        assertArrayEquals(dump, AnnotationData.dump(frames), 0f)
        assertNull(AnnotationData.frames(dump.copyOf(dump.size - 1)))
    }

    @Test
    fun framesHold() {
        val frames = AnnotationData.frames(dump)!!
        assertEquals(1, AnnotationData.shownAt(frames, 4)!!.frame)
        assertEquals(5, AnnotationData.shownAt(frames, 9)!!.frame)
        assertNull(AnnotationData.shownAt(frames, 0))
    }

    @Test
    fun jsonRoundTrip() {
        val style = floatArrayOf(1f, 0f, 0f, 1f, 6f)
        val parsed = AnnotationData.fromJson(AnnotationData.toJson(dump, style))!!
        assertArrayEquals(dump, parsed.dump, 1e-6f)
        assertArrayEquals(style, parsed.style, 1e-6f)
    }

    @Test
    fun exportLayerOnlyWhereShown() {
        val style = floatArrayOf(1f, 0f, 0f, 1f, 6f)
        val layer = AnnotationData.exportLayer(dump, style, 3)!!
        assertEquals("Annotations", layer.name); assertEquals(2, layer.shapes.size)
        assertEquals(6f, layer.shapes[0].strokeWidth, 0f); assertNull(layer.shapes[0].fill)
        assertNull(AnnotationData.exportLayer(dump, style, 0))
    }
}
