/*
 * Copyright 2026
 * Module: Onboard SOS Push Button (SW2)
 * Platform: NXP FRDM-RW612
 */

#include "button_sos.h"
#include "board.h"

/*******************************************************************************
 * Button SOS State & Hardware Read (Port 0 Pin 24)
 * Active Low input with internal pull-up.
 * Supports both interrupt latching and polled read.
 ******************************************************************************/
#define HW_GPIO_PORT0_PIN    (*(volatile uint32_t *)(0x40102100U))

volatile bool g_manualSosTrigger = false;
static bool   s_buttonLatched    = false;

/* Interrupt Service Routine for GPIO Pin Interrupt (PINT / GPIO_INTA) */
void BUTTON_SOS_IRQHandler(void)
{
    /* Falling edge detected (button pressed to GND) */
    g_manualSosTrigger = true;
    s_buttonLatched    = true;
}

void BUTTON_SOS_Init(void)
{
    g_manualSosTrigger = false;
    s_buttonLatched    = false;
}

bool BUTTON_SOS_IsPressed(void)
{
    /* Read physical pin 24: active LOW when SW2 is pressed */
    bool physicalPressed = ((HW_GPIO_PORT0_PIN & (1U << BOARD_SW2_GPIO_PIN)) == 0U);

    if (physicalPressed)
    {
        s_buttonLatched = true;
    }

    return (g_manualSosTrigger || s_buttonLatched || physicalPressed);
}

void BUTTON_SOS_Clear(void)
{
    g_manualSosTrigger = false;
    s_buttonLatched    = false;
}

