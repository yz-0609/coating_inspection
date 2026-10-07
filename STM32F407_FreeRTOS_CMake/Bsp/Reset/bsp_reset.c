#include "bsp_reset.h"
#include "reset_reason.h"
#include "stm32f4xx_hal.h"

#if RCC_CSR_BORRSTF != (1UL << 25) || RCC_CSR_PINRSTF != (1UL << 26) || \
    RCC_CSR_PORRSTF != (1UL << 27) || RCC_CSR_SFTRSTF != (1UL << 28) || \
    RCC_CSR_IWDGRSTF != (1UL << 29) || RCC_CSR_WWDGRSTF != (1UL << 30) || \
    RCC_CSR_LPWRRSTF != (1UL << 31)
#error RCC reset flag mapping must match STM32F407
#endif

static BspResetInfo bootReset;

void BSP_ResetCapture(void)
{
    bootReset.raw_csr = RCC->CSR;
    bootReset.reason = ResetReason_Decode(bootReset.raw_csr);
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

const BspResetInfo *BSP_ResetInfo(void)
{
    return &bootReset;
}
