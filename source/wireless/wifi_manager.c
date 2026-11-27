/* Copyright 2026. RW612 station connection and asynchronous HTTPS queue. */
#include "wifi_manager.h"
#include "app_config.h"
#include "dashboard_transport.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "wpl.h"
#include "lwip/apps/sntp.h"
#include "lwip/tcpip.h"

#include <string.h>

#define WIFI_TASK_STACK_WORDS  4096U
#define WIFI_TASK_PRIORITY     (tskIDLE_PRIORITY + 2U)
#define WIFI_RETRY_MS          5000U
#define WIFI_PAYLOAD_CAPACITY  512U
#define WIFI_NETWORK_LABEL     "worker_safety"

volatile bool     g_wifiConnected         = false;
volatile uint32_t g_wifiStatus            = WIRELESS_STATUS_DISCONNECTED;
volatile uint32_t g_wifiPacketsSent       = 0U;
volatile uint32_t g_wifiReconnectAttempts = 0U;
char              g_activeSsid[33]        = WIFI_SSID_DEFAULT;
static char       s_activePassword[65]    = WIFI_PASSWORD_DEFAULT;
static QueueHandle_t s_payloadQueue       = NULL;

static void WIFI_LinkChanged(bool connected)
{
    g_wifiConnected = connected;
    g_wifiStatus = connected ? WIRELESS_STATUS_CONNECTED : WIRELESS_STATUS_DISCONNECTED;
    PRINTF(">> WIFI: link %s\r\n", connected ? "up" : "down");
}

static const char *WIFI_ErrStr(wpl_ret_t status)
{
    switch (status)
    {
        case WPLRET_SUCCESS:      return "SUCCESS";
        case WPLRET_FAIL:         return "FAIL";
        case WPLRET_NOT_FOUND:    return "NOT_FOUND";
        case WPLRET_AUTH_FAILED:  return "AUTH_FAILED";
        case WPLRET_ADDR_FAILED:  return "ADDR_FAILED";
        case WPLRET_NOT_CONNECTED:return "NOT_CONNECTED";
        case WPLRET_NOT_READY:    return "NOT_READY";
        case WPLRET_TIMEOUT:      return "TIMEOUT";
        case WPLRET_BAD_PARAM:    return "BAD_PARAM";
        default:                  return "UNKNOWN";
    }
}

static bool WIFI_StartDriver(void)
{
    static bool s_wplInitialized = false;
    wpl_ret_t status;

    g_wifiStatus = WIRELESS_STATUS_CONNECTING;
    if (!s_wplInitialized)
    {
        status = WPL_Init();
        if (status != WPLRET_SUCCESS)
        {
            PRINTF(">> WIFI: init failed (%d:%s)\r\n", (int)status, WIFI_ErrStr(status));
            return false;
        }
        s_wplInitialized = true;
    }
    status = WPL_Start(WIFI_LinkChanged);
    if (status != WPLRET_SUCCESS)
    {
        PRINTF(">> WIFI: start failed (%d:%s)\r\n", (int)status, WIFI_ErrStr(status));
        return false;
    }
    status = WPL_AddNetworkWithSecurity(g_activeSsid, s_activePassword, WIFI_NETWORK_LABEL, WPL_SECURITY_WILDCARD);
    if (status != WPLRET_SUCCESS)
    {
        PRINTF(">> WIFI: profile setup failed (%d:%s)\r\n", (int)status, WIFI_ErrStr(status));
        return false;
    }
    return true;
}

static bool WIFI_JoinNetwork(void)
{
    static bool s_useWpa3 = false;
    static bool s_scannedOnce = false;
    char ip[16] = {0};
    wpl_ret_t status;

    status = WPL_Join(WIFI_NETWORK_LABEL);
    if (status != WPLRET_SUCCESS)
    {
        PRINTF(">> WIFI: join '%s' failed (%d:%s, mode=%s); retrying\r\n",
               g_activeSsid, (int)status, WIFI_ErrStr(status),
               s_useWpa3 ? "WPA3-SAE" : "WILDCARD");
        if (!s_scannedOnce)
        {
            s_scannedOnce = true;
            PRINTF(">> WIFI: scanning nearby APs...\r\n");
            (void)WPL_Scan();
        }
        if ((status == WPLRET_AUTH_FAILED) || (status == WPLRET_FAIL) || (status == WPLRET_TIMEOUT))
        {
            s_useWpa3 = !s_useWpa3;
            (void)WPL_RemoveNetwork(WIFI_NETWORK_LABEL);
            (void)WPL_AddNetworkWithSecurity(g_activeSsid, s_activePassword, WIFI_NETWORK_LABEL,
                                                s_useWpa3 ? WPL_SECURITY_WPA3_SAE : WPL_SECURITY_WILDCARD);
        }
        g_wifiConnected = false;
        g_wifiStatus = WIRELESS_STATUS_DISCONNECTED;
        return false;
    }
    status = WPL_GetIP(ip, 1);
    if (status == WPLRET_SUCCESS)
    {
        PRINTF(">> WIFI: connected, IP %s\r\n", ip);
    }
    g_wifiConnected = true;
    g_wifiStatus = WIRELESS_STATUS_CONNECTED;
    return true;
}

static void WIFI_Task(void *argument)
{
    char payload[WIFI_PAYLOAD_CAPACITY];
    bool driverStarted = false;
    bool sntpStarted = false;
    uint32_t sntpWaitSec = 0U;

    (void)argument;
    for (;;)
    {
        if (!driverStarted)
        {
            if (!WIFI_StartDriver())
            {
                g_wifiReconnectAttempts++;
                g_wifiStatus = WIRELESS_STATUS_ERROR;
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
                continue;
            }
            driverStarted = true;
        }
        if (!g_wifiConnected)
        {
            g_wifiReconnectAttempts++;
            if (!WIFI_JoinNetwork())
            {
                g_wifiStatus = WIRELESS_STATUS_ERROR;
                vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
                continue;
            }
        }

        if (!sntpStarted)
        {
            LOCK_TCPIP_CORE();
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0U, SNTP_SERVER_ADDRESS);
            sntp_init();
            UNLOCK_TCPIP_CORE();
            sntpStarted = true;
            PRINTF(">> WIFI: syncing UTC time for TLS certificate validation\r\n");
        }
        if (!DASHBOARD_IsTimeReady())
        {
            if (++sntpWaitSec >= 10U)
            {
                /* Fallback UTC epoch (2026-11-27) if hotspot/carrier blocks UDP 123 NTP */
                DASHBOARD_TimeSyncSet(1795784400U);
                PRINTF(">> WIFI: SNTP timeout, using fallback UTC time for TLS\r\n");
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(1000U));
                continue;
            }
        }

        if (xQueueReceive(s_payloadQueue, payload, pdMS_TO_TICKS(1000U)) == pdPASS)
        {
            int http_status = DASHBOARD_PostJson(DASHBOARD_SERVER_HOST, DASHBOARD_SERVER_PATH,
                                                  DASHBOARD_SERVER_PORT, TELEMETRY_SECRET_VALUE, payload);
            if ((http_status >= 200) && (http_status < 300))
            {
                g_wifiPacketsSent++;
            }
            else
            {
                PRINTF(">> WIFI: HTTPS telemetry post failed (%d)\r\n", http_status);
            }
        }
    }
}

void WIFI_Manager_Init(void)
{
    g_wifiConnected = false;
    g_wifiStatus = WIRELESS_STATUS_DISCONNECTED;
    g_wifiPacketsSent = 0U;
    g_wifiReconnectAttempts = 0U;
    s_payloadQueue = xQueueCreate(1U, WIFI_PAYLOAD_CAPACITY);
    if (s_payloadQueue == NULL)
    {
        g_wifiStatus = WIRELESS_STATUS_ERROR;
        PRINTF(">> WIFI: telemetry queue allocation failed\r\n");
        return;
    }
    if (xTaskCreate(WIFI_Task, "wifi", WIFI_TASK_STACK_WORDS, NULL, WIFI_TASK_PRIORITY, NULL) != pdPASS)
    {
        g_wifiStatus = WIRELESS_STATUS_ERROR;
        PRINTF(">> WIFI: task creation failed\r\n");
    }
}

void WIFI_Manager_SetCredentials(const char *ssid, const char *password)
{
    if ((ssid != NULL) && (ssid[0] != '\0'))
    {
        strncpy(g_activeSsid, ssid, sizeof(g_activeSsid) - 1U);
        g_activeSsid[sizeof(g_activeSsid) - 1U] = '\0';
    }
    if (password != NULL)
    {
        strncpy(s_activePassword, password, sizeof(s_activePassword) - 1U);
        s_activePassword[sizeof(s_activePassword) - 1U] = '\0';
    }
    g_wifiConnected = false;
    g_wifiStatus = WIRELESS_STATUS_CONNECTING;
}

WirelessStatus_t WIFI_Manager_GetStatus(void)
{
    return (WirelessStatus_t)g_wifiStatus;
}

bool WIFI_Manager_IsConnected(void)
{
    return g_wifiConnected;
}

bool WIFI_Manager_QueuePayload(const char *payload)
{
    if ((payload == NULL) || (s_payloadQueue == NULL) || !g_wifiConnected)
    {
        return false;
    }
    return xQueueOverwrite(s_payloadQueue, payload) == pdPASS;
}

void WIFI_Manager_Process(void)
{
    /* Wi-Fi work runs in WIFI_Task so radio, DNS and TLS cannot stall sensor sampling. */
}
