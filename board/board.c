/*
 * Copyright 2026
 * Board: NXP FRDM-RW612 (RW612ETA2I)
 * Industrial Worker Safety Monitoring and Emergency Alert System
 */

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"

extern void DebugConsole_Init(void);

void BOARD_InitDebugConsole(void)
{
    DebugConsole_Init();
}

void BOARD_InitHardware(void)
{
    BOARD_BootClockRUN();
    BOARD_InitPins();
    BOARD_InitDebugConsole();
}
