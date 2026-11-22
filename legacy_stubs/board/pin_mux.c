/*
 * Copyright 2026
 * Board: NXP FRDM-RW612 (RW612ETA2I)
 * Industrial Worker Safety Monitoring and Emergency Alert System
 */

#include "pin_mux.h"
#include "board.h"

/*******************************************************************************
 * Hardware Pin Routing & GPIO Multiplexing
 ******************************************************************************/
#define HW_GPIO_PORT0_DIR    (*(volatile uint32_t *)(0x40102000U))

void BOARD_InitPins(void)
{
    /* 
     * Configure Pin Directions for Port 0:
     * Outputs:
     * - Pin 4: Active Buzzer Module
     * - Pin 5: Normal Status LED (Green)
     * - Pin 6: Warning Status LED (Yellow)
     * - Pin 7: Emergency Status LED (Red)
     * Inputs:
     * - Pin 24: SW2 User Pushbutton (Active Low, SOS Emergency Input)
     */
    HW_GPIO_PORT0_DIR |= (1U << BOARD_BUZZER_GPIO_PIN)    |
                         (1U << BOARD_LED_NORMAL_PIN)     |
                         (1U << BOARD_LED_WARNING_PIN)    |
                         (1U << BOARD_LED_EMERGENCY_PIN);

    /* Ensure Pin 24 is set as input (bit cleared) */
    HW_GPIO_PORT0_DIR &= ~(1U << BOARD_SW2_GPIO_PIN);
}
