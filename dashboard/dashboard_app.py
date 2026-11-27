"""Flask backend for the FRDM-RW612 Industrial Worker Safety Dashboard.

This service is a monitoring mirror, not a replacement for the node's local
safety classifier or physical actuator logic. The demo's state and audit history
are intentionally in memory and reset when the process restarts.
"""
from __future__ import annotations

import copy
import importlib
import json
import logging
import math
import os
import threading
import time
import urllib.error
import urllib.request
from collections import deque
from datetime import datetime, timezone
from typing import Any

from flask import Flask, jsonify, render_template, request, send_from_directory

try:
    serial = importlib.import_module("serial")
    list_ports = importlib.import_module("serial.tools.list_ports")
except ImportError:  # Serial ingestion is optional at runtime.
    serial = None
    list_ports = None

LOG = logging.getLogger("rw612-dashboard")
NODE_ID = "FRDM-RW612-NODE-01"
VALID_STATES = {"NORMAL", "WARNING", "EMERGENCY", "OFFLINE"}
VALID_HAZARDS = {"NONE", "HIGH_TEMP", "GAS_LEAK", "FALL_DETECTED", "MANUAL_SOS", "SENSOR_FAULT"}
STATE_RANK = {"OFFLINE": -1, "NORMAL": 0, "WARNING": 1, "EMERGENCY": 2}


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def iso_now() -> str:
    return utc_now().isoformat(timespec="milliseconds").replace("+00:00", "Z")


def epoch_ms() -> int:
    return int(time.time() * 1000)


def parse_bool(value: Any, field: str) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)) and value in (0, 1):
        return bool(value)
    if isinstance(value, str) and value.strip().lower() in {"true", "false", "1", "0", "yes", "no"}:
        return value.strip().lower() in {"true", "1", "yes"}
    raise ValueError(f"'{field}' must be a boolean")


def parse_number(value: Any, field: str, minimum: float, maximum: float) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"'{field}' must be numeric") from None
    if not math.isfinite(number) or not minimum <= number <= maximum:
        raise ValueError(f"'{field}' must be between {minimum:g} and {maximum:g}")
    return number


KV_REST_API_URL = os.getenv("KV_REST_API_URL") or os.getenv("UPSTASH_REDIS_REST_URL", "")
KV_REST_API_TOKEN = os.getenv("KV_REST_API_TOKEN") or os.getenv("UPSTASH_REDIS_REST_TOKEN", "")
REMOTE_STATUS_URL = os.getenv(
    "REMOTE_STATUS_URL",
    "https://worker-safety-dashboard.vercel.app/api/status",
).strip()
REMOTE_STATUS_CACHE_SECONDS = 2.0
_remote_status_lock = threading.Lock()
_remote_status_cache: dict[str, Any] = {"expires_at": 0.0, "data": None}


def _kv_command(command: list[str]) -> Any:
    if not KV_REST_API_URL or not KV_REST_API_TOKEN:
        return None
    try:
        url = KV_REST_API_URL.rstrip("/")
        req = urllib.request.Request(url, method="POST")
        req.add_header("Authorization", f"Bearer {KV_REST_API_TOKEN}")
        req.add_header("Content-Type", "application/json")
        data = json.dumps(command).encode("utf-8")
        with urllib.request.urlopen(req, data=data, timeout=1.5) as resp:
            body = json.loads(resp.read().decode("utf-8"))
            return body.get("result")
    except Exception as exc:
        LOG.debug("KV command failed: %s", exc)
        return None


_last_kv_write = 0.0
_last_kv_state_sig = ""


def _kv_save_snapshot(snapshot_data: dict[str, Any], force: bool = False) -> None:
    global _last_kv_write, _last_kv_state_sig
    if not KV_REST_API_URL or not KV_REST_API_TOKEN:
        return

    now = time.monotonic()
    state_name = str(snapshot_data.get("safety_state", "OFFLINE"))
    hazard = str(snapshot_data.get("hazard_type", "NONE"))
    state_sig = f"{state_name}:{hazard}"

    # Rate-limit to conserve Upstash daily quota: only write on state change, emergency, or every >=5s
    if not force and state_sig == _last_kv_state_sig and (now - _last_kv_write < 5.0) and state_name != "EMERGENCY":
        return

    try:
        payload_str = json.dumps(snapshot_data)
        _kv_command(["SET", "rw612:snapshot", payload_str, "EX", "120"])
        _last_kv_write = now
        _last_kv_state_sig = state_sig
    except Exception as exc:
        LOG.debug("KV save failed: %s", exc)


def _kv_load_snapshot() -> dict[str, Any] | None:
    if not KV_REST_API_URL or not KV_REST_API_TOKEN:
        return None
    try:
        res = _kv_command(["GET", "rw612:snapshot"])
        if isinstance(res, str):
            return json.loads(res)
    except Exception as exc:
        LOG.debug("KV load failed: %s", exc)
    return None


_remote_refresh_inflight = False


def _refresh_remote_status_async() -> None:
    global _remote_refresh_inflight
    try:
        req = urllib.request.Request(
            REMOTE_STATUS_URL,
            headers={"Accept": "application/json", "Cache-Control": "no-cache"},
        )
        with urllib.request.urlopen(req, timeout=2.5) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
        if not isinstance(payload, dict):
            payload = None
        elif payload.get("has_telemetry"):
            received_at = int(payload.get("received_at_ms") or 0)
            if not received_at or epoch_ms() - received_at >= 10000:
                payload = None
    except Exception as exc:
        LOG.debug("Remote status read failed: %s", exc)
        payload = None
    with _remote_status_lock:
        _remote_status_cache["data"] = payload
        _remote_status_cache["expires_at"] = time.monotonic() + REMOTE_STATUS_CACHE_SECONDS
        _remote_refresh_inflight = False


def _remote_status_snapshot() -> dict[str, Any] | None:
    """Return cached cloud mirror without ever blocking the local /api/status response."""
    global _remote_refresh_inflight
    if not REMOTE_STATUS_URL:
        return None
    now = time.monotonic()
    with _remote_status_lock:
        if now >= _remote_status_cache["expires_at"] and not _remote_refresh_inflight:
            _remote_refresh_inflight = True
            threading.Thread(target=_refresh_remote_status_async, name="remote-status-poll", daemon=True).start()
        return _remote_status_cache["data"]


class TelemetryStore:
    """Thread-safe current-state store with bounded histories and audit events."""

    def __init__(self) -> None:
        self.lock = threading.RLock()
        self.events: deque[dict[str, Any]] = deque(maxlen=500)
        self.history: deque[dict[str, Any]] = deque(maxlen=120)
        self.packet_count = 0
        self.event_count = 0
        self.fall_count = 0
        self.last_message_id: int | str | None = None
        self.last_sample_time: float | None = None
        self.last_hardware_sample_time: float | None = None
        self.freefall_time: float | None = None
        self.fall_latch_until = 0.0
        self.serial_state = "Disabled"
        self.state: dict[str, Any] = {
            "node_id": NODE_ID,
            "link_state": "Disconnected",
            "link_source": "none",
            "buffered": False,
            "offline_queue_count": 0,
            "packets_received": 0,
            "id": None,
            "timestamp_ms": None,
            "received_at_ms": None,
            "timestamp": None,
            "temperature_c": None,
            "temp": None,
            "accel_magnitude_g": None,
            "accel": None,
            "accel_x": None,
            "accel_y": None,
            "accel_z": None,
            "gas_ppm": None,
            "gas": None,
            "gas_raw": None,
            "gas_adc_v": None,
            "sos_active": False,
            "sos": False,
            "sensor_fault": False,
            "temp_ok": None,
            "mpu_ok": None,
            "gas_ok": None,
            "sensor_status": "Awaiting Link",
            "safety_state": "OFFLINE",
            "hazard_type": "NONE",
            "node_reported_state": "OFFLINE",
            "node_reported_hazard": "NONE",
            "led_normal": False,
            "led_warning": False,
            "led_warning_mode": "OFF",
            "led_emergency": False,
            "buzzer_active": False,
            "buzzer_mode": "OFF",
            "fall_event_count": 0,
            "temperature_min_c": None,
            "temperature_max_c": None,
            "source": "none",
            "has_telemetry": False,
        }

    def _history_point_locked(self) -> dict[str, Any]:
        return {
            "timestamp_ms": self.state.get("timestamp_ms", epoch_ms()),
            "received_at_ms": self.state.get("received_at_ms", epoch_ms()),
            "id": self.state.get("id"),
            "temperature_c": self.state.get("temperature_c"),
            "gas_ppm": self.state.get("gas_ppm"),
            "accel_magnitude_g": self.state.get("accel_magnitude_g"),
            "accel_x": self.state.get("accel_x"),
            "accel_y": self.state.get("accel_y"),
            "accel_z": self.state.get("accel_z"),
            "safety_state": self.state.get("safety_state", "OFFLINE"),
            "hazard_type": self.state.get("hazard_type", "NONE"),
            "source": self.state.get("source", "none"),
        }

    def _record_event_locked(self, severity: str, message: str, hazard: str) -> None:
        self.event_count += 1
        self.events.appendleft({
            "event_id": self.event_count,
            "timestamp": iso_now(),
            "timestamp_ms": epoch_ms(),
            "severity": severity,
            "message": message,
            "hazard_type": hazard,
            "safety_state": self.state.get("safety_state", "OFFLINE"),
            "packet_id": self.last_message_id,
            "source": self.state.get("source", "server"),
        })

    def _classify_locked(self) -> tuple[str, str]:
        s = self.state
        if not s.get("has_telemetry", False):
            return "OFFLINE", "NONE"

        temp = s.get("temperature_c") or 0.0
        gas = s.get("gas_ppm") or 0.0
        sos = bool(s.get("sos_active", False))
        sensor_fault = bool(s.get("sensor_fault", False))
        node_state = s.get("node_reported_state", "NORMAL")
        node_hazard = s.get("node_reported_hazard", "NONE")
        fall_active = time.monotonic() < self.fall_latch_until

        # An asserted SOS is always the highest-priority override.
        if sos or node_hazard == "MANUAL_SOS":
            return "EMERGENCY", "MANUAL_SOS"
        if fall_active:
            return "EMERGENCY", "FALL_DETECTED"
        if node_hazard == "FALL_DETECTED":
            node_hazard = "NONE"
            if node_state == "EMERGENCY":
                node_state = "NORMAL"

        # Environmental critical thresholds are emergencies; warnings follow.
        if gas >= 850.0:
            return "EMERGENCY", "GAS_LEAK"
        if temp >= 50.0:
            return "EMERGENCY", "HIGH_TEMP"
        if sensor_fault:
            computed = ("WARNING", "SENSOR_FAULT")
        elif gas >= 700.0:
            computed = ("WARNING", "GAS_LEAK")
        elif temp >= 40.0:
            computed = ("WARNING", "HIGH_TEMP")
        else:
            computed = ("NORMAL", "NONE")

        # Never allow a lower-severity reported node state to mask local readings.
        if node_state in VALID_STATES and STATE_RANK[str(node_state)] > STATE_RANK[computed[0]]:
            if node_hazard in VALID_HAZARDS and node_hazard != "NONE":
                return str(node_state), str(node_hazard)
            return str(node_state), computed[1]
        if node_state == computed[0] and node_hazard in VALID_HAZARDS and node_hazard != "NONE" and computed[1] == "NONE":
            return str(node_state), str(node_hazard)
        return computed

    def _refresh_outputs_locked(self) -> None:
        if not self.state.get("has_telemetry", False):
            self.state["safety_state"] = "OFFLINE"
            self.state["hazard_type"] = "NONE"
            self.state["led_normal"] = False
            self.state["led_warning"] = False
            self.state["led_warning_mode"] = "OFF"
            self.state["led_emergency"] = False
            self.state["buzzer_active"] = False
            self.state["buzzer_mode"] = "OFF"
            self.state["fall_event_count"] = self.fall_count
            return

        state_name, hazard = self._classify_locked()
        previous = (self.state.get("safety_state"), self.state.get("hazard_type"))
        self.state["safety_state"] = state_name
        self.state["hazard_type"] = hazard
        self.state["led_normal"] = state_name == "NORMAL"
        self.state["led_warning"] = state_name == "WARNING"
        self.state["led_warning_mode"] = "PULSE_1HZ" if hazard == "SENSOR_FAULT" else ("SOLID" if state_name == "WARNING" else "OFF")
        self.state["led_emergency"] = state_name == "EMERGENCY"
        self.state["buzzer_active"] = state_name in ("WARNING", "EMERGENCY")
        self.state["buzzer_mode"] = {
            "NORMAL": "OFF",
            "WARNING": "WARNING_BEEP_200MS",
            "EMERGENCY": "CONTINUOUS_RAPID_ALARM",
            "OFFLINE": "OFF",
        }.get(state_name, "OFF")
        self.state["fall_event_count"] = self.fall_count

        current = (state_name, hazard)
        if current != previous and state_name != "OFFLINE":
            if state_name == "EMERGENCY":
                severity = "EMERGENCY"
                label = {
                    "MANUAL_SOS": "Manual SOS activated — immediate response required.",
                    "FALL_DETECTED": "Fall detected — free-fall followed by high-impact event.",
                    "GAS_LEAK": "Critical gas concentration — hazardous exposure / fire risk.",
                    "HIGH_TEMP": "Critical ambient temperature — heat-stroke risk.",
                }.get(hazard, f"Emergency hazard: {hazard}.")
            elif state_name == "WARNING":
                severity = "WARNING"
                label = {
                    "HIGH_TEMP": "Elevated ambient temperature — advisory threshold reached.",
                    "GAS_LEAK": "Elevated gas / smoke concentration — advisory threshold reached.",
                    "SENSOR_FAULT": "Sensor fault — check node input and wiring.",
                }.get(hazard, f"Advisory warning: {hazard}.")
            else:
                severity = "INFO"
                label = "Hazard cleared — worker parameters are within safe limits."
            self._record_event_locked(severity, label, hazard)

    def ingest(self, payload: dict[str, Any], source: str = "wifi") -> dict[str, Any]:
        if not isinstance(payload, dict):
            raise ValueError("Telemetry payload must be a JSON object")
        with self.lock:
            current = self.state
            previous_node_hazard = current.get("node_reported_hazard", "NONE")

            temp_key = "temperature_c" if "temperature_c" in payload else "temp"
            accel_key = "accel_magnitude_g" if "accel_magnitude_g" in payload else "accel"
            gas_key = "gas_ppm" if "gas_ppm" in payload else "gas"

            temperature = parse_number(payload[temp_key], temp_key, -80, 200) if temp_key in payload else (current["temperature_c"] or 24.0)
            gas = parse_number(payload[gas_key], gas_key, 0, 100000) if gas_key in payload else (current["gas_ppm"] or 0.0)
            accel = parse_number(payload[accel_key], accel_key, 0, 100) if accel_key in payload else (current["accel_magnitude_g"] or 1.0)
            ax = parse_number(payload.get("accel_x", current["accel_x"] if current["accel_x"] is not None else 0.0), "accel_x", -100, 100)
            ay = parse_number(payload.get("accel_y", current["accel_y"] if current["accel_y"] is not None else 0.0), "accel_y", -100, 100)
            az = parse_number(payload.get("accel_z", current["accel_z"] if current["accel_z"] is not None else 1.0), "accel_z", -100, 100)

            sos_value = payload.get("sos_active", payload.get("sos", current["sos_active"]))
            sos = parse_bool(sos_value, "sos")
            fault_value = payload["sensor_fault"] if "sensor_fault" in payload else not parse_bool(payload.get("sensors_ok", True), "sensors_ok")
            sensor_fault = parse_bool(fault_value, "sensor_fault")

            node_state = str(payload.get("state", payload.get("safety_state", current.get("node_reported_state", "NORMAL")))).upper()
            if node_state not in VALID_STATES:
                node_state = "NORMAL"
            node_hazard = str(payload.get("hazard", payload.get("hazard_type", current.get("node_reported_hazard", "NONE")))).upper()
            if node_hazard not in VALID_HAZARDS:
                node_hazard = "NONE"

            raw_id = payload.get("id", payload.get("message_id"))
            if raw_id is not None:
                if isinstance(raw_id, bool):
                    message_id: int | str = str(raw_id).lower()
                elif isinstance(raw_id, (int, float)) and math.isfinite(float(raw_id)):
                    message_id = int(raw_id)
                else:
                    message_id = str(raw_id)[:64]
                self.last_message_id = message_id
            elif self.last_message_id is None:
                self.last_message_id = 1
            else:
                self.last_message_id = self.last_message_id + 1 if isinstance(self.last_message_id, int) else str(epoch_ms())

            timestamp = payload.get("timestamp_ms", epoch_ms())
            try:
                timestamp_ms = int(timestamp)
            except (TypeError, ValueError, OverflowError):
                timestamp_ms = epoch_ms()
            if timestamp_ms < 0 or timestamp_ms > 9_999_999_999_999:
                timestamp_ms = epoch_ms()

            self.state.update({
                "temperature_c": temperature,
                "temp": temperature,
                "gas_ppm": gas,
                "gas": gas,
                "gas_raw": parse_number(payload["gas_raw"], "gas_raw", 0, 65535) if "gas_raw" in payload else current.get("gas_raw"),
                "gas_adc_v": parse_number(payload["gas_adc_v"], "gas_adc_v", 0, 1.2) if "gas_adc_v" in payload else current.get("gas_adc_v"),
                "accel_magnitude_g": accel,
                "accel": accel,
                "accel_x": ax,
                "accel_y": ay,
                "accel_z": az,
                "sos_active": sos,
                "sos": sos,
                "sensor_fault": sensor_fault,
                "temp_ok": parse_bool(payload["temp_ok"], "temp_ok") if "temp_ok" in payload else None,
                "mpu_ok": parse_bool(payload["mpu_ok"], "mpu_ok") if "mpu_ok" in payload else None,
                "gas_ok": parse_bool(payload["gas_ok"], "gas_ok") if "gas_ok" in payload else None,
                "sensor_status": "Fault" if sensor_fault else "Nominal",
                "node_reported_state": node_state,
                "node_reported_hazard": node_hazard,
                "id": self.last_message_id,
                "timestamp_ms": timestamp_ms,
                "received_at_ms": epoch_ms(),
                "timestamp": iso_now(),
                "link_state": {"connected": "Connected", "disconnected": "Disconnected", "reconnecting": "Reconnecting", "buffered": "Buffered"}.get(str(payload.get("link_state", "connected" if source != "serial" or self.serial_state == "Connected" else "reconnecting")).strip().lower(), "Connected"),
                "link_source": str(payload.get("link_source", "USB Serial" if source == "serial" else ("Simulation" if source == "simulator" else "Wi-Fi")))[:32],
                "buffered": parse_bool(payload.get("buffered", payload.get("offline_buffered", False)), "buffered"),
                "offline_queue_count": max(0, min(1_000_000, int(payload.get("offline_queue_count", payload.get("queue_count", 0)) or 0))),
                "source": source,
                "has_telemetry": True,
            })
            self.packet_count += 1
            self.state["packets_received"] = self.packet_count
            self.last_sample_time = time.monotonic()
            if source in ("wifi", "serial"):
                self.last_hardware_sample_time = time.monotonic()

            # Host-side fall rule: <0.4 g free fall followed by >2.5 g within 1 s.
            now = time.monotonic()
            fall_detected_by_motion = False
            if accel < 0.4:
                self.freefall_time = now
            elif accel > 2.5:
                if self.freefall_time is not None and now - self.freefall_time <= 1.0:
                    self.fall_latch_until = now + 5.0
                    self.fall_count += 1
                    fall_detected_by_motion = True
                self.freefall_time = None
            elif self.freefall_time is not None and now - self.freefall_time > 1.0:
                self.freefall_time = None
            if node_hazard == "FALL_DETECTED":
                self.fall_latch_until = max(self.fall_latch_until, now + 5.0)
                if previous_node_hazard != "FALL_DETECTED" and not fall_detected_by_motion:
                    self.fall_count += 1

            if self.state["temperature_min_c"] is None:
                self.state["temperature_min_c"] = temperature
                self.state["temperature_max_c"] = temperature
            else:
                self.state["temperature_min_c"] = min(self.state["temperature_min_c"], temperature)
                self.state["temperature_max_c"] = max(self.state["temperature_max_c"], temperature)

            self._refresh_outputs_locked()
            self.history.append(self._history_point_locked())
            snap = self.snapshot_locked()
            _kv_save_snapshot(snap)
            return snap

    def reset_to_disconnected(self) -> dict[str, Any]:
        with self.lock:
            self.fall_latch_until = 0.0
            self.freefall_time = None
            # Preserve self.fall_count and self.history across disconnection events
            self.state.update({
                "link_state": "Disconnected",
                "link_source": "none",
                "buffered": False,
                "offline_queue_count": 0,
                "temp_ok": None,
                "mpu_ok": None,
                "gas_ok": None,
                "sensor_status": "Awaiting Link",
                "safety_state": "OFFLINE",
                "hazard_type": "NONE",
                "node_reported_state": "OFFLINE",
                "node_reported_hazard": "NONE",
                "led_normal": False,
                "led_warning": False,
                "led_warning_mode": "OFF",
                "led_emergency": False,
                "buzzer_active": False,
                "buzzer_mode": "OFF",
                "source": "none",
                "has_telemetry": False,
            })
            self._record_event_locked("INFO", "Telemetry link disconnected — awaiting FRDM-RW612 transmission.", "NONE")
            snap = self.snapshot_locked()
            _kv_save_snapshot(snap)
            return snap

    def snapshot_locked(self) -> dict[str, Any]:
        # 5.0s Heartbeat Watchdog: If no fresh packet has arrived within 5.0s, transition to OFFLINE
        if self.state.get("has_telemetry", False):
            if self.last_sample_time is not None and (time.monotonic() - self.last_sample_time > 5.0):
                self.reset_to_disconnected()

        self._refresh_outputs_locked()
        result = copy.deepcopy(self.state)
        result["incidents"] = copy.deepcopy(list(self.events))
        result["history"] = copy.deepcopy(list(self.history))
        result["serial_status"] = self.serial_state
        result["server_time"] = iso_now()
        return result

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            return self.snapshot_locked()


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
templates_dir = os.path.join(BASE_DIR, "templates")
if not os.path.isdir(templates_dir):
    templates_dir = os.path.join(os.path.dirname(BASE_DIR), "templates")

static_dir = os.path.join(BASE_DIR, "static")
if not os.path.isdir(static_dir):
    static_dir = os.path.join(os.path.dirname(BASE_DIR), "static")

store = TelemetryStore()
# The debug endpoints dump request headers and the WSGI environ, and Flask's debugger allows code
# execution; both were on by default and the app binds 0.0.0.0. Opt in with DASHBOARD_DEBUG=1.
DEBUG_ENDPOINTS = os.getenv("DASHBOARD_DEBUG", "0").strip().lower() in {"1", "true", "yes", "on"}
app = Flask(
    __name__,
    template_folder=templates_dir,
    static_folder=static_dir,
)
app.config["JSON_SORT_KEYS"] = False


@app.before_request
def handle_vercel_matched_path():
    """Ensure Vercel rewrites to /api/index route accurately to target API endpoints."""
    endpoint = request.args.get("endpoint", "").strip().lower()
    path_param = request.args.get("path", "").strip().lower()

    if endpoint == "status" or path_param == "api/status" or path_param.endswith("/status"):
        return get_status()
    if (endpoint == "telemetry" or path_param == "api/telemetry") and request.method == "POST":
        return post_telemetry()
    if (endpoint == "simulate" or path_param == "api/simulate") and request.method == "POST":
        return simulate()
    if DEBUG_ENDPOINTS and (endpoint == "debug-env" or path_param == "debug-env"):
        return jsonify({
            "path": request.path,
            "endpoint": endpoint,
            "path_param": path_param,
            "url": request.url,
            "headers": dict(request.headers),
            "environ": {k: str(v) for k, v in request.environ.items() if isinstance(v, (str, int, float, bool))}
        })

    matched = (
        request.headers.get("x-vercel-matched-path") or
        request.headers.get("x-matched-path") or
        request.headers.get("x-forwarded-uri") or
        request.headers.get("x-real-path") or
        request.environ.get("HTTP_X_VERCEL_MATCHED_PATH") or
        request.environ.get("HTTP_X_MATCHED_PATH") or
        request.environ.get("RAW_URI") or
        request.environ.get("REQUEST_URI")
    )
    if matched:
        cleaned = matched.split("?")[0].rstrip("/")
        if cleaned.endswith("/api/status"):
            return get_status()
        if cleaned.endswith("/api/telemetry") and request.method == "POST":
            return post_telemetry()
        if cleaned.endswith("/api/simulate") and request.method == "POST":
            return simulate()


@app.route("/debug-env")
def debug_env():
    if not DEBUG_ENDPOINTS:
        return jsonify({"error": "disabled (set DASHBOARD_DEBUG=1)"}), 404
    return jsonify({
        "path": request.path,
        "url": request.url,
        "headers": dict(request.headers),
        "environ": {k: str(v) for k, v in request.environ.items() if isinstance(v, (str, int, float, bool))}
    })


@app.route("/", methods=["GET", "POST"])
@app.route("/api/index", methods=["GET", "POST"])
@app.route("/api/index.py", methods=["GET", "POST"])
def dashboard():
    # If this is a POST, it is incoming telemetry or simulation
    if request.method == "POST":
        payload = request.get_json(silent=True) or {}
        if "action" in payload and "temp" not in payload:
            return simulate()
        return post_telemetry()

    # Check if this is an API GET request
    uri = (request.headers.get("x-forwarded-uri", "") or 
           request.headers.get("x-matched-path", "") or 
           request.environ.get("REQUEST_URI", "") or 
           request.environ.get("RAW_URI", ""))
    if "/api/status" in uri or "/status" in uri or "status" in request.args:
        return get_status()
    if DEBUG_ENDPOINTS and ("debug" in uri or "debug" in request.args):
        return jsonify({
            "path": request.path,
            "uri": uri,
            "headers": dict(request.headers),
            "environ": {k: str(v) for k, v in request.environ.items() if isinstance(v, (str, int, float, bool))}
        })
    return render_template("index.html")


@app.route("/static/<path:filename>")
def serve_static(filename: str):
    return send_from_directory(static_dir, filename)


@app.errorhandler(404)
def handle_404(e):
    """Fallback handler so any rewritten path on Vercel serves the dashboard UI."""
    if request.path.startswith("/api/") and not request.path.startswith(("/api/index", "/api/status", "/api/telemetry", "/api/simulate")):
        return jsonify({"error": f"API endpoint '{request.path}' not found"}), 404
    return render_template("index.html")


@app.route("/manus-routes.json")
def route_manifest():
    return send_from_directory(app.root_path, "manus-routes.json", mimetype="application/json")


@app.route("/api/status", methods=["GET"])
@app.route("/api/index/api/status", methods=["GET"])
def get_status():
    snap = store.snapshot()
    local_received_at = int(snap.get("received_at_ms") or 0)
    if snap.get("has_telemetry") and local_received_at and epoch_ms() - local_received_at < 10000:
        return jsonify(snap)

    for remote in (_kv_load_snapshot(), _remote_status_snapshot()):
        if remote and remote.get("has_telemetry", False):
            received_at = int(remote.get("received_at_ms") or 0)
            if received_at and epoch_ms() - received_at < 10000:
                return jsonify(remote)

    return jsonify(snap)


@app.route("/api/telemetry", methods=["POST"])
@app.route("/api/index/api/telemetry", methods=["POST"])
def post_telemetry():
    secret_expected = os.getenv("TELEMETRY_SECRET", "").strip()
    if secret_expected:
        secret_given = request.headers.get("X-Telemetry-Secret", "") or request.args.get("secret", "")
        if secret_given != secret_expected:
            return jsonify({"error": "Unauthorized telemetry source"}), 401

    payload = request.get_json(silent=True)
    if not isinstance(payload, dict):
        return jsonify({"error": "Request body must be a JSON object"}), 400
    # The bridge sends an explicit OFFLINE marker (null readings). Previously this hit the numeric
    # validator and returned HTTP 400, so the dashboard only went offline via the watchdog.
    if payload.get("has_telemetry") is False or str(payload.get("link_state", "")).strip().lower() == "disconnected":
        return jsonify({"ok": True, "data": store.reset_to_disconnected()})
    try:
        link_source = str(payload.get("link_source", "")).lower()
        ingest_source = "serial" if "serial" in link_source or "vcom" in link_source else "wifi"
        return jsonify({"ok": True, "data": store.ingest(payload, ingest_source)})
    except (ValueError, TypeError) as exc:
        return jsonify({"error": str(exc)}), 400


@app.route("/api/simulate", methods=["POST"])
@app.route("/api/index/api/simulate", methods=["POST"])
def simulate():
    secret_expected = os.getenv("TELEMETRY_SECRET", "").strip()
    allow_sim = os.getenv("ALLOW_SIMULATION", "1").strip().lower() in {"1", "true", "yes", "on"}
    if secret_expected and not allow_sim:
        secret_given = request.headers.get("X-Telemetry-Secret", "") or request.args.get("secret", "")
        if secret_given != secret_expected:
            return jsonify({"error": "Unauthorized: Simulation requires valid X-Telemetry-Secret header"}), 403

    payload = request.get_json(silent=True) or {}
    action = str(payload.get("action", "")).lower().strip()

    sim_map = {
        "sos": {
            "sos_active": True,
            "safety_state": "EMERGENCY",
            "hazard_type": "MANUAL_SOS",
            "link_source": "Simulation (Manual SOS)",
        },
        "fall": {
            "accel_magnitude_g": 3.45,
            "safety_state": "EMERGENCY",
            "hazard_type": "FALL_DETECTED",
            "link_source": "Simulation (Impact Fall)",
        },
        "high_temp": {
            "temperature_c": 52.8,
            "safety_state": "EMERGENCY",
            "hazard_type": "HIGH_TEMP",
            "link_source": "Simulation (Thermal Hazard)",
        },
        "gas_leak": {
            "temperature_c": 26.5,
            "gas_ppm": 880.0,
            "safety_state": "EMERGENCY",
            "hazard_type": "GAS_LEAK",
            "link_source": "Simulation (Gas Hazard)",
        },
        "sensor_fault": {
            "temperature_c": 25.0,
            "gas_ppm": 35.0,
            "accel_magnitude_g": 1.0,
            "sos_active": False,
            "sensor_fault": True,
            "safety_state": "WARNING",
            "hazard_type": "SENSOR_FAULT",
            "link_source": "Simulation (Fault Diagnostic)",
        },
        "normal": {
            "temperature_c": 26.5,
            "gas_ppm": 45.0,
            "accel_magnitude_g": 1.01,
            "accel_x": 0.05,
            "accel_y": 0.02,
            "accel_z": 1.0,
            "sos_active": False,
            "sensor_fault": False,
            "safety_state": "NORMAL",
            "hazard_type": "NONE",
            "link_source": "Simulation (Normal Safe)",
        },
        "clear": {
            "temperature_c": 25.0,
            "gas_ppm": 35.0,
            "accel_magnitude_g": 1.0,
            "sos_active": False,
            "sensor_fault": False,
            "safety_state": "NORMAL",
            "hazard_type": "NONE",
            "link_source": "Simulation (Cleared)",
        }
    }

    if action not in sim_map:
        return jsonify({
            "error": f"Unknown simulation action '{action}'. Supported: {list(sim_map.keys())}"
        }), 400

    sim_payload = sim_map[action]
    sim_payload["timestamp_ms"] = epoch_ms()
    sim_payload["link_state"] = "Connected"
    sim_payload["has_telemetry"] = True

    if action in ("normal", "clear", "sensor_fault"):
        with store.lock:
            store.fall_latch_until = 0.0
            store.freefall_time = None

    result = store.ingest(sim_payload, source="simulator")
    _kv_save_snapshot(result, force=True)
    return jsonify({"ok": True, "action": action, "data": result})


def serial_reader() -> None:
    """Discover MCU-Link VCOM ports and ingest one-line telemetry JSON messages."""
    if serial is None or list_ports is None:
        with store.lock:
            store.serial_state = "Unavailable — install pyserial"
        LOG.warning("SERIAL_INGEST enabled, but pyserial is not installed")
        return
    while True:
        ports = []
        try:
            ports = list(list_ports.comports())
        except Exception as exc:
            LOG.debug("Serial port scan failed: %s", exc)
        match = next((port for port in ports if any(term in f"{port.description} {port.manufacturer} {port.hwid}".lower() for term in ("mcu-link", "mcu link", "nxp", "jlink", "j-link"))), None)
        if match is None:
            with store.lock:
                store.serial_state = "Scanning — no MCU-Link port"
            time.sleep(5)
            continue
        try:
            with serial.Serial(match.device, baudrate=115200, bytesize=8, parity="N", stopbits=1, timeout=1) as connection:
                with store.lock:
                    store.serial_state = "Connected"
                LOG.info("Reading telemetry from MCU-Link VCOM %s", match.device)
                while True:
                    line = connection.readline().decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    if ">> WIFI:" in line:
                        LOG.warning("RW612 %s", line.split(">> WIFI:", 1)[1].strip())
                        continue
                    marker = "[TELEMETRY]"
                    if marker not in line:
                        continue
                    raw_json = line.split(marker, 1)[1].strip()
                    try:
                        payload = json.loads(raw_json)
                        store.ingest(payload, "serial")
                    except (json.JSONDecodeError, ValueError, TypeError) as exc:
                        LOG.warning("Ignoring malformed serial telemetry: %s", exc)
        except Exception as exc:
            with store.lock:
                store.serial_state = "Reconnecting"
                if store.state.get("link_source") == "USB Serial":
                    store.state["link_state"] = "Reconnecting"
            LOG.warning("MCU-Link serial connection ended: %s", exc)
            time.sleep(3)


_serial_thread_started = False


def start_serial_reader() -> None:
    enabled = os.getenv("SERIAL_INGEST", "0").strip().lower() in {"1", "true", "yes", "on"}
    if enabled:
        global _serial_thread_started
        if not _serial_thread_started:
            threading.Thread(target=serial_reader, name="mcu-link-serial", daemon=True).start()
            _serial_thread_started = True
    else:
        store.serial_state = "Disabled — set SERIAL_INGEST=1 to scan"


start_serial_reader()

if __name__ == "__main__":
    app.run(host=os.getenv("HOST", "127.0.0.1"), port=int(os.getenv("PORT", "5000")), debug=DEBUG_ENDPOINTS)
