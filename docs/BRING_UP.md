# FRDM-RW612 Hardware Bring-Up & Diagnostics Guide

**Project:** Industrial Worker Safety Monitoring and Emergency Alert System  
**Target Board:** NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)  
**Firmware Architecture:** Freestanding MCUXpresso SDK 2.14+ Port  

---

## 1. Hardware Pinout & Wiring Checklist

Before powering up the board, cross-check the physical wiring against the following verified pin mappings:

| Subsystem / Sensor | Board Pin / Header | Signal / Mode | Electrical & Circuit Protection |
|---|---|---|---|
| **Digital Temperature (P3T1755)** | Onboard | Flexcomm 2 I2C (`0x48`) | SDA: `GPIO_16`, SCL: `GPIO_17` (Onboard pull-ups) |
| **Motion & Fall (MPU-6050)** | J5 Pin 9 (SDA), J5 Pin 10 (SCL) | Flexcomm 2 I2C (`0x68`) | Powered from 3.3V (J4 Pin 4) & GND (J4 Pin 7) |
| **Toxic Gas & Smoke (MQ-2)** | **J7 Pin 3 (GPIO_46)** | **GAU GPADC0 Channel CH4** | **CRITICAL: Resistor divider required!** (See Section 2) |
| **Emergency SOS Button (SW2)** | **Onboard SW2 (GPIO_11)** | Digital Input, Port 0 Pin 11 | Active LOW, falling-edge interrupt (`GPIO_INTA_IRQn`) |
| **Active Buzzer Actuator** | Header D4 (`GPIO_4`) | Digital Output, Port 0 Pin 4 | Active HIGH 3.3V trigger |
| **Status LED: Normal (Green)** | Header D5 (`GPIO_5`) | Digital Output, Port 0 Pin 5 | Active HIGH via 220 Ω current limiting resistor |
| **Status LED: Warning (Yellow)**| Header D6 (`GPIO_6`) | Digital Output, Port 0 Pin 6 | Active HIGH via 220 Ω current limiting resistor |
| **Status LED: Emergency (Red)** | Header D7 (`GPIO_7`) | Digital Output, Port 0 Pin 7 | Active HIGH via 220 Ω current limiting resistor |
| **MCU-Link Debug Console** | Onboard Virtual COM port | Flexcomm 3 USART (`GPIO_24`)| 115200 baud, 8-N-1, no parity |

---

## 2. MQ-2 Analog Voltage Divider Circuit

> [!CAUTION]
> **Do NOT connect MQ-2 AOUT directly to the FRDM-RW612 ADC pin!**  
> The MQ-2 heater runs on 5.0 V DC and its analog output swings up to ~5.0 V.  
> The RW612 GAU ADC reference is **1.2 V** (`kADC_Vref1P2V`).

### Resistor Divider Calculation:
- **$R_{top}$ (AOUT side):** $47\text{ k}\Omega$
- **$R_{bottom}$ (GND side):** $10\text{ k}\Omega$
- **Divider Ratio:**
  $$\text{Ratio} = \frac{R_{bottom}}{R_{top} + R_{bottom}} = \frac{10\text{ k}\Omega}{47\text{ k}\Omega + 10\text{ k}\Omega} = \frac{10}{57} \approx 0.1754$$
- When $V_{AOUT} = 5.0\text{ V}$, the pin voltage is:
  $$V_{pin} = 5.0\text{ V} \times 0.1754 = 0.877\text{ V} \quad (\le 1.2\text{ V safe range})$$

In `source/app_config.h`, the driver converts the measured pin voltage back through this ratio:
$$V_{AOUT} = \frac{V_{pin}}{\text{Ratio}}$$

---

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
1. Open the dashboard in **Google Chrome** or **Microsoft Edge** (`http://localhost:5000` or deployed Vercel URL).
2. Click the **USB DIRECT** button in the top navigation bar.
3. Select your **MCU-Link VCOM** serial port in the browser prompt and click **Connect**.
4. Live telemetry will immediately stream at 115200 baud with real-time gauges, 3-axis motion bars, and hazard classifications.

### Option B: Python Serial Bridge + Local Dashboard
1. Run the Python serial bridge on the host laptop (with optional CSV logging):
```powershell
python serial_bridge.py --baud 115200 --port COM3 --csv telemetry_log.csv --raw
```
2. The bridge detects the MCU-Link COM port, parses `[TELEMETRY]` JSON lines, and dispatches them asynchronously via HTTP POST to the local dashboard.

### Step 2: Mission Control Dashboard
In a separate terminal, launch the local dashboard:
```powershell
python dashboard/dashboard_app.py
```
Open `http://localhost:5000` in your web browser. You should observe live temperature, total acceleration, individual accelerometer axes (X, Y, Z), gas concentration (PPM), and real-time safety status.

### Step 3: Triggering Safety States
- **Normal State:** Green LED ON, buzzer silent.
- **High Temperature Warning:** Warm the P3T1755 above 40°C -> Yellow LED ON, periodic buzzer chirp.
- **Gas Emergency:** Expose MQ-2 to butane/smoke (>600 PPM) -> Red LED ON, rapid buzzer alarm, dashboard emergency alert.
- **Fall Detection:** Rapidly drop and catch the board (free-fall <0.4g followed by impact >2.5g) -> Emergency state latched, fall counter incremented.
- **Manual SOS:** Press onboard button SW2 -> Emergency state triggered with 5.0 s latch hold.
