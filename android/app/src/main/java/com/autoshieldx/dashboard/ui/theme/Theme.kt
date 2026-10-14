package com.autoshieldx.dashboard.ui.theme

import android.app.Activity
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

// STRICTLY LIGHT THEME ONLY - Premium Automotive Cybersecurity Palette
private val LightColorScheme = lightColorScheme(
    primary = AccentTechBlue,
    onPrimary = SurfaceLight,
    primaryContainer = CardGlassBackground,
    onPrimaryContainer = TextPrimary,
    secondary = AccentCyan,
    onSecondary = SurfaceLight,
    background = BackgroundLight,
    onBackground = TextPrimary,
    surface = SurfaceLight,
    onSurface = TextPrimary,
    surfaceVariant = BackgroundGradientEnd,
    onSurfaceVariant = TextSecondary,
    outline = CardGlassBorderSubtle,
    error = SecurityCriticalRed,
    onError = SurfaceLight
)

@Composable
fun AutoShieldXTheme(
    content: @Composable () -> Unit
) {
    val colorScheme = LightColorScheme
    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as Activity).window
            window.statusBarColor = BackgroundLight.toArgb()
            window.navigationBarColor = BackgroundLight.toArgb()
            WindowCompat.getInsetsController(window, view).isAppearanceLightStatusBars = true
            WindowCompat.getInsetsController(window, view).isAppearanceLightNavigationBars = true
        }
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = Typography,
        content = content
    )
}
