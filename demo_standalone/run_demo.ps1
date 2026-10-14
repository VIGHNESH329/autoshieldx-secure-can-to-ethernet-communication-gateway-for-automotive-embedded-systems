# AutoShieldX - Standalone Hardware-Free Demonstration Launcher
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "  AutoShieldX Hardware-Free Standalone Demonstration" -ForegroundColor Cyan
Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "Starting demonstration server at http://127.0.0.1:8080 ..." -ForegroundColor Green
Write-Host "Live 20 Hz vehicle telemetry & automatic ML behavioral anomaly events active." -ForegroundColor Yellow

python demo_server.py 8080
