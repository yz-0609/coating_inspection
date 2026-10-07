#include "debounce.h"
void Debounce_Init(Debounce *d, bool initial, uint32_t now)
{ d->stable=initial; d->candidate=initial; d->since=now; }
bool Debounce_Update(Debounce *d, bool raw, uint32_t now)
{
    if (raw!=d->candidate) { d->candidate=raw; d->since=now; }
    if (d->stable!=d->candidate && (uint32_t)(now-d->since)>=30U) {
        d->stable=d->candidate; return true;
    }
    return false;
}
