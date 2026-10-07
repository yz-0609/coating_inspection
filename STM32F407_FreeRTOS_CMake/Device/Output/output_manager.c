#include "output_manager.h"
#include "output_policy.h"
#include "protocol.h"
#include "board_config.h"
#include "bsp_board.h"
static GPIO_PinState Level(GPIO_PinState inactive, bool active)
{ return active ? (inactive==GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET) : inactive; }
uint16_t OutputManager_Commit(uint16_t request, bool gate)
{
    static bool initialized;
    static uint16_t previous;
    uint16_t applied=OutputPolicy_Resolve(request,gate);
    if (initialized && applied==previous) return applied;
    /* Break before make. PASS and FAIL never overlap even between GPIO writes. */
    HAL_GPIO_WritePin(BOARD_PASS_PORT,BOARD_PASS_PIN,BOARD_PASS_INACTIVE);
    HAL_GPIO_WritePin(BOARD_FAIL_PORT,BOARD_FAIL_PIN,BOARD_FAIL_INACTIVE);
    HAL_GPIO_WritePin(BOARD_FAULT_PORT,BOARD_FAULT_PIN,
                      Level(BOARD_FAULT_INACTIVE,(applied & OUTPUT_FAULT)!=0));
    if (applied==OUTPUT_PASS)
        HAL_GPIO_WritePin(BOARD_PASS_PORT,BOARD_PASS_PIN,Level(BOARD_PASS_INACTIVE,true));
    else if (applied==OUTPUT_FAIL)
        HAL_GPIO_WritePin(BOARD_FAIL_PORT,BOARD_FAIL_PIN,Level(BOARD_FAIL_INACTIVE,true));
    previous=applied; initialized=true; return applied;
}
