"""Telemetry Bridge for NXP FRDM-RW612 Worker Safety Monitor.

Reads structured `[TELEMETRY] {json}` lines emitted over the USB MCU-Link Virtual
COM port (or optional J-Link RTT) and dispatches them asynchronously to the local
and cloud dashboards.

Usage:
  python serial_bridge.py                       # Auto-detects MCU-Link COM port @ 115200
  python serial_bridge.py COM3 --baud 115200    # Specify COM port
  python serial_bridge.py --raw                 # Print every received console line
  python serial_bridge.py --rtt                 # Use J-Link SWD RTT instead of serial
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from datetime import datetime
from typing import Any

DEFAULT_CLOUD_URL = os.getenv("DASHBOARD_URL", "https://worker-safety-dashboard.vercel.app/api/telemetry")
DEFAULT_LOCAL_URL = os.getenv("LOCAL_URL", "http://localhost:5000/api/telemetry")
DEFAULT_BAUD = int(os.getenv("BAUD_RATE", "115200"))
TELEMETRY_SECRET = os.getenv("TELEMETRY_SECRET", "")

LINK_TIMEOUT_S = 5.0        # No telemetry for 5.0s -> signal OFFLINE to dashboard
NO_DATA_HINT_S = 8.0        # No bytes for 8s -> print wiring & firmware diagnostic hint

_last_error: dict[str, str] = {}
_dispatch_queue: queue.Queue[dict[str, Any] | None] = queue.Queue(maxsize=1)


def enqueue_latest(payload: dict[str, Any]) -> None:
    """Keep the newest state while the dashboard endpoint is unavailable."""
    try:
        _dispatch_queue.put_nowait(payload)
    except queue.Full:
        try:
            _dispatch_queue.get_nowait()
            _dispatch_queue.task_done()
        except queue.Empty:
            pass
        try:
            _dispatch_queue.put_nowait(payload)
        except queue.Full:
            pass


def ts() -> str:
    return datetime.now().strftime("%H:%M:%S")


def disconnect_payload() -> dict[str, Any]:
    return {
        "link_state": "Disconnected",
        "safety_state": "OFFLINE",
        "hazard_type": "NONE",
        "has_telemetry": False,
    }


def natural_key(path: str):
    return [int(p) if p.isdigit() else p for p in re.split(r"(\d+)", path)]


def find_jlink_rtt_logger() -> str | None:
    patterns = [
        r"C:\Program Files\SEGGER\JLink*\JLinkRTTLogger.exe",
        r"C:\Program Files (x86)\SEGGER\JLink*\JLinkRTTLogger.exe",
    ]
    for pattern in patterns:
        matches = glob.glob(pattern)
        if matches:
            return max(matches, key=natural_key)
    return None


def _post_http(url: str, payload: dict[str, Any], secret: str = "") -> bool:
    try:
        data = json.dumps(payload).encode("utf-8")
        headers = {
            "Content-Type": "application/json",
            "User-Agent": "FRDM-RW612-Bridge/1.2"
        }
        if secret:
            headers["X-Telemetry-Secret"] = secret

        req = urllib.request.Request(url, data=data, headers=headers, method="POST")
        with urllib.request.urlopen(req, timeout=3.5) as resp:
            ok = resp.status in (200, 201)
            if ok:
                _last_error.pop(url, None)
            return ok
    except urllib.error.HTTPError as exc:
        reason = f"HTTP {exc.code} {exc.reason}"
        try:
            reason += f" - {exc.read().decode('utf-8', 'replace')[:160]}"
        except Exception:
            pass
    except Exception as exc:
        reason = f"{type(exc).__name__}: {exc}"

    if _last_error.get(url) != reason:
        _last_error[url] = reason
        print(f"  [{ts()}] [POST FAIL] {url} -> {reason}")
    return False


def _http_dispatcher_worker(cloud_url: str, local_url: str, secret: str):
    """Post telemetry off the serial thread and retry transient endpoint failures."""
    while True:
        item = _dispatch_queue.get()
        if item is None:
            break
        try:
            # The local dashboard is the user's live monitor, so deliver there first.
            # Retain a failed local item and retry with backoff instead of silently dropping it.
            if local_url:
                delay = 0.5
                while not _post_http(local_url, item, secret):
                    time.sleep(delay)
                    delay = min(delay * 2.0, 8.0)
            # Cloud mirroring is best effort; it must never stall local dashboard updates.
            if cloud_url:
                _post_http(cloud_url, item, secret)
        except Exception as exc:
            print(f"  [{ts()}] [DISPATCH ERROR] {exc}")
        finally:
            _dispatch_queue.task_done()


def handle_line(line: str, link_source: str, state: dict[str, Any], raw_mode: bool, csv_path: str | None = None) -> bool:
    """Process one console line. Returns True if valid telemetry packet was extracted."""
    line = line.strip()
    if not line:
        return False

    if raw_mode:
        print(f"  [{ts()}] RAW: {line}")

    if "[TELEMETRY]" not in line:
        if not raw_mode and line.startswith(("[TEMP]", "[MPU]", "[GAS]", "[FALL]", "[SOS]", ">>", "[HARDFAULT]")):
            print(f"  [{ts()}] [MCU LOG] {line}")
        return False

    raw_json = line.split("[TELEMETRY]", 1)[1].strip()
    try:
        payload = json.loads(raw_json)
    except json.JSONDecodeError as exc:
        print(f"  [{ts()}] [BAD JSON] {exc}: {raw_json[:120]}")
        return False

    state["count"] += 1
    payload["link_state"] = "Connected"
    payload["link_source"] = link_source

    # Optional persistent CSV logging
    if csv_path:
        try:
            write_header = not os.path.exists(csv_path) or os.path.getsize(csv_path) == 0
            with open(csv_path, "a", encoding="utf-8") as f:
                if write_header:
                    f.write("timestamp,id,uptime_ms,temp_c,accel_g,accel_x,accel_y,accel_z,gas_ppm,state,hazard,sos,sensors_ok\n")
                f.write(f"{datetime.now().isoformat()},{payload.get('id','')},{payload.get('uptime_ms','')},"
                        f"{payload.get('temp','')},{payload.get('accel','')},{payload.get('accel_x','')},"
                        f"{payload.get('accel_y','')},{payload.get('accel_z','')},{payload.get('gas','')},"
                        f"{payload.get('state','')},{payload.get('hazard','')},{payload.get('sos','')},{payload.get('sensors_ok','')}\n")
        except Exception as exc:
            print(f"  [{ts()}] [CSV ERROR] Failed to write log: {exc}")

    # Enqueue asynchronously for HTTP transmission
    enqueue_latest(payload)

    temp = payload.get("temp", payload.get("temperature_c", "—"))
    accel = payload.get("accel", payload.get("accel_magnitude_g", "—"))
    gas = payload.get("gas", payload.get("gas_ppm", "—"))
    sstate = payload.get("state", payload.get("safety_state", "NORMAL"))
    hazard = payload.get("hazard", payload.get("hazard_type", "NONE"))
    ok = "" if payload.get("sensors_ok", True) else " | SENSOR FAULT"

    print(f"  [{ts()}] #{state['count']} [{link_source}] Temp:{temp}°C | Accel:{accel}g | "
          f"Gas:{gas}PPM | State:{sstate} | Hazard:{hazard}{ok}")
    return True


def stream_serial(target_port: str, baud_rate: int, raw_mode: bool, csv_path: str | None = None):
    import serial
    print(f"\n[*] Connecting to {target_port} at {baud_rate} baud...")
    while True:
        try:
            with serial.Serial(target_port, baudrate=baud_rate, timeout=1.0) as ser:
                print(f"[✓] Connected to {target_port}! Listening for live telemetry...\n")
                state = {"count": 0}
                opened_at = time.time()
                last_packet = time.time()
                got_any_bytes = False
                offline_sent = False
                hint_printed = False

                while True:
                    raw = ser.readline()
                    now = time.time()
                    if raw:
                        got_any_bytes = True
                        if handle_line(raw.decode("utf-8", errors="replace"), f"USB Serial ({target_port})", state, raw_mode, csv_path):
                            last_packet = now
                            offline_sent = False
                    else:
                        if not got_any_bytes and not hint_printed and (now - opened_at > NO_DATA_HINT_S):
                            print("  [!] Port is open but no data received yet. Verify:")
                            print("      1. Board is flashed and running (MCU-Link J10 USB connected).")
                            print("      2. Correct COM port was selected.")
                            print("      3. Debug UART is operating at 115200 baud.")
                            hint_printed = True

                    # Check link timeout regardless of whether non-telemetry bytes were received
                    if state["count"] > 0 and (now - last_packet > LINK_TIMEOUT_S) and not offline_sent:
                        print(f"  [{ts()}] [LINK] Telemetry stream stopped (>5.0s) — signalling OFFLINE")
                        enqueue_latest(disconnect_payload())
                        offline_sent = True
        except Exception as exc:
            enqueue_latest(disconnect_payload())
            print(f"\n[!] Serial connection to {target_port} lost: {exc}")
            print("[*] Reconnecting in 3 seconds...")
            time.sleep(3)


def stream_rtt(rtt_exe: str, raw_mode: bool, csv_path: str | None = None):
    print(f"[*] Found J-Link RTT Logger: {rtt_exe}")
    print("[*] Connecting to NXP FRDM-RW612 via SWD RTT...")

    log_file = os.path.join(tempfile.gettempdir(), f"rw612_rtt_{int(time.time())}.log")
    cmd = [rtt_exe, "-Device", "RW612", "-If", "SWD", "-Speed", "4000", "-RTTChannel", "0", log_file]

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    deadline = time.time() + 15
    while not os.path.exists(log_file):
        if proc.poll() is not None:
            out = proc.stdout.read().decode("utf-8", "replace") if proc.stdout else ""
            raise RuntimeError(f"JLinkRTTLogger exited with code {proc.returncode}: {out.strip()[:400]}")
        if time.time() > deadline:
            proc.terminate()
            raise RuntimeError("Timed out waiting for RTT control block.")
        time.sleep(0.1)

    print("[✓] RTT attached. Streaming live telemetry...\n")
    state = {"count": 0}
    last_packet = time.time()
    buf = b""

    try:
        with open(log_file, "rb") as f:
            while proc.poll() is None:
                chunk = f.read(4096)
                if chunk:
                    buf += chunk
                    *lines, buf = buf.split(b"\n")
                    for raw in lines:
                        if handle_line(raw.decode("utf-8", "replace"), "MCU-Link RTT", state, raw_mode, csv_path):
                            last_packet = time.time()
                    continue
                time.sleep(0.05)
    finally:
        enqueue_latest(disconnect_payload())
        if proc.poll() is None:
            proc.terminate()


def pick_serial_port(requested: str | None) -> str | None:
    import serial.tools.list_ports
    ports = list(serial.tools.list_ports.comports())
    if requested:
        return requested
    for p in ports:
        text = f"{p.description} {p.manufacturer} {p.hwid}".lower()
        if any(t in text for t in ("mcu-link", "mcu link", "nxp", "cmsis-dap", "vcom", "serial")):
            return p.device
    if ports:
        print("[!] Available COM ports:")
        for p in ports:
            print(f"    {p.device}: {p.description}")
        return ports[0].device
    return None


def main():
    parser = argparse.ArgumentParser(description="NXP FRDM-RW612 Worker Safety Telemetry Bridge")
    parser.add_argument("port", nargs="?", default=None, help="COM port (e.g. COM3 or /dev/ttyACM0)")
    parser.add_argument("--port", dest="opt_port", default=None, help="COM port option")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--csv", default=None, help="Save received telemetry packets into a CSV log file")
    parser.add_argument("--raw", action="store_true", help="Print all raw incoming serial lines")
    parser.add_argument("--rtt", action="store_true", help="Force J-Link SWD RTT transport")
    parser.add_argument("--cloud-url", default=DEFAULT_CLOUD_URL, help="Target Cloud Dashboard API URL")
    parser.add_argument("--local-url", default=DEFAULT_LOCAL_URL, help="Target Local Dashboard API URL")
    parser.add_argument("--secret", default=TELEMETRY_SECRET, help="Optional shared authentication secret")

    args = parser.parse_args()
    port = args.opt_port or args.port

    print("=" * 72)
    print("  NXP FRDM-RW612 Worker Safety Monitor — Telemetry Bridge")
    print("=" * 72)
    print(f"  Target Cloud Dashboard: {args.cloud_url}")
    print(f"  Target Local Dashboard: {args.local_url}")
    print(f"  Serial Baud Rate:       {args.baud}")
    if args.csv:
        print(f"  Logging Packets to CSV: {args.csv}")
    print("=" * 72)

    # Start non-blocking HTTP dispatcher worker thread
    dispatch_thread = threading.Thread(
        target=_http_dispatcher_worker,
        args=(args.cloud_url, args.local_url, args.secret),
        daemon=True,
        name="http-dispatch"
    )
    dispatch_thread.start()

    if args.rtt:
        rtt_exe = find_jlink_rtt_logger()
        if not rtt_exe:
            print("[ERROR] JLinkRTTLogger.exe not found. Falling back to USB serial...")
        else:
            try:
                stream_rtt(rtt_exe, args.raw, args.csv)
                return
            except Exception as exc:
                print(f"[!] RTT failed: {exc}. Falling back to USB serial...")

    target_port = pick_serial_port(port)
    if target_port:
        stream_serial(target_port, args.baud, args.raw, args.csv)
    else:
        print("[ERROR] No MCU-Link COM port detected. Please plug in FRDM-RW612 via MCU-Link J10 USB.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[*] Telemetry Bridge stopped by user.")
