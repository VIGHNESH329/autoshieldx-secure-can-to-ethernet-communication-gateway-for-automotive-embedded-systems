package com.autoshieldx.dashboard.ui.dashboard

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
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
import com.autoshieldx.dashboard.data.SecurityStats
import com.autoshieldx.dashboard.data.SecurityStatus
import com.autoshieldx.dashboard.ui.components.GlassmorphicCard
import com.autoshieldx.dashboard.ui.components.SecurityStatusBadge
import com.autoshieldx.dashboard.ui.theme.AccentCyan
import com.autoshieldx.dashboard.ui.theme.AccentTechBlue
import com.autoshieldx.dashboard.ui.theme.SecurityCriticalRed
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedGreen
import com.autoshieldx.dashboard.ui.theme.SecurityWarningAmber
import com.autoshieldx.dashboard.ui.theme.TextPrimary
import com.autoshieldx.dashboard.ui.theme.TextSecondary
import com.autoshieldx.dashboard.ui.theme.TextTertiary

@Composable
fun SecurityOverviewSection(
    securityStatus: SecurityStatus,
    securityStats: SecurityStats,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // Section Header
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "SECURITY STATUS",
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                color = TextSecondary,
                letterSpacing = 1.sp
            )
            Text(
                text = "RFC 2104 HMAC-SHA256",
                fontSize = 11.sp,
                fontWeight = FontWeight.SemiBold,
                color = AccentTechBlue
            )
        }

        // 4 Security Pipeline Stages
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            val isHmacOk = securityStatus.hmacStatus == "VERIFIED"
            SecurityStatusBadge(
                modifier = Modifier.weight(1f),
                stageName = "HMAC Auth",
                statusText = securityStatus.hmacStatus,
                isNormal = isHmacOk,
                isWarning = false
            )

            val isValOk = securityStatus.frameValidation == "ACTIVE"
            SecurityStatusBadge(
                modifier = Modifier.weight(1f),
                stageName = "Frame Validation",
                statusText = securityStatus.frameValidation,
                isNormal = isValOk,
                isWarning = false
            )
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            val isReplayOk = securityStatus.replayProtection == "ACTIVE"
            SecurityStatusBadge(
                modifier = Modifier.weight(1f),
                stageName = "Replay Defense",
                statusText = securityStatus.replayProtection,
                isNormal = isReplayOk,
                isWarning = false
            )

            val isMlOk = securityStatus.mlDetection == "NORMAL"
            SecurityStatusBadge(
                modifier = Modifier.weight(1f),
                stageName = "ML Anomaly",
                statusText = securityStatus.mlDetection,
                isNormal = isMlOk,
                isWarning = !isMlOk
            )
        }

        // Live Statistics Strip
        GlassmorphicCard(
            modifier = Modifier.fillMaxWidth(),
            cornerRadius = 18.dp,
            elevation = 3.dp,
            borderColor = AccentTechBlue.copy(alpha = 0.2f)
        ) {
            Column(modifier = Modifier.fillMaxWidth()) {
                Text(
                    text = "GATEWAY ARBITRATION METRICS",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = TextTertiary,
                    letterSpacing = 0.8.sp
                )

                Spacer(modifier = Modifier.height(10.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    StatCounterPill(
                        label = "Valid",
                        count = securityStats.validFrames,
                        accentColor = SecurityVerifiedGreen
                    )
                    StatCounterPill(
                        label = "Rejected",
                        count = securityStats.rejectedFrames,
                        accentColor = SecurityCriticalRed
                    )
                    StatCounterPill(
                        label = "HMAC Err",
                        count = securityStats.hmacFailures,
                        accentColor = SecurityCriticalRed
                    )
                    StatCounterPill(
                        label = "Replays",
                        count = securityStats.replayDetections,
                        accentColor = SecurityCriticalRed
                    )
                    StatCounterPill(
                        label = "ML Flags",
                        count = securityStats.mlAnomalies,
                        accentColor = SecurityWarningAmber
                    )
                }
            }
        }
    }
}

@Composable
private fun StatCounterPill(
    label: String,
    count: Long,
    accentColor: Color
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = count.toString(),
            fontSize = 16.sp,
            fontWeight = FontWeight.Bold,
            color = if (count > 0 && accentColor != SecurityVerifiedGreen) accentColor else TextPrimary
        )
        Text(
            text = label,
            fontSize = 10.sp,
            fontWeight = FontWeight.Medium,
            color = TextSecondary
        )
    }
}
