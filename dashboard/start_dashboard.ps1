$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$baseUrl = 'http://127.0.0.1:5000'
$statusUrl = "$baseUrl/api/status"
$python = Join-Path $repoRoot '.venv\Scripts\python.exe'
$status = $null

try {
    $status = Invoke-RestMethod -Uri $statusUrl -TimeoutSec 2
} catch {
    # No dashboard is listening yet; start one below.
}

if ($null -ne $status -and ($status.serial_status -like 'Disabled*' -or $status.serial_status -like 'Unavailable*')) {
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -like '*dashboard_app.py*' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 500
    $status = $null
}

if ($null -eq $status) {
    if (-not (Test-Path -LiteralPath $python)) {
        throw "Dashboard Python environment not found at $python. Create .venv and install dashboard\requirements.txt first."
    }

    $env:SERIAL_INGEST = '1'
    $env:HOST = '127.0.0.1'
    $env:PORT = '5000'
    $stdoutLog = Join-Path $env:TEMP 'rw612-dashboard.log'
    $stderrLog = Join-Path $env:TEMP 'rw612-dashboard-error.log'

    Start-Process -FilePath $python `
        -ArgumentList @('dashboard\dashboard_app.py') `
        -WorkingDirectory $repoRoot `
        -WindowStyle Hidden `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog | Out-Null

    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        Start-Sleep -Milliseconds 500
        try {
            $status = Invoke-RestMethod -Uri $statusUrl -TimeoutSec 2
            break
        } catch {
            $status = $null
        }
    }
}

if ($null -eq $status) {
    throw "Dashboard did not start. Check $env:TEMP\rw612-dashboard-error.log"
}

if ($status.serial_status -like 'Disabled*' -or $status.serial_status -like 'Unavailable*') {
    throw "Dashboard is running without USB serial ingestion ($($status.serial_status)). Stop the old dashboard process, then reopen this workspace."
}

Start-Process $baseUrl
