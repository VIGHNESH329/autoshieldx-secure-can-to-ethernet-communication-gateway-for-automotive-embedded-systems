package com.autoshieldx.dashboard.ui.dashboard

import androidx.compose.foundation.background
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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.autoshieldx.dashboard.ui.components.TelemetryMiniChart
import com.autoshieldx.dashboard.ui.theme.AccentCyan
import com.autoshieldx.dashboard.ui.theme.AccentTechBlue
import com.autoshieldx.dashboard.ui.theme.BackgroundLight
import com.autoshieldx.dashboard.ui.theme.ChartRpmGradientStart
import com.autoshieldx.dashboard.ui.theme.ChartTempGradientStart
import com.autoshieldx.dashboard.ui.theme.ChartVibGradientStart
import com.autoshieldx.dashboard.ui.theme.TextPrimary
import com.autoshieldx.dashboard.ui.theme.TextSecondary
import com.autoshieldx.dashboard.viewmodel.DashboardUiState
import java.util.Locale

@Composable
fun DashboardScreen(
    uiState: DashboardUiState,
    onPortChanged: (Int) -> Unit
) {
    var showPortDialog by remember { mutableStateOf(false) }
    var enteredPortText by remember { mutableStateOf(uiState.listeningPort.toString()) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        BackgroundLight,
                        Color(0xFFF1F5F9),
                        BackgroundLight
                    )
                )
            )
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp, vertical = 20.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // 1. Dashboard Header & Connectivity
            DashboardHeader(
                connectionStatus = uiState.connectionStatus,
                dataSourceMode = uiState.dataSourceMode,
                gatewayIp = uiState.gatewayIp,
                listeningPort = uiState.listeningPort,
                lastPacketTimestamp = uiState.lastPacketTimestamp,
                onConfigurePortClicked = {
                    enteredPortText = uiState.listeningPort.toString()
                    showPortDialog = true
                }
            )

            // 2. Vehicle Telemetry Grid
            TelemetryGrid(telemetry = uiState.telemetry)

            // 3. Security Status & Arbitration Counters
            SecurityOverviewSection(
                securityStatus = uiState.securityStatus,
                securityStats = uiState.securityStats
            )

            // 4. Live Charts Section
            Text(
                text = "REAL-TIME TELEMETRY CHARTS",
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                color = TextSecondary,
                letterSpacing = 1.sp
            )

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                TelemetryMiniChart(
                    modifier = Modifier.weight(1f),
                    title = "Speed Trend",
                    currentValue = String.format(Locale.US, "%.1f", uiState.telemetry.speedKmh),
                    unit = "km/h",
                    history = uiState.speedHistory,
                    lineColor = AccentTechBlue,
                    fillColorStart = AccentCyan.copy(alpha = 0.35f)
                )

                TelemetryMiniChart(
                    modifier = Modifier.weight(1f),
                    title = "RPM Profile",
                    currentValue = String.format(Locale.US, "%.0f", uiState.telemetry.rpm),
                    unit = "RPM",
                    history = uiState.rpmHistory,
                    lineColor = ChartRpmGradientStart,
                    fillColorStart = ChartRpmGradientStart.copy(alpha = 0.30f)
                )
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                TelemetryMiniChart(
                    modifier = Modifier.weight(1f),
                    title = "Temperature",
                    currentValue = String.format(Locale.US, "%.1f", uiState.telemetry.temperatureC),
                    unit = "°C",
                    history = uiState.tempHistory,
                    lineColor = ChartTempGradientStart,
                    fillColorStart = ChartTempGradientStart.copy(alpha = 0.30f)
                )

                TelemetryMiniChart(
                    modifier = Modifier.weight(1f),
                    title = "Vibration",
                    currentValue = String.format(Locale.US, "%.3f", uiState.telemetry.vibrationG),
                    unit = "g",
                    history = uiState.vibrationHistory,
                    lineColor = ChartVibGradientStart,
                    fillColorStart = ChartVibGradientStart.copy(alpha = 0.30f)
                )
            }

            // 5. Forensic Security Alerts Section
            SecurityAlertsList(alerts = uiState.recentAlerts)

            Spacer(modifier = Modifier.height(24.dp))
        }

        // Port Configuration Dialog
        if (showPortDialog) {
            AlertDialog(
                onDismissRequest = { showPortDialog = false },
                title = {
                    Text(
                        text = "UDP Telemetry Port",
                        fontWeight = FontWeight.Bold,
                        color = TextPrimary
                    )
                },
                text = {
                    Column {
                        Text(
                            text = "Set UDP listener port to match your Raspberry Pi 4 Gateway forward destination (Default 16000):",
                            fontSize = 13.sp,
                            color = TextSecondary
                        )
                        Spacer(modifier = Modifier.height(12.dp))
                        OutlinedTextField(
                            value = enteredPortText,
                            onValueChange = { enteredPortText = it.filter { char -> char.isDigit() } },
                            label = { Text("Port Number") },
                            singleLine = true,
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                },
                confirmButton = {
                    Button(
                        onClick = {
                            val portNum = enteredPortText.toIntOrNull()
                            if (portNum != null && portNum in 1024..65535) {
                                onPortChanged(portNum)
                            }
                            showPortDialog = false
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = AccentTechBlue)
                    ) {
                        Text("Apply")
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showPortDialog = false }) {
                        Text("Cancel", color = TextSecondary)
                    }
                },
                shape = RoundedCornerShape(20.dp),
                containerColor = Color.White
            )
        }
    }
}
