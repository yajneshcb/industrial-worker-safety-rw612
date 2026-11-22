/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 * Module: Board Port Hardware Initialization
 */

#ifndef _HARDWARE_INIT_H_
#define _HARDWARE_INIT_H_

#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @brief Initialize all board hardware peripherals, clocks, power, pin muxing and debug console.
 */
void BOARD_InitHardware(void);

#if defined(__cplusplus)
}
#endif

#endif /* _HARDWARE_INIT_H_ */
