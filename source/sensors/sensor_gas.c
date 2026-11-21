/*
 * Copyright 2026
 * Module: MQ-2 Toxic Gas & Smoke Sensor Driver
 * Platform: NXP FRDM-RW612 (Cortex-M33)
 */

#include "sensor_gas.h"
#include "board.h"
#include "app_config.h"
#include <stddef.h>

/*******************************************************************************
 * Sensor Calibration Constants & Hardware Definitions
 ******************************************************************************/
#define MQ2_ADC_MAX_COUNTS       4095.0f   /* 12-bit ADC resolution */
#define MQ2_VREF_VOLTS           3.3f      /* ADC Reference Voltage */

#define GAS_SENSOR_FAULT_THRESHOLD 5U

static uint32_t s_gasConsecutiveErr = 0U;
static uint32_t s_gasTotalErr       = 0U;
static bool     s_gasInitialized    = false;

/*******************************************************************************
 * NXP RW612 LPADC0 Hardware Register Definitions
 * Base Address: 0x400A1000U (LPADC0)
 * Channel 0 is routed to GPIO_60 (Header J3 Pin 2 / Arduino A0)
 ******************************************************************************/
#define LPADC0_BASE             0x400A1000U
#define LPADC0_CTRL             (*(volatile uint32_t *)(LPADC0_BASE + 0x08U))
#define LPADC0_STAT             (*(volatile uint32_t *)(LPADC0_BASE + 0x0CU))
#define LPADC0_CFG              (*(volatile uint32_t *)(LPADC0_BASE + 0x18U))
#define LPADC0_SWTRIG           (*(volatile uint32_t *)(LPADC0_BASE + 0x20U))
#define LPADC0_CMD1             (*(volatile uint32_t *)(LPADC0_BASE + 0x28U))
#define LPADC0_RESFIFO          (*(volatile uint32_t *)(LPADC0_BASE + 0x300U))
#define LPADC0_FCTRL            (*(volatile uint32_t *)(LPADC0_BASE + 0x304U))

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

void SENSOR_Gas_Init(void)
{
    s_gasConsecutiveErr = 0U;
    s_gasTotalErr       = 0U;

    /* Initialize hardware LPADC0: Enable ADC and configure Command 1 for Channel 0 */
    LPADC0_CTRL = (1U << 0); /* ADC Enable */
    LPADC0_CFG  = 0U;        /* Default 12-bit single-ended mode */
    LPADC0_CMD1 = (BOARD_MQ2_ADC_CHANNEL & 0x1FU); /* Channel 0 (GPIO_60) */

    s_gasInitialized = true;
}

float SENSOR_Gas_RawToPpm(uint32_t raw_adc)
{
    float voltage = ((float)raw_adc / MQ2_ADC_MAX_COUNTS) * MQ2_VREF_VOLTS;

    /* Gas curve scaling factor for MQ-2 SnO2 sensor */
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

    if (!s_gasInitialized)
    {
        RecordFailure(out_data);
        return false;
    }

    /* Trigger LPADC conversion on Channel 0 */
    LPADC0_SWTRIG = (1U << 0);

    /* Wait for conversion completion in RESFIFO */
    volatile uint32_t timeout = 50000U;
    while (((LPADC0_FCTRL & 0x1FU) == 0U) && (--timeout > 0U))
    {
        __asm__ volatile("nop");
    }

    if (timeout == 0U)
    {
        /* ADC conversion timeout or hardware not responding */
        RecordFailure(out_data);
        return false;
    }

    /* Read converted raw 12-bit ADC result */
    uint32_t fifoWord = LPADC0_RESFIFO;
    uint32_t rawCounts = (fifoWord >> 3U) & 0x0FFFU;

    float voltage = ((float)rawCounts / MQ2_ADC_MAX_COUNTS) * MQ2_VREF_VOLTS;
    float ppm     = SENSOR_Gas_RawToPpm(rawCounts);

    out_data->raw_adc_value = rawCounts;
    out_data->voltage_v     = voltage;
    out_data->gas_ppm       = ppm;
    out_data->is_valid      = true;
    s_gasConsecutiveErr     = 0U;

    return true;
}

bool SENSOR_Gas_IsFaulted(void)
{
    return (s_gasConsecutiveErr >= GAS_SENSOR_FAULT_THRESHOLD);
}

uint32_t SENSOR_Gas_GetErrorCount(void)
{
    return s_gasTotalErr;
}
