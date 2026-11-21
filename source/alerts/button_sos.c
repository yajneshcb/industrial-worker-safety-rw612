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
 * Latches for 5.0 seconds (50 ticks @ 100ms) upon press, then auto-clears.
 ******************************************************************************/
#define HW_GPIO_PORT0_PIN    (*(volatile uint32_t *)(0x40102100U))
#define SOS_LATCH_TICKS      50U /* 5.0 seconds emergency hold */

static uint32_t s_sosLatchCountdown = 0U;

void BUTTON_SOS_IRQHandler(void)
{
    /* Falling edge detected (button pressed to GND) */
    s_sosLatchCountdown = SOS_LATCH_TICKS;
}

void BUTTON_SOS_Init(void)
{
    s_sosLatchCountdown = 0U;
}

bool BUTTON_SOS_IsPressed(void)
{
    /* Read physical pin 24: active LOW when SW2 is pressed */
    bool physicalPressed = ((HW_GPIO_PORT0_PIN & (1U << BOARD_SW2_GPIO_PIN)) == 0U);

    if (physicalPressed)
    {
        s_sosLatchCountdown = SOS_LATCH_TICKS;
    }
    else if (s_sosLatchCountdown > 0U)
    {
        s_sosLatchCountdown--;
    }

    return (physicalPressed || (s_sosLatchCountdown > 0U));
}

void BUTTON_SOS_Clear(void)
{
    s_sosLatchCountdown = 0U;
}
