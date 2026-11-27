/*
 * Copyright 2026
 * Module: Alert Reporter & Telemetry Dispatcher
 * Platform: NXP FRDM-RW612
 */

#include "alert_reporter.h"
#include "wifi_manager.h"
#include "app_config.h"
#include "fsl_debug_console.h"
#include <stdio.h>

/*******************************************************************************
 * Global Metrics Live State
 ******************************************************************************/
volatile uint32_t g_telemetryPacketsSent = 0U;
volatile uint32_t g_emergencyAlertsSent  = 0U;
volatile uint32_t g_offlineQueueCount    = 0U;
volatile uint32_t g_lastPacketId         = 0U;

static const char *GetStateString(SafetyState_t state)
{
    switch (state)
    {
        case SAFETY_STATE_WARNING:   return "WARNING";
        case SAFETY_STATE_EMERGENCY: return "EMERGENCY";
        case SAFETY_STATE_NORMAL:
        default:                     return "NORMAL";
    }
}

static const char *GetHazardString(HazardType_t hazard)
{
    switch (hazard)
    {
        case HAZARD_HIGH_TEMPERATURE: return "HIGH_TEMP";
        case HAZARD_GAS_LEAK:         return "GAS_LEAK";
        case HAZARD_FALL_DETECTED:    return "FALL_DETECTED";
        case HAZARD_MANUAL_SOS:       return "MANUAL_SOS";
        case HAZARD_SENSOR_FAULT:     return "SENSOR_FAULT";
        case HAZARD_NONE:
        default:                      return "NONE";
    }
}

/* Split a float into sign / integer / 2-decimal fraction so negatives print as valid JSON
 * (the old (x100 / 100, x100 % 100) form produced e.g. "-5.-25" for negative values). */
typedef struct { const char *sign; int whole; int frac; } Fixed2_t;

static Fixed2_t ToFixed2(float val)
{
    Fixed2_t out;
    int32_t centi = (int32_t)(val * 100.0f);
    out.sign = "";
    if (centi < 0)
    {
        out.sign = "-";
        centi    = -centi;
    }
    out.whole = (int)(centi / 100);
    out.frac  = (int)(centi % 100);
    return out;
}

static void DispatchPacketToTransport(const TelemetryPacket_t *pkt)
{
    /*
     * Single-line JSON, prefixed with [TELEMETRY] so serial_bridge.py can find it among other logs.
     * 'uptime_ms' is deliberately NOT called 'timestamp_ms': the dashboard treats timestamp_ms as
     * epoch time and would show 1970 dates for a device uptime counter.
     */
    Fixed2_t t  = ToFixed2(pkt->temperature_c);
    Fixed2_t a  = ToFixed2(pkt->accel_magnitude_g);
    Fixed2_t ax = ToFixed2(pkt->accel_x_g);
    Fixed2_t ay = ToFixed2(pkt->accel_y_g);
    Fixed2_t az = ToFixed2(pkt->accel_z_g);
    Fixed2_t g  = ToFixed2(pkt->gas_ppm);
    Fixed2_t gv = ToFixed2(pkt->gas_adc_pin_voltage_v);
    char payload[560];
    int payloadLength = snprintf(payload, sizeof(payload),
           "{\"id\":%u,\"uptime_ms\":%u,"
           "\"temp\":%s%d.%02d,\"accel\":%s%d.%02d,"
           "\"accel_x\":%s%d.%02d,\"accel_y\":%s%d.%02d,\"accel_z\":%s%d.%02d,"
           "\"gas\":%s%d.%02d,\"gas_raw\":%u,\"gas_adc_v\":%s%d.%02d,"
           "\"state\":\"%s\",\"hazard\":\"%s\","
           "\"sos\":%s,\"sensors_ok\":%s,"
           "\"temp_ok\":%s,\"mpu_ok\":%s,\"gas_ok\":%s,"
           "\"offline_queue_count\":%u}",
           (unsigned int)pkt->message_id,
           (unsigned int)pkt->timestamp_ms,
           t.sign,  t.whole,  t.frac,
           a.sign,  a.whole,  a.frac,
           ax.sign, ax.whole, ax.frac,
           ay.sign, ay.whole, ay.frac,
           az.sign, az.whole, az.frac,
           g.sign,  g.whole,  g.frac,
           (unsigned int)pkt->gas_raw_adc,
           gv.sign, gv.whole, gv.frac,
           GetStateString(pkt->state),
           GetHazardString(pkt->hazard),
           pkt->sos_active ? "true" : "false",
           pkt->sensors_ok ? "true" : "false",
           pkt->temp_ok ? "true" : "false",
           pkt->mpu_ok ? "true" : "false",
           pkt->gas_ok ? "true" : "false",
           (unsigned int)g_offlineQueueCount);

    if ((payloadLength > 0) && ((size_t)payloadLength < sizeof(payload)))
    {
        PRINTF("[TELEMETRY] %s\r\n", payload);
        (void)WIFI_Manager_QueuePayload(payload);
    }
}

/*******************************************************************************
 * Alert Reporter Implementations
 ******************************************************************************/

void ALERT_Reporter_Init(void)
{
    g_telemetryPacketsSent = 0U;
    g_emergencyAlertsSent  = 0U;
    g_offlineQueueCount    = 0U;
    g_lastPacketId         = 0U;

    g_offlineQueueCount = 0U;
}

bool ALERT_Reporter_SendTelemetry(const TelemetryPacket_t *packet)
{
    if (packet == NULL)
    {
        return false;
    }

    g_lastPacketId = packet->message_id;

    /* Always dispatch structured JSON to USB debug console (MCU-Link VCOM) */
    DispatchPacketToTransport(packet);

    /* 1. Transmit immediately if Wi-Fi link is active */
    if (WIFI_Manager_IsConnected())
    {
        g_telemetryPacketsSent++;
        return true;
    }

    /* Do not replay stale movement data after a Wi-Fi outage. The next fresh sample is sent. */
    return false;
}

bool ALERT_Reporter_SendEmergencyAlert(const TelemetryPacket_t *packet)
{
    if (packet == NULL)
    {
        return false;
    }

    g_lastPacketId = packet->message_id;
    g_emergencyAlertsSent++;

    /* Always dispatch immediate emergency JSON to USB debug console (MCU-Link VCOM) */
    DispatchPacketToTransport(packet);

    if (WIFI_Manager_IsConnected())
    {
        g_telemetryPacketsSent++;
        return true;
    }

    return false;
}

void ALERT_Reporter_ProcessQueue(void)
{
    /* Fresh packets are handed directly to WIFI_Manager's one-sample queue. */
}


