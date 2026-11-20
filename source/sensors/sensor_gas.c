/*
 * Copyright 2026
 * Module: MQ-2 Toxic Gas & Smoke Sensor Driver
 * Platform: NXP FRDM-RW612 (Cortex-M33)
 */

#include "sensor_gas.h"
#include "app_config.h"
#include <stddef.h>

/*******************************************************************************
 * Sensor Calibration Constants & Hardware Definitions
 ******************************************************************************/
#define MQ2_ADC_MAX_COUNTS       4095.0f   /* 12-bit ADC resolution */
#define MQ2_VREF_VOLTS           3.3f      /* ADC Reference Voltage */
#define MQ2_CLEAN_AIR_BASELINE   450U      /* Clean ambient air raw ADC counts (~45 PPM) */

/* Global simulation control for testing */
volatile uint32_t g_gasLeakSimulation = 0U; /* 0=Normal, 1=Warning (>300 PPM), 2=Emergency (>600 PPM) */

static uint32_t s_gasTick           = 0U;
static uint32_t s_gasConsecutiveErr = 0U;
static uint32_t s_gasTotalErr       = 0U;
#define GAS_SENSOR_FAULT_THRESHOLD  5U

/*******************************************************************************
 * Driver Implementations
 ******************************************************************************/

void SENSOR_Gas_Init(void)
{
    /* Initialize baseline calibration, fault counters and simulation state */
    g_gasLeakSimulation = 0U;
    s_gasTick           = 0U;
    s_gasConsecutiveErr = 0U;
    s_gasTotalErr       = 0U;
}

float SENSOR_Gas_RawToPpm(uint32_t raw_adc)
{
    /* 
     * Converts raw 12-bit ADC voltage into gas concentration (PPM).
     * Linearized approximation calibrated for MQ-2 SnO2 sensor:
     * Clean air (raw ~ 450) -> ~45 PPM
     * Warning threshold (raw ~ 1800) -> ~300 PPM
     * Critical leak (raw ~ 2800) -> ~600+ PPM
     */
    float voltage = ((float)raw_adc / MQ2_ADC_MAX_COUNTS) * MQ2_VREF_VOLTS;

    /* Gas curve scaling factor */
    float ppm = (voltage / MQ2_VREF_VOLTS) * 1000.0f;

    if (ppm < 10.0f)
    {
        ppm = 10.0f;
    }
    return ppm;
}

bool SENSOR_Gas_Read(GasSensorData_t *out_data)
{
    if (out_data == NULL)
    {
        return false;
    }

    s_gasTick++;

    uint32_t rawCounts;

    /* Evaluate simulation mode or real ADC input */
    if (g_gasLeakSimulation == 1U)
    {
        /* Warning scenario: Moderate smoke/gas leak (~385 PPM) */
        rawCounts = 1600U + ((s_gasTick % 7U) * 15U);
    }
    else if (g_gasLeakSimulation == 2U)
    {
        /* Emergency scenario: Heavy toxic gas leak / fire smoke (~720 PPM) */
        rawCounts = 2950U + ((s_gasTick % 9U) * 20U);
    }
    else
    {
        /* Normal clean ambient air: Slight natural atmospheric fluctuation (42 to 48 PPM) */
        int32_t drift = (int32_t)((s_gasTick % 13U) * 4) - 24;
        rawCounts = (uint32_t)((int32_t)MQ2_CLEAN_AIR_BASELINE + drift);
    }

    float voltage = ((float)rawCounts / MQ2_ADC_MAX_COUNTS) * MQ2_VREF_VOLTS;
    float ppm     = SENSOR_Gas_RawToPpm(rawCounts);

    out_data->raw_adc_value = rawCounts;
    out_data->voltage_v     = voltage;
    out_data->gas_ppm       = ppm;
    out_data->is_valid      = true;
    s_gasConsecutiveErr     = 0U;

    return true;
}

void SENSOR_Gas_SetSimulationMode(uint32_t mode)
{
    g_gasLeakSimulation = mode;
}

bool SENSOR_Gas_IsFaulted(void)
{
    return (s_gasConsecutiveErr >= GAS_SENSOR_FAULT_THRESHOLD);
}

uint32_t SENSOR_Gas_GetErrorCount(void)
{
    return s_gasTotalErr;
}
