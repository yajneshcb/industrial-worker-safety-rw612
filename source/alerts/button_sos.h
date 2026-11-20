/*
 * Copyright 2026
 * Module: Onboard SOS Push Button (SW2)
 * Platform: NXP FRDM-RW612
 */

#ifndef _BUTTON_SOS_H_
#define _BUTTON_SOS_H_

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern volatile bool g_manualSosTrigger;

void BUTTON_SOS_Init(void);
bool BUTTON_SOS_IsPressed(void);
void BUTTON_SOS_Clear(void);

#if defined(__cplusplus)
}
#endif

#endif /* _BUTTON_SOS_H_ */

