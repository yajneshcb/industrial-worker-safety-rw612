/*
 * Copyright 2026
 * Module: MPU-6050 Motion & Fall Detection Sensor
 * Platform: NXP FRDM-RW612
 */

#ifndef _SENSOR_MPU6050_H_
#define _SENSOR_MPU6050_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*******************************************************************************
 * Data Structures & Types
 ******************************************************************************/
typedef struct {
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
    bool is_valid;
} Mpu6050Data_t;

/*******************************************************************************
 * API Declarations
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

void     SENSOR_MPU6050_Init(void);
bool     SENSOR_MPU6050_Read(Mpu6050Data_t *out_data);
float    SENSOR_MPU6050_GetTotalAccelG(const Mpu6050Data_t *data);
bool     SENSOR_MPU6050_IsFaulted(void);
uint32_t SENSOR_MPU6050_GetErrorCount(void);

#if defined(__cplusplus)
}
#endif

#endif /* _SENSOR_MPU6050_H_ */

