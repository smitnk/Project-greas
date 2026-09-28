package com.smitnk.projectgrease

import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.smitnk.projectgrease.editor.EditorController
import com.smitnk.projectgrease.nativebridge.GPNative
import com.smitnk.projectgrease.nativebridge.ProjectGreaseEglViewport
import com.smitnk.projectgrease.ui.GreaseUiState
import com.smitnk.projectgrease.ui.ProjectGreaseEditor
import com.smitnk.projectgrease.ui.ProjectGreaseTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val jniReady = runCatching { GPNative.nativePing() }.getOrDefault(false)
        Log.i("ProjectGrease", "Android JNI smoke connection: " + jniReady)

        setContent {
            ProjectGreaseTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    val controller = remember { EditorController() }
                    var state by remember { mutableStateOf(GreaseUiState()) }

                    ProjectGreaseEditor(
                        controller = controller,
                        state = state,
                        onStateChange = { state = it },
                        blenderViewport = {
                            ProjectGreaseEglViewport(
                                modifier = Modifier.fillMaxSize(),
                                controller = controller
                            )
                        }
                    )
                }
            }
        }
    }
}
