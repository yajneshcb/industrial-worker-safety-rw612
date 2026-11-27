/*
 * Copyright 2026
 * Module: MPU-6050 Motion & Fall Detection Sensor
 * Platform: NXP FRDM-RW612
 */

#include "sensor_mpu6050.h"
#include "board.h"
#include "app_config.h"

#include "fsl_clock.h"
#include "fsl_i2c.h"
#include "fsl_debug_console.h"
#include <math.h>

/*******************************************************************************
 * MPU-6050 Register Definitions & Scale Factors
 ******************************************************************************/
#define MPU6050_REG_SMPLRT_DIV       0x19U
#define MPU6050_REG_CONFIG           0x1AU
#define MPU6050_REG_GYRO_CONFIG      0x1BU
#define MPU6050_REG_ACCEL_CONFIG     0x1CU
#define MPU6050_REG_ACCEL_XOUT_H     0x3BU
#define MPU6050_REG_PWR_MGMT_1       0x6BU
#define MPU6050_REG_WHO_AM_I         0x75U

#define MPU6050_EXPECTED_WHO_AM_I    0x68U

/* Sensitivity scale factors for ±8g and ±250 deg/s */
#define MPU6050_ACCEL_SCALE_8G       4096.0f    /* LSB per g for ±8g range (captures impact spikes > 2.5g) */
#define MPU6050_GYRO_SCALE_250DPS    131.0f     /* LSB per (deg/s) */

#define MPU6050_MAX_RETRIES          3U
#define MPU6050_FAULT_THRESHOLD      5U
#define MPU6050_REINIT_PERIOD        20U   /* retry configuration every N failed reads (~2 s) */

/*******************************************************************************
 * Module State
 ******************************************************************************/
static bool     s_mpuInitialized    = false;
static uint32_t s_mpuConsecutiveErr = 0U;
static uint32_t s_mpuTotalErr       = 0U;
static uint32_t s_mpuReinitCounter  = 0U;

/*******************************************************************************
 * Internal Helpers
 ******************************************************************************/

static status_t MPU6050_WriteByte(uint8_t reg, uint8_t val)
{
    i2c_master_transfer_t xfer;
    uint8_t data = val;

    xfer.flags          = kI2C_TransferDefaultFlag;
    xfer.slaveAddress   = BOARD_MPU6050_I2C_ADDR;
    xfer.direction      = kI2C_Write;
    xfer.subaddress     = reg;
    xfer.subaddressSize = 1U;
    xfer.data           = &data;
    xfer.dataSize       = 1U;

    return I2C_MasterTransferBlocking(BOARD_MPU6050_I2C_BASE, &xfer);
}

static status_t MPU6050_ReadBytes(uint8_t reg, uint8_t *buf, size_t len)
{
    i2c_master_transfer_t xfer;

    xfer.flags          = kI2C_TransferDefaultFlag;
    xfer.slaveAddress   = BOARD_MPU6050_I2C_ADDR;
    xfer.direction      = kI2C_Read;
    xfer.subaddress     = reg;
    xfer.subaddressSize = 1U;
    xfer.data           = buf;
    xfer.dataSize       = len;

    return I2C_MasterTransferBlocking(BOARD_MPU6050_I2C_BASE, &xfer);
}

static void RecordFailure(Mpu6050Data_t *out_data)
{
    s_mpuConsecutiveErr++;
    s_mpuTotalErr++;

    if (out_data != NULL)
    {
        out_data->accel_x_g   = 0.0f;
        out_data->accel_y_g   = 0.0f;
        out_data->accel_z_g   = 0.0f;
        out_data->gyro_x_dps  = 0.0f;
        out_data->gyro_y_dps  = 0.0f;
        out_data->gyro_z_dps  = 0.0f;
        out_data->is_valid    = false;
    }
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

void SENSOR_MPU6050_Init(void);

/* Probe + configure the MPU-6050 (I2C master must already be initialised). */
static bool MPU6050_TryConfigure(void)
{
    uint8_t whoami = 0U;

    if (MPU6050_ReadBytes(MPU6050_REG_WHO_AM_I, &whoami, 1U) != kStatus_Success)
    {
        uint8_t altWhoami = 0U;
        i2c_master_transfer_t altXfer;
        altXfer.flags          = kI2C_TransferDefaultFlag;
        altXfer.slaveAddress   = 0x69U;
        altXfer.direction      = kI2C_Read;
        altXfer.subaddress     = MPU6050_REG_WHO_AM_I;
        altXfer.subaddressSize = 1U;
        altXfer.data           = &altWhoami;
        altXfer.dataSize       = 1U;
        if (I2C_MasterTransferBlocking(BOARD_MPU6050_I2C_BASE, &altXfer) == kStatus_Success)
        {
            PRINTF(">> MPU: responded at 0x69 (AD0 is HIGH) instead of 0x68 - tie AD0 to GND\r\n");
        }
        else
        {
            PRINTF(">> MPU: no I2C response at 0x%02X (check SDA J2-18/SCL J2-20/3V3/GND)\r\n",
                   (unsigned int)BOARD_MPU6050_I2C_ADDR);
        }
        return false;
    }

    /* 0x68 = genuine MPU-6050; many GY-521 clones report 0x70/0x71/0x72 but share the same
     * accelerometer register map, so accept them instead of dead-locking in a permanent fault. */
    if ((whoami != MPU6050_EXPECTED_WHO_AM_I) && (whoami != 0x70U) &&
        (whoami != 0x71U) && (whoami != 0x72U))
    {
        PRINTF(">> MPU: unexpected WHO_AM_I=0x%02X (expected 0x68)\r\n", (unsigned int)whoami);
        return false;
    }

    (void)MPU6050_WriteByte(MPU6050_REG_PWR_MGMT_1, 0x00U);   /* wake up */
    (void)MPU6050_WriteByte(MPU6050_REG_SMPLRT_DIV, 0x07U);   /* 1 kHz / 8 = 125 Hz */
    /* DLPF_CFG=3 (44 Hz). The old value 0x06 (5 Hz) smoothed a short impact spike far below the
     * 2.5 g threshold, so falls were effectively undetectable. */
    (void)MPU6050_WriteByte(MPU6050_REG_CONFIG, 0x03U);
    (void)MPU6050_WriteByte(MPU6050_REG_GYRO_CONFIG, 0x00U);  /* +/-250 dps */
    (void)MPU6050_WriteByte(MPU6050_REG_ACCEL_CONFIG, 0x10U); /* +/-8 g */

    PRINTF(">> MPU: configured, WHO_AM_I=0x%02X\r\n", (unsigned int)whoami);
    return true;
}

void SENSOR_MPU6050_Init(void)
{
    i2c_master_config_t masterConfig;

    /* Attach clock to Flexcomm 2 and initialize I2C master */
    CLOCK_AttachClk(BOARD_MPU6050_I2C_CLK_ATTACH);
    I2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = BOARD_MPU6050_I2C_BAUDRATE;
    I2C_MasterInit(BOARD_MPU6050_I2C_BASE, &masterConfig, BOARD_MPU6050_I2C_CLK_FREQ);

    s_mpuConsecutiveErr = 0U;
    s_mpuTotalErr       = 0U;
    s_mpuReinitCounter  = 0U;

    s_mpuInitialized = MPU6050_TryConfigure();
    if (!s_mpuInitialized)
    {
        s_mpuTotalErr++;
    }
}

bool SENSOR_MPU6050_Read(Mpu6050Data_t *out_data)
{
    uint8_t rawBuf[14];

    if (out_data == NULL)
    {
        return false;
    }

    if (!s_mpuInitialized)
    {
        /* Previously a failed init was permanent until reset. Retry periodically so a late-powered
         * or re-plugged sensor recovers on its own. */
        if ((++s_mpuReinitCounter >= MPU6050_REINIT_PERIOD) && MPU6050_TryConfigure())
        {
            s_mpuInitialized    = true;
            s_mpuConsecutiveErr = 0U;
        }
        if (!s_mpuInitialized)
        {
            s_mpuReinitCounter = (s_mpuReinitCounter >= MPU6050_REINIT_PERIOD) ? 0U : s_mpuReinitCounter;
            RecordFailure(out_data);
            return false;
        }
    }

    for (uint32_t attempt = 0U; attempt < MPU6050_MAX_RETRIES; attempt++)
    {
        /* Burst read 14 bytes: Accel (6) + Temp (2) + Gyro (6) */
        if (MPU6050_ReadBytes(MPU6050_REG_ACCEL_XOUT_H, rawBuf, 14U) == kStatus_Success)
        {
            int16_t ax = (int16_t)(((uint16_t)rawBuf[0]  << 8) | (uint16_t)rawBuf[1]);
            int16_t ay = (int16_t)(((uint16_t)rawBuf[2]  << 8) | (uint16_t)rawBuf[3]);
            int16_t az = (int16_t)(((uint16_t)rawBuf[4]  << 8) | (uint16_t)rawBuf[5]);

            int16_t gx = (int16_t)(((uint16_t)rawBuf[8]  << 8) | (uint16_t)rawBuf[9]);
            int16_t gy = (int16_t)(((uint16_t)rawBuf[10] << 8) | (uint16_t)rawBuf[11]);
            int16_t gz = (int16_t)(((uint16_t)rawBuf[12] << 8) | (uint16_t)rawBuf[13]);

            out_data->accel_x_g  = (float)ax / MPU6050_ACCEL_SCALE_8G;
            out_data->accel_y_g  = (float)ay / MPU6050_ACCEL_SCALE_8G;
            out_data->accel_z_g  = (float)az / MPU6050_ACCEL_SCALE_8G;

            out_data->gyro_x_dps = (float)gx / MPU6050_GYRO_SCALE_250DPS;
            out_data->gyro_y_dps = (float)gy / MPU6050_GYRO_SCALE_250DPS;
            out_data->gyro_z_dps = (float)gz / MPU6050_GYRO_SCALE_250DPS;

            out_data->is_valid   = true;
            s_mpuConsecutiveErr  = 0U;
            return true;
        }
    }

    RecordFailure(out_data);
    return false;
}

float SENSOR_MPU6050_GetTotalAccelG(const Mpu6050Data_t *data)
{
    if ((data == NULL) || (!data->is_valid))
    {
        return 0.0f;
    }

    return sqrtf((data->accel_x_g * data->accel_x_g) +
                 (data->accel_y_g * data->accel_y_g) +
                 (data->accel_z_g * data->accel_z_g));
}

bool SENSOR_MPU6050_IsFaulted(void)
{
    return (s_mpuConsecutiveErr >= MPU6050_FAULT_THRESHOLD);
}

uint32_t SENSOR_MPU6050_GetErrorCount(void)
{
    return s_mpuTotalErr;
}
