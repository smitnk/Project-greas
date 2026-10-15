package com.smitnk.projectgrease

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class CrashLogBugTest {
    @Test fun catchesKnownSymptoms() {
        assertNotNull(CrashLog.detectBug("PASTE frame 3 -> false"))
        assertNotNull(CrashLog.detectBug("SLOW fill ok=true 1204 ms (setup 0 ms)"))
        assertNull(CrashLog.detectBug("SLOW fill ok=true 400 ms (setup 0 ms)"))
        assertNull(CrashLog.detectBug("LASSO 463 noose points, op 3, select mode 1 -> ok=false, selected 8 of 8 points"))
        assertNotNull(CrashLog.detectBug("LASSO 1 noose points, op 3"))
        assertNotNull(CrashLog.detectBug("TOUCH ACTION_DOWN x1 tool CIRCLE canvas 535,-40 late 1 ms handled 1 ms off-canvas"))
        assertNull(CrashLog.detectBug("BUG FAILED: x -> false"))
    }

    @Test fun glErrorsOnlyWhenTheCountGrows() {
        val line = "SLOW render [native frame 30 ms: GL errors: 0 frames / 2 in engine, last 0x501, swap 1 ms] 30 ms"
        assertEquals(true, CrashLog.detectBug(line)?.startsWith("GL"))
        assertNull(CrashLog.detectBug(line))
    }
}
