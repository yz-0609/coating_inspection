#include "bsp_watchdog.h"
#include "main.h"
static IWDG_HandleTypeDef watchdog;
void BSP_WatchdogInit(void)
{
    watchdog.Instance=IWDG;
    watchdog.Init.Prescaler=IWDG_PRESCALER_32; watchdog.Init.Reload=2999;
    if (HAL_IWDG_Init(&watchdog)!=HAL_OK) Error_Handler();
}
void BSP_WatchdogFeed(void) { IWDG->KR=IWDG_KEY_RELOAD; }
