/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612
 * NXP SDK I2C Driver Header
 */

#ifndef _FSL_I2C_H_
#define _FSL_I2C_H_

#include "fsl_common.h"

typedef struct {
    volatile uint32_t CFG;
    volatile uint32_t STAT;
    volatile uint32_t INTENSET;
    volatile uint32_t INTENCLR;
    volatile uint32_t TIMEOUT;
    volatile uint32_t CLKDIV;
    volatile uint32_t INTSTAT;
    volatile uint32_t RESERVED[1];
    volatile uint32_t MSTDAT;
    volatile uint32_t MSTCTL;
    volatile uint32_t MSTTIME;
} I2C_Type;

#define I2C0 ((I2C_Type *)0x40038000u)
#define I2C1 ((I2C_Type *)0x40039000u)
#define I2C2 ((I2C_Type *)0x4003A000u)
#define I2C3 ((I2C_Type *)0x4003B000u)

typedef enum _i2c_direction {
    kI2C_Write = 0U,
    kI2C_Read  = 1U,
} i2c_direction_t;

enum _i2c_master_transfer_flags {
    kI2C_TransferDefaultFlag       = 0x00U,
    kI2C_TransferNoStopFlag        = 0x01U,
    kI2C_TransferRepeatedStartFlag = 0x02U,
};

typedef struct _i2c_master_config {
    bool enableMaster;
    uint32_t baudRate_Bps;
    bool enableTimeout;
} i2c_master_config_t;

typedef struct _i2c_master_transfer {
    uint32_t flags;
    uint8_t slaveAddress;
    i2c_direction_t direction;
    uint32_t subaddress;
    uint8_t subaddressSize;
    uint8_t *data;
    size_t dataSize;
} i2c_master_transfer_t;

#ifdef __cplusplus
extern "C" {
#endif

void I2C_MasterGetDefaultConfig(i2c_master_config_t *masterConfig);
void I2C_MasterInit(I2C_Type *base, const i2c_master_config_t *masterConfig, uint32_t srcClock_Hz);
status_t I2C_MasterTransferBlocking(I2C_Type *base, i2c_master_transfer_t *xfer);

#ifdef __cplusplus
}
#endif

#endif /* _FSL_I2C_H_ */

