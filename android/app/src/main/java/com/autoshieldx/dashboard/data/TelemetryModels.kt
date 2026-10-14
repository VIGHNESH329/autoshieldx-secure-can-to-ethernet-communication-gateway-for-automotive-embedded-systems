package com.autoshieldx.dashboard.data

/**
 * Vehicle telemetry packet structure matching AutoShieldX C++ gateway protocol.
 */
data class VehicleTelemetry(
    val speedKmh: Double = 0.0,
    val rpm: Double = 850.0,
    val temperatureC: Double = 25.0,
    val vibrationG: Double = 0.08,
    val steeringDeg: Double = 0.0,
    val throttlePct: Double = 0.0,
    val wheelSpeedKmh: Double = 0.0,
    val sequenceNumber: Long = 0L,
    val canId: Long = 256L,
    val timestampUs: Long = 0L,
    val hmacVerified: Boolean = true,
    val gatewayDevice: String = "RaspberryPi4_SecureGateway"
)

/**
 * 4-Stage defense-in-depth pipeline state indicators.
 */
data class SecurityStatus(
    val hmacStatus: String = "VERIFIED",          // VERIFIED | FAILED
    val frameValidation: String = "ACTIVE",       // ACTIVE | INVALID
    val replayProtection: String = "ACTIVE",      // ACTIVE | REPLAY DETECTED
    val mlDetection: String = "NORMAL"            // NORMAL | ANOMALY DETECTED
)

/**
 * Live security arbitration counters from the gateway.
 */
data class SecurityStats(
    val validFrames: Long = 0L,
    val rejectedFrames: Long = 0L,
    val hmacFailures: Long = 0L,
    val replayDetections: Long = 0L,
    val mlAnomalies: Long = 0L
)

/**
 * Structured forensic security alert item.
 */
data class SecurityAlert(
    val id: String = java.util.UUID.randomUUID().toString(),
    val timestampIso: String = "",
    val canId: Long = 0L,
    val eventType: String = "",
    val messageStatus: String = "",
    val attackType: String = "NONE",
    val severity: String = "INFO",
    val details: String = ""
)

enum class ConnectionStatus {
    CONNECTED,
    STANDBY,
    UNAVAILABLE
}

enum class DataSourceMode {
    REAL_GATEWAY,
    LOCAL_SENSOR_FALLBACK
}
