package com.smitnk.projectgrease.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

// Every M3 role is set: roles left out fall back to the baseline purple scheme, and sheets, menus,
// navigation bars and chips (which read surfaceContainer*) then looked mismatched.
private val LightColors = lightColorScheme(
    primary = Color(0xFFB4235A), onPrimary = Color.White,
    primaryContainer = Color(0xFFFFD9E1), onPrimaryContainer = Color(0xFF3F0019),
    secondary = Color(0xFF74565E), onSecondary = Color.White,
    secondaryContainer = Color(0xFFFFD9E1), onSecondaryContainer = Color(0xFF2B151B),
    tertiary = Color(0xFF7B5733), onTertiary = Color.White,
    tertiaryContainer = Color(0xFFFFDCBE), onTertiaryContainer = Color(0xFF2D1600),
    error = Color(0xFFBA1A1A), onError = Color.White,
    errorContainer = Color(0xFFFFDAD6), onErrorContainer = Color(0xFF410002),
    background = Color(0xFFFFF8F8), onBackground = Color(0xFF22191B),
    surface = Color(0xFFFFF8F8), onSurface = Color(0xFF22191B),
    surfaceVariant = Color(0xFFF3DDE1), onSurfaceVariant = Color(0xFF524346),
    outline = Color(0xFF847376), outlineVariant = Color(0xFFD6C2C5),
    inverseSurface = Color(0xFF382E30), inverseOnSurface = Color(0xFFFEEDEF), inversePrimary = Color(0xFFFFB1C5),
    surfaceDim = Color(0xFFE7D6D8), surfaceBright = Color(0xFFFFF8F8),
    surfaceContainerLowest = Color.White, surfaceContainerLow = Color(0xFFFFF0F2),
    surfaceContainer = Color(0xFFFBEAEC), surfaceContainerHigh = Color(0xFFF5E4E6),
    surfaceContainerHighest = Color(0xFFF0DEE1),
)

private val DarkColors = darkColorScheme(
    primary = Color(0xFFFFB1C5), onPrimary = Color(0xFF65002D),
    primaryContainer = Color(0xFF8F0F43), onPrimaryContainer = Color(0xFFFFD9E1),
    secondary = Color(0xFFE3BDC5), onSecondary = Color(0xFF422930),
    secondaryContainer = Color(0xFF5B3F46), onSecondaryContainer = Color(0xFFFFD9E1),
    tertiary = Color(0xFFEFBD93), onTertiary = Color(0xFF472A0A),
    tertiaryContainer = Color(0xFF61401E), onTertiaryContainer = Color(0xFFFFDCBE),
    error = Color(0xFFFFB4AB), onError = Color(0xFF690005),
    errorContainer = Color(0xFF93000A), onErrorContainer = Color(0xFFFFDAD6),
    background = Color(0xFF191113), onBackground = Color(0xFFEFDFE1),
    surface = Color(0xFF191113), onSurface = Color(0xFFEFDFE1),
    surfaceVariant = Color(0xFF524346), onSurfaceVariant = Color(0xFFD6C2C5),
    outline = Color(0xFF9F8C8F), outlineVariant = Color(0xFF524346),
    inverseSurface = Color(0xFFEFDFE1), inverseOnSurface = Color(0xFF382E30), inversePrimary = Color(0xFFB4235A),
    surfaceDim = Color(0xFF191113), surfaceBright = Color(0xFF413739),
    surfaceContainerLowest = Color(0xFF140C0E), surfaceContainerLow = Color(0xFF22191B),
    surfaceContainer = Color(0xFF261D1F), surfaceContainerHigh = Color(0xFF312829),
    surfaceContainerHighest = Color(0xFF3C3234),
)

private val GreaseShapes = Shapes(
    extraSmall = RoundedCornerShape(4.dp),
    small = RoundedCornerShape(8.dp),
    medium = RoundedCornerShape(12.dp),
    large = RoundedCornerShape(16.dp),
    extraLarge = RoundedCornerShape(28.dp),
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
        shapes = GreaseShapes,
        content = content
    )
}
