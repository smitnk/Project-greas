package com.smitnk.projectgrease.ui

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

data class ProjectRecord(
    val name: String,
    val width: Int,
    val height: Int,
    val fps: Int,
    val lastOpened: Long
)

class ProjectStore(context: Context) {
    private val appContext = context.applicationContext
    /** Atomic, Android-free document storage (see ProjectFiles; unit-tested on the JVM). */
    val files = ProjectFiles(java.io.File(appContext.filesDir, "projects"))
    private val prefs = context.applicationContext.getSharedPreferences("project_grease_projects", Context.MODE_PRIVATE)
    private val key = "records"

    fun load(): List<ProjectRecord> {
        val raw = prefs.getString(key, null) ?: return emptyList()
        return runCatching {
            val array = JSONArray(raw)
            buildList {
                for (i in 0 until array.length()) {
                    val o = array.getJSONObject(i)
                    add(
                        ProjectRecord(
                            name = o.optString("name", "Project Grease"),
                            width = o.optInt("width", 1280).coerceAtLeast(1),
                            height = o.optInt("height", 720).coerceAtLeast(1),
                            fps = o.optInt("fps", 12).coerceIn(1, 120),
                            lastOpened = o.optLong("lastOpened", 0L)
                        )
                    )
                }
            }.sortedByDescending { it.lastOpened }
        }.getOrDefault(emptyList())
    }

    fun upsert(record: ProjectRecord) {
        val records = load().filterNot { it.name == record.name }.toMutableList()
        records.add(record)
        save(records.sortedByDescending { it.lastOpened })
    }

    /** Saves an existing project to its own file (overwrites it atomically). */
    fun saveDocument(name:String, json:String) = files.write(name, json)

    fun loadDocument(name:String):String? = files.read(name)

    /** A name for a new/duplicated project that collides with neither a file nor a record. */
    fun uniqueProjectName(name:String):String {
        val names = load().map { it.name }.toSet()
        return files.uniqueName(name) { it in names }
    }

    /** Duplicates [source] under a fresh unique name; never overwrites another project. */
    fun duplicateDocument(source:String, newName:String = source):String? {
        val names = load().map { it.name }.toSet()
        return files.duplicate(source, newName) { it in names }
    }

    fun save(records: List<ProjectRecord>) {
        val array = JSONArray()
        records.take(100).forEach { record ->
            array.put(
                JSONObject().apply {
                    put("name", record.name)
                    put("width", record.width)
                    put("height", record.height)
                    put("fps", record.fps)
                    put("lastOpened", record.lastOpened)
                }
            )
        }
        prefs.edit().putString(key, array.toString()).apply()
    }
}
