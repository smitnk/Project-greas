package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class BlenderPrimitiveRulesTest {

    @Test
    fun shapeToolsMapToNativePrimitiveIds() {
        assertEquals(ProjectGreasePrimitive.BOX, ProjectGreasePrimitive.forTool(GreaseTool.RECTANGLE))
        assertEquals(ProjectGreasePrimitive.LINE, ProjectGreasePrimitive.forTool(GreaseTool.LINE))
        assertEquals(ProjectGreasePrimitive.POLYLINE, ProjectGreasePrimitive.forTool(GreaseTool.POLYLINE))
        assertEquals(ProjectGreasePrimitive.CIRCLE, ProjectGreasePrimitive.forTool(GreaseTool.CIRCLE))
        assertEquals(ProjectGreasePrimitive.ARC, ProjectGreasePrimitive.forTool(GreaseTool.ARC))
        assertNull(ProjectGreasePrimitive.forTool(GreaseTool.DRAW))
    }

    @Test
    fun onlyBoxAndCircleAreCyclicLikeBlender() {
        assertTrue(ProjectGreasePrimitive.isCyclic(ProjectGreasePrimitive.BOX))
        assertTrue(ProjectGreasePrimitive.isCyclic(ProjectGreasePrimitive.CIRCLE))
        assertFalse(ProjectGreasePrimitive.isCyclic(ProjectGreasePrimitive.LINE))
        assertFalse(ProjectGreasePrimitive.isCyclic(ProjectGreasePrimitive.ARC))
        assertFalse(ProjectGreasePrimitive.isCyclic(ProjectGreasePrimitive.POLYLINE))
    }

    @Test
    fun firstDragCreatesTwoVertices() {
        val session = PolylineSession()
        session.press(0f, 0f)
        session.move(100f, 0f)
        assertEquals(PolylineSession.Release.CONTINUE, session.release(100f, 0f))
        assertEquals(listOf(0f to 0f, 100f to 0f), session.vertices)
    }

    @Test
    fun firstTapKeepsSingleStartVertex() {
        val session = PolylineSession()
        session.press(0f, 0f)
        assertEquals(PolylineSession.Release.CONTINUE, session.release(2f, 1f))
        assertEquals(listOf(0f to 0f), session.vertices)
    }

    @Test
    fun tapAwayFromLastVertexAddsVertex() {
        val session = twoVertexSession()
        session.press(200f, 50f)
        assertEquals(PolylineSession.Release.CONTINUE, session.release(200f, 50f))
        assertEquals(3, session.vertices.size)
        assertEquals(200f to 50f, session.vertices.last())
    }

    @Test
    fun tapOnLastVertexFinishes() {
        val session = twoVertexSession()
        session.press(101f, 2f)
        assertEquals(PolylineSession.Release.FINISH, session.release(101f, 2f))
        assertEquals(2, session.vertices.size)
    }

    @Test
    fun tapOnOnlyVertexDoesNotFinish() {
        val session = PolylineSession()
        session.press(0f, 0f)
        session.release(0f, 0f)
        session.press(1f, 1f)
        assertEquals(PolylineSession.Release.CONTINUE, session.release(1f, 1f))
        assertEquals(1, session.vertices.size)
    }

    @Test
    fun previewIncludesDraggedEnd() {
        val session = twoVertexSession()
        session.press(100f, 0f)
        session.move(150f, 80f)
        assertEquals(listOf(0f to 0f, 100f to 0f, 150f to 80f), session.previewVertices())
    }

    @Test
    fun cancelledFirstGestureLeavesNothing() {
        val session = PolylineSession()
        session.press(5f, 5f)
        session.cancelGesture()
        assertFalse(session.isActive)
    }

    private fun twoVertexSession(): PolylineSession {
        val session = PolylineSession()
        session.press(0f, 0f)
        session.move(100f, 0f)
        session.release(100f, 0f)
        return session
    }
}
