#include "bsp_watchdog.h"
#include "main.h"
static IWDG_HandleTypeDef watchdog;
void BSP_WatchdogInit(void)
{
    watchdog.Instance=IWDG;
    /* 按 LSI 标称 32 kHz 计算约 3 s，实际期限随 LSI 偏差变化；喂狗由健康任务控制。 */
    watchdog.Init.Prescaler=IWDG_PRESCALER_32; watchdog.Init.Reload=2999;
    if (HAL_IWDG_Init(&watchdog)!=HAL_OK) Error_Handler();
}
void BSP_WatchdogFeed(void) { IWDG->KR=IWDG_KEY_RELOAD; }
