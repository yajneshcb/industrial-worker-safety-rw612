# FRDM-RW612 Hardware Pinout & Wiring Mapping

This document details the hardware mapping between the **NXP FRDM-RW612** board and peripheral sensors/actuators based on the project synopsis.

---

## 1. Onboard Components

| Feature / Component | Peripheral / Signal | Pin / GPIO | Notes |
|---|---|---|---|
| **Digital Temp Sensor (P3T1755)** | Flexcomm 2 I2C SDA | `GPIO_16` | Onboard I2C sensor (Slave Addr `0x48`) |
| **Digital Temp Sensor (P3T1755)** | Flexcomm 2 I2C SCL | `GPIO_17` | Onboard I2C sensor clock |
| **SOS Emergency Button (SW2)** | Digital Input | `GPIO_24` (`AON_WAKEUP`) | Active Low, internal pull-up enabled |
| **MCU-Link Debug UART (TX)** | Flexcomm 3 USART TX | `GPIO_26` | Virtual COM port to Laptop |
| **MCU-Link Debug UART (RX)** | Flexcomm 3 USART RX | `GPIO_27` | Virtual COM port to Laptop |

---

## 2. External Sensors (Breadboard Assembly)

| External Module Pin | FRDM-RW612 Pin / Header | Function / Interface | Notes |
|---|---|---|---|
| **MPU-6050 VCC** | `3.3V` (J4 Pin 4) | Power supply | 3.3V DC |
| **MPU-6050 GND** | `GND` (J4 Pin 7) | Ground reference | Common system GND |
| **MPU-6050 SDA** | `GPIO_16` (Header J5 Pin 9) | Flexcomm 2 I2C SDA | Addr `0x68` (Shares bus with P3T1755 at `0x48`) |
| **MPU-6050 SCL** | `GPIO_17` (Header J5 Pin 10)| Flexcomm 2 I2C SCL | Onboard pull-ups already present |
| **MQ-2 VCC** | `5V` (J4 Pin 10) or `3.3V` | Heater / Module Power| 5.0V recommended for optimal SnO2 heating |
| **MQ-2 GND** | `GND` (J4 Pin 7) | Ground reference | Common system GND |
| **MQ-2 AOUT** | `GPIO_60` (Arduino A0 / J3 Pin 2) | LPADC0 Channel 0 | 0 to 3.3V analog input range |

---

## 3. Local Alert Actuators (Breadboard Assembly)

| Actuator Pin | FRDM-RW612 Pin | Direction / Mode | Wiring / Resistor Protection |
|---|---|---|---|
| **Normal LED (Green) Anode (+)** | `GPIO_5` | Digital Output | Connect via 220 Ω resistor in series to GPIO_5 |
| **Warning LED (Yellow) Anode (+)**| `GPIO_6` | Digital Output | Connect via 220 Ω resistor in series to GPIO_6 |
| **Emergency LED (Red) Anode (+)** | `GPIO_7` | Digital Output | Connect via 220 Ω resistor in series to GPIO_7 |
| **All LED Cathodes (-)** | `GND` | Power Return | Connect directly to Common GND rail |
| **Active Buzzer VCC (+)** | `GPIO_4` | Digital Output | Active High trigger (integrated transistor on module)|
| **Active Buzzer GND (-)** | `GND` | Ground Reference | Connect directly to Common GND rail |

