package com.autoshieldx.dashboard.ui.dashboard

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.autoshieldx.dashboard.data.VehicleTelemetry
import com.autoshieldx.dashboard.ui.components.TelemetryCard
import com.autoshieldx.dashboard.ui.theme.AccentCyan
import com.autoshieldx.dashboard.ui.theme.AccentElectricCobalt
import com.autoshieldx.dashboard.ui.theme.AccentTechBlue
import com.autoshieldx.dashboard.ui.theme.ChartRpmGradientStart
import com.autoshieldx.dashboard.ui.theme.ChartTempGradientStart
import com.autoshieldx.dashboard.ui.theme.ChartVibGradientStart
import java.util.Locale

@Composable
fun TelemetryGrid(
    telemetry: VehicleTelemetry,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // Row 1: Primary Powertrain Kinematics (Speed & RPM)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            TelemetryCard(
                modifier = Modifier.weight(1f),
                title = "Speed",
                value = String.format(Locale.US, "%.1f", telemetry.speedKmh),
                unit = "km/h",
                progressFraction = (telemetry.speedKmh / 200.0).toFloat(),
                accentColor = AccentTechBlue,
                subInfo = "CAN ID: 0x${telemetry.canId.toString(16).uppercase()}"
            )

            TelemetryCard(
                modifier = Modifier.weight(1f),
                title = "Engine RPM",
                value = String.format(Locale.US, "%.0f", telemetry.rpm),
                unit = "RPM",
                progressFraction = (telemetry.rpm / 7000.0).toFloat(),
                accentColor = ChartRpmGradientStart,
                subInfo = "Powertrain Core"
            )
        }

        // Row 2: Dynamics & Steering (Steering & Wheel Speed)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            TelemetryCard(
                modifier = Modifier.weight(1f),
                title = "Steering",
                value = String.format(Locale.US, "%+.1f", telemetry.steeringDeg),
                unit = "deg",
                progressFraction = ((telemetry.steeringDeg + 45.0) / 90.0).toFloat(),
                accentColor = AccentCyan,
                subInfo = if (telemetry.steeringDeg >= 0) "Right Vector" else "Left Vector"
            )

            TelemetryCard(
                modifier = Modifier.weight(1f),
                title = "Wheel Speed",
                value = String.format(Locale.US, "%.1f", telemetry.wheelSpeedKmh),
                unit = "km/h",
                progressFraction = (telemetry.wheelSpeedKmh / 200.0).toFloat(),
                accentColor = AccentElectricCobalt,
                subInfo = "ABS Dynamics"
            )
        }

        // Row 3: Environmental & Physical Safety (Temp, Vibration, Throttle)
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            TelemetryCard(
                modifier = Modifier.weight(1f),
                title = "Coolant Temp",
                value = String.format(Locale.US, "%.1f", telemetry.temperatureC),
                unit = "°C",
                progressFraction = (telemetry.temperatureC / 110.0).toFloat(),
                accentColor = ChartTempGradientStart,
                subInfo = "Thermal State"
            )

            TelemetryCard(
                modifier = Modifier.weight(1f),
                title = "Vibration",
                value = String.format(Locale.US, "%.3f", telemetry.vibrationG),
                unit = "g",
                progressFraction = (telemetry.vibrationG / 2.0).toFloat(),
                accentColor = ChartVibGradientStart,
                subInfo = "Chassis Motion"
            )
        }

        // Row 4: Throttle Position Full-Width Card
        TelemetryCard(
            modifier = Modifier.fillMaxWidth(),
            title = "Throttle Position",
            value = String.format(Locale.US, "%.1f", telemetry.throttlePct),
            unit = "%",
            progressFraction = (telemetry.throttlePct / 100.0).toFloat(),
            accentColor = AccentTechBlue,
            subInfo = "Sequence Counter: #${telemetry.sequenceNumber}"
        )
    }
}
