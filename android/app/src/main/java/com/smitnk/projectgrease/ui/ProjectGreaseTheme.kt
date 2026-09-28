
package com.smitnk.projectgrease.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val LightColors = lightColorScheme(
    primary = Color(0xFFE84F7B), onPrimary = Color.White,
    primaryContainer = Color(0xFFFFD9E3), secondary = Color(0xFF5D5F66),
    background = Color(0xFFF6F6F4), surface = Color.White,
    surfaceVariant = Color(0xFFECECEC), onSurface = Color(0xFF202124),
    onSurfaceVariant = Color(0xFF62656B)
)

private val DarkColors = darkColorScheme(
    primary = Color(0xFFFF6B96), onPrimary = Color(0xFF3B0718),
    primaryContainer = Color(0xFF6F1937), secondary = Color(0xFFBFC1C7),
    background = Color(0xFF121315), surface = Color(0xFF1B1D20),
    surfaceVariant = Color(0xFF292C31), onSurface = Color(0xFFF1F1F2),
    onSurfaceVariant = Color(0xFFB8BAC0)
)

enum class ProjectGreaseThemeMode { LIGHT, DARK, SYSTEM }

@Composable
fun ProjectGreaseTheme(
    mode: ProjectGreaseThemeMode = ProjectGreaseThemeMode.SYSTEM,
    content: @Composable () -> Unit
) {
    val dark = when (mode) {
        ProjectGreaseThemeMode.LIGHT -> false
        ProjectGreaseThemeMode.DARK -> true
        ProjectGreaseThemeMode.SYSTEM -> isSystemInDarkTheme()
    }
    MaterialTheme(
        colorScheme = if (dark) DarkColors else LightColors,
        content = content
    )
}
