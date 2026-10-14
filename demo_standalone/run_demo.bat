@echo off
setlocal
cd /d "%~dp0"
title AutoShieldX - Standalone Hardware-Free Demonstration
echo ===================================================================
echo   AutoShieldX Hardware-Free Standalone Demonstration
echo ===================================================================
echo Starting demo server on http://127.0.0.1:8080 ...
echo Telemetry and ML anomaly events will update automatically.
echo.
python demo_server.py 8080
pause
