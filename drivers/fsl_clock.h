/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612
 * NXP SDK Clock Definitions
 */

#ifndef _FSL_CLOCK_H_
#define _FSL_CLOCK_H_

#include "fsl_common.h"

typedef enum _clock_attach_id {
    kFFRO_to_FLEXCOMM0 = 0,
    kFFRO_to_FLEXCOMM1 = 1,
    kFFRO_to_FLEXCOMM2 = 2,
    kFFRO_to_FLEXCOMM3 = 3,
} clock_attach_id_t;

static inline void CLOCK_AttachClk(clock_attach_id_t attach_id)
{
    (void)attach_id;
}

static inline uint32_t CLOCK_GetFlexCommClkFreq(uint32_t id)
{
    (void)id;
    return 24000000U;
}

#endif /* _FSL_CLOCK_H_ */

