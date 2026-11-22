# ==============================================================================
# Flash Firmware Script for NXP FRDM-RW612 Worker Safety Monitor
# Works out of the box without requiring MCUXpresso SDK or project import
# ==============================================================================

$binPath = Join-Path $PSScriptRoot "bin\FRDM_RW612_WorkerSafety.bin"

if (-not (Test-Path $binPath)) {
    Write-Host "[ERROR] Binary not found at $binPath" -ForegroundColor Red
    exit 1
}

$binSize = (Get-Item $binPath).Length
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "  FRDM-RW612 Standalone Firmware Flasher" -ForegroundColor Cyan
Write-Host "  Binary: bin\FRDM_RW612_WorkerSafety.bin ($binSize bytes)" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Check for MCU-Link Removable USB Drive
Write-Host ">> Checking for connected MCU-Link USB drive..." -ForegroundColor Yellow
$drives = [System.IO.DriveInfo]::GetDrives() | Where-Object { 
    $_.IsReady -and ($_.VolumeLabel -match "MCU-LINK|FRDM|NXP|RT_FLASH" -or $_.Name -match "MCU-LINK")
}

if ($drives) {
    foreach ($drive in $drives) {
        $dest = $drive.RootDirectory.FullName
        Write-Host ">> Found MCU-Link Drive: $dest (Volume: $($drive.VolumeLabel))" -ForegroundColor Green
        Write-Host ">> Copying binary to $dest..." -ForegroundColor Yellow
        Copy-Item -Path $binPath -Destination (Join-Path $dest "FRDM_RW612_WorkerSafety.bin") -Force
        Write-Host ">> SUCCESS! Binary copied to MCU-Link drive." -ForegroundColor Green
        Write-Host ">> The board LED is blinking and programming the RW612 flash." -ForegroundColor Green
        Write-Host ">> The board will automatically reboot with the new firmware." -ForegroundColor Green
        exit 0
    }
}

# 2. Check for pyocd in system PATH or MCUXpresso tools
Write-Host ">> MCU-Link mass storage drive not detected. Checking for pyOCD..." -ForegroundColor Yellow
$pyocdCmd = Get-Command pyocd -ErrorAction SilentlyContinue
$pyocdExe = if ($pyocdCmd) { $pyocdCmd.Source } else { $null }

if (-not $pyocdExe) {
    $candidates = @(
        "C:\Users\$env:USERNAME\.mcuxpressotools\.mcux-venv-3.12\Scripts\pyocd.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python*\Scripts\pyocd.exe"
    )
    foreach ($c in $candidates) {
        $found = Get-Item $c -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $pyocdExe = $found.FullName; break }
    }
}

if ($pyocdExe) {
    Write-Host ">> Found pyOCD: $pyocdExe" -ForegroundColor Green
    Write-Host ">> Flashing to target rw612 at address 0x08001000..." -ForegroundColor Yellow
    & $pyocdExe flash -t rw612 $binPath --base-address 0x08001000
    if ($LASTEXITCODE -eq 0) {
        Write-Host ">> SUCCESS! Board flashed via pyOCD." -ForegroundColor Green
        exit 0
    }
}

# 3. Fallback instructions if hardware is not plugged in
Write-Host ""
Write-Host "[NOTICE] Neither MCU-Link USB drive nor pyOCD probe detected." -ForegroundColor Yellow
Write-Host "Make sure the board is plugged into your PC via the MCU-Link USB port (Header J10)." -ForegroundColor White
Write-Host ""
Write-Host "Manual Flashing Options (No SDK needed):" -ForegroundColor Cyan
Write-Host "  1. Drag & Drop: In Windows File Explorer, drag 'bin\FRDM_RW612_WorkerSafety.bin'" -ForegroundColor White
Write-Host "     directly into the removable USB drive named 'MCU-LINK'." -ForegroundColor White
Write-Host "  2. SEGGER J-Flash Lite: Open J-Flash Lite -> Device: RW612ETA2I ->" -ForegroundColor White
Write-Host "     select 'bin\FRDM_RW612_WorkerSafety.bin' -> click 'Program Device'." -ForegroundColor White
Write-Host ""
exit 1
