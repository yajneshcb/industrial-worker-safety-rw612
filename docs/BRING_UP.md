# FRDM-RW612 Hardware Bring-Up & Diagnostics Guide

**Project:** Industrial Worker Safety Monitoring and Emergency Alert System  
**Target Board:** NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)  
**Firmware Architecture:** Freestanding MCUXpresso SDK 2.14+ Port  

---

## 1. Hardware Pinout & Wiring Checklist

Use the complete, labeled wire schedule in [pinout_mapping.md](pinout_mapping.md) for every connection. It follows the attached FRDM-RW612 Quick Start Guide and includes board header pins, GPIO names, suggested wire labels/colors, power rails, and the MQ-2 divider.

Firmware compilation and flashing are performed with the **MCUXpresso extension for VS Code**. Connect the FRDM-RW612 to the host at MCU-Link USB J10, build the project from the extension's project/Quickstart view, then use its debug/download action to program the board. For boot logs, open the extension's Serial Monitor on the MCU-Link VCOM port at 115200 baud. See [BUILD.md](BUILD.md) and [NXP's VS Code getting-started guide](https://www.nxp.com/document/guide/getting-started-with-frdm-rw612:GS-FRDM-RW612).

The table below summarizes principal sensor connections:

| **Component** | **FRDM-RW612 Header Pin** | **Official Header Label** | **Signal / GPIO** | **Notes** |
|---|---|---|---|---|
| **MPU-6050: VCC** | **Header J3 Pin 4** | `ARD 3V3` | `3V3` | 3.3V power |
| **MPU-6050: GND** | **Header J3 Pin 12 / 14** | `ARD GND` | `GND` | Ground rail |
| **MPU-6050: SCL** | **Header J2 Pin 20** | `ARD_D19` | `GPIO17` (FC2 I2C SCL) | Clock line (also on J7 Pin 6) |
| **MPU-6050: SDA** | **Header J2 Pin 18** | `ARD_D18` | `GPIO16` (FC2 I2C SDA) | Data line (also on J7 Pin 8) |
| **MPU-6050: AD0** | Common GND rail | `GND` | `GND` | Sets address to `0x68` |
| **MQ-2: VCC** | **Header J3 Pin 10** | `ARD 5V0` | `5V0` | 5.0V heater supply |
| **MQ-2: GND** | **Header J3 Pin 12 / 14** | `ARD GND` | `GND` | Ground rail |
| **MQ-2: AOUT** | **Header J7 Pin 3** | `LCD_MOSI` | `GPIO46` (ADC0 CH4) | Connect via 36k/10k divider |
| **Active Buzzer (+)** | **Header J1 Pin 1** | `GPIO4 (I2S)` | `GPIO_4` | Active HIGH 3.3V trigger |
| **Status LED: Normal (Green)** | **Header J1 Pin 7** | `GPIO5 (MCLK)` | `GPIO_5` | Active HIGH via 220 Ω resistor |
| **Status LED: Warning (Yellow)**| **Header J2 Pin 6** | `GPIO6 (ARD_D10)` | `GPIO_6` | Active HIGH via 220 Ω resistor |
| **Status LED: Emergency (Red)** | **Header J2 Pin 12** | `GPIO7 (ARD_D13)` | `GPIO_7` | Active HIGH via 220 Ω resistor |
| **MCU-Link Debug Console** | Onboard USB J10 | Flexcomm 3 USART | Board routed VCOM | 115200 baud, 8-N-1 |

---

## 2. MQ-2 Analog Voltage Divider Circuit

> [!CAUTION]
> **Do NOT connect MQ-2 AOUT directly to the FRDM-RW612 ADC pin!**  
> The MQ-2 heater runs on 5.0 V DC and its analog output swings up to ~5.0 V.  
> The RW612 GAU ADC reference is **1.2 V** (`kADC_Vref1P2V`).

### Resistor Divider Calculation:
- **$R_{top}$ (AOUT side):** $36\text{ k}\Omega$
- **$R_{bottom}$ (GND side):** $10\text{ k}\Omega$
- **Divider Ratio:**
  $$\text{Ratio} = \frac{R_{bottom}}{R_{top} + R_{bottom}} = \frac{10\text{ k}\Omega}{36\text{ k}\Omega + 10\text{ k}\Omega} = \frac{10}{46} \approx 0.2174$$
- When $V_{AOUT} = 5.0\text{ V}$, the pin voltage is:
  $$V_{pin} = 5.0\text{ V} \times 0.2174 = 1.087\text{ V} \quad (\le 1.2\text{ V safe range})$$
- When clean air is present ($V_{AOUT} \approx 0.9\text{--}1.0\text{ V}$), the pin voltage is $\approx 0.20\text{--}0.22\text{ V}$, staying safely above the GAU ADC $0.2\text{ V}$ minimum operating floor.

In `source/app_config.h`, the driver converts the measured pin voltage back through this ratio:
$$V_{AOUT} = \frac{V_{pin}}{\text{Ratio}}$$

---

**Gas unit limitation:** the current firmware does not produce calibrated gas PPM. `MQ2_R0_OHMS` is zero, so the published `gas` value is a relative 0–1000 voltage index. The 300/600 warning limits are demonstration index limits only. Calibrate against a known gas concentration before assigning PPM units or using readings for exposure decisions.

## 3. Power-On & Boot Diagnostics Sequence

When the firmware boots on the FRDM-RW612, it outputs a diagnostic report on the debug console (115200 baud):

```text
==================================================
Worker Safety: Temp (P3T1755) + Motion (MPU) + Gas (MQ-2)
==================================================
>> BOOT: SystemCoreClock=260000000 Hz, FC2_I2C_Clk=16000000 Hz, hardfaults=0, timebase=DWT running (exact 100ms/20ms timing)
>> BOOT: I2C 0x48 [P3T1755 (Onboard Temp)]: ACK - ONLINE (status=0)
>> BOOT: I2C 0x68 [MPU-6050 (6-Axis IMU)]: ACK - ONLINE (status=0)
>> BOOT: Sensor Fault Status -> TEMP:0 MPU:0 GAS:0

>> BOARD RUNNING: Multi-Rate Sensor & Safety Scheduler Starting...
>> Fast IMU Sampling: 50 Hz (20 ms) | Main Sensor/Logic: 10 Hz (100 ms)
```

### Diagnostic Checks:
1. **`SystemCoreClock`:** Confirms the core PLL RUN frequency (260 MHz).
2. **`FC2_I2C_Clk`:** Confirms Flexcomm 2 clock is running from SFRO at 16 MHz.
3. **`hardfaults`:** Must remain `0`. If a fault occurs, the handler logs diagnostic details and executes `NVIC_SystemReset()`.
4. **`timebase`:** Confirms the ARM Cortex-M33 DWT cycle counter is operational for jitter-free 20 ms / 100 ms scheduling.
5. **I2C ACK:** Both `0x48` (P3T1755) and `0x68` (MPU-6050) must report `ACK - ONLINE`. If `NO RESPONSE`, verify $V_{CC}$, GND, and pull-ups.

---

## 4. End-to-End System Testing

### Option A: Direct Browser Connection via Web Serial (Zero Backend Setup)
1. Open the dashboard in **Google Chrome** or **Microsoft Edge** (`http://localhost:5000`).
2. Click the **USB DIRECT** button in the top navigation bar.
3. Select your **MCU-Link VCOM** serial port in the browser prompt and click **Connect**.
4. Live telemetry will immediately stream at 115200 baud with real-time gauges, 3-axis motion bars, and hazard classifications.

### Option B: Python Serial Bridge + Local Dashboard
1. Run the Python serial bridge on the host laptop (with optional CSV logging):
```powershell
python serial_bridge.py --baud 115200 --port COM3 --csv telemetry_log.csv --raw
```
2. The bridge detects the MCU-Link COM port, parses `[TELEMETRY]` JSON lines, and dispatches them asynchronously to the local dashboard. Transient local HTTP failures are retried; optional cloud mirroring is best effort.

### Step 2: Mission Control Dashboard
In a separate terminal, launch the local dashboard:
```powershell
python dashboard/dashboard_app.py
```
Open `http://localhost:5000` in your web browser. You should observe live temperature, total acceleration, individual accelerometer axes (X, Y, Z), the MQ-2 relative output index, and real-time safety status.

### Step 3: Triggering Safety States
- **Normal State:** Green LED ON, buzzer silent.
- **High Temperature Warning:** Warm the P3T1755 above 40°C -> Yellow LED ON, periodic buzzer chirp.
- **Gas-index demonstration:** Use a safe, controlled test source and monitor the relative MQ-2 output index. Do not treat the 600 threshold as PPM or a certified alarm level.
- **Fall Detection:** Rapidly drop and catch the board (free-fall <0.4g followed by impact >2.5g) -> Emergency state latched, fall counter incremented.
- **Manual SOS:** Press onboard button SW2 -> Emergency state triggered with 5.0 s latch hold.
