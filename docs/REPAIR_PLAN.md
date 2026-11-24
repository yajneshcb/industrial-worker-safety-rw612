# Project repair plan

## Intended system

The synopsis defines a locally autonomous FRDM-RW612 safety node: onboard P3T1755 temperature, external MPU-6050 motion, external MQ-2 gas sensor, an onboard SOS button, and local LEDs/buzzer. The existing web dashboard is the monitoring display. It should receive current telemetry from the node while continuing to show link loss clearly. The dashboard must not be the safety controller.

## Current implementation state

| Area | Repository evidence | State |
|---|---|---|
| Dashboard UI and HTTP API | `dashboard/` Flask app; 1-second status polling; bounded history and event log | Present; process memory is not shared across Vercel instances |
| USB telemetry path | Firmware prints `[TELEMETRY]` JSON; `serial_bridge.py` POSTs to dashboard | Implemented; transient local POST failures now retry |
| Temperature and motion | P3T1755 I2C at `0x48`; MPU-6050 I2C at `0x68` | Firmware drivers exist; physical operation still requires bench validation |
| Gas sensing | GAU ADC path on J7 pin 3/GPIO46 with 36 kΩ / 10 kΩ divider | Driver and explicit GPIO46 pad mux exist; the published value is an uncalibrated relative index, not PPM |
| Local alarms and SOS | GPIO outputs and SW2 handling | Firmware exists; confirm board routing and GPIO electrical limits at bench |
| Wireless telemetry | `WIFI_STACK_AVAILABLE` defaults to `0`; manager is a placeholder | Not implemented. Current working data transport is USB VCOM through the host bridge |
| Firmware build | CMake SDK project and prior `.bin` artifact | Cannot reproduce here: configured Arm GCC path is absent; no board-in-loop test available |

## Execution sequence

1. **Make the existing path repeatable:** launch the existing dashboard on the laptop, connect the MCU-Link VCOM bridge, and show packet age/link loss in the dashboard. Keep local dashboard delivery independent from optional cloud mirroring. This repository change adds retry on transient local HTTP errors.
2. **Correct hardware mapping and safe wiring:** use the attached Quick Start pinout: MPU SDA J2-18/GPIO16, SCL J2-20/GPIO17; MQ-2 AOUT J7-3/GPIO46 through a 36 kΩ / 10 kΩ divider; common GND; 3.3 V MPU supply and 5 V MQ-2 heater supply; external LEDs/buzzer at the documented GPIO headers. Never apply 5 V to the ADC input. This repository change corrects the conflicting bring-up pin references.
3. **Prove each sensor in isolation:** build/flash, verify I2C ACK and WHO_AM_I, inspect raw ADC counts and voltage at the pin, then check expected temperature and static motion values. Do not tune thresholds from a simulated dashboard.
4. **Validate safety and fault behavior:** test staged motion events, threshold boundaries, SOS, missing/disconnected sensors, and Wi-Fi/USB interruption. Verify local LEDs/buzzer continue when reporting is unavailable.
5. **Choose the transport for the demo:** until a real RW612 Wi-Fi client and server transport are implemented and tested, use USB VCOM + `serial_bridge.py` + the existing laptop dashboard. For wireless-only operation, implement and validate the SDK Wi-Fi stack, credentials, reachability, reconnection, and authenticated telemetry POST before claiming it works.
6. **Calibrate gas units:** characterize the actual MQ-2 module and load resistor against known concentrations; set R0/curve and then label the output. Until then present the output as a relative index and treat threshold levels as demonstration values.

## Bench acceptance checks

- The dashboard updates from a new hardware packet within two seconds and marks the node offline after five seconds without packets.
- At least 100 consecutive telemetry packets reach the existing dashboard without bridge parse errors; a short dashboard restart recovers automatically when it returns.
- Temperature and MPU values are plausible and stable at rest; the ADC reading tracks measured divider output without exceeding the ADC input rating.
- Each hazard drives the expected local actuator and dashboard state; sensor or network faults do not stop the local scheduler.
- A reproducible SDK build completes from a clean configure using the installed MCUXpresso SDK and Arm GNU toolchain.

## Remaining blockers

This workspace cannot establish that the electrical wiring, the ADC channel-to-header mapping, sensor calibration, or wireless operation works on the physical board. The configured Arm toolchain path in `mcux_include.local.json` is not present in this environment. Do not treat the existing prebuilt `.bin` as proof of the current source. The actual Wi-Fi transport and calibrated gas concentration remain unimplemented.
