/*
 * Copyright 2026
 * Module: Alert Reporter & Telemetry Dispatcher
 * Platform: NXP FRDM-RW612
 */

#ifndef _ALERT_REPORTER_H_
#define _ALERT_REPORTER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "safety_decision.h"
#include "sensor_temp.h"
#include "sensor_mpu6050.h"
#include "sensor_gas.h"

/* Global live reporter metrics for VS Code Watch */
extern volatile uint32_t g_telemetryPacketsSent;
extern volatile uint32_t g_emergencyAlertsSent;
extern volatile uint32_t g_offlineQueueCount;
extern volatile uint32_t g_lastPacketId;

typedef struct {
    uint32_t message_id;
    uint32_t timestamp_ms;
    SafetyState_t state;
    HazardType_t hazard;
    float temperature_c;
    float gas_ppm;
    float accel_magnitude_g;
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    bool sos_active;
    bool sensors_ok;      /* false if any sensor is currently faulted / invalid */
} TelemetryPacket_t;

#if defined(__cplusplus)
extern "C" {
#endif

void ALERT_Reporter_Init(void);
bool ALERT_Reporter_SendTelemetry(const TelemetryPacket_t *packet);
bool ALERT_Reporter_SendEmergencyAlert(const TelemetryPacket_t *packet);
void ALERT_Reporter_ProcessQueue(void);

#if defined(__cplusplus)
}
#endif

#endif /* _ALERT_REPORTER_H_ */

