package com.autoshieldx.dashboard.ui.splash

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autoshieldx.dashboard.ui.theme.AccentCyan
import com.autoshieldx.dashboard.ui.theme.AccentTechBlue
import com.autoshieldx.dashboard.ui.theme.BackgroundLight
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedGreen
import com.autoshieldx.dashboard.ui.theme.TextPrimary
import com.autoshieldx.dashboard.ui.theme.TextSecondary
import com.autoshieldx.dashboard.ui.theme.TextTertiary
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

@Composable
fun FuturisticCarSplashScreen(
    onSplashFinished: () -> Unit
) {
    val carAlpha = remember { Animatable(0f) }
    val carTranslation = remember { Animatable(-35f) }
    val scanlineProgress = remember { Animatable(0f) }
    val textAlpha = remember { Animatable(0f) }
    val pulseGlow = remember { Animatable(0.4f) }

    LaunchedEffect(Unit) {
        launch {
            pulseGlow.animateTo(
                targetValue = 1f,
                animationSpec = infiniteRepeatable(
                    animation = tween(1200, easing = LinearEasing),
                    repeatMode = RepeatMode.Reverse
                )
            )
        }

        // 1. Vehicle silhouette glides in smoothly
        launch {
            carTranslation.animateTo(
                targetValue = 0f,
                animationSpec = tween(900, easing = FastOutSlowInEasing)
            )
        }
        carAlpha.animateTo(
            targetValue = 1f,
            animationSpec = tween(800, easing = FastOutSlowInEasing)
        )

        // 2. Cyan cyber scanline sweeps across the silhouette
        scanlineProgress.animateTo(
            targetValue = 1f,
            animationSpec = tween(900, easing = FastOutSlowInEasing)
        )

        // 3. AutoShieldX title & gateway info fade in
        textAlpha.animateTo(
            targetValue = 1f,
            animationSpec = tween(500, easing = FastOutSlowInEasing)
        )

        delay(800) // Brief pause to appreciate the animation
        onSplashFinished()
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        BackgroundLight,
                        Color(0xFFEFF6FF),
                        BackgroundLight
                    )
                )
            )
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null
            ) {
                // Tap to skip
                onSplashFinished()
            },
        contentAlignment = Alignment.Center
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
            modifier = Modifier.padding(24.dp)
        ) {
            // Futuristic car silhouette canvas
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(180.dp),
                contentAlignment = Alignment.Center
            ) {
                Canvas(
                    modifier = Modifier
                        .fillMaxWidth(0.9f)
                        .height(140.dp)
                        .alpha(carAlpha.value)
                ) {
                    val w = size.width
                    val h = size.height
                    val offsetX = carTranslation.value

                    // Center reference box
                    val startX = w * 0.05f + offsetX
                    val endX = w * 0.95f + offsetX
                    val groundY = h * 0.82f

                    // Draw subtle ground speed line
                    drawLine(
                        brush = Brush.horizontalGradient(
                            colors = listOf(Color.Transparent, AccentCyan.copy(alpha = 0.5f), Color.Transparent)
                        ),
                        start = Offset(0f, groundY + 4f),
                        end = Offset(w, groundY + 4f),
                        strokeWidth = 2.dp.toPx()
                    )

                    // Vehicle Aero Silhouette Path (Sleek sports hypercar silhouette)
                    val carPath = Path().apply {
                        // Rear bumper
                        moveTo(startX, groundY - 10f)
                        // Rear diffuser
                        lineTo(startX + (w * 0.04f), groundY - 2f)
                        // Rear wheel arch cutout
                        arcTo(
                            rect = androidx.compose.ui.geometry.Rect(
                                left = startX + (w * 0.08f),
                                top = groundY - 32f,
                                right = startX + (w * 0.26f),
                                bottom = groundY + 6f
                            ),
                            startAngleDegrees = 180f,
                            sweepAngleDegrees = 180f,
                            forceMoveTo = false
                        )
                        // Side skirt
                        lineTo(startX + (w * 0.56f), groundY - 2f)
                        // Front wheel arch cutout
                        arcTo(
                            rect = androidx.compose.ui.geometry.Rect(
                                left = startX + (w * 0.58f),
                                top = groundY - 32f,
                                right = startX + (w * 0.76f),
                                bottom = groundY + 6f
                            ),
                            startAngleDegrees = 180f,
                            sweepAngleDegrees = 180f,
                            forceMoveTo = false
                        )
                        // Front splitter
                        lineTo(endX, groundY - 4f)
                        // Front nose / aggressive bumper
                        lineTo(endX, groundY - 18f)
                        // Front bonnet / hood rake
                        cubicTo(
                            x1 = endX - (w * 0.06f), y1 = groundY - 30f,
                            x2 = endX - (w * 0.16f), y2 = groundY - 44f,
                            x3 = endX - (w * 0.22f), y3 = groundY - 48f
                        )
                        // Windscreen rake
                        cubicTo(
                            x1 = endX - (w * 0.28f), y1 = groundY - 58f,
                            x2 = endX - (w * 0.36f), y2 = groundY - 76f,
                            x3 = endX - (w * 0.44f), y3 = groundY - 78f
                        )
                        // Sleek fastback roofline
                        cubicTo(
                            x1 = endX - (w * 0.52f), y1 = groundY - 79f,
                            x2 = endX - (w * 0.64f), y2 = groundY - 74f,
                            x3 = endX - (w * 0.72f), y3 = groundY - 58f
                        )
                        // Rear bootlid ducktail spoiler
                        lineTo(startX + (w * 0.08f), groundY - 42f)
                        lineTo(startX, groundY - 36f)
                        close()
                    }

                    // Fill sleek car silhouette with rich metallic tech gradient
                    drawPath(
                        path = carPath,
                        brush = Brush.horizontalGradient(
                            colors = listOf(
                                Color(0xFF0F172A),
                                Color(0xFF1E293B),
                                Color(0xFF334155),
                                Color(0xFF0F172A)
                            )
                        )
                    )

                    // Draw aerodynamic outline
                    drawPath(
                        path = carPath,
                        brush = Brush.horizontalGradient(
                            colors = listOf(AccentTechBlue, AccentCyan, Color(0xFF60A5FA))
                        ),
                        style = Stroke(width = 2.5.dp.toPx(), cap = StrokeCap.Round)
                    )

                    // Front LED Headlight Beam
                    val headLightPath = Path().apply {
                        moveTo(endX - (w * 0.02f), groundY - 22f)
                        lineTo(endX + (w * 0.12f), groundY - 30f)
                        lineTo(endX + (w * 0.12f), groundY + 8f)
                        close()
                    }
                    drawPath(
                        path = headLightPath,
                        brush = Brush.horizontalGradient(
                            colors = listOf(AccentCyan.copy(alpha = 0.6f), Color.Transparent)
                        )
                    )

                    // Rear Cyber Tail Light Glow
                    drawLine(
                        color = Color(0xFFEF4444),
                        start = Offset(startX, groundY - 36f),
                        end = Offset(startX + 14f, groundY - 34f),
                        strokeWidth = 3.dp.toPx(),
                        cap = StrokeCap.Round
                    )

                    // Front & Rear Wheels
                    val rearWheelCenter = Offset(startX + (w * 0.17f), groundY - 13f)
                    val frontWheelCenter = Offset(startX + (w * 0.67f), groundY - 13f)
                    val wheelRadius = 17.dp.toPx()

                    // Wheels
                    listOf(rearWheelCenter, frontWheelCenter).forEach { center ->
                        drawCircle(color = Color(0xFF0F172A), radius = wheelRadius, center = center)
                        drawCircle(
                            color = AccentCyan.copy(alpha = 0.8f),
                            radius = wheelRadius,
                            center = center,
                            style = Stroke(width = 2.dp.toPx())
                        )
                        drawCircle(color = Color(0xFFCBD5E1), radius = wheelRadius * 0.45f, center = center)
                    }

                    // Dynamic Cyber Scanline Sweep
                    if (scanlineProgress.value in 0.01f..0.99f) {
                        val scanX = startX + ((endX - startX) * scanlineProgress.value)
                        drawLine(
                            brush = Brush.verticalGradient(
                                colors = listOf(Color.Transparent, AccentCyan, Color.Transparent)
                            ),
                            start = Offset(scanX, 0f),
                            end = Offset(scanX, h),
                            strokeWidth = 3.5.dp.toPx(),
                            cap = StrokeCap.Round
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(20.dp))

            // AutoShieldX Branding
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.alpha(textAlpha.value)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Box(
                        modifier = Modifier
                            .size(10.dp)
                            .background(SecurityVerifiedGreen, CircleShape)
                    )
                    Spacer(modifier = Modifier.size(8.dp))
                    Text(
                        text = "AUTOSHIELDX",
                        color = TextPrimary,
                        fontSize = 30.sp,
                        fontWeight = FontWeight.ExtraBold,
                        letterSpacing = 2.sp
                    )
                }

                Spacer(modifier = Modifier.height(6.dp))

                Text(
                    text = "Secure CAN-to-Ethernet Gateway",
                    color = AccentTechBlue,
                    fontSize = 15.sp,
                    fontWeight = FontWeight.SemiBold,
                    letterSpacing = 0.5.sp
                )

                Spacer(modifier = Modifier.height(4.dp))

                Text(
                    text = "Hardware In-The-Loop Defense Pipeline Active",
                    color = TextTertiary,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Medium
                )
            }
        }
    }
}
