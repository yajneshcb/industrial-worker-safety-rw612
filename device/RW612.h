/*
 * Copyright 2026
 * Device Header: NXP RW612 (RW612ETA2I)
 * ARM Cortex-M33 Core
 */

#ifndef _RW612_H_
#define _RW612_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Interrupt Number Definition stub */
typedef enum IRQn
{
  NonMaskableInt_IRQn   = -14,
  HardFault_IRQn        = -13,
  MemoryManagement_IRQn = -12,
  BusFault_IRQn         = -11,
  UsageFault_IRQn       = -10,
  SVCall_IRQn           = -5,
  DebugMonitor_IRQn     = -4,
  PendSV_IRQn           = -2,
  SysTick_IRQn          = -1,

  /* RW612 Specific Interrupts */
  FLEXCOMM0_IRQn        = 0,
  FLEXCOMM1_IRQn        = 1,
  FLEXCOMM2_IRQn        = 2,
  FLEXCOMM3_IRQn        = 3,
  GPIO_INTA_IRQn        = 4,
  GPIO_INTB_IRQn        = 5,
  LPADC_IRQn            = 6
} IRQn_Type;

/* Processor and Core Configuration */
#define __CM33_REV              0x0000U
#define __MPU_PRESENT           1U
#define __NVIC_PRIO_BITS        3U
#define __Vendor_SysTickConfig  0U
#define __FPU_PRESENT           1U
#define __DSP_PRESENT           1U

#ifdef __cplusplus
}
#endif

#endif /* _RW612_H_ */

