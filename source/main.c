/*
 * Copyright 2026
 * Project: Industrial Worker Safety Monitoring and Emergency Alert System
 * Platform: NXP FRDM-RW612 (RW612ETA2I)
 *
 * Description: Main application entry point.
 *
 * CURRENT STAGE: Temperature (P3T1755) + Motion & Fall (MPU-6050).
 * Other subsystems are still stubs and are left commented out.
 */

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "app_config.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "system_RW612.h"

/* Modular headers */
#include "sensor_temp.h"
#include "sensor_mpu6050.h"
#include "sensor_gas.h"
#include "safety_decision.h"
#include "alert_manager.h"
#include "button_sos.h"
#include "wifi_manager.h"
#include "alert_reporter.h"

/*******************************************************************************
 * Global Live Telemetry (For VS Code Live Watch & Debugger)
 ******************************************************************************/
volatile uint32_t g_heartbeatCounter = 0U;
volatile float    g_liveTemperature  = 0.0f;
volatile float    g_liveAccelTotal   = 0.0f;
volatile float    g_liveAccelX       = 0.0f;
volatile float    g_liveAccelY       = 0.0f;
volatile float    g_liveAccelZ       = 0.0f;
volatile float    g_liveGasPpm       = 0.0f;

/* Edge Safety Status (0=NORMAL, 1=WARNING, 2=EMERGENCY) */
volatile uint32_t g_safetyState          = SAFETY_STATE_NORMAL;
/* Primary Hazard (0=NONE, 1=HIGH_TEMP, 2=GAS_LEAK, 3=FALL, 4=SOS) */
volatile uint32_t g_primaryHazard        = HAZARD_NONE;
volatile uint32_t g_fallEventCount       = 0U;
volatile bool     g_manualWarningTrigger = false;

/*******************************************************************************
 * Application Prototypes & Local State
 ******************************************************************************/
static void App_InitSystem(void);
static void App_ProcessSensors(void);
static void App_ProcessSafetyLogic(void);
static void App_ProcessAlerts(void);
static void App_ProcessCommunication(void);

static TempSensorData_t   s_currentTemp;
static Mpu6050Data_t      s_currentMpu;
static GasSensorData_t    s_currentGas;
static SafetyEvaluation_t s_lastEvaluation;

/* Print a float as "[-]dd.dd" without needing float support in PRINTF. */
static void App_PrintFloat2Dec(float val)
{
    int32_t centi = (int32_t)(val * 100.0f);
    const char *sign = "";

    if (centi < 0)
    {
        sign  = "-";
        centi = -centi;
    }
    PRINTF("%s%d.%02d", sign, (int)(centi / 100), (int)(centi % 100));
}

static void App_CycleDelay(void)
{
    SDK_DelayAtLeastUs(100000U, SystemCoreClock);
}

/*******************************************************************************
 * Main Function Skeleton
 ******************************************************************************/
int main(void)
{
    /* Initialize NXP Board, Clocks, Pins, and Debug Console */
    App_InitSystem();

    /* Main application executive loop */
    PRINTF("\r\n>> BOARD RUNNING: Sensor Telemetry Starting...\r\n");

    while (1)
    {
        /* 1. Sensor Acquisition Stage */
        App_ProcessSensors();

        /* 2. On-device Edge Decision Logic Stage */
        App_ProcessSafetyLogic();

        /* 3. Local Alerting Stage (LEDs & Buzzer) */
        App_ProcessAlerts();

        /* 4. Wireless Telemetry & Dashboard Reporting Stage */
        App_ProcessCommunication();

        /* 5. 100ms sample interval delay */
        App_CycleDelay();
    }

    return 0; 
}

/*******************************************************************************
 * Function Skeletons (No implementation code as requested)
 ******************************************************************************/

static void App_InitSystem(void)
{
    /* Ensure Cortex-M33 hardware FPU (CP10 and CP11) is powered and fully enabled */
    volatile uint32_t *pCPACR = (volatile uint32_t *)0xE000ED88U;
    *pCPACR |= ((3UL << 20) | (3UL << 22));
    __asm__ volatile ("dsb 0xF\n isb 0xF" ::: "memory");

    /* Initialize NXP Board, Clocks, Pins, and Flexcomm 3 Debug UART */
    BOARD_InitHardware();
    setvbuf(stdout, NULL, _IONBF, 0);

    PRINTF("\r\n==================================================\r\n");
    PRINTF("Worker Safety: Temp (P3T1755) + Motion (MPU) + Gas (MQ-2)\r\n");
    PRINTF("==================================================\r\n");

    SENSOR_Temp_Init();
    SENSOR_MPU6050_Init();
    SENSOR_Gas_Init();
    SAFETY_Decision_Init();
    BUTTON_SOS_Init();
    ALERT_Manager_Init();
    WIFI_Manager_Init();
    ALERT_Reporter_Init();
}

static void App_ProcessSensors(void)
{
    static uint32_t printDivider = 0U;

    (void)SENSOR_Temp_Read(&s_currentTemp);
    (void)SENSOR_MPU6050_Read(&s_currentMpu);
    (void)SENSOR_Gas_Read(&s_currentGas);

    g_heartbeatCounter++;

    if (s_currentTemp.is_valid)
    {
        g_liveTemperature = s_currentTemp.temperature_c;
    }
    if (s_currentMpu.is_valid)
    {
        g_liveAccelX     = s_currentMpu.accel_x_g;
        g_liveAccelY     = s_currentMpu.accel_y_g;
        g_liveAccelZ     = s_currentMpu.accel_z_g;
        g_liveAccelTotal = SENSOR_MPU6050_GetTotalAccelG(&s_currentMpu);
    }
    if (s_currentGas.is_valid)
    {
        g_liveGasPpm     = s_currentGas.gas_ppm;
    }

    /* Print once per TELEMETRY_REPORT_INTERVAL_MS (poll runs every SENSOR_POLL_INTERVAL_MS). */
    if (++printDivider >= (TELEMETRY_REPORT_INTERVAL_MS / SENSOR_POLL_INTERVAL_MS))
    {
        printDivider = 0U;

        /* 1. Onboard Temperature Output */
        if (s_currentTemp.is_valid)
        {
            PRINTF("[TEMP] ");
            App_PrintFloat2Dec(s_currentTemp.temperature_c);
            PRINTF(" C | ");
        }
        else
        {
            PRINTF("[TEMP] FAIL | ");
        }

        /* 2. MPU-6050 Motion / Acceleration Output */
        if (s_currentMpu.is_valid)
        {
            float total_g = SENSOR_MPU6050_GetTotalAccelG(&s_currentMpu);
            PRINTF("[MPU] Accel=(X:");
            App_PrintFloat2Dec(s_currentMpu.accel_x_g);
            PRINTF(", Y:");
            App_PrintFloat2Dec(s_currentMpu.accel_y_g);
            PRINTF(", Z:");
            App_PrintFloat2Dec(s_currentMpu.accel_z_g);
            PRINTF(") Total=");
            App_PrintFloat2Dec(total_g);
            PRINTF(" g");
        }
        else
        {
            PRINTF("[MPU] READ FAILED");
        }

        /* 3. MQ-2 Gas & Smoke Concentration Output */
        if (s_currentGas.is_valid)
        {
            PRINTF(" | [GAS] ");
            App_PrintFloat2Dec(s_currentGas.gas_ppm);
            PRINTF(" PPM\r\n");
        }
        else
        {
            PRINTF(" | [GAS] FAIL\r\n");
        }
    }
}

static void App_ProcessSafetyLogic(void)
{
    bool sosActive = BUTTON_SOS_IsPressed();
    s_lastEvaluation = SAFETY_Decision_Evaluate(&s_currentTemp, &s_currentMpu, &s_currentGas, sosActive);

    /* Manual warning trigger override for interactive demonstration */
    if (g_manualWarningTrigger && (s_lastEvaluation.state < SAFETY_STATE_WARNING))
    {
        s_lastEvaluation.state          = SAFETY_STATE_WARNING;
        s_lastEvaluation.primary_hazard = HAZARD_HIGH_TEMPERATURE;
    }

    g_safetyState   = (uint32_t)s_lastEvaluation.state;
    g_primaryHazard = (uint32_t)s_lastEvaluation.primary_hazard;

    if (s_lastEvaluation.primary_hazard == HAZARD_FALL_DETECTED)
    {
        g_fallEventCount++;
    }
}

static void App_ProcessAlerts(void)
{
    /* Update buzzer pattern and status LEDs based on safety evaluation */
    ALERT_Manager_Update(&s_lastEvaluation);
}

static void App_ProcessCommunication(void)
{
    static uint32_t telemetryTick = 0U;

    /* Assemble live telemetry packet */
    TelemetryPacket_t packet;
    packet.message_id        = g_heartbeatCounter;
    packet.timestamp_ms      = g_heartbeatCounter * 100U;
    packet.state             = s_lastEvaluation.state;
    packet.hazard            = s_lastEvaluation.primary_hazard;
    packet.temperature_c     = g_liveTemperature;
    packet.gas_ppm           = s_currentGas.gas_ppm;
    packet.accel_magnitude_g = g_liveAccelTotal;
    packet.sos_active        = (s_lastEvaluation.primary_hazard == HAZARD_MANUAL_SOS);

    /* 1. High-Priority Dispatch: Emergency alerts transmit immediately */
    if (s_lastEvaluation.state == SAFETY_STATE_EMERGENCY)
    {
        ALERT_Reporter_SendEmergencyAlert(&packet);
    }
    /* 2. Normal / Warning Periodic Telemetry Dispatch (Every 1000ms) */
    else if (++telemetryTick >= (TELEMETRY_REPORT_INTERVAL_MS / SENSOR_POLL_INTERVAL_MS))
    {
        telemetryTick = 0U;
        ALERT_Reporter_SendTelemetry(&packet);
    }

    /* 3. Handle background connection keep-alive & recover offline buffered packets */
    WIFI_Manager_Process();
    ALERT_Reporter_ProcessQueue();
}

