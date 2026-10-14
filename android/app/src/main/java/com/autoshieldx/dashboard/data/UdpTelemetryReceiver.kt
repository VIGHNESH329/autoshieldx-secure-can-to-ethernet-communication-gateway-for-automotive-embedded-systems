package com.autoshieldx.dashboard.data

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress
import java.net.SocketException
import kotlin.coroutines.coroutineContext

class UdpTelemetryReceiver(
    private var port: Int = 16000,
    private val onTelemetryReceived: (VehicleTelemetry, String) -> Unit,
    private val onSecurityAlertReceived: (SecurityAlert) -> Unit,
    private val onError: (String) -> Unit
) {
    private val tag = "AutoShieldX_UDP"
    private var socket: DatagramSocket? = null
    @Volatile
    private var isRunning = false

    suspend fun startListening() = withContext(Dispatchers.IO) {
        if (isRunning) return@withContext
        isRunning = true

        val buffer = ByteArray(4096)
        try {
            socket = DatagramSocket(null).apply {
                reuseAddress = true
                broadcast = true
                soTimeout = 1000 // 1-second timeout to check coroutine active state
                bind(InetSocketAddress("0.0.0.0", port))
            }
            Log.i(tag, "UDP receiver bound on 0.0.0.0:$port (Broadcast & Unicast enabled)")
        } catch (e: Exception) {
            Log.e(tag, "Failed to bind UDP port $port: ${e.message}")
            onError("Cannot bind UDP port $port: ${e.message}")
            isRunning = false
            return@withContext
        }

        val packet = DatagramPacket(buffer, buffer.size)

        while (coroutineContext.isActive && isRunning) {
            try {
                socket?.receive(packet)
                val rawJson = String(packet.data, packet.offset, packet.length, Charsets.UTF_8).trim()
                val senderIp = packet.address?.hostAddress ?: "Unknown"

                if (rawJson.isNotEmpty()) {
                    parsePacket(rawJson, senderIp)
                }
            } catch (e: java.net.SocketTimeoutException) {
                // Expected timeout loop to allow clean shutdown/cancellation
            } catch (e: SocketException) {
                if (isRunning) {
                    Log.w(tag, "Socket exception: ${e.message}")
                }
                break
            } catch (e: Exception) {
                Log.e(tag, "Error processing packet: ${e.message}")
            }
        }

        close()
    }

    private fun parsePacket(jsonStr: String, senderIp: String) {
        try {
            val json = JSONObject(jsonStr)
            val type = json.optString("type", "")

            if (type == "VEHICLE_TELEMETRY" || json.has("speed_kmh") || json.has("rpm")) {
                val telemetry = VehicleTelemetry(
                    speedKmh = json.optDouble("speed_kmh", 0.0),
                    rpm = json.optDouble("rpm", 850.0),
                    temperatureC = json.optDouble("temperature_c", 25.0),
                    vibrationG = json.optDouble("vibration_g", 0.08),
                    steeringDeg = json.optDouble("steering_deg", 0.0),
                    throttlePct = json.optDouble("throttle_pct", 0.0),
                    wheelSpeedKmh = json.optDouble("wheel_speed_kmh", 0.0),
                    sequenceNumber = json.optLong("sequence_number", 0L),
                    canId = json.optLong("can_id", 256L),
                    timestampUs = json.optLong("timestamp_us", 0L),
                    hmacVerified = json.optBoolean("hmac_verified", true),
                    gatewayDevice = json.optString("gateway_device", "RaspberryPi4_SecureGateway")
                )
                onTelemetryReceived(telemetry, senderIp)
            } else if (json.has("event_type") || json.has("message_status")) {
                val alert = SecurityAlert(
                    timestampIso = json.optString("timestamp_iso", ""),
                    canId = json.optLong("can_id", 0L),
                    eventType = json.optString("event_type", ""),
                    messageStatus = json.optString("message_status", ""),
                    attackType = json.optString("attack_type", "NONE"),
                    severity = json.optString("severity", "INFO"),
                    details = json.optString("details", "")
                )
                onSecurityAlertReceived(alert)
            }
        } catch (e: Exception) {
            Log.w(tag, "Malformed JSON packet: ${e.message}")
        }
    }

    fun stop() {
        isRunning = false
        close()
    }

    private fun close() {
        try {
            socket?.close()
        } catch (e: Exception) {
            // Ignore close exceptions
        } finally {
            socket = null
        }
    }

    fun updatePort(newPort: Int) {
        if (port != newPort) {
            port = newPort
            stop()
        }
    }
}
