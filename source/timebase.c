/*
 * Copyright 2026
 * Module: Millisecond timebase (Cortex-M33 DWT cycle counter)
 * Platform: NXP FRDM-RW612
 */

#include "timebase.h"
#include "system_RW612.h"

/* Architectural Armv8-M debug registers (identical on every Cortex-M33, not chip specific). */
#define TB_DEMCR     (*(volatile uint32_t *)0xE000EDFCU)
#define TB_DWT_CTRL  (*(volatile uint32_t *)0xE0001000U)
#define TB_DWT_CYC   (*(volatile uint32_t *)0xE0001004U)
#define TB_DEMCR_TRCENA        (1UL << 24)
#define TB_DWT_CTRL_CYCCNTENA  (1UL << 0)

static bool     s_running   = false;
static uint32_t s_lastCycles = 0U;
static uint64_t s_cycleAccum = 0U;
static uint32_t s_ms         = 0U;

bool Timebase_Init(void)
{
    uint32_t first;
    uint32_t second;

    TB_DEMCR    |= TB_DEMCR_TRCENA;
    TB_DWT_CTRL |= TB_DWT_CTRL_CYCCNTENA;

    first = TB_DWT_CYC;
    for (volatile uint32_t i = 0U; i < 64U; i++)
    {
        __asm__ volatile ("nop");
    }
    second = TB_DWT_CYC;

    s_running    = (second != first);
    s_lastCycles = second;
    s_cycleAccum = 0U;
    s_ms         = 0U;
    return s_running;
}

bool Timebase_IsRunning(void)
{
    return s_running;
}

uint32_t Timebase_GetMs(void)
{
    uint32_t now;
    uint32_t cyclesPerMs;

    if (!s_running)
    {
        return s_ms;
    }

    cyclesPerMs = SystemCoreClock / 1000U;
    if (cyclesPerMs == 0U)
    {
        return s_ms;
    }

    now           = TB_DWT_CYC;
    s_cycleAccum += (uint32_t)(now - s_lastCycles); /* unsigned subtraction handles counter wrap */
    s_lastCycles  = now;

    s_ms         += (uint32_t)(s_cycleAccum / cyclesPerMs);
    s_cycleAccum %= cyclesPerMs;
    return s_ms;
}
