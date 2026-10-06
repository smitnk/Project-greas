package com.smitnk.projectgrease.editor

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** Batch 21 Kotlin logic: draw-parameter layout, brush presets, curves, shape edit, modifier handles, MP4 maths. */
class Batch21Test {
    private fun repoFile(relative: String): File {
        System.getenv("PG_REPO_ROOT")?.let { root -> File(root, relative).takeIf { it.exists() }?.let { return it } }
        var dir: File? = File(System.getProperty("user.dir")).absoluteFile
        while (dir != null) {
            val candidate = File(dir, relative)
            if (candidate.exists()) return candidate
            dir = dir.parentFile
        }
        throw AssertionError("$relative not found")
    }

    @Test
    fun drawParametersMatchTheNativeLayout() {
        val header = repoFile("native/blender_gp/project_grease_tool_session.h").readText()
        assertTrue(header.contains("PG_DRAW_P_STRENGTH_CURVE_N = PG_DRAW_P_PRESSURE_CURVE_XY + 16"))
        assertTrue(header.contains("PG_DRAW_P_PX_PER_UNIT = PG_DRAW_P_STRENGTH_CURVE_XY + 16,"))
        assertTrue(Regex("PG_DRAW_P_PX_PER_UNIT = PG_DRAW_P_STRENGTH_CURVE_XY \\+ 16,\\s*(/\\*.*?\\*/\\s*)?PG_DRAW_P_SEED,\\s*PG_DRAW_P_COUNT\\b", RegexOption.DOT_MATCHES_ALL).containsMatchIn(header))
        assertEquals(ToolSession.DRAW_P_STRENGTH_CURVE_N + 17, ToolSession.DRAW_P_PX_PER_UNIT)
        assertEquals(ToolSession.DRAW_P_PX_PER_UNIT + 1, ToolSession.DRAW_P_SEED)
        assertEquals(ToolSession.DRAW_P_SEED + 1, ToolSession.DRAW_P_COUNT)
        val p = ToolSession.DrawSettings(material = 2, thickness = 9f, guideType = 3, guideX = 10f, guideY = 20f,
            guideAngle = 0.5f, guideSpacing = 40f, pressureCurvePoints = listOf(0f to 0f, 0.5f to 0.1f, 1f to 1f)).toParams()
        assertEquals(ToolSession.DRAW_P_COUNT, p.size)
        assertEquals(2f, p[0], 0f); assertEquals(9f, p[1], 0f)
        assertEquals(4f, p[19], 0f) // guide type + 1
        assertArrayEquals(floatArrayOf(10f, 20f, 0.5f, 40f), p.copyOfRange(20, 24), 0f)
        assertEquals(3f, p[ToolSession.DRAW_P_PRESSURE_CURVE_N], 0f)
        assertArrayEquals(floatArrayOf(0f, 0f, 0.5f, 0.1f, 1f, 1f), p.copyOfRange(25, 31), 0f)
        assertEquals(0f, p[ToolSession.DRAW_P_STRENGTH_CURVE_N], 0f) // no strength curve: power curve
        assertEquals(0f, ToolSession.DrawSettings(0, 1f).toParams()[19], 0f) // guide off
    }

    @Test
    fun brushPresetsCarryTheBrushCcValues() {
        val m = MaterialController()
        val b = BrushController(m)
        b.select(BrushPreset.AIRBRUSH)
        assertEquals(300f, b.size, 0f); assertEquals(0.4f, b.strength, 0f); assertTrue(b.useStrengthPressure)
        b.select(BrushPreset.MARKER_CHISEL)
        assertEquals(150f, b.size, 0f); assertEquals(0.5f, b.angleFactor, 0f); assertEquals(Math.toRadians(35.0).toFloat(), b.angle, 1e-6f)
        assertEquals(listOf(0f to 0f, 0.31f to 0.22f, 0.61f to 0.88f, 1f to 1f), b.strengthCurvePoints)
        b.select(BrushPreset.INK_PEN)
        assertEquals(60f, b.size, 0f); assertFalse(b.useStrengthPressure)
        assertEquals(listOf(0f to 0f, 0.63448f to 0.375f, 1f to 1f), b.pressureCurvePoints)
        b.select(BrushPreset.PEN)
        assertEquals(25f, b.size, 0f); assertFalse(b.usePressure)
        assertEquals(BrushPreset.Kind.ERASE, BrushPreset.ERASER_STROKE.kind)
        assertEquals(EraserMode.STROKE, BrushPreset.ERASER_STROKE.eraser)
    }

    @Test
    fun curvePointsStaySortedClampedAndAtLeastTwo() {
        assertEquals(listOf(0f to 0f, 0.5f to 1f, 1f to 1f), CurvePoints.clean(listOf(1f to 1f, 0.5f to 2f, -1f to 0f)))
        assertEquals(BrushPreset.LINEAR_CURVE, CurvePoints.clean(listOf(0.3f to 0.3f)))
        val two = listOf(0f to 0f, 1f to 1f)
        assertEquals(two, CurvePoints.remove(two, 0)) // two points stay
        val three = CurvePoints.insert(two, 0.4f, 0.2f)
        assertEquals(3, three.size); assertEquals(1, CurvePoints.hit(three, 0.41f, 0.21f, 0.05f))
    }

    @Test
    fun shapeEditKeepsHandlesUntilConfirmed() {
        val e = ShapeEditSession()
        assertFalse(e.start(ProjectGreasePrimitive.POLYLINE, 0f to 0f, 10f to 0f))
        assertTrue(e.start(ProjectGreasePrimitive.LINE, 0f to 0f, 100f to 0f))
        assertTrue(e.press(99f, 1f, 8f)) // end handle
        e.move(100f, 50f); e.release()
        assertEquals(listOf(0f to 0f, 100f to 50f), e.anchors())
        assertFalse(e.press(50f, 200f, 8f)) // away from the handles: confirm
        assertTrue(e.extrude())
        assertEquals(ProjectGreasePrimitive.POLYLINE, e.effectiveType())
        assertEquals(3, e.handles().size)
        e.reset(); assertFalse(e.isActive)
        assertTrue(e.start(ProjectGreasePrimitive.BOX, 0f to 0f, 10f to 10f))
        assertFalse(e.extrude()) // only lines extrude
    }

    @Test
    fun modifierCanvasHandlesMoveTheParameters() {
        val hook = FloatArray(ModifierType.MAX_PARAMS).also { it[0] = 100f; it[1] = 50f; it[2] = 10f; it[3] = 0f }
        assertEquals(listOf(100f to 50f, 110f to 50f), ModifierSpecs.canvasHandles(ModifierType.HOOK, hook))
        val moved = ModifierSpecs.moveHandle(ModifierType.HOOK, hook, 0, 80f, 50f) // centre moves, target stays
        assertEquals(listOf(80f to 50f, 110f to 50f), ModifierSpecs.canvasHandles(ModifierType.HOOK, moved))
        val lat = FloatArray(ModifierType.MAX_PARAMS).also { it[2] = 200f; it[3] = 100f; it[4] = 3f; it[5] = 2f }
        val nodes = ModifierSpecs.canvasHandles(ModifierType.LATTICE, lat)
        assertEquals(6, nodes.size); assertEquals(100f to 0f, nodes[1]); assertEquals(200f to 100f, nodes[5])
        val dragged = ModifierSpecs.moveHandle(ModifierType.LATTICE, lat, 4, 110f, 120f)
        assertEquals(10f, dragged[ModifierSpecs.latticeOffsetIndex(3, 1, 1)], 0f)
        assertEquals(20f, dragged[ModifierSpecs.latticeOffsetIndex(3, 1, 1) + 1], 0f)
        val curve = ModifierSpecs.withCurve(FloatArray(ModifierType.MAX_PARAMS), true, listOf(0f to 0.2f, 1f to 1f))
        assertEquals(1f, curve[ModifierType.CURVE_BASE], 0f)
        assertEquals(listOf(0f to 0.2f, 1f to 1f), ModifierSpecs.curvePoints(curve))
        assertEquals(8, ModifierSpecs.filterSpecs().size)
        assertTrue(ModifierSpecs.filterSpecs().all { it.index in ModifierType.FILTER_BASE until ModifierType.MAX_PARAMS })
    }

    @Test
    fun videoFramesMaths() {
        assertEquals(1280 to 720, VideoFrames.evenSize(1281, 721))
        assertEquals(500_000L, VideoFrames.presentationTimeUs(6, 12))
        // white and black in BT.601 limited range
        assertEquals(235, VideoFrames.y(255, 255, 255)); assertEquals(16, VideoFrames.y(0, 0, 0))
        assertEquals(128, VideoFrames.u(128, 128, 128)); assertEquals(128, VideoFrames.v(128, 128, 128))
        val argb = IntArray(4 * 2) { if (it % 4 < 2) 0xFFFF0000.toInt() else 0xFF0000FF.toInt() }
        val y = ByteArray(8); val u = ByteArray(2); val v = ByteArray(2)
        VideoFrames.argbToYuv420(argb, 4, 4, 2, y, 4, 1, u, v, 2, 1)
        assertEquals(VideoFrames.y(255, 0, 0), y[0].toInt() and 255)
        assertEquals(VideoFrames.v(255, 0, 0), v[0].toInt() and 255) // red block: high V
        assertEquals(VideoFrames.u(0, 0, 255), u[1].toInt() and 255) // blue block: high U
    }
}
