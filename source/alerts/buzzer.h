/*
 * Copyright 2026
 * Module: Active Buzzer Driver
 * Platform: NXP FRDM-RW612
 */

#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Global live buzzer state for VS Code Watch */
extern volatile bool     g_buzzerActive;
extern volatile uint32_t g_buzzerMode;

typedef enum {
    BUZZER_MODE_OFF = 0,
    BUZZER_MODE_WARNING_BEEP,
    BUZZER_MODE_EMERGENCY_ALARM
} BuzzerMode_t;

#if defined(__cplusplus)
extern "C" {
#endif

void BUZZER_Init(void);
void BUZZER_SetMode(BuzzerMode_t mode);
void BUZZER_Process(void);

#if defined(__cplusplus)
}
#endif

#endif /* _BUZZER_H_ */

