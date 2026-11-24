package com.smitnk.projectgrease.ui

import android.content.Context
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import com.smitnk.projectgrease.editor.EditorController

internal fun toast(context: Context, text: String) { com.smitnk.projectgrease.CrashLog.event("MESSAGE $text"); Toast.makeText(context, text, Toast.LENGTH_SHORT).show() }

private const val MAX_SVG_BYTES = 20 * 1024 * 1024

/** Project > Import SVG: picks an .svg through the Storage Access Framework and imports it. */
@Composable
fun rememberSvgImport(controller: EditorController, context: Context, onDone: () -> Unit): () -> Unit {
    val scope = androidx.compose.runtime.rememberCoroutineScope()
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch {
            // read off the main thread; a file over the cap is refused before it is held in memory
            val text = withContext(Dispatchers.IO) {
                runCatching {
                    context.contentResolver.openInputStream(uri)?.use { s ->
                        val out = java.io.ByteArrayOutputStream()
                        val chunk = ByteArray(64 * 1024)
                        while (true) {
                            val n = s.read(chunk)
                            if (n < 0) break
                            out.write(chunk, 0, n)
                            if (out.size() > MAX_SVG_BYTES) return@runCatching ""
                        }
                        out.toString(Charsets.UTF_8.name())
                    }
                }.getOrNull()
            }
            if (text != null && text.isEmpty()) toast(context, "SVG is too large (limit 20 MB)")
            else {
                val count = if (text == null) 0 else controller.importSvg(text)
                toast(context, if (count > 0) "Imported $count strokes" else "Nothing imported")
            }
            onDone()
        }
    }
    return { launcher.launch(arrayOf("image/svg+xml", "text/xml", "application/xml", "*/*")) }
}
