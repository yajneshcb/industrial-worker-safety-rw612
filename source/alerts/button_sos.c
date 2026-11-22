/*
 * Copyright 2026
 * Module: Onboard SOS Push Button (SW2)
 * Platform: NXP FRDM-RW612
 *
 * Uses NXP MCUXpresso SDK lpc_gpio driver.
 * Configured as interrupt-driven on GPIO Port 0 Pin 11 (SW2, active LOW).
 * Falling edge triggers GPIO_INTA_DriverIRQHandler which latches emergency hold for 5.0 seconds.
 * Polling via GPIO_PinRead is maintained as a backup verification.
 */

#include "button_sos.h"
#include "app_config.h"
#include "fsl_common.h"
#include "fsl_gpio.h"

#define SOS_LATCH_TICKS 50U /* 5.0 seconds emergency hold (50 ticks @ 100ms) */

static volatile uint32_t s_sosLatchCountdown = 0U;

/*******************************************************************************
 * Hardware Interrupt Handler for GPIO Port 0 Interrupt A
 * Triggered on falling edge when SW2 is pressed down to GND.
 ******************************************************************************/
void GPIO_INTA_DriverIRQHandler(void)
{
    /* Clear interrupt flag for SOS button pin on Interrupt A */
    GPIO_PinClearInterruptFlag(GPIO, APP_SOS_GPIO_PORT, APP_SOS_GPIO_PIN, (uint32_t)kGPIO_InterruptA);

    /* Latch emergency state */
    s_sosLatchCountdown = SOS_LATCH_TICKS;

    SDK_ISR_EXIT_BARRIER;
}

void BUTTON_SOS_IRQHandler(void)
{
    /* Manual trigger or legacy call */
    s_sosLatchCountdown = SOS_LATCH_TICKS;
}

void BUTTON_SOS_Init(void)
{
    s_sosLatchCountdown = 0U;

    gpio_pin_config_t btnConfig = {
        .pinDirection = kGPIO_DigitalInput,
        .outputLogic  = 0U
    };
    gpio_interrupt_config_t intConfig = {
        .mode     = kGPIO_PinIntEnableEdge,
        .polarity = kGPIO_PinIntEnableLowOrFall
    };

    /* Ensure GPIO Port 0 clock is enabled */
    GPIO_PortInit(GPIO, APP_SOS_GPIO_PORT);

    /* Configure GPIO_11 as digital input */
    GPIO_PinInit(GPIO, APP_SOS_GPIO_PORT, APP_SOS_GPIO_PIN, &btnConfig);

    /* Configure falling-edge interrupt on Interrupt A */
    GPIO_SetPinInterruptConfig(GPIO, APP_SOS_GPIO_PORT, APP_SOS_GPIO_PIN, &intConfig);
    GPIO_PinEnableInterrupt(GPIO, APP_SOS_GPIO_PORT, APP_SOS_GPIO_PIN, (uint32_t)kGPIO_InterruptA);

    /* Enable Interrupt in NVIC */
    NVIC_SetPriority(GPIO_INTA_IRQn, 3);
    EnableIRQ(GPIO_INTA_IRQn);
}

bool BUTTON_SOS_IsPressed(void)
{
    /* Read physical pin 11: active LOW when SW2 is pressed */
    bool physicalPressed = (GPIO_PinRead(GPIO, APP_SOS_GPIO_PORT, APP_SOS_GPIO_PIN) == 0U);

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
