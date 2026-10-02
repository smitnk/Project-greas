package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CurveSessionTest {
    private fun near(e: Float, a: Float) = assertEquals(e, a, 1e-4f)

    @Test
    fun dragThenHandlesAtBlenderDefaults() {
        val c = CurveSession()
        assertEquals(CurveSession.Press.LINE, c.press(0f, 0f, 10f))
        c.move(50f, 0f)
        assertEquals(listOf(0f to 0f, 50f to 0f), c.anchors())
        c.release(100f, 0f)
        assertEquals(CurveSession.Phase.EDIT, c.phase)
        val a = c.anchors()
        assertEquals(4, a.size)
        // midpoint 50; cp1 = mid + 0.33 (start - mid), cp2 = mid + 0.33 (end - mid)
        near(33.5f, a[2].first); near(66.5f, a[3].first)
    }

    @Test
    fun handleDragAndConfirm() {
        val c = CurveSession()
        c.press(0f, 0f, 10f); c.release(100f, 0f)
        assertEquals(CurveSession.Press.HANDLE, c.press(34f, 2f, 10f))
        c.move(30f, 40f); c.release(30f, 40f)
        assertEquals(30f to 40f, c.anchors()[2])
        assertEquals(CurveSession.Press.CONFIRM, c.press(50f, 80f, 10f))
        assertTrue(c.isActive)
    }

    @Test
    fun cancelRestoresHandleAndTapIsNoCurve() {
        val c = CurveSession()
        c.press(0f, 0f, 10f); c.release(100f, 0f)
        c.press(100f, 0f, 10f); c.move(120f, 30f); c.cancelGesture()
        assertEquals(100f to 0f, c.anchors()[1])
        val tap = CurveSession()
        tap.press(5f, 5f, 10f); tap.release(5f, 5f)
        assertFalse(tap.isActive)
        assertTrue(tap.anchors().isEmpty())
    }
}
