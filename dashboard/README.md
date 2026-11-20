# FRDM-RW612 Industrial Worker Safety Dashboard

A lightweight, responsive monitoring dashboard and Flask REST service for an NXP FRDM-RW612 edge-processing worker-safety node. The node remains responsible for local classification and physical LED/buzzer control; this app mirrors incoming telemetry and is not a safety controller.

## Requirements and quick start

Python 3.10 or newer is recommended. From the extracted project directory (named `dashboard` if desired), install dependencies and launch the local server:

```bash
python -m venv .venv
# Linux / macOS
source .venv/bin/activate
# Windows PowerShell: .venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python dashboard_app.py
```

Open <http://127.0.0.1:3000>. Set `PORT` to use another port. The app binds to `0.0.0.0` so a board or bridge on the same reachable network can POST telemetry. No database, build step, node package installation, or external chart runtime is required. Visual alerts work without browser audio; click **Audio muted · Enable** once to permit browser-generated warning and emergency sounds.

For a production-style container, build and run the included Dockerfile. It uses Gunicorn with a single worker process so the in-memory telemetry snapshot remains consistent:

```bash
docker build -t rw612-safety-dashboard .
docker run --rm -p 3000:3000 -e PORT=3000 rw612-safety-dashboard
```

The Flask development server used by `python dashboard_app.py` is intended for local bench use. Put a production deployment behind TLS and an appropriate network access policy.

## Dashboard contents

The interface shows the NORMAL / WARNING / EMERGENCY state, live clock and connection health; temperature with 40 °C and 50 °C markers and min/max readings; motion magnitude, tri-axial vectors and fall counter; gas concentration with 300 / 600 PPM zones; a visual twin for green/yellow/red outputs and the buzzer; a timestamped/filterable incident log with CSV and JSON exports; and six bench simulation actions.

The in-memory event log retains up to 500 events and the trend buffer up to 120 samples. Both reset when the process restarts. The UI polls `GET /api/status` once per second.

## REST API

### `GET /api/status`

Returns current state, actuator mirror, node/link metadata, the latest bounded `incidents` array and trend `history`. Core names include `temperature_c`, `accel_magnitude_g`, `accel_x`, `accel_y`, `accel_z`, `gas_ppm`, `sos_active`, `safety_state`, `hazard_type`, `led_normal`, `led_warning`, `led_emergency`, and `buzzer_active`.

### `POST /api/telemetry`

Send JSON from the MCU bridge. The compact payload from the project brief is accepted:

```bash
curl -X POST http://127.0.0.1:3000/api/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"id":1042,"temp":28.45,"accel":1.02,"accel_x":0.02,"accel_y":0.03,"accel_z":1.01,"gas":48.0,"state":"NORMAL","hazard":"NONE","sos":false}'
```

Descriptive aliases `temperature_c`, `accel_magnitude_g`, `gas_ppm`, `safety_state`, `hazard_type`, and `sos_active` are also accepted. Optional values include `sensor_fault` (or `sensors_ok`), `timestamp_ms`, `link_state`, `link_source`, `buffered`, and `offline_queue_count`. Valid node-reported severity is retained, but a lower-reported state cannot mask a locally detectable higher-severity temperature/gas reading. The server also recognizes `<0.4 g` followed within `1.0 s` by `>2.5 g` and holds a fall emergency for five seconds.

### `POST /api/simulate`

For bench or academic demonstration only, send one of these action names:

```bash
curl -X POST http://127.0.0.1:3000/api/simulate \
  -H 'Content-Type: application/json' \
  -d '{"action":"fall"}'
```

Actions: `sos`, `fall`, `high_temp`, `gas_leak`, `sensor_fault`, and `normal`. The interface exposes the same controls. `fall` injects a free-fall sample and then a 2.85 g impact. `high_temp` sets 48.5 °C (warning); `gas_leak` sets 550 PPM (warning); `normal` restores the safe baseline and clears the fall latch.

## Optional USB Serial VCOM ingestion

The Python service can scan for an MCU-Link / NXP serial port and consume lines containing `[TELEMETRY]` followed by a JSON object. Enable it with `SERIAL_INGEST=1` before starting the app; serial uses 115200 baud, 8 data bits, no parity, 1 stop bit. Example board output:

```text
[TELEMETRY] {"id":1042,"temp":28.45,"accel":1.02,"gas":48.0,"state":"NORMAL","hazard":"NONE","sos":false}
```

Linux hosts may need suitable `/dev/ttyACM*` or `/dev/ttyUSB*` permissions; Windows/macOS require the MCU-Link VCOM driver/device. If no matching port exists, the dashboard continues to serve HTTP and the background thread retries. Serial ingestion is opt-in and needs `pyserial` (included in `requirements.txt`).

## Thresholds and limitations

- Temperature: advisory warning at **≥40.0 °C**, emergency at **≥50.0 °C**.
- Gas/smoke: advisory warning at **≥300 PPM**, emergency at **≥600 PPM**.
- Fall: free-fall **<0.4 g**, then impact **>2.5 g** within **1.0 second**; emergency latch **5 seconds**.
- Manual SOS: highest-priority emergency override.
- Normal actuator mirror: green solid, yellow/red off, buzzer off. Environmental warning: yellow solid and periodic warning beep. Sensor fault: yellow 1 Hz pulse. Emergency: red 100 ms strobe and continuous rapid alarm.

State, simulation data, trend history, and incidents are held in process memory; this example does not provide authentication, durable persistence, multi-worker shared state, or server-side audit guarantees. **Do not expose the demo simulation endpoint to an untrusted network or use this dashboard as the only safety mechanism.** Keep actual hazard logic and actuators on the FRDM-RW612 node, and add TLS, authentication, authorization, durable storage, operational monitoring, and security review before a real deployment.
