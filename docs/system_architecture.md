# System Architecture & Edge Processing Pipeline

## 1. Overview
The **Industrial Worker Safety Monitoring and Emergency Alert System** utilizes an edge-processing paradigm on the **NXP FRDM-RW612** microcontroller. Sensor data is evaluated locally in real time to generate immediate acoustic/visual alerts independently of cloud or Wi-Fi availability.

```
       LAPTOP (USB Power + Debug)
                  │
                  ▼
         ┌───────────────────┐
         │    FRDM-RW612     │
         │                   │
         │ Built-in Temp SNS │
         │   Edge Decision   │
         └─────────┬─────────┘
                   │
    ┌──────────────┼──────────────┐
    ▼              ▼              ▼
 MPU6050       MQ-2 Gas       User Button
Motion / Fall   Hazard           (SOS)
    │              │              │
    └──────────────┼──────────────┘
                   ▼
          Safety Decision Logic
                   │
         ┌─────────┴─────────┐
         ▼                   ▼
      BUZZER               LEDS
         │
         ▼
     Wi-Fi / BLE
         │
         ▼
  Laptop Dashboard
         │
         ▼
 Safety Status / Log
```

---

## 2. Processing Pipeline

1. **Acquisition Stage:**
   - Polling of P3T1755 temperature sensor via Flexcomm 2 I2C.
   - Polling of MPU-6050 accelerometer & gyroscope via Flexcomm 0 I2C.
   - Sampling of MQ-2 gas concentration via LPADC.
   - Interrupt/latch monitoring of SW2 user push button (SOS).

2. **Edge Decision Stage:**
   - Onboard thresholds evaluation:
     - Free-fall signature + impact spike = Fall Detected.
     - Gas PPM > Warning / Emergency thresholds = Gas Hazard.
     - Temperature > Critical threshold = Heat Hazard.
     - SW2 trigger = Immediate Manual SOS.
   - Health check: Detect sensor disconnect/fault.

3. **Autonomous Local Alerting:**
   - Green LED: System Normal.
   - Yellow LED + intermittent buzzer: Warning state.
   - Red LED + continuous alarm: Emergency state.

4. **Wireless Reporting & Fault-Tolerant Queuing:**
   - Telemetry dispatched over Wi-Fi / BLE to laptop dashboard.
   - If wireless connection drops, local detection and alerting operate without pause; offline messages are buffered and flushed upon reconnect.

