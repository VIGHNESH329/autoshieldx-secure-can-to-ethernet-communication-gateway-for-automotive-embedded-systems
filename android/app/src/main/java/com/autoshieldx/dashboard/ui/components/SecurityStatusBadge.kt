package com.autoshieldx.dashboard.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autoshieldx.dashboard.ui.theme.SecurityCriticalLight
import com.autoshieldx.dashboard.ui.theme.SecurityCriticalRed
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedGreen
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedLight
import com.autoshieldx.dashboard.ui.theme.SecurityWarningAmber
import com.autoshieldx.dashboard.ui.theme.SecurityWarningLight
import com.autoshieldx.dashboard.ui.theme.TextPrimary
import com.autoshieldx.dashboard.ui.theme.TextSecondary

@Composable
fun SecurityStatusBadge(
    modifier: Modifier = Modifier,
    stageName: String,
    statusText: String,
    isNormal: Boolean = true,
    isWarning: Boolean = false
) {
    val (bgColor, textColor, borderColor) = when {
        !isNormal -> Triple(SecurityCriticalLight, SecurityCriticalRed, SecurityCriticalRed.copy(alpha = 0.4f))
        isWarning -> Triple(SecurityWarningLight, SecurityWarningAmber, SecurityWarningAmber.copy(alpha = 0.4f))
        else -> Triple(SecurityVerifiedLight, SecurityVerifiedGreen, SecurityVerifiedGreen.copy(alpha = 0.3f))
    }

    val iconText = when {
        !isNormal -> "✕"
        isWarning -> "⚠"
        else -> "✓"
    }

    GlassmorphicCard(
        modifier = modifier,
        cornerRadius = 16.dp,
        elevation = 2.dp,
        borderColor = borderColor
    ) {
        Column(
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(
                text = stageName.uppercase(),
                color = TextSecondary,
                fontSize = 10.sp,
                fontWeight = FontWeight.SemiBold,
                letterSpacing = 0.7.sp
            )

            Spacer(modifier = Modifier.height(6.dp))

            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(12.dp))
                        .background(bgColor)
                        .border(1.dp, borderColor, RoundedCornerShape(12.dp))
                        .padding(horizontal = 8.dp, vertical = 4.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = iconText,
                            color = textColor,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold
                        )
                        Spacer(modifier = Modifier.size(4.dp))
                        Text(
                            text = statusText,
                            color = textColor,
                            fontSize = 11.sp,
                            fontWeight = FontWeight.SemiBold
                        )
                    }
                }
            }
        }
    }
}
