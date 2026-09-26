package com.smitnk.projectgrease

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.smitnk.projectgrease.ui.ProjectGreaseEditor
import com.smitnk.projectgrease.ui.GreaseUiState

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            MaterialTheme {
                Surface(modifier = Modifier) {
                    var state by remember { mutableStateOf(GreaseUiState()) }

                    ProjectGreaseEditor(
                        state = state,
                        onStateChange = { state = it },
                        blenderViewport = {
                            // Native Blender GP viewport is connected in the next layer.
                        }
                    )
                }
            }
        }
    }
}
