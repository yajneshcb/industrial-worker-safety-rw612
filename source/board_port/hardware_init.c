/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 * Module: Board Port Hardware Initialization
 *
 * Configures the real NXP MCUXpresso SDK clocks, pin muxing, debug console,
 * Flexcomm 2 I2C clock attachment, and GAU ADC power and clock.
 */

#include "hardware_init.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_common.h"
#include "fsl_power.h"
#include "fsl_clock.h"
#include "fsl_reset.h"
#include "app_config.h"

void BOARD_InitHardware(void)
{
    /* 1. Initialize pins configured in pin_mux.c */
    BOARD_InitPins();

    /* 2. Configure system clocks to RUN mode using SDK board clock generator */
    BOARD_BootClockRUN();

    /* 3. Initialize Flexcomm 3 Debug Console UART (GPIO_24) for USB MCU-Link serial communication */
    BOARD_InitDebugConsole();

    /* 4. Attach Flexcomm 2 (I2C bus for P3T1755 and MPU-6050): SFRO 16 MHz per SDK example */
    CLOCK_AttachClk(BOARD_I2C_CLK_ATTACH);

    /* 5. Attach and enable GAU General-Purpose ADC clock and power */
    CLOCK_AttachClk(kT3PLL_MCI_256M_to_GAU_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivGauClk, 4U);
    CLOCK_EnableClock(kCLOCK_Gau);
    RESET_PeripheralReset(kGAU_RST_SHIFT_RSTn);

    POWER_PowerOnGau();
}
