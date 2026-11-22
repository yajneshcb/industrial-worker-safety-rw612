# NXP MCUXpresso SDK Peripheral Drivers

This directory houses or references the peripheral driver files from the **MCUXpresso SDK for FRDM-RW612**:

Key drivers utilized by the Worker Safety Monitoring system:
1. `fsl_gpio.h / .c`: General purpose I/O for Active Buzzer, LEDs, and SOS Push Button.
2. `fsl_flexcomm.h / .c` & `fsl_flexcomm_i2c.h / .c`:
   - Flexcomm 2: Interfacing with onboard P3T1755 digital temperature sensor and external MPU-6050 motion sensor (shared I2C bus).
3. `fsl_flexcomm_usart.h / .c`: Debug console serial communication via MCU-Link.
4. `fsl_lpadc.h / .c`: Low-power ADC driver for sampling the MQ-2 analog gas sensor.
5. `fsl_clock.h / .c` & `fsl_reset.h / .c`: Peripheral clock gating and reset management.
6. `fsl_debug_console.h / .c`: Formatted print (`PRINTF`) output.

*Note: In an MCUXpresso IDE workspace, these drivers can be linked directly from the SDK package or copied into this directory for standalone builds.*

