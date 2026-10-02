package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Guards against false-completeness and silently gated tools.
 *
 * The UI disables (and ToolController refuses) any tool whose feature is NOT_IMPLEMENTED. A
 * missing registry entry used to default to NOT_IMPLEMENTED, which is how the shape and fill
 * tools became unreachable although their native code was finished.
 */
class FeatureRegistryTest {

    @Test
    fun everyFeatureHasAnExplicitAuditEntry() {
        assertEquals(FeatureId.entries.toSet(), FeatureRegistry.explicitIds())
    }

    @Test
    fun everyToolCanBeSelected() {
        GreaseTool.entries.forEach { tool ->
            assertTrue("$tool is gated off by the registry", ToolController().select(tool))
        }
    }

    @Test
    fun shapeAndFillToolsAreReachable() {
        listOf(
            FeatureId.LINE, FeatureId.RECTANGLE, FeatureId.CIRCLE, FeatureId.ARC,
            FeatureId.POLYLINE, FeatureId.FILL
        ).forEach {
            assertNotEquals("$it", FeatureState.NOT_IMPLEMENTED, FeatureRegistry.capability(it).state)
        }
    }

    @Test
    fun nothingIsAvailableOrCompleteWithoutDeviceEvidence() {
        FeatureRegistry.all().forEach {
            if (it.state == FeatureState.AVAILABLE) assertTrue("${it.id} lacks device evidence", it.deviceVerified)
            if (it.audit == AuditStatus.COMPLETE) assertTrue("${it.id} lacks device evidence", it.deviceVerified)
        }
    }

    @Test
    fun auditStatusAgreesWithTheGateState() {
        FeatureRegistry.all().forEach {
            when (it.audit) {
                AuditStatus.COMPLETE -> assertEquals(FeatureState.AVAILABLE, it.state)
                AuditStatus.IN_PROGRESS -> assertEquals("${it.id}", FeatureState.IN_PROGRESS, it.state)
                AuditStatus.NOT_IMPLEMENTED, AuditStatus.BLOCKED ->
                    assertEquals("${it.id}", FeatureState.NOT_IMPLEMENTED, it.state)
            }
        }
    }

    @Test
    fun everyUnfinishedFeatureStatesItsLimitation() {
        FeatureRegistry.all().filter { it.state != FeatureState.AVAILABLE }.forEach {
            assertFalse("${it.id} has no limitation text", it.limitation.isBlank())
        }
    }

    @Test
    fun knownGapsAreNotHiddenBehindAnInProgressLabel() {
        // Save/load now round-trips material, thickness, cyclic flag, fill, layer state and palette
        // (ProjectDocumentRoundTripTest); the registry must not still call it lossy.
        assertFalse(FeatureRegistry.capability(FeatureId.SAVE).limitation.contains("LOSSY"))
        assertFalse(FeatureRegistry.capability(FeatureId.OPEN_PROJECT).limitation.contains("LOSSY"))
        // Vector export (SVG/PDF) and PNG export exist but are unverified on a device; GIF is not implemented.
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.EXPORT).state)
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.EXPORT_PNG).state)
        assertFalse(FeatureRegistry.capability(FeatureId.EXPORT_PNG).deviceVerified)
        assertEquals(FeatureState.NOT_IMPLEMENTED, FeatureRegistry.capability(FeatureId.EXPORT_GIF).state)
        // The live modifier stack exists but has no device evidence; Dash is baked only and Outline is not ported.
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.MODIFIER_ORDERING).state)
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.DASH).state)
        assertTrue(FeatureRegistry.capability(FeatureId.DASH).limitation.contains("baked"))
        assertTrue(FeatureRegistry.capability(FeatureId.GENERATE).limitation.contains("Build is still not ported"))
        assertTrue(FeatureRegistry.capability(FeatureId.COPY_PASTE).limitation.contains("not saved"))
        assertEquals(FeatureState.NOT_IMPLEMENTED, FeatureRegistry.capability(FeatureId.OUTLINE).state)
        // Line Art: all three batches are in (strokes are generated) but nothing is device-verified and
        // shadows / material settings / collections are missing, so it stays IN_PROGRESS and says so.
        val lineArt = FeatureRegistry.capability(FeatureId.LINE_ART)
        assertEquals(AuditStatus.IN_PROGRESS, lineArt.audit)
        assertFalse(lineArt.deviceVerified)
        assertTrue(lineArt.limitation.contains("Not supported: shadows"))
    }

    @Test
    fun nextBatchFeaturesAreWiredButNotClaimedVerified() {
        listOf(
            FeatureId.SELECT_SEGMENT, FeatureId.FILL_GAP_TOLERANCE, FeatureId.FILL_EXPANSION, FeatureId.FILL_BOUNDARY,
            FeatureId.DELETE_MATERIAL, FeatureId.FIT_CANVAS, FeatureId.ONION_FADE, FeatureId.ONION_LAYER_FILTER,
            FeatureId.SAVE_AS, FeatureId.EXPORT_PNG
        ).forEach {
            val c = FeatureRegistry.capability(it)
            assertEquals("$it", FeatureState.IN_PROGRESS, c.state)
            assertFalse("$it", c.deviceVerified)
            assertTrue("$it", c.limitation.contains("Device validation NOT VERIFIED"))
        }
    }

    @Test
    fun vertexPaintModeChipIsEnabledButNotClaimedVerified() {
        // The mode chip and EditorController.setMode gate on "not NOT_IMPLEMENTED".
        val capability = FeatureRegistry.capability(FeatureId.VERTEX_PAINT)
        assertEquals(FeatureState.IN_PROGRESS, capability.state)
        assertFalse(capability.deviceVerified)
        // Weight Paint is enabled the same way, and is not claimed verified either.
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.WEIGHT_PAINT).state)
        assertFalse(FeatureRegistry.capability(FeatureId.WEIGHT_PAINT).deviceVerified)
    }

    @Test
    fun sculptModeStaysUsableWhileUnverified() {
        // The sculpt mode chip and controller gate on "not NOT_IMPLEMENTED" now that nothing is
        // AVAILABLE; this keeps that assumption visible.
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.SCULPT).state)
    }
}
