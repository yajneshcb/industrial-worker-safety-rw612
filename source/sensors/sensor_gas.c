/*
 * Copyright 2026
 * Module: MQ-2 Toxic Gas & Smoke Sensor Driver
 * Platform: NXP FRDM-RW612 (Cortex-M33)
 *
 * Uses NXP MCUXpresso SDK cns_adc driver (GAU_GPADC0, Channel kADC_CH4 on GPIO_46 / Header J7 Pin 3).
 * Converts 0-1.2V ADC pin readings back through the 36k/10k hardware voltage divider to measure
 * actual MQ-2 AOUT (0-5.0V), then computes sensor resistance (Rs) and PPM.
 */

#include "sensor_gas.h"
#include "app_config.h"
#include "fsl_common.h"
#include "fsl_adc.h"
#include "fsl_debug_console.h"
#include <math.h>

/*******************************************************************************
 * Module State & Fault Counters
 ******************************************************************************/
#define GAS_SENSOR_FAULT_THRESHOLD 5U

static uint32_t s_gasConsecutiveErr = 0U;
static uint32_t s_gasTotalErr       = 0U;
static bool     s_gasInitialized    = false;
static uint32_t s_warmupCountdown   = MQ2_WARMUP_TICKS;

static void RecordFailure(GasSensorData_t *out_data)
{
    s_gasConsecutiveErr++;
    s_gasTotalErr++;

    if (out_data != NULL)
    {
        out_data->raw_adc_value = 0U;
        out_data->voltage_v     = 0.0f;
        out_data->gas_ppm       = 0.0f;
        out_data->is_valid      = false;
    }
}

/*******************************************************************************
 * Public Driver Functions
 ******************************************************************************/

void SENSOR_Gas_Init(void)
{
    adc_config_t adcConfig;

    s_gasConsecutiveErr = 0U;
    s_gasTotalErr       = 0U;
    s_gasInitialized    = false;
    s_warmupCountdown   = MQ2_WARMUP_TICKS;

    /* 1. Retrieve default ADC configuration and customize for GAU GPADC0 */
    ADC_GetDefaultConfig(&adcConfig);
    adcConfig.vrefSource            = kADC_Vref1P2V;         /* Internal 1.2V reference */
    adcConfig.inputMode             = kADC_InputSingleEnded; /* Single-ended analog input */
    adcConfig.conversionMode        = kADC_ConversionOneShot;/* Software on-demand conversion */
    adcConfig.inputGain             = kADC_InputGain1;       /* Unity input gain */
    adcConfig.resolution            = kADC_Resolution16Bit;  /* 16-bit resolution */
    adcConfig.fifoThreshold         = kADC_FifoThresholdData1;
    adcConfig.averageLength         = kADC_Average16;        /* 16x hardware averaging for stable SNR */
    adcConfig.enableInputGainBuffer = false;
    adcConfig.enableInputBufferChop = false;
    adcConfig.enableADC             = true;

    /* 2. Initialize ADC peripheral */
    ADC_Init(BOARD_MQ2_ADC_BASE, &adcConfig);

    /* 3. Enable DataReady interrupt mask in peripheral register so ADC_REG_ISR flag sets */
    ADC_EnableInterrupts(BOARD_MQ2_ADC_BASE, (uint32_t)kADC_DataReadyInterruptEnable);

    /* 4. Execute automatic calibration against internal 1.2V reference */
    status_t calStatus = ADC_DoAutoCalibration(BOARD_MQ2_ADC_BASE, kADC_CalibrationVrefInternal);
    if (calStatus != kStatus_Success)
    {
        PRINTF("[GAS ADC] Calibration warning (status=%d)\r\n", (int)calStatus);
    }

    /* 5. Map Scan Channel 0 to hardware channel kADC_CH4 (GPIO_46 / Header J7 Pin 3) */
    ADC_SetScanChannel(BOARD_MQ2_ADC_BASE, kADC_ScanChannel0, BOARD_MQ2_ADC_CHANNEL);

    s_gasInitialized = true;
}

float SENSOR_Gas_RawToPpm(uint32_t raw_adc)
{
    /* 1. Convert raw counts (0 to 32767) to pin voltage (0 to 1.2V) */
    float v_pin = ((float)raw_adc / MQ2_ADC_FULLSCALE_COUNTS) * MQ2_ADC_VREF_VOLTS;
    if (v_pin < 0.0f)
    {
        v_pin = 0.0f;
    }
    if (v_pin > MQ2_ADC_VREF_VOLTS)
    {
        v_pin = MQ2_ADC_VREF_VOLTS;
    }

    /* Below the clean-air baseline floor (e.g. <0.18V at pin), consider clean air */
    if (v_pin < MQ2_ADC_MIN_CLEAN_VOLTS)
    {
        return 0.0f;
    }

    /* 2. Compensate for the 36k top / 10k bottom resistor divider to obtain true AOUT */
    float v_aout = v_pin / MQ2_DIVIDER_RATIO;
    if (v_aout > MQ2_VCC_VOLTS)
    {
        v_aout = MQ2_VCC_VOLTS;
    }

    /* 3. Compute sensor resistance Rs: Vaout = Vcc * RL / (Rs + RL) => Rs = RL * (Vcc - Vaout) / Vaout */
    float ppm = 0.0f;
    if (v_aout > 0.05f && v_aout < (MQ2_VCC_VOLTS - 0.01f))
    {
        float rs = MQ2_RL_OHMS * (MQ2_VCC_VOLTS - v_aout) / v_aout;

        if (MQ2_R0_OHMS > 0.0f)
        {
            /* Calibrated curve: ppm = A * (Rs / R0) ^ B */
            float ratio = rs / MQ2_R0_OHMS;
            ppm = MQ2_CURVE_A * powf(ratio, MQ2_CURVE_B);
        }
        else
        {
            /* Uncalibrated relative index (0-1000): scaled by voltage */
            ppm = (v_aout / MQ2_VCC_VOLTS) * 1000.0f;
        }
    }
    else if (v_aout <= 0.05f)
    {
        ppm = 0.0f;
    }
    else
    {
        ppm = 1000.0f;
    }

    /* 4. During initial sensor warm-up (first 30s after power-on):
     * Suppress moderate heater drift (250 - 600) to avoid false boot warnings,
     * but NEVER mask genuine critical gas hazards (>= 600 PPM / Vaout >= 3.0V). */
    if (s_warmupCountdown > 0U && ppm > 250.0f && ppm < 600.0f)
    {
        ppm = 50.0f; /* Safe nominal baseline during heater stabilization */
    }

    if (ppm < 10.0f && v_aout > 0.2f)
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

    if (!s_gasInitialized)
    {
        RecordFailure(out_data);
        return false;
    }

    /* Track heater warm-up phase */
    if (s_warmupCountdown > 0U)
    {
        s_warmupCountdown--;
    }

    /* 1. Clear previous Data Ready flags (both masked interrupt and raw status) */
    ADC_ClearStatusFlags(BOARD_MQ2_ADC_BASE, (uint32_t)kADC_DataReadyInterruptFlag | (uint32_t)kADC_DataReadyRawFlag);

    /* 2. Initiate software conversion */
    ADC_DoSoftwareTrigger(BOARD_MQ2_ADC_BASE);

    /* 3. Poll for conversion completion: check both masked flag and raw flag */
    volatile uint32_t timeout = 50000U;
    while (((ADC_GetStatusFlags(BOARD_MQ2_ADC_BASE) & ((uint32_t)kADC_DataReadyInterruptFlag | (uint32_t)kADC_DataReadyRawFlag)) == 0U) && (--timeout > 0U))
    {
        __asm__ volatile ("nop");
    }

    if (timeout == 0U)
    {
        ADC_StopConversion(BOARD_MQ2_ADC_BASE);
        RecordFailure(out_data);
        return false;
    }

    /* 4. Read 16-bit packed result and stop conversion */
    uint32_t packedResult = ADC_GetConversionResult(BOARD_MQ2_ADC_BASE);
    ADC_StopConversion(BOARD_MQ2_ADC_BASE);

    int16_t signedCounts = (int16_t)(packedResult & 0xFFFFU);
    if (signedCounts < 0)
    {
        signedCounts = 0;
    }
    uint32_t rawCounts = (uint32_t)signedCounts;

    /* 5. Calculate pin voltage and true AOUT through voltage divider */
    float v_pin  = ((float)rawCounts / MQ2_ADC_FULLSCALE_COUNTS) * MQ2_ADC_VREF_VOLTS;
    float v_aout = v_pin / MQ2_DIVIDER_RATIO;
    if (v_aout > MQ2_VCC_VOLTS)
    {
        v_aout = MQ2_VCC_VOLTS;
    }

    /* 6. Convert to PPM or Relative Gas Index */
    float ppm = SENSOR_Gas_RawToPpm(rawCounts);

    out_data->raw_adc_value = rawCounts;
    out_data->voltage_v     = v_aout;
    out_data->gas_ppm       = ppm;
    out_data->is_valid      = true;
    s_gasConsecutiveErr     = 0U;

    return true;
}

bool SENSOR_Gas_IsFaulted(void)
{
    return (s_gasConsecutiveErr >= GAS_SENSOR_FAULT_THRESHOLD);
}

bool SENSOR_Gas_IsWarmingUp(void)
{
    return (s_warmupCountdown > 0U);
}

uint32_t SENSOR_Gas_GetErrorCount(void)
{
    return s_gasTotalErr;
}
