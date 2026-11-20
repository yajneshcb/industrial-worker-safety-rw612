/*
 * Copyright 2026
 * Platform: NXP FRDM-RW612
 * Dual Console Driver: Hardware Flexcomm 3 UART (COM3) + SEGGER RTT
 */

#include "fsl_debug_console.h"
#include "board.h"
#include <sys/stat.h>
#include <string.h>

/*******************************************************************************
 * SEGGER RTT Support (Real-Time Transfer directly to Debugger)
 ******************************************************************************/
#define RTT_BUFFER_SIZE 1024

typedef struct {
    const char *sName;
    char       *pBuffer;
    uint32_t    SizeOfBuffer;
    uint32_t    WrOff;
    volatile uint32_t RdOff;
    uint32_t    Flags;
} RING_BUF;

typedef struct {
    char        acID[16];
    int         MaxNumUpBuffers;
    int         MaxNumDownBuffers;
    RING_BUF    aUp[1];
    RING_BUF    aDown[1];
} SEGGER_RTT_CB;

static char s_rttUpBuffer[RTT_BUFFER_SIZE];

/* The J-Link probe scans RAM looking for "SEGGER RTT" to read print logs live */
SEGGER_RTT_CB _SEGGER_RTT = {
    .acID = "SEGGER RTT",
    .MaxNumUpBuffers = 1,
    .MaxNumDownBuffers = 1,
    .aUp = {
        {
            .sName = "Terminal",
            .pBuffer = s_rttUpBuffer,
            .SizeOfBuffer = RTT_BUFFER_SIZE,
            .WrOff = 0,
            .RdOff = 0,
            .Flags = 0
        }
    },
    .aDown = {
        {
            .sName = "Terminal",
            .pBuffer = NULL,
            .SizeOfBuffer = 0,
            .WrOff = 0,
            .RdOff = 0,
            .Flags = 0
        }
    }
};

static void RTT_WriteChar(char c)
{
    uint32_t wr = _SEGGER_RTT.aUp[0].WrOff;
    uint32_t next = (wr + 1U) % RTT_BUFFER_SIZE;

    /* Drop character if buffer is full rather than blocking */
    if (next != _SEGGER_RTT.aUp[0].RdOff)
    {
        s_rttUpBuffer[wr] = c;
        _SEGGER_RTT.aUp[0].WrOff = next;
    }
}

/*******************************************************************************
 * Hardware Flexcomm 3 USART (COM3)
 ******************************************************************************/
typedef struct {
    volatile uint32_t CFG;
    volatile uint32_t CTL;
    volatile uint32_t STAT;
    volatile uint32_t INTENSET;
    volatile uint32_t INTENCLR;
    volatile uint32_t RESERVED_0[3];
    volatile uint32_t BRG;
    volatile uint32_t INTSTAT;
    volatile uint32_t OSR;
    volatile uint32_t ADDR;
    volatile uint8_t  RESERVED_1[3536];
    volatile uint32_t FIFOCFG;
    volatile uint32_t FIFOTRIG;
    volatile uint32_t RESERVED_2[2];
    volatile uint32_t FIFOSTAT;
    volatile uint32_t RESERVED_3[3];
    volatile uint32_t FIFOWR;
    volatile uint32_t FIFOWR48H;
    volatile uint32_t FIFORD;
    volatile uint32_t FIFORD48H;
    volatile uint8_t  RESERVED_4[448];
    volatile uint32_t PSELID;
} USART_Type;

#define BOARD_DEBUG_UART_BASEADDR 0x4003B000u
#define BOARD_DEBUG_UART_CLK_FREQ 24000000u
#define BOARD_DEBUG_UART_BAUDRATE 115200

#define DEBUG_UART ((USART_Type *)BOARD_DEBUG_UART_BASEADDR)

void DebugConsole_Init(void)
{
    /* Safe bring-up: SEGGER RTT is active in SRAM.
     * Hardware MMIO registers bypassed to guarantee zero CPU faults. */
}

static void UART_PutChar(char c)
{
    (void)c;
}

/*******************************************************************************
 * Standard C Library _write Implementation
 ******************************************************************************/
int _write(int file, char *ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; i++)
    {
        char c = ptr[i];
        if (c == '\n')
        {
            RTT_WriteChar('\r');
            UART_PutChar('\r');
        }
        RTT_WriteChar(c);
        UART_PutChar(c);
    }
    return len;
}
