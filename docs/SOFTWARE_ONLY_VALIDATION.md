# Software-only validation without the RW612 SDK

This checks the existing dashboard and its API. It does not compile firmware, emulate sensors, or prove board wiring or Wi-Fi.

## Start the dashboard

Requirements: Python 3.10 or newer. No MCUXpresso SDK, Arm compiler, Node.js, or board is needed.

From the repository root in PowerShell:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
py -3 -m pip install -r dashboard\requirements.txt
$env:PORT = "5000"
py -3 dashboard\dashboard_app.py
```

Open `http://127.0.0.1:5000`. Use the dashboard's demo controls to exercise normal, warning, emergency, fall, SOS, and sensor-fault display states. The demo controls inject software samples; they do not represent readings from a physical sensor.

## Check the telemetry API

In a second PowerShell terminal, send one sample:

```powershell
$payload = @{
  id = 1
  temp = 28.5
  accel = 1.0
  accel_x = 0.0
  accel_y = 0.0
  accel_z = 1.0
  gas = 45
  state = "NORMAL"
  hazard = "NONE"
  sos = $false
  sensors_ok = $true
  link_state = "Connected"
  link_source = "Software validation"
} | ConvertTo-Json

Invoke-RestMethod -Method Post `
  -Uri http://127.0.0.1:5000/api/telemetry `
  -ContentType "application/json" `
  -Body $payload

Invoke-RestMethod http://127.0.0.1:5000/api/status
```

The status response should show the submitted readings and `has_telemetry: true`; the browser should reflect the new state on its next poll. Repeat with `temp = 52` and `state = "NORMAL"` to check that the dashboard promotes a critical temperature to emergency. Post `{"action":"normal"}` to `/api/simulate` or use the Clear/Reset demo action to restore the baseline.

## What this does and does not establish

This validates the Python dashboard process, browser rendering, and HTTP telemetry contract. It does not validate the C firmware build, serial bridge against real MCU-Link output, RW612 pin mux, ADC channel, sensors, LEDs/buzzer, firmware Wi-Fi, or the Vercel deployment. Firmware compilation requires the MCUXpresso SDK and Arm GNU toolchain. Hardware acceptance checks are listed in `REPAIR_PLAN.md` and `BRING_UP.md`.
