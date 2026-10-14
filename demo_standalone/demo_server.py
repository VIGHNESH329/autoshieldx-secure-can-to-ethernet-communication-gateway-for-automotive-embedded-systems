#!/usr/bin/env python3
"""
AutoShieldX — Hardware-Free Demonstration Server & Local Telemetry Provider
===========================================================================
This standalone server runs without physical CAN hardware, microcontrollers,
MCP2515 modules, or SocketCAN can0 interfaces.

Key Capabilities:
1. Reusable AutoShieldX Operations Dashboard (HTML, CSS, JS).
2. Local Telemetry Provider generating authentic 20 Hz dynamic vehicle kinematics.
3. Deterministic Behavioral Anomaly Generator producing controlled, repeatable
   ML anomalies (vibration spikes / kinematic deviations) logged and synchronized
   with the event pipeline and dashboard counters.
4. Broadcasts telemetry and event packets over UDP port 16000 to local and Android
   companion listeners (127.0.0.1 and 10.0.2.2).
5. Provides REST API for status polling (/api/status) and counter resets (/api/reset).
"""

import sys
import os
import time
import math
import json
import random
import socket
import datetime
import threading
import http.server
import socketserver

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_DIR = os.path.join(SCRIPT_DIR, "logs")
LOG_FILE = os.path.join(LOG_DIR, "autoshieldx_demo.log")
os.makedirs(LOG_DIR, exist_ok=True)

STATE_LOCK = threading.Lock()

STATE = {
    "total_messages": 0,
    "valid_messages": 0,
    "rejected_messages": 0,
    "hmac_failures": 0,
    "ml_anomalies": 0,
    "latest_telemetry": {
        "speed_kmh": 0.0,
        "rpm": 850.0,
        "temperature_c": 25.0,
        "vibration_g": 0.080,
        "steering_deg": 0.0,
        "throttle_pct": 0.0,
        "wheel_speed_kmh": 0.0,
        "can_id": 256,
        "sequence_number": 0,
        "timestamp_us": 0,
        "hmac_verified": True,
        "gateway_device": "AutoShieldX_Demo_Provider"
    },
    "recent_events": [],
    "data_source_mode": "DEMO_HARDWARE_FREE_SIMULATION"
}

def log_event_to_disk(event_dict):
    """Persists event to demo log file in JSON Lines format."""
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(json.dumps(event_dict) + "\n")
    except Exception as e:
        pass

def broadcast_udp(packet_dict, port=16000):
    """Sends telemetry / security alert packet to localhost and Android emulator."""
    payload = json.dumps(packet_dict).encode("utf-8")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    except Exception:
        pass

    destinations = [
        ("127.0.0.1", port),
        ("10.0.2.2", port)
    ]
    for ip, p in destinations:
        try:
            sock.sendto(payload, (ip, p))
        except Exception:
            pass
    try:
        sock.close()
    except Exception:
        pass

def telemetry_and_anomaly_worker():
    """
    20 Hz Telemetry Loop & Periodic Deterministic Anomaly Generator.
    Maintains realistic vehicle kinematics and injects controlled ML anomalies every 12 seconds.
    """
    dt = 0.05  # 20 Hz (50 ms tick)
    elapsed_time = 0.0
    seq = 1

    # Kinematics state variables
    speed_kmh = 0.0
    target_speed_kmh = 68.0
    temp_c = 25.0
    rpm = 850.0
    last_anomaly_cycle = -1

    while True:
        tick_start = time.time()
        elapsed_time += dt

        # --- 1. Vehicle Acceleration & Drive Cycle Dynamics ---
        # Phase 1: Smooth acceleration 0 -> 68 km/h
        # Phase 2: Gentle highway cruising with speed adjustments (60 - 95 km/h)
        if elapsed_time > 30.0:
            target_speed_kmh = 75.0 + 15.0 * math.sin(elapsed_time * 0.08)

        speed_error = target_speed_kmh - speed_kmh
        accel_rate = 3.5  # km/h per second max
        delta_v = max(-accel_rate, min(accel_rate, speed_error * 0.4)) * dt
        speed_kmh = max(0.0, speed_kmh + delta_v)

        # Correlated throttle
        if speed_kmh < target_speed_kmh:
            throttle_pct = max(10.0, min(80.0, 25.0 + (target_speed_kmh - speed_kmh) * 1.5))
        else:
            throttle_pct = max(5.0, min(25.0, 12.0 + random.uniform(-2.0, 2.0)))

        # 5-Speed Gear Ratio RPM Simulation
        if speed_kmh < 20.0: gear_ratio = 80.0
        elif speed_kmh < 40.0: gear_ratio = 50.0
        elif speed_kmh < 65.0: gear_ratio = 36.0
        elif speed_kmh < 95.0: gear_ratio = 26.0
        else: gear_ratio = 20.0

        base_rpm = 850.0 + (speed_kmh * gear_ratio)
        rpm = max(800.0, min(6500.0, base_rpm + random.uniform(-20.0, 20.0)))

        # Engine Coolant Thermal Curve (warms from 25 C to ~90 C, then stabilizes)
        if temp_c < 88.0:
            temp_c += (0.12 * (1.0 + throttle_pct / 50.0)) * dt
        else:
            temp_c = 90.0 + 1.2 * math.sin(elapsed_time * 0.05) + random.uniform(-0.15, 0.15)

        # Steering angle sinusoid (gentle highway lane-keeping)
        steering_deg = 3.5 * math.sin(elapsed_time * 0.25) + random.uniform(-0.4, 0.4)

        # Wheel speed tracks vehicle speed with minor tire slip
        wheel_speed_kmh = max(0.0, speed_kmh + random.uniform(-0.3, 0.3))

        # Baseline Road Texture Vibration (0.05 - 0.25 G)
        vibration_g = max(0.03, min(0.35, 0.06 + (speed_kmh / 250.0) * 0.20 + random.uniform(0.0, 0.05)))

        # --- 2. Deterministic Behavioral-Anomaly Generator ---
        # Every 12 seconds, trigger a controlled transient vibration spike anomaly (3.42 G)
        # lasting exactly 200ms (4 ticks).
        cycle_idx = int(elapsed_time / 12.0)
        cycle_phase = elapsed_time - (cycle_idx * 12.0)
        is_anomaly_pulse = (cycle_phase >= 11.0 and cycle_phase < 11.25)

        if is_anomaly_pulse:
            vibration_g = 3.42  # Extreme shock signature above ML threshold (3.0 G)

            # Fire anomaly event once per cycle
            if cycle_idx > last_anomaly_cycle:
                last_anomaly_cycle = cycle_idx
                iso_time = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"
                epoch_ms = int(time.time() * 1000)

                anomaly_event = {
                    "timestamp_iso": iso_time,
                    "timestamp_epoch_ms": epoch_ms,
                    "can_id": 257,
                    "can_id_hex": "0x101",
                    "sequence_number": seq,
                    "event_type": "ML_ANOMALY_FLAGGED",
                    "message_status": "FLAGGED_MONITORED",
                    "attack_type": "NONE",
                    "severity": "WARNING",
                    "ml_result": "VIBRATION_SPIKE_ANOMALY",
                    "ml_anomaly_score": 0.72,
                    "details": f"ML Behavioral Anomaly: Transient shock spike ({vibration_g:.2f} G) exceeds baseline road envelope on SW-420 model [Score: 0.72]"
                }

                with STATE_LOCK:
                    STATE["ml_anomalies"] += 1
                    STATE["recent_events"].insert(0, anomaly_event)
                    if len(STATE["recent_events"]) > 50:
                        STATE["recent_events"].pop()

                log_event_to_disk(anomaly_event)
                broadcast_udp(anomaly_event)

        # Periodic Routine Integrity Verification Log (every 30s)
        if int(elapsed_time) > 0 and int(elapsed_time) % 30 == 0 and abs(elapsed_time - round(elapsed_time)) < dt:
            iso_time = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"
            info_event = {
                "timestamp_iso": iso_time,
                "timestamp_epoch_ms": int(time.time() * 1000),
                "can_id": 256,
                "can_id_hex": "0x100",
                "sequence_number": seq,
                "event_type": "FRAME_ACCEPTED",
                "message_status": "ACCEPTED_VALID",
                "attack_type": "NONE",
                "severity": "INFO",
                "ml_result": "NORMAL",
                "ml_anomaly_score": 0.05,
                "details": "Routine cryptographic check passed (HMAC verified, monotonic sequence counter verified)"
            }
            with STATE_LOCK:
                STATE["recent_events"].insert(0, info_event)
                if len(STATE["recent_events"]) > 50:
                    STATE["recent_events"].pop()
            log_event_to_disk(info_event)

        # --- 3. Synchronize State & Broadcast Telemetry ---
        timestamp_us = int(time.time() * 1_000_000)
        telemetry_pkt = {
            "type": "VEHICLE_TELEMETRY",
            "speed_kmh": round(speed_kmh, 1),
            "rpm": round(rpm, 0),
            "temperature_c": round(temp_c, 1),
            "vibration_g": round(vibration_g, 3),
            "steering_deg": round(steering_deg, 1),
            "throttle_pct": round(throttle_pct, 1),
            "wheel_speed_kmh": round(wheel_speed_kmh, 1),
            "can_id": 256 if (seq % 2 == 1) else 257,
            "sequence_number": seq,
            "timestamp_us": timestamp_us,
            "hmac_verified": True,
            "gateway_device": "AutoShieldX_Demo_Provider"
        }

        with STATE_LOCK:
            STATE["total_messages"] += 1
            STATE["valid_messages"] += 1
            STATE["latest_telemetry"] = telemetry_pkt

        # Broadcast telemetry over UDP 16000 every 4 ticks (5 Hz UDP rate is smooth & bandwidth friendly)
        if seq % 4 == 0:
            broadcast_udp(telemetry_pkt)

        seq += 1

        # Sleep to maintain accurate 20 Hz loop
        elapsed_tick = time.time() - tick_start
        sleep_time = max(0.001, dt - elapsed_tick)
        time.sleep(sleep_time)

class DemoHttpHandler(http.server.SimpleHTTPRequestHandler):
    """Serves the static AutoShieldX Dashboard and /api/ endpoints."""
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=SCRIPT_DIR, **kwargs)

    def do_GET(self):
        if self.path == "/api/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
            self.end_headers()
            with STATE_LOCK:
                resp = json.dumps(STATE)
            self.wfile.write(resp.encode("utf-8"))
        elif self.path in ("/api/reset", "/api/clear"):
            with STATE_LOCK:
                STATE["total_messages"] = 0
                STATE["valid_messages"] = 0
                STATE["rejected_messages"] = 0
                STATE["hmac_failures"] = 0
                STATE["ml_anomalies"] = 0
                STATE["recent_events"] = []
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(b'{"status":"ok","message":"Demo metrics reset successfully"}')
        else:
            super().do_GET()

    def do_POST(self):
        if self.path in ("/api/reset", "/api/clear"):
            with STATE_LOCK:
                STATE["total_messages"] = 0
                STATE["valid_messages"] = 0
                STATE["rejected_messages"] = 0
                STATE["hmac_failures"] = 0
                STATE["ml_anomalies"] = 0
                STATE["recent_events"] = []
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(b'{"status":"ok","message":"Demo metrics reset successfully"}')
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        # Suppress routine static resource request spam in terminal
        if "api/status" not in args[0]:
            super().log_message(format, *args)

def run_server(port=8080):
    # Start background telemetry & anomaly worker
    worker_thread = threading.Thread(target=telemetry_and_anomaly_worker, daemon=True)
    worker_thread.start()

    socketserver.TCPServer.allow_reuse_address = True
    server_address = ("", port)
    httpd = socketserver.TCPServer(server_address, DemoHttpHandler)

    print("\n" + "="*65)
    print("  AutoShieldX — Hardware-Free Standalone Demonstration Build")
    print("="*65)
    print(f"  Web Dashboard UI      : http://127.0.0.1:{port}/")
    print(f"  Status JSON API       : http://127.0.0.1:{port}/api/status")
    print(f"  Telemetry UDP Emitter : Port 16000 (127.0.0.1 & 10.0.2.2)")
    print(f"  Local Demo Log File   : {LOG_FILE}")
    print(f"  Data Mode             : Realistic Kinematics + ML Anomaly Generator")
    print("="*65)
    print("  Ready for presentation. Press Ctrl+C to terminate.\n")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[Demo Server] Graceful shutdown completed.")

if __name__ == "__main__":
    http_port = 8080
    if len(sys.argv) >= 2:
        try:
            http_port = int(sys.argv[1])
        except ValueError:
            pass
    run_server(http_port)
