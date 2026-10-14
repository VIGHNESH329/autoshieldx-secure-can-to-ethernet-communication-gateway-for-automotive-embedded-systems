package com.autoshieldx.dashboard.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
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
import com.autoshieldx.dashboard.ui.theme.TextPrimary
import com.autoshieldx.dashboard.ui.theme.TextSecondary

@Composable
fun TelemetryMiniChart(
    modifier: Modifier = Modifier,
    title: String,
    currentValue: String,
    unit: String,
    history: List<Float>,
    lineColor: Color = AccentTechBlue,
    fillColorStart: Color = AccentCyan.copy(alpha = 0.35f),
    fillColorEnd: Color = AccentTechBlue.copy(alpha = 0.02f)
) {
    GlassmorphicCard(
        modifier = modifier,
        cornerRadius = 18.dp,
        elevation = 3.dp,
        borderColor = lineColor.copy(alpha = 0.20f)
    ) {
        Column(
            modifier = Modifier.fillMaxWidth()
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = title.uppercase(),
                    color = TextSecondary,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.SemiBold,
                    letterSpacing = 0.8.sp
                )

                Row(verticalAlignment = Alignment.Bottom) {
                    Text(
                        text = currentValue,
                        color = TextPrimary,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Spacer(modifier = Modifier.padding(1.dp))
                    Text(
                        text = unit,
                        color = TextSecondary,
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Medium
                    )
                }
            }

            Spacer(modifier = Modifier.height(10.dp))

            // Live Canvas chart
            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(65.dp)
            ) {
                if (history.size < 2) return@Canvas

                val minVal = (history.minOrNull() ?: 0f).coerceAtLeast(0f)
                val maxVal = (history.maxOrNull() ?: 100f).coerceAtLeast(minVal + 1f)
                val range = (maxVal - minVal).coerceAtLeast(0.001f)

                val width = size.width
                val height = size.height
                val stepX = width / (history.size - 1)

                val strokePath = Path()
                val fillPath = Path()

                history.forEachIndexed { index, value ->
                    val x = index * stepX
                    val normalizedY = 1f - ((value - minVal) / range).coerceIn(0f, 1f)
                    // Leave 4dp margin top and bottom
                    val y = 4f + (normalizedY * (height - 8f))

                    if (index == 0) {
                        strokePath.moveTo(x, y)
                        fillPath.moveTo(x, height)
                        fillPath.lineTo(x, y)
                    } else {
                        strokePath.lineTo(x, y)
                        fillPath.lineTo(x, y)
                    }

                    if (index == history.size - 1) {
                        fillPath.lineTo(x, height)
                        fillPath.close()
                    }
                }

                // Draw gradient area underneath
                drawPath(
                    path = fillPath,
                    brush = Brush.verticalGradient(
                        colors = listOf(fillColorStart, fillColorEnd)
                    )
                )

                // Draw curve stroke line
                drawPath(
                    path = strokePath,
                    color = lineColor,
                    style = Stroke(width = 3.dp.toPx(), cap = StrokeCap.Round)
                )

                // Draw end point indicator dot
                val lastVal = history.last()
                val lastNormY = 1f - ((lastVal - minVal) / range).coerceIn(0f, 1f)
                val lastY = 4f + (lastNormY * (height - 8f))
                drawCircle(
                    color = Color.White,
                    radius = 4.dp.toPx(),
                    center = Offset(width, lastY)
                )
                drawCircle(
                    color = lineColor,
                    radius = 2.5.dp.toPx(),
                    center = Offset(width, lastY)
                )
            }
        }
    }
}
