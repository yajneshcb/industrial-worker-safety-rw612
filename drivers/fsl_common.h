/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612
 * NXP SDK Common Definitions
 */

#ifndef _FSL_COMMON_H_
#define _FSL_COMMON_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int32_t status_t;

enum _status_groups {
    kStatusGroup_Generic = 0,
    kStatusGroup_I2C = 1,
    kStatusGroup_FLEXCOMM = 2
};

enum {
    kStatus_Success = 0,
    kStatus_Fail = 1,
    kStatus_ReadOnly = 2,
    kStatus_OutOfRange = 3,
    kStatus_InvalidArgument = 4,
    kStatus_Timeout = 5,
    kStatus_NoTransferInProgress = 6,
};

#define SDK_DelayAtLeastUs(us, coreClock) do { \
    volatile uint32_t count = (uint32_t)(((uint64_t)(us) * 24U) / 4U); \
    while (count--) { __asm__ volatile("nop"); } \
} while(0)

#endif /* _FSL_COMMON_H_ */

