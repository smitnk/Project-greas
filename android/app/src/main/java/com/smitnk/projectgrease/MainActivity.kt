package com.smitnk.projectgrease

import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.Surface
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.nativebridge.GPNative
import com.smitnk.projectgrease.nativebridge.ProjectGreaseEglViewport
import com.smitnk.projectgrease.ui.ProjectGreaseApp
import com.smitnk.projectgrease.ui.ProjectGreaseTheme

class MainActivity : ComponentActivity() {
    /** One editor per activity (the instrumented tests drive and inspect it through the activity). */
    val controller by lazy { EditorController() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val jniReady = runCatching { GPNative.nativePing() }.getOrDefault(false)
        Log.i("ProjectGrease", "Android JNI smoke connection: " + jniReady)
        setContent {
            ProjectGreaseTheme {
                // targetSdk 35 draws edge-to-edge: inset the whole app (top bar, canvas, timeline) by the
                // system bars and display cutout so the header no longer sits under the status bar.
                Surface(Modifier.fillMaxSize()) {
                  androidx.compose.foundation.layout.Box(Modifier.fillMaxSize().safeDrawingPadding()) {
                    val controller = remember { this@MainActivity.controller }
                    ProjectGreaseApp(controller = controller, blenderViewport = {
                        ProjectGreaseEglViewport(Modifier.fillMaxSize(), controller)
                    })
                  }
                }
            }
        }
    }
}
