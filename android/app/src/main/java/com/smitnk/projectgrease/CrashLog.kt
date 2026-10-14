package com.smitnk.projectgrease

import android.content.Context
import android.content.Intent
import android.os.Build
import java.io.File

/** Keeps the stack trace of the last app crash (Kotlin / Java) so it can be shared from the menu. */
object CrashLog {
    private fun file(context: Context) = File(context.filesDir, "last_crash.txt")

    fun install(context: Context) {
        val app = context.applicationContext
        // native (C/C++) crashes land in the same file
        runCatching { com.smitnk.projectgrease.nativebridge.GPNative.nativeInstallCrashHandler(file(app).absolutePath) }
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            runCatching {
                file(app).writeText("Project Grease crash  ${java.util.Date()}\nAndroid ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT}), " +
                    "${Build.MANUFACTURER} ${Build.MODEL}\nthread ${thread.name}\n\n${error.stackTraceToString()}")
            }
            previous?.uncaughtException(thread, error)
        }
    }

    private fun freezeFile(context: Context) = File(context.filesDir, "last_freeze.txt")
    private val slow = ArrayDeque<String>()

    /** A freeze is not a crash: a watchdog thread writes where the main thread is stuck after 3 s. */
    private var watching = false
    fun watchFreezes(context: Context) {
        if (watching) return
        watching = true
        val app = context.applicationContext
        val main = android.os.Handler(android.os.Looper.getMainLooper())
        Thread({
            var tick = 0L
            val seen = java.util.concurrent.atomic.AtomicLong(0)
            while (true) {
                val want = ++tick
                main.post { seen.set(want) }
                Thread.sleep(3000)
                if (seen.get() >= want) continue
                val t = android.os.Looper.getMainLooper().thread
                runCatching {
                    freezeFile(app).writeText("Project Grease froze (main thread stuck over 3 s)  ${java.util.Date()}\n" +
                        "${Build.MANUFACTURER} ${Build.MODEL}, Android ${Build.VERSION.RELEASE}\n\n" +
                        t.stackTrace.joinToString("\n") { "  at $it" } + "\n\nSlow steps before it:\n" + synchronized(slow) { slow.joinToString("\n") })
                }
                while (seen.get() < want) Thread.sleep(500) // one record per freeze
            }
        }, "pg-freeze-watch").apply { isDaemon = true }.start()
    }

    /** Remembers the last slow steps (render, fill, export) for the report. */
    fun noteSlow(what: String) = synchronized(slow) {
        slow.addLast("${java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.US).format(java.util.Date())} $what")
        while (slow.size > 15) slow.removeFirst()
    }

    fun read(context: Context): String? {
        val parts = listOfNotNull(
            file(context).takeIf { it.isFile }?.readText(),
            freezeFile(context).takeIf { it.isFile }?.readText(),
            synchronized(slow) { slow.takeIf { it.isNotEmpty() }?.joinToString("\n", "Slow steps this session:\n") })
        return parts.takeIf { it.isNotEmpty() }?.joinToString("\n\n")
    }
    fun clear(context: Context) { file(context).delete(); freezeFile(context).delete(); synchronized(slow) { slow.clear() } }

    fun share(context: Context, text: String) {
        val send = Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text)
            .putExtra(Intent.EXTRA_SUBJECT, "Project Grease crash report")
        context.startActivity(Intent.createChooser(send, "Share crash report").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    }
}
