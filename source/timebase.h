/*
 * Copyright 2026
 * Module: Millisecond timebase (Cortex-M33 DWT cycle counter)
 * Platform: NXP FRDM-RW612
 *
 * Replaces the old busy-wait "100 ms" delay, which assumed a clock that was never configured
 * (at 260 MHz it only lasted ~12 ms). Timing is now derived from the real CPU cycle counter and
 * SystemCoreClock, so it is correct as soon as SystemCoreClock matches the real core clock.
 * Timebase_GetMs() must be called at least once every ~15 s (the 32-bit counter wrap period at
 * 260 MHz); the main loop calls it every cycle.
 */

#ifndef _TIMEBASE_H_
#define _TIMEBASE_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Starts the cycle counter. Returns false if the counter does not advance (not implemented). */
bool     Timebase_Init(void);
bool     Timebase_IsRunning(void);
/* Milliseconds since Timebase_Init(). Wraps after ~49 days; compare with signed subtraction. */
uint32_t Timebase_GetMs(void);

#if defined(__cplusplus)
}
#endif

#endif /* _TIMEBASE_H_ */
