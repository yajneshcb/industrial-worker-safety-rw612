/*
 * Copyright 2026
 * Module: Status LEDs Driver (Normal, Warning, Emergency)
 * Platform: NXP FRDM-RW612
 *
 * Uses NXP MCUXpresso SDK lpc_gpio driver.
 * Controls Status LEDs connected to GPIO_5 (Green), GPIO_6 (Yellow), GPIO_7 (Red) on Port 0.
 */

#include "leds.h"
#include "app_config.h"
#include "fsl_common.h"
#include "fsl_gpio.h"

/*******************************************************************************
 * Global Actuator Live State
 ******************************************************************************/
volatile bool g_ledNormalOn    = false;
volatile bool g_ledWarningOn   = false;
volatile bool g_ledEmergencyOn = false;

void LEDS_Init(void)
{
    g_ledNormalOn    = true;  /* Normal green ON by default on boot */
    g_ledWarningOn   = false;
    g_ledEmergencyOn = false;

    gpio_pin_config_t ledConfig = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic  = 0U
    };

    /* Ensure Port 0 is initialized */
    GPIO_PortInit(GPIO, APP_ACTUATOR_GPIO_PORT);

    /* Initialize each status LED pin as digital output */
    GPIO_PinInit(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_NORMAL_PIN, &ledConfig);
    GPIO_PinInit(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_WARNING_PIN, &ledConfig);
    GPIO_PinInit(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_EMERGENCY_PIN, &ledConfig);

    /* Initial state: Green ON, Yellow & Red OFF */
    GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_NORMAL_PIN, 1U);
    GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_WARNING_PIN, 0U);
    GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_EMERGENCY_PIN, 0U);
}

void LEDS_SetState(LedIdentifier_t led, bool on)
{
    switch (led)
    {
        case LED_NORMAL:
            g_ledNormalOn = on;
            GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_NORMAL_PIN, on ? 1U : 0U);
            break;
        case LED_WARNING:
            g_ledWarningOn = on;
            GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_WARNING_PIN, on ? 1U : 0U);
            break;
        case LED_EMERGENCY:
            g_ledEmergencyOn = on;
            GPIO_PinWrite(GPIO, APP_ACTUATOR_GPIO_PORT, BOARD_LED_EMERGENCY_PIN, on ? 1U : 0U);
            break;
        default:
            break;
    }
}

void LEDS_Toggle(LedIdentifier_t led)
{
    switch (led)
    {
        case LED_NORMAL:
            LEDS_SetState(LED_NORMAL, !g_ledNormalOn);
            break;
        case LED_WARNING:
            LEDS_SetState(LED_WARNING, !g_ledWarningOn);
            break;
        case LED_EMERGENCY:
            LEDS_SetState(LED_EMERGENCY, !g_ledEmergencyOn);
            break;
        default:
            break;
    }
}
