/*
 * Copyright 2026
 * Device System Implementation: NXP RW612 (RW612ETA2I)
 */

#include "system_RW612.h"

uint32_t SystemCoreClock = 260000000U;

void SystemInit(void)
{
    /* Enable ARM Cortex-M33 Floating Point Unit (FPU coprocessors CP10 & CP11) */
    volatile uint32_t *pCPACR = (volatile uint32_t *)0xE000ED88U;
    *pCPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));

    __asm__ volatile ("dsb 0xF" ::: "memory");
    __asm__ volatile ("isb 0xF" ::: "memory");
}

void SystemCoreClockUpdate(void)
{
    /* TODO: Update SystemCoreClock variable based on current clock tree */
}

volatile uint32_t g_hardfaultCount = 0U;

void HardFault_Handler(void)
{
    g_hardfaultCount++;
    while (1)
    {
        /* Trapped in HardFault: Inspect call stack or g_hardfaultCount */
    }
}


