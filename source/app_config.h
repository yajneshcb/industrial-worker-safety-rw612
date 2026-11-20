/*
 * Copyright 2026
 * Project: Industrial Worker Safety Monitoring and Emergency Alert System
 * Platform: NXP FRDM-RW612
 */

#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * System Sampling & Timing Configurations (Placeholders)
 ******************************************************************************/
#define SENSOR_POLL_INTERVAL_MS        100U   /* Sensor sampling interval */
#define TELEMETRY_REPORT_INTERVAL_MS   1000U  /* Periodic reporting interval */
#define EMERGENCY_REPORT_INTERVAL_MS   200U   /* High-priority reporting interval */

/*******************************************************************************
 * Onboard Temperature Sensor (P3T1755) driver settings
 ******************************************************************************/
#define TEMP_SENSOR_MAX_RETRIES        3U      /* I2C attempts per read          */
#define TEMP_SENSOR_FAULT_THRESHOLD    5U      /* consecutive failed reads = fault */
#define TEMP_SENSOR_MIN_VALID_C        (-40.0f)/* P3T1755 rated range            */
#define TEMP_SENSOR_MAX_VALID_C        125.0f

/*******************************************************************************
 * Edge Safety Thresholds (Placeholders - to be tuned during testing)
 ******************************************************************************/
#define THRESHOLD_TEMP_HIGH_WARNING    40.0f  /* Deg C */
#define THRESHOLD_TEMP_HIGH_EMERGENCY  50.0f  /* Deg C */

#define THRESHOLD_GAS_WARNING_PPM      300.0f /* Gas concentration warning */
#define THRESHOLD_GAS_EMERGENCY_PPM    600.0f /* Gas concentration critical */

#define THRESHOLD_FALL_ACCEL_MIN_G     0.4f   /* Free-fall lower threshold */
#define THRESHOLD_FALL_IMPACT_MAX_G    2.5f   /* Impact spike threshold */

/*******************************************************************************
 * Wireless & Dashboard Configuration (Placeholders)
 ******************************************************************************/
#define WIFI_SSID_DEFAULT              "realme P1 5G eb01"
#define WIFI_PASSWORD_DEFAULT          "yajjugowda"
#define DASHBOARD_SERVER_IP            "10.106.218.220"
#define DASHBOARD_SERVER_PORT          5000U

#endif /* _APP_CONFIG_H_ */

