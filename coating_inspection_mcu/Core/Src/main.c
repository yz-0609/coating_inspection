/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "bsp_UART.h"
#include "bsp_reset.h"
#include "bsp_board.h"
#include "board_config.h"
#include "bsp_rs485.h"
#include "bsp_watchdog.h"
#include "app_runtime.h"

void SystemClock_Config(void);
void MX_FREERTOS_Init(void);

int main(void)
{
    /* 保存复位原因后初始化 HAL/时钟，再预置安全 GPIO，最后启动通信、看门狗和任务。 */
    BSP_ResetCapture();
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    BSP_DebugInit();
    BSP_RS485_Init();
    BSP_WatchdogInit();

    MX_FREERTOS_Init();
    /* 调度器正常情况下不返回；若启动失败，统一进入致命故障输出路径。 */
    vTaskStartScheduler();
    Error_Handler();
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  /* 25 MHz HSE / 25 * 336 / 2 = 168 MHz SYSCLK；APB1 42 MHz，APB2 84 MHz。 */
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* TIM6 提供 HAL 毫秒时基，SysTick 专供 FreeRTOS，避免两套时基争用。 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
}

void Error_Handler(void)
{
    /* 禁止继续执行任务或中断，直接设置安全故障输出；IWDG 已启动时最终触发复位。 */
    __disable_irq();
    BSP_BoardFaultOutputs();
    for (;;) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
