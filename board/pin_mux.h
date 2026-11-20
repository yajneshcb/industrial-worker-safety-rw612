/*
 * Copyright 2026
 * Board: NXP FRDM-RW612 (RW612ETA2I)
 * Industrial Worker Safety Monitoring and Emergency Alert System
 */

#ifndef _PIN_MUX_H_
#define _PIN_MUX_H_

#include <stdint.h>

/*******************************************************************************
 * Pin Multiplexing Prototypes
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

/* Initialize all pin routing for FRDM-RW612 worker safety node */
void BOARD_InitPins(void);

#if defined(__cplusplus)
}
#endif

#endif /* _PIN_MUX_H_ */

