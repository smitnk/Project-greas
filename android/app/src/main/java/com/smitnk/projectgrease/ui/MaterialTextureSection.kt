package com.smitnk.projectgrease.ui

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.FilterChip
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.MaterialTexture

/** Decodes an image (SAF URI) to ARGB pixels, downscaled to at most [maxSide] px on its long side. */
internal fun decodeTextureImage(context: Context, uri: Uri, maxSide: Int = 1024): Triple<IntArray, Int, Int>? = runCatching {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    context.contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, bounds) }
    var sample = 1
    while (bounds.outWidth / sample > maxSide * 2 || bounds.outHeight / sample > maxSide * 2) sample *= 2
    val decoded = context.contentResolver.openInputStream(uri)?.use {
        BitmapFactory.decodeStream(it, null, BitmapFactory.Options().apply { inSampleSize = sample })
    } ?: return null
    val scale = minOf(1f, maxSide.toFloat() / maxOf(decoded.width, decoded.height))
    val bmp = if (scale < 1f) Bitmap.createScaledBitmap(decoded, maxOf(1, (decoded.width * scale).toInt()), maxOf(1, (decoded.height * scale).toInt()), true) else decoded
    val px = IntArray(bmp.width * bmp.height)
    bmp.getPixels(px, 0, bmp.width, 0, 0, bmp.width, bmp.height)
    Triple(px, bmp.width, bmp.height).also { if (bmp !== decoded) bmp.recycle(); decoded.recycle() }
}.getOrNull()

/** After a project is opened: decode the saved texture images (URIs) and hand them to the editor. */
internal fun loadPendingTextureImages(context: Context, controller: EditorController) {
    for ((slot, fill, uri) in controller.texturesNeedingImages()) {
        val img = decodeTextureImage(context, Uri.parse(uri)) ?: continue
        controller.setMaterialTexture(slot, fill, controller.materialTexture(slot, fill), img.first, img.second, img.third)
    }
}

/**
 * Stroke / fill texture of the active material (MaterialGPencilStyle stroke_style / fill_style
 * Texture): pick an image, mix with the material colour, pixel size (stroke) or scale / offset /
 * angle (fill). The image URI is saved in the project.
 */
@Composable
fun MaterialTextureSection(controller: EditorController, context: Context, redraw: () -> Unit) {
    var fill by remember { mutableStateOf(false) }
    var tick by remember { mutableIntStateOf(0) }
    val slot = controller.materials.activeMaterial
    @Suppress("UNUSED_VARIABLE") val observed = tick
    val tex = controller.materialTexture(slot, fill)
    fun set(t: MaterialTexture, img: Triple<IntArray, Int, Int>? = null) {
        if (controller.setMaterialTexture(slot, fill, t, img?.first, img?.second ?: 0, img?.third ?: 0)) { tick++; redraw() }
    }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        runCatching { context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }
        val img = decodeTextureImage(context, uri) ?: return@rememberLauncherForActivityResult
        set(tex.copy(uri = uri.toString(), enabled = true), img)
    }
    Column(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 8.dp)) {
        Text("Texture", fontWeight = FontWeight.Bold)
        Row {
            FilterChip(selected = !fill, onClick = { fill = false }, label = { Text("Stroke") }, modifier = Modifier.padding(end = 6.dp))
            FilterChip(selected = fill, onClick = { fill = true }, label = { Text("Fill") })
        }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(if (tex.uri == null) "No image" else "Image picked", Modifier.weight(1f), fontSize = 12.sp)
            OutlinedButton(onClick = { picker.launch(arrayOf("image/*")) }) { Text("Pick image") }
        }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text("Use texture", Modifier.weight(1f))
            Switch(checked = tex.enabled, onCheckedChange = { set(tex.copy(enabled = it)) }, enabled = tex.uri != null)
        }
        Text("Mix with colour " + "%.2f".format(tex.mix), fontSize = 12.sp)
        Slider(tex.mix, { set(tex.copy(mix = it)) }, valueRange = 0f..1f)
        if (!fill) {
            Text("Pixel size " + tex.pixelSize.toInt(), fontSize = 12.sp)
            Slider(tex.pixelSize, { set(tex.copy(pixelSize = it)) }, valueRange = 10f..400f)
        } else {
            Text("Scale " + "%.2f".format(tex.scaleX), fontSize = 12.sp)
            Slider(tex.scaleX, { set(tex.copy(scaleX = it, scaleY = it)) }, valueRange = 0.1f..10f)
            Text("Angle " + Math.toDegrees(tex.angle.toDouble()).toInt() + "°", fontSize = 12.sp)
            Slider(tex.angle, { set(tex.copy(angle = it)) }, valueRange = -3.1416f..3.1416f)
            Text("Offset X " + "%.2f".format(tex.offsetX), fontSize = 12.sp)
            Slider(tex.offsetX, { set(tex.copy(offsetX = it)) }, valueRange = -1f..1f)
            Text("Offset Y " + "%.2f".format(tex.offsetY), fontSize = 12.sp)
            Slider(tex.offsetY, { set(tex.copy(offsetY = it)) }, valueRange = -1f..1f)
        }
    }
}
