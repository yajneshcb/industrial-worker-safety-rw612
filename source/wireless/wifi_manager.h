/*
 * Copyright 2026
 * Module: Wireless Connection Manager (Wi-Fi / BLE)
 * Platform: NXP FRDM-RW612
 */

#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    WIRELESS_STATUS_DISCONNECTED = 0,
    WIRELESS_STATUS_CONNECTING,
    WIRELESS_STATUS_CONNECTED,
    WIRELESS_STATUS_ERROR
} WirelessStatus_t;

/* Global live wireless status for VS Code Watch */
extern volatile bool     g_wifiConnected;
extern volatile uint32_t g_wifiStatus;
extern volatile uint32_t g_wifiPacketsSent;
extern volatile uint32_t g_wifiReconnectAttempts;
extern char              g_activeSsid[33];

#if defined(__cplusplus)
extern "C" {
#endif

void WIFI_Manager_Init(void);
WirelessStatus_t WIFI_Manager_GetStatus(void);
bool WIFI_Manager_IsConnected(void);
void WIFI_Manager_Process(void);
void WIFI_Manager_SetCredentials(const char *ssid, const char *password);
bool WIFI_Manager_QueuePayload(const char *payload);

#if defined(__cplusplus)
}
#endif

#endif /* _WIFI_MANAGER_H_ */

