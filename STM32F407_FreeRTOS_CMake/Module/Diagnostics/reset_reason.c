#include "reset_reason.h"

uint16_t ResetReason_Decode(uint32_t raw_csr)
{
    uint16_t reason = 0;
    if (raw_csr & (UINT32_C(1) << 27)) reason |= RESET_REASON_POWER_ON;
    if (raw_csr & (UINT32_C(1) << 26)) reason |= RESET_REASON_PIN;
    if (raw_csr & (UINT32_C(1) << 28)) reason |= RESET_REASON_SOFTWARE;
    if (raw_csr & (UINT32_C(1) << 29)) reason |= RESET_REASON_IWDG;
    if (raw_csr & (UINT32_C(1) << 30)) reason |= RESET_REASON_WWDG;
    if (raw_csr & (UINT32_C(1) << 31)) reason |= RESET_REASON_LOW_POWER;
    if (raw_csr & (UINT32_C(1) << 25)) reason |= RESET_REASON_BROWN_OUT;
    return reason;
}
