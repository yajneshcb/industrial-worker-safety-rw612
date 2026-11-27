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
    /* Follow the FRDM-RW612 Wi-Fi example's boot sequence. In particular, the
     * XIP image needs the low-power boot clocks and OTP/ELS setup before the
     * WLAN firmware is initialized by WPL_Init(). */
    BOARD_InitBootPins();
    if (BOARD_IS_XIP())
    {
        BOARD_BootClockLPR();
        CLOCK_EnableClock(kCLOCK_Otp);
        CLOCK_EnableClock(kCLOCK_Els);
        CLOCK_EnableClock(kCLOCK_ElsApb);
        RESET_PeripheralReset(kOTP_RST_SHIFT_RSTn);
        RESET_PeripheralReset(kELS_APB_RST_SHIFT_RSTn);
    }
    else
    {
        BOARD_InitBootClocks();
    }

    /* Initialize Flexcomm 3 Debug Console UART (GPIO_24). */
    BOARD_InitDebugConsole();

    /* Complete the SDK Wi-Fi board initialization before WPL_Init(). */
    RESET_PeripheralReset(kGDMA_RST_SHIFT_RSTn);
    POWER_ConfigCauInSleep(false);
    BOARD_InitSleepPinConfig();

    /* Attach Flexcomm 2 I2C clock for P3T1755 and MPU-6050. */
    CLOCK_AttachClk(BOARD_I2C_CLK_ATTACH);

    /* Attach and enable the GAU ADC clock and power. */
    CLOCK_AttachClk(kT3PLL_MCI_256M_to_GAU_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivGauClk, 4U);
    CLOCK_EnableClock(kCLOCK_Gau);
    RESET_PeripheralReset(kGAU_RST_SHIFT_RSTn);

    POWER_PowerOnGau();
}
