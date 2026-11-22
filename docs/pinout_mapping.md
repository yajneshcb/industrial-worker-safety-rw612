# FRDM-RW612 Hardware Pinout & Wiring Mapping

This document details the physical and peripheral mapping between the **NXP FRDM-RW612** board (RW612ETA2I) and external sensors/actuators.

---

## 1. Onboard Components

| Feature / Component | Peripheral / Signal | Pin / GPIO | Notes |
|---|---|---|---|
| **Digital Temp Sensor (P3T1755)** | Flexcomm 2 I2C SDA | `GPIO_16` | Onboard I2C sensor (Slave Addr `0x48`) |
| **Digital Temp Sensor (P3T1755)** | Flexcomm 2 I2C SCL | `GPIO_17` | Onboard I2C sensor clock |
| **SOS Emergency Button (SW2)** | Digital Input | `GPIO_11` (Port 0 Pin 11) | Active LOW, falling-edge interrupt (`GPIO_INTA_IRQn`), internal pull-up |
| **MCU-Link Debug Console** | Flexcomm 3 USART DATA | `GPIO_24` | Virtual COM port via USB MCU-Link (115200 baud) |

---

## 2. External Sensors (Breadboard Assembly)

| External Module Pin | FRDM-RW612 Pin / Header | Function / Interface | Notes |
|---|---|---|---|
| **MPU-6050 VCC** | `3.3V` (Header J4 Pin 4) | Power supply | 3.3V DC power rail |
| **MPU-6050 GND** | `GND` (Header J4 Pin 7) | Ground reference | Common system GND |
| **MPU-6050 SDA** | `GPIO_16` (Header J5 Pin 9) | Flexcomm 2 I2C SDA | Addr `0x68` (Shares bus with onboard P3T1755 at `0x48`) |
| **MPU-6050 SCL** | `GPIO_17` (Header J5 Pin 10)| Flexcomm 2 I2C SCL | Shared I2C clock (100 kHz) |
| **MQ-2 VCC** | `5V` (Header J4 Pin 10) | Heater Power | 5.0V DC recommended for SnO2 internal heater element |
| **MQ-2 GND** | `GND` (Header J4 Pin 7) | Ground reference | Common system GND |
| **MQ-2 AOUT** | **Header J7 Pin 3 (`GPIO_46`)** | **GAU GPADC0 Channel CH4** | **Requires 47k/10k resistor divider** (0–1.2V ADC input range). 47k between AOUT and pin, 10k between pin and GND. |

---

## 3. Local Alert Actuators (Breadboard Assembly)

| Actuator Pin | FRDM-RW612 Signal / Header | Direction / Mode | Circuit Protection & Connection |
|---|---|---|---|
| **Active Buzzer VCC (+)** | `GPIO_4` (Header J1 Pin 1 / Arduino D4) | Digital Output (Port 0 Pin 4) | Active HIGH 3.3V trigger (configurable via `BUZZER_ACTIVE_LOW`) |
| **Normal LED (Green) Anode (+)** | `GPIO_5` (Header J1 Pin 3 / Arduino D5) | Digital Output (Port 0 Pin 5) | Connect via 220 Ω resistor in series to GPIO_5 |
| **Warning LED (Yellow) Anode (+)**| `GPIO_6` (Header J2 Pin 6* / Arduino D6) | Digital Output (Port 0 Pin 6) | Connect via 220 Ω resistor in series to GPIO_6 |
| **Emergency LED (Red) Anode (+)** | `GPIO_7` (Header J2 Pin 12* / Arduino D7)| Digital Output (Port 0 Pin 7) | Connect via 220 Ω resistor in series to GPIO_7 |
| **All LED Cathodes (-)** | `GND` (Header J2-14 or J3-14) | Power Return | Connect directly to Common GND rail |
| **Active Buzzer GND (-)** | `GND` (Header J2-14 or J3-14) | Ground Reference | Connect directly to Common GND rail |

> [!WARNING]
> **Pin Silk / Schematic Check for J2-6 and J2-12**:
> On FRDM-RW612 board revisions, `GPIO_6` and `GPIO_7` map to Arduino headers D6 and D7 (Header J2 Pins 6 and 12). On some revisions, Flexcomm 1 SPI routing can alternate J2-6 between GPIO_6 and GPIO_7. Check the board silkscreen before wiring external LEDs, and swap the yellow and red LED jumpers if the colors appear inverted during boot test.
