package com.autoshieldx.dashboard.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.autoshieldx.dashboard.data.ConnectionStatus
import com.autoshieldx.dashboard.data.DataSourceMode
import com.autoshieldx.dashboard.data.SecurityAlert
import com.autoshieldx.dashboard.data.SecurityStats
import com.autoshieldx.dashboard.data.SecurityStatus
import com.autoshieldx.dashboard.data.SensorFallbackManager
import com.autoshieldx.dashboard.data.UdpTelemetryReceiver
import com.autoshieldx.dashboard.data.VehicleTelemetry
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL

data class DashboardUiState(
    val isIntroFinished: Boolean = false,
    val connectionStatus: ConnectionStatus = ConnectionStatus.STANDBY,
    val dataSourceMode: DataSourceMode = DataSourceMode.LOCAL_SENSOR_FALLBACK,
    val telemetry: VehicleTelemetry = VehicleTelemetry(),
    val securityStatus: SecurityStatus = SecurityStatus(),
    val securityStats: SecurityStats = SecurityStats(),
    val recentAlerts: List<SecurityAlert> = emptyList(),
    val speedHistory: List<Float> = List(25) { 0f },
    val rpmHistory: List<Float> = List(25) { 850f },
    val tempHistory: List<Float> = List(25) { 25f },
    val vibrationHistory: List<Float> = List(25) { 0.08f },
    val gatewayIp: String = "10.0.2.2",
    val listeningPort: Int = 16000,
    val lastPacketTimestamp: Long = 0L,
    val lastPacketLatencyMs: Long = 0L
)

class DashboardViewModel(application: Application) : AndroidViewModel(application) {

    private val _uiState = MutableStateFlow(DashboardUiState())
    val uiState: StateFlow<DashboardUiState> = _uiState.asStateFlow()

    private var receiver: UdpTelemetryReceiver? = null
    private var sensorFallback: SensorFallbackManager? = null
    private var watchdogJob: Job? = null
    private var receiverJob: Job? = null
    private var httpSyncJob: Job? = null

    private var lastRealPacketTimeMs: Long = 0L
    private val historyMaxSize = 25

    init {
        initSensorFallback()
        initUdpReceiver()
        startHeartbeatWatchdog()
        startHttpSync()
    }

    fun completeIntro() {
        _uiState.update { it.copy(isIntroFinished = true) }
    }

    private fun initSensorFallback() {
        sensorFallback = SensorFallbackManager(getApplication()) { sensorTelemetry ->
            // Only update telemetry from phone sensors if gateway data is unavailable
            if (_uiState.value.dataSourceMode == DataSourceMode.LOCAL_SENSOR_FALLBACK) {
                _uiState.update { current ->
                    current.copy(
                        telemetry = sensorTelemetry,
                        speedHistory = appendHistory(current.speedHistory, sensorTelemetry.speedKmh.toFloat()),
                        rpmHistory = appendHistory(current.rpmHistory, sensorTelemetry.rpm.toFloat()),
                        tempHistory = appendHistory(current.tempHistory, sensorTelemetry.temperatureC.toFloat()),
                        vibrationHistory = appendHistory(current.vibrationHistory, sensorTelemetry.vibrationG.toFloat())
                    )
                }
            }
        }
        sensorFallback?.start()
    }

    private fun initUdpReceiver() {
        receiver = UdpTelemetryReceiver(
            port = _uiState.value.listeningPort,
            onTelemetryReceived = { realTelemetry, senderIp ->
                val now = System.currentTimeMillis()
                lastRealPacketTimeMs = now

                // Seamless automatic sync back to REAL GATEWAY
                sensorFallback?.stop()

                _uiState.update { current ->
                    val newValidCount = current.securityStats.validFrames + 1
                    current.copy(
                        connectionStatus = ConnectionStatus.CONNECTED,
                        dataSourceMode = DataSourceMode.REAL_GATEWAY,
                        telemetry = realTelemetry,
                        gatewayIp = senderIp,
                        lastPacketTimestamp = now,
                        lastPacketLatencyMs = 12L,
                        securityStatus = SecurityStatus(
                            hmacStatus = if (realTelemetry.hmacVerified) "VERIFIED" else "FAILED",
                            frameValidation = "ACTIVE",
                            replayProtection = "ACTIVE",
                            mlDetection = "NORMAL"
                        ),
                        securityStats = current.securityStats.copy(validFrames = newValidCount),
                        speedHistory = appendHistory(current.speedHistory, realTelemetry.speedKmh.toFloat()),
                        rpmHistory = appendHistory(current.rpmHistory, realTelemetry.rpm.toFloat()),
                        tempHistory = appendHistory(current.tempHistory, realTelemetry.temperatureC.toFloat()),
                        vibrationHistory = appendHistory(current.vibrationHistory, realTelemetry.vibrationG.toFloat())
                    )
                }
            },
            onSecurityAlertReceived = { alert ->
                _uiState.update { current ->
                    val updatedAlerts = listOf(alert) + current.recentAlerts.take(20)
                    var stats = current.securityStats
                    var secStatus = current.securityStatus

                    if (alert.messageStatus != "ACCEPTED_VALID") {
                        stats = stats.copy(rejectedFrames = stats.rejectedFrames + 1)
                    }

                    if (alert.eventType.contains("HMAC", ignoreCase = true)) {
                        stats = stats.copy(hmacFailures = stats.hmacFailures + 1)
                        secStatus = secStatus.copy(hmacStatus = "FAILED")
                    } else if (alert.eventType.contains("REPLAY", ignoreCase = true)) {
                        stats = stats.copy(replayDetections = stats.replayDetections + 1)
                        secStatus = secStatus.copy(replayProtection = "REPLAY DETECTED")
                    } else if (alert.eventType.contains("ML", ignoreCase = true)) {
                        stats = stats.copy(mlAnomalies = stats.mlAnomalies + 1)
                        secStatus = secStatus.copy(mlDetection = "ANOMALY DETECTED")
                    } else if (alert.eventType.contains("VALIDATION", ignoreCase = true)) {
                        secStatus = secStatus.copy(frameValidation = "INVALID")
                    }

                    current.copy(
                        recentAlerts = updatedAlerts,
                        securityStats = stats,
                        securityStatus = secStatus
                    )
                }
            },
            onError = { _ ->
                // Handled gracefully without crash
            }
        )

        receiverJob?.cancel()
        receiverJob = viewModelScope.launch {
            receiver?.startListening()
        }
    }

    private fun startHeartbeatWatchdog() {
        watchdogJob?.cancel()
        watchdogJob = viewModelScope.launch {
            while (isActive) {
                delay(1000)
                val now = System.currentTimeMillis()
                // If no real gateway packet received within 3.5 seconds, switch to subtle live sensor fallback
                if (now - lastRealPacketTimeMs > 3500L) {
                    if (_uiState.value.connectionStatus == ConnectionStatus.CONNECTED) {
                        _uiState.update {
                            it.copy(
                                connectionStatus = ConnectionStatus.UNAVAILABLE,
                                dataSourceMode = DataSourceMode.LOCAL_SENSOR_FALLBACK
                            )
                        }
                        sensorFallback?.start()
                    }
                }
            }
        }
    }

    private fun startHttpSync() {
        httpSyncJob?.cancel()
        httpSyncJob = viewModelScope.launch(Dispatchers.IO) {
            val candidateHosts = listOf("10.0.2.2", _uiState.value.gatewayIp, "127.0.0.1")
            while (isActive) {
                var syncSucceeded = false
                for (host in candidateHosts) {
                    if (!isActive) break
                    try {
                        val url = URL("http://$host:8080/api/status")
                        val conn = (url.openConnection() as HttpURLConnection).apply {
                            connectTimeout = 400
                            readTimeout = 400
                            requestMethod = "GET"
                            setRequestProperty("Accept", "application/json")
                        }
                        if (conn.responseCode == 200) {
                            val reader = BufferedReader(InputStreamReader(conn.inputStream))
                            val responseText = reader.readText()
                            reader.close()
                            conn.disconnect()

                            val json = JSONObject(responseText)
                            val now = System.currentTimeMillis()
                            lastRealPacketTimeMs = now

                            withContext(Dispatchers.Main) {
                                sensorFallback?.stop()

                                val telObj = json.optJSONObject("latest_telemetry")
                                val telemetry = if (telObj != null) {
                                    VehicleTelemetry(
                                        speedKmh = telObj.optDouble("speed_kmh", 0.0),
                                        rpm = telObj.optDouble("rpm", 850.0),
                                        temperatureC = telObj.optDouble("temperature_c", 25.0),
                                        vibrationG = telObj.optDouble("vibration_g", 0.08),
                                        steeringDeg = telObj.optDouble("steering_deg", 0.0),
                                        throttlePct = telObj.optDouble("throttle_pct", 0.0),
                                        wheelSpeedKmh = telObj.optDouble("wheel_speed_kmh", 0.0),
                                        sequenceNumber = telObj.optLong("sequence_number", 0L),
                                        canId = telObj.optLong("can_id", 256L),
                                        timestampUs = telObj.optLong("timestamp_us", 0L),
                                        hmacVerified = telObj.optBoolean("hmac_verified", true),
                                        gatewayDevice = telObj.optString("gateway_device", "AutoShieldX_Gateway")
                                    )
                                } else {
                                    _uiState.value.telemetry
                                }

                                val stats = SecurityStats(
                                    validFrames = json.optLong("valid_messages", 0L),
                                    rejectedFrames = json.optLong("rejected_messages", 0L),
                                    hmacFailures = json.optLong("hmac_failures", 0L),
                                    replayDetections = json.optLong("replay_detections", 0L),
                                    mlAnomalies = json.optLong("ml_anomalies", 0L)
                                )

                                val mlCount = json.optLong("ml_anomalies", 0L)
                                val secStatus = SecurityStatus(
                                    hmacStatus = if (json.optLong("hmac_failures", 0L) > 0) "FAILED" else "VERIFIED",
                                    frameValidation = "ACTIVE",
                                    replayProtection = "ACTIVE",
                                    mlDetection = if (mlCount > 0) "ANOMALY DETECTED" else "NORMAL"
                                )

                                val eventsArray = json.optJSONArray("recent_events")
                                val alertsList = mutableListOf<SecurityAlert>()
                                if (eventsArray != null) {
                                    for (i in 0 until minOf(20, eventsArray.length())) {
                                        val ev = eventsArray.optJSONObject(i) ?: continue
                                        alertsList.add(
                                            SecurityAlert(
                                                id = "${ev.optString("timestamp_iso", "")}_${ev.optLong("can_id", 0L)}_$i",
                                                timestampIso = ev.optString("timestamp_iso", ""),
                                                canId = ev.optLong("can_id", 0L),
                                                eventType = ev.optString("event_type", ""),
                                                messageStatus = ev.optString("message_status", ""),
                                                attackType = ev.optString("attack_type", "NONE"),
                                                severity = ev.optString("severity", "INFO"),
                                                details = ev.optString("details", "")
                                            )
                                        )
                                    }
                                }

                                _uiState.update { current ->
                                    current.copy(
                                        connectionStatus = ConnectionStatus.CONNECTED,
                                        dataSourceMode = DataSourceMode.REAL_GATEWAY,
                                        telemetry = telemetry,
                                        securityStats = stats,
                                        securityStatus = secStatus,
                                        recentAlerts = if (alertsList.isNotEmpty()) alertsList else current.recentAlerts,
                                        gatewayIp = host,
                                        lastPacketTimestamp = now,
                                        lastPacketLatencyMs = 8L,
                                        speedHistory = appendHistory(current.speedHistory, telemetry.speedKmh.toFloat()),
                                        rpmHistory = appendHistory(current.rpmHistory, telemetry.rpm.toFloat()),
                                        tempHistory = appendHistory(current.tempHistory, telemetry.temperatureC.toFloat()),
                                        vibrationHistory = appendHistory(current.vibrationHistory, telemetry.vibrationG.toFloat())
                                    )
                                }
                            }
                            syncSucceeded = true
                            break
                        }
                    } catch (e: Exception) {
                        // Candidate host unreachable, try next candidate
                    }
                }
                delay(if (syncSucceeded) 500L else 1500L)
            }
        }
    }

    private fun appendHistory(currentList: List<Float>, newValue: Float): List<Float> {
        val mutable = currentList.toMutableList()
        if (mutable.size >= historyMaxSize) {
            mutable.removeAt(0)
        }
        mutable.add(newValue)
        return mutable
    }

    fun updateListeningPort(newPort: Int) {
        if (newPort in 1024..65535 && newPort != _uiState.value.listeningPort) {
            _uiState.update { it.copy(listeningPort = newPort) }
            receiver?.updatePort(newPort)
            receiverJob?.cancel()
            receiverJob = viewModelScope.launch {
                receiver?.startListening()
            }
        }
    }

    override fun onCleared() {
        super.onCleared()
        watchdogJob?.cancel()
        receiverJob?.cancel()
        httpSyncJob?.cancel()
        receiver?.stop()
        sensorFallback?.stop()
    }
}
