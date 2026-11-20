/*
 * Copyright 2026
 * Module: Active Buzzer Driver
 * Platform: NXP FRDM-RW612
 */

#include "buzzer.h"
#include "board.h"

/*******************************************************************************
 * Global Actuator Live State
 ******************************************************************************/
volatile bool     g_buzzerActive = false;
volatile uint32_t g_buzzerMode   = BUZZER_MODE_OFF;

static uint32_t   s_buzzerTicks  = 0U;

/*******************************************************************************
 * Hardware GPIO Register Access for NXP RW612 (Port 0)
 * Pin D4 = GPIO_4 (Active Buzzer Trigger)
 ******************************************************************************/
#define HW_GPIO_PORT0_DIR    (*(volatile uint32_t *)(0x40102000U))
#define HW_GPIO_PORT0_SET    (*(volatile uint32_t *)(0x40102200U))
#define HW_GPIO_PORT0_CLR    (*(volatile uint32_t *)(0x40102300U))

static inline void HW_BUZZER_WritePin(bool level)
{
    if (level)
    {
        HW_GPIO_PORT0_SET = (1U << BOARD_BUZZER_GPIO_PIN);
        *((volatile uint8_t *)(0x40100000U + BOARD_BUZZER_GPIO_PIN)) = 1U;
    }
    else
    {
        HW_GPIO_PORT0_CLR = (1U << BOARD_BUZZER_GPIO_PIN);
        *((volatile uint8_t *)(0x40100000U + BOARD_BUZZER_GPIO_PIN)) = 0U;
    }
}

/*******************************************************************************
 * Buzzer Implementations
 ******************************************************************************/

void BUZZER_Init(void)
{
    g_buzzerActive = false;
    g_buzzerMode   = BUZZER_MODE_OFF;
    s_buzzerTicks  = 0U;

    /* Configure GPIO_4 as digital output and drive LOW (Silent) */
    HW_GPIO_PORT0_DIR |= (1U << BOARD_BUZZER_GPIO_PIN);
    HW_BUZZER_WritePin(false);
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
            /* Periodic chirp: 200ms ON every 1000ms */
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

    /* Drive physical buzzer pin */
    HW_BUZZER_WritePin(g_buzzerActive);
}


