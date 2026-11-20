/*
 * Copyright 2026
 * Module: Alert Manager Subsystem
 * Platform: NXP FRDM-RW612
 */

#ifndef _ALERT_MANAGER_H_
#define _ALERT_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include "safety_decision.h"

/*******************************************************************************
 * API Declarations
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

void ALERT_Manager_Init(void);
void ALERT_Manager_Update(const SafetyEvaluation_t *eval);

#if defined(__cplusplus)
}
#endif

#endif /* _ALERT_MANAGER_H_ */

