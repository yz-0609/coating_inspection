/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "app_runtime.h"

static StaticTask_t idleTaskControlBlock;
static StackType_t idleTaskStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **controlBlock,
                                  StackType_t **stack, uint32_t *stackSize)
{
    *controlBlock = &idleTaskControlBlock;
    *stack = idleTaskStack;
    *stackSize = configMINIMAL_STACK_SIZE;
}

void MX_FREERTOS_Init(void)
{
    App_CreateTasks();
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}
