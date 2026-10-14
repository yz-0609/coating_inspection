#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H
#include "FreeRTOS.h"
#include "task.h"
#include "workstation.h"
void App_CreateTasks(void);
/* SWD-only diagnostics; normal access goes through the critical-section owner. */
extern Workstation g_workstation;
extern volatile uint16_t g_stack_watermark[3];
extern volatile uint32_t g_task_cycles[3];
#endif
