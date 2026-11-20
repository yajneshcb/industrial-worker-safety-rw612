/*
 * Copyright 2026
 * Device System Header: NXP RW612 (RW612ETA2I)
 */

#ifndef _SYSTEM_RW612_H_
#define _SYSTEM_RW612_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* _SYSTEM_RW612_H_ */

