/*
 * Copyright 2026
 * Module: Active Buzzer Driver
 * Platform: NXP FRDM-RW612
 *
 * Uses NXP MCUXpresso SDK lpc_gpio driver.
 * Controls active buzzer connected to GPIO_4 on Port 0 (Header J1 Pin 1).
 * Supports both Active-HIGH and Active-LOW buzzer modules via BUZZER_ACTIVE_LOW define.
 */

#include "buzzer.h"
#include "app_config.h"
#include "fsl_common.h"
#include "fsl_gpio.h"

/*******************************************************************************
 * Global Actuator Live State
 ******************************************************************************/
volatile bool     g_buzzerActive = false;
volatile uint32_t g_buzzerMode   = BUZZER_MODE_OFF;

static uint32_t   s_buzzerTicks  = 0U;

static inline void HW_BUZZER_Drive(bool active)
{
#if (BUZZER_ACTIVE_LOW != 0)
    /* Active-LOW module: LOW (0) produces sound, HIGH (1) is silent */
    GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_BUZZER_GPIO_PIN, active ? 0U : 1U);
#else
    /* Active-HIGH module: HIGH (1) produces sound, LOW (0) is silent */
    GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_BUZZER_GPIO_PIN, active ? 1U : 0U);
#endif
}

void BUZZER_Init(void)
{
    g_buzzerActive = false;
    g_buzzerMode   = BUZZER_MODE_OFF;
    s_buzzerTicks  = 0U;

    gpio_pin_config_t buzzerConfig = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic  = (BUZZER_ACTIVE_LOW != 0) ? 1U : 0U
    };

    /* Ensure Port 0 is initialized and configure Buzzer pin */
    GPIO_PortInit(GPIO, APP_ACTUATOR_GPIO_PORT);
    GPIO_PinInit(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_BUZZER_GPIO_PIN, &buzzerConfig);
    HW_BUZZER_Drive(false);
}

void BUZZER_SetMode(BuzzerMode_t mode)
{
    if (g_buzzerMode != (uint32_t)mode)
    {
        g_buzzerMode  = (uint32_t)mode;
        s_buzzerTicks = 0U;
    }
}

void BUZZER_Process(void)
{
    switch ((BuzzerMode_t)g_buzzerMode)
    {
        case BUZZER_MODE_OFF:
            g_buzzerActive = false;
            s_buzzerTicks  = 0U;
            break;

        case BUZZER_MODE_WARNING_BEEP:
            /* Periodic chirp: 200ms ON every 1000ms (2 ticks ON out of 10 @ 100ms) */
            s_buzzerTicks = (s_buzzerTicks + 1U) % 10U;
            g_buzzerActive = (s_buzzerTicks < 2U);
            break;

        case BUZZER_MODE_EMERGENCY_ALARM:
            /* Urgent rapid alarm: 100ms ON / 100ms OFF continuous pulse */
            s_buzzerTicks = (s_buzzerTicks + 1U) % 2U;
            g_buzzerActive = (s_buzzerTicks == 0U);
            break;

        default:
            g_buzzerActive = false;
            break;
    }

    /* Drive physical buzzer pin according to polarity configuration */
    HW_BUZZER_Drive(g_buzzerActive);
}
