/*
 * Copyright 2026
 * Module: Alert Reporter & Telemetry Dispatcher
 * Platform: NXP FRDM-RW612
 */

#include "alert_reporter.h"
#include "wifi_manager.h"
#include "app_config.h"
#include "fsl_debug_console.h"

/*******************************************************************************
 * Global Metrics Live State
 ******************************************************************************/
volatile uint32_t g_telemetryPacketsSent = 0U;
volatile uint32_t g_emergencyAlertsSent  = 0U;
volatile uint32_t g_offlineQueueCount    = 0U;
volatile uint32_t g_lastPacketId         = 0U;

/*******************************************************************************
 * Offline Fault-Tolerant Ring Buffer
 ******************************************************************************/
#define OFFLINE_QUEUE_CAPACITY 16U

static TelemetryPacket_t s_offlineQueue[OFFLINE_QUEUE_CAPACITY];
static uint32_t          s_queueHead = 0U;
static uint32_t          s_queueTail = 0U;

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

    PRINTF("[TELEMETRY] {\"id\":%u,\"uptime_ms\":%u,"
           "\"temp\":%s%d.%02d,\"accel\":%s%d.%02d,"
           "\"accel_x\":%s%d.%02d,\"accel_y\":%s%d.%02d,\"accel_z\":%s%d.%02d,"
           "\"gas\":%s%d.%02d,\"state\":\"%s\",\"hazard\":\"%s\","
           "\"sos\":%s,\"sensors_ok\":%s,\"offline_queue_count\":%u}\r\n",
           (unsigned int)pkt->message_id,
           (unsigned int)pkt->timestamp_ms,
           t.sign,  t.whole,  t.frac,
           a.sign,  a.whole,  a.frac,
           ax.sign, ax.whole, ax.frac,
           ay.sign, ay.whole, ay.frac,
           az.sign, az.whole, az.frac,
           g.sign,  g.whole,  g.frac,
           GetStateString(pkt->state),
           GetHazardString(pkt->hazard),
           pkt->sos_active ? "true" : "false",
           pkt->sensors_ok ? "true" : "false",
           (unsigned int)g_offlineQueueCount);
}

static void EnqueueOffline(const TelemetryPacket_t *packet)
{
#if !WIFI_STACK_AVAILABLE
    /* No Wi-Fi stack: the console line already printed is the real delivery path, so buffering would
     * only fill a queue that can never be flushed and report a misleading offline_queue_count. */
    (void)packet;
    return;
#else
    /* When full, drop the OLDEST entry so the newest (most relevant / emergency) data is kept. */
    if (g_offlineQueueCount >= OFFLINE_QUEUE_CAPACITY)
    {
        s_queueTail = (s_queueTail + 1U) % OFFLINE_QUEUE_CAPACITY;
        g_offlineQueueCount--;
    }
    s_offlineQueue[s_queueHead] = *packet;
    s_queueHead = (s_queueHead + 1U) % OFFLINE_QUEUE_CAPACITY;
    g_offlineQueueCount++;
#endif
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

    s_queueHead = 0U;
    s_queueTail = 0U;
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
        g_wifiPacketsSent++;
        return true;
    }

    /* 2. Wi-Fi link dropped: Store in offline fault-tolerant queue */
    EnqueueOffline(packet);

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
        g_wifiPacketsSent++;
        return true;
    }

    /* Emergency packets take priority into the offline buffer */
    EnqueueOffline(packet);

    return false;
}

void ALERT_Reporter_ProcessQueue(void)
{
    /* If connection restored, flush buffered packets */
    if (WIFI_Manager_IsConnected() && (g_offlineQueueCount > 0U))
    {
        const TelemetryPacket_t *queuedPacket = &s_offlineQueue[s_queueTail];
        DispatchPacketToTransport(queuedPacket);

        s_queueTail = (s_queueTail + 1U) % OFFLINE_QUEUE_CAPACITY;
        g_offlineQueueCount--;
        g_telemetryPacketsSent++;
        g_wifiPacketsSent++;
    }
}


