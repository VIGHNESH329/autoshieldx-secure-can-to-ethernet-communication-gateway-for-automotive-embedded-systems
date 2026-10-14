#!/usr/bin/env python3
"""
AutoShieldX - Functional Monitoring Dashboard Server & Telemetry Collector
Listens to:
1. Validated vehicle telemetry forwarded over Ethernet (UDP 16000) by the C++ Secure Gateway.
2. Structured security logs generated in logs/autoshieldx_security.log.
3. Optional AWS CloudWatch Logs forwarding to region us-east-1, group /autoshieldx/telemetry.

Serves a technical live operations dashboard on http://127.0.0.1:8080.
Requires only standard Python 3. Gracefully uses boto3 if available for CloudWatch.
"""

import http.server
import socketserver
import threading
import socket
import json
import os
import sys
import time
import queue

STATE = {
    "total_messages": 0,
    "valid_messages": 0,
    "rejected_messages": 0,
    "hmac_failures": 0,
    "replay_detections": 0,
    "validation_failures": 0,
    "ml_anomalies": 0,
    "latest_telemetry": {
        "speed_kmh": 0.0,
        "rpm": 850.0,
        "temperature_c": 25.0,
        "vibration_g": 0.08,
        "steering_deg": 0.0,
        "throttle_pct": 0.0,
        "wheel_speed_kmh": 0.0,
        "sequence_number": 0,
        "can_id": 256,
        "timestamp_us": 0
    },
    "recent_events": []
}

STATE_LOCK = threading.Lock()

DASHBOARD_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(DASHBOARD_DIR)

SEEN_EVENTS = set()
CLOUDWATCH_QUEUE = queue.Queue(maxsize=5000)

# Optional boto3 CloudWatch client
HAVE_BOTO3 = False
try:
    import boto3
    HAVE_BOTO3 = True
except ImportError:
    HAVE_BOTO3 = False

def load_aws_config():
    """Load AWS CloudWatch configuration from config/aws_config.json."""
    candidates = [
        os.path.join(PROJECT_ROOT, "config", "aws_config.json"),
        os.path.join(os.getcwd(), "config", "aws_config.json"),
        os.path.join(PROJECT_ROOT, "aws_config.json")
    ]
    for p in candidates:
        if os.path.exists(p):
            try:
                with open(p, "r", encoding="utf-8") as f:
                    cfg = json.load(f)
                    return cfg.get("aws_cloud_logging", {})
            except Exception:
                pass
    return {
        "region": "us-east-1",
        "cloudwatch_log_group": "/autoshieldx/telemetry",
        "mock_mode": True,
        "mock_output_file": "logs/aws_mock_telemetry_batch.json"
    }

def process_security_event_locked(entry):
    """Processes a real gateway security event and updates dashboard counters & event table."""
    # Deduplicate events received across UDP stream and log file
    event_key = (
        entry.get("timestamp_epoch_ms"),
        entry.get("can_id"),
        entry.get("sequence_number"),
        entry.get("event_type"),
        entry.get("message_status")
    )
    if event_key in SEEN_EVENTS:
        return
    SEEN_EVENTS.add(event_key)
    if len(SEEN_EVENTS) > 10000:
        SEEN_EVENTS.clear()

    STATE["total_messages"] += 1
    status = entry.get("message_status", "")
    evt = entry.get("event_type", "")

    if status in ("ACCEPTED_VALID", "FLAGGED_MONITORED"):
        STATE["valid_messages"] += 1
    else:
        STATE["rejected_messages"] += 1

    if "HMAC" in evt:
        STATE["hmac_failures"] += 1
    elif "REPLAY" in evt:
        STATE["replay_detections"] += 1
    elif "VALIDATION" in evt:
        STATE["validation_failures"] += 1
    elif "ML" in evt:
        STATE["ml_anomalies"] += 1

    STATE["recent_events"].insert(0, entry)
    if len(STATE["recent_events"]) > 50:
        STATE["recent_events"].pop()

    try:
        CLOUDWATCH_QUEUE.put_nowait({
            "type": "SECURITY_EVENT",
            "data": entry,
            "timestamp_ms": entry.get("timestamp_epoch_ms", int(time.time() * 1000))
        })
    except queue.Full:
        pass

def udp_ethernet_listener(port=16000):
    """Listens for validated telemetry packets and live security events forwarded across Ethernet."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Enable address reuse
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    except Exception:
        pass

    try:
        sock.bind(("0.0.0.0", port))
        print(f"[Telemetry Collector] Bound successfully to UDP 0.0.0.0:{port}")
        print(f"[Telemetry Collector] Ready for live telemetry from Gateway (Raspberry Pi can0 -> UDP {port})")
    except Exception as e:
        print(f"[Telemetry Collector] Warning: Could not bind UDP port {port}: {e}")
        return

    while True:
        try:
            data, addr = sock.recvfrom(4096)
            text = data.decode("utf-8", errors="ignore")
            packet = json.loads(text)
            
            with STATE_LOCK:
                if packet.get("type") == "VEHICLE_TELEMETRY":
                    # Update live telemetry gauge state
                    STATE["latest_telemetry"]["speed_kmh"] = packet.get("speed_kmh", 0.0)
                    STATE["latest_telemetry"]["rpm"] = packet.get("rpm", 0.0)
                    STATE["latest_telemetry"]["temperature_c"] = packet.get("temperature_c", 0.0)
                    STATE["latest_telemetry"]["vibration_g"] = packet.get("vibration_g", 0.0)
                    STATE["latest_telemetry"]["steering_deg"] = packet.get("steering_deg", 0.0)
                    STATE["latest_telemetry"]["throttle_pct"] = packet.get("throttle_pct", 0.0)
                    STATE["latest_telemetry"]["wheel_speed_kmh"] = packet.get("wheel_speed_kmh", 0.0)
                    STATE["latest_telemetry"]["sequence_number"] = packet.get("sequence_number", 0)
                    STATE["latest_telemetry"]["can_id"] = packet.get("can_id", 0)
                    STATE["latest_telemetry"]["timestamp_us"] = packet.get("timestamp_us", 0)

                    # Update message counters
                    STATE["total_messages"] += 1
                    if packet.get("hmac_verified", True):
                        STATE["valid_messages"] += 1
                    else:
                        STATE["rejected_messages"] += 1
                        STATE["hmac_failures"] += 1

                    try:
                        CLOUDWATCH_QUEUE.put_nowait({
                            "type": "VEHICLE_TELEMETRY",
                            "data": packet,
                            "timestamp_ms": int(time.time() * 1000)
                        })
                    except queue.Full:
                        pass

                elif "event_type" in packet or "message_status" in packet:
                    process_security_event_locked(packet)
        except Exception:
            pass

def log_file_tailer():
    """Monitors local security log file for real-time security events and attack metrics."""
    last_pos = 0
    current_path = None
    is_initial_open = True

    while True:
        candidates = [
            os.path.join(PROJECT_ROOT, "logs", "autoshieldx_security.log"),
            os.path.join(PROJECT_ROOT, "build", "logs", "autoshieldx_security.log"),
            os.path.join(PROJECT_ROOT, "build", "Release", "logs", "autoshieldx_security.log"),
            os.path.join(os.getcwd(), "logs", "autoshieldx_security.log"),
            os.path.join(os.getcwd(), "build", "logs", "autoshieldx_security.log"),
        ]

        target_path = None
        for p in candidates:
            if os.path.exists(p) and os.path.getsize(p) > 0:
                target_path = p
                break

        if target_path:
            if target_path != current_path:
                current_path = target_path
                try:
                    # On first attachment to an existing file, seek to end so historical logs aren't re-simulated
                    with open(current_path, "r", encoding="utf-8", errors="ignore") as f:
                        if is_initial_open:
                            f.seek(0, os.SEEK_END)
                            last_pos = f.tell()
                            is_initial_open = False
                        else:
                            last_pos = 0
                except Exception:
                    last_pos = 0

            try:
                with open(current_path, "r", encoding="utf-8", errors="ignore") as f:
                    f.seek(last_pos)
                    lines = f.readlines()
                    last_pos = f.tell()

                    if lines:
                        with STATE_LOCK:
                            for line in lines:
                                line = line.strip()
                                if not line:
                                    continue
                                try:
                                    entry = json.loads(line)
                                    process_security_event_locked(entry)
                                except Exception:
                                    pass
            except Exception:
                pass
        time.sleep(0.5)

def cloudwatch_exporter_worker():
    """Batches telemetry and security events to AWS CloudWatch Logs (/autoshieldx/telemetry in us-east-1)."""
    aws_cfg = load_aws_config()
    region = aws_cfg.get("region", "us-east-1")
    log_group = aws_cfg.get("cloudwatch_log_group", "/autoshieldx/telemetry")
    mock_mode = aws_cfg.get("mock_mode", True)
    mock_file = os.path.join(PROJECT_ROOT, aws_cfg.get("mock_output_file", "logs/aws_mock_telemetry_batch.json"))

    cw_client = None
    stream_name = f"collector_stream_{int(time.time())}"

    if HAVE_BOTO3 and not mock_mode:
        try:
            cw_client = boto3.client("logs", region_name=region)
            # Ensure log stream exists
            try:
                cw_client.create_log_stream(logGroupName=log_group, logStreamName=stream_name)
                print(f"[AWS CloudWatch] Connected: region={region}, group={log_group}, stream={stream_name}")
            except Exception:
                pass
        except Exception as e:
            print(f"[AWS CloudWatch] Notice: Boto3 initialization note ({e}). Using local mock logging.")
            cw_client = None

    batch = []
    last_flush = time.time()

    while True:
        try:
            item = CLOUDWATCH_QUEUE.get(timeout=1.0)
            batch.append(item)
        except queue.Empty:
            pass

        now = time.time()
        if (len(batch) >= 25) or (batch and now - last_flush >= 10.0):
            # Flush batch
            if cw_client:
                try:
                    events = []
                    for b in batch:
                        events.append({
                            "timestamp": b.get("timestamp_ms", int(now * 1000)),
                            "message": json.dumps(b.get("data", {}))
                        })
                    cw_client.put_log_events(
                        logGroupName=log_group,
                        logStreamName=stream_name,
                        logEvents=events
                    )
                except Exception as e:
                    pass
            else:
                # Mock Mode serialization to disk in exact CloudWatch Logs JSON format
                try:
                    os.makedirs(os.path.dirname(mock_file), exist_ok=True)
                    record = {
                        "aws_destination": f"cloudwatch://{region}{log_group}",
                        "batch_timestamp_ms": int(now * 1000),
                        "records_count": len(batch),
                        "records": [b.get("data", {}) for b in batch]
                    }
                    with open(mock_file, "a", encoding="utf-8") as f:
                        f.write(json.dumps(record) + "\n")
                except Exception:
                    pass

            batch.clear()
            last_flush = now

class DashboardHttpHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DASHBOARD_DIR, **kwargs)

    def do_GET(self):
        if self.path == "/api/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
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
                STATE["replay_detections"] = 0
                STATE["validation_failures"] = 0
                STATE["ml_anomalies"] = 0
                STATE["recent_events"] = []
                SEEN_EVENTS.clear()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(b'{"status":"ok","message":"Dashboard reset successfully"}')
        else:
            super().do_GET()

    def do_POST(self):
        if self.path in ("/api/reset", "/api/clear"):
            with STATE_LOCK:
                STATE["total_messages"] = 0
                STATE["valid_messages"] = 0
                STATE["rejected_messages"] = 0
                STATE["hmac_failures"] = 0
                STATE["replay_detections"] = 0
                STATE["validation_failures"] = 0
                STATE["ml_anomalies"] = 0
                STATE["recent_events"] = []
                SEEN_EVENTS.clear()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(b'{"status":"ok","message":"Dashboard reset successfully"}')
        else:
            self.send_response(404)
            self.end_headers()

def run_server(http_port=8080, udp_port=16000):
    # Check environment variable overrides
    env_udp = os.getenv("AUTOSHIELDX_UDP_PORT") or os.getenv("ETHERNET_PORT")
    if env_udp:
        try: udp_port = int(env_udp)
        except ValueError: pass

    # Start background listener threads
    t_udp = threading.Thread(target=udp_ethernet_listener, args=(udp_port,), daemon=True)
    t_udp.start()

    t_log = threading.Thread(target=log_file_tailer, daemon=True)
    t_log.start()

    t_cw = threading.Thread(target=cloudwatch_exporter_worker, daemon=True)
    t_cw.start()

    socketserver.TCPServer.allow_reuse_address = True
    server_address = ("", http_port)
    httpd = socketserver.TCPServer(server_address, DashboardHttpHandler)

    aws_cfg = load_aws_config()
    print(f"\n============================================================")
    print(f"  AutoShieldX Security Monitoring Dashboard & Collector      ")
    print(f"============================================================")
    print(f"  Web Dashboard UI      : http://127.0.0.1:{http_port}/")
    print(f"  Telemetry UDP Listener: Port {udp_port} (0.0.0.0:{udp_port})")
    print(f"  AWS CloudWatch Target : region {aws_cfg.get('region', 'us-east-1')} | log group {aws_cfg.get('cloudwatch_log_group', '/autoshieldx/telemetry')}")
    print(f"============================================================\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[Dashboard Server] Shutting down.")

if __name__ == "__main__":
    http_p = 8080
    udp_p = 16000
    if len(sys.argv) >= 2:
        try: http_p = int(sys.argv[1])
        except ValueError: pass
    if len(sys.argv) >= 3:
        try: udp_p = int(sys.argv[2])
        except ValueError: pass
    run_server(http_p, udp_p)
