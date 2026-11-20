# Industrial Worker Safety Monitoring Dashboard — Plan

## Implementation approach

Build a self-contained Python/Flask application served on the project's configured port 3000. Flask will serve the responsive dashboard, static assets, and JSON endpoints from the same origin. The project will contain a lock-protected in-memory telemetry snapshot and bounded event/history buffers; this keeps the demo deployable without provisioning a database while leaving telemetry ingestion simple for an RW612 node or serial bridge. REST ingestion will accept the supplied short field names and documented expanded names, validate and normalize values, track message IDs, packet counts, timestamps, link/buffer state, and derive a browser-facing hardware mirror from the telemetry. A configurable optional PySerial worker will look for MCU-Link VCOM ports and ingest `[TELEMETRY] {...}` lines at 115200 8N1; it must shut down cleanly and remain harmless when no serial device/dependency is present.

Hazard evaluation will mirror the supplied temperature/gas thresholds and SOS/fall/fault states, giving SOS highest priority. Fall detection in server-ingested samples will recognize free-fall below 0.4 g followed within 1.0 seconds by impact above 2.5 g, and latch the emergency for five seconds. Since the board performs classification locally, the API will also accept and preserve node-provided `state`/`hazard` values when valid. Simulation actions will provide repeatable demonstration inputs matching the brief. The frontend polls status once per second, renders current gauges, sparklines, motion vector, event log and actuator state, and sends simulation requests; incident exports are generated from the current event buffer. Browser audio uses the Web Audio API only after a user gesture and provides a mute/acknowledge control.

## Project structure

- `dashboard_app.py` — Flask application, REST endpoints, telemetry normalization/classification, incident generation, optional serial ingestion, and startup.
- `templates/index.html` — accessible single-page dashboard shell and content sections.
- `static/css/dashboard.css` — responsive SCADA visual system and state-specific motion treatments.
- `static/js/dashboard.js` — API polling, gauges/charts, incident filters/exports, simulation controls, clock, and Web Audio alerts.
- `requirements.txt` — Flask and optional PySerial dependency.
- `README.md` — local install/run instructions, API payloads, serial configuration, limitations, and demo walkthrough.
- `Dockerfile` — self-contained container runtime for the Flask server on the project port.
- `plan.md`, `TODO.md` — implementation/design record and acceptance outcomes.

## Design

The requested industrial SCADA / mission-control visual language is the design reference, so no alternate styles are introduced.

- **Design Movement:** Industrial SCADA console reinterpreted as a crisp, contemporary control-room interface.
- **Core Principles:** Risk is unmistakable at a glance; dense telemetry remains orderly; interactions are direct and reversible; sensor provenance and connectivity are always visible.
- **Color Philosophy:** Zinc and slate form a low-glare work surface. Emerald means verified safe, amber signals intervention, and crimson is reserved for immediate danger. Cool cyan is used sparingly for live data and controls.
- **Layout Paradigm:** A narrow status/navigation rail anchors identity and connection context, followed by a full-width master alarm banner and asymmetrical telemetry/control regions; logs and demonstrations occupy practical lower panels rather than a generic centered marketing grid.
- **Signature Elements:** A segmented live-state beacon; instrument-style threshold arcs and tick marks; a compact hardware-twin LED stack with actuator feedback.
- **Interaction Philosophy:** Monitoring remains passive and low-noise; deliberate operator actions are explicit, while emergency state transitions receive immediate visual and optional audible attention.
- **Animation:** Subtle live-data shimmer and pulsing connection beacon; warning glow breathes slowly; emergency banner and red LED strobe rapidly but respect reduced-motion preferences. No decorative continuous movement elsewhere.
- **Typography System:** System sans-serif for legibility, with a restrained monospace face for measurements, IDs, timestamps, and protocol values. Numeric values use tabular figures and clear units.
- **Brand Essence:** A focused safety console for plant-floor officers and evaluators that makes edge-node risk and actuator state immediately legible. **Personality:** precise, vigilant, practical.
- **Brand Voice:** Short, operational, and unambiguous. Examples: “SYSTEM NORMAL — ALL WORKER PARAMETERS SECURE”; “CRITICAL EMERGENCY — IMMEDIATE ACTION REQUIRED”.
- **Wordmark & Logo:** A compact shield outline enclosing a three-segment signal trace, paired with an uppercase condensed “RW612 SAFETY NODE” label.
- **Signature Brand Color:** Signal cyan, reserved for the live edge telemetry trace and active node identity.

## Material constraints

The requested board performs hazard classification and actuator control locally; the dashboard is a monitoring/ingestion mirror, not a substitute for MCU safety logic. Browser audio requires prior user interaction due to browser autoplay rules. Telemetry and audit history are in-memory and reset on server restart; production persistence and authentication are intentionally outside the supplied scope. USB serial is optional and depends on host access to an attached VCOM device. Simulation endpoints are for bench demonstrations and should be disabled or protected before exposing an installation to an untrusted network. The requested package targets `python dashboard_app.py`; the Manus Preview also uses the same Flask process on port 3000.
