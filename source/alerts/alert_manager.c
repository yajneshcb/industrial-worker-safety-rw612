/*
 * Copyright 2026
 * Module: Alert Manager Subsystem
 * Platform: NXP FRDM-RW612
 */

#include "alert_manager.h"
#include "buzzer.h"
#include "leds.h"
#include <stddef.h>

/*******************************************************************************
 * Alert Manager Implementations
 ******************************************************************************/

static uint32_t s_alertCycle = 0U;

void ALERT_Manager_Init(void)
{
    s_alertCycle = 0U;
    LEDS_Init();
    BUZZER_Init();
}

void ALERT_Manager_Update(const SafetyEvaluation_t *eval)
{
    if (eval == NULL)
    {
        return;
    }

    s_alertCycle++;

    switch (eval->state)
    {
        case SAFETY_STATE_NORMAL:
            LEDS_SetState(LED_NORMAL, true);
            LEDS_SetState(LED_WARNING, false);
            LEDS_SetState(LED_EMERGENCY, false);
            BUZZER_SetMode(BUZZER_MODE_OFF);
            break;

        case SAFETY_STATE_WARNING:
            LEDS_SetState(LED_NORMAL, false);
            LEDS_SetState(LED_EMERGENCY, false);
            if (eval->primary_hazard == HAZARD_SENSOR_FAULT)
            {
                /* Pulse Yellow LED on sensor fault (1 Hz blink rate: 500ms ON / 500ms OFF) */
                LEDS_SetState(LED_WARNING, ((s_alertCycle % 10U) < 5U));
            }
            else
            {
                /* Solid Yellow LED on environmental high temperature / elevated gas warning */
                LEDS_SetState(LED_WARNING, true);
            }
            BUZZER_SetMode(BUZZER_MODE_WARNING_BEEP);
            break;

        case SAFETY_STATE_EMERGENCY:
            LEDS_SetState(LED_NORMAL, false);
            LEDS_SetState(LED_WARNING, false);
            /* Strobe Red LED synchronized with rapid emergency buzzer (100ms strobe) */
            LEDS_SetState(LED_EMERGENCY, ((s_alertCycle % 2U) == 0U));
            BUZZER_SetMode(BUZZER_MODE_EMERGENCY_ALARM);
            break;

        default:
            break;
    }

    /* Advance non-blocking sound pattern generator */
    BUZZER_Process();
}


