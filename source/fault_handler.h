/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 * Module: Fault Handler Header
 */

#ifndef _FAULT_HANDLER_H_
#define _FAULT_HANDLER_H_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern volatile uint32_t g_hardfaultCount;

void HardFault_Handler(void);

#if defined(__cplusplus)
}
#endif

#endif /* _FAULT_HANDLER_H_ */
