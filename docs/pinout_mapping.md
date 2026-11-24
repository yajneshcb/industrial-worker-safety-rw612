# FRDM-RW612 worker-safety wiring and labels

This is the wiring reference for the project. It follows the attached FRDM-RW612 Quick Start Guide pinout and NXP's board/ADC documentation. Board header pins are written as **Jx-pin** (for example `J2-18`); GPIO numbers are not header pin numbers.

## Before wiring

- Power the FRDM-RW612 from its MCU-Link USB connector, **J10**.
- Power the MPU-6050 from 3.3 V. Power the MQ-2 heater/module from 5 V only if the module is rated for 5 V. Connect all module grounds to the same board ground.
- The MQ-2 analog output must pass through the divider below before it reaches the RW612 ADC. Never wire the sensor's raw AOUT directly to J7-3.
- The MQ-2 firmware value is currently a relative output index, not calibrated PPM. Its 300/600 thresholds are demonstration values only.
- GPIO outputs are logic signals. Check the buzzer module's voltage and current; use a transistor/MOSFET driver for a load that exceeds the MCU GPIO ratings. Do not power a high-current buzzer from a GPIO.

## Physical wiring schedule

Add the listed label to **both ends** of each jumper. Wire colors below are suggestions; the printed labels are authoritative.

| Wire ID / label | From | To | Suggested color | Notes |
|---|---|---|---|---|
| `PWR-3V3-MPU` | Board J3-4 (`3V3`, alternatively J2-16) | MPU-6050 `VCC` | Red | Use 3.3 V only. |
| `GND-COM` | Board J3-12 or J3-14 (`GND`) | Breadboard ground rail | Black | Connect MPU-6050 GND, MQ-2 GND, and LED returns to this rail. |
| `MPU-SDA` | Board J2-18 (`GPIO16`, `ARD_D18`, I2C SDA); alternatively J7-8 | MPU-6050 `SDA` | Blue | Shared I2C bus with onboard P3T1755. Do not connect both alternate header locations to different nets. |
| `MPU-SCL` | Board J2-20 (`GPIO17`, `ARD_D19`, I2C SCL); alternatively J7-6 | MPU-6050 `SCL` | Yellow | Shared I2C bus with onboard P3T1755. |
| `MPU-AD0` | MPU-6050 `AD0` | `GND-COM` | Black | Sets expected I2C address to `0x68`. |
| `PWR-5V-MQ2` | Board J3-10 (`5V0`, `ARD 5V0`) | MQ-2 module `VCC` | Orange | Verify the module accepts 5 V. The MQ-2 heater can draw substantially more current than a logic sensor. |
| `MQ2-GND` | MQ-2 module `GND` | `GND-COM` | Black | Common ground is required for analog measurement. |
| `MQ2-AOUT-5V` | MQ-2 module `AOUT` | Divider top resistor, 36 kΩ | Purple | Divider input. Do not connect directly to the MCU. |
| `MQ2-AOUT-ADC` | Junction of 36 kΩ and 10 kΩ resistors | Board J7-3 (`GPIO46`, `LCD_MOSI`, ADC0 channel 4) | Violet | ADC node. Firmware selects GAU GPADC0 channel 4. |
| `MQ2-DIV-GND` | Divider bottom resistor, 10 kΩ | `GND-COM` | Black | Completes divider. |
| `BUZZER-VCC` | Board rail matching buzzer module rating | Active buzzer module `VCC` (if present) | Per module | Check the module's allowed supply voltage; do not assume it accepts 5 V. |
| `BUZZER-SIG` | Board J1-1 (`GPIO4`, `I2S`) | Active buzzer module `SIG` / driver input | White | Firmware uses active-high. If using a bare buzzer or unknown-current module, add a suitable transistor driver; do not assume it is safe to drive directly. |
| `BUZZER-GND` | Active buzzer module `GND` | `GND-COM` | Black | Common return. |
| `LED-NORMAL` | Board J1-7 (`GPIO5`, `MCLK`) | Green LED anode through 220 Ω series resistor | Green | LED cathode to `GND-COM`. |
| `LED-WARNING` | Board J2-6 (`GPIO6`, `ARD_D10`) | Yellow LED anode through 220 Ω series resistor | Yellow | LED cathode to `GND-COM`. |
| `LED-EMERGENCY` | Board J2-12 (`GPIO7`, `ARD_D13`) | Red LED anode through 220 Ω series resistor | Red | LED cathode to `GND-COM`. |

The onboard P3T1755 temperature sensor is on the shared I2C bus at `0x48`; it needs no external wiring. The onboard SW2 SOS button is connected to GPIO11 and needs no external jumper. Firmware treats the button as active-low. Do not confuse SW2 with the reset button SW1.

## MQ-2 divider

Wire this series/shunt divider:

```text
MQ-2 AOUT ── 36 kΩ ──┬── J7-3 / GPIO46 / ADC0 channel 4
                      └── 10 kΩ ── GND-COM
```

With a 5.0 V AOUT maximum, the ideal divider output is about 1.09 V (`5 × 10 / (36 + 10)`), below the configured 1.2 V ADC reference. NXP's FRDM-RW612 ADC example identifies J7-3/GPIO46 and notes the analog input should be above 0.2 V for that example. Confirm the actual module AOUT range and measure the divider node before attaching it to the board. The divider does not calibrate the MQ-2 concentration.

## Header quick reference

| Header | Pin | Board label | Project use |
|---|---:|---|---|
| J3 | 4 | 3V3 / ARD 3V3 | MPU-6050 supply |
| J3 | 10 | 5V0 / ARD 5V0 | MQ-2 module supply |
| J3 | 12, 14 | GND / ARD GND | Common ground |
| J2 | 18 | GPIO16 / ARD_D18 | I2C SDA |
| J2 | 20 | GPIO17 / ARD_D19 | I2C SCL |
| J7 | 3 | GPIO46 / LCD_MOSI | MQ-2 divided AOUT / ADC0 CH4 |
| J7 | 6 | GPIO17 / I2C_SCL | Alternate SCL connection |
| J7 | 8 | GPIO16 / I2C_SDA | Alternate SDA connection |
| J1 | 1 | GPIO4 / I2S | Buzzer driver signal |
| J1 | 7 | GPIO5 / MCLK | Green LED |
| J2 | 6 | GPIO6 / ARD_D10 | Yellow LED |
| J2 | 12 | GPIO7 / ARD_D13 | Red LED |

## Build and flash workflow

This project uses the **MCUXpresso extension for VS Code** for firmware compilation and flashing. Connect the board to the host at MCU-Link USB J10. In VS Code, use the MCUXpresso extension's Quickstart/Projects view to select the project and build it; then start the extension's debug/download action to program the connected board. Open the extension's Serial Monitor on the MCU-Link VCOM port at 115200 baud to read boot diagnostics and `[TELEMETRY]` lines.

The host machine compiling firmware needs the RW612 MCUXpresso SDK and the extension's supported Arm GNU toolchain/debug tools. The dashboard-only software validation does not need the SDK. See [NXP's FRDM-RW612 VS Code getting-started guide](https://www.nxp.com/document/guide/getting-started-with-frdm-rw612:GS-FRDM-RW612) for extension setup and the Build and Flash flow.

## Firmware/hardware caveats

- Current firmware telemetry is available over USB serial. The firmware Wi-Fi manager is still a placeholder; the board does not yet POST telemetry to the dashboard over Wi-Fi.
- The ADC pin and divider are mapped in source, but the full sensor-to-dashboard path still needs to be validated on a real board.
- `gas` is an uncalibrated relative index while `MQ2_R0_OHMS` is zero. Do not call it PPM or use it as a certified gas alarm.

## References

- [NXP FRDM-RW612 Quick Start Guide](https://www.nxp.com/docs/en/quick-reference-guide/CS_FRDM-RW612-QSG-LR.pdf)
- [NXP MCUXpresso SDK RW612 ADC software-trigger example](https://mcuxpresso.nxp.com/mcuxsdk/26.03.00-pvw1/html/examples/_boards/frdmrw612/driver_examples/adc/software_trigger/example_board_readme.html)
- [NXP RW612 data sheet](https://www.nxp.com/docs/en/data-sheet/RW612.pdf)
