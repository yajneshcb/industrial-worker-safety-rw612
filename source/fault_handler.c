/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 * Module: Fault Handler Implementation
 *
 * Catches hardware faults, increments diagnostic counter and reports
 * via debug console before trapping.
 */

#include "fault_handler.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"

volatile uint32_t g_hardfaultCount = 0U;

void HardFault_Handler(void)
{
    g_hardfaultCount++;

    PRINTF("\r\n[HARDFAULT] Fatal Hardware Fault detected! Count=%u\r\n", (unsigned int)g_hardfaultCount);
    PRINTF("[HARDFAULT] Performing emergency reboot via NVIC_SystemReset()...\r\n");

    /* Small delay to allow UART TX FIFO to flush */
    for (volatile uint32_t i = 0U; i < 2000000U; i++)
    {
        __asm__ volatile ("nop");
    }

    /* Recover system automatically */
    NVIC_SystemReset();
}
