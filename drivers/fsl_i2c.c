/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612
 * Hardware Flexcomm I2C Master Driver Implementation
 */

#include "fsl_i2c.h"

/* Flexcomm I2C Register Bitfields */
#define I2C_CFG_MSTEN_MASK          (1U << 0)
#define I2C_STAT_MSTPENDING_MASK    (1U << 0)
#define I2C_STAT_MSTSTATE_MASK      (7U << 1)
#define I2C_STAT_MSTSTATE_IDLE      (0U << 1)
#define I2C_STAT_MSTSTATE_RXREADY   (1U << 1)
#define I2C_STAT_MSTSTATE_TXREADY   (2U << 1)
#define I2C_STAT_MSTSTATE_NACKADDR  (3U << 1)
#define I2C_STAT_MSTSTATE_NACKDATA  (4U << 1)

#define I2C_MSTCTL_MSTCONTINUE_MASK (1U << 0)
#define I2C_MSTCTL_MSTSTART_MASK    (1U << 1)
#define I2C_MSTCTL_MSTSTOP_MASK     (1U << 2)

#define I2C_TIMEOUT_CYCLES          100000U

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
    if (base == NULL || masterConfig == NULL)
    {
        return;
    }

    /* Configure clock divider for desired I2C baudrate */
    if (srcClock_Hz > 0U && masterConfig->baudRate_Bps > 0U)
    {
        uint32_t div = srcClock_Hz / (masterConfig->baudRate_Bps * 4U);
        if (div > 0U)
        {
            div -= 1U;
        }
        base->CLKDIV = div & 0xFFFFU;
    }

    /* Enable I2C Master operation */
    if (masterConfig->enableMaster)
    {
        base->CFG = I2C_CFG_MSTEN_MASK;
    }
    else
    {
        base->CFG = 0U;
    }
}

static inline status_t I2C_WaitPending(I2C_Type *base)
{
    volatile uint32_t timeout = I2C_TIMEOUT_CYCLES;
    while ((!(base->STAT & I2C_STAT_MSTPENDING_MASK)) && (--timeout > 0U))
    {
        __asm__ volatile("nop");
    }
    return (timeout > 0U) ? kStatus_Success : kStatus_Timeout;
}

status_t I2C_MasterTransferBlocking(I2C_Type *base, i2c_master_transfer_t *xfer)
{
    if (base == NULL || xfer == NULL)
    {
        return kStatus_InvalidArgument;
    }

    /* Wait until master interface is ready */
    if (I2C_WaitPending(base) != kStatus_Success)
    {
        return kStatus_Timeout;
    }

    /* 1. If subaddress is specified, write slave address (write) + subaddress first */
    if (xfer->subaddressSize > 0U)
    {
        /* Send Slave Address for Write: (ADDR << 1) | 0 */
        base->MSTDAT = (uint32_t)((xfer->slaveAddress << 1U) | 0U);
        base->MSTCTL = I2C_MSTCTL_MSTSTART_MASK;

        if (I2C_WaitPending(base) != kStatus_Success)
        {
            base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
            return kStatus_Timeout;
        }

        uint32_t state = base->STAT & I2C_STAT_MSTSTATE_MASK;
        if (state != I2C_STAT_MSTSTATE_TXREADY)
        {
            base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
            return kStatus_Fail; /* Slave not responding (NACK) */
        }

        /* Send Subaddress byte(s) MSB first */
        for (int8_t i = (int8_t)(xfer->subaddressSize - 1U); i >= 0; i--)
        {
            base->MSTDAT = (uint32_t)((xfer->subaddress >> (i * 8)) & 0xFFU);
            base->MSTCTL = I2C_MSTCTL_MSTCONTINUE_MASK;

            if (I2C_WaitPending(base) != kStatus_Success)
            {
                base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
                return kStatus_Timeout;
            }

            state = base->STAT & I2C_STAT_MSTSTATE_MASK;
            if (state != I2C_STAT_MSTSTATE_TXREADY)
            {
                base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
                return kStatus_Fail;
            }
        }
    }

    /* 2. Execute Data Phase */
    if (xfer->direction == kI2C_Write)
    {
        if (xfer->subaddressSize == 0U)
        {
            base->MSTDAT = (uint32_t)((xfer->slaveAddress << 1U) | 0U);
            base->MSTCTL = I2C_MSTCTL_MSTSTART_MASK;

            if (I2C_WaitPending(base) != kStatus_Success)
            {
                base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
                return kStatus_Timeout;
            }

            if ((base->STAT & I2C_STAT_MSTSTATE_MASK) != I2C_STAT_MSTSTATE_TXREADY)
            {
                base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
                return kStatus_Fail;
            }
        }

        for (size_t i = 0U; i < xfer->dataSize; i++)
        {
            base->MSTDAT = (uint32_t)xfer->data[i];
            base->MSTCTL = I2C_MSTCTL_MSTCONTINUE_MASK;

            if (I2C_WaitPending(base) != kStatus_Success)
            {
                base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
                return kStatus_Timeout;
            }

            if ((base->STAT & I2C_STAT_MSTSTATE_MASK) != I2C_STAT_MSTSTATE_TXREADY)
            {
                base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
                return kStatus_Fail;
            }
        }

        base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
    }
    else /* kI2C_Read */
    {
        /* Generate START (or repeated START) with Slave Address (Read): (ADDR << 1) | 1 */
        base->MSTDAT = (uint32_t)((xfer->slaveAddress << 1U) | 1U);
        base->MSTCTL = I2C_MSTCTL_MSTSTART_MASK;

        if (I2C_WaitPending(base) != kStatus_Success)
        {
            base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
            return kStatus_Timeout;
        }

        uint32_t state = base->STAT & I2C_STAT_MSTSTATE_MASK;
        if (state != I2C_STAT_MSTSTATE_RXREADY)
        {
            base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
            return kStatus_Fail; /* Slave not responding on read address */
        }

        for (size_t i = 0U; i < xfer->dataSize; i++)
        {
            if (i > 0U)
            {
                base->MSTCTL = I2C_MSTCTL_MSTCONTINUE_MASK;
                if (I2C_WaitPending(base) != kStatus_Success)
                {
                    base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
                    return kStatus_Timeout;
                }
            }

            xfer->data[i] = (uint8_t)(base->MSTDAT & 0xFFU);
        }

        /* Complete read by asserting STOP */
        base->MSTCTL = I2C_MSTCTL_MSTSTOP_MASK;
    }

    return kStatus_Success;
}
