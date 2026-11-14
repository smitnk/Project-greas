package com.smitnk.projectgrease.ui

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.OutputStream

/**
 * Plain-JVM project document storage (no Android dependencies, unit-testable).
 * Writes are atomic: data goes to a temp file in the same directory, is flushed and
 * fsync'ed, then renamed over the target. A failure mid-write leaves the old file intact.
 */
class ProjectFiles(val dir: File) {
    init { dir.mkdirs() }

    fun fileFor(name: String): File = File(dir, safeName(name) + EXT)

    /** Where files written before names were made unique lived (every non-ASCII character became "_"). */
    private fun legacyFileFor(name: String): File = File(dir, legacySafeName(name) + EXT)

    /** True when a file exists for [name], under its current file name or (not yet migrated) its old one. */
    fun exists(name: String): Boolean = fileFor(name).isFile || (legacyFileFor(name) != fileFor(name) && legacyFileFor(name).isFile)

    /**
     * [name]'s document. A project saved under the old naming is found at its old file and moved to the new
     * name on first read (the first project asking claims it: the old names of two projects were the same file).
     */
    fun read(name: String): String? {
        val f = fileFor(name)
        if (!f.isFile) {
            val old = legacyFileFor(name)
            if (old == f || !old.isFile) return null
            val text = old.readText(Charsets.UTF_8)
            writeAtomic(f) { it.write(text.toByteArray(Charsets.UTF_8)) }
            old.delete()
            return text
        }
        return f.readText(Charsets.UTF_8)
    }

    /** Deletes [name]'s document (and its not yet migrated old file). True when something was removed. */
    fun delete(name: String): Boolean {
        val f = fileFor(name)
        val old = legacyFileFor(name)
        var removed = false
        if (f.isFile) removed = f.delete()
        else if (old != f && old.isFile) removed = old.delete()
        return removed
    }

    /** Overwrites [name]'s own file atomically (saving an existing project). */
    fun write(name: String, json: String) = writeAtomic(fileFor(name)) { it.write(json.toByteArray(Charsets.UTF_8)) }

    /** "Name", or "Name (2)", "Name (3)", ... — the first whose file does not exist yet. */
    fun uniqueName(name: String, taken: (String) -> Boolean = { false }): String {
        val base = name.ifBlank { DEFAULT }
        if (!exists(base) && !taken(base)) return base
        var n = 2
        while (true) {
            val candidate = "$base ($n)"
            if (!exists(candidate) && !taken(candidate)) return candidate
            n++
        }
    }

    /** Creates a new project under a unique name; never overwrites another project. Returns the name used. */
    fun create(name: String, json: String, taken: (String) -> Boolean = { false }): String {
        val unique = uniqueName(name, taken)
        write(unique, json)
        return unique
    }

    /** Copies [source]'s document to a new unique name derived from [newName] (default: source name). */
    fun duplicate(source: String, newName: String = source, taken: (String) -> Boolean = { false }): String? {
        val json = read(source) ?: return null
        return create(newName, json, taken)
    }

    companion object {
        const val EXT = ".gpjson"
        const val DEFAULT = "Project Grease"

        /** The pre-batch-2 file name: every character outside [A-Za-z0-9._-] became "_", so "आमची" and "माझा" collided. */
        fun legacySafeName(name: String): String =
            name.replace(Regex("[^A-Za-z0-9._-]"), "_").ifBlank { "Project_Grease" }

        /**
         * Unique and stable file name for a project name: a name made only of safe characters is itself; any other name
         * (Marathi, spaces, punctuation) keeps a readable ASCII stem and gets a short hash of the full name, so two
         * different names never share a file.
         */
        fun safeName(name: String): String {
            if (name.isNotEmpty() && name.all { it in 'A'..'Z' || it in 'a'..'z' || it in '0'..'9' || it == '.' || it == '_' || it == '-' }) return name
            val digest = java.security.MessageDigest.getInstance("SHA-256").digest(name.toByteArray(Charsets.UTF_8))
            val hash = digest.take(6).joinToString("") { "%02x".format(it) }
            return legacySafeName(name).take(40) + "~" + hash
        }

        /**
         * Writes [target] atomically via a sibling temp file + fsync + rename.
         * [writer] may throw; then the temp file is deleted and [target] is untouched.
         */
        fun writeAtomic(target: File, writer: (OutputStream) -> Unit) {
            val parent = target.absoluteFile.parentFile ?: throw IOException("No parent for $target")
            parent.mkdirs()
            val tmp = File.createTempFile(target.name + ".", ".tmp", parent)
            try {
                FileOutputStream(tmp).use { out ->
                    writer(out)
                    out.flush()
                    out.fd.sync()
                }
                if (!tmp.renameTo(target)) {
                    // Fallback for filesystems where rename-over fails: still atomic on POSIX via NIO.
                    java.nio.file.Files.move(tmp.toPath(), target.toPath(),
                        java.nio.file.StandardCopyOption.REPLACE_EXISTING,
                        java.nio.file.StandardCopyOption.ATOMIC_MOVE)
                }
            } catch (t: Throwable) {
                tmp.delete()
                throw t
            }
        }
    }
}
