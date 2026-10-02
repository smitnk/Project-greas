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
        // Export is a toast, not a pipeline.
        assertEquals(FeatureState.NOT_IMPLEMENTED, FeatureRegistry.capability(FeatureId.EXPORT).state)
        // The live modifier stack exists but has no device evidence; Dash and Outline are still not ported.
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.MODIFIER_ORDERING).state)
        assertEquals(FeatureState.NOT_IMPLEMENTED, FeatureRegistry.capability(FeatureId.DASH).state)
        assertEquals(FeatureState.NOT_IMPLEMENTED, FeatureRegistry.capability(FeatureId.OUTLINE).state)
        assertEquals(AuditStatus.BLOCKED, FeatureRegistry.capability(FeatureId.LINE_ART).audit)
    }

    @Test
    fun sculptModeStaysUsableWhileUnverified() {
        // The sculpt mode chip and controller gate on "not NOT_IMPLEMENTED" now that nothing is
        // AVAILABLE; this keeps that assumption visible.
        assertEquals(FeatureState.IN_PROGRESS, FeatureRegistry.capability(FeatureId.SCULPT).state)
    }
}
