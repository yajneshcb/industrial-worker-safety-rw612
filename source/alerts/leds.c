/*
 * Copyright 2026
 * Module: Status LEDs Driver (Normal, Warning, Emergency)
 * Platform: NXP FRDM-RW612
 */

#include "leds.h"
#include "board.h"

/*******************************************************************************
 * Global Actuator Live State
 ******************************************************************************/
volatile bool g_ledNormalOn    = false;
volatile bool g_ledWarningOn   = false;
volatile bool g_ledEmergencyOn = false;

/*******************************************************************************
 * Hardware GPIO Register Access for NXP RW612 (Port 0)
 * Pin D5 = GPIO_5 (Green Normal LED)
 * Pin D6 = GPIO_6 (Yellow Warning LED)
 * Pin D7 = GPIO_7 (Red Emergency LED)
 ******************************************************************************/
#define HW_GPIO_PORT0_DIR    (*(volatile uint32_t *)(0x40102000U))
#define HW_GPIO_PORT0_SET    (*(volatile uint32_t *)(0x40102200U))
#define HW_GPIO_PORT0_CLR    (*(volatile uint32_t *)(0x40102300U))

static inline void HW_GPIO_WritePin(uint32_t pin, bool level)
{
    if (level)
    {
        /* Set pin HIGH (3.3V) via both Set register and Byte register */
        HW_GPIO_PORT0_SET = (1U << pin);
        *((volatile uint8_t *)(0x40100000U + pin)) = 1U;
    }
    else
    {
        /* Clear pin LOW (0V) via both Clear register and Byte register */
        HW_GPIO_PORT0_CLR = (1U << pin);
        *((volatile uint8_t *)(0x40100000U + pin)) = 0U;
    }
}

/*******************************************************************************
 * LED Implementations
 ******************************************************************************/

void LEDS_Init(void)
{
    g_ledNormalOn    = true;  /* Normal green ON by default on boot */
    g_ledWarningOn   = false;
    g_ledEmergencyOn = false;

    /* Configure dedicated status LED pins as digital outputs */
    HW_GPIO_PORT0_DIR |= (1U << BOARD_LED_NORMAL_PIN)    | 
                         (1U << BOARD_LED_WARNING_PIN)   | 
                         (1U << BOARD_LED_EMERGENCY_PIN);

    /* Initial state: Green ON, Yellow & Red OFF */
    HW_GPIO_WritePin(BOARD_LED_NORMAL_PIN, true);
    HW_GPIO_WritePin(BOARD_LED_WARNING_PIN, false);
    HW_GPIO_WritePin(BOARD_LED_EMERGENCY_PIN, false);
}

void LEDS_SetState(LedIdentifier_t led, bool on)
{
    switch (led)
    {
        case LED_NORMAL:
            g_ledNormalOn = on;
            HW_GPIO_WritePin(BOARD_LED_NORMAL_PIN, on);
            break;
        case LED_WARNING:
            g_ledWarningOn = on;
            HW_GPIO_WritePin(BOARD_LED_WARNING_PIN, on);
            break;
        case LED_EMERGENCY:
            g_ledEmergencyOn = on;
            HW_GPIO_WritePin(BOARD_LED_EMERGENCY_PIN, on);
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



