#include "bsp_board.h"
#include "board_config.h"

void BSP_BoardSafeOutputs(void)
{
    /* 预装输出锁存器的关闭电平；可在引脚切为输出模式前调用。 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(BOARD_PASS_PORT, BOARD_PASS_PIN, BOARD_PASS_INACTIVE);
    HAL_GPIO_WritePin(BOARD_FAIL_PORT, BOARD_FAIL_PIN, BOARD_FAIL_INACTIVE);
    HAL_GPIO_WritePin(BOARD_FAULT_PORT, BOARD_FAULT_PIN, BOARD_FAULT_INACTIVE);
    HAL_GPIO_WritePin(BOARD_RS485_DE_PORT, BOARD_RS485_DE_PIN, BOARD_RS485_DE_INACTIVE);
}

void BSP_BoardFaultOutputs(void)
{
    /* 致命错误路径直接关闭结果和 DE，再拉起 FAULT，不依赖任务调度或状态机。 */
    BSP_BoardSafeOutputs();
    HAL_GPIO_WritePin(BOARD_FAULT_PORT,BOARD_FAULT_PIN,
        BOARD_FAULT_INACTIVE==GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
