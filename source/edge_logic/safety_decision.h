/*
 * Copyright 2026
 * Module: Edge Decision Logic & Hazard Detection
 * Platform: NXP FRDM-RW612
 */

#ifndef _SAFETY_DECISION_H_
#define _SAFETY_DECISION_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sensor_temp.h"
#include "sensor_mpu6050.h"
#include "sensor_gas.h"

/*******************************************************************************
 * Enumerations & Types
 ******************************************************************************/
typedef enum {
    SAFETY_STATE_NORMAL = 0,
    SAFETY_STATE_WARNING,
    SAFETY_STATE_EMERGENCY
} SafetyState_t;

typedef enum {
    HAZARD_NONE = 0,
    HAZARD_HIGH_TEMPERATURE,
    HAZARD_GAS_LEAK,
    HAZARD_FALL_DETECTED,
    HAZARD_MANUAL_SOS,
    HAZARD_SENSOR_FAULT
} HazardType_t;

typedef struct {
    SafetyState_t state;
    HazardType_t primary_hazard;
    uint32_t timestamp_ms;
} SafetyEvaluation_t;

/*******************************************************************************
 * API Declarations
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

void SAFETY_Decision_Init(void);
SafetyEvaluation_t SAFETY_Decision_Evaluate(const TempSensorData_t *temp,
                                            const Mpu6050Data_t *mpu,
                                            const GasSensorData_t *gas,
                                            bool sos_active);

#if defined(__cplusplus)
}
#endif

#endif /* _SAFETY_DECISION_H_ */

