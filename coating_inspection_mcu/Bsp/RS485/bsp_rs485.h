#ifndef BSP_RS485_H
#define BSP_RS485_H
#include "rtu.h"
#include "stm32f4xx_hal.h"
void BSP_RS485_Init(void);
uint32_t BSP_Micros(void);
bool BSP_RS485_Read(RxRecord *record);
uint32_t BSP_RS485_Errors(void);
bool BSP_RS485_Healthy(void);
void BSP_RS485_Recover(void);
bool BSP_RS485_Send(const uint8_t *data, size_t length);
/* 0 pending, 1 successfully reached USART TC, -1 failed or timed out. */
int BSP_RS485_TxResult(void);
void BSP_RS485_MonitorIRQ(void);
void BSP_RS485_UartIRQ(void);
void BSP_RS485_RxDMAIRQ(void);
void BSP_RS485_TxDMAIRQ(void);
void BSP_RS485_WakeFromISR(void);
#endif
