"""Vercel entrypoint. The real application lives in dashboard/dashboard_app.py.

This file used to be a full duplicate copy that drifted out of sync; it now just re-exports the app.
NOTE: Vercel functions are stateless, so the in-memory TelemetryStore does not persist between
requests/instances. For a reliable live demo run the local dashboard (python dashboard/dashboard_app.py)
or back the store with an external service (e.g. Redis/KV).
"""
import os
import sys

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from dashboard.dashboard_app import app  # noqa: E402,F401
