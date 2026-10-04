package com.smitnk.projectgrease.ui

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.IOException

class ProjectFilesTest {
    @get:Rule val tmp = TemporaryFolder()

    private fun doc(v: String) = JSONObject().put("v", v).toString()

    @Test fun interruptedSaveKeepsOldFileIntact() {
        val files = ProjectFiles(tmp.newFolder("projects"))
        files.write("Demo", doc("old"))
        val target = files.fileFor("Demo")
        try {
            ProjectFiles.writeAtomic(target) { out ->
                out.write("{\"v\":\"new-part".toByteArray())
                throw IOException("simulated crash mid-write")
            }
            fail("expected exception")
        } catch (e: IOException) { }
        assertEquals("old", JSONObject(files.read("Demo")!!).getString("v"))
        assertEquals(listOf(target.name), files.dir.list()!!.toList())
    }

    @Test fun twoProjectsWithSameNameBothSurvive() {
        val files = ProjectFiles(tmp.newFolder("projects"))
        val a = files.create("Name", doc("first"))
        val b = files.create("Name", doc("second"))
        assertEquals("Name", a); assertEquals("Name (2)", b)
        assertNotEquals(files.fileFor(a), files.fileFor(b))
        assertEquals("first", JSONObject(files.read(a)!!).getString("v"))
        assertEquals("second", JSONObject(files.read(b)!!).getString("v"))
    }

    @Test fun duplicatePicksNextFreeSuffix() {
        val files = ProjectFiles(tmp.newFolder("projects"))
        files.create("Name", doc("orig"))
        files.create("Name", doc("two"))
        val dup = files.duplicate("Name")
        assertEquals("Name (3)", dup)
        assertEquals("orig", JSONObject(files.read(dup!!)!!).getString("v"))
        assertEquals("two", JSONObject(files.read("Name (2)")!!).getString("v"))
        // a name taken only in the record list is also skipped
        assertEquals("Other (2)", files.uniqueName("Other") { it == "Other" })
        assertTrue(files.exists("Name (3)"))
    }

    @Test fun saveExistingOverwritesAtomically() {
        val files = ProjectFiles(tmp.newFolder("projects"))
        files.write("P", doc("a")); files.write("P", doc("b"))
        assertEquals("b", JSONObject(files.read("P")!!).getString("v"))
        assertEquals(1, files.dir.list()!!.size)
    }
}
