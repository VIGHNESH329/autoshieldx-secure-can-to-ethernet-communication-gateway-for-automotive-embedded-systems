package com.autoshieldx.dashboard.ui.dashboard

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
import com.autoshieldx.dashboard.data.SecurityAlert
import com.autoshieldx.dashboard.ui.components.GlassmorphicCard
import com.autoshieldx.dashboard.ui.theme.SecurityCriticalLight
import com.autoshieldx.dashboard.ui.theme.SecurityCriticalRed
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedGreen
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedLight
import com.autoshieldx.dashboard.ui.theme.SecurityWarningAmber
import com.autoshieldx.dashboard.ui.theme.SecurityWarningLight
import com.autoshieldx.dashboard.ui.theme.TextPrimary
import com.autoshieldx.dashboard.ui.theme.TextSecondary
import com.autoshieldx.dashboard.ui.theme.TextTertiary

@Composable
fun SecurityAlertsList(
    alerts: List<SecurityAlert>,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "SECURITY FORENSICS & ALERTS",
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                color = TextSecondary,
                letterSpacing = 1.sp
            )
            Text(
                text = if (alerts.isEmpty()) "0 active threats" else "${alerts.size} events",
                fontSize = 11.sp,
                fontWeight = FontWeight.SemiBold,
                color = if (alerts.isEmpty()) SecurityVerifiedGreen else SecurityCriticalRed
            )
        }

        if (alerts.isEmpty()) {
            GlassmorphicCard(
                modifier = Modifier.fillMaxWidth(),
                cornerRadius = 16.dp,
                elevation = 2.dp,
                borderColor = SecurityVerifiedGreen.copy(alpha = 0.25f)
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Box(
                        modifier = Modifier
                            .size(28.dp)
                            .clip(CircleShape)
                            .background(SecurityVerifiedLight),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = "✓",
                            color = SecurityVerifiedGreen,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }

                    Spacer(modifier = Modifier.size(12.dp))

                    Column {
                        Text(
                            text = "No active threats detected",
                            fontSize = 13.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = TextPrimary
                        )
                        Text(
                            text = "Vehicle CAN bus authentic • HMAC & Replay defenses nominal",
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Normal,
                            color = TextSecondary
                        )
                    }
                }
            }
        } else {
            alerts.take(5).forEach { alert ->
                AlertItemCard(alert = alert)
            }
        }
    }
}

@Composable
private fun AlertItemCard(alert: SecurityAlert) {
    val isCritical = alert.severity.equals("CRITICAL", ignoreCase = true) ||
            alert.messageStatus.contains("REJECT", ignoreCase = true)
    val isWarning = alert.severity.equals("WARNING", ignoreCase = true)

    val (badgeBg, badgeText, badgeBorder) = when {
        isCritical -> Triple(SecurityCriticalLight, SecurityCriticalRed, SecurityCriticalRed.copy(alpha = 0.4f))
        isWarning -> Triple(SecurityWarningLight, SecurityWarningAmber, SecurityWarningAmber.copy(alpha = 0.4f))
        else -> Triple(SecurityVerifiedLight, SecurityVerifiedGreen, SecurityVerifiedGreen.copy(alpha = 0.3f))
    }

    GlassmorphicCard(
        modifier = Modifier.fillMaxWidth(),
        cornerRadius = 14.dp,
        elevation = 2.dp,
        borderColor = badgeBorder
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.Top
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(6.dp))
                            .background(badgeBg)
                            .border(1.dp, badgeBorder, RoundedCornerShape(6.dp))
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = alert.attackType,
                            color = badgeText,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }
                    Spacer(modifier = Modifier.size(8.dp))
                    Text(
                        text = if (alert.canId > 0) "CAN 0x${alert.canId.toString(16).uppercase()}" else "CAN Bus",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = TextSecondary
                    )
                }

                Spacer(modifier = Modifier.height(4.dp))

                Text(
                    text = alert.details.ifEmpty { alert.eventType },
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Medium,
                    color = TextPrimary
                )
            }

            Text(
                text = if (alert.timestampIso.length >= 19) alert.timestampIso.substring(11, 19) else "Live",
                fontSize = 10.sp,
                fontWeight = FontWeight.Normal,
                color = TextTertiary
            )
        }
    }
}
