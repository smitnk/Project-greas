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

    @Test fun marathiNamesGetDifferentFiles() {
        val files = ProjectFiles(tmp.newFolder("projects"))
        val a = "\u0906\u092E\u091A\u0940 \u0917\u094B\u0937\u094D\u091F" // आमची गोष्ट
        val b = "\u092E\u093E\u091D\u093E \u091A\u093F\u0924\u094D\u0930\u092A\u091F" // माझा चित्रपट
        assertNotEquals(files.fileFor(a), files.fileFor(b))
        files.write(a, doc("one")); files.write(b, doc("two"))
        assertEquals("one", JSONObject(files.read(a)!!).getString("v"))
        assertEquals("two", JSONObject(files.read(b)!!).getString("v"))
        assertEquals(files.fileFor(a), files.fileFor(a)) // stable
    }

    @Test fun oldFileNameIsMigratedOnRead() {
        val files = ProjectFiles(tmp.newFolder("projects"))
        val name = "My Project"
        val old = java.io.File(files.dir, ProjectFiles.legacySafeName(name) + ProjectFiles.EXT)
        old.writeText(doc("legacy").toString())
        assertTrue(files.exists(name))
        assertEquals("legacy", JSONObject(files.read(name)!!).getString("v"))
        assertTrue(files.fileFor(name).isFile)
        assertTrue(!old.exists())
    }

    @Test fun deleteRemovesTheDocument() {
        val files = ProjectFiles(tmp.newFolder("projects"))
        files.write("Gone", doc("x"))
        assertTrue(files.delete("Gone"))
        assertTrue(!files.exists("Gone"))
        assertTrue(!files.delete("Gone"))
    }
}
