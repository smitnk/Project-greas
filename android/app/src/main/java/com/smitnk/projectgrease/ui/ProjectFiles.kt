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

    fun exists(name: String): Boolean = fileFor(name).isFile

    fun read(name: String): String? = fileFor(name).takeIf { it.isFile }?.readText(Charsets.UTF_8)

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

        fun safeName(name: String): String =
            name.replace(Regex("[^A-Za-z0-9._-]"), "_").ifBlank { "Project_Grease" }

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
