/*
 * Copyright 2026
 * Project: Industrial Worker Safety Monitoring and Emergency Alert System
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 *
 * Board, peripheral, pin assignments, thresholds, and timing parameters.
 */

#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

/* Provided by source/board_port/hardware_init.c */
#if defined(__cplusplus)
extern "C" {
#endif
void BOARD_InitHardware(void);
#if defined(__cplusplus)
}
#endif

/*******************************************************************************
 * System Sampling & Timing Configurations
 ******************************************************************************/
#define IMU_POLL_INTERVAL_MS           20U    /* Fast 50 Hz motion/fall sampling */
#define SENSOR_POLL_INTERVAL_MS        100U   /* Main sensor sampling & edge decision (10 Hz) */
#define TELEMETRY_REPORT_INTERVAL_MS   1000U  /* Periodic reporting interval (1 Hz) */
#define EMERGENCY_REPORT_INTERVAL_MS   200U   /* High-priority reporting interval (5 Hz) */

/*******************************************************************************
 * Board Pin & Peripheral Mapping (NXP FRDM-RW612)
 * Confirmed with MCUXpresso SDK 2.14+ Driver Examples & Board Definitions
 ******************************************************************************/

/* ---- I2C Bus: Shared by P3T1755 (0x48) and MPU-6050 (0x68) ----
 * Flexcomm 2 I2C on GPIO_16 (SDA) = Header J2 Pin 18, GPIO_17 (SCL) = Header J2 Pin 20.
 * GND on Header J2 Pin 14 (or J3 Pin 14).
 * 3.3V Power on Header J3 Pin 4 (or silkscreen 3V3 test point). */
#define BOARD_I2C_BASE                I2C2
#define BOARD_I2C_CLK_ATTACH          kSFRO_to_FLEXCOMM2       /* 16 MHz SFRO per SDK I2C examples */
#define BOARD_I2C_CLK_FREQ            CLOCK_GetFlexCommClkFreq(2U)
#define BOARD_I2C_BAUDRATE            100000U                  /* 100 kHz standard mode */
#define BOARD_I2C_SDA_GPIO            16U                      /* Header J2 Pin 18 */
#define BOARD_I2C_SCL_GPIO            17U                      /* Header J2 Pin 20 */

#define BOARD_TEMP_I2C_BASE           BOARD_I2C_BASE
#define BOARD_TEMP_I2C_CLK_ATTACH     BOARD_I2C_CLK_ATTACH
#define BOARD_TEMP_I2C_CLK_FREQ       BOARD_I2C_CLK_FREQ
#define BOARD_TEMP_I2C_BAUDRATE       BOARD_I2C_BAUDRATE
#define BOARD_TEMP_I2C_ADDR           0x48U                    /* Onboard P3T1755, 7-bit address */

#define BOARD_MPU6050_I2C_BASE        BOARD_I2C_BASE
#define BOARD_MPU6050_I2C_CLK_ATTACH  BOARD_I2C_CLK_ATTACH
#define BOARD_MPU6050_I2C_CLK_FREQ    BOARD_I2C_CLK_FREQ
#define BOARD_MPU6050_I2C_BAUDRATE    BOARD_I2C_BAUDRATE
#define BOARD_MPU6050_I2C_ADDR        0x68U                    /* MPU-6050, AD0 pulled to GND */

/* ---- MQ-2 Toxic Gas & Smoke Sensor on GAU GPADC0 ----
 * NXP FRDM-RW612 ADC example: GAU_GPADC0, channel kADC_CH4 = GPIO_46 = Header J7 Pin 3.
 * ADC input voltage range is 0 - 1.2 V with a ~0.2 V minimum operating point.
 * Voltage divider: 36 kΩ top (AOUT side) + 10 kΩ bottom (GND side)
 * Divider ratio: 10 / (36 + 10) = 0.2174. At AOUT = 5.0 V, Vpin = 1.087 V (< 1.2 V).
 * Clean-air AOUT (~0.9-1.0 V) gives Vpin ~0.20-0.22 V, safely above the 0.2 V floor. */
#define BOARD_MQ2_ADC_BASE            GAU_GPADC0
#define BOARD_MQ2_ADC_CHANNEL         kADC_CH4
#define BOARD_MQ2_ADC_GPIO            46U                      /* Header J7 Pin 3 */

#define MQ2_DIVIDER_R_TOP_OHMS        36000.0f                 /* 36 kΩ top resistor */
#define MQ2_DIVIDER_R_BOTTOM_OHMS     10000.0f                 /* 10 kΩ bottom resistor */
#define MQ2_DIVIDER_RATIO             (MQ2_DIVIDER_R_BOTTOM_OHMS / (MQ2_DIVIDER_R_TOP_OHMS + MQ2_DIVIDER_R_BOTTOM_OHMS))

#define MQ2_ADC_VREF_VOLTS            1.2f                     /* GAU GPADC internal reference */
#define MQ2_ADC_FULLSCALE_COUNTS      32768.0f                 /* 16-bit single-ended full-scale */
#define MQ2_ADC_MIN_CLEAN_VOLTS       0.18f                    /* Below this threshold, treat as clean air */

/* MQ-2 Electrical & Calibration Settings */
#define MQ2_VCC_VOLTS                 5.0f
#define MQ2_RL_OHMS                   5000.0f
#define MQ2_WARMUP_TICKS              300U                     /* 30-sec warm-up lockout to avoid boot false alarms */
#define MQ2_R0_OHMS                   0.0f                     /* 0 = Uncalibrated: reports relative gas index (0-1000) */
#define MQ2_CURVE_A                   574.25f                  /* LPG curve fit: ppm = A * (Rs/R0)^B */
#define MQ2_CURVE_B                   (-2.222f)

/* ---- Onboard SOS Push Button: SW2 = GPIO_11 on Port 0 (Active LOW) ---- */
#define APP_SOS_GPIO_PORT             0U
#define APP_SOS_GPIO_PIN              11U                      /* Onboard SW2 button */

/* ---- Actuator GPIOs (Port 0) — Official Quick Start Guide Mappings ---- */
#define APP_ACTUATOR_GPIO_PORT        0U
#define BOARD_BUZZER_GPIO_PIN         4U                       /* Header J1 Pin 1 (GPIO4 / I2S) */
#define BOARD_LED_NORMAL_PIN          5U                       /* Header J1 Pin 7 (GPIO5 / MCLK - Green LED) */
#define BOARD_LED_WARNING_PIN         6U                       /* Header J2 Pin 6 (GPIO6 / ARD_D10 - Yellow LED) */
#define BOARD_LED_EMERGENCY_PIN       7U                       /* Header J2 Pin 12 (GPIO7 / ARD_D13 - Red LED) */

/* Set to 1 if using an active-LOW buzzer module; 0 for standard active-HIGH (default) */
#define BUZZER_ACTIVE_LOW             0

/*******************************************************************************
 * Onboard Temperature Sensor (P3T1755) Driver Settings
 ******************************************************************************/
#define TEMP_SENSOR_MAX_RETRIES        3U
#define TEMP_SENSOR_FAULT_THRESHOLD    5U
#define TEMP_SENSOR_MIN_VALID_C        (-40.0f)
#define TEMP_SENSOR_MAX_VALID_C        125.0f

/*******************************************************************************
 * Edge Safety Thresholds
 ******************************************************************************/
#define THRESHOLD_TEMP_HIGH_WARNING    40.0f                   /* Deg C (Board surface self-heating taken into account) */
#define THRESHOLD_TEMP_HIGH_EMERGENCY  50.0f                   /* Deg C */

#define THRESHOLD_GAS_WARNING_PPM      300.0f                  /* Gas Index / PPM warning */
#define THRESHOLD_GAS_EMERGENCY_PPM    600.0f                  /* Gas Index / PPM emergency */

#define THRESHOLD_FALL_ACCEL_MIN_G     0.4f                    /* Free-fall lower threshold (<0.4g) */
#define THRESHOLD_FALL_IMPACT_MAX_G    2.5f                    /* Impact shock threshold (>2.5g) */

/*******************************************************************************
 * Wireless & Dashboard Configuration
 ******************************************************************************/
#if defined(__has_include)
#  if __has_include("app_secrets.h")
#    include "app_secrets.h"
#  endif
#endif

#ifndef WIFI_SSID_DEFAULT
#define WIFI_SSID_DEFAULT              "CHANGE_ME_SSID"
#endif
#ifndef WIFI_PASSWORD_DEFAULT
#define WIFI_PASSWORD_DEFAULT          "CHANGE_ME_PASSWORD"
#endif
#ifndef DASHBOARD_SERVER_IP
#define DASHBOARD_SERVER_IP            "192.168.0.100"
#endif
#ifndef DASHBOARD_SERVER_PORT
#define DASHBOARD_SERVER_PORT          5000U
#endif

#ifndef WIFI_STACK_AVAILABLE
#define WIFI_STACK_AVAILABLE           0
#endif

#endif /* _APP_CONFIG_H_ */
