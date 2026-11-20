/*
 * Copyright 2026
 * Module: MQ-2 Gas and Smoke Sensor
 * Platform: NXP FRDM-RW612
 */

#ifndef _SENSOR_GAS_H_
#define _SENSOR_GAS_H_

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * Data Structures & Types
 ******************************************************************************/
typedef struct {
    uint32_t raw_adc_value;
    float    voltage_v;
    float    gas_ppm;
    bool     is_valid;
} GasSensorData_t;

/* Global simulation control for hardware testing */
extern volatile uint32_t g_gasLeakSimulation; /* 0=Normal, 1=Warning (>300 PPM), 2=Emergency (>600 PPM) */

/*******************************************************************************
 * API Declarations
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

void SENSOR_Gas_Init(void);
bool SENSOR_Gas_Read(GasSensorData_t *out_data);
float SENSOR_Gas_RawToPpm(uint32_t raw_adc);
void SENSOR_Gas_SetSimulationMode(uint32_t mode);
bool SENSOR_Gas_IsFaulted(void);
uint32_t SENSOR_Gas_GetErrorCount(void);

#if defined(__cplusplus)
}
#endif

#endif /* _SENSOR_GAS_H_ */

