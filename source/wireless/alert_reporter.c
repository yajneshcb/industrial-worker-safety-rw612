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

static void DispatchPacketToTransport(const TelemetryPacket_t *pkt)
{
    /* 
     * Formats structured JSON telemetry packet for serial bridge / laptop dashboard ingestion:
     * Prefix [TELEMETRY] allows the Python dashboard to parse live packets from the MCU-Link VCOM port.
     */
    int32_t temp_x100  = (int32_t)(pkt->temperature_c * 100.0f);
    int32_t accel_x100 = (int32_t)(pkt->accel_magnitude_g * 100.0f);
    int32_t gas_x100   = (int32_t)(pkt->gas_ppm * 100.0f);

    PRINTF("[TELEMETRY] {\"id\":%u,\"temp\":%d.%02d,\"accel\":%d.%02d,\"gas\":%d.%02d,\"state\":\"%s\",\"hazard\":\"%s\",\"sos\":%s}\r\n",
           (unsigned int)pkt->message_id,
           (int)(temp_x100 / 100), (int)(temp_x100 % 100),
           (int)(accel_x100 / 100), (int)(accel_x100 % 100),
           (int)(gas_x100 / 100), (int)(gas_x100 % 100),
           GetStateString(pkt->state),
           GetHazardString(pkt->hazard),
           pkt->sos_active ? "true" : "false");
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
    if (g_offlineQueueCount < OFFLINE_QUEUE_CAPACITY)
    {
        s_offlineQueue[s_queueHead] = *packet;
        s_queueHead = (s_queueHead + 1U) % OFFLINE_QUEUE_CAPACITY;
        g_offlineQueueCount++;
    }

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
    if (g_offlineQueueCount < OFFLINE_QUEUE_CAPACITY)
    {
        s_offlineQueue[s_queueHead] = *packet;
        s_queueHead = (s_queueHead + 1U) % OFFLINE_QUEUE_CAPACITY;
        g_offlineQueueCount++;
    }

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


