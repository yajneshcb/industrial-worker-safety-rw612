# FRDM-RW612 Official Hardware Pinout & Wiring Mapping

This document details the exact physical header pins, GPIOs, and signal labels for the **NXP FRDM-RW612** board (RW612ETA2I), directly verified against the official **NXP Quick Start Guide FRDM-RW612**.

---

## 1. Power & Ground Header References (Header J3 & J2)

From the official Quick Start Guide Header J3 / J2 diagram:

| Voltage Rail | Header & Pin Number | Official Header Label | Use Case / Target Module |
|---|---|---|---|
| **3.3 V Power** | **Header J3 Pin 4** | `3V3` (ARD 3V3) | MPU-6050 VCC |
| **3.3 V (Alt)** | **Header J3 Pin 8** or **Header J2 Pin 16** | `3V3` (ARD 3V3) | Alternative 3.3V power source |
| **5.0 V Power** | **Header J3 Pin 10** | `5V0` (ARD 5V0) | **MQ-2 VCC (Heater power supply)** |
| **5.0 V (Alt)** | **MikroBUS J5 Pin 7** | `5v0` | Alternative 5.0V power source |
| **GND** | **Header J3 Pin 12** or **Pin 14** | `GND` (ARD GND) | Main Breadboard Common GND Rail |
| **GND (Alt)** | **Header J2 Pin 14** or **J7 Pin 9/10** | `GND` | Ground return |

> [!CAUTION]
> **Do NOT use J4-4 for power:** On Header J4, Pin 4 is `GPIO43` (`ARD_A1` / DAC Analog Output). Always take 3.3V from **Header J3 Pin 4** and 5.0V from **Header J3 Pin 10**.

---

## 2. Sensor Connections (MPU-6050, MQ-2, P3T1755)

| Sensor Pin | FRDM-RW612 Header & Pin | GPIO / Signal | Official Quick Start Guide Label | Wiring & Circuit Details |
|---|---|---|---|---|
| **MPU-6050 VCC** | **Header J3 Pin 4** | `3V3` | `ARD 3V3` | Connect directly to 3.3V |
| **MPU-6050 GND** | **Header J3 Pin 12** or **14** | `GND` | `ARD GND` | Connect to Common GND rail |
| **MPU-6050 SCL** | **Header J2 Pin 20** | `GPIO17` | `ARD_D19` (`I2C_SCL`) | Flexcomm 2 I²C Clock *(also on J7 Pin 6)* |
| **MPU-6050 SDA** | **Header J2 Pin 18** | `GPIO16` | `ARD_D18` (`I2C_SDA`) | Flexcomm 2 I²C Data *(also on J7 Pin 8)* |
| **MPU-6050 AD0** | **GND rail** | `GND` | Ground | Pull to GND to set I²C address to `0x68` |
| **MPU-6050 INT** | *Unconnected* | — | — | Not needed (polled at 50 Hz in firmware) |
| **MQ-2 VCC** | **Header J3 Pin 10** | `5V0` | `ARD 5V0` | Connect directly to 5.0V for internal heater |
| **MQ-2 GND** | **Header J3 Pin 12** or **14** | `GND` | `ARD GND` | Connect to Common GND rail |
| **MQ-2 DOUT** | *Unconnected* | — | — | Digital threshold comparator not used |
| **MQ-2 AOUT** | **Header J7 Pin 3** | `GPIO46` | `LCD_MOSI` (GAU GPADC0 CH4) | **Connect via Voltage Divider:**<br>1. MQ-2 `AOUT` $\rightarrow$ **$36\text{ k}\Omega$ resistor**<br>2. Other side of $36\text{ k}\Omega$ resistor $\rightarrow$ **Header J7 Pin 3**<br>3. Connect a **$10\text{ k}\Omega$ resistor** from **Header J7 Pin 3** to **GND** |
| **P3T1755 (Temp)** | *Onboard* | `GPIO16`/`17` | `I2C2` (`0x48`) | **No external wiring.** Hardwired internally. |
| **SW2 SOS Button** | *Onboard* | `GPIO11` | `SW2` | **No external wiring.** Built-in push button. |

---

## 3. Local Alert Actuators (Buzzer & LEDs)

| Actuator | Anode (+) / Signal Pin | Cathode (-) / GND Pin | Official Header & Pin | Official Quick Start Guide Label | Resistor / Mode |
|---|---|---|---|---|---|
| **Active Buzzer** | Header **J1 Pin 1** | Common GND rail | `GPIO4` | `GPIO4 / I2S` | Active HIGH 3.3V trigger |
| **Green LED (Normal)** | Header **J1 Pin 7** | Common GND rail | `GPIO5` | `GPIO5 / MCLK` | In series with **$220\ \Omega$** resistor |
| **Yellow LED (Warning)** | Header **J2 Pin 6** | Common GND rail | `GPIO6` | `GPIO6 / ARD_D10` | In series with **$220\ \Omega$** resistor |
| **Red LED (Emergency)** | Header **J2 Pin 12** | Common GND rail | `GPIO7` | `GPIO7 / ARD_D13` | In series with **$220\ \Omega$** resistor |

---

## 4. Header Visual Reference Summary

### Header J3 (Power Header)
```
          (Inner Column)             (Outer Column / Edge)
 Pin  1 [ NC ]                    Pin  2 [ NC ]
 Pin  3 [ NC ]                    Pin  4 [ 3V3 (ARD 3V3) ]       <-- MPU-6050 VCC
 Pin  5 [ NC ]                    Pin  6 [ RESET (ARD RESET) ]
 Pin  7 [ NC ]                    Pin  8 [ 3V3 (ARD 3V3) ]
 Pin  9 [ NC ]                    Pin 10 [ 5V0 (ARD 5V0) ]       <-- MQ-2 VCC (5V)
 Pin 11 [ NC ]                    Pin 12 [ GND (ARD GND) ]       <-- Common GND
 Pin 13 [ NC ]                    Pin 14 [ GND (ARD GND) ]       <-- Common GND
 Pin 15 [ NC ]                    Pin 16 [ VIN (ARD VIN) ]
```

### Header J7 (PMOD Header)
```
          (Column 1 - Odd)           (Column 2 - Even)
 Pin  1 [ GPIO49 / LCD_CSn  ]     Pin  2 [ GPIO18 / TP_INTn ]
 Pin  3 [ GPIO46 / LCD_MOSI ]     Pin  4 [ GPIO44 / LCD_RSTn ]   <-- MQ-2 AOUT (after divider)
 Pin  5 [ GPIO47 / LCD_D/C  ]     Pin  6 [ GPIO17 / I2C_SCL  ]   <-- Alt MPU-6050 SCL
 Pin  7 [ GPIO48 / LCD_SCLK ]     Pin  8 [ GPIO16 / I2C_SDA  ]   <-- Alt MPU-6050 SDA
 Pin  9 [ GND               ]     Pin 10 [ GND               ]
 Pin 11 [ 3V3               ]     Pin 12 [ 3V3               ]
```

### Header J1 (Lower Right Arduino / Digital Header)
```
          (Inner Column - Odd)       (Outer Column - Even)
 Pin  1 [ GPIO4 (I2S)  ]          Pin  2 [ GPIO9  (ARD_D0) ]     <-- Buzzer (+)
 Pin  3 [ GPIO3 (I2S)  ]          Pin  4 [ GPIO8  (ARD_D1) ]
 Pin  5 [ GPIO2 (I2S)  ]          Pin  6 [ GPIO11 (ARD_D2) ]
 Pin  7 [ GPIO5 (MCLK) ]          Pin  8 [ GPIO15 (ARD_D3) ]     <-- Green LED (+)
 Pin  9 [ GPIO7 (I2S)  ]          Pin 10 [ GPIO18 (ARD_D4) ]
 Pin 11 [ NC           ]          Pin 12 [ GPIO27 (ARD_D5) ]
 Pin 13 [ GPIO8 (I2S)  ]          Pin 14 [ GPIO0  (ARD_D6) ]
 Pin 15 [ GPIO9 (I2S)  ]          Pin 16 [ GPIO20 (ARD_D7) ]
```

### Header J2 (Upper Right Arduino / Digital Header)
```
          (Inner Column - Odd)       (Outer Column - Even)
 Pin  1 [ NC ]                    Pin  2 [ GPIO50 (ARD_D8)  ]
 Pin  3 [ NC ]                    Pin  4 [ GPIO52 (ARD_D9)  ]
 Pin  5 [ NC ]                    Pin  6 [ GPIO6  (ARD_D10) ]    <-- Yellow LED (+)
 Pin  7 [ NC ]                    Pin  8 [ GPIO9  (ARD_D11) ]
 Pin  9 [ NC ]                    Pin 10 [ GPIO8  (ARD_D12) ]
 Pin 11 [ NC ]                    Pin 12 [ GPIO7  (ARD_D13) ]    <-- Red LED (+)
 Pin 13 [ NC ]                    Pin 14 [ GND    (ARD GND) ]    <-- Common GND
 Pin 15 [ NC ]                    Pin 16 [ 3V3    (ARD 3V3) ]    <-- 3.3V
 Pin 17 [ NC ]                    Pin 18 [ GPIO16 (ARD_D18) ]    <-- MPU-6050 SDA
 Pin 19 [ NC ]                    Pin 20 [ GPIO17 (ARD_D19) ]    <-- MPU-6050 SCL
```
