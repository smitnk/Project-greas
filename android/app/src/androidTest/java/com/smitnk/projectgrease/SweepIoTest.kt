package com.smitnk.projectgrease

import android.graphics.BitmapFactory
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.GifEncoder
import com.smitnk.projectgrease.editor.GreaseTemplates
import com.smitnk.projectgrease.editor.ProjectSettings
import com.smitnk.projectgrease.editor.VectorExport
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.ByteArrayOutputStream
import java.io.File

/** Templates, import SVG, trace image, save/load round trip, exports (PNG/GIF/PNG sequence/MP4/SVG/PDF), project settings, Line Art. */
@RunWith(AndroidJUnit4::class)
class SweepIoTest : SweepBase() {

    private fun artifact(name: String, bytes: ByteArray) {
        val f = File(rule.activity.cacheDir, name); f.writeBytes(bytes)
        shell("cp ${f.absolutePath} $SHOT_DIR/$name")
    }

    private fun settings(end: Int) = assertEquals(null, onUi {
        controller.applyProjectSettings(ProjectSettings().apply {
            width = controller.document.canvasWidth; height = controller.document.canvasHeight; fps = 12; frameStart = 1; frameEnd = end
        })
    })

    @Test fun templates() {
        for (t in GreaseTemplates.ALL) {
            assertTrue("template ${t.id}", onUi { controller.resetDocument() && controller.applyTemplate(t) })
            assertEquals(t.layers, onUi { (0 until controller.layerCount()).map { controller.layerName(it) } })
            assertTrue(onUi { controller.materialCount() } >= t.materials.size)
            shot("template_${t.id}")
        }
    }

    @Test fun importSvg() {
        val svg = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
            <path d="M10 10 L90 10 L90 90 L10 90 Z" stroke="#ff0000" stroke-width="4" fill="none"/>
            <circle cx="50" cy="50" r="20" fill="#0000ff"/></svg>"""
        val n = onUi { controller.importSvg(svg) }
        assertTrue("imported strokes $n", n >= 2)
        assertEquals(n, strokes().size)
        val bmp = shot("import_svg")
        assertTrue("red outline on screen", countPixels(bmp) { Color.red(it) > 180 && Color.green(it) < 80 && Color.blue(it) < 80 } > 0)
        assertTrue("blue circle on screen", countPixels(bmp) { Color.blue(it) > 180 && Color.red(it) < 80 } > 0)
    }

    @Test fun traceImage() {
        val w = 64; val h = 64
        val img = IntArray(w * h) { i -> val x = i % w; val y = i / w; if (x in 16..47 && y in 16..47) 0xFF000000.toInt() else 0xFFFFFFFF.toInt() }
        val n = onUi { controller.traceImage(img, w, h, 0.5f, false, 1f) }
        assertTrue("traced outlines $n", n >= 1)
        assertTrue(strokes().all { it.getBoolean("cyclic") })
        assertTrue("trace drawn", countPixels(shot("trace_image")) { isDark(it) } > 0)
    }

    @Test fun saveLoadRoundTrip() {
        drag(200f to 300f, 600f to 200f, 1000f to 300f)
        onUi { controller.createFrame(4); controller.selectFrame(4) }
        line(500f)
        onUi { controller.addModifier(com.smitnk.projectgrease.editor.ModifierType.NOISE); controller.addVertexGroup("G") }
        val first = onUi { controller.saveDocumentJson() }!!
        assertTrue(onUi { controller.resetDocument() })
        assertEquals(0, strokes().size)
        assertTrue(onUi { controller.loadDocumentJson(first) })
        val second = onUi { controller.saveDocumentJson() }!!
        assertEquals(first, second)
        artifact("roundtrip.json", first.toByteArray())
        onUi { controller.selectFrame(1); controller.render() }
        assertInk(shot("save_load_round_trip"), 600f, 200f, "reloaded stroke")
    }

    @Test fun exportPng() {
        onUi { controller.brushes.setSize(30f) }; line(300f)
        val px = onUi { controller.exportCanvasPixels(false) }!!
        val w = onUi { controller.document.canvasWidth }; val h = onUi { controller.document.canvasHeight }
        assertEquals(w * h, px.size)
        val bmp = android.graphics.Bitmap.createBitmap(px, w, h, android.graphics.Bitmap.Config.ARGB_8888)
        val out = ByteArrayOutputStream(); bmp.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, out)
        artifact("export.png", out.toByteArray())
        val back = BitmapFactory.decodeByteArray(out.toByteArray(), 0, out.size())
        assertEquals(w, back.width)
        assertTrue("stroke in the PNG", isDark(back.getPixel(600, 300)))
        assertEquals("project background", onUi { controller.projectSettings.background }, back.getPixel(600, 700))
        val transparent = onUi { controller.exportCanvasPixels(true) }!!
        assertEquals("transparent background", 0, Color.alpha(transparent[700 * w + 600]))
        shot("export_png")
    }

    @Test fun exportGif() {
        line(300f); settings(3)
        val out = ByteArrayOutputStream(); var frames = 0
        val ok = onUi {
            val gif = GifEncoder(out, controller.document.canvasWidth, controller.document.canvasHeight, 12)
            gif.begin(); val r = controller.renderExportFrames(false) { _, p -> gif.addFrame(p); frames++ }; gif.finish(); r
        }
        artifact("export.gif", out.toByteArray())
        assertTrue(ok); assertEquals(3, frames)
        assertTrue(BitmapFactory.decodeByteArray(out.toByteArray(), 0, out.size()) != null)
        shot("export_gif")
    }

    @Test fun exportPngSequence() {
        line(300f); onUi { controller.createFrame(3); controller.selectFrame(3) }; line(500f); settings(4)
        val names = ArrayList<String>(); var dark = 0
        val w = onUi { controller.document.canvasWidth }
        val ok = onUi { controller.renderExportFrames(false) { item, p -> names += item.fileName; if (isDark(p[300 * w + 600])) dark++ } }
        assertTrue(ok); assertEquals(4, names.size); assertEquals(4, names.toSet().size)
        assertEquals("frames 1,2 show the first key; 3,4 the second", 2, dark)
        shot("export_png_sequence")
    }

    @Test fun exportMp4() {
        line(300f); settings(6)
        val file = File(rule.activity.cacheDir, "export.mp4")
        val frames = onUi { com.smitnk.projectgrease.ui.VideoExport.exportToFile(controller, file) }
        shell("cp ${file.absolutePath} $SHOT_DIR/export.mp4")
        assertEquals(6, frames)
        val r = android.media.MediaMetadataRetriever()
        try {
            r.setDataSource(file.absolutePath)
            assertEquals("6", r.extractMetadata(android.media.MediaMetadataRetriever.METADATA_KEY_VIDEO_FRAME_COUNT))
            assertTrue(r.getFrameAtIndex(0) != null)
        } finally { r.release() }
        shot("export_mp4")
    }

    @Test fun exportSvg() {
        onUi { controller.selectMaterial(2); controller.setMaterialColor(0xFFFF0000.toInt()) }; line(300f)
        val pages = onUi { controller.exportPages(listOf(1)) }
        assertEquals(1, pages.size)
        val svg = VectorExport.toSvg(pages[0])
        artifact("export.svg", svg.toByteArray())
        assertTrue(svg.startsWith("<?xml") || svg.contains("<svg"))
        assertTrue("red stroke in SVG", svg.contains("#ff0000", ignoreCase = true))
        // re-import it: the SVG round trips through our own importer
        assertTrue(onUi { controller.resetDocument() })
        assertTrue(onUi { controller.importSvg(svg) } >= 1)
        shot("export_svg_reimported")
    }

    @Test fun exportPdf() {
        line(300f); onUi { controller.createFrame(2) }
        val pdf = VectorExport.toPdf(onUi { controller.exportPages(listOf(1, 2)) })
        artifact("export.pdf", pdf)
        assertEquals("%PDF", String(pdf, 0, 4, Charsets.US_ASCII))
        val fd = File(rule.activity.cacheDir, "export.pdf")
        val renderer = android.graphics.pdf.PdfRenderer(android.os.ParcelFileDescriptor.open(fd, android.os.ParcelFileDescriptor.MODE_READ_ONLY))
        try { assertEquals(2, renderer.pageCount) } finally { renderer.close() }
        shot("export_pdf")
    }

    @Test fun projectSettings() {
        val err = onUi {
            controller.applyProjectSettings(ProjectSettings().apply { width = 1280; height = 720; fps = 30; frameStart = 5; frameEnd = 60; background = 0xFF203040.toInt() })
        }
        assertEquals(null, err)
        val bad = onUi { controller.applyProjectSettings(ProjectSettings().apply { width = 0 }) }
        assertTrue("invalid settings rejected", bad != null)
        assertEquals(1280, onUi { controller.document.canvasWidth })
        assertEquals(30, onUi { controller.animation.fps })
        shot("project_settings")
        // Exports use the project background (the viewport keeps its paper colour, as Blender's does).
        var first = 0
        assertTrue(onUi { controller.renderExportFrames(false) { _, p -> if (first == 0) first = p[360 * 1280 + 640] } })
        assertEquals(0xFF203040.toInt(), first)
        assertEquals(0xFF203040.toInt(), onUi { controller.exportCanvasPixels(false) }!![10])
    }

    @Test fun lineArt() {
        val cube = """v -1 -1 -1
v 1 -1 -1
v 1 1 -1
v -1 1 -1
v -1 -1 1
v 1 -1 1
v 1 1 1
v -1 1 1
f 1 2 3 4
f 5 8 7 6
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f 5 1 4 8
"""
        val added = onUi { controller.reference.importObj(cube.toByteArray(), controller.document.canvasWidth, controller.document.canvasHeight) }
        assertTrue("OBJ loaded $added", added > 0)
        val n = onUi { controller.generateLineArt() }
        assertTrue("line art strokes $n", n > 0)
        assertTrue("Line Art" in onUi { (0 until controller.layerCount()).map { controller.layerName(it) } })
        assertTrue("line art drawn", countPixels(shot("line_art")) { isDark(it) } > 0)
        val baked = onUi { controller.bakeLineArt(1, 3, 0f, 0.5f) }
        assertEquals(3, baked)
    }
}
