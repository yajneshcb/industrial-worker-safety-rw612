/*
 * Copyright 2026
 * Module: Wireless Connection Manager (Wi-Fi / BLE)
 * Platform: NXP FRDM-RW612
 */

#include "wifi_manager.h"
#include "app_config.h"
#include <string.h>

/*******************************************************************************
 * Global Wireless Live State
 ******************************************************************************/
volatile bool     g_wifiConnected          = (WIFI_STACK_AVAILABLE != 0); /* false until a real stack exists */
volatile uint32_t g_wifiStatus             = (WIFI_STACK_AVAILABLE ? WIRELESS_STATUS_CONNECTED : WIRELESS_STATUS_DISCONNECTED);
volatile uint32_t g_wifiPacketsSent        = 0U;
volatile uint32_t g_wifiReconnectAttempts  = 0U;
char              g_activeSsid[33]         = WIFI_SSID_DEFAULT;
static char       s_activePassword[65]     = WIFI_PASSWORD_DEFAULT;

/*******************************************************************************
 * Wi-Fi Manager Implementations
 ******************************************************************************/

void WIFI_Manager_Init(void)
{
    g_wifiConnected         = (WIFI_STACK_AVAILABLE != 0);
    g_wifiStatus            = WIFI_STACK_AVAILABLE ? WIRELESS_STATUS_CONNECTED : WIRELESS_STATUS_DISCONNECTED;
    g_wifiPacketsSent       = 0U;
    g_wifiReconnectAttempts = 0U;
    strncpy(g_activeSsid, WIFI_SSID_DEFAULT, sizeof(g_activeSsid) - 1);
    g_activeSsid[sizeof(g_activeSsid) - 1] = '\0';
}

void WIFI_Manager_SetCredentials(const char *ssid, const char *password)
{
    if ((ssid != NULL) && (ssid[0] != '\0'))
    {
        strncpy(g_activeSsid, ssid, sizeof(g_activeSsid) - 1);
        g_activeSsid[sizeof(g_activeSsid) - 1] = '\0';
    }
    if (password != NULL)
    {
        strncpy(s_activePassword, password, sizeof(s_activePassword) - 1);
        s_activePassword[sizeof(s_activePassword) - 1] = '\0';
    }

    /* Initiate reconnect sequence to the new Wi-Fi network (no-op until a real stack exists) */
    g_wifiConnected         = false;
    g_wifiStatus            = WIFI_STACK_AVAILABLE ? WIRELESS_STATUS_CONNECTING : WIRELESS_STATUS_DISCONNECTED;
    g_wifiReconnectAttempts = 0U;
}

WirelessStatus_t WIFI_Manager_GetStatus(void)
{
    return (WirelessStatus_t)g_wifiStatus;
}

bool WIFI_Manager_IsConnected(void)
{
    return g_wifiConnected;
}

void WIFI_Manager_Process(void)
{
    /* Maintain wireless connection state */
    if (!g_wifiConnected)
    {
        g_wifiStatus = WIRELESS_STATUS_DISCONNECTED;
    }
    else
    {
        g_wifiStatus = WIRELESS_STATUS_CONNECTED;
    }
}


