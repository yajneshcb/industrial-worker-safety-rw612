Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  NXP FRDM-RW612 Worker Safety Monitor - USB Serial Bridge Launcher" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location "$scriptDir"

python serial_bridge.py $args
