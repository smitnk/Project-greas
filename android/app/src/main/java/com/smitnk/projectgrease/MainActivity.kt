package com.smitnk.projectgrease

import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.smitnk.projectgrease.nativebridge.GPNative
import com.smitnk.projectgrease.nativebridge.ProjectGreaseEglViewport
import com.smitnk.projectgrease.ui.ProjectGreaseEditor
import com.smitnk.projectgrease.ui.GreaseUiState

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Runtime smoke check: verifies System.loadLibrary() and the exported
        // JNI nativePing() entry point when the app actually starts.
        val jniReady = runCatching { GPNative.nativePing() }.getOrDefault(false)
        Log.i("ProjectGrease", "Android JNI smoke connection: $jniReady")

        setContent {
            MaterialTheme {
                Surface(modifier = Modifier) {
                    var state by remember { mutableStateOf(GreaseUiState()) }

                    ProjectGreaseEditor(
                        state = state,
                        onStateChange = { state = it },
                        blenderViewport = {
                            ProjectGreaseEglViewport(modifier = Modifier.fillMaxSize())
                        }
                    )
                }
            }
        }
    }
}
