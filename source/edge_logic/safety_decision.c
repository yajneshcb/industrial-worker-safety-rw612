/*
 * Copyright 2026
 * Module: Edge Decision Logic & Hazard Detection
 * Platform: NXP FRDM-RW612
 *
 * Implements autonomous edge hazard classification.
 * High-speed 50 Hz (20 ms) motion processing captures rapid free-fall (<0.4g)
 * and sharp impact (>2.5g) spikes reliably.
 */

#include "safety_decision.h"
#include "app_config.h"
#include <stddef.h>

/*******************************************************************************
 * Private State Variables
 ******************************************************************************/
static uint32_t s_freeFallCountdown = 0U; /* Ticks remaining to detect impact after free-fall */
static bool     s_fallLatched       = false;
static uint32_t s_fallLatchTimer    = 0U; /* Emergency active duration */

#define FALL_WINDOW_TICKS (1000U / IMU_POLL_INTERVAL_MS) /* 50 ticks @ 20ms = 1.0 second window */
#define FALL_LATCH_TICKS  (5000U / IMU_POLL_INTERVAL_MS) /* 250 ticks @ 20ms = 5.0 seconds latch */

void SAFETY_Decision_Init(void)
{
    s_freeFallCountdown = 0U;
    s_fallLatched       = false;
    s_fallLatchTimer    = 0U;
}

void SAFETY_Decision_ProcessFall(const Mpu6050Data_t *mpu)
{
    if (s_fallLatched)
    {
        if (s_fallLatchTimer > 0U)
        {
            s_fallLatchTimer--;
        }
        else
        {
            s_fallLatched = false;
        }
    }

    if (mpu != NULL && mpu->is_valid)
    {
        float total_g = SENSOR_MPU6050_GetTotalAccelG(mpu);

        /* Stage A: Detect Free-fall condition (< 0.4g) */
        if (total_g < THRESHOLD_FALL_ACCEL_MIN_G)
        {
            s_freeFallCountdown = FALL_WINDOW_TICKS;
        }
        /* Stage B: Detect Impact spike (> 2.5g) within the 1.0s window */
        else if ((s_freeFallCountdown > 0U) && (total_g > THRESHOLD_FALL_IMPACT_MAX_G))
        {
            s_freeFallCountdown = 0U;
            s_fallLatched       = true;
            s_fallLatchTimer    = FALL_LATCH_TICKS;
        }

        if (s_freeFallCountdown > 0U)
        {
            s_freeFallCountdown--;
        }
    }
}

bool SAFETY_Decision_IsFallLatched(void)
{
    return s_fallLatched;
}

SafetyEvaluation_t SAFETY_Decision_Evaluate(const TempSensorData_t *temp,
                                            const Mpu6050Data_t *mpu,
                                            const GasSensorData_t *gas,
                                            bool sos_active)
{
    SafetyEvaluation_t eval;
    eval.state          = SAFETY_STATE_NORMAL;
    eval.primary_hazard = HAZARD_NONE;
    eval.timestamp_ms   = 0U;

    /* 1. HIGHEST PRIORITY: Manual SOS Emergency Button (Immediate) */
    if (sos_active)
    {
        eval.state          = SAFETY_STATE_EMERGENCY;
        eval.primary_hazard = HAZARD_MANUAL_SOS;
        return eval;
    }

    /* 2. Fall Detection State Machine (evaluated at 50 Hz via SAFETY_Decision_ProcessFall) */
    if (s_fallLatched)
    {
        eval.state          = SAFETY_STATE_EMERGENCY;
        eval.primary_hazard = HAZARD_FALL_DETECTED;
        return eval;
    }

    /* 3. Thermal Hazard Evaluation */
    if (temp != NULL && temp->is_valid)
    {
        if (temp->temperature_c >= THRESHOLD_TEMP_HIGH_EMERGENCY)
        {
            eval.state          = SAFETY_STATE_EMERGENCY;
            eval.primary_hazard = HAZARD_HIGH_TEMPERATURE;
            return eval;
        }
        else if (temp->temperature_c >= THRESHOLD_TEMP_HIGH_WARNING)
        {
            eval.state          = SAFETY_STATE_WARNING;
            eval.primary_hazard = HAZARD_HIGH_TEMPERATURE;
        }
    }

    /* 4. Toxic Gas / Smoke Hazard Evaluation */
    if (gas != NULL && gas->is_valid)
    {
        if (gas->gas_ppm >= THRESHOLD_GAS_EMERGENCY_PPM)
        {
            eval.state          = SAFETY_STATE_EMERGENCY;
            eval.primary_hazard = HAZARD_GAS_LEAK;
            return eval;
        }
        else if (gas->gas_ppm >= THRESHOLD_GAS_WARNING_PPM)
        {
            if (eval.state != SAFETY_STATE_EMERGENCY)
            {
                eval.state          = SAFETY_STATE_WARNING;
                eval.primary_hazard = HAZARD_GAS_LEAK;
            }
        }
    }

    /* 5. Sensor Fault Detection */
    if (SENSOR_Temp_IsFaulted() || SENSOR_MPU6050_IsFaulted() || SENSOR_Gas_IsFaulted())
    {
        if (eval.state == SAFETY_STATE_NORMAL)
        {
            eval.state          = SAFETY_STATE_WARNING;
            eval.primary_hazard = HAZARD_SENSOR_FAULT;
        }
    }

    return eval;
}
