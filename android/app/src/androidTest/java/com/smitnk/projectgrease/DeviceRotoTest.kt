package com.smitnk.projectgrease

import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.smitnk.projectgrease.editor.Rotoscope
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

/** Rotoscope reference on the device: under the strokes, with its opacity, in step with the timeline. */
@RunWith(AndroidJUnit4::class)
class DeviceRotoTest : SweepBase() {
    private fun png(name: String, color: Int): String {
        val f = File(instrumentation.targetContext.cacheDir, name)
        val b = Bitmap.createBitmap(64, 36, Bitmap.Config.ARGB_8888).apply { eraseColor(color) }
        f.outputStream().use { b.compress(Bitmap.CompressFormat.PNG, 100, it) }
        return Uri.fromFile(f).toString()
    }

    @Test fun sequenceFollowsTheTimelineUnderTheStrokes() {
        val red = png("roto_red.png", Color.RED); val blue = png("roto_blue.png", Color.BLUE)
        onUi {
            controller.rotoscope.set(Rotoscope.Kind.SEQUENCE, listOf(red, blue), 2)
            controller.rotoscope.setOpacity(1f)
            controller.createFrame(2)
            controller.selectFrame(1)
            controller.rotoscopeChanged()
        }
        line(300f)
        var bmp = shot("roto_frame1")
        val c1 = pixel(bmp, 640f, 500f)
        assertTrue("frame 1 shows the red image: ${Integer.toHexString(c1)}", Color.red(c1) > 200 && Color.blue(c1) < 60)
        assertTrue("the stroke is drawn over the reference", inkNear(bmp, 600f, 300f))
        onUi { controller.selectFrame(2) }
        bmp = shot("roto_frame2")
        val c2 = pixel(bmp, 640f, 500f)
        assertTrue("frame 2 shows the blue image: ${Integer.toHexString(c2)}", Color.blue(c2) > 200 && Color.red(c2) < 60)
        onUi { controller.rotoscope.setOpacity(0.5f); controller.rotoscopeChanged() }
        val c3 = pixel(shot("roto_half"), 640f, 500f)
        assertTrue("half opacity mixes with the paper: ${Integer.toHexString(c3)}", Color.red(c3) in 90..200 && Color.blue(c3) > 200)
        // saved with the project
        assertEquals("SEQUENCE", document().getJSONObject("rotoscope").getString("kind"))
    }
}
