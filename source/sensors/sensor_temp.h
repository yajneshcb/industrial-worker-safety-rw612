/*
 * Copyright 2026
 * Module: Onboard Digital Temperature Sensor (P3T1755)
 * Platform: NXP FRDM-RW612
 *
 * The P3T1755 is read over Flexcomm 2 in I2C mode (GPIO16 = SDA, GPIO17 = SCL,
 * 7-bit address 0x48). The sensor runs continuously after power-up, so the
 * driver only needs to read the 16-bit temperature register.
 */

#ifndef _SENSOR_TEMP_H_
#define _SENSOR_TEMP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*******************************************************************************
 * Data Structures & Types
 ******************************************************************************/
typedef struct {
    float   temperature_c;   /* Valid only when is_valid == true               */
    int16_t raw;             /* Signed 12-bit register value (1 LSB = 0.0625C) */
    bool    is_valid;        /* false on bus error or out-of-range reading     */
} TempSensorData_t;

/*******************************************************************************
 * API Declarations
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

/* Attach the Flexcomm 2 clock and initialise it as an I2C master. */
void SENSOR_Temp_Init(void);

/* Read the temperature. Returns true and fills out_data on success.
 * On failure out_data->is_valid is false and false is returned. */
bool SENSOR_Temp_Read(TempSensorData_t *out_data);

/* true once TEMP_SENSOR_FAULT_THRESHOLD consecutive reads have failed.
 * Intended for the edge-logic "sensor status" check. */
bool SENSOR_Temp_IsFaulted(void);

/* Total number of failed read attempts since init (diagnostics). */
uint32_t SENSOR_Temp_GetErrorCount(void);

#if defined(__cplusplus)
}
#endif

#endif /* _SENSOR_TEMP_H_ */
