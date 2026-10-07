/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

#include "gpio.h"
#include "board_config.h"
#include "bsp_board.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef config = {0};
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Preload inactive ODR before switching any result/DE pin to output. */
    BSP_BoardSafeOutputs();

    config.Pin = BOARD_PRESENT_PIN;
    config.Mode = GPIO_MODE_INPUT;
    config.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(BOARD_PRESENT_PORT, &config);

    config.Pin = BOARD_ACTION_PIN;
    config.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BOARD_ACTION_PORT, &config);
    config.Pin = BOARD_RESET_PIN;
    HAL_GPIO_Init(BOARD_RESET_PORT, &config);

    config.Mode = GPIO_MODE_OUTPUT_PP;
    config.Pull = GPIO_NOPULL;
    config.Speed = GPIO_SPEED_FREQ_LOW;
    config.Pin = BOARD_PASS_PIN;
    HAL_GPIO_Init(BOARD_PASS_PORT, &config);
    config.Pin = BOARD_FAIL_PIN;
    HAL_GPIO_Init(BOARD_FAIL_PORT, &config);
    config.Pin = BOARD_FAULT_PIN;
    HAL_GPIO_Init(BOARD_FAULT_PORT, &config);
    config.Pin = BOARD_RS485_DE_PIN;
    HAL_GPIO_Init(BOARD_RS485_DE_PORT, &config);
}
