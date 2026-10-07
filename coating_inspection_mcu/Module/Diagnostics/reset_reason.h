#ifndef RESET_REASON_H
#define RESET_REASON_H

#include <stdint.h>

enum {
    RESET_REASON_POWER_ON = 1U << 0,
    RESET_REASON_PIN = 1U << 1,
    RESET_REASON_SOFTWARE = 1U << 2,
    RESET_REASON_IWDG = 1U << 3,
    RESET_REASON_WWDG = 1U << 4,
    RESET_REASON_LOW_POWER = 1U << 5,
    RESET_REASON_BROWN_OUT = 1U << 6
};

/* STM32F407 RCC_CSR reset flags. This module has no HAL/RTOS dependency. */
uint16_t ResetReason_Decode(uint32_t raw_csr);

#endif
