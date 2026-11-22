/*
 * Copyright 2026
 * Board: NXP FRDM-RW612 (RW612ETA2I)
 * Industrial Worker Safety Monitoring and Emergency Alert System
 */

#ifndef _CLOCK_CONFIG_H_
#define _CLOCK_CONFIG_H_

#include <stdint.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BOARD_BOOTCLOCKRUN_CORE_CLOCK 260000000U /* Core clock frequency: 260MHz */

/*******************************************************************************
 * API Declarations
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

void BOARD_BootClockRUN(void);

#if defined(__cplusplus)
}
#endif

#endif /* _CLOCK_CONFIG_H_ */

