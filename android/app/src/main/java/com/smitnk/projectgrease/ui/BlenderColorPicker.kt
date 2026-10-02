package com.smitnk.projectgrease.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.smitnk.projectgrease.editor.ColorMath

/**
 * Blender-style color picker: saturation/value square, hue strip and hex field. Values use
 * Blender's HSV math (ColorMath). The picked color is reported as ARGB with alpha 0xFF; opacity
 * stays on the separate opacity slider.
 */
@Composable
fun BlenderColorPicker(argb: Int, onColor: (Int) -> Unit, modifier: Modifier = Modifier) {
    val start = remember(argb) { ColorMath.argbToHsva(argb) }
    var h by remember(argb) { mutableStateOf(start.h) }
    var s by remember(argb) { mutableStateOf(start.s) }
    var v by remember(argb) { mutableStateOf(start.v) }
    var hex by remember(argb) { mutableStateOf(ColorMath.toHex(argb)) }

    fun emit() {
        val c = ColorMath.hsvaToArgb(ColorMath.Hsva(h, s, v, 1f))
        hex = ColorMath.toHex(c)
        onColor(c)
    }
    val hueColor = Color(ColorMath.hsvaToArgb(ColorMath.Hsva(h, 1f, 1f, 1f)))

    Column(modifier.padding(horizontal = 20.dp)) {
        // saturation (x) / value (y) square
        Box(
            Modifier.fillMaxWidth().aspectRatio(1.6f)
                .pointerInput(Unit) {
                    fun pick(o: Offset) {
                        s = (o.x / size.width).coerceIn(0f, 1f)
                        v = 1f - (o.y / size.height).coerceIn(0f, 1f)
                        emit()
                    }
                    detectTapGestures { pick(it) }
                }
                .pointerInput(Unit) {
                    detectDragGestures { change, _ ->
                        s = (change.position.x / size.width).coerceIn(0f, 1f)
                        v = 1f - (change.position.y / size.height).coerceIn(0f, 1f)
                        emit()
                    }
                }
        ) {
            Canvas(Modifier.fillMaxWidth().aspectRatio(1.6f)) {
                drawRect(Brush.horizontalGradient(listOf(Color.White, hueColor)))
                drawRect(Brush.verticalGradient(listOf(Color.Transparent, Color.Black)))
                val c = Offset(s * size.width, (1f - v) * size.height)
                drawCircle(Color.Black, radius = 9f, center = c, style = Stroke(width = 4f))
                drawCircle(Color.White, radius = 7f, center = c, style = Stroke(width = 2f))
            }
        }
        Spacer(Modifier.height(10.dp))
        // hue strip
        val hues = remember { (0..6).map { Color(ColorMath.hsvaToArgb(ColorMath.Hsva(it / 6f, 1f, 1f, 1f))) } }
        Box(
            Modifier.fillMaxWidth().height(28.dp)
                .pointerInput(Unit) {
                    detectTapGestures { h = (it.x / size.width).coerceIn(0f, 0.9999f); emit() }
                }
                .pointerInput(Unit) {
                    detectDragGestures { change, _ ->
                        h = (change.position.x / size.width).coerceIn(0f, 0.9999f); emit()
                    }
                }
        ) {
            Canvas(Modifier.fillMaxWidth().height(28.dp)) {
                drawRect(Brush.horizontalGradient(hues))
                val x = h * size.width
                drawRect(Color.White, topLeft = Offset(x - 3f, 0f),
                    size = androidx.compose.ui.geometry.Size(6f, size.height), style = Stroke(width = 3f))
            }
        }
        Spacer(Modifier.height(10.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                Modifier.size(44.dp)
                    .background(Color(ColorMath.hsvaToArgb(ColorMath.Hsva(h, s, v, 1f))), RoundedCornerShape(8.dp))
                    .border(1.dp, Color.Gray, RoundedCornerShape(8.dp))
            )
            Spacer(Modifier.width(12.dp))
            OutlinedTextField(
                value = hex,
                onValueChange = { text ->
                    hex = text
                    ColorMath.parseHex(text)?.let {
                        val hsva = ColorMath.argbToHsva(it)
                        h = hsva.h; s = hsva.s; v = hsva.v
                        onColor(it)
                    }
                },
                singleLine = true,
                label = { Text("Hex") },
                modifier = Modifier.weight(1f)
            )
        }
        Text(
            "H %.3f  S %.3f  V %.3f".format(h, s, v),
            fontSize = 11.sp,
            modifier = Modifier.padding(top = 4.dp)
        )
    }
}
