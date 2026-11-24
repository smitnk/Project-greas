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
// Blender-style neutral greys with one accent (Blender's active orange). Every M3 role is set: roles
// left out fall back to the baseline purple scheme. Text/background pairs are >= 4.5:1
// (onSurface on surface ~14:1 dark, ~15:1 light; orange primary vs its on-colour >= 5.9:1).
private val Accent = Color(0xFFFF9F2A)      // dark theme accent
private val AccentDeep = Color(0xFFA34E00)  // light theme accent (5.9:1 on white)

private val LightColors = lightColorScheme(
    primary = AccentDeep, onPrimary = Color.White,
    primaryContainer = Color(0xFFFFDCC2), onPrimaryContainer = Color(0xFF301400),
    secondary = Color(0xFF4A4A4A), onSecondary = Color.White,
    secondaryContainer = Color(0xFFE0E0E0), onSecondaryContainer = Color(0xFF1A1A1A),
    tertiary = Color(0xFF2F5F9E), onTertiary = Color.White,
    tertiaryContainer = Color(0xFFD6E3FF), onTertiaryContainer = Color(0xFF001B3E),
    error = Color(0xFFBA1A1A), onError = Color.White,
    errorContainer = Color(0xFFFFDAD6), onErrorContainer = Color(0xFF410002),
    background = Color(0xFFF4F4F4), onBackground = Color(0xFF151515),
    surface = Color(0xFFF4F4F4), onSurface = Color(0xFF151515),
    surfaceVariant = Color(0xFFDDDDDD), onSurfaceVariant = Color(0xFF3F3F3F),
    outline = Color(0xFF6E6E6E), outlineVariant = Color(0xFFC4C4C4),
    inverseSurface = Color(0xFF2B2B2B), inverseOnSurface = Color(0xFFEEEEEE), inversePrimary = Accent,
    surfaceDim = Color(0xFFDADADA), surfaceBright = Color(0xFFF9F9F9),
    surfaceContainerLowest = Color.White, surfaceContainerLow = Color(0xFFEEEEEE),
    surfaceContainer = Color(0xFFE8E8E8), surfaceContainerHigh = Color(0xFFE2E2E2),
    surfaceContainerHighest = Color(0xFFDCDCDC),
)

private val DarkColors = darkColorScheme(
    primary = Accent, onPrimary = Color(0xFF1A0C00),
    primaryContainer = Color(0xFF6B3500), onPrimaryContainer = Color(0xFFFFDCC2),
    secondary = Color(0xFFC8C8C8), onSecondary = Color(0xFF1D1D1D),
    secondaryContainer = Color(0xFF474747), onSecondaryContainer = Color(0xFFEEEEEE),
    tertiary = Color(0xFF8FB8FF), onTertiary = Color(0xFF00214A),
    tertiaryContainer = Color(0xFF284777), onTertiaryContainer = Color(0xFFD6E3FF),
    error = Color(0xFFFFB4AB), onError = Color(0xFF690005),
    errorContainer = Color(0xFF93000A), onErrorContainer = Color(0xFFFFDAD6),
    background = Color(0xFF1D1D1D), onBackground = Color(0xFFEEEEEE),
    surface = Color(0xFF1D1D1D), onSurface = Color(0xFFEEEEEE),
    surfaceVariant = Color(0xFF3A3A3A), onSurfaceVariant = Color(0xFFCFCFCF),
    outline = Color(0xFF9A9A9A), outlineVariant = Color(0xFF474747),
    inverseSurface = Color(0xFFEEEEEE), inverseOnSurface = Color(0xFF2B2B2B), inversePrimary = AccentDeep,
    surfaceDim = Color(0xFF1D1D1D), surfaceBright = Color(0xFF444444),
    surfaceContainerLowest = Color(0xFF141414), surfaceContainerLow = Color(0xFF232323),
    surfaceContainer = Color(0xFF282828), surfaceContainerHigh = Color(0xFF303030),
    surfaceContainerHighest = Color(0xFF3A3A3A),
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
