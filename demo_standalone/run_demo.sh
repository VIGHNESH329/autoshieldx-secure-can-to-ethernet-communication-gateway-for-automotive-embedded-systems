#!/usr/bin/env bash
# AutoShieldX - Standalone Hardware-Free Demonstration Launcher
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
cd "$DIR"

echo "==================================================================="
echo "  AutoShieldX Hardware-Free Standalone Demonstration"
echo "==================================================================="
echo "Starting demonstration server at http://127.0.0.1:8080 ..."
echo "Live 20 Hz vehicle telemetry & automatic ML behavioral anomaly events active."
echo

python3 demo_server.py 8080
