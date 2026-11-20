/*
 * Copyright 2026
 * Board: NXP FRDM-RW612 (RW612ETA2I)
 * Industrial Worker Safety Monitoring and Emergency Alert System
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BOARD_NAME "FRDM-RW612"

/* Debug console UART configuration */
#define BOARD_DEBUG_UART_TYPE     "FLEXCOMM"
#define BOARD_DEBUG_UART_BASEADDR 0x4003B000u
#define BOARD_DEBUG_UART_CLK_FREQ 24000000u
#define BOARD_DEBUG_UART_BAUDRATE 115200

/* Onboard User / Boot Button (SW2) -> SOS Emergency Input */
#define BOARD_SW2_GPIO_PORT       0U
#define BOARD_SW2_GPIO_PIN        24U
#define BOARD_SW2_NAME            "SW2_SOS"

/* Onboard Digital Temperature Sensor (P3T1755, Flexcomm 2 I2C on GPIO16/GPIO17) */
#define BOARD_TEMP_I2C_BASE       I2C2                       /* from SDK RW612.h */
#define BOARD_TEMP_I2C_CLK_ATTACH kFFRO_to_FLEXCOMM2         /* VERIFY vs SDK clock enums */
#define BOARD_TEMP_I2C_CLK_FREQ   CLOCK_GetFlexCommClkFreq(2U)
#define BOARD_TEMP_I2C_BAUDRATE   100000U                    /* 100 kHz for bring-up */
#define BOARD_TEMP_I2C_ADDR       0x48U                      /* 7-bit address */
#define BOARD_TEMP_I2C_SDA_PIN    16U
#define BOARD_TEMP_I2C_SCL_PIN    17U

/* External MPU6050 Motion Sensor (Header J5: Flexcomm 2 I2C on GPIO_16 / GPIO_17) */
#define BOARD_MPU6050_I2C_BASE       I2C2
#define BOARD_MPU6050_I2C_CLK_ATTACH kFFRO_to_FLEXCOMM2
#define BOARD_MPU6050_I2C_CLK_FREQ   CLOCK_GetFlexCommClkFreq(2U)
#define BOARD_MPU6050_I2C_BAUDRATE   100000U
#define BOARD_MPU6050_I2C_ADDR       0x68U
#define BOARD_MPU6050_SDA_PIN        16U
#define BOARD_MPU6050_SCL_PIN        17U

/* External MQ-2 Gas Sensor (LPADC) */
#define BOARD_MQ2_ADC_CHANNEL     0U
#define BOARD_MQ2_ADC_PIN         60U

/* Actuator GPIOs (Buzzer & Status LEDs on Port 0) */
#define BOARD_BUZZER_GPIO_PIN          4U  /* Active Buzzer Module */
#define BOARD_LED_NORMAL_PIN           5U  /* Normal Status LED (Green) */
#define BOARD_LED_WARNING_PIN          6U  /* Warning Status LED (Yellow) */
#define BOARD_LED_EMERGENCY_PIN        7U  /* Emergency Status LED (Red) */

/*******************************************************************************
 * API Declarations (Prototypes only)
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

void BOARD_InitDebugConsole(void);
void BOARD_InitHardware(void);

#if defined(__cplusplus)
}
#endif

#endif /* _BOARD_H_ */

