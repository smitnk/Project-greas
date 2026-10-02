package com.smitnk.projectgrease.editor

import org.junit.Assert.assertEquals
import org.junit.Test

class DocumentFrameEndTest {
    @Test
    fun frameEndIsOptional() {
        assertEquals(250, ProjectDocumentCodec.parse("""{"version":5,"frameEnd":250}""")!!.frameEnd)
        assertEquals(0, ProjectDocumentCodec.parse("""{"version":5}""")!!.frameEnd)
        assertEquals(0, ProjectDocumentCodec.parse("""{"version":5,"frameEnd":-3}""")!!.frameEnd)
    }
}
