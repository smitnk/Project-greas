package com.smitnk.projectgrease.editor

/** Save as: the project continues under the name of the file it was written to. */
object SaveAsNaming {
    /** "Sketch v2.gpjson" -> "Sketch v2"; null for a missing / blank name. */
    fun projectName(displayName: String?): String? {
        val trimmed = displayName?.trim().orEmpty()
        if (trimmed.isEmpty()) return null
        val base = trimmed.substringAfterLast('/')
        val stem = if (base.endsWith(".gpjson", ignoreCase = true) || base.endsWith(".json", ignoreCase = true))
            base.substringBeforeLast('.') else base
        return stem.trim().ifEmpty { null }
    }
}
