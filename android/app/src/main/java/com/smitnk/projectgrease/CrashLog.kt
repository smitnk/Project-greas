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

    fun read(context: Context): String? = file(context).takeIf { it.isFile }?.readText()
    fun clear(context: Context) { file(context).delete() }

    fun share(context: Context, text: String) {
        val send = Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, text)
            .putExtra(Intent.EXTRA_SUBJECT, "Project Grease crash report")
        context.startActivity(Intent.createChooser(send, "Share crash report").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    }
}
