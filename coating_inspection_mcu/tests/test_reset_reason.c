#include "reset_reason.h"
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #expr); return 1; } } while (0)

int main(void)
{
    const unsigned flags[] = {27, 26, 28, 29, 30, 31, 25};
    unsigned combination;
    /* Enumerate every combination: reset causes may be simultaneous. */
    for (combination = 0; combination < 128; ++combination) {
        uint32_t raw = 0;
        unsigned index;
        for (index = 0; index < 7; ++index)
            if (combination & (1U << index)) raw |= UINT32_C(1) << flags[index];
        CHECK(ResetReason_Decode(raw) == combination);
        /* LSI status/control and other non-reset bits must not leak into protocol. */
        CHECK(ResetReason_Decode(raw | UINT32_C(0x01ffffff)) == combination);
    }
    CHECK(ResetReason_Decode(UINT32_C(0xffffffff)) == UINT16_C(0x007f));
    puts("reset reason: all 128 combinations and non-reset bits passed");
    return 0;
}
