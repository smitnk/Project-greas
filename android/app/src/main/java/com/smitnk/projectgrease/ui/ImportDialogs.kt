package com.smitnk.projectgrease.ui

import android.content.Context
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import com.smitnk.projectgrease.editor.EditorController

internal fun toast(context: Context, text: String) = Toast.makeText(context, text, Toast.LENGTH_SHORT).show()

/** Project > Import SVG: picks an .svg through the Storage Access Framework and imports it. */
@Composable
fun rememberSvgImport(controller: EditorController, context: Context, onDone: () -> Unit): () -> Unit {
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        val text = runCatching {
            context.contentResolver.openInputStream(uri)?.use { it.readBytes().toString(Charsets.UTF_8) }
        }.getOrNull()
        val count = if (text == null) 0 else controller.importSvg(text)
        toast(context, if (count > 0) "Imported $count strokes" else "Nothing imported")
        onDone()
    }
    return { launcher.launch(arrayOf("image/svg+xml", "text/xml", "application/xml", "*/*")) }
}
