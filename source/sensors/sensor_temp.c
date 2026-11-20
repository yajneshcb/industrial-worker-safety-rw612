/*
 * Copyright 2026
 * Module: Onboard Digital Temperature Sensor (P3T1755)
 * Platform: NXP FRDM-RW612
 */

#include "sensor_temp.h"
#include "board.h"
#include "app_config.h"

#include "fsl_clock.h"
#include "fsl_i2c.h"

/*******************************************************************************
 * P3T1755 register map / conversion constants
 ******************************************************************************/
#define P3T1755_REG_TEMP        0x00U   /* Temperature, read-only, 16 bit        */
#define P3T1755_REG_CONFIG      0x01U   /* Configuration (not modified here)     */

/* Temperature register: 12-bit two's complement, left-justified in bits 15..4. */
#define P3T1755_RAW_SHIFT       4
#define P3T1755_LSB_CELSIUS     0.0625f

/*******************************************************************************
 * Module state
 ******************************************************************************/
static bool     s_initialized       = false;
static uint32_t s_consecutiveErrors = 0U;
static uint32_t s_totalErrors       = 0U;

/*******************************************************************************
 * Helpers
 ******************************************************************************/

/* Read two bytes starting at 'reg' (MSB first). */
static status_t P3T1755_ReadReg16(uint8_t reg, uint8_t buf[2])
{
    i2c_master_transfer_t xfer;

    xfer.flags          = kI2C_TransferDefaultFlag;
    xfer.slaveAddress   = BOARD_TEMP_I2C_ADDR;
    xfer.direction      = kI2C_Read;
    xfer.subaddress     = reg;
    xfer.subaddressSize = 1U;
    xfer.data           = buf;
    xfer.dataSize       = 2U;

    return I2C_MasterTransferBlocking(BOARD_TEMP_I2C_BASE, &xfer);
}

static int16_t P3T1755_RawFromBytes(const uint8_t buf[2])
{
    int16_t word = (int16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
    return (int16_t)(word >> P3T1755_RAW_SHIFT);   /* arithmetic shift keeps sign */
}

static void RecordFailure(TempSensorData_t *out_data)
{
    s_consecutiveErrors++;
    s_totalErrors++;

    if (out_data != NULL)
    {
        out_data->temperature_c = 0.0f;
        out_data->raw           = 0;
        out_data->is_valid      = false;
    }
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

void SENSOR_Temp_Init(void)
{
    i2c_master_config_t masterConfig;

    CLOCK_AttachClk(BOARD_TEMP_I2C_CLK_ATTACH);

    I2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = BOARD_TEMP_I2C_BAUDRATE;
    I2C_MasterInit(BOARD_TEMP_I2C_BASE, &masterConfig, BOARD_TEMP_I2C_CLK_FREQ);

    s_consecutiveErrors = 0U;
    s_totalErrors       = 0U;
    s_initialized       = true;
}

bool SENSOR_Temp_Read(TempSensorData_t *out_data)
{
    uint8_t buf[2];

    if (out_data == NULL)
    {
        return false;
    }

    if (!s_initialized)
    {
        RecordFailure(out_data);
        return false;
    }

    for (uint32_t attempt = 0U; attempt < TEMP_SENSOR_MAX_RETRIES; attempt++)
    {
        if (P3T1755_ReadReg16(P3T1755_REG_TEMP, buf) == kStatus_Success)
        {
            int16_t raw  = P3T1755_RawFromBytes(buf);
            float   degC = (float)raw * P3T1755_LSB_CELSIUS;

            /* Reject values outside the sensor's rated range (bus glitch / wrong device). */
            if ((degC >= TEMP_SENSOR_MIN_VALID_C) && (degC <= TEMP_SENSOR_MAX_VALID_C))
            {
                out_data->raw           = raw;
                out_data->temperature_c = degC;
                out_data->is_valid      = true;
                s_consecutiveErrors     = 0U;
                return true;
            }
        }
    }

    RecordFailure(out_data);
    return false;
}

bool SENSOR_Temp_IsFaulted(void)
{
    return (s_consecutiveErrors >= TEMP_SENSOR_FAULT_THRESHOLD);
}

uint32_t SENSOR_Temp_GetErrorCount(void)
{
    return s_totalErrors;
}
