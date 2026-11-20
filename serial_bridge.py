"""High-Speed Telemetry Bridge for NXP FRDM-RW612 Worker Safety Monitor.

Supports dual transports:
  1. J-Link RTT (High-Speed SWD, zero COM port lock conflicts, 4 MHz transfer)
  2. USB Virtual COM Port (115200 baud fallback)

Reads live telemetry JSON packets emitted by the NXP FRDM-RW612 and automatically
forwards them to both the Cloud Vercel Dashboard and any local dashboard instance.
"""
import glob
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.request
from datetime import datetime

DEFAULT_CLOUD_URL = os.getenv("DASHBOARD_URL", "https://worker-safety-dashboard.vercel.app/api/telemetry")
DEFAULT_LOCAL_URL = os.getenv("LOCAL_URL", "http://localhost:5000/api/telemetry")
BAUD_RATE = int(os.getenv("BAUD_RATE", "115200"))


def find_jlink_rtt_logger() -> str | None:
    patterns = [
        r"C:\Program Files\SEGGER\JLink*\JLinkRTTLogger.exe",
        r"C:\Program Files (x86)\SEGGER\JLink*\JLinkRTTLogger.exe",
    ]
    for pattern in patterns:
        matches = glob.glob(pattern)
        if matches:
            return matches[-1]
    return None


def forward_packet(url: str, payload: dict) -> bool:
    try:
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            url,
            data=data,
            headers={"Content-Type": "application/json", "User-Agent": "FRDM-RW612-Bridge/1.0"},
            method="POST"
        )
        with urllib.request.urlopen(req, timeout=3.5) as resp:
            return resp.status in (200, 201)
    except Exception:
        return False


def stream_rtt(rtt_exe: str):
    print(f"[*] Found J-Link RTT Logger: {rtt_exe}")
    print("[*] Connecting to NXP FRDM-RW612 via SWD RTT...")

    log_file = os.path.join(tempfile.gettempdir(), f"rw612_rtt_{int(time.time())}.log")
    if os.path.exists(log_file):
        try:
            os.remove(log_file)
        except OSError:
            pass

    cmd = [
        rtt_exe,
        "-Device", "RW612",
        "-If", "SWD",
        "-Speed", "4000",
        "-RTTChannel", "0",
        log_file
    ]

    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)

    print("[✓] Connected to MCU via High-Speed RTT! Streaming live telemetry...\n")
    packet_count = 0
    last_packet_time = time.time()
    disconnected_sent = False

    try:
        with open(log_file, "r", encoding="utf-8", errors="replace") as f:
            while proc.poll() is None:
                line = f.readline()
                if not line:
                    if time.time() - last_packet_time > 2.5 and not disconnected_sent:
                        ts = datetime.now().strftime("%H:%M:%S")
                        print(f"  [{ts}] [LINK] Telemetry stream stopped / board disconnected — signaling OFFLINE")
                        disconnect_payload = {
                            "link_state": "Disconnected",
                            "safety_state": "OFFLINE",
                            "hazard_type": "NONE",
                            "has_telemetry": False,
                            "temperature_c": None,
                            "temp": None,
                            "accel_magnitude_g": None,
                            "accel": None,
                            "gas_ppm": None,
                            "gas": None,
                            "sos": False,
                            "sos_active": False,
                        }
                        forward_packet(DEFAULT_CLOUD_URL, disconnect_payload)
                        forward_packet(DEFAULT_LOCAL_URL, disconnect_payload)
                        disconnected_sent = True
                    time.sleep(0.05)
                    continue

                line = line.strip()
                if not line:
                    continue

                if "[TELEMETRY]" not in line:
                    if line.startswith(("[TEMP]", "[MPU]", "[GAS]", "[FALL]", "[SOS]", ">>")):
                        ts = datetime.now().strftime("%H:%M:%S")
                        print(f"  [{ts}] [MCU LOG] {line}")
                    continue

                raw_json = line.split("[TELEMETRY]", 1)[1].strip()
                try:
                    payload = json.loads(raw_json)
                except json.JSONDecodeError:
                    continue

                packet_count += 1
                last_packet_time = time.time()
                disconnected_sent = False
                payload["link_state"] = "Connected"
                payload["link_source"] = "MCU-Link RTT (High-Speed)"

                cloud_ok = forward_packet(DEFAULT_CLOUD_URL, payload)
                local_ok = forward_packet(DEFAULT_LOCAL_URL, payload)

                ts = datetime.now().strftime("%H:%M:%S")
                cloud_tag = "[Vercel: OK]" if cloud_ok else "[Vercel: --]"
                local_tag = "[Local: OK]" if local_ok else ""

                temp = payload.get("temp", payload.get("temperature_c", "—"))
                accel = payload.get("accel", payload.get("accel_magnitude_g", "—"))
                gas = payload.get("gas", payload.get("gas_ppm", "—"))
                state = payload.get("state", payload.get("safety_state", "NORMAL"))
                hazard = payload.get("hazard", payload.get("hazard_type", "NONE"))

                print(f"  [{ts}] #{packet_count} {cloud_tag} {local_tag} Temp:{temp}°C | Accel:{accel}g | Gas:{gas}PPM | State:{state} | Hazard:{hazard}")

    finally:
        disconnect_payload = {
            "link_state": "Disconnected",
            "safety_state": "OFFLINE",
            "hazard_type": "NONE",
            "has_telemetry": False,
            "temperature_c": None,
            "temp": None,
            "accel_magnitude_g": None,
            "accel": None,
            "gas_ppm": None,
            "gas": None,
            "sos": False,
            "sos_active": False,
        }
        forward_packet(DEFAULT_CLOUD_URL, disconnect_payload)
        forward_packet(DEFAULT_LOCAL_URL, disconnect_payload)
        if proc.poll() is None:
            proc.terminate()


def stream_serial(target_port: str):
    import serial
    print(f"\n[*] Connecting to {target_port} at {BAUD_RATE} baud...")
    while True:
        try:
            with serial.Serial(target_port, baudrate=BAUD_RATE, timeout=1.0) as ser:
                print(f"[✓] Connected to {target_port}! Listening for live telemetry...\n")
                packet_count = 0
                while True:
                    line = ser.readline().decode("utf-8", errors="replace").strip()
                    if not line:
                        continue

                    if "[TELEMETRY]" not in line:
                        if line.startswith(("[TEMP]", "[MPU]", "[GAS]", "[FALL]", "[SOS]", ">>")):
                            ts = datetime.now().strftime("%H:%M:%S")
                            print(f"  [{ts}] [MCU LOG] {line}")
                        continue

                    raw_json = line.split("[TELEMETRY]", 1)[1].strip()
                    try:
                        payload = json.loads(raw_json)
                    except json.JSONDecodeError:
                        continue

                    packet_count += 1
                    payload["link_state"] = "Connected"
                    payload["link_source"] = f"USB Serial ({target_port})"

                    cloud_ok = forward_packet(DEFAULT_CLOUD_URL, payload)
                    local_ok = forward_packet(DEFAULT_LOCAL_URL, payload)

                    ts = datetime.now().strftime("%H:%M:%S")
                    cloud_tag = "[Vercel: OK]" if cloud_ok else "[Vercel: --]"
                    local_tag = "[Local: OK]" if local_ok else ""

                    temp = payload.get("temp", payload.get("temperature_c", "—"))
                    accel = payload.get("accel", payload.get("accel_magnitude_g", "—"))
                    gas = payload.get("gas", payload.get("gas_ppm", "—"))
                    state = payload.get("state", payload.get("safety_state", "NORMAL"))
                    hazard = payload.get("hazard", payload.get("hazard_type", "NONE"))

                    print(f"  [{ts}] #{packet_count} {cloud_tag} {local_tag} Temp:{temp}°C | Accel:{accel}g | Gas:{gas}PPM | State:{state} | Hazard:{hazard}")

        except Exception as exc:
            disconnect_payload = {
                "link_state": "Disconnected",
                "safety_state": "OFFLINE",
                "hazard_type": "NONE",
                "has_telemetry": False,
                "temperature_c": None,
                "temp": None,
                "accel_magnitude_g": None,
                "accel": None,
                "gas_ppm": None,
                "gas": None,
                "sos": False,
                "sos_active": False,
            }
            forward_packet(DEFAULT_CLOUD_URL, disconnect_payload)
            forward_packet(DEFAULT_LOCAL_URL, disconnect_payload)
            print(f"\n[!] Serial connection to {target_port} lost: {exc}")
            print("[*] Reconnecting in 3 seconds...")
            time.sleep(3)


def main():
    print("=" * 72)
    print("  NXP FRDM-RW612 Worker Safety Monitor — Telemetry Bridge")
    print("=" * 72)
    print(f"  Target Cloud Dashboard: {DEFAULT_CLOUD_URL}")
    print(f"  Target Local Dashboard: {DEFAULT_LOCAL_URL}")
    print("=" * 72)

    force_serial = "--serial" in sys.argv or (len(sys.argv) > 1 and sys.argv[1].upper().startswith("COM"))
    rtt_exe = find_jlink_rtt_logger()

    if rtt_exe and not force_serial:
        try:
            stream_rtt(rtt_exe)
            return
        except Exception as exc:
            print(f"\n[!] RTT streaming ended: {exc}. Falling back to COM port...")

    import serial
    import serial.tools.list_ports
    ports = list(serial.tools.list_ports.comports())
    target_port = sys.argv[1] if (len(sys.argv) > 1 and not sys.argv[1].startswith("--")) else (ports[0].device if ports else None)
    if target_port:
        stream_serial(target_port)
    else:
        print("[ERROR] No MCU-Link COM port or RTT probe detected.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n[*] Bridge stopped by user.")
