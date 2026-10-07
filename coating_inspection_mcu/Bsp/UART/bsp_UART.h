#ifndef BSP_DEBUG_UART_H
#define BSP_DEBUG_UART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void BSP_DebugInit(void);
bool BSP_DebugWrite(const char *data, size_t length);
bool BSP_DebugRead(uint8_t *byte);
void BSP_DebugIRQ(void);
extern volatile uint32_t g_debug_tx_dropped, g_debug_rx_dropped;
#endif
