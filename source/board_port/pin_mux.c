/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 * Module: Board Port Pin Multiplexing Implementation
 *
 * Mux configuration:
 *  - GPIO_24: Flexcomm 3 USART Data (MCU-Link Virtual COM console)
 *  - GPIO_16: Flexcomm 2 I2C SDA (P3T1755 & MPU-6050 bus)
 *  - GPIO_17: Flexcomm 2 I2C SCL (P3T1755 & MPU-6050 bus)
 *  - GPIO_11: SW2 Onboard Emergency SOS Push Button (Active LOW)
 *  - GPIO_4:  Active Buzzer actuator
 *  - GPIO_5:  Status LED Normal (Green)
 *  - GPIO_6:  Status LED Warning (Yellow)
 *  - GPIO_7:  Status LED Emergency (Red)
 *  - GPIO_46: ADC0 Channel 4 input (Header J7 Pin 3, MQ-2 analog voltage via divider)
 */

#include "pin_mux.h"
#include "fsl_common.h"
#include "fsl_gpio.h"
#include "fsl_io_mux.h"
#include "app_config.h"

void BOARD_InitBootPins(void)
{
    BOARD_InitPins();
}

void BOARD_InitPins(void)
{
    /* 1. Flexcomm 3 USART Data on GPIO_24 for MCU-Link debug console */
    IO_MUX_SetPinMux(IO_MUX_FC3_USART_DATA);

    /* 2. Flexcomm 2 I2C on GPIO_16 (SDA) and GPIO_17 (SCL) */
    IO_MUX_SetPinMux(IO_MUX_FC2_I2C_16_17);

    /* 3. Actuator GPIO outputs on Port 0 (Buzzer & Status LEDs) with strongest drive */
    IO_MUX_SetPinMux(IO_MUX_GPIO4);
    IO_MUX_SetPinMux(IO_MUX_GPIO5);
    IO_MUX_SetPinMux(IO_MUX_GPIO6);
    IO_MUX_SetPinMux(IO_MUX_GPIO7);
    IO_MUX_SetPinConfig(4U, IO_MUX_PinConfigNoPullDriveStrongest);
    IO_MUX_SetPinConfig(5U, IO_MUX_PinConfigNoPullDriveStrongest);
    IO_MUX_SetPinConfig(6U, IO_MUX_PinConfigNoPullDriveStrongest);
    IO_MUX_SetPinConfig(7U, IO_MUX_PinConfigNoPullDriveStrongest);
    /* LEDs are driven as GPIO outputs; leave all three pads without bias resistors. */
    IO_MUX_SetPinConfig(5U, IO_MUX_PinConfigNoPull);
    IO_MUX_SetPinConfig(6U, IO_MUX_PinConfigNoPull);
    IO_MUX_SetPinConfig(7U, IO_MUX_PinConfigNoPull);

    /* 4. Onboard Emergency SOS Push Button (SW2) on GPIO_11 */
    IO_MUX_SetPinMux(IO_MUX_GPIO11);

    /* 5. Select the J7-3/GPIO46 pad function used by ADC0 channel 4. */
    IO_MUX_SetPinMux(IO_MUX_GPIO46);
    /* ADC input: leave pad bias disabled. This SDK exposes no separate DIGIMODE control. */
    IO_MUX_SetPinConfig(46U, IO_MUX_PinConfigNoPull);

    /* 6. Enable clock for GPIO Port 0 module */
    GPIO_PortInit(GPIO, 0U);
}
