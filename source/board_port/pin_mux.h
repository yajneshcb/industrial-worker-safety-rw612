/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 * Module: Board Port Pin Multiplexing Header
 */

#ifndef _PIN_MUX_H_
#define _PIN_MUX_H_

#if defined(__cplusplus)
extern "C" {
#endif

void BOARD_InitBootPins(void);
void BOARD_InitPins(void);

#if defined(__cplusplus)
}
#endif

#endif /* _PIN_MUX_H_ */
