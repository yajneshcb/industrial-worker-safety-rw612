/*
 * Copyright 2026
 * Module: Status LEDs Driver (Normal, Warning, Emergency)
 * Platform: NXP FRDM-RW612
 */

#ifndef _LEDS_H_
#define _LEDS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Global live actuator states for VS Code Watch */
extern volatile bool g_ledNormalOn;
extern volatile bool g_ledWarningOn;
extern volatile bool g_ledEmergencyOn;

typedef enum {
    LED_NORMAL = 0,    /* Green LED */
    LED_WARNING,       /* Yellow LED */
    LED_EMERGENCY      /* Red LED */
} LedIdentifier_t;

#if defined(__cplusplus)
extern "C" {
#endif

void LEDS_Init(void);
void LEDS_SetState(LedIdentifier_t led, bool on);
void LEDS_Toggle(LedIdentifier_t led);

#if defined(__cplusplus)
}
#endif

#endif /* _LEDS_H_ */

