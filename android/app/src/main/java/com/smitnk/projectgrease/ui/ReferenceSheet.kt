package com.smitnk.projectgrease.ui

import android.content.Context
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.editor.LineArtOptions

/** Wireframe of the 3D reference over the canvas, mapped like the EGL surface's canvasPoint(). */
@Composable
internal fun ReferenceOverlay(controller: EditorController, tick: Int) {
    @Suppress("UNUSED_VARIABLE") val observed = tick
    val reference = controller.reference
    if (!reference.visible || reference.isEmpty) return
    val cw = controller.document.canvasWidth.coerceAtLeast(1)
    val ch = controller.document.canvasHeight.coerceAtLeast(1)
    val segments = reference.canvasSegments(cw, ch)
    Canvas(Modifier.fillMaxSize()) {
        val fit = minOf(size.width / cw, size.height / ch) * 0.92f * controller.view.zoom
        val ox = (size.width - cw * fit) * 0.5f + controller.view.panX
        val oy = (size.height - ch * fit) * 0.5f + controller.view.panY
        val color = Color(0x9926A69A)
        var i = 0
        while (i + 3 < segments.size) {
            drawLine(color, Offset(ox + segments[i] * fit, oy + segments[i + 1] * fit),
                Offset(ox + segments[i + 2] * fit, oy + segments[i + 3] * fit), 1.5f)
            i += 4
        }
    }
}

/**
 * 3D reference for Line Art (SPEC_LINE_ART_ARCHITECTURE batch 1): import OBJ meshes, set the
 * camera (perspective lens or orthographic scale, orbit, shift), see the mesh edges through
 * Line Art's camera. Line generation comes in later batches.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun ReferenceSheet(controller: EditorController, context: Context, onDismiss: () -> Unit, redraw: () -> Unit) {
    val reference = controller.reference
    val cw = controller.document.canvasWidth
    val ch = controller.document.canvasHeight
    var tick by remember { mutableIntStateOf(0) }
    @Suppress("UNUSED_VARIABLE") val observed = tick
    var camera by remember { mutableStateOf(reference.camera) }
    var lineThickness by remember { mutableFloatStateOf(3f) }
    var includeHidden by remember { mutableStateOf(false) }
    var bakeFrames by remember { mutableIntStateOf(24) }
    var bakeSweep by remember { mutableFloatStateOf((Math.PI / 2).toFloat()) }
    var options by remember { mutableStateOf(reference.lineArtOptions) }
    fun setOptions(value: LineArtOptions) {
        options = value
        reference.lineArtOptions = value
        tick++
    }
    fun apply(value: com.smitnk.projectgrease.editor.ReferenceCamera) {
        camera = value
        reference.setCamera(value, cw, ch)
        tick++
        redraw()
    }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        val bytes = runCatching { context.contentResolver.openInputStream(uri)?.use { it.readBytes() } }.getOrNull()
        val added = if (bytes == null) 0 else reference.importObj(bytes, cw, ch)
        toast(context, if (added > 0) "Imported $added objects" else "Not a usable OBJ file")
        tick++
        redraw()
    }
    ModalBottomSheet(onDismissRequest = onDismiss) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 20.dp)) {
            Text("3D reference (Line Art)", style = MaterialTheme.typography.headlineSmall)
            val s = reference.stats()
            Text("${s[0]} objects • ${s[1]} vertices • ${s[2]} triangles • ${s[3]} edges (${s[4]} loose)")
            Text("Preview Line Art's visible lines, then generate them as strokes on a new layer of the current frame.",
                style = MaterialTheme.typography.bodySmall)
            Row(Modifier.padding(vertical = 8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { picker.launch(arrayOf("model/obj", "text/plain", "application/octet-stream", "*/*")) }) { Text("Import OBJ") }
                OutlinedButton(onClick = { reference.clear(); tick++; redraw() }) { Text("Clear reference") }
            }
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text("Show over canvas", Modifier.weight(1f))
                Switch(reference.visible, { reference.setVisible(it); tick++; redraw() })
            }
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text("Line Art preview")
                    Text(if (reference.lineArtPreview) {
                        val (total, shown) = reference.lineArtCounts()
                        "Visible lines of Blender's Line Art: $shown of $total segments"
                    } else "Off: every mesh edge is shown", style = MaterialTheme.typography.bodySmall)
                }
                Switch(reference.lineArtPreview, { reference.setLineArtPreview(it, cw, ch); tick++; redraw() })
            }
            Text("Line Art strokes", Modifier.padding(top = 8.dp), style = MaterialTheme.typography.titleMedium)
            Text("Line thickness %.0f px".format(lineThickness))
            Slider(lineThickness, { lineThickness = it }, valueRange = 1f..20f)
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text("Include hidden lines (occlusion level 1)", Modifier.weight(1f))
                Switch(includeHidden, { includeHidden = it })
            }
            Text("Line Art options", Modifier.padding(top = 8.dp), style = MaterialTheme.typography.titleMedium)
            Text("Blender's Line Art modifier settings used by Generate and Bake.", style = MaterialTheme.typography.bodySmall)
            Text("Line types", Modifier.padding(top = 4.dp))
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for ((label, type) in listOf("Contour" to LineArtOptions.EDGE_CONTOUR, "Crease" to LineArtOptions.EDGE_CREASE,
                    "Material" to LineArtOptions.EDGE_MATERIAL, "Edge marks" to LineArtOptions.EDGE_MARK,
                    "Intersections" to LineArtOptions.EDGE_INTERSECTION, "Loose" to LineArtOptions.EDGE_LOOSE,
                    "Light contour" to LineArtOptions.EDGE_LIGHT_CONTOUR, "Cast shadow" to LineArtOptions.EDGE_SHADOW)) {
                    FilterChip(selected = options.hasType(type), onClick = { setOptions(options.withType(type, !options.hasType(type))) },
                        label = { Text(label) })
                }
            }
            Text("Crease threshold %.0f°".format(Math.toDegrees(options.creaseThreshold.toDouble())))
            Slider(options.creaseThreshold, { setOptions(options.copy(creaseThreshold = it)) }, valueRange = 0f..Math.PI.toFloat())
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text("Occlusion level range", Modifier.weight(1f))
                Switch(options.useMultipleLevels, { setOptions(options.copy(useMultipleLevels = it)) })
            }
            Text(if (options.useMultipleLevels) "Levels ${options.levelStart} to ${options.levelEnd}" else "Level ${options.levelStart}")
            Slider(options.levelStart.toFloat(), { setOptions(options.copy(levelStart = it.toInt())) }, valueRange = 0f..8f, steps = 7)
            if (options.useMultipleLevels) {
                Slider(options.levelEnd.toFloat(), { setOptions(options.copy(levelEnd = it.toInt())) }, valueRange = 0f..8f, steps = 7)
            }
            Text("Chaining", Modifier.padding(top = 4.dp))
            Text("Image threshold %.4f".format(options.chainingImageThreshold))
            Slider(options.chainingImageThreshold, { setOptions(options.copy(chainingImageThreshold = it)) }, valueRange = 0f..0.3f)
            Text("Smooth tolerance %.2f".format(options.chainSmoothTolerance))
            Slider(options.chainSmoothTolerance, { setOptions(options.copy(chainSmoothTolerance = it)) }, valueRange = 0f..1f)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for ((label, flag) in listOf("Loose edges" to LineArtOptions.CHAIN_LOOSE_EDGES,
                    "Geometry space" to LineArtOptions.CHAIN_GEOMETRY_SPACE, "Preserve details" to LineArtOptions.CHAIN_PRESERVE_DETAILS,
                    "Intersection with contour" to LineArtOptions.INTERSECTION_AS_CONTOUR,
                    "Back face culling" to LineArtOptions.USE_BACK_FACE_CULLING,
                    "Crease on smooth" to LineArtOptions.USE_CREASE_ON_SMOOTH)) {
                    FilterChip(selected = options.hasFlag(flag), onClick = { setOptions(options.withFlag(flag, !options.hasFlag(flag))) },
                        label = { Text(label) })
                }
            }
            Text("Light (shadow and light contour)", Modifier.padding(top = 4.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for ((label, type) in listOf("None" to -1, "Sun" to LineArtOptions.LIGHT_SUN, "Point" to LineArtOptions.LIGHT_POINT)) {
                    FilterChip(selected = options.lightType == type, onClick = { setOptions(options.copy(lightType = type)) }, label = { Text(label) })
                }
            }
            if (options.lightType >= 0) {
                Text("Light orbit %.0f°, elevation %.0f°".format(Math.toDegrees(options.lightYaw.toDouble()), Math.toDegrees(options.lightPitch.toDouble())))
                Slider(options.lightYaw, { setOptions(options.copy(lightYaw = it)) }, valueRange = (-Math.PI).toFloat()..Math.PI.toFloat())
                Slider(options.lightPitch, { setOptions(options.copy(lightPitch = it)) }, valueRange = -1.5f..1.5f)
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    for ((label, sel) in listOf("All" to 0, "Illuminated" to 1, "Shaded" to 2, "Illuminated shapes" to 3)) {
                        FilterChip(selected = options.shadowSelection == sel, onClick = { setOptions(options.copy(shadowSelection = sel)) }, label = { Text(label) })
                    }
                }
            }
            Text("Silhouette", Modifier.padding(top = 4.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for ((label, sel) in listOf("Off" to 0, "Group" to 1, "Individual" to 2)) {
                    FilterChip(selected = options.silhouetteSelection == sel, onClick = { setOptions(options.copy(silhouetteSelection = sel)) }, label = { Text(label) })
                }
            }
            Text("Masks (bits 1-8)", Modifier.padding(top = 4.dp))
            Text("Material mask")
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                FilterChip(selected = (options.maskSwitches and LineArtOptions.MATERIAL_MASK_ENABLE) != 0,
                    onClick = { setOptions(options.copy(maskSwitches = options.maskSwitches xor LineArtOptions.MATERIAL_MASK_ENABLE)) },
                    label = { Text("On") })
                FilterChip(selected = (options.maskSwitches and LineArtOptions.MATERIAL_MASK_MATCH) != 0,
                    onClick = { setOptions(options.copy(maskSwitches = options.maskSwitches xor LineArtOptions.MATERIAL_MASK_MATCH)) },
                    label = { Text("Exact") })
                for (bit in 0 until 8) {
                    FilterChip(selected = (options.materialMaskBits shr bit and 1) != 0,
                        onClick = { setOptions(options.copy(materialMaskBits = options.materialMaskBits xor (1 shl bit))) },
                        label = { Text("${bit + 1}") })
                }
            }
            Text("Intersection mask")
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                FilterChip(selected = (options.maskSwitches and LineArtOptions.INTERSECTION_MATCH) != 0,
                    onClick = { setOptions(options.copy(maskSwitches = options.maskSwitches xor LineArtOptions.INTERSECTION_MATCH)) },
                    label = { Text("Exact") })
                for (bit in 0 until 8) {
                    FilterChip(selected = (options.intersectionMask shr bit and 1) != 0,
                        onClick = { setOptions(options.copy(intersectionMask = options.intersectionMask xor (1 shl bit))) },
                        label = { Text("${bit + 1}") })
                }
            }
            val objectNames = reference.names(0)
            if (objectNames.isNotEmpty()) {
                Text("Source", Modifier.padding(top = 4.dp))
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    FilterChip(selected = options.sourceType == LineArtOptions.SOURCE_SCENE,
                        onClick = { setOptions(options.copy(sourceType = LineArtOptions.SOURCE_SCENE, sourceIndex = -1)) },
                        label = { Text("Scene") })
                    objectNames.forEachIndexed { index, name ->
                        FilterChip(selected = options.sourceType == LineArtOptions.SOURCE_OBJECT && options.sourceIndex == index,
                            onClick = { setOptions(options.copy(sourceType = LineArtOptions.SOURCE_OBJECT, sourceIndex = index)) },
                            label = { Text(name) })
                    }
                }
                Text("Object usage", Modifier.padding(top = 4.dp))
                objectNames.forEachIndexed { index, name ->
                    Text(name, style = MaterialTheme.typography.bodySmall)
                    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        for ((label, usage) in LineArtOptions.OBJECT_USAGES) {
                            FilterChip(selected = reference.objectUsage(index) == usage,
                                onClick = { reference.setObjectUsage(index, usage); tick++ },
                                label = { Text(label) })
                        }
                    }
                }
            }
            OutlinedButton(onClick = { setOptions(LineArtOptions()) }) { Text("Reset Line Art options") }
            Button(onClick = {
                val n = controller.generateLineArt(lineThickness, includeHidden)
                toast(context, if (n > 0) "Line Art: $n strokes on layer \"Line Art\"" else "No Line Art lines in view")
                redraw()
            }, enabled = !reference.isEmpty) { Text("Generate Line Art strokes") }
            Text("Bake to frames", Modifier.padding(top = 8.dp), style = MaterialTheme.typography.titleMedium)
            Text("The camera orbits from the start angle to the end angle; each frame gets its own Line Art on a new \"Line Art bake\" layer.",
                style = MaterialTheme.typography.bodySmall)
            Text("Frames 1 to $bakeFrames")
            Slider(bakeFrames.toFloat(), { bakeFrames = it.toInt() }, valueRange = 2f..120f)
            Text("Orbit %.0f° to %.0f°".format(Math.toDegrees(camera.yaw.toDouble()), Math.toDegrees((camera.yaw + bakeSweep).toDouble())))
            Slider(bakeSweep, { bakeSweep = it }, valueRange = (-2 * Math.PI).toFloat()..(2 * Math.PI).toFloat())
            OutlinedButton(onClick = {
                val n = controller.bakeLineArt(1, bakeFrames, camera.yaw, camera.yaw + bakeSweep, lineThickness, includeHidden)
                toast(context, if (n > 0) "Line Art baked on $n frames" else "No Line Art lines in view")
                tick++
                redraw()
            }, enabled = !reference.isEmpty) { Text("Bake Line Art to frames") }
            Text("Camera", Modifier.padding(top = 8.dp), style = MaterialTheme.typography.titleMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(selected = !camera.orthographic, onClick = { apply(camera.copy(orthographic = false)) }, label = { Text("Perspective") })
                FilterChip(selected = camera.orthographic, onClick = { apply(camera.copy(orthographic = true)) }, label = { Text("Orthographic") })
            }
            if (camera.orthographic) {
                Text("Orthographic scale %.2f".format(camera.orthoScale))
                Slider(camera.orthoScale, { apply(camera.copy(orthoScale = it)) }, valueRange = 0.5f..50f)
            } else {
                Text("Focal length %.0f mm".format(camera.lens))
                Slider(camera.lens, { apply(camera.copy(lens = it)) }, valueRange = 10f..200f)
            }
            Text("Orbit %.0f°".format(Math.toDegrees(camera.yaw.toDouble())))
            Slider(camera.yaw, { apply(camera.copy(yaw = it)) }, valueRange = (-Math.PI).toFloat()..Math.PI.toFloat())
            Text("Elevation %.0f°".format(Math.toDegrees(camera.pitch.toDouble())))
            Slider(camera.pitch, { apply(camera.copy(pitch = it)) }, valueRange = -1.5f..1.5f)
            Text("Distance %.1f".format(camera.distance))
            Slider(camera.distance, { apply(camera.copy(distance = it)) }, valueRange = 1f..80f)
            Text("Shift X %.2f".format(camera.shiftX))
            Slider(camera.shiftX, { apply(camera.copy(shiftX = it)) }, valueRange = -1f..1f)
            Text("Shift Y %.2f".format(camera.shiftY))
            Slider(camera.shiftY, { apply(camera.copy(shiftY = it)) }, valueRange = -1f..1f)
            OutlinedButton(onClick = { apply(com.smitnk.projectgrease.editor.ReferenceCamera()) }) { Text("Reset camera") }
            Spacer(Modifier.height(24.dp))
        }
    }
}
