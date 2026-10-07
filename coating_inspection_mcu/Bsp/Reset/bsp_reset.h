#ifndef BSP_RESET_H
#define BSP_RESET_H

#include <stdint.h>

typedef struct {
    uint32_t raw_csr;
    uint16_t reason;
} BspResetInfo;

/* Call exactly once at entry to main, before any reset-flag clearing. */
void BSP_ResetCapture(void);
const BspResetInfo *BSP_ResetInfo(void);

#endif
