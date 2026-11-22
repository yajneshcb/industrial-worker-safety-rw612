/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612
 * NXP SDK Common Definitions
 */

#ifndef _FSL_COMMON_H_
#define _FSL_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int32_t status_t;

enum _status_groups {
    kStatusGroup_Generic = 0,
    kStatusGroup_I2C = 1,
    kStatusGroup_FLEXCOMM = 2
};

enum {
    kStatus_Success = 0,
    kStatus_Fail = 1,
    kStatus_ReadOnly = 2,
    kStatus_OutOfRange = 3,
    kStatus_InvalidArgument = 4,
    kStatus_Timeout = 5,
    kStatus_NoTransferInProgress = 6,
};

/*
 * Delay based on the Cortex-M33 DWT cycle counter and the REAL core clock passed in by the caller.
 * (The previous version looped a fixed count that assumed 24 MHz, so at 260 MHz a "100 ms" delay
 * lasted ~12 ms.) The real SDK provides its own SDK_DelayAtLeastUs and will replace this stub.
 */
static inline void SDK_DelayAtLeastUs_Impl(uint32_t us, uint32_t coreClock)
{
    volatile uint32_t *demcr  = (volatile uint32_t *)0xE000EDFCU;
    volatile uint32_t *dwtCtl = (volatile uint32_t *)0xE0001000U;
    volatile uint32_t *dwtCyc = (volatile uint32_t *)0xE0001004U;
    uint64_t cycles64 = (uint64_t)us * (uint64_t)(coreClock / 1000000U);
    uint32_t a;
    uint32_t b;

    *demcr  |= (1UL << 24);
    *dwtCtl |= 1UL;
    a = *dwtCyc;
    __asm__ volatile ("nop\n nop\n nop\n nop");
    b = *dwtCyc;

    if (a == b)
    {
        /* Cycle counter not running: crude fallback (~4 cycles per iteration). */
        volatile uint32_t n = (uint32_t)(cycles64 / 4U) + 1U;
        while (n--) { __asm__ volatile("nop"); }
        return;
    }

    {
        uint32_t start  = *dwtCyc;
        uint32_t cycles = (uint32_t)cycles64;
        while ((uint32_t)(*dwtCyc - start) < cycles) { }
    }
}

#define SDK_DelayAtLeastUs(us, coreClock) SDK_DelayAtLeastUs_Impl((uint32_t)(us), (uint32_t)(coreClock))

#endif /* _FSL_COMMON_H_ */

