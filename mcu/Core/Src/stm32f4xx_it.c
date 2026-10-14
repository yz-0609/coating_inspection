/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "main.h"
#include "stm32f4xx_it.h"
#include "bsp_rs485.h"
#include "bsp_UART.h"

extern TIM_HandleTypeDef htim6;

void NMI_Handler(void)
{
  Error_Handler();
}

void HardFault_Handler(void)
{
  Error_Handler();
}

void MemManage_Handler(void)
{
  Error_Handler();
}

void BusFault_Handler(void)
{
  Error_Handler();
}

void UsageFault_Handler(void)
{
  Error_Handler();
}

void DebugMon_Handler(void)
{

}

void TIM6_DAC_IRQHandler(void)
{

  HAL_TIM_IRQHandler(&htim6);

}

void USART1_IRQHandler(void) { BSP_DebugIRQ(); }
void USART2_IRQHandler(void) { BSP_RS485_UartIRQ(); }
void DMA1_Stream5_IRQHandler(void) { BSP_RS485_RxDMAIRQ(); }
void DMA1_Stream6_IRQHandler(void) { BSP_RS485_TxDMAIRQ(); }
void TIM3_IRQHandler(void) { BSP_RS485_MonitorIRQ(); }
