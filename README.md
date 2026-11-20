# Industrial Worker Safety Monitoring and Emergency Alert System

<div align="center">

![Platform](https://img.shields.io/badge/Platform-NXP%20FRDM--RW612-005696?style=for-the-badge&logo=nxp&logoColor=white)
![Core](https://img.shields.io/badge/MCU-Arm%20Cortex--M33%20%40%20260MHz-0091BD?style=for-the-badge&logo=arm&logoColor=white)
![Wireless](https://img.shields.io/badge/Wireless-Tri--Radio%20(Wi--Fi%206%20%2F%20BLE%205.3)-orange?style=for-the-badge&logo=wi-fi&logoColor=white)
![Language](https://img.shields.io/badge/Firmware-Embedded%20C%20%2F%20MCUXpresso-00599C?style=for-the-badge&logo=c&logoColor=white)
![Dashboard](https://img.shields.io/badge/Dashboard-Python%20Flask%20%2F%20Vercel-black?style=for-the-badge&logo=vercel&logoColor=white)
[![Live Demo](https://img.shields.io/badge/Live%20Dashboard-worker--safety--dashboard.vercel.app-10b981?style=for-the-badge&logo=vercel&logoColor=white)](https://worker-safety-dashboard.vercel.app/)
![Status](https://img.shields.io/badge/Status-Functional%20Academic%20Prototype-success?style=for-the-badge)

<p align="center">
  <b>An Edge-Processing Based Wireless Industrial IoT Prototype for Real-Time Hazard Detection, Local Emergency Alerting, and Cloud Telemetry</b><br>
  🌐 <b>Live Mission Control Dashboard:</b> <a href="https://worker-safety-dashboard.vercel.app/">https://worker-safety-dashboard.vercel.app/</a>
</p>

---

</div>

## 📌 Executive Summary

Industrial environments such as chemical plants, manufacturing workshops, warehouses, and fabrication units expose personnel to sudden occupational hazards, including **accidental falls, thermal overexposure, and toxic/combustible gas leaks**. Conventional industrial safety relies predominantly on manual reporting or standalone single-parameter detectors that fail to communicate automatically. Centralized cloud-reliant IoT systems, conversely, compromise worker safety whenever the wireless link drops.

This project delivers an intelligent, self-contained **Edge-Processing Industrial IoT Safety Node** engineered on the **NXP FRDM-RW612** platform. Sensor data from an onboard digital temperature sensor, an external 6-axis motion tracking sensor, an analog gas/smoke sensor, and a manual SOS emergency button is processed entirely **on-device (at the edge)**. Abnormal hazards trigger immediate local acoustic and visual alerts (buzzer and status LEDs) **completely independent of network availability**, while simultaneously streaming real-time telemetry and emergency packets to a laptop and Vercel-deployed web dashboard.

---

## 🎯 Key Objectives

1. **Autonomous Edge Processing**: Continuous sampling (100 ms) and local classification into `NORMAL`, `WARNING`, or `EMERGENCY` without dependence on a remote server.
2. **Multi-Hazard Sensor Array**:
   - **Thermal Hazard**: Onboard P3T1755 high-precision digital temperature sensor via Flexcomm 2 I²C.
   - **Fall & Impact Detection**: External MPU-6050 6-axis MEMS accelerometer/gyroscope with a two-stage free-fall and impact recognition algorithm.
   - **Toxic Gas & Smoke**: MQ-2 SnO₂ semiconductor sensor via the RW612 LPADC (Low Power Analog-to-Digital Converter).
   - **Manual Emergency (SOS)**: Onboard SW2 user pushbutton mapped to an active-low emergency trigger.
3. **Network-Independent Local Actuation**: High-visibility status LEDs (Green/Yellow/Red) and an active buzzer providing instant local warnings.
4. **Fault-Tolerant Wireless Reporting**: Real-time telemetry streaming over Wi-Fi / USB VCOM with a 16-entry ring buffer that holds packets during connection outages and flushes them upon link recovery.
5. **Mission Control Web Dashboard**: Real-time, responsive dual-theme (Dark / Light) SCADA dashboard deployed on Vercel with Web Audio alarms, interactive actuator hardware twin, sparklines, CSV/JSON audit logs, and academic simulation controls.

---

## 🏗️ System Architecture

```text
                                  LAPTOP / CLOUD
                            USB POWER + VCOM / WI-FI
                                       │
                                       ▼
                       ┌────────────────────────────────┐
                       │        NXP FRDM-RW612          │
                       │    Arm Cortex-M33 @ 260MHz     │
                       │                                │
                       │    ┌──────────────────────┐    │
                       │    │ Onboard P3T1755 Temp │    │
                       │    └──────────┬───────────┘    │
                       └───────────────┼────────────────┘
                                       │
          ┌────────────────────────────┼────────────────────────────┐
          ▼                            ▼                            ▼
   MPU-6050 (I2C2)              MQ-2 Gas (LPADC)             SW2 Pushbutton
Motion & Fall Detection       Combustible Gas/Smoke         Manual SOS Emergency
          │                            │                            │
          └────────────────────────────┼────────────────────────────┘
                                       ▼
                         ┌───────────────────────────┐
                         │     Edge Decision Logic   │
                         │  (Normal / Warn / Emerg)  │
                         └─────────────┬─────────────┘
                                       │
                    ┌──────────────────┴──────────────────┐
                    ▼                                     ▼
      Local Actuators (Hardware)             Telemetry & Dispatcher
      ├── Green LED (Normal)                 ├── Wi-Fi / Serial Stream
      ├── Yellow LED (Warning / Fault)       ├── 16-Packet Offline Queue
      ├── Red LED (Emergency Strobe)         └── Laptop / Vercel Dashboard
      └── Active Buzzer (Chirp / Alarm)          ├── Live SVG Gauges
                                                 ├── 3D Vector & Sparklines
                                                 └── Incident Audit Log
```

---

## ⚡ Edge Decision Logic & Hazard Rules

The FRDM-RW612 microcontroller evaluates worker safety every **100 ms** against predefined industrial safety thresholds:

| Hazard Condition | Sensing Input | Edge Detection Rule | Local Actuator Response | Dashboard Event |
|---|---|---|---|---|
| **Manual SOS** | SW2 Pushbutton (`GPIO_24`) | Active-low button pressed (highest priority) | Red LED strobe (100 ms) + continuous rapid buzzer alarm | `MANUAL_SOS` (Immediate Emergency) |
| **Worker Fall** | MPU-6050 6-Axis IMU (`GPIO_16`/`17`) | Resultant acceleration $< 0.4\,\text{g}$ (free-fall) followed by $> 2.5\,\text{g}$ (impact) within $1.0\,\text{s}$ | Red LED strobe + emergency buzzer (5.0s latch) | `FALL_DETECTED` (Emergency) |
| **Critical Gas Leak / Smoke** | MQ-2 Sensor (`GPIO_60` LPADC) | Concentration $\ge 600\,\text{PPM}$ | Red LED strobe + emergency buzzer | `GAS_LEAK` (Critical Emergency) |
| **Elevated Gas Concentration**| MQ-2 Sensor (`GPIO_60` LPADC) | Concentration $\ge 300\,\text{PPM}$ and $< 600\,\text{PPM}$ | Solid Yellow LED + periodic 200 ms warning chirp | `GAS_LEAK` (Advisory Warning) |
| **High Thermal Hazard** | P3T1755 Sensor (`GPIO_16`/`17`) | Ambient temperature $\ge 50.0\,^{\circ}\text{C}$ | Red LED strobe + emergency buzzer | `HIGH_TEMP` (Emergency) |
| **Elevated Temperature** | P3T1755 Sensor (`GPIO_16`/`17`) | Ambient temperature $\ge 40.0\,^{\circ}\text{C}$ and $< 50.0\,^{\circ}\text{C}$ | Solid Yellow LED + periodic 200 ms warning chirp | `HIGH_TEMP` (Warning) |
| **Sensor Disconnect / Fault**| I²C / LPADC bus monitoring | Consecutive read failures $\ge 5$ | Pulsing Yellow LED (1 Hz: 500 ms ON / 500 ms OFF) + warning chirp | `SENSOR_FAULT` (Warning) |
| **Normal Operation** | All sensors | All parameters within safe limits | Solid Green LED ON, Yellow/Red OFF, Buzzer silent | `NORMAL` (Periodic Telemetry) |

---

## 🔌 Hardware Pinout & Breadboard Wiring Mapping

The breadboard prototype operates directly from the USB Type-C supply from the host laptop ($3.3\,\text{V}$ and $5.0\,\text{V}$ rails):

### 1. Power & Ground Distribution
| Power Rail | FRDM-RW612 Pin | Destination | Purpose |
|---|---|---|---|
| **3.3V DC** | Header `J4` Pin 4 | MPU-6050 `VCC` | Power for 3.3V MEMS motion sensor |
| **5.0V DC** | Header `J4` Pin 10 | MQ-2 `VCC` | Power for MQ-2 internal SnO₂ heater coil |
| **GND** | Header `J4` Pin 7 | Breadboard Common Ground Rail | Shared ground return for all components |

### 2. Sensor Interfacing
| Component | Module Pin | FRDM-RW612 Pin | Interface / Peripheral | Notes |
|---|---|---|---|---|
| **P3T1755** *(Onboard)* | - | `GPIO_16` (SDA), `GPIO_17` (SCL) | Flexcomm 2 I²C | Slave Address `0x48` |
| **MPU-6050** *(External)*| `SDA` | `GPIO_16` (Header `J5` Pin 9) | Flexcomm 2 I²C | Slave Address `0x68` (Shared bus) |
| | `SCL` | `GPIO_17` (Header `J5` Pin 10) | Flexcomm 2 I²C | Onboard 4.7kΩ pull-ups present |
| **MQ-2 Gas Sensor** | `AOUT` | `GPIO_60` (Arduino `A0` / `J3` Pin 2) | LPADC0 Channel 0 | 0 to 3.3V analog input |
| **SOS Button** *(Onboard)*| `SW2` | `GPIO_24` (`AON_WAKEUP`) | Digital Input | Active Low with internal pull-up |

### 3. Actuator Wiring
| Actuator | Module / Lead | FRDM-RW612 Pin | Protection | Behavior |
|---|---|---|---|---|
| **Normal LED (Green)** | Anode (+) | `GPIO_5` | $220\,\Omega$ Resistor in series | Solid ON in Normal state |
| **Warning LED (Yellow)**| Anode (+) | `GPIO_6` | $220\,\Omega$ Resistor in series | Solid ON in Warning; 1Hz pulse on Sensor Fault |
| **Emergency LED (Red)** | Anode (+) | `GPIO_7` | $220\,\Omega$ Resistor in series | 100 ms rapid strobe during Emergency |
| **All LED Cathodes** | Cathode (-) | Common `GND` | None | Direct connection to GND rail |
| **Active Buzzer Module**| `I/O` / `VCC` | `GPIO_4` | Integrated driver transistor | Active High trigger (3.3V drive) |
| | `GND` | Common `GND` | None | Direct connection to GND rail |

```text
       FRDM-RW612 BOARD                                   BREADBOARD
    ┌──────────────────────┐
    │                 3.3V ├────────────────────────────> MPU-6050 (VCC)
    │                 5.0V ├────────────────────────────> MQ-2 Gas (VCC)
    │                  GND ├────────────────────────────> COMMON GND RAIL
    │                      │
    │  [J5-Pin 9]  GPIO_16 ├────────────────────────────> MPU-6050 (SDA)
    │  [J5-Pin 10] GPIO_17 ├────────────────────────────> MPU-6050 (SCL)
    │  [J3-Pin 2]  GPIO_60 ├────────────────────────────> MQ-2 Gas (AOUT)
    │                      │
    │              GPIO_5  ├───[ 220 Ω ]───(>|)─────────> GREEN LED (Normal) ───┐
    │              GPIO_6  ├───[ 220 Ω ]───(>|)─────────> YELLOW LED (Warning)──┤
    │              GPIO_7  ├───[ 220 Ω ]───(>|)─────────> RED LED (Emergency)───┤
    │              GPIO_4  ────────────────[ BUZZER (+) ]─> ACTIVE BUZZER ──────┤
    │                      │                                                    │
    │  [Onboard SW2]GPIO_24│ (Internal SOS Push Button)                         │
    └──────────────────────┘                                                    │
                                                                   GND RAIL <───┘
```

---

## 🖥️ Mission Control Web Dashboard

The repository features a high-performance, dark SCADA-inspired monitoring dashboard built with **Python Flask / Vercel Serverless**, styled with responsive custom CSS and SVG visualizations with zero external runtime dependencies.

### Features
* **Master Status Banner**: High-visibility banner displaying system health with dynamic color transitions and pulsing siren animations during emergencies.
* **Ambient Temperature Widget**: Circular SVG radial gauge with dynamic arc fills, min/max tracking, and marker ticks at $40\,^{\circ}\text{C}$ and $50\,^{\circ}\text{C}$.
* **Motion & Impact Widget**: Real-time tri-axial acceleration bars ($X, Y, Z$), resultant magnitude gauge, impact signature trend sparkline, and persistent fall counter.
* **Toxic Gas & Smoke Meter**: Multi-zone linear gas meter (Safe $<300\,\text{PPM}$, Warning $300\text{--}599\,\text{PPM}$, Danger $\ge 600\,\text{PPM}$) with Air Quality Index classification.
* **Actuator Hardware Twin**: Live interactive digital twin mirroring the physical breadboard LEDs and active buzzer mode.
* **Incident & Audit Log**: Chronological tabular log of all state transitions and hazard events with real-time filtering, **CSV export**, and **JSON export**.
* **Browser Audio Synthesizer**: Web Audio API tone generator producing audible warning chirps and emergency sirens with an on-screen mute/unmute control.
* **Simulation Console**: 6 instant test triggers for academic demonstration (`Simulate SOS`, `Simulate Fall`, `High Temp`, `Gas Leak`, `Sensor Fault`, and `Clear/Reset`).

---

## 📂 Repository Structure

```text
industrial-worker-safety-rw612/
├── .gitignore                          # Git exclusion rules
├── .project / .cproject                # MCUXpresso IDE project configurations
├── CMakeLists.txt                      # CMake build configuration for Arm GCC
├── CMakePresets.json                   # Build presets
├── FRDM_RW612_WorkerSafety.mex         # MCUXpresso Config Tools (Pins, Clocks, Peripherals)
├── Procfile                            # Cloud process file for containerized hosts
├── README.md                           # Master project documentation
├── requirements.txt                    # Python dependencies for dashboard
├── vercel.json                         # Vercel serverless deployment routing
│
├── api/                                # Vercel Serverless Function
│   └── index.py                        # WSGI entrypoint for Vercel deployment
│
├── board/                              # FRDM-RW612 Board Support Package
│   ├── board.h / board.c               # Hardware definitions and board initialization
│   ├── clock_config.h / clock_config.c # 260MHz System PLL and Flexcomm clocking
│   ├── pin_mux.h / pin_mux.c           # GPIO directions and pin multiplexing
│   └── peripherals.h / peripherals.c   # Peripheral configuration stubs
│
├── dashboard/                          # Mission Control Web Dashboard
│   ├── dashboard_app.py                # Flask server, REST API & USB serial bridge
│   ├── requirements.txt                # Dashboard Python dependencies
│   ├── vercel.json                     # Standalone Vercel configuration
│   ├── static/                         # Frontend Static Assets
│   │   ├── safety-mark.svg             # Application brand icon
│   │   ├── css/dashboard.css           # Mission control dark theme stylesheet
│   │   └── js/dashboard.js             # Real-time state polling, audio & sparklines
│   └── templates/
│       └── index.html                  # Dashboard HTML5 template
│
├── device/                             # CMSIS Core & RW612 Definitions
│   ├── RW612.h                         # Device register mappings and IRQ channels
│   └── system_RW612.h / .c             # System core clock and initialization
│
├── docs/                               # Engineering Documentation & Academic Papers
│   ├── pinout_mapping.md               # Detailed breadboard wiring and pinout notes
│   ├── system_architecture.md          # Data pipeline and architectural block diagrams
│   ├── Industrial_Worker_Safety_Synopsis.docx # Project Synopsis document
│   └── paper/                          # IEEE Conference Paper files
│       ├── worker_safety_monitor_ieee_final.tex # IEEE conference paper LaTeX source
│       └── IEEEtran.cls                # IEEE LaTeX formatting class
│
├── drivers/                            # NXP MCUXpresso SDK Drivers
│   ├── fsl_clock.h                     # Clock gating driver
│   ├── fsl_common.h                    # SDK common definitions
│   ├── fsl_debug_console.h / .c        # Flexcomm UART debug console driver
│   └── fsl_i2c.h / .c                  # Flexcomm I²C master driver
│
└── source/                             # Application Firmware Source Code
    ├── main.c                          # Main execution loop & system sequencer
    ├── app_config.h                    # Safety thresholds, timing, and network constants
    ├── alerts/                         # Actuator Subsystems
    │   ├── alert_manager.h / .c        # Alert state machine and actuator coordinator
    │   ├── buzzer.h / .c               # Active buzzer driver (chirp & urgent pulses)
    │   ├── leds.h / .c                 # Status LEDs driver (Green, Yellow, Red)
    │   └── button_sos.h / .c           # SW2 SOS input interrupt & latching
    ├── edge_logic/                     # Edge Computing Engine
    │   └── safety_decision.h / .c      # Hazard classification & sensor fault logic
    ├── sensors/                        # Sensor Acquisition Drivers
    │   ├── sensor_temp.h / .c          # P3T1755 digital temperature driver (I2C)
    │   ├── sensor_mpu6050.h / .c       # MPU-6050 6-axis IMU & fall detection (I2C)
    │   └── sensor_gas.h / .c           # MQ-2 analog gas & smoke driver (LPADC)
    └── wireless/                       # Wireless Telemetry & Offline Buffering
        ├── alert_reporter.h / .c       # Telemetry serialization & 16-packet ring buffer
        └── wifi_manager.h / .c         # Wi-Fi link management and auto-reconnect
```

---

## 🚀 Getting Started

### 1. Firmware Setup (MCUXpresso IDE)
1. Install **MCUXpresso IDE** (v11.8+ recommended) with the **FRDM-RW612 SDK**.
2. Open MCUXpresso and select **File -> Import -> Existing Projects into Workspace**.
3. Choose the repository root directory.
4. Verify Wi-Fi network credentials in `source/app_config.h`:
   - `WIFI_SSID_DEFAULT`: `"realme P1 5G eb01"`
   - `WIFI_PASSWORD_DEFAULT`: `"yajjugowda"`
5. Connect the FRDM-RW612 board via the USB Type-C `MCU-Link` port.
6. Click **Build** and **Debug** to flash the firmware onto the board.
7. Open a serial terminal (115200 8N1) on the MCU-Link VCOM port to view real-time logs.

### 2. Forwarding Live USB Serial Telemetry to Dashboard
When the board is plugged in via USB, you can forward live readings directly to both your local dashboard and the Cloud Vercel Dashboard using the included bridge:

```bash
# 1-Click Windows Batch launcher:
.\start_serial_bridge.bat

# Or directly via Python:
python serial_bridge.py
```
The bridge automatically detects the MCU-Link COM port at 115200 baud, extracts structured `[TELEMETRY]` JSON messages, and forwards them in real time to `https://worker-safety-dashboard.vercel.app/api/telemetry` and `http://localhost:5000/api/telemetry`.

### 3. Local Dashboard Startup
```bash
# Navigate to the dashboard directory
cd dashboard

# (Optional) Create and activate virtual environment
python -m venv .venv
# Windows:
.venv\Scripts\Activate.ps1
# Linux / macOS:
source .venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# (Optional) Enable automatic USB MCU-Link serial port ingestion
$env:SERIAL_INGEST="1"

# Launch dashboard
python dashboard_app.py
```
Open your browser at **`http://localhost:5000`** (or port specified in `$env:PORT`).

### 4. Vercel Cloud Deployment
Deploy with a single command using the [Vercel CLI](https://vercel.com/cli):
```bash
npm install -g vercel
vercel
```
Vercel automatically reads `vercel.json` and serves the dashboard via serverless Python. Production URL: **`https://worker-safety-dashboard.vercel.app/`**

---

## 📡 REST API Specification

### `POST /api/telemetry`
Receives live telemetry packets from the FRDM-RW612 node:
```json
{
  "id": 1042,
  "temp": 28.45,
  "accel": 1.02,
  "gas": 48.0,
  "state": "NORMAL",
  "hazard": "NONE",
  "sos": false
}
```

### `GET /api/status`
Returns the complete snapshot of current metrics, actuator states, link status, and recent bounded event history.

### `POST /api/simulate`
For academic demonstration and verification:
```json
{
  "action": "fall"
}
```
*Supported actions:* `"sos"`, `"fall"`, `"high_temp"`, `"gas_leak"`, `"sensor_fault"`, `"normal"`.

---

## 👨‍🎓 Authors & Guidance

* **Submitted by**: **Yajnesh C B**
* **Degree**: Bachelor of Technology in Electronics and Communication Engineering
* **Academic Year**: 2026–2027
* **Platform**: NXP Semiconductors FRDM-RW612 Development Board

---

## 📜 References

1. NXP Semiconductors, *"FRDM-RW612 Development Board User Manual & Schematics"*, 2024.
2. InvenSense / TDK, *"MPU-6050 Six-Axis (Gyro + Accelerometer) MEMS MotionTracking Device Datasheet"*, Rev. 3.4.
3. NXP Semiconductors, *"P3T1755 $\pm 0.5\,^{\circ}\text{C}$ Accurate Digital Temperature Sensor with I3C/I2C Interface"*, 2023.
4. Winsen Electronics, *"MQ-2 Semiconductor Sensor for Combustible Gas and Smoke"*, Technical Manual.
5. A. K. Bourke et al., *"Evaluation of a threshold-based tri-axial accelerometer fall detection algorithm"*, *Gait & Posture*, vol. 26, no. 2, pp. 194–199, 2007.
