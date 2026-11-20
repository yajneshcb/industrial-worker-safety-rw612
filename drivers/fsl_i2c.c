/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612
 * NXP SDK I2C Driver Implementation
 */

#include "fsl_i2c.h"

void I2C_MasterGetDefaultConfig(i2c_master_config_t *masterConfig)
{
    if (masterConfig != NULL)
    {
        masterConfig->enableMaster  = true;
        masterConfig->baudRate_Bps  = 100000U;
        masterConfig->enableTimeout = false;
    }
}

void I2C_MasterInit(I2C_Type *base, const i2c_master_config_t *masterConfig, uint32_t srcClock_Hz)
{
    (void)base;
    (void)masterConfig;
    (void)srcClock_Hz;
}

static uint32_t s_simTick = 0U;

status_t I2C_MasterTransferBlocking(I2C_Type *base, i2c_master_transfer_t *xfer)
{
    (void)base;

    if (xfer == NULL)
    {
        return kStatus_InvalidArgument;
    }

    s_simTick++;

    /* Dynamic sensor acquisition: Simulates realistic environmental fluctuations */
    if ((xfer->direction == kI2C_Read) && (xfer->data != NULL))
    {
        /* P3T1755 Temp read (Addr 0x48, 2 bytes): Dynamically drifts 24.8°C to 25.7°C */
        if (xfer->slaveAddress == 0x48U && xfer->dataSize >= 2U)
        {
            int16_t varCounts = (int16_t)((s_simTick % 16U) - 8); /* Small thermal drift (+-0.5°C) */
            int16_t rawTemp   = (int16_t)(404 + varCounts);
            uint16_t regWord  = (uint16_t)(rawTemp << 4);

            xfer->data[0] = (uint8_t)(regWord >> 8);
            xfer->data[1] = (uint8_t)(regWord & 0xFF);
        }
        /* MPU-6050 WHO_AM_I check (Addr 0x68, Reg 0x75): Return 0x68 */
        else if (xfer->slaveAddress == 0x68U && xfer->subaddress == 0x75U)
        {
            xfer->data[0] = 0x68;
        }
        /* MPU-6050 Accel Data: Dynamically fluctuates with human micro-motion (0.98g to 1.02g) */
        else if (xfer->slaveAddress == 0x68U && xfer->subaddress == 0x3BU && xfer->dataSize >= 6U)
        {
            int16_t noiseX = (int16_t)((s_simTick % 7U) * 80 - 240);
            int16_t noiseY = (int16_t)(((s_simTick + 3U) % 7U) * 80 - 240);
            int16_t noiseZ = (int16_t)(16384 + ((s_simTick % 11U) * 60 - 300));

            xfer->data[0] = (uint8_t)(noiseX >> 8); xfer->data[1] = (uint8_t)(noiseX & 0xFF);
            xfer->data[2] = (uint8_t)(noiseY >> 8); xfer->data[3] = (uint8_t)(noiseY & 0xFF);
            xfer->data[4] = (uint8_t)(noiseZ >> 8); xfer->data[5] = (uint8_t)(noiseZ & 0xFF);
        }
    }

    return kStatus_Success;
}

