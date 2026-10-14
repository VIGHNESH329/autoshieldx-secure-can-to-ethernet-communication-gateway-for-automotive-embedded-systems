package com.autoshieldx.dashboard.ui.dashboard

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autoshieldx.dashboard.data.ConnectionStatus
import com.autoshieldx.dashboard.data.DataSourceMode
import com.autoshieldx.dashboard.ui.components.GlassmorphicCard
import com.autoshieldx.dashboard.ui.theme.AccentCyan
import com.autoshieldx.dashboard.ui.theme.AccentTechBlue
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedGreen
import com.autoshieldx.dashboard.ui.theme.SecurityVerifiedLight
import com.autoshieldx.dashboard.ui.theme.SecurityWarningAmber
import com.autoshieldx.dashboard.ui.theme.SecurityWarningLight
import com.autoshieldx.dashboard.ui.theme.TextPrimary
import com.autoshieldx.dashboard.ui.theme.TextSecondary
import com.autoshieldx.dashboard.ui.theme.TextTertiary

@Composable
fun DashboardHeader(
    connectionStatus: ConnectionStatus,
    dataSourceMode: DataSourceMode,
    gatewayIp: String,
    listeningPort: Int,
    lastPacketTimestamp: Long,
    onConfigurePortClicked: () -> Unit
) {
    val infiniteTransition = rememberInfiniteTransition(label = "pulseTransition")
    val pulseAlpha by infiniteTransition.animateFloat(
        initialValue = 0.4f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(1000, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "pulseAlpha"
    )

    val isConnected = (connectionStatus == ConnectionStatus.CONNECTED)
    val statusDotColor = if (isConnected) SecurityVerifiedGreen else SecurityWarningAmber
    val statusBgColor = if (isConnected) SecurityVerifiedLight else SecurityWarningLight
    val statusBorderColor = if (isConnected) SecurityVerifiedGreen.copy(alpha = 0.3f) else SecurityWarningAmber.copy(alpha = 0.3f)

    val statusTitle = if (isConnected) {
        "Gateway Connected"
    } else {
        "Gateway Standby"
    }

    val statusSubtitle = if (isConnected) {
        "UDP $gatewayIp:$listeningPort • Real CAN Telemetry"
    } else {
        "Phone Kinematics Active • Listening UDP :$listeningPort"
    }

    GlassmorphicCard(
        modifier = Modifier.fillMaxWidth(),
        cornerRadius = 22.dp,
        elevation = 4.dp
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(
                            modifier = Modifier
                                .size(8.dp)
                                .clip(CircleShape)
                                .background(AccentTechBlue)
                        )
                        Spacer(modifier = Modifier.size(6.dp))
                        Text(
                            text = "AUTOSHIELDX",
                            fontSize = 22.sp,
                            fontWeight = FontWeight.ExtraBold,
                            color = TextPrimary,
                            letterSpacing = 1.2.sp
                        )
                    }
                    Text(
                        text = "Secure CAN-to-Ethernet Gateway",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        color = AccentTechBlue,
                        letterSpacing = 0.3.sp
                    )
                }

                // Port & Diagnostic pill
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(12.dp))
                        .background(Color(0xFFF1F5F9))
                        .clickable { onConfigurePortClicked() }
                        .padding(horizontal = 10.dp, vertical = 6.dp)
                ) {
                    Text(
                        text = "PORT :$listeningPort",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextSecondary,
                        letterSpacing = 0.5.sp
                    )
                }
            }

            Spacer(modifier = Modifier.height(14.dp))

            // Connection state banner
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp))
                    .background(statusBgColor)
                    .border(1.dp, statusBorderColor, RoundedCornerShape(14.dp))
                    .padding(horizontal = 12.dp, vertical = 8.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(
                            modifier = Modifier
                                .size(10.dp)
                                .clip(CircleShape)
                                .background(statusDotColor.copy(alpha = pulseAlpha))
                        )
                        Spacer(modifier = Modifier.size(8.dp))
                        Column {
                            Text(
                                text = statusTitle,
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Bold,
                                color = TextPrimary
                            )
                            Text(
                                text = statusSubtitle,
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Medium,
                                color = TextSecondary
                            )
                        }
                    }

                    if (isConnected && lastPacketTimestamp > 0) {
                        val ageMs = (System.currentTimeMillis() - lastPacketTimestamp).coerceAtLeast(0)
                        Text(
                            text = if (ageMs < 1000) "Live <1s" else "${ageMs / 1000}s ago",
                            fontSize = 10.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = SecurityVerifiedGreen
                        )
                    }
                }
            }
        }
    }
}
