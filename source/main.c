/*
 * Copyright 2026
 * Project: Industrial Worker Safety Monitoring and Emergency Alert System
 * Platform: NXP FRDM-RW612 (RW612ETA2I, Cortex-M33)
 *
 * Description: Main application entry point and executive real-time scheduler.
 * High-speed 50 Hz (20 ms) IMU acquisition captures rapid fall events reliably.
 * Main sensor acquisition, edge safety logic, local alerting, and telemetry run at 10 Hz (100 ms).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "app_config.h"
#include "fsl_common.h"
#include "fsl_i2c.h"
#include "fsl_debug_console.h"
#include "system_RW612.h"

/* Modular subsystem headers */
#include "sensor_temp.h"
#include "sensor_mpu6050.h"
#include "sensor_gas.h"
#include "safety_decision.h"
#include "alert_manager.h"
#include "button_sos.h"
#include "wifi_manager.h"
#include "alert_reporter.h"
#include "timebase.h"
#include "fault_handler.h"

/*******************************************************************************
 * Global Live Telemetry (For Debugger & Live Watch)
 ******************************************************************************/
volatile uint32_t g_heartbeatCounter = 0U;
volatile float    g_liveTemperature  = 0.0f;
volatile float    g_liveAccelTotal   = 0.0f;
volatile float    g_liveAccelX       = 0.0f;
volatile float    g_liveAccelY       = 0.0f;
volatile float    g_liveAccelZ       = 0.0f;
volatile float    g_liveGasPpm       = 0.0f;

/* Edge Safety Status (0=NORMAL, 1=WARNING, 2=EMERGENCY) */
volatile uint32_t g_safetyState      = SAFETY_STATE_NORMAL;
/* Primary Hazard (0=NONE, 1=HIGH_TEMP, 2=GAS_LEAK, 3=FALL, 4=SOS) */
volatile uint32_t g_primaryHazard    = HAZARD_NONE;
volatile uint32_t g_fallEventCount   = 0U;

/*******************************************************************************
 * Application Prototypes & Local State
 ******************************************************************************/
static void App_InitSystem(void);
static void App_BootDiagnostics(void);
static void App_ProcessMotionAndFall(void);
static void App_ProcessSensors(void);
static void App_ProcessSafetyLogic(void);
static void App_ProcessAlerts(void);
static void App_ProcessCommunication(void);

static TempSensorData_t   s_currentTemp;
static Mpu6050Data_t      s_currentMpu;
static GasSensorData_t    s_currentGas;
static SafetyEvaluation_t s_lastEvaluation;

/* Print a float as "[-]dd.dd" without requiring float printf support */
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

/*******************************************************************************
 * High-Precision Fixed-Period Real-Time Scheduler
 * Runs at IMU_POLL_INTERVAL_MS (20 ms / 50 Hz).
 ******************************************************************************/
static uint32_t s_nextDeadlineMs = 0U;

static void App_CycleDelay(void)
{
    if (!Timebase_IsRunning())
    {
        SDK_DelayAtLeastUs(IMU_POLL_INTERVAL_MS * 1000U, SystemCoreClock);
        return;
    }

    s_nextDeadlineMs += IMU_POLL_INTERVAL_MS;

    if ((int32_t)(Timebase_GetMs() - s_nextDeadlineMs) > 0)
    {
        /* Slot overrun: resynchronize */
        s_nextDeadlineMs = Timebase_GetMs();
        return;
    }

    while ((int32_t)(Timebase_GetMs() - s_nextDeadlineMs) < 0)
    {
    }
}

/*******************************************************************************
 * Boot Diagnostics Output
 ******************************************************************************/
static void App_BootDiagnostics(void)
{
    static const uint8_t probeAddrs[] = { BOARD_TEMP_I2C_ADDR, BOARD_MPU6050_I2C_ADDR };
    static const char *probeNames[]   = { "P3T1755 (Onboard Temp)", "MPU-6050 (6-Axis IMU)" };

    uint32_t fc2Clk = CLOCK_GetFlexCommClkFreq(2U);
    PRINTF(">> BOOT: SystemCoreClock=%u Hz, FC2_I2C_Clk=%u Hz, hardfaults=%u, timebase=%s\r\n",
           (unsigned int)SystemCoreClock, (unsigned int)fc2Clk, (unsigned int)g_hardfaultCount,
           Timebase_IsRunning() ? "DWT running (exact 100ms/20ms timing)" : "NOT RUNNING (fallback timing)");

    /* Probe I2C Bus Devices */
    for (uint32_t i = 0U; i < (sizeof(probeAddrs) / sizeof(probeAddrs[0])); i++)
    {
        uint8_t dummy = 0U;
        i2c_master_transfer_t xfer;
        xfer.flags          = kI2C_TransferDefaultFlag;
        xfer.slaveAddress   = probeAddrs[i];
        xfer.direction      = kI2C_Read;
        xfer.subaddress     = 0U;
        xfer.subaddressSize = 0U;
        xfer.data           = &dummy;
        xfer.dataSize       = 1U;
        status_t st = I2C_MasterTransferBlocking(BOARD_TEMP_I2C_BASE, &xfer);
        PRINTF(">> BOOT: I2C 0x%02X [%s]: %s (status=%d)\r\n",
               (unsigned int)probeAddrs[i], probeNames[i],
               (st == kStatus_Success) ? "ACK - ONLINE" : "NO RESPONSE - verify wiring/power/pull-ups",
               (int)st);
    }

    PRINTF(">> BOOT: Sensor Fault Status -> TEMP:%d MPU:%d GAS:%d\r\n",
           (int)SENSOR_Temp_IsFaulted(), (int)SENSOR_MPU6050_IsFaulted(), (int)SENSOR_Gas_IsFaulted());
}

/*******************************************************************************
 * Main Application Function
 ******************************************************************************/
int main(void)
{
    /* Initialize NXP Board, Clocks, Pins, and Debug Console */
    App_InitSystem();

    PRINTF("\r\n>> BOARD RUNNING: Multi-Rate Sensor & Safety Scheduler Starting...\r\n");
    PRINTF(">> Fast IMU Sampling: 50 Hz (20 ms) | Main Sensor/Logic: 10 Hz (100 ms)\r\n");

    uint32_t subtickPhase = 0U;

    while (1)
    {
        /* 1. Fast 50 Hz Stage: Acquire MPU-6050 IMU & Process High-Speed Fall Detection every 20 ms */
        App_ProcessMotionAndFall();

        /* 2. Time-Division Multiplexed 10 Hz Tasks: Spread across 5 x 20 ms sub-ticks to prevent loop overruns */
        switch (subtickPhase)
        {
            case 0U:
                /* Phase 0 (0 ms): Ambient temperature (I2C) & MQ-2 gas (ADC) acquisition */
                App_ProcessSensors();
                break;

            case 1U:
                /* Phase 1 (20 ms): Edge multi-sensor classification logic */
                App_ProcessSafetyLogic();
                break;

            case 2U:
                /* Phase 2 (40 ms): Actuator physical update (LEDs & Buzzer) */
                App_ProcessAlerts();
                break;

            case 3U:
                /* Phase 3 (60 ms): Telemetry serialization & UART transmission */
                App_ProcessCommunication();
                break;

            case 4U:
            default:
                /* Phase 4 (80 ms): Background queue & WiFi processing */
                WIFI_Manager_Process();
                ALERT_Reporter_ProcessQueue();
                break;
        }

        if (++subtickPhase >= (SENSOR_POLL_INTERVAL_MS / IMU_POLL_INTERVAL_MS))
        {
            subtickPhase = 0U;
        }

        /* 3. Maintain exact 20 ms loop timing */
        App_CycleDelay();
    }

    return 0; 
}

/*******************************************************************************
 * Subsystem Implementations
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

    (void)Timebase_Init();
    s_nextDeadlineMs = Timebase_GetMs();

    PRINTF("\r\n==================================================\r\n");
    PRINTF("Worker Safety: Temp (P3T1755) + Motion (MPU) + Gas (MQ-2)\r\n");
    PRINTF("==================================================\r\n");

    SENSOR_Temp_Init();
    SENSOR_MPU6050_Init();
    SENSOR_Gas_Init();
    App_BootDiagnostics();
    SAFETY_Decision_Init();
    BUTTON_SOS_Init();
    ALERT_Manager_Init();
    WIFI_Manager_Init();
    ALERT_Reporter_Init();
}

static void App_ProcessMotionAndFall(void)
{
    (void)SENSOR_MPU6050_Read(&s_currentMpu);

    if (s_currentMpu.is_valid)
    {
        g_liveAccelX     = s_currentMpu.accel_x_g;
        g_liveAccelY     = s_currentMpu.accel_y_g;
        g_liveAccelZ     = s_currentMpu.accel_z_g;
        g_liveAccelTotal = SENSOR_MPU6050_GetTotalAccelG(&s_currentMpu);

        /* Evaluate fall detection at full 50 Hz (20 ms resolution) */
        SAFETY_Decision_ProcessFall(&s_currentMpu);
    }
}

static void App_ProcessSensors(void)
{
    (void)SENSOR_Temp_Read(&s_currentTemp);
    (void)SENSOR_Gas_Read(&s_currentGas);

    g_heartbeatCounter++;

    if (s_currentTemp.is_valid)
    {
        g_liveTemperature = s_currentTemp.temperature_c;
    }
    if (s_currentGas.is_valid)
    {
        g_liveGasPpm     = s_currentGas.gas_ppm;
    }
}

static void App_ProcessSafetyLogic(void)
{
    static HazardType_t s_prevHazard = HAZARD_NONE;
    bool sosActive = BUTTON_SOS_IsPressed();
    s_lastEvaluation = SAFETY_Decision_Evaluate(&s_currentTemp, &s_currentMpu, &s_currentGas, sosActive);

    g_safetyState   = (uint32_t)s_lastEvaluation.state;
    g_primaryHazard = (uint32_t)s_lastEvaluation.primary_hazard;

    /* Increment fall event counter strictly on rising edge of detection */
    if (s_lastEvaluation.primary_hazard == HAZARD_FALL_DETECTED && s_prevHazard != HAZARD_FALL_DETECTED)
    {
        g_fallEventCount++;
    }
    s_prevHazard = s_lastEvaluation.primary_hazard;
}

static void App_ProcessAlerts(void)
{
    ALERT_Manager_Update(&s_lastEvaluation);
}

static void App_ProcessCommunication(void)
{
    static uint32_t telemetryTick = 0U;
    static uint32_t emergencyTick = 0U;

    TelemetryPacket_t packet;
    packet.message_id        = g_heartbeatCounter;
    packet.timestamp_ms      = Timebase_IsRunning() ? Timebase_GetMs() : (g_heartbeatCounter * SENSOR_POLL_INTERVAL_MS);
    packet.state             = s_lastEvaluation.state;
    packet.hazard            = s_lastEvaluation.primary_hazard;
    packet.temperature_c     = g_liveTemperature;
    packet.gas_ppm           = s_currentGas.gas_ppm;
    packet.accel_magnitude_g = g_liveAccelTotal;
    packet.accel_x_g         = g_liveAccelX;
    packet.accel_y_g         = g_liveAccelY;
    packet.accel_z_g         = g_liveAccelZ;
    packet.sos_active        = (s_lastEvaluation.primary_hazard == HAZARD_MANUAL_SOS);
    packet.sensors_ok        = !(SENSOR_Temp_IsFaulted() || SENSOR_MPU6050_IsFaulted() || SENSOR_Gas_IsFaulted());

    /* High-Priority Dispatch: Emergency alerts at EMERGENCY_REPORT_INTERVAL_MS */
    if (s_lastEvaluation.state == SAFETY_STATE_EMERGENCY)
    {
        if (++emergencyTick >= (EMERGENCY_REPORT_INTERVAL_MS / SENSOR_POLL_INTERVAL_MS))
        {
            emergencyTick = 0U;
            ALERT_Reporter_SendEmergencyAlert(&packet);
        }
    }
    /* Normal / Warning Periodic Telemetry Dispatch */
    else if (++telemetryTick >= (TELEMETRY_REPORT_INTERVAL_MS / SENSOR_POLL_INTERVAL_MS))
    {
        telemetryTick = 0U;
        ALERT_Reporter_SendTelemetry(&packet);
    }

    WIFI_Manager_Process();
    ALERT_Reporter_ProcessQueue();
}
